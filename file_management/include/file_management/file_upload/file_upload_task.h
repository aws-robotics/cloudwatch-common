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

#include <file_management/file_upload/task_utils.h>
#include <file_management/file_upload/file_manager.h>

#include <dataflow_lite/dataflow/observed_queue.h>
#include <dataflow_lite/dataflow/sink.h>

#include <future>
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
template <typename T>
class Task {
 public:
  virtual ~Task() = default;

  virtual void run(std::shared_ptr<IPublisher<T>> publisher) {
    auto status = publisher->attemptPublish(getBatchData());
    this->onComplete(status);
  }

  virtual void cancel() {
    this->onComplete(FAIL);
  }

  virtual void onComplete(const UploadStatus &status) = 0;

  virtual T& getBatchData() = 0;
};

template<typename T>
class BasicTask :
  public Task<T> {
public:
  explicit BasicTask(
    std::shared_ptr<T> batch_data) : Task<T>()
  {
    this->batch_data_ = batch_data;
    // null is allowable as there is a guard above (default action do nothing)
    this->upload_status_function_ = nullptr;
  }

  virtual ~BasicTask() = default;

  virtual void onComplete(const UploadStatus &status) override {
    if (upload_status_function_) {
      upload_status_function_(status, *batch_data_);
    }
  }

  void setOnCompleteFunction(
    const UploadStatusFunction<UploadStatus, T> upload_status_function)
  {
      // null is allowable as there is a guard above (default action do nothing)
      upload_status_function_ = upload_status_function;
  }

  T& getBatchData() override {
    return *batch_data_;
  }

private:
  std::shared_ptr<T> batch_data_;
  UploadStatusFunction<UploadStatus, T> upload_status_function_;
};

/**
 * The file upload task which calls the upload status callback with the data from the initial task.
 *
 * @tparam T
 */
template<typename T>
class FileUploadTask : public Task<T> {
 public:
  using FileUploadStatusFunc = UploadStatusFunction<UploadStatus, FileObject<T>>;

  explicit FileUploadTask(
      FileObject<T> &&batch_data,
      FileUploadStatusFunc upload_status_function
      ) : Task<T>()
  {
    this->batch_data_ = batch_data;
    this->upload_status_function_ = upload_status_function;
  }

  virtual ~FileUploadTask() = default;

  void onComplete(const UploadStatus &status) override {
    if (upload_status_function_) {
      upload_status_function_(status, batch_data_);
    }
  }

  T& getBatchData() override {
    return batch_data_.batch_data;
  }

private:
  FileObject<T> batch_data_;
  FileUploadStatusFunc upload_status_function_ = nullptr;
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
      FileObject<T> &&batch_data) : Task<T>()
  {
    this->batch_data_ = batch_data;
  }

  virtual ~FileUploadTaskAsync() = default;

  void onComplete(const UploadStatus &status) override {
    file_upload_promise_.set_value(
        std::pair<FileObject<T>, UploadStatus>{batch_data_, status});
  }

  inline std::future<std::pair<FileObject<T>, UploadStatus>> getResult() {
    return file_upload_promise_.get_future();
  }

  T& getBatchData() override {
    return batch_data_.batch_data;
  }

private:
  FileObject<T> batch_data_;
  std::promise<std::pair<FileObject<T>, UploadStatus>> file_upload_promise_ = nullptr;
};

//------------- Definitions --------------//
template<typename T>
using TaskPtr = std::shared_ptr<Task<T>>;

template<typename T>
using FileUploadTaskPtr = std::shared_ptr<FileUploadTask<T>>;

template<typename T>
using TaskObservedQueue = Aws::DataFlow::ObservedQueue<TaskPtr<T>>;

template<typename T>
using TaskObservedBlockingQueue = Aws::DataFlow::ObservedBlockingQueue<TaskPtr<T>>;

template<typename T>
using TaskSink = Aws::DataFlow::Sink<TaskPtr<T>>;
//----------------------------------------//

}  // namespace FileManagement
}  // namespace Aws
