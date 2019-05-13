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

#include <iostream>
#include <fstream>
#include "cloudwatch_logs_common/utils/log_file_manager.h"
#include "cloudwatch_logs_common/file_upload/file_manager_strategy.h"
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/logging/LogMacros.h>

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

void LogFileManager::uploadCompleteStatus(
  const ROSCloudWatchLogsErrors& upload_status,
  const LogType &log_messages)
{
  if (!log_messages.empty()) {
    if (ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED != upload_status) {
      AWS_LOG_INFO(__func__, "Writing logs to file");
      write(log_messages);
    }
  }
}

FileObject<LogType> LogFileManager::readBatch(
  size_t batch_size)
{
  LogType log_data;
  FileManagement::FileInfo file_info;
  AWS_LOG_INFO(__func__, "Reading Logbatch");
  size_t actual_batch_size = 0;
  for (size_t i = 0; i < batch_size; ++i) {
    std::string line;
    file_info = read(line);
    if (FileManagement::FileStatus::END_OF_READ == file_info.file_status) {
      break;
    }
    Aws::String aws_line(line.c_str());
    Aws::Utils::Json::JsonValue value(aws_line);
    Aws::CloudWatchLogs::Model::InputLogEvent input_event(value);
    actual_batch_size++;
    log_data.push_back(input_event);
  }
  FileObject<LogType> file_object;
  file_object.batch_data = log_data;
  file_object.file_info = file_info;
  file_object.batch_size = actual_batch_size;
return file_object;
}

void LogFileManager::write(const LogType & data) {
  for (const Model::InputLogEvent &log: data) {
    auto aws_str = log.Jsonize().View().WriteCompact();
    std::string str(aws_str.c_str());
    file_manager_strategy_->write(str);
  }
  if (FileManager::file_status_monitor_) {
    AWS_LOG_INFO(__func__,
                 "Set file status available");
    FileManager::file_status_monitor_->setStatus(Aws::FileManagement::Status::AVAILABLE);
  }
}

}  // namespace Utils
}  // namespace CloudwatchLogs
}  // namespace Aws
