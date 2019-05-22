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

LogManager::LogManager(const std::shared_ptr<ILogPublisher> log_publisher)
{
  this->log_publisher_ = log_publisher;
}

LogManager::~LogManager() = default;

using Aws::FileManagement::BasicTask;
using Aws::FileManagement::UploadStatus;
using Aws::FileManagement::UploadStatusFunction;

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogManager::RecordLog(
  const std::string & log_msg_formatted)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;
  Aws::CloudWatchLogs::Model::InputLogEvent log_event;
  using namespace std::chrono;
  milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  log_event.SetMessage(log_msg_formatted.c_str());
  log_event.SetTimestamp(ms.count());
  if (!current_task_) {
    using std::placeholders::_1;
    using std::placeholders::_2;
    Utils::LogType logs;
    BasicTask<Utils::LogType> data(logs,
      std::bind(&Utils::LogFileManager::uploadCompleteStatus, log_file_manager_, _1, _2));
    current_task_ = std::make_shared<BasicTask<Utils::LogType>>(logs,
      std::bind(&Utils::LogFileManager::uploadCompleteStatus, log_file_manager_, _1, _2));
  }
  current_task_->getBatchData().push_back(log_event);
  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogManager::Service()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;
  if (getSink()) {
    getSink()->enqueue(current_task_);
    current_task_ = nullptr;
  } else {
    status = CW_LOGS_LOG_STREAM_NOT_CONFIGURED;
  }
  return status;
}
