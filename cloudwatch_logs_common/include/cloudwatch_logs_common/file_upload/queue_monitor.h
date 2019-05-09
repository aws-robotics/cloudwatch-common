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
#include <cloudwatch_logs_common/file_upload/status_monitor.h>
#include <cloudwatch_logs_common/file_upload/observed_queue.h>

namespace Aws {
namespace FileManagement {

template<typename T>
class QueueMonitor : public MultiStatusConditionMonitor {
public:
  QueueMonitor() = default;
  virtual ~QueueMonitor() = default;

  inline void add_queue(std::shared_ptr<ObservedQueue<T>> observed_queue) {
    auto status_monitor = std::make_shared<StatusMonitor>();
    addStatusMonitor(status_monitor);
    observed_queue->setStatusMonitor(status_monitor);
    queues_.push_back(observed_queue);
  }

  inline T dequeue() {
    T data;
    for (auto &queue : queues_)
    {
      if (!queue->empty()) {
        data = queue->dequeue();
      }
    }
    return data;
  }

protected:
  inline bool hasWork() override {
    return (std::accumulate(
        status_monitors_.begin(),
        status_monitors_.end(),
        !status_monitors_.empty(),
        [](bool amount, const std::shared_ptr<StatusMonitor> statusMonitor) -> bool {
          return amount || statusMonitor->getStatus();
        }));
  }
private:
  std::vector<std::shared_ptr<ObservedQueue<T>>> queues_;
};

}  // namespace FileManagement
}  // namespace Aws