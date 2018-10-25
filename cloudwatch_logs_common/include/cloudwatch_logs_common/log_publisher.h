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
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/cloudwatch_facade.h>
#include <cloudwatch_logs_common/utils/shared_object.h>

#include <memory>
#include <thread>

namespace Aws {
namespace CloudWatchLogs {

/**
 *  @brief Class that handles sending logs data to CloudWatch
 *  This class is responsible for emitting all the stored logs to AWS CloudWatch.
 *  Logs are published asynchronously using a thread. The thread waits on a condition
 *  variable and is signaled (by AWSCloudWatchLogManager) whenever new logs are
 *  available.
 */
class LogPublisher
{
public:
  /**
   *  @brief Creates a LogPublisher object that uses the provided client and SDK configuration
   *  Constructs a LogPublisher object that will use the provided CloudWatchClient and SDKOptions
   * when it publishes logs.
   *
   *  @param cw_client The CloudWatchClient that this publisher will use when pushing logs to
   * CloudWatch.
   */
  LogPublisher(const std::string & log_group, const std::string & log_stream,
               std::shared_ptr<Aws::CloudWatchLogs::Utils::CloudWatchFacade> cw_client);

  /**
   *  @brief Tears down the LogPublisher object
   */
  virtual ~LogPublisher();

  /**
   *  @brief Asynchronously publishes the provided logs to CloudWatch.
   *  This function will start an asynchronous request to CloudWatch to post the provided logs. If
   * the publisher is already in the process of publishing a previous batch of logs it will fail
   * with AWS_ERR_ALREADY.
   *
   *  @param shared_logs A reference to a shared object that contains a map where the key is a
   * string representing logs group name and the value is a map where the key is a string
   *      representing log stream and the value is a list containing the logs to be published
   *  @return An error code that will be SUCCESS if it started publishing successfully, otherwise it
   * will be an error code.
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors PublishLogs(
    Aws::CloudWatchLogs::Utils::SharedObject<
      std::list<Aws::CloudWatchLogs::Model::InputLogEvent> *> * shared_logs);

  /**
   *  @brief Starts the background thread used by the publisher.
   *  Use this function in order to start the background thread that the publisher uses for the
   * asynchronous posting of logs to CloudWatch
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors StartPublisherThread();

  /**
   *  @brief Stops the background thread used by the publisher.
   *  Use this function in order to attempt a graceful shutdown of the background thread that the
   * publisher uses.
   *
   *  @return An error code that will be SUCCESS if the thread was shutdown successfully
   */
  virtual Aws::CloudWatchLogs::ROSCloudWatchLogsErrors StopPublisherThread();

private:
  void Run();

  std::shared_ptr<Aws::CloudWatchLogs::Utils::CloudWatchFacade> cloudwatch_facade_;
  std::shared_ptr<Aws::CloudWatchLogs::CloudWatchLogsClient> cloudwatch_client_;
  Aws::SDKOptions aws_sdk_options_;
  std::thread * publisher_thread_;
  std::atomic_bool thread_keep_running_;
  std::atomic<Aws::CloudWatchLogs::Utils::SharedObject<
    std::list<Aws::CloudWatchLogs::Model::InputLogEvent> *> *>
    shared_logs_;
  std::string log_group_;
  std::string log_stream_;
  bool does_stream_exist_;
  bool does_group_exist_;
};

}  // namespace CloudWatchLogs
}  // namespace Aws