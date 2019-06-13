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
#include <aws/logs/model/InputLogEvent.h>

#include <cloudwatch_logs_common/file_upload/file_manager.h>

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

//------------- Definitions --------------//
using LogType = std::list<Aws::CloudWatchLogs::Model::InputLogEvent>;
using LogTypePtr = LogType *;
//----------------------------------------//

using FileManagement::FileManager;
using FileManagement::FileManagerStrategy;
using FileManagement::FileObject;

/**
 * The log specific file manager. Handles the specific writes of log data.
 */
class LogFileManager :
    public FileManager<LogType> {
 public:
  /**
   * Default Constructor.
   */
  LogFileManager()  = default;

  explicit LogFileManager(
      const std::shared_ptr<FileManagerStrategy> &file_manager_strategy)
      : FileManager(file_manager_strategy)
  {
  }

  ~LogFileManager() override = default;

  void write(const LogType & data) override;

  FileObject<LogType> readBatch(size_t batch_size) override;
};

}  // namespace Utils
}  // namespace CloudwatchLogs
}  // namespace Aws