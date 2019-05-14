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

#include <cloudwatch_logs_common/file_upload/status_monitor.h>

namespace Aws {
namespace FileManagement {


template<
  class T,
  class Allocator = std::allocator<T>>
class IObservedQueue {
public:
  virtual void enqueue(T&& value) = 0;
  virtual void enqueue(T& value) = 0;
  virtual T dequeue() = 0;
  virtual bool empty() = 0;
  virtual size_t size() = 0;
  virtual void setStatusMonitor(std::weak_ptr<StatusMonitor> status_monitor) = 0;
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
class ObservedQueue : public IObservedQueue<T, Allocator> {
public:

  virtual ~ObservedQueue() = default;

  /**
   * Set the observer for the queue.
   *
   * @param status_monitor
   */
  inline void setStatusMonitor(std::weak_ptr<StatusMonitor> status_monitor) override {
    status_monitor_ = status_monitor;
  }

  /**
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline void enqueue(T&& value) override {
    dequeue_.push_back(value);
    notifyMonitor(AVAILABLE);
  }

  /**
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline void enqueue(T& value) override {
    dequeue_.push_back(value);
    notifyMonitor(AVAILABLE);
  }

  /**
   * Dequeue data and notify the observer of data unavailable if the queue is empty.
   *
   * @return the front of the dequeue
   */
  inline T dequeue() override {
    T front = dequeue_.front();
    dequeue_.pop_front();
    if (dequeue_.empty()) {
      notifyMonitor(UNAVAILABLE);
    }
    return front;
  }

  /**
   * @return true if the queue is empty
   */
  inline bool empty() override {
    return dequeue_.empty();
  }

  /**
   * @return the size of the queue
   */
  inline size_t size() override {
    return dequeue_.size();
  }

protected:

  /**
   * Notify the monitor if it exists.
   *
   * @param status the status to notify the monitor of.
   */
  void notifyMonitor(const Status &status) {
    auto status_monitor_ptr = status_monitor_.lock();
    if (status_monitor_ptr) {
      status_monitor_ptr->setStatus(status);
    }
  }

  /**
   * The status monitor observer.
   */
  std::weak_ptr<StatusMonitor> status_monitor_;

  /**
   * The dequeue to store data.
   */
  std::deque<T, Allocator> dequeue_;
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
   */
  explicit ObservedBlockingQueue(const size_t &max_queue_size) {
    // @todo(rddesmon) throw exception if max_queue_size is 0
    max_queue_size_ = max_queue_size;
  }

  virtual ~ObservedBlockingQueue() = default;
  /**
   * Blocking call
   *
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline void enqueue(T&& value) override {
    enqueueOnCondition(
      value,
      std::bind(&ObservedBlockingQueue::wait, std::ref(condition_variable_), std::placeholders::_1));
  }

  inline void enqueue(T& value) override {
    enqueueOnCondition(
      value,
      std::bind(&ObservedBlockingQueue::wait, std::ref(condition_variable_), std::placeholders::_1));
  }

  /**
   * Blocking call.
   *
   * @param value
   * @param duration
   * @return
   */
  inline bool tryEnqueue(
    T& value,
    const std::chrono::microseconds &duration)
  {
    std::cv_status (std::condition_variable::*wf)(std::unique_lock<std::mutex>&, const std::chrono::microseconds&);
    wf = &std::condition_variable::wait_for;
    return enqueueOnCondition(
      value,
      std::bind(wf, &condition_variable_, std::placeholders::_1, duration));
  }

  inline bool tryEnqueue(
      T&& value,
      const std::chrono::microseconds &duration)
  {
    std::cv_status (std::condition_variable::*wf)(std::unique_lock<std::mutex>&, const std::chrono::microseconds&);
    wf = &std::condition_variable::wait_for;
    return enqueueOnCondition(
      value,
      std::bind(wf, &condition_variable_, std::placeholders::_1, duration));
  }

  /**
   * Dequeue data and notify the observer of data unavailable if the queue is empty.
   *
   * @return the front of the dequeue
   */
  inline T dequeue() override {
    auto data = OQ::dequeue();
    std::unique_lock<std::mutex> lck(enqueue_mutex_);
    condition_variable_.notify_one();
    return data;
  }

private:
  using WaitFunc = std::function <std::cv_status (std::unique_lock<std::mutex>&)>;

  /**
   * Static wait function which returns no_timeout on completion.
   *
   * @param condition_variable
   * @param lock
   * @return std::cv_status::no_timeout
   */
  static std::cv_status wait(
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
  bool enqueueOnCondition(T& value,
    const WaitFunc &wait_func)
  {
    bool can_enqueue = true;
    if (OQ::size() >= max_queue_size_) {
      std::unique_lock<std::mutex> lk(enqueue_mutex_);
      can_enqueue = wait_func(lk) == std::cv_status::no_timeout;
    }
    if (can_enqueue) {
      OQ::enqueue(value);
    }
    return can_enqueue;
  }

  using OQ = ObservedQueue<T, Allocator>;
  size_t max_queue_size_;
  std::condition_variable condition_variable_;
  std::mutex enqueue_mutex_;
};

}  // namespace FileManagement
}  // namespace Aws