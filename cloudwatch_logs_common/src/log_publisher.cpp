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

#include <functional>
#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/logs/CloudWatchLogsClient.h>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/utils/cloudwatch_logs_facade.h>

#include <dataflow_lite/utils/publisher.h>
#include <dataflow_lite/dataflow/source.h>

#include <cloudwatch_logs_common/definitions/definitions.h>
#include <cloudwatch_logs_common/definitions/ros_cloudwatch_logs_errors.h>

#include <list>
#include <memory>
#include <fstream>
#include <utility>


namespace Aws {
namespace CloudWatchLogs {

constexpr int kMaxRetries = 1; // todo this should probably be configurable, maybe part of the generic publisher interface

LogPublisher::LogPublisher(
  const std::string & log_group,
  const std::string & log_stream,
  const Aws::Client::ClientConfiguration & client_config)
  : run_state_(LOG_PUBLISHER_RUN_CREATE_GROUP)
{
  this->client_config_ = client_config;
  this->log_group_ = log_group;
  this->log_stream_ = log_stream;
  this->cloudwatch_facade_ = nullptr;
}

LogPublisher::LogPublisher(
  const std::string & log_group,
  const std::string & log_stream,
  std::shared_ptr<Aws::CloudWatchLogs::Utils::CloudWatchLogsFacade> cloudwatch_facade)
  : run_state_(LOG_PUBLISHER_RUN_CREATE_GROUP)
{
  this->cloudwatch_facade_ = std::move(cloudwatch_facade);
  this->log_group_ = log_group;
  this->log_stream_ = log_stream;
}

LogPublisher::~LogPublisher()
= default;

bool LogPublisher::CheckIfConnected(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors error) {
  return CW_LOGS_NOT_CONNECTED != error;
}

/**
 * Checks to see if a log group already exists and tries to create it if not.
 */
bool LogPublisher::CreateGroup()
{
  auto status =
    this->cloudwatch_facade_->CheckLogGroupExists(this->log_group_);

  if (!CheckIfConnected(status)) {
    return false;
  }

  AWS_LOGSTREAM_DEBUG(__func__,  "CheckLogGroupExists code:" << status);

  if (CW_LOGS_SUCCEEDED == status) {

    this->run_state_.SetValue(Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM);
    AWS_LOGSTREAM_DEBUG(__func__, "Found existing log group: " << log_group_);
    return true;
  }

  status = this->cloudwatch_facade_->CreateLogGroup(this->log_group_);

  if (!CheckIfConnected(status)) {
    return false;
  }

  if (CW_LOGS_SUCCEEDED == status) {

    this->run_state_.SetValue(Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM);
    AWS_LOGSTREAM_DEBUG(__func__, "Successfully created log group.");
    return true;

  } else if (CW_LOGS_LOG_GROUP_ALREADY_EXISTS == status) {

    this->run_state_.SetValue(Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM);
    AWS_LOGSTREAM_INFO(__func__, "Log group already exists.");
    return true;

  } else {

    AWS_LOGSTREAM_ERROR(__func__, "Failed to create log group, status: "
                        << status);
    return false;
  }
}

/**
 * Checks to see if a log stream already exists and tries to create it if not.
 */
bool LogPublisher::CreateStream()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status =
    this->cloudwatch_facade_->CheckLogStreamExists(this->log_group_, this->log_stream_, nullptr);
  if (!CheckIfConnected(status)) {
    return false;
  }

  if (CW_LOGS_SUCCEEDED == status) {
    this->run_state_.SetValue(Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN);
    AWS_LOGSTREAM_DEBUG(__func__, "Found existing log stream: " << this->log_stream_);
    return true;
  }

  status = this->cloudwatch_facade_->CreateLogStream(this->log_group_, this->log_stream_);
  if (!CheckIfConnected(status)) {
    return false;
  }

  if (CW_LOGS_SUCCEEDED == status) {
    this->run_state_.SetValue(Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN);
    AWS_LOG_DEBUG(__func__, "Successfully created log stream.");
    return true;
  } else if (CW_LOGS_LOG_STREAM_ALREADY_EXISTS == status) {
    this->run_state_.SetValue(Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN);
    AWS_LOG_DEBUG(__func__, "Log stream already exists");
    return true;

  } else {
    AWS_LOGSTREAM_ERROR(__func__, "Failed to create log stream, status: " 
                        << status);
    return false;
  }
}

/**
 * Fetches the token to use for writing logs to a stream. 
 */
bool LogPublisher::InitToken(Aws::String & next_token)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status =
    this->cloudwatch_facade_->GetLogStreamToken(this->log_group_, this->log_stream_,
                                                next_token);
  if (!CheckIfConnected(status)) {
    // don't reset token, could still be valid
    return false;
  }

  if (CW_LOGS_SUCCEEDED == status) {
    AWS_LOG_DEBUG(__func__, "Get Token succeeded");
    return true;
  } else {

    AWS_LOGSTREAM_ERROR(__func__, "Unable to obtain the sequence token to use, status: " 
                        << status);
    ResetInitToken(); // reset token given error
    return false;
  }
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogPublisher::SendLogs(Aws::String & next_token, LogCollection & data) {

  AWS_LOG_DEBUG(__func__,
                "Attempting to use logs of size %i", data.size());

  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors send_logs_status = CW_LOGS_FAILED;
  if (!data.empty()) {
    int tries = kMaxRetries;
    while (CW_LOGS_SUCCEEDED != send_logs_status && tries > 0) {

      AWS_LOG_INFO(__func__, "Sending logs to CW");

      if (!std::ifstream("/tmp/internet").good()) {
        send_logs_status = this->cloudwatch_facade_->SendLogsToCloudWatch(
            next_token, this->log_group_, this->log_stream_, data);
        AWS_LOG_DEBUG(__func__, "SendLogs status=%d", send_logs_status);
      }

      if (CW_LOGS_SUCCEEDED != send_logs_status) {
        AWS_LOG_WARN(__func__, "Unable to send logs to CloudWatch, retrying ...");
        Aws::CloudWatchLogs::ROSCloudWatchLogsErrors get_token_status =
            this->cloudwatch_facade_->GetLogStreamToken(this->log_group_, this->log_stream_,
                                                        next_token);
        if (CW_LOGS_SUCCEEDED != get_token_status) {
          AWS_LOG_WARN(__func__,
                       "Unable to obtain the sequence token to use");
          break;
        }
      }
      tries--;
    }
    if (CW_LOGS_SUCCEEDED != send_logs_status) {
      AWS_LOG_WARN(
              __func__,
              "Unable to send logs to CloudWatch");
    }
  } else {
    AWS_LOG_DEBUG(__func__,
                  "Unable to obtain the sequence token to use");
  }

  CheckIfConnected(send_logs_status);  // mark offline if needed
  return send_logs_status;
}

void LogPublisher::ResetInitToken()
{
  this->next_token_ = kUninitializedToken;
}
LogPublisherRunState LogPublisher::GetRunState() {
  return this->run_state_.GetValue();
}

bool LogPublisher::Configure() {

  if (GetRunState() == LOG_PUBLISHER_RUN_CREATE_GROUP) {
    // attempt to create group
    if (!CreateGroup()) {
      AWS_LOG_WARN(__func__, "CreateGroup FAILED");
      return false;
    }
    AWS_LOG_DEBUG(__func__, "CreateGroup succeeded");
  }

  if (GetRunState() == LOG_PUBLISHER_RUN_CREATE_STREAM) {
    // attempt to create stream
    if (!CreateStream()) {
      AWS_LOG_WARN(__func__, "CreateStream FAILED");
      return false;
    }
    AWS_LOG_DEBUG(__func__, "CreateGroup succeeded");
  }

  if (GetRunState() == LOG_PUBLISHER_RUN_INIT_TOKEN) {

    // init and check if we have a valid token
    bool token_success = InitToken(next_token_);

    if(!token_success || next_token_ == Aws::CloudWatchLogs::kUninitializedToken) {
      AWS_LOG_WARN(__func__, "INIT TOKEN FAILED");
      return false;
    }
    AWS_LOG_DEBUG(__func__, "INIT TOKEN succeeded");
  }

  return true;
}

Aws::DataFlow::UploadStatus LogPublisher::PublishData(LogCollection & data)
{

  // if no data don't attempt to configure or publish
  if (data.empty()) {
    AWS_LOG_DEBUG(__func__, "no data to publish");
    return Aws::DataFlow::INVALID_DATA;
  }

  // attempt to configure (if needed, based on run_state_)
  if (!Configure()) {
    return Aws::DataFlow::FAIL;
  }

  AWS_LOG_DEBUG(__func__, "attempting to SendLogFiles");

  // all config succeeded: attempt to publish
  this->run_state_.SetValue(LOG_PUBLISHER_ATTEMPT_SEND_LOGS);
  auto status = SendLogs(next_token_, data);

  // if failed attempt to get the token next time
  // otherwise everything succeeded to attempt to send logs again
  this->run_state_.SetValue(status == CW_LOGS_SUCCEEDED ? LOG_PUBLISHER_ATTEMPT_SEND_LOGS : LOG_PUBLISHER_RUN_INIT_TOKEN);
  AWS_LOG_DEBUG(__func__, "finished SendLogs");

  switch(status) {

    case CW_LOGS_SUCCEEDED:
      return Aws::DataFlow::SUCCESS;
    case CW_LOGS_INVALID_PARAMETER:
      return Aws::DataFlow::INVALID_DATA;
    default:
      AWS_LOG_WARN(__func__, "error finishing SendLogs %d", status);
      return Aws::DataFlow::FAIL;
  }
}

bool LogPublisher::Start() {

  if (!this->cloudwatch_facade_) {
    this->cloudwatch_facade_ = std::make_shared<Aws::CloudWatchLogs::Utils::CloudWatchLogsFacade>(this->client_config_);
  }

  return Service::Start();
}

bool LogPublisher::Shutdown() {
  bool is_shutdown = Publisher::Shutdown();
  ResetInitToken();
  Aws::ShutdownAPI(this->options_);
  return is_shutdown;
}

}  // namespace CloudWatchLogs
}  // namespace Aws
