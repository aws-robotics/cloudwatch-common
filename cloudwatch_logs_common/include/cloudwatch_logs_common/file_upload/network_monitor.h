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

#include <vector>
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable

namespace Aws {
namespace FileManagement {

enum Status : uint {
  UNAVAILABLE = 0,
  AVAILABLE
};

class StatusMonitor {
public:
  virtual ~StatusMonitor() = default;
  void setStatus(const Status &status) {
    status_ = status;
  }

  Status getStatus() const {
    return status_;
  }
private:
  Status status_ = UNAVAILABLE;
};

class MultiStatusConditionMonitor {
public:

  inline void waitForWork() {
    if (!hasWork()) {
      std::unique_lock<std::mutex> lck(idle_mutex_);
      work_condition_.wait(lck);
    }
  }

  inline void addStatusMonitor(std::shared_ptr<StatusMonitor> &status_monitor) {
    if (status_monitor) {
      status_monitors_.push_back(status_monitor);
    }
  }

  inline void setStatus(const Status &status) {
    if (hasWork()) {
      std::unique_lock<std::mutex> lck(idle_mutex_);
      work_condition_.notify_one();
    }
  }

protected:
  virtual inline bool hasWork() {
    return (std::accumulate(
        status_monitors_.begin(),
        status_monitors_.end(),
        !status_monitors_.empty(),
        [](bool amount, StatusMonitor statusMonitor) -> bool {
          return amount && statusMonitor.getStatus();
        }));
  }

  std::vector<std::shared_ptr<StatusMonitor>> status_monitors_;
  std::mutex idle_mutex_;
  std::condition_variable work_condition_;
};

}  // namespace FileManagement
}  // namespace Aws