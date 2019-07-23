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
#include <cloudwatch_logs_common/utils/shared_object.h>

#include <memory>
#include <fstream>

constexpr int kMaxRetries = 1;

using namespace Aws::CloudWatchLogs;

LogPublisher::LogPublisher(
  const std::string & log_group, const std::string & log_stream,
  std::shared_ptr<Aws::CloudWatchLogs::Utils::CloudWatchFacade> cloudwatch_facade)
{
  this->cloudwatch_facade_ = cloudwatch_facade;
  this->shared_logs_.store(nullptr, std::memory_order_release);
  this->publisher_thread_ = nullptr;
  this->log_group_ = log_group;
  this->log_stream_ = log_stream;
  this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_GROUP;
}

LogPublisher::~LogPublisher()
{
  if (nullptr != this->publisher_thread_) {
    AWS_LOG_INFO(__func__, "Shutting down Log Publisher");
    this->LogPublisher::StopPublisherThread();
  }
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogPublisher::PublishLogs(
  Utils::SharedObject<LogTypePtr> * shared_logs)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;
  if (nullptr == shared_logs) {
    AWS_LOG_WARN(__func__,
                 "Failed to update log set to be send to CloudWatch due to logs are nullptr");
    return CW_LOGS_NULL_PARAMETER;
  } else if (!shared_logs->isDataAvailable()) {
    AWS_LOG_WARN(__func__,
                 "Failed to update log set to be send to CloudWatch due to shared object is busy");
    return CW_LOGS_DATA_LOCKED;
  } else if (nullptr == this->publisher_thread_) {
    AWS_LOG_WARN(__func__,
                 "Failed to update log set to be send to CloudWatch due to publisher thread is not "
                 "initialized");
    return CW_LOGS_PUBLISHER_THREAD_NOT_INITIALIZED;
  } else if (nullptr != this->shared_logs_.load(std::memory_order_acquire)) {
    AWS_LOG_WARN(
      __func__,
      "Failed to update log set to be send to CloudWatch due to logs cannot be loaded into memory");
    return CW_LOGS_THREAD_BUSY;
  }

  this->shared_logs_.store(shared_logs, std::memory_order_release);

  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogPublisher::StartPublisherThread()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;
  if (nullptr != this->publisher_thread_) {
    AWS_LOG_WARN(
      __func__,
      "Failed to start publisher thread because publisher thread was already initialized.");
    return CW_LOGS_THREAD_BUSY;
  }
  this->thread_keep_running_.store(true, std::memory_order_release);
  this->publisher_thread_ = new std::thread(&LogPublisher::Run, this);
  AWS_LOG_INFO(__func__, "Started publisher thread");
  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogPublisher::StopPublisherThread()
{
  if (nullptr == this->publisher_thread_) {
    AWS_LOG_WARN(__func__,
                 "Failed to stop publisher thread because publisher thread was not initialized.");
    return CW_LOGS_PUBLISHER_THREAD_NOT_INITIALIZED;
  }
  this->thread_keep_running_.store(false, std::memory_order_release);
  this->publisher_thread_->join();
  delete this->publisher_thread_;
  this->publisher_thread_ = nullptr;
  AWS_LOG_INFO(__func__, "Stopped publisher thread");
  return CW_LOGS_SUCCEEDED;
}

/**
 * Checks to see if a log group already exists and tries to create it if not.
 */
void LogPublisher::CreateGroup()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors check_log_group_exists_status =
    this->cloudwatch_facade_->CheckLogGroupExists(this->log_group_);
  if (CW_LOGS_SUCCEEDED == check_log_group_exists_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOGSTREAM_DEBUG(__func__, "Found existing log group: " << log_group_);
    return;
  }
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors create_log_group_status =
    this->cloudwatch_facade_->CreateLogGroup(this->log_group_);
  if (CW_LOGS_SUCCEEDED == create_log_group_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOG_DEBUG(__func__, "Successfully created log group.");
  } else if (CW_LOGS_LOG_GROUP_ALREADY_EXISTS == create_log_group_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_CREATE_STREAM;
    AWS_LOG_DEBUG(__func__, "Log group already exists, quit attempting to create it.");
  } else {
    AWS_LOGSTREAM_ERROR(__func__, "Failed to create log group, status: " 
                        << create_log_group_status << ". Retrying ...");
  }
}

/**
 * Checks to see if a log stream already exists and tries to create it if not.
 */
void LogPublisher::CreateStream()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors check_log_stream_exists_status =
    this->cloudwatch_facade_->CheckLogStreamExists(this->log_group_, this->log_stream_,
                                                   nullptr);
  if (CW_LOGS_SUCCEEDED == check_log_stream_exists_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOGSTREAM_DEBUG(__func__, "Found existing log stream: " << this->log_stream_);
    return;
  }
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors create_log_stream_status =
    this->cloudwatch_facade_->CreateLogStream(this->log_group_, this->log_stream_);
  if (CW_LOGS_SUCCEEDED == create_log_stream_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOG_DEBUG(__func__, "Successfully created log stream.");
  } else if (CW_LOGS_LOG_STREAM_ALREADY_EXISTS == create_log_stream_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_INIT_TOKEN;
    AWS_LOG_DEBUG(__func__, "Log stream already exists, quit attempting to create it.");
  } else {
    AWS_LOGSTREAM_ERROR(__func__, "Failed to create log stream, status: " 
                        << create_log_stream_status << ". Retrying ...");
  }
}

/**
 * Fetches the token to use for writing logs to a stream. 
 */
void LogPublisher::InitToken(Aws::String & next_token)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors get_token_status =
    this->cloudwatch_facade_->GetLogStreamToken(this->log_group_, this->log_stream_,
                                                next_token);
  if (CW_LOGS_SUCCEEDED == get_token_status) {
    this->run_state_ = Aws::CloudWatchLogs::LOG_PUBLISHER_RUN_SEND_LOGS;
  } else {
    AWS_LOGSTREAM_ERROR(__func__, "Unable to obtain the sequence token to use, status: " 
                        << get_token_status << ". Retrying ...");
  }
}

void LogPublisher::SendLogFiles(Aws::String & next_token) {
  if (queue_monitor_) {
    AWS_LOG_INFO(__func__,
                 "Attempting to get data off queue");
    auto data = queue_monitor_->dequeue();
    if (data) {
      AWS_LOG_INFO(__func__,
                    "Attempting to send log file data");
      LogType batch_data = data->getBatchData();
      auto status = SendLogs(next_token, &batch_data);
      using Aws::FileManagement::UploadStatus;
      UploadStatus upload_status = (status == CW_LOGS_SUCCEEDED) ?
          UploadStatus::SUCCESS : UploadStatus::FAIL;
      data->onComplete(upload_status);
    }
  }
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogPublisher::SendLogs(Aws::String & next_token, LogTypePtr logs) {
  AWS_LOG_DEBUG(__func__,
                "Attempting to use logs of size %i", logs->size());
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors send_logs_status = CW_LOGS_FAILED;
  if (!logs->empty()) {
    int tries = kMaxRetries;
    while (CW_LOGS_SUCCEEDED != send_logs_status && tries > 0) {
      AWS_LOG_INFO(__func__, "Sending logs to CW");
      if (!std::ifstream("/tmp/internet").good()) {
        send_logs_status = this->cloudwatch_facade_->SendLogsToCloudWatch(
            next_token, this->log_group_, this->log_stream_, logs);
      }
      if (CW_LOGS_SUCCEEDED != send_logs_status) {
        AWS_LOG_WARN(__func__, "Unable to send logs to CloudWatch, retrying ...");
        Aws::CloudWatchLogs::ROSCloudWatchLogsErrors get_token_status =
            this->cloudwatch_facade_->GetLogStreamToken(this->log_group_, this->log_stream_,
                                                        next_token);
        if (CW_LOGS_SUCCEEDED != get_token_status) {
          AWS_LOG_WARN(__func__,
                       "Unable to obtain the sequence token to use, quit attempting to send "
                       "logs to CloudWatch");
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
                  "Unable to obtain the sequence token to use, quit attempting to send "
                  "logs to CloudWatch");
  }
  if (network_monitor_) {
    auto network_status = send_logs_status ?
                          Aws::FileManagement::Status::UNAVAILABLE : Aws::FileManagement::Status::AVAILABLE;
    network_monitor_->setStatus(network_status);
  }
  return send_logs_status;
}
/**
 * Checks if there are logs ready to be sent out. If so then it attempts to send them
 * to CloudWatch.
 */
void LogPublisher::SendLogs(Aws::String & next_token)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors publisher_status;
  auto shared_logs_obj = this->shared_logs_.load(std::memory_order_acquire);
  if (nullptr != shared_logs_obj) {
    std::list<Aws::CloudWatchLogs::Model::InputLogEvent> * logs;
    publisher_status = shared_logs_obj->getDataAndLock(logs);
    Aws::CloudWatchLogs::ROSCloudWatchLogsErrors send_logs_status = CW_LOGS_FAILED;
    if (publisher_status == CW_LOGS_SUCCEEDED) {
      send_logs_status = SendLogs(next_token, logs);
    }
    if (this->upload_status_function_) {
      AWS_LOG_INFO(__func__,
                    "Calling callback function with logs of size %i", logs->size());
      upload_status_function_(send_logs_status, *logs);
    }
    /* Null out the class reference before unlocking the object. The external thread will be
       looking to key off if the shared object is locked or not to know it can start again, but
       the PublishLogs function checks if the the pointer is null or not. */
    this->shared_logs_.store(nullptr, std::memory_order_release);
    shared_logs_obj->freeDataAndUnlock();
  }
}

void LogPublisher::SetNetworkMonitor(
    std::shared_ptr<Aws::FileManagement::StatusMonitor> &network_monitor)
{
  network_monitor_ = network_monitor;
}

void LogPublisher::SetLogFileManager(
    std::shared_ptr<Utils::LogFileManager> &log_file_manager)
{
  log_file_manager_ = log_file_manager;
  using namespace std::placeholders;
  upload_status_function_ =
      std::bind(&Utils::LogFileManager::uploadCompleteStatus, log_file_manager_, _1, _2);
}

void LogPublisher::Run()
{
  Aws::String next_token;
  this->run_state_ = LOG_PUBLISHER_RUN_CREATE_GROUP;
  
  while (this->thread_keep_running_.load(std::memory_order_acquire)) {
    LogPublisherRunState previous_state = this->run_state_;
    switch (this->run_state_) {
      case LOG_PUBLISHER_RUN_CREATE_GROUP:
        CreateGroup();
        break;
      case LOG_PUBLISHER_RUN_CREATE_STREAM:
        CreateStream();
        break;
      case LOG_PUBLISHER_RUN_INIT_TOKEN:
        InitToken(next_token);
        break;
      case LOG_PUBLISHER_RUN_SEND_LOGS:
        SendLogs(next_token);
        SendLogFiles(next_token);
        break;
      default:
        AWS_LOGSTREAM_ERROR(__func__, "Unknown state!");
        break;
    }
    if (previous_state == this->run_state_) {
      // Don't sleep between state changes for a faster startup
      std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }
  }
}
