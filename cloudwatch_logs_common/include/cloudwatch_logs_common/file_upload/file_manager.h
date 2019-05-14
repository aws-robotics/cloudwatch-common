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

#include <aws/core/utils/logging/LogMacros.h>

#include "cloudwatch_logs_common/file_upload/task_utils.h"
#include "cloudwatch_logs_common/file_upload/file_manager_strategy.h"
#include "cloudwatch_logs_common/dataflow/status_monitor.h"

namespace Aws {
namespace FileManagement {

template <typename T>
class FileObject {
public:
  T batch_data;
  size_t batch_size;
  FileInfo file_info;
};

enum UploadStatus {
  FAIL,
  SUCCESS
};

/**
 * File manager specific to the type of data to write to files.
 * @tparam T type of data to handle
 */
template <typename T>
class FileManager {
public:
  /**
   * Default constructor.
   */
  FileManager() {
    file_manager_strategy_ = std::make_shared<FileManagerStrategy>();
  }

  /**
   * Initialize the file manager with a custom file manager strategy.
   *
   * @param file_manager_strategy custom strategy.
   */
  FileManager(std::shared_ptr<FileManagerStrategy> &file_manager_strategy) {
    file_manager_strategy_ = file_manager_strategy;
  }

  virtual ~FileManager() = default;

  /**
   * Read data from file and get the file info related to the data.
   *
   * @param data [out] to fill with info
   * @return FileInfo meta info
   */
  FileInfo read(std::string &data) {
    auto file_info = file_manager_strategy_->read(data);
    if (file_info.file_status == END_OF_READ) {
      file_status_monitor_->setStatus(Aws::FileManagement::Status::UNAVAILABLE);
    }
    return file_info;
  }

  /**
   * Write data to the appropriate file.
   * @param data to write.
   */
  virtual void write(const T & data) = 0;

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
  inline void fileUploadCompleteStatus(
      const UploadStatus& upload_status,
      const FileObject<T> &log_messages) {
    if (UploadStatus::SUCCESS == upload_status) {
      total_logs_uploaded_ += log_messages.batch_size;
      AWS_LOG_INFO(__func__,
                   "Total logs uploaded: %i",
                   total_logs_uploaded_);
      // Delete file if empty log_messages.file_location.
      if (END_OF_READ == log_messages.file_info.file_status) {
        AWS_LOG_INFO(__func__,
                     "Found end of file, deleting file: %s",
                     log_messages.file_info.file_name);
        file_manager_strategy_->deleteFile(log_messages.file_info.file_name);
      }
    } else {
      // Set last read location for this file.
    }
  }

  /**
   * Add a file status monitor to notify observers when there
   * @param status_monitor
   */
  inline void addFileStatusMonitor(std::shared_ptr<Aws::FileManagement::StatusMonitor> status_monitor) {
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
  std::shared_ptr<FileManagerStrategy> file_manager_strategy_;

  /**
   * The status monitor for notifying an observer when files are available.
   */
  std::shared_ptr<Aws::FileManagement::StatusMonitor> file_status_monitor_;

};


}  // namespace FileManagement
}  // namespace Aws
