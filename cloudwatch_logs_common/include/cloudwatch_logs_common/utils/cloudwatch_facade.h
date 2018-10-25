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
#include <aws/logs/CloudWatchLogsClient.h>
#include <aws/logs/model/InputLogEvent.h>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

/**
 *  @brief This class is a simple Facade over the CloudWatch client.
 *  This class is a very small abstraction over the CloudWatch client. It allows us to change the
 * details of how we're communicating with CloudWatch without the need to expose this in the rest of
 * our code. It also provides a shim for us to be able to Mock to unit test the rest of the code.
 *
 *  This class expects Aws::InitAPI() to have already been called before an instance is constructed
 *
 */
class CloudWatchFacade
{
public:
  /**
   *  @brief Creates a new CloudWatchFacade
   *
   *  @param client_config The configuration for the cloudwatch client
   */
  CloudWatchFacade(const Aws::Client::ClientConfiguration & client_config);
  virtual ~CloudWatchFacade() = default;

  /**
   *  @brief Sends a list of logs to CloudWatch
   *
   *  @param next_token The next sequence token to use for sending logs to cloudwatch
   *  @param log_group A reference to a string with the log group name for all the logs being posted
   *  @param log_stream A reference to a string with the log stream name for all the logs being
   * posted
   *  @param logs A reference to a list of logs that you want sent to CloudWatch
   *  @return An error code that will be SUCCESS if all logs were sent successfully.
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors SendLogsToCloudWatch(
    Aws::String & next_token, const std::string & log_group, const std::string & log_stream,
    std::list<Aws::CloudWatchLogs::Model::InputLogEvent> * logs);

  /**
   * @brief Creates a log group
   *
   * @param log_group Name of the log group
   * @return An error code that will be SUCCESS if log group is successfully created
   *         or resource already exists
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CreateLogGroup(
    const std::string & log_group);

  /**
   * @brief Creates a log stream in the specified log group
   *
   * @param log_group Name of the log group
   * @param log_stream Name of the stream
   * @return An error code that will be SUCCESS if log stream is successfully created
   *         or resource already exists
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CreateLogStream(
    const std::string & log_group, const std::string & log_stream);
  /**
   * @brief Gets the next sequence token to use for sending logs to cloudwatch
   *
   * @param log_group Name of the log group
   * @param log_stream Name of the stream
   * @return An error code of SUCCESS if the specified log stream in the log group
   *         can be found and has a next sequence token (a new stream will not have a sequence
   * token)
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors GetLogStreamToken(
    const std::string & log_group, const std::string & log_stream, Aws::String & next_token);

protected:
  CloudWatchFacade() = default;

private:
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors SendLogsRequest(
    const Aws::CloudWatchLogs::Model::PutLogEventsRequest & request, Aws::String & next_token);

  Aws::CloudWatchLogs::CloudWatchLogsClient cw_client_;
};

}  // namespace Utils
}  // namespace CloudWatchLogs
}  // namespace Aws
