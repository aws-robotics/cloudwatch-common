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
  if (ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED != upload_status) {
    write(log_messages);
  }
}

void LogFileManager::write(const LogType & data) {
  std::ofstream log_file;
  log_file.open(FileManager::file_manager_strategy_->getFileToWrite());
  for (const Model::InputLogEvent &log: data) {
    auto str = log.Jsonize().View().WriteCompact();
    log_file << str << std::endl;
  }
  log_file.close();
}

}  // namespace Utils
}  // namespace CloudWatchLogs
}  // namespace Aws
