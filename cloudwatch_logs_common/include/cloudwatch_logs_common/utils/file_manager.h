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
#include <list>

#include <aws/logs/model/InputLogEvent.h>

#include "cloudwatch_logs_common/utils/task_utils.h"
#include "cloudwatch_logs_common/ros_cloudwatch_logs_errors.h"

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

using LogType = std::list<Aws::CloudWatchLogs::Model::InputLogEvent> *;

/**
 * Manages how files are split up, which files to write to and read when requested.
 */
class FileManagerStrategy {
public:
  /**
   * Get the file name to write to.
   *
   * @return current file name
   */
  std::string getFileToWrite(){
    return file_name_;
  }

private:
  /**
   * Current file name to write to.
   */
  std::string file_name_ = "example_file.log";
};

/**
 * File manager specific to the type of data to write to files.
 * @tparam T type of data to write
 */
template <typename T>
class FileManager {
public:
  virtual ~FileManager() = default;

  /**
   * Write data to the appropriate file.
   * @param data to write.
   */
  virtual void write(const T & data) = 0;
protected:
  /**
   * The object that keeps track of which files to delete, read, or write to.
   * Should probably be thread safe or something :)
   */
  FileManagerStrategy file_manager_strategy_;
};

/**
 * The log specific file manager. Handles the specific writes of log data.
 */
class LogFileManager : FileManager<LogType>{
public:
  /**
   * Handle an upload complete status.
   *
   * @param upload_status the status of an attempted upload of data
   * @param log_messages the data which was attempted to be uploaded
   */
  void uploadCompleteStatus(const ROSCloudWatchLogsErrors& upload_status, const LogType &log_messages);
  void write(const LogType & data) override;
};

}  // namespace Utils
}  // namespace CloudwatchLogs
}  // namespace Aws
