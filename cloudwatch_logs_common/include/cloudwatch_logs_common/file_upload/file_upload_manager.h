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

#include <cloudwatch_logs_common/file_upload/network_monitor.h>
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
  virtual T getBatchData() = 0;
  virtual void onComplete() = 0;
};

template<typename T>
class FileUploadTask : public Task<T> {
public:
  FileUploadTask(
    FileObject<T> && batch_data,
    UploadStatusFunction<UploadStatus, FileObject<T>> upload_status_function)
  {
    this->batch_data_ = batch_data;
    this->upload_status_function_ = upload_status_function;
  }

  inline T& getBatchData() override {
    return batch_data_.batch_data;
  }

  inline void onComplete(const UploadStatus &upload_status) override {
    upload_status_function_(upload_status, batch_data_);
  }

  FileObject<T> batch_data_;
  UploadStatusFunction<UploadStatus, FileObject<T>> upload_status_function_;
};

template<typename Status, typename T>
class FileUploadManager {
public:
  FileUploadManager(
    std::shared_ptr<MultiStatusConditionMonitor> status_condition_monitor,
    std::shared_ptr<FileManager<T>> file_manager,
    std::shared_ptr<ObservedQueue<FileUploadTask<T>>> observed_queue,
    size_t batch_size)
  {
    status_condition_monitor_ = status_condition_monitor;
    file_manager_ = file_manager;
    observed_queue_ = observed_queue;
    batch_size_ = batch_size;
  }

  virtual ~FileUploadManager() = default;
  inline void run() {
    status_condition_monitor_->waitForWork();
    T batch = file_manager_.readBatch(batch_size_);
    using namespace std::placeholders;
    UploadStatusFunction<Status, T> upload_func =
        std::bind(&FileManager<T>::fileUploadCompleteStatus, file_manager_, _1, _2);
    FileUploadTask<T> file_upload_task(batch, upload_func);
    observed_queue_->push_back(file_upload_task);
  }

private:
  size_t batch_size_;
  std::shared_ptr<MultiStatusConditionMonitor> status_condition_monitor_;
  std::shared_ptr<FileManager<T>> file_manager_;
  std::shared_ptr<ObservedQueue<FileUploadTask<T>>> observed_queue_;
};

}  // namespace FileManagement
}  // namespace Aws
