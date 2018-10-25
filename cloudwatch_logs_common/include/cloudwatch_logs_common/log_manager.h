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
#include <aws/logs/model/PutLogEventsRequest.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/shared_object.h>

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

namespace Aws {
namespace CloudWatchLogs {

class LogManager
{
public:
  /**
   *  @brief Creates a new LogManager
   *  Creates a new LogManager that will group/buffer logs and then send them to the provided
   * log_publisher to be sent out to CloudWatch
   *
   *  @param log_publisher A shared pointer to a LogPublisher that will be used to publish the
   * buffered logs
   */
  LogManager(const std::shared_ptr<LogPublisher> log_publisher);

  /**
   *  @brief Tears down a LogManager object
   */
  virtual ~LogManager();

  /**
   *  @brief Used to add a log event to the buffer
   *  Adds the given log event to the internal buffer that will be flushed periodically.
   *  @param a string containing the log msg
   *
   *  @return An error code that will be SUCCESS if the log event was recorded successfully
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors RecordLog(
    const std::string & log_msg_formatted);

  /**
   *  @brief Services the log manager by performing periodic tasks when called.
   *  Calling the Service function allows for periodic tasks associated with the log manager, such
   * as flushing buffered logs, to be performed.
   *
   *  @return An error code that will be SUCCESS if all periodic tasks are executed successfully
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors Service();

private:
  std::shared_ptr<LogPublisher> log_publisher_ = nullptr;
  Aws::CloudWatchLogs::Utils::SharedObject<std::list<Aws::CloudWatchLogs::Model::InputLogEvent> *>
    shared_object_;
  std::list<Aws::CloudWatchLogs::Model::InputLogEvent> log_buffers_[2];
  uint8_t active_buffer_indx_ = 0;
};

}  // namespace CloudWatchLogs
}  // namespace Aws
