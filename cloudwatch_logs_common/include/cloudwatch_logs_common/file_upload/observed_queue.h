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

#include <cloudwatch_logs_common/file_upload/status_monitor.h>

namespace Aws {
namespace FileManagement {

/**
 * An observed queue is a dequeue wrapper which notifies an observer when a task is added.
 *
 * @tparam T type of data
 * @tparam Allocator
 */
template<
    class T,
    class Allocator = std::allocator<T>>
class ObservedQueue {
public:
  /**
   * Set the observer for the queue.
   *
   * @param status_monitor
   */
  inline void setStatusMonitor(std::shared_ptr<StatusMonitor> &status_monitor) {
    status_monitor_ = status_monitor;
  }

  /**
   * Enqueue data and notify the observer of data available.
   *
   * @param value to enqueue
   */
  inline void enqueue(T&& value) {
    dequeue_.push_back(value);
    if (status_monitor_) {
      status_monitor_->setStatus(AVAILABLE);
    }
  }

  /**
   * Dequeue data and notify the observer of data unavailable if the queue is empty.
   *
   * @return the front of the dequeue
   */
  inline T dequeue() {
    T front = dequeue_.front();
    dequeue_.pop_front();
    if (dequeue_.empty()) {
      if (status_monitor_) {
        status_monitor_->setStatus(UNAVAILABLE);
      }
    }
    return front;
  }

  /**
   * @return true if the queue is empty
   */
  inline bool empty() {
    return dequeue_.empty();
  }

  /**
   * @return the size of the queue
   */
  inline size_t size() {
    return dequeue_.size();
  }

private:
  /**
   * The status monitor observer.
   */
  std::shared_ptr<StatusMonitor> status_monitor_;

  /**
   * The dequeue to store data.
   */
  std::deque<T, Allocator> dequeue_;
};

}  // namespace FileManagement
}  // namespace Aws