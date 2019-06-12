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
#include <aws/logs/model/InputLogEvent.h>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <cloudwatch_logs_common/log_batcher.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>

#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <stdexcept>

using namespace Aws::CloudWatchLogs;

LogBatcher::LogBatcher() : DataBatcher() {
  this->batched_data_ = std::make_shared<LogType>();
}

LogBatcher::LogBatcher(size_t size) : DataBatcher(size) {
  this->batched_data_ = std::make_shared<LogType>();
}

LogBatcher::~LogBatcher() = default;

using Aws::FileManagement::BasicTask;
using Aws::FileManagement::UploadStatus;
using Aws::FileManagement::UploadStatusFunction;

bool LogBatcher::batchData(const std::string &log_msg_formatted) {

  std::lock_guard<std::recursive_mutex> lck(batch_and_publish_lock_);
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch());

  return batchData(log_msg_formatted, ms);
}

bool LogBatcher::batchData(const std::string &log_msg_formatted, const std::chrono::milliseconds &milliseconds) {

  std::lock_guard<std::recursive_mutex> lck(batch_and_publish_lock_);
  auto log_event = convertToLogEvent(log_msg_formatted, milliseconds);
  this->batched_data_->push_back(log_event);

  // publish if the size has been configured
  auto mbs = this->getMaxBatchSize();
  if (mbs != DataBatcher::DEFAULT_SIZE && this->batched_data_->size() >= mbs) {
    this->publishBatchedData();
  }
  return true;
}

Aws::CloudWatchLogs::Model::InputLogEvent LogBatcher::convertToLogEvent(const std::string &message, const std::chrono::milliseconds &milliseconds) {
  Aws::CloudWatchLogs::Model::InputLogEvent log_event;
  log_event.SetMessage(message.c_str());
  log_event.SetTimestamp(milliseconds.count());
  return log_event;
}

size_t LogBatcher::getCurrentBatchSize()
{
  std::lock_guard <std::recursive_mutex> lck(batch_and_publish_lock_);
  if (this->batched_data_) {
    return this->batched_data_->size();
  }
  return 0;
}

bool LogBatcher::publishBatchedData() {

  std::lock_guard <std::recursive_mutex> lck(batch_and_publish_lock_);

  //todo getSink is kind of race-y
  if (getSink()) {

    auto p = std::make_shared<BasicTask<LogType>>(this->batched_data_);

    if (log_file_manager_ ) {

      // register the task failure function
      auto function = [&log_file_manager = this->log_file_manager_](const FileManagement::UploadStatus &upload_status,
              const LogType &log_messages)
      {
          if (!log_messages.empty()) {
            if (FileManagement::SUCCESS != upload_status) {
              AWS_LOG_INFO(__func__, "Task failed: writing logs to file");
              log_file_manager->write(log_messages);
            }
          }
      };

      p->setOnCompleteFunction(function);
    }

    getSink()->enqueue(p); //todo should we try enqueue? if we can't queue (too fast then we need to fail to file

    this->batched_data_ = std::make_shared<LogType>();
    return true;

  } else {
    //todo log unable to queue
    return false;
  }
}

//todo implement drop data function (queued too much too fast, etc)

bool LogBatcher::initialize() {
  return true;
}
bool LogBatcher::start() {
  return true;
}
bool LogBatcher::shutdown() {
  this->batched_data_->clear();
  return true;
}

void LogBatcher::setLogFileManager(std::shared_ptr<Aws::CloudWatchLogs::Utils::FileManager<LogType>> log_file_manager)
{
  if (nullptr == log_file_manager) {
    throw std::invalid_argument("input LogFileManager cannot be null");
  }
  this->log_file_manager_ = log_file_manager;
}

