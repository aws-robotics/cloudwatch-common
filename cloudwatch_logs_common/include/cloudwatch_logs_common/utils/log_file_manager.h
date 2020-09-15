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
#include <queue>
#include <tuple>
#include <aws/logs/model/InputLogEvent.h>

#include <cloudwatch_logs_common/definitions/ros_cloudwatch_logs_errors.h>
#include <file_management/file_upload/file_manager.h>
#include <cloudwatch_logs_common/definitions/definitions.h>

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

using FileManagement::FileManager;
using FileManagement::FileManagerStrategy;
using FileManagement::FileObject;

/**
 * The log specific file manager. Handles the specific writes of log data.
 */
class LogFileManager :
    public FileManager<LogCollection> {
 public:
  /**
   * Default Constructor.
   */
  LogFileManager()  = default;

  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  LogFileManager(const Aws::FileManagement::FileManagerStrategyOptions &options)
    : FileManager(options) {
  }

  explicit LogFileManager(const std::shared_ptr<Aws::FileManagement::DataManagerStrategy> &file_manager_strategy)
      : FileManager(file_manager_strategy)
  {
  }

  ~LogFileManager() override = default;

  void write(const LogCollection & data) override;

  /*  
    AWSClient will return 'InvalidParameterException' error when the log events in a
    single batch span more than 24 hours. Therefore the readBatch function will only
    return as many logs as can fit within the 24 hour span and the actual number of 
    logs batched may end up being less than the original batch_size.

    We must sort the log data chronologically because it is not guaranteed
    to be ordered chronologically in the file, but CloudWatch requires all
    puts in a single batch to be sorted chronologically
  */
  FileObject<LogCollection> readBatch(size_t batch_size) override;

  using Timestamp = long;
  Timestamp latestTime = 0;
};

}  // namespace Utils

  const long ONE_DAY_IN_MILLISEC = 24*60*60*1000;
  const long TWO_WEEK_IN_MILLISEC = 14*ONE_DAY_IN_MILLISEC;
}  // namespace CloudWatchLogs
}  // namespace Aws
