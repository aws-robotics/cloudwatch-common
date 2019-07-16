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

#include <aws/core/utils/logging/LogMacros.h>

#include <dataflow_lite/dataflow/pipeline.h>
#include <dataflow_lite/dataflow/status_monitor.h>
#include <dataflow_lite/dataflow/observed_queue.h>
#include <dataflow_lite/dataflow/queue_monitor.h>
#include <dataflow_lite/utils/service.h>
#include <dataflow_lite/task/task.h>

#include <file_management/file_upload/file_manager.h>
#include <file_management/file_upload/file_upload_task.h>

namespace Aws {
namespace FileManagement {

using Aws::DataFlow::MultiStatusConditionMonitor;
using Aws::DataFlow::OutputStage;

static constexpr std::chrono::milliseconds kTimeout = std::chrono::seconds(1);

struct FileUploadStreamerOptions {

  /**
   * Max number of data processed per read.
   */
  size_t batch_size;

  /**
   * Max number of elements in the queue.
   */
  size_t queue_size;
};

/**
 * File upload manager handles reading data from the file manager and placing it in the observed queue.
 *
 * @tparam T
 */
template<typename T>
class FileUploadStreamer :
  public OutputStage<TaskPtr<T>>, public RunnableService {
public:
  /**
   * Create a file upload manager.
   *
   * @param status_condition_monitor
   * @param file_manager
   * @param observed_queue
   * @param batch_size
   */
  explicit FileUploadStreamer(
    std::shared_ptr<DataReader<T>> data_reader,
    FileUploadStreamerOptions options)
  {
    data_reader_ = data_reader;
    auto data_status_monitor = std::make_shared<StatusMonitor>();
    addStatusMonitor(data_status_monitor);
    network_monitor_ = std::make_shared<StatusMonitor>();
    addStatusMonitor(network_monitor_);

    data_reader_->setStatusMonitor(data_status_monitor);
    batch_size_ = options.batch_size;

  }

  virtual ~FileUploadStreamer() = default;

  /**
   * Add a status monitor for the file upload manager to wait for work on.
   *
   * @param status_monitor to add
   */
  inline void addStatusMonitor(std::shared_ptr<StatusMonitor> &status_monitor) {
    status_condition_monitor_.addStatusMonitor(status_monitor);
  }

  inline bool shutdown() {
    bool b = true;
    b &= RunnableService::shutdown();
    b &= data_reader_->shutdown();
    return b;
  }

  void onPublisherStateChange(const Aws::DataFlow::Status &status) {
    AWS_LOG_INFO(__func__,
                 "Publisher state has changed to: %s",
                 (status == Aws::DataFlow::Status::AVAILABLE) ? "available" : "unavailable");
    network_monitor_->setStatus(status);
  }

  bool initialize() {
    return true;
  }

  void onComplete(const Aws::DataFlow::UploadStatus & upload_status, const FileObject<T> &message) {
    if (upload_status == Aws::DataFlow::UploadStatus::FAIL) {
      OutputStage<TaskPtr<T>>::getSink()->clear();
    }
    data_reader_->fileUploadCompleteStatus(upload_status, message);
  }

  /**
   * Start the upload thread.
   */
  bool start() {
    bool b = true;
    b &= data_reader_->start();
    b &= RunnableService::start();
    return b;
  }

  // todo this is a hack. Should just implement an extension in test
  inline void forceWork() {
    this->work();
  }

protected:

    /**
     * Attempt to start uploading.
     *
     * 1. First wait for work on all the status conditions. (i.e wait until files are available to upload)
     * 2. Read a batch of data from the file_manager
     * 3. Queue up the task to be worked on.
     * 4. Wait for the task to be completed to continue.
     */
    inline void work() override {
      if (!stored_task_) {
        AWS_LOG_DEBUG(__func__,
                     "Waiting for files and work.");
        auto wait_result = status_condition_monitor_.waitForWork(kTimeout);
        if (wait_result == std::cv_status::timeout) {
          AWS_LOG_DEBUG(__func__,
                        "Timed out when waiting for work");
          return;
        }
        if (!OutputStage<TaskPtr<T>>::getSink()) {
          AWS_LOG_WARN(__func__,
                       "No Sink Configured");
          return;
        }
        AWS_LOG_DEBUG(__func__,
                     "Found work! Batching");
        FileObject<T> file_object = data_reader_->readBatch(batch_size_);
        total_logs_uploaded += file_object.batch_size;
        stored_task_ = std::make_shared<FileUploadTask<T>>(
            std::move(file_object),
            std::bind(
                &FileUploadStreamer<T>::onComplete,
                this,
                std::placeholders::_1,
                std::placeholders::_2));
      } else {
        AWS_LOG_DEBUG(__func__,
                     "Previous task found, retrying upload.");
      }
      auto is_accepted = OutputStage<TaskPtr<T>>::getSink()->tryEnqueue(stored_task_, kTimeout);
      if (is_accepted) {
        AWS_LOG_DEBUG(__func__,
                     "Enqueue_accepted");
        stored_task_ = nullptr;
      } else {
        AWS_LOG_DEBUG(__func__,
                     "Enqueue failed");
      }
    }

private:
  /**
   * The status condition monitor to wait on before uploading.
   */
  MultiStatusConditionMonitor status_condition_monitor_;

  /**
   * Current task to upload.
   */
  std::shared_ptr<FileUploadTask<T>> stored_task_;

  /**
   * Metric on number of logs queued in the TaskObservedQueue.
   */
  size_t total_logs_uploaded = 0;

  /**
   * The configured batch size to use when uploading.
   */
  size_t batch_size_;

  /**
   * The file manager to read data from.
   */
  std::shared_ptr<DataReader<T>> data_reader_;

  /**
   * Network status monitor.
   */
  std::shared_ptr<StatusMonitor> network_monitor_;
};

}  // namespace FileManagement
}  // namespace Aws
