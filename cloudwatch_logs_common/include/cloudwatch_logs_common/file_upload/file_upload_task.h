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

#include <future>
#include <cloudwatch_logs_common/file_upload/task_utils.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>

#pragma once

namespace Aws {
namespace FileManagement {

/**
 * Define a task to get batch data and call a callback when finished with this task.
 * @tparam T
 */
template<typename T>
class Task {
 public:
  virtual ~Task() = default;
  virtual T& getBatchData() = 0;
  virtual void onComplete(const UploadStatus &upload_status) = 0;
};

/**
 * The file upload task which calls the upload status callback with the data from the initial task.
 *
 * @tparam T
 */
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

  virtual ~FileUploadTask() = default;

  inline T& getBatchData() override {
    return batch_data_.batch_data;
  }

  inline void onComplete(const UploadStatus &upload_status) override {
    upload_status_function_(upload_status, batch_data_);
  }

 private:
  FileObject<T> batch_data_;
  UploadStatusFunction<UploadStatus, FileObject<T>> upload_status_function_;
};

/**
 * The file upload task which calls the upload status callback with the data from the initial task.
 *
 * @tparam T
 */
template<typename T>
class FileUploadTaskAsync : public Task<T> {
 public:
  explicit FileUploadTaskAsync(
      FileObject<T> batch_data)
  {
    this->batch_data_ = batch_data;
  }

  virtual ~FileUploadTaskAsync() = default;

  inline T& getBatchData() override {
    return batch_data_.batch_data;
  }

  inline void onComplete(const UploadStatus &upload_status) override {
    file_upload_promise_.set_value(
        std::pair<FileObject<T>, UploadStatus>{batch_data_, upload_status});
  }

  inline std::future<std::pair<FileObject<T>, UploadStatus>> getResult() {
    return file_upload_promise_.get_future();
  }

 private:
  FileObject<T> batch_data_;
  std::promise<std::pair<FileObject<T>, UploadStatus>> file_upload_promise_;
};

//------------- Definitions --------------//
template<typename T>
using TaskPtr = std::shared_ptr<Task<T>>;

template<typename T>
using FileUploadTaskPtr = std::shared_ptr<FileUploadTask<T>>;

template<typename T>
using TaskObservedQueue = ObservedQueue<TaskPtr<T>>;
//----------------------------------------//

}  // namespace FileManagement
}  // namespace Aws
