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
#include <cloudwatch_logs_common/log_batcher.h>
#include <file_management/file_upload/file_upload_task.h>
#include <dataflow_lite/utils/data_batcher.h>

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


LogBatcher::LogBatcher(size_t max_allowable_batch_size,
                       size_t publish_trigger_size)
                       : DataBatcher(max_allowable_batch_size, publish_trigger_size) {
}

LogBatcher::~LogBatcher() = default;

bool LogBatcher::publishBatchedData() {
  std::lock_guard<std::recursive_mutex> lk(mtx);

  // is there anything to send?
  if (getCurrentBatchSize() == 0) {
    AWS_LOGSTREAM_DEBUG(__func__, "Nothing batched to publish");
    return false;
  }

  //todo getSink is kind of race-y
  if (getSink()) {

    std::shared_ptr<LogType> log_type = this->batched_data_;
    std::shared_ptr<BasicTask<LogType>> log_task = std::make_shared<BasicTask<LogType>>(log_type);

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

      log_task->setOnCompleteFunction(function);
    }

    getSink()->enqueue(log_task); //todo should we try enqueue? if we can't queue (too fast then we need to fail to file

    this->resetBatchedData();
    return true;

  } else {
    AWS_LOGSTREAM_WARN(__func__, "Unable to obtain queue");
    return false;
  }
}

void LogBatcher::handleSizeExceeded() {

  if (this->log_file_manager_) {
    AWS_LOG_INFO(__func__, "Writing data to file");
    log_file_manager_->write(*this->batched_data_);
  } else {
    AWS_LOG_WARN(__func__, "Dropping data");
  }
  this->resetBatchedData();
}


bool LogBatcher::start() {
  return true;
}
bool LogBatcher::shutdown() {
  this->resetBatchedData();
  return true;
}

void LogBatcher::setLogFileManager(std::shared_ptr<Aws::CloudWatchLogs::Utils::FileManager<LogType>> log_file_manager)
{
  if (nullptr == log_file_manager) {
    throw std::invalid_argument("input LogFileManager cannot be null");
  }
  this->log_file_manager_ = log_file_manager;
}