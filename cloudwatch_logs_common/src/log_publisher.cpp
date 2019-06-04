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

#include <list>
#include <memory>
#include <fstream>

constexpr int kMaxRetries = 1;

using namespace Aws::CloudWatchLogs;

LogPublisher::LogPublisher(
  const std::string & log_group, const std::string & log_stream,
  std::shared_ptr<Aws::CloudWatchLogs::Utils::CloudWatchFacade> cloudwatch_facade,
  Aws::SDKOptions options)
  : Publisher<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>()
{
  this->cloudwatch_facade_ = cloudwatch_facade;
  this->log_group_ = log_group;
  this->log_stream_ = log_stream;
  this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_GROUP;
  this->next_token = EMPTY_TOKEN;
  this->options_ = options;
}

LogPublisher::~LogPublisher()
{}

/**
 * Checks to see if a log group already exists and tries to create it if not.
 */
bool LogPublisher::CreateGroup()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors check_log_group_exists_status =
    this->cloudwatch_facade_->CheckLogGroupExists(this->log_group_);
  if (CW_LOGS_SUCCEEDED == check_log_group_exists_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOGSTREAM_DEBUG(__func__, "Found existing log group: " << log_group_);
    return true;
  }
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors create_log_group_status =
    this->cloudwatch_facade_->CreateLogGroup(this->log_group_);
  if (CW_LOGS_SUCCEEDED == create_log_group_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOG_DEBUG(__func__, "Successfully created log group.");
    return true;
  } else if (CW_LOGS_LOG_GROUP_ALREADY_EXISTS == create_log_group_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOG_DEBUG(__func__, "Log group already exists.");
    return true;
  } else {
    AWS_LOGSTREAM_ERROR(__func__, "Failed to create log group, status: " 
                        << create_log_group_status << ". Retrying ...");
    return false;
  }
}

/**
 * Checks to see if a log stream already exists and tries to create it if not.
 */
bool LogPublisher::CreateStream()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors check_log_stream_exists_status =
    this->cloudwatch_facade_->CheckLogStreamExists(this->log_group_, this->log_stream_,
                                                   nullptr);
  if (CW_LOGS_SUCCEEDED == check_log_stream_exists_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOGSTREAM_DEBUG(__func__, "Found existing log stream: " << this->log_stream_);
    return true;
  }
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors create_log_stream_status =
    this->cloudwatch_facade_->CreateLogStream(this->log_group_, this->log_stream_);
  if (CW_LOGS_SUCCEEDED == create_log_stream_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOG_DEBUG(__func__, "Successfully created log stream.");
    return true;
  } else if (CW_LOGS_LOG_STREAM_ALREADY_EXISTS == create_log_stream_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOG_DEBUG(__func__, "Log stream already exists");
    return true;
  } else {
    AWS_LOGSTREAM_ERROR(__func__, "Failed to create log stream, status: " 
                        << create_log_stream_status << ". Retrying ...");
    return false;
  }
}

/**
 * Fetches the token to use for writing logs to a stream. 
 */
bool LogPublisher::InitToken(Aws::String & next_token)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors get_token_status =
    this->cloudwatch_facade_->GetLogStreamToken(this->log_group_, this->log_stream_,
                                                next_token);
  if (CW_LOGS_SUCCEEDED == get_token_status) {
    return true;
  } else {
    AWS_LOGSTREAM_ERROR(__func__, "Unable to obtain the sequence token to use, status: " 
                        << get_token_status << ". Retrying ...");
    next_token = EMPTY_TOKEN;
    return false;
  }
}

bool LogPublisher::SendLogFiles(Aws::String & next_token, std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & logs) {
  auto status = SendLogs(next_token, logs);
  return status == CW_LOGS_SUCCEEDED;
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
          "Unable to send logs to CloudWatch and retried, dropping this batch of logs ...");
    }
  } else {
    AWS_LOG_DEBUG(__func__,
                  "Unable to obtain the sequence token to use");
  }
  // set the new observed state, will fire any registered listeners
  if (network_monitor_) {
    auto network_status = send_logs_status ?
                          Aws::DataFlow::Status::UNAVAILABLE : Aws::DataFlow::Status::AVAILABLE;
    network_monitor_->setStatus(network_status);
  }
  return send_logs_status;
}

bool LogPublisher::configure()
{
  //todo lock
  //attempt to fully configure
  if(!CreateGroup()) {
    return false;
  }
  if(!CreateStream()) {
    return false;
  }
  return InitToken(next_token);
}

bool LogPublisher::publishData(std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & data)
{
  //todo lock
  if(next_token == Aws::CloudWatchLogs::EMPTY_TOKEN) {
    return false; //InitToken failed
  }

  //attempt to configure
  bool b = this->configure();
  if (!b) {
    return false; //configuration failed, new state?
  }

  this->run_state_ = LOG_PUBLISHER_ATTEMPT_SEND_LOGS;

  bool success = SendLogFiles(next_token, data);

  this->run_state_ = LOG_PUBLISHER_FINISHED_SEND_LOGS; //we finished even if we failed, new state?
  return success;
}

bool LogPublisher::initialize() {
  Aws::InitAPI(this->options_); //TODO what happens when offline? //does this need to be called again?
  return true;
}

bool LogPublisher::shutdown() {
  Aws::ShutdownAPI(this->options_);
  return true;
}