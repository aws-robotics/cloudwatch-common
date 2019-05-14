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

/**
 * Status.
 */
enum Status : uint {
  UNAVAILABLE = 0,
  AVAILABLE
};

class MultiStatusConditionMonitor;

/**
 * Handle a single set/get status. Notify the multi_status_condition when the status is changed.
 */
class StatusMonitor {
public:
  virtual ~StatusMonitor() = default;
  void setStatus(const Status &status);

  inline void setStatusObserver(MultiStatusConditionMonitor *multi_status_cond) {
    multi_status_cond_ = multi_status_cond;
  }

  inline Status getStatus() const {
    return status_;
  }
private:
  Status status_ = UNAVAILABLE;
  MultiStatusConditionMonitor *multi_status_cond_ = nullptr;
};

/**
 * Multi Status Condition Monitor listens to N StatusMonitors and determines whether to trigger wait for work
 * based on the hasWork() function.
 */
class MultiStatusConditionMonitor {
public:
  void waitForWork();
  std::cv_status waitForWork(const std::chrono::milliseconds &duration);
  void addStatusMonitor(std::shared_ptr<StatusMonitor> &status_monitor);
  void setStatus(const Status &status);
protected:
  virtual bool hasWork();
  std::vector<std::shared_ptr<StatusMonitor>> status_monitors_;
  std::mutex idle_mutex_;
  std::condition_variable work_condition_;
};

}  // namespace FileManagement
}  // namespace Aws