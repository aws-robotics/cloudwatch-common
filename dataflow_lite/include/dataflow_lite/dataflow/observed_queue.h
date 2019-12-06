/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <semaphore.h>

#include <dataflow_lite/dataflow/sink.h>
#include <dataflow_lite/dataflow/source.h>
#include <dataflow_lite/dataflow/status_monitor.h>

namespace Aws {
namespace DataFlow {

template<
  class T,
  class Allocator = std::allocator<T>>
class IObservedQueue:
  public Sink<T>,
  public Source<T>
{
public:
  virtual bool Empty() const = 0;
  virtual size_t Size() const = 0;
  virtual void SetStatusMonitor(std::shared_ptr<StatusMonitor> status_monitor) = 0;
};

/**
 * An observed queue is a dequeue wrapper which notifies an observer when a task is added.
 *
 * @tparam T type of data
 * @tparam Allocator
 */
template<
  class T,
  class Allocator = std::allocator<T>>
class ObservedQueue :
  public IObservedQueue<T, Allocator> {
public:

  ~ObservedQueue() override = default;

  /**
   * Set the observer for the queue.
   *
   * @param status_monitor
   */
  inline void SetStatusMonitor(std::shared_ptr<StatusMonitor> status_monitor) override {
    status_monitor_ = status_monitor;
  }

  /**
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline bool Enqueue(T&& value) override {
    dequeue_.push_back(value);
    NotifyMonitor(AVAILABLE);
    return true;
  }

  /**
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline bool Enqueue(T& value) override {
    dequeue_.push_back(value);
    NotifyMonitor(AVAILABLE);
    return true;
  }

  inline bool TryEnqueue(
      T& value,
      const std::chrono::microseconds& /*unused*/) override
  {
    return Enqueue(value);
  }

  inline bool TryEnqueue(
      T&& value,
      const std::chrono::microseconds& /*unused*/) override
  {
    return Enqueue(value);
  }

  /**
   * Dequeue data and notify the observer of data unavailable if the queue is empty.
   *
   * @return the front of the dequeue
   */
  inline bool Dequeue(
    T& data,
    const std::chrono::microseconds& /*unused*/) override
  {
    bool is_data = false;
    if (!dequeue_.empty()) {
      data = dequeue_.front();
      dequeue_.pop_front();
      is_data = true;
      if (dequeue_.empty()) {
        NotifyMonitor(UNAVAILABLE);
      }
    }
    return is_data;
  }

  /**
   * @return true if the queue is empty
   */
  inline bool Empty() const override {
    return dequeue_.empty();
  }

  /**
   * @return the size of the queue
   */
  inline size_t Size() const override {
    return dequeue_.size();
  }

  /**
   * Clear the dequeue
   */
  void Clear() override {
    dequeue_.clear();
  }

protected:

  /**
   * Notify the monitor if it exists.
   *
   * @param status the status to notify the monitor of.
   */
  void NotifyMonitor(const Status &status) {
    if (status_monitor_) {
      status_monitor_->SetStatus(status);
    }
  }

  /**
   * The status monitor observer.
   */
  std::shared_ptr<StatusMonitor> status_monitor_;

  /**
   * The dequeue to store data.
   */
  std::deque<T, Allocator> dequeue_;

};

/**
 * Adds basic thread safety to the ObservedQueue.
 *
 * @tparam T
 * @tparam Allocator
 */
template<
    class T,
    class Allocator = std::allocator<T>>
class ObservedSynchronizedQueue : public ObservedQueue<T, Allocator> {
public:
  ~ObservedSynchronizedQueue() override = default;

  /**
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline bool Enqueue(T&& value) override {
    std::unique_lock<DequeueMutex> lock(dequeue_mutex_);
    return OQ::Enqueue(std::move(value));
  }

  /**
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline bool Enqueue(T& value) override {
    std::unique_lock<DequeueMutex> lock(dequeue_mutex_);
    return OQ::Enqueue(value);
  }

  inline bool TryEnqueue(
      T& value,
      const std::chrono::microseconds &duration) override
  {
    std::unique_lock<DequeueMutex> lock(dequeue_mutex_, std::defer_lock);
    bool result = lock.try_lock_for(duration);
    if (result) {
      OQ::Enqueue(value);
    }
    return result;
  }

  inline bool TryEnqueue(
      T&& value,
      const std::chrono::microseconds &duration) override
  {
    std::unique_lock<DequeueMutex> lock(dequeue_mutex_, std::defer_lock);
    bool result = lock.try_lock_for(duration);
    if (result) {
      OQ::Enqueue(std::move(value));
    }
    return result;
  }

  /**
   * Dequeue data and notify the observer of data unavailable if the queue is empty.
   *
   * @return the front of the dequeue
   */
  inline bool Dequeue(
      T& data,
      const std::chrono::microseconds &duration) override
  {
    std::unique_lock<DequeueMutex> lock(dequeue_mutex_, std::defer_lock);
    bool result = lock.try_lock_for(duration);
    if (result) {
      result = OQ::Dequeue(data, duration);
    }
    return result;
  }

  /**
   * @return true if the queue is empty
   */
  inline bool Empty() const override {
    std::unique_lock<DequeueMutex> lock(dequeue_mutex_);
    return OQ::Empty();
  }

  /**
   * @return the size of the queue
   */
  inline size_t Size() const override {
    std::unique_lock<DequeueMutex> lock(dequeue_mutex_);
    return OQ::Size();
  }

  /**
   * Clear the dequeue
   */
  void Clear() override {
    std::unique_lock<DequeueMutex> lock(dequeue_mutex_);
    OQ::Clear();
  }

private:
  using OQ = ObservedQueue<T, Allocator>;
  // @todo (rddesmon): Dual semaphore for read optimization
  using DequeueMutex = std::timed_mutex;
  mutable DequeueMutex dequeue_mutex_;
};
/**
 * An observed queue is a dequeue wrapper which notifies an observer when a task is added.
 *
 * @tparam T type of data
 * @tparam Allocator
 */
template<
  class T,
  class Allocator = std::allocator<T>>
class ObservedBlockingQueue : public ObservedQueue<T, Allocator> {
public:

  /**
   * Create an observed blocking queue.
   *
   * @param max_queue_size to configure.
   * @throws std::invalid_argument max_queue_size is 0
   */
  explicit ObservedBlockingQueue(const size_t &max_queue_size) {
    if (max_queue_size == 0) {
      throw std::invalid_argument("Max queue size invalid: 0");
    }
    max_queue_size_ = max_queue_size;
  }

  ~ObservedBlockingQueue() override = default;
  /**
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline bool Enqueue(T&& value) override
  {
    bool is_queued = false;
    std::unique_lock<std::mutex> lk(dequeue_mutex_);
    if (OQ::Size() <= max_queue_size_) {
      OQ::Enqueue(value);
      is_queued = true;
    }
    return is_queued;
  }

  inline bool Enqueue(T& value) override {
    bool is_queued = false;
    std::unique_lock<std::mutex> lk(dequeue_mutex_);
    if (OQ::Size() <= max_queue_size_) {
      OQ::Enqueue(value);
      is_queued = true;
    }
    return is_queued;
  }

  /**
   * Blocking call.
   *
   * @param value
   * @param duration
   * @return
   */
  inline bool TryEnqueue(
    T& value,
    const std::chrono::microseconds &duration) override
  {
    std::cv_status (std::condition_variable::*wf)(std::unique_lock<std::mutex>&, const std::chrono::microseconds&);
    wf = &std::condition_variable::wait_for;
    return EnqueueOnCondition(
      value,
      std::bind(wf, &condition_variable_, std::placeholders::_1, duration));
  }

  inline bool TryEnqueue(
      T&& value,
      const std::chrono::microseconds &duration) override
  {
    std::cv_status (std::condition_variable::*wf)(std::unique_lock<std::mutex>&, const std::chrono::microseconds&);
    wf = &std::condition_variable::wait_for;
    return EnqueueOnCondition(
      value,
      std::bind(wf, &condition_variable_, std::placeholders::_1, duration));
  }

  /**
   * Dequeue data and notify the observer of data unavailable if the queue is empty.
   *
   * @return the front of the dequeue
   */
  inline bool Dequeue(T& data, const std::chrono::microseconds &duration) override {
    auto is_retrieved = OQ::Dequeue(data, duration);
    if (is_retrieved) {
      std::unique_lock<std::mutex> lck(dequeue_mutex_);
      condition_variable_.notify_one();
    }
    return is_retrieved;
  }

  /**
   * @return true if the queue is empty
   */
  inline bool Empty() const override {
    std::lock_guard<std::mutex> lock(dequeue_mutex_);
    return OQ::Empty();
  }

  /**
   * @return the size of the queue
   */
  inline size_t Size() const override {
    std::lock_guard<std::mutex> lock(dequeue_mutex_);
    return OQ::Size();
  }

  /**
   * Clear the dequeue
   */
  void Clear() override {
    std::lock_guard<std::mutex> lock(dequeue_mutex_);
    OQ::Clear();
  }

private:
  using OQ = ObservedQueue<T, Allocator>;
  using WaitFunc = std::function <std::cv_status (std::unique_lock<std::mutex>&)>;

  /**
   * Static wait function which returns no_timeout on completion.
   *
   * @param condition_variable
   * @param lock
   * @return std::cv_status::no_timeout
   */
  static std::cv_status Wait(
    std::condition_variable &condition_variable,
    std::unique_lock<std::mutex> &lock)
  {
    condition_variable.wait(lock);
    return std::cv_status::no_timeout;
  }

  /**
   * Enqueue on the condition variable.
   *
   * @param value to enqueue
   * @param wait_func to wait for availability
   * @return true if the value was enqueued
   */
  inline bool EnqueueOnCondition(T& value,
    const WaitFunc &wait_func)
  {
    std::unique_lock<std::mutex> lk(dequeue_mutex_);
    bool can_enqueue = true;
    if (OQ::Size() >= max_queue_size_) {
      can_enqueue = wait_func(lk) == std::cv_status::no_timeout;
    }
    if (can_enqueue) {
      OQ::Enqueue(value);
    }
    return can_enqueue;
  }

  size_t max_queue_size_;
  std::condition_variable condition_variable_;
  mutable std::mutex dequeue_mutex_;
};

}  // namespace DataFlow
}  // namespace Aws
