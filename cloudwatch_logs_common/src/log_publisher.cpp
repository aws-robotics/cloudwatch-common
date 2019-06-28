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
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/cloudwatch_facade.h>

#include <dataflow_lite/utils/publisher.h>
#include <dataflow_lite/dataflow/source.h>

#include <list>
#include <memory>
#include <fstream>

constexpr int kMaxRetries = 1; // todo this should probably be configurable, maybe part of the generic publisher interface

using namespace Aws::CloudWatchLogs;

LogPublisher::LogPublisher(
  const std::string & log_group, const std::string & log_stream,
  const Aws::Client::ClientConfiguration & client_config)
  : Publisher<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>()
{
  this->client_config_ = client_config;
  this->log_group_ = log_group;
  this->log_stream_ = log_stream;
  this->cloudwatch_facade_ = nullptr;
  this->markOffline(); // reset token and set state to init
}

LogPublisher::~LogPublisher()
{
}

bool LogPublisher::checkIfConnected(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors error) {
  if (CW_LOGS_NOT_CONNECTED == error) {
    this->markOffline();
    return false;
  }
  return true;
}

/**
 * Checks to see if a log group already exists and tries to create it if not.
 */
bool LogPublisher::CreateGroup()
{
  auto status =
    this->cloudwatch_facade_->CheckLogGroupExists(this->log_group_);
  if (!checkIfConnected(status)) {
    return false;
  }

  AWS_LOGSTREAM_DEBUG(__func__,  "CheckLogGroupExists code:" << status);

  if (CW_LOGS_SUCCEEDED == status) {

    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOGSTREAM_DEBUG(__func__, "Found existing log group: " << log_group_);
    return true;
  }

  status = this->cloudwatch_facade_->CreateLogGroup(this->log_group_);
  if (!checkIfConnected(status)) {
    return false;
  }

  if (CW_LOGS_SUCCEEDED == status) {

    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOGSTREAM_DEBUG(__func__, "Successfully created log group.");
    return true;

  } else if (CW_LOGS_LOG_GROUP_ALREADY_EXISTS == status) {

    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOGSTREAM_INFO(__func__, "Log group already exists.");
    return true;

  } else {

    AWS_LOGSTREAM_ERROR(__func__, "Failed to create log group, status: "
                        << status << ". Retrying ...");
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
  if (!checkIfConnected(status)) {
    return false;
  }

  if (CW_LOGS_SUCCEEDED == status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOGSTREAM_DEBUG(__func__, "Found existing log stream: " << this->log_stream_);
    return true;
  }

  status = this->cloudwatch_facade_->CreateLogStream(this->log_group_, this->log_stream_);
  if (!checkIfConnected(status)) {
    return false;
  }

  if (CW_LOGS_SUCCEEDED == status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOG_DEBUG(__func__, "Successfully created log stream.");
    return true;
  } else if (CW_LOGS_LOG_STREAM_ALREADY_EXISTS == status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOG_DEBUG(__func__, "Log stream already exists");
    return true;

  } else {
    AWS_LOGSTREAM_ERROR(__func__, "Failed to create log stream, status: " 
                        << status << ". Retrying ...");
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
  if (!checkIfConnected(status)) {
    return false;
  }

  if (CW_LOGS_SUCCEEDED == status) {
    AWS_LOG_DEBUG(__func__, "Get Token succeeded");
    return true;
  } else {

    AWS_LOGSTREAM_ERROR(__func__, "Unable to obtain the sequence token to use, status: " 
                        << status << ". Retrying ...");
    resetInitToken();
    return false;
  }
}

bool LogPublisher::SendLogFiles(Aws::String & next_token, std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & logs) {
  auto status = SendLogs(next_token, logs);
  bool b = status == CW_LOGS_SUCCEEDED;
  AWS_LOG_DEBUG(__func__, "SendLogFiles status=%d", b);
  return b;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogPublisher::SendLogs(Aws::String & next_token, std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & data) {
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

  checkIfConnected(send_logs_status);  // mark offline if needed
  return send_logs_status;
}

void LogPublisher::markOffline()
{
  resetInitToken();
  this->run_state_ = LOG_PUBLISHER_INITIALIZED;
}

void LogPublisher::resetInitToken()
{
  this->next_token = EMPTY_TOKEN;
}

bool LogPublisher::publishData(std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & data)
{
  // attempt to configure, this will fail if offline
  bool b = this->configure();
  if (!b) {
    AWS_LOG_WARN(__func__, "configure FAILED");
    return false;
  }
  AWS_LOG_DEBUG(__func__, "configure succeeded");

  this->run_state_ = LOG_PUBLISHER_ATTEMPT_SEND_LOGS;
  bool success = SendLogFiles(next_token, data);
  AWS_LOG_DEBUG(__func__, "finished SendLogFiles");

  return success;
}

bool LogPublisher::configure()
{
  // attempt to fully configure
  if(!CreateGroup()) {
    AWS_LOG_WARN(__func__, "CreateGroup FAILED");
    return false;
  }

  AWS_LOG_DEBUG(__func__, "CreateGroup succeeded");

  // attempt to create stream
  if(!CreateStream()) {
    AWS_LOG_WARN(__func__,"CreateStream FAILED");
    return false;
  }
  AWS_LOG_DEBUG(__func__, "CreateGroup succeeded");

  // init and check if we have a valid token
  InitToken(next_token);
  if(next_token == Aws::CloudWatchLogs::EMPTY_TOKEN) {
    AWS_LOG_WARN(__func__, "INIT TOKEN FAILED");
    return false;
  }
  AWS_LOG_DEBUG(__func__, "INIT TOKEN succeeded");

  return true;
}

bool LogPublisher::start() {

  // only call if init is needed
  if (LOG_PUBLISHER_INITIALIZED != this->run_state_) {
    AWS_LOG_DEBUG(__func__, "Not initialized, skipping");
    return false;
  }

  if (!this->cloudwatch_facade_) {
    this->cloudwatch_facade_ = std::make_shared<Aws::CloudWatchLogs::Utils::CloudWatchFacade>(this->client_config_);
  }

  return true;
}

bool LogPublisher::shutdown() {
  Aws::ShutdownAPI(this->options_);
  return true;
}