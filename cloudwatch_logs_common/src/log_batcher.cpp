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


LogBatcher::LogBatcher(size_t max_allowable_batch_size, size_t publish_trigger_size) : DataBatcher(publish_trigger_size) {

  validateConfigurableSizes(publish_trigger_size, max_allowable_batch_size);

  this->max_allowable_batch_size_.store(max_allowable_batch_size);
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

  // check if we exceeded our allowable limit
  handleSizeExceeded();

  // publish if the size has been configured
  auto mbs = this->getPublishTriggerBatchSize();
  if (mbs != DataBatcher::kDefaultTriggerSize && this->batched_data_->size() >= mbs) {
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

  // is there anything to send?
  if (this-batched_data_->size() == 0) {
    AWS_LOGSTREAM_DEBUG(__func__, "Nothing batched to publish");
    return false;
  }

  //todo getSink is kind of race-y
  if (getSink()) {
    auto data_to_queue = std::make_shared<BasicTask<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>>(this->batched_data_);

    //todo register complete with drop data function
    // todo if file manager reference is set then publish to that when failed
    getSink()->enqueue(data_to_queue); //todo should we try enqueue? if we can't queue (too fast then we need to fail to file

    this->batched_data_ = std::make_shared<LogType>();
    return true;

  } else {
    AWS_LOGSTREAM_WARN(__func__, "Unable to obtain queue");
    return false;
  }
}

bool LogBatcher::handleSizeExceeded() {

  std::lock_guard <std::recursive_mutex> lck(batch_and_publish_lock_);

  auto allowed_max = this->max_allowable_batch_size_.load();

  if (getCurrentBatchSize() > allowed_max) {
    AWS_LOG_WARN(__func__, "Current batch size exceeded");

    if (this->log_file_manager_) {
      AWS_LOG_INFO(__func__, "Writing data to file");
      log_file_manager_->write(*this->batched_data_);
      this->batched_data_ = std::make_shared<LogType>();

    } else {
      AWS_LOG_WARN(__func__, "Dropping data");
    }

    return true;
  }
  return false;
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

size_t LogBatcher::getMaxAllowableBatchSize() {
  return this->max_allowable_batch_size_.load();
}

void LogBatcher::setMaxAllowableBatchSize(size_t new_batch_size) {

  //todo move all this config to DataBatcher, need to check both variables when set
  validateConfigurableSizes(this->getPublishTriggerBatchSize(), new_batch_size);

  this->max_allowable_batch_size_.store(new_batch_size);
}

void LogBatcher::validateConfigurableSizes(size_t publish_trigger_size, size_t max_allowable_batch_size) {
  // if the publish_trigger_sizeis defined and larger than the max_allowable_batch_size then data
  // will never be published, but dropped every time limit has been reached

  if (publish_trigger_size != DataBatcher::kDefaultTriggerSize && publish_trigger_size > max_allowable_batch_size) {
    throw std::invalid_argument("publish_trigger_size cannot be greater than max_allowable_batch_size");
  }
}