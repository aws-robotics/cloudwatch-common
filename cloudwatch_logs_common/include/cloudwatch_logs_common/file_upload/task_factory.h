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

#pragma once

#include <vector>
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <functional>

namespace Aws {
namespace FileManagement {

using namespace Aws::CloudWatchLogs;

template<typename T>
class ITaskFactory {
public:
  virtual std::shared_ptr<FileUploadTaskAsync<T>>
  createFileUploadTaskAsync(
    FileObject<T>&& batch_data) = 0;
};

template<typename T>
class TaskFactory :
  public ITaskFactory<T> {
public:

    //for testing
    inline TaskFactory(std::shared_ptr<IPublisher<T>> publisher) {
      publisher_= publisher;
      file_manager_ = nullptr;
    }

    inline TaskFactory(std::shared_ptr<IPublisher<T>> publisher, std::shared_ptr<FileManager<T>> file_manager) {
      publisher_= publisher;
      file_manager_ = file_manager;
    }
    inline BasicTask<T> createBasicTask(std::shared_ptr<T> batch_data) {

      auto upload_function = std::bind(&TaskFactory<T>::emptyFunction, this);
      if (file_manager_) {
        auto upload_function = std::bind(&FileManager<T>::uploadCompleteStatus, this->file_manager_,
                                         std::placeholders::_1, std::placeholders::_2);
      }
      return BasicTask<T>(batch_data, upload_function, this->publisher_);
    }
    inline std::shared_ptr<FileUploadTaskAsync<T>> createFileUploadTaskAsync(
        FileObject<T>&& batch_data) override {
      return std::make_shared<FileUploadTaskAsync<T>>(std::move(batch_data), this->publisher_);
    }

    //do nothing
    void emptyFunction() {};

private:
    std::shared_ptr<IPublisher<T>> publisher_;
    std::shared_ptr<FileManager<T>> file_manager_; //allow this to be null?
};
}
}

