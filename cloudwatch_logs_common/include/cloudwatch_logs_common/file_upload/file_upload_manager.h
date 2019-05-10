/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <thread>
#include <memory>
#include <cloudwatch_logs_common/file_upload/status_monitor.h>
#include <cloudwatch_logs_common/file_upload/observed_queue.h>
#include <cloudwatch_logs_common/file_upload/queue_monitor.h>
#include <cloudwatch_logs_common/utils/task_utils.h>
#include <cloudwatch_logs_common/utils/file_manager.h>

namespace Aws {
namespace FileManagement {


using Aws::CloudWatchLogs::Utils::UploadStatusFunction;
using Aws::CloudWatchLogs::Utils::FileManager;
using Aws::CloudWatchLogs::Utils::FileObject;
using Aws::CloudWatchLogs::Utils::UploadStatus;

template<typename T>
class Task {
public:
  virtual ~Task() = default;
  virtual T& getBatchData() = 0;
  virtual void onComplete(const UploadStatus &upload_status) = 0;
};

template<typename T>
class FileUploadTask : public Task<T> {
public:
  explicit FileUploadTask(
    FileObject<T> batch_data,
    UploadStatusFunction<UploadStatus, FileObject<T>> upload_status_function)
  {
    this->batch_data_ = batch_data;
    this->upload_status_function_ = upload_status_function;
  }

  virtual ~FileUploadTask() override = default;

  inline T& getBatchData() override {
    return batch_data_.batch_data;
  }

  inline void onComplete(const UploadStatus &upload_status) override {
    upload_status_function_(upload_status, batch_data_);
  }

  FileObject<T> batch_data_;
  UploadStatusFunction<UploadStatus, FileObject<T>> upload_status_function_;
};

template<typename T>
class FileUploadManager {
public:
  explicit FileUploadManager(
    std::shared_ptr<MultiStatusConditionMonitor> status_condition_monitor,
    std::shared_ptr<FileManager<T>> file_manager,
    std::shared_ptr<ObservedQueue<std::shared_ptr<Task<T>>>> observed_queue,
    size_t batch_size)
  {
    status_condition_monitor_ = status_condition_monitor;
    file_manager_ = file_manager;
    observed_queue_ = observed_queue;
    batch_size_ = batch_size;
  }

  virtual ~FileUploadManager() {
    if (thread) {
      thread->join();
    }
  }

  void addStatusMonitor(std::shared_ptr<StatusMonitor> &status_monitor) {
    status_condition_monitor_->addStatusMonitor(status_monitor);
  }

  std::shared_ptr<ObservedQueue<std::shared_ptr<Task<T>>>> getObservedQueue() {
    return observed_queue_;
  }

  inline void startRun() {
    while (true) {
      run();
    }
  }
  inline void run() {
    AWS_LOG_INFO(__func__,
                 "Waiting for files and work.");
    status_condition_monitor_->waitForWork();
    AWS_LOG_INFO(__func__,
                 "Found work! Batching");
    FileObject<T> file_object = file_manager_->readBatch(batch_size_);
    total_logs_uploaded += file_object.batch_size;
    auto upload_func =
        std::bind(
            &FileManager<T>::fileUploadCompleteStatus,
            file_manager_,
            std::placeholders::_1,
            std::placeholders::_2);
    auto file_upload_task =
        std::make_shared<FileUploadTask<T>>(file_object, upload_func);
    observed_queue_->enqueue(file_upload_task);
    AWS_LOG_INFO(__func__,
                 "Total logs from file queued %i", total_logs_uploaded);
  }

  void start() {
    thread = std::make_shared<std::thread>(std::bind(&FileUploadManager::startRun,this));
  }

  void join() {
    if (thread) {
        thread->join();
      }
  }

private:
  size_t total_logs_uploaded = 0;
  std::shared_ptr<std::thread> thread;
  size_t batch_size_;
  std::shared_ptr<MultiStatusConditionMonitor> status_condition_monitor_;
  std::shared_ptr<FileManager<T>> file_manager_;
  std::shared_ptr<ObservedQueue<std::shared_ptr<Task<T>>>> observed_queue_;
};

}  // namespace FileManagement
}  // namespace Aws
