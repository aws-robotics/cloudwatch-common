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
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>
#include <cloudwatch_logs_common/file_upload/task_factory.h>

#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace Aws::CloudWatchLogs;

LogBatcher::LogBatcher(std::shared_ptr<TaskFactory<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>> task_factory)
: DataBatcher()
{
  //todo check arguments
  this->task_factory_ = task_factory;
  this->batched_data_ = std::make_shared<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>();
}

LogBatcher::LogBatcher(std::shared_ptr<TaskFactory<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>> task_factory, int size)
: DataBatcher(size)
{
  //todo check arguments
  this->task_factory_ = task_factory;
  this->batched_data_ = std::make_shared<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>(); //todo set to size so we don't have to keep allocating
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
  if (max_batch_size_ != DataBatcher::DEFAULT_SIZE && batched_data_->size() >= max_batch_size_) {
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

bool LogBatcher::publishBatchedData() {

  std::lock_guard <std::recursive_mutex> lck(batch_and_publish_lock_);

  if (getSink()) {

    auto bt = task_factory_->createBasicTask(batched_data_);
    auto p = std::make_shared < BasicTask < std::list < Aws::CloudWatchLogs::Model::InputLogEvent >> > (bt);
    getSink()->enqueue(p);

    this->batched_data_ = std::make_shared < std::list < Aws::CloudWatchLogs::Model::InputLogEvent >> ();
    return true;

  } else {
    //todo log unable to queue
    return false;
  }
}

int LogBatcher::getCurrentBatchSize() {
  return this->batched_data_->size();
}

bool LogBatcher::initialize() {
  return true;
}
bool LogBatcher::start() {
  return true;
}
bool LogBatcher::shutdown() {
  batched_data_->clear();
}