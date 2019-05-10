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
#include "cloudwatch_logs_common/utils/file_manager.h"
#include <aws/core/utils/json/JsonSerializer.h>

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

void LogFileManager::uploadCompleteStatus(const ROSCloudWatchLogsErrors& upload_status, const LogType &log_messages) {
  if (!log_messages.empty()) {
    if (ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED != upload_status) {
      write(log_messages);
    }
  }
}

std::string LogFileManager::read() {
  return file_manager_strategy_->read();
}

void LogFileManager::write(const LogType & data) {
  for (const Model::InputLogEvent &log: data) {
    auto aws_str = log.Jsonize().View().WriteCompact();
    std::string str(aws_str.c_str());
    file_manager_strategy_->write(str);
  }
  if (FileManager::file_status_monitor_) {
    FileManager::file_status_monitor_->setStatus(Aws::FileManagement::Status::AVAILABLE);
  }
}


}  // namespace Utils
}  // namespace CloudWatchLogs
}  // namespace Aws
