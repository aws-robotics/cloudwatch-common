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
#include <cloudwatch_logs_common/utils/log_file_manager.h>
#include <cloudwatch_logs_common/file_upload/task_utils.h>
#include <cloudwatch_logs_common/dataflow/status_monitor.h>
#include <cloudwatch_logs_common/dataflow/queue_monitor.h>
#include <cloudwatch_logs_common/file_upload/file_upload_streamer.h>

#include <cloudwatch_logs_common/utils/publisher.h>

#include <list>
#include <memory>
#include <thread>

namespace Aws {
namespace CloudWatchLogs {

/** 
 * @enum Aws::CloudWatchLogs::LogPublisherRunState
 * @brief Defines the different runtime states for the Publisher
 * This enum is used by the LogPublisher to track the current runtime state of the Run function
 */
enum LogPublisherRunState {
  LOG_PUBLISHER_INITIALIZED, // also not connected / offline, need to start from the beginning
  LOG_PUBLISHER_RUN_CREATE_GROUP,
  LOG_PUBLISHER_RUN_CREATE_STREAM,
  LOG_PUBLISHER_RUN_INIT_TOKEN,
  LOG_PUBLISHER_ATTEMPT_SEND_LOGS,
};

const static Aws::String EMPTY_TOKEN = "";

/**
 * Wrapping class around the CloudWatch Logs API.
 */
class LogPublisher : public Publisher<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>
{
public:
  using LogType = std::list<Aws::CloudWatchLogs::Model::InputLogEvent>; //todo in types class? this was not easy to find
  /**
   *  @brief Creates a LogPublisher object that uses the provided client and SDK configuration
   *  Constructs a LogPublisher object that will use the provided CloudWatchClient and SDKOptions
   * when it publishes logs.
   *
   *  @param cw_client The CloudWatchClient that this publisher will use when pushing logs to
   * CloudWatch.
   */
  LogPublisher(const std::string & log_group, const std::string & log_stream,
               const Aws::Client::ClientConfiguration & client_config, Aws::SDKOptions options);

  /**
   *  @brief Tears down the LogPublisher object
   */
  virtual ~LogPublisher();

  using Task = Aws::FileManagement::Task<LogType>;
  using LogTaskSource = std::shared_ptr<Aws::DataFlow::Source<std::shared_ptr<Task>>>;
  virtual inline void SetLogTaskSource(LogTaskSource &queue_monitor) {
    queue_monitor_ = queue_monitor;
  }
  virtual bool shutdown() override;
  /**
   * Initialize the AWS API and create the cloudwatch facade
   * @return
   */
  virtual bool start() override;

private:

  bool checkIfConnected(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors error);

  /**
   * Reset the current init token to EMPTY_TOKEN
   */
  void resetInitToken();
  /**
  * Handle internal state reset and cleanup when we receive a NOT_CONNECTED status
  */
  void markOffline();
  //config
  bool CreateGroup();
  bool CreateStream();
  bool InitToken(Aws::String & next_token);

  //overall config
  bool configure() override;

  //main publish mechanism
  bool publishData(std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & data) override;

  bool SendLogFiles(Aws::String & next_token, std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & logs);
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors SendLogs(Aws::String & next_token, std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & data);

  std::shared_ptr<Utils::LogFileManager> log_file_manager_ ;
  LogTaskSource queue_monitor_;
  std::shared_ptr<Aws::CloudWatchLogs::Utils::CloudWatchFacade> cloudwatch_facade_;
  std::shared_ptr<Aws::CloudWatchLogs::CloudWatchLogsClient> cloudwatch_client_;
  Aws::SDKOptions aws_sdk_options_;
  std::string log_group_; //todo const?
  std::string log_stream_; //todo const?
  Aws::SDKOptions options_; //todo const?
  Aws::Client::ClientConfiguration client_config_; //todo const?

  LogPublisherRunState run_state_; // todo atomic?
  Aws::String next_token;
  mutable std::recursive_mutex mtx_;
};

}  // namespace CloudWatchLogs
}  // namespace Aws
