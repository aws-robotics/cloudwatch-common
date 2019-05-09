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

#include <cloudwatch_logs_common/file_upload/network_monitor.h>

namespace Aws {
namespace FileManagement {

template<
    class T,
    class Allocator = std::allocator<T>>
class ObservedQueue {
public:
  inline void setStatusMonitor(std::shared_ptr<StatusMonitor> &status_monitor) {
    status_monitor_ = status_monitor;
  }
  inline void enqueue( const T& value ) {
    dequeue_.push_back(value);
    status_monitor_->setStatus(AVAILABLE);
  }

  inline T dequeue( const T& value ) {
    T front = dequeue_.front();
    dequeue_.pop_front();
    if (dequeue_.empty()) {
      status_monitor_->setStatus(UNAVAILABLE);
    }
    return front;
  }

private:
  std::shared_ptr<StatusMonitor> status_monitor_;
  std::deque<T, Allocator> dequeue_;
};

}  // namespace FileManagement
}  // namespace Aws