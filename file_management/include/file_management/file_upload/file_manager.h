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
#include <list>
#include <memory>
#include <stdexcept>
#include <thread>

#include <aws/core/utils/logging/LogMacros.h>

#include "file_management/file_upload/task_utils.h"
#include "file_management/file_upload/file_manager_strategy.h"
#include <dataflow_lite/dataflow/status_monitor.h>

#include <dataflow_lite/utils/service.h>

namespace Aws {
namespace FileManagement {

using Aws::DataFlow::StatusMonitor;

template <typename T>
class FileObject {
public:
  T batch_data;
  size_t batch_size;
  std::list<DataToken> data_tokens;
}; //todo this should be immutable

//todo move to a different file, e.g. statuses?
enum UploadStatus {
  FAIL,
  SUCCESS
};

template <typename T>
class DataReader : public Service {
public:
  /**
  * Read a specific amount of data from a file.
  * @param batch_size to read
  * @return a FileObject containing the data read plus some metadata about the file read
  */
  virtual FileObject<T> readBatch(size_t batch_size) = 0;

  /**
   * Handle an upload complete status.
   *
   * @param upload_status the status of an attempted upload of data
   * @param log_messages the data which was attempted to be uploaded
   */
  virtual void fileUploadCompleteStatus(
    const UploadStatus &upload_status,
    const FileObject<T> &log_messages) = 0;

  virtual void setStatusMonitor(std::shared_ptr<StatusMonitor> status_monitor) = 0;
};

/**
 * File manager specific to the type of data to write to files.
 * @tparam T type of data to handle
 */
template <typename T>
class FileManager :
  public DataReader <T>
//  public Service
{
public:

  /**
   * Default constructor.
   */
  FileManager() {
    // todo use customer data to VERIFY that the above kOptions is valid
    file_manager_strategy_ = std::make_shared<FileManagerStrategy>(Aws::FileManagement::kDefaultFileManagerStrategyOptions);
  }

  /**
   * Constructor that takes in FileManagerStrategyOptions.
   *
   * @param options for the FileManagerStrategy
   */
  FileManager(const FileManagerStrategyOptions &options) {
    // todo all arguments should be configurable and should have input checking
    file_manager_strategy_ = std::make_shared<FileManagerStrategy>(options);
  }

  /**
   * Initialize the file manager with a custom file manager strategy.
   *
   * @throws invalid argument for null DataManagerStrategy
   * @param file_manager_strategy custom strategy.
   */
  explicit FileManager(std::shared_ptr<DataManagerStrategy> file_manager_strategy) {

    // todo how to allow null for testing?
//    if (nullptr == file_manager_strategy) {
//      throw std::invalid_argument("DataManagerStrategy cannot be null");
//    }
    if(file_manager_strategy) {
      file_manager_strategy_ = file_manager_strategy;
    }
  }

  virtual bool start() override {
    if(file_manager_strategy_) {
      file_manager_strategy_->start();
      if (file_manager_strategy_->isDataAvailable()) {
        FileManager::file_status_monitor_->setStatus(Aws::DataFlow::Status::AVAILABLE);
      }
    }
    return true;
  }

  virtual bool shutdown() override {
    if(file_manager_strategy_) {
      FileManager::file_status_monitor_->setStatus(Aws::DataFlow::Status::UNAVAILABLE);
      file_manager_strategy_->shutdown();
    }
    return true;
  }

  virtual ~FileManager() = default;

  /**
   * Read data from file and get the file info related to the data.
   *
   * @param data [out] to fill with info
   * @return FileInfo meta info
   */
  inline DataToken read(std::string &data) {
    DataToken token = file_manager_strategy_->read(data);
    if (!file_manager_strategy_->isDataAvailable()) {
      AWS_LOG_INFO(__func__,
                   "Data is no longer available to read.");
      file_status_monitor_->setStatus(Aws::DataFlow::Status::UNAVAILABLE);
    }
    return token;
  }

  /**
   * Write data to the appropriate file.
   * @param data to write.
   */
  virtual void write(const T & data) = 0; // todo shouldn't this be a DataWriter interface?

/**
 * Handle an upload complete status.
 *
 * @param upload_status the status of an attempted upload of data
 * @param log_messages the data which was attempted to be uploaded
 */
  virtual void fileUploadCompleteStatus(
      const UploadStatus& upload_status,
      const FileObject<T> &log_messages) override {
    if (UploadStatus::SUCCESS == upload_status) {
      total_logs_uploaded_ += log_messages.batch_size;
      AWS_LOG_INFO(__func__,
                   "Total logs uploaded: %i",
                   total_logs_uploaded_);
    }

    // Delete file if empty log_messages.file_location.
    for (const auto &token : log_messages.data_tokens) {
      // this may block, file IO can be expensive
      file_manager_strategy_->resolve(token, upload_status == SUCCESS);
    }
  }

  /**
   * Add a file status monitor to notify observers when there
   * @param status_monitor
   */
  void setStatusMonitor(std::shared_ptr<StatusMonitor> status_monitor) override {
    file_status_monitor_ = status_monitor;
  }

protected:
  /**
   * The object that keeps track of which files to delete, read, or write to.
   * Should probably be thread safe or something :)
   */
  size_t total_logs_uploaded_ = 0;
  
  /**
   * The file manager strategy to use for handling files.
   */
  std::shared_ptr<DataManagerStrategy> file_manager_strategy_;

  /**
   * The status monitor for notifying an observer when files are available.
   */
  std::shared_ptr<StatusMonitor> file_status_monitor_;

};


}  // namespace FileManagement
}  // namespace Aws
