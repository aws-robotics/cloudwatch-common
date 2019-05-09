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

#include <cloudwatch_logs_common/file_upload/status_monitor.h>
#include <numeric>
using namespace Aws::FileManagement;
void StatusMonitor::setStatus(const Status &status) {
  status_ = status;
  if (multi_status_cond_) {
    multi_status_cond_->setStatus(status);
  }
}

void MultiStatusConditionMonitor::waitForWork() {
  if (!hasWork()) {
    std::unique_lock<std::mutex> lck(idle_mutex_);
    work_condition_.wait(lck);
  }
}

void MultiStatusConditionMonitor::addStatusMonitor(
std::shared_ptr<StatusMonitor> &status_monitor) {
  if (status_monitor) {
    status_monitor->setStatusObserver(this);
    status_monitors_.push_back(status_monitor);
  }
}

void MultiStatusConditionMonitor::setStatus(
  const Status &status) {
  if (hasWork()) {
    std::unique_lock<std::mutex> lck(idle_mutex_);
    work_condition_.notify_one();
  }
}

bool MultiStatusConditionMonitor::hasWork() {
  return (std::accumulate(
    status_monitors_.begin(),
    status_monitors_.end(),
    !status_monitors_.empty(),
    [](bool amount, const std::shared_ptr<StatusMonitor> statusMonitor) -> bool {
      return amount && statusMonitor->getStatus();
    }));
}