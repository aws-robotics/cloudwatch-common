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

#pragma once

#include <aws/core/Aws.h>
#include <aws/logs/model/InputLogEvent.h>

#include <file_management/file_upload/file_upload_streamer.h>
#include <file_management/file_upload/file_manager.h>

#include <cloudwatch_logs_common/log_batcher.h>
#include <cloudwatch_logs_common/log_publisher.h>

#include <dataflow_lite/utils/service.h>
#include <dataflow_lite/cloudwatch/cloudwatch_service.h>

#include <chrono>
#include <stdexcept>
#include <string>

namespace Aws {
namespace CloudWatchLogs {

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;

/**
 * Implementation to send logs to Cloudwatch. Note: though the batcher and publisher are required, the file streamer
 * is not. If the file streamer is not provided then log data is dropped if any failure is observed during the
 * attempt to publish.
 */
class LogService : public Aws::CloudWatch::CloudWatchService<std::string, Aws::CloudWatchLogs::Model::InputLogEvent> {
public:

    /**
     * Construct a new instance of LogService.
     *
     * @param publisher used to publish logs to CloudWatch
     * @param batcher used to batch / queue logs before publishing
     * @param file_upload_streamer used to save logs data and upload later in the event of network connectivity changes
     */
  LogService(std::shared_ptr<Publisher<LogType>> log_publisher,
             std::shared_ptr<DataBatcher<Aws::CloudWatchLogs::Model::InputLogEvent>> log_batcher,
             std::shared_ptr<FileUploadStreamer<LogType>> log_file_upload_streamer = nullptr)
          : CloudWatchService(log_publisher, log_batcher) {

    this->file_upload_streamer_ = log_file_upload_streamer; // allow null, all this means is failures aren't written to file
  }

  /**
  * Convert an input string and timestamp to a log event.
  *
  * @param input string input to be sent as a log
  * @param milliseconds timestamp of the log event
  * @return the AWS SDK log object to  be send to CloudWatch
  */
  virtual Aws::CloudWatchLogs::Model::InputLogEvent convertInputToBatched(
          const std::string &input,
          const std::chrono::milliseconds &milliseconds) override {

    Aws::CloudWatchLogs::Model::InputLogEvent log_event;

    log_event.SetMessage(input.c_str());
    log_event.SetTimestamp(milliseconds.count());

    return log_event;
  }

  /**
  * Convert an input string to a log event. The current system time is used as the log event timestamp.
  *
  * @param input string input to be sent as a log
  * @return the AWS SDK log object to  be send to CloudWatch
  */
  virtual Aws::CloudWatchLogs::Model::InputLogEvent convertInputToBatched(
          const std::string &input) override {

    Aws::CloudWatchLogs::Model::InputLogEvent log_event;

    log_event.SetMessage(input.c_str());
    log_event.SetTimestamp(this->getCurrentTimestamp().count());

    return log_event;
  }

};


}  // namespace CloudWatchlogs
}  // namespace AWS

