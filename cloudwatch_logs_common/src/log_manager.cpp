/*
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/logs/model/InputLogEvent.h>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <cloudwatch_logs_common/log_manager.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/shared_object.h>

#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

using namespace Aws::CloudWatchLogs;

LogManager::LogManager(const std::shared_ptr<LogPublisher> log_publisher)
{
  this->log_publisher_ = log_publisher;
}

LogManager::~LogManager()
{
  this->log_buffers_[0].clear();
  this->log_buffers_[1].clear();
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogManager::RecordLog(
  const std::string & log_msg_formatted)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;
  Aws::CloudWatchLogs::Model::InputLogEvent log_event;
  using namespace std::chrono;
  milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  log_event.SetMessage(log_msg_formatted.c_str());
  log_event.SetTimestamp(ms.count());
  this->log_buffers_[active_buffer_indx_].push_back(log_event);
  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogManager::Service()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;

  // If the shared object is not still locked for publishing, then swap the buffers and publish the
  // new metrics
  if (!this->shared_object_.isDataAvailable()) {
    uint8_t new_active_buffer_indx = 1 - active_buffer_indx_;
    // Clean any old metrics out the buffer that previously used for publishing
    this->log_buffers_[new_active_buffer_indx].clear();

    status =
      this->shared_object_.setDataAndMarkReady(&this->log_buffers_[this->active_buffer_indx_]);
    // After this point no changes should be made to the active_buffer until it is swapped with the
    // secondary buffer

    if (CW_LOGS_SUCCEEDED == status) {
      status = this->log_publisher_->PublishLogs(&this->shared_object_);

      if (CW_LOGS_SUCCEEDED != status) {
        // For some reason we failed to publish, so mark the shared object as free so we can try
        // again later
        this->shared_object_.freeDataAndUnlock();
      } else {
        // If everything has been successful for publishing the buffer, then swap to the new buffer.
        this->active_buffer_indx_ = new_active_buffer_indx;
      }
    } else {
      AWS_LOG_ERROR(__func__, "Failed to set share object ready");
    }
  }
  return status;
}
