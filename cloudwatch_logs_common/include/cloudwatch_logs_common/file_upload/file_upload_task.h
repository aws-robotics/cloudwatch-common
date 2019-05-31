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
#include <memory>
#pragma once

namespace Aws {
namespace FileManagement {

template<typename T>
class IPublisher {
public:
    virtual UploadStatus attemptPublish(T &batch_data) = 0; //todo shared pointer input? or unique pointer?
};

/**
 * Define a task (runnable) to get batch data and call a callback when finished with this task.
 * @tparam T
 */
template<typename T>
class Task {
 public:
  inline Task(std::shared_ptr<IPublisher<T>> publisher) {
    this->iPublisher_ = publisher;
  }
  virtual ~Task() = default;
  virtual void run() = 0; //todo would be nice to define here, same definition is reused
  virtual void cancel() = 0;
protected:
  std::shared_ptr<IPublisher<T>> iPublisher_;
};


template<typename T>
class BasicTask :
  public Task<T> {
public:
  explicit BasicTask(
    std::shared_ptr<T> batch_data,
    UploadStatusFunction<UploadStatus, T> upload_status_function,
    std::shared_ptr<IPublisher<T>> iPublisher) : Task<T>(iPublisher)
  {
    this->batch_data_ = batch_data;
    this->upload_status_function_ = upload_status_function;
  }

  virtual ~BasicTask() = default;

  inline void run() {
    auto status = this->iPublisher_->attemptPublish(*batch_data_);
    upload_status_function_(status, *batch_data_); //todo input should be a shared pointer
  }

  inline void cancel() {
    upload_status_function_(UploadStatus::FAIL, *batch_data_);
  }

private:
  std::shared_ptr<T> batch_data_; //todo consider const
  UploadStatusFunction<UploadStatus, T> upload_status_function_; //todo consider const
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
      std::shared_ptr<FileObject<T>> batch_data,
      UploadStatusFunction<UploadStatus, FileObject<T>> upload_status_function,
      std::shared_ptr<IPublisher<T>> iPublisher) : Task<T>(iPublisher)
  {
    this->batch_data_ = batch_data;
    this->upload_status_function_ = upload_status_function;
  }

  virtual ~FileUploadTask() = default;

    inline void run() {
      auto status = this->iPublisher_->attemptPublish(*batch_data_);
      upload_status_function_(status, batch_data_);
    }

    inline void cancel() {
      upload_status_function_(UploadStatus::FAIL, *batch_data_);
    }

private:
  std::shared_ptr<FileObject<T>> batch_data_;
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
      FileObject<T> &&batch_data,
      std::shared_ptr<IPublisher<T>> publisher
      ) : Task<T>(publisher)
  {
    this->batch_data_ = batch_data;
  }

  virtual ~FileUploadTaskAsync() = default;

  inline void run(){
    auto status = this->iPublisher_->attemptPublish(batch_data_.batch_data);
    file_upload_promise_.set_value(
        std::pair<FileObject<T>, UploadStatus>{batch_data_, status});
  }

  inline std::future<std::pair<FileObject<T>, UploadStatus>> getResult() {
    return file_upload_promise_.get_future();
  }

  inline void cancel() {

  };

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
using TaskObservedQueue = Aws::DataFlow::ObservedQueue<TaskPtr<T>>;

template<typename T>
using TaskSink = Aws::DataFlow::Sink<TaskPtr<T>>;
//----------------------------------------//

}  // namespace FileManagement
}  // namespace Aws
