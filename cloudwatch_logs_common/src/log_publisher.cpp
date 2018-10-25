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

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/logs/CloudWatchLogsClient.h>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/cloudwatch_facade.h>
#include <cloudwatch_logs_common/utils/shared_object.h>

#include <memory>

constexpr int kMaxRetries = 5;

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
  this->does_stream_exist_ = false;
  this->does_group_exist_ = false;
}

LogPublisher::~LogPublisher()
{
  if (nullptr != this->publisher_thread_) {
    AWS_LOG_INFO(__func__, "Shutting down Log Publisher");
    this->LogPublisher::StopPublisherThread();
  }
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors LogPublisher::PublishLogs(
  Utils::SharedObject<std::list<Aws::CloudWatchLogs::Model::InputLogEvent> *> * shared_logs)
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

void LogPublisher::Run()
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors publisher_status = CW_LOGS_SUCCEEDED;
  Aws::String next_token;
  this->cloudwatch_facade_->GetLogStreamToken(this->log_group_, this->log_stream_, next_token);
  while (this->thread_keep_running_.load(std::memory_order_acquire)) {
    // if log group cannot be created or log stream doesnt already exist, don't try to send to CW
    if (!this->does_group_exist_) {
      Aws::CloudWatchLogs::ROSCloudWatchLogsErrors create_log_group_status =
        this->cloudwatch_facade_->CreateLogGroup(this->log_group_);
      if (CW_LOGS_SUCCEEDED == create_log_group_status) {
        this->does_group_exist_ = true;
        AWS_LOG_DEBUG(__func__, "Successfully created log group.");
      } else if (CW_LOGS_LOG_GROUP_ALREADY_EXISTS == create_log_group_status) {
        this->does_group_exist_ = true;
        AWS_LOG_DEBUG(__func__, "Log group already exists, quit attempting to create it.");
      } else {
        AWS_LOG_ERROR(__func__, "Failed to create log group, retrying ...");
        continue;
      }
    }

    // if log stream cannot be created or log group doesnt already exist, don't try to send to CW
    if (!this->does_stream_exist_) {
      Aws::CloudWatchLogs::ROSCloudWatchLogsErrors create_log_stream_status =
        this->cloudwatch_facade_->CreateLogStream(this->log_group_, this->log_stream_);
      if (CW_LOGS_SUCCEEDED == create_log_stream_status) {
        this->does_stream_exist_ = true;
        AWS_LOG_DEBUG(__func__, "Successfully created log stream.");
      } else if (CW_LOGS_LOG_STREAM_ALREADY_EXISTS == create_log_stream_status) {
        this->does_stream_exist_ = true;
        AWS_LOG_DEBUG(__func__, "Log stream already exists, quit attempting to create it.");
      } else {
        AWS_LOGSTREAM_ERROR(__func__, "Failed to create log stream with , retrying ...");
        continue;
      }
    }

    auto shared_logs_obj = this->shared_logs_.load(std::memory_order_acquire);
    if (nullptr != shared_logs_obj) {
      std::list<Aws::CloudWatchLogs::Model::InputLogEvent> * logs;
      publisher_status = shared_logs_obj->getDataAndLock(logs);
      if (CW_LOGS_SUCCEEDED == publisher_status) {
        if (!logs->empty()) {
          Aws::CloudWatchLogs::ROSCloudWatchLogsErrors send_logs_status = CW_LOGS_FAILED;
          int tries = kMaxRetries;

          while (CW_LOGS_SUCCEEDED != send_logs_status && tries > 0) {
            send_logs_status = this->cloudwatch_facade_->SendLogsToCloudWatch(
              next_token, this->log_group_, this->log_stream_, logs);
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
        }
      }
      // TODO: For now we're just going to discard logs that fail to get sent. Later we may add some
      // retry strategy

      /* Null out the class reference before unlocking the object. The external thread will be
         looking to key off if the shared object is locked or not to know it can start again, but
         the PublishLogs function checks if the the pointer is null or not. */
      this->shared_logs_.store(nullptr, std::memory_order_release);
      shared_logs_obj->freeDataAndUnlock();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}
