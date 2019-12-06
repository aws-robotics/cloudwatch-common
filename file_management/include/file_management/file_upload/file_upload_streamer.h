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

static constexpr std::chrono::milliseconds kTimeout = std::chrono::minutes(5);

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
  public OutputStage<TaskPtr<T>>, public RunnableService{
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
    AddStatusMonitor(data_status_monitor);
    network_monitor_ = std::make_shared<StatusMonitor>();
    AddStatusMonitor(network_monitor_);

    data_reader_->SetStatusMonitor(data_status_monitor);
    batch_size_ = options.batch_size;
    status_monitor_timeout_ = kTimeout;
  }

  ~FileUploadStreamer() override = default;

  /**
   * Add a status monitor for the file upload manager to wait for work on.
   *
   * @param status_monitor to add
   */
  inline void AddStatusMonitor(std::shared_ptr<StatusMonitor> & status_monitor)
  {
    status_condition_monitor_.AddStatusMonitor(status_monitor);
  }

  inline bool Shutdown() override
  {
    bool is_shutdown = true;
    is_shutdown &= RunnableService::Shutdown();
    is_shutdown &= data_reader_->Shutdown();
    return is_shutdown;
  }

  void OnPublisherStateChange(const Aws::DataFlow::Status & status)
  {
    AWS_LOG_INFO(__func__,
                 "Publisher state has changed to: %s",
                 (status == Aws::DataFlow::Status::AVAILABLE) ? "available" : "unavailable");
    network_monitor_->SetStatus(status);
  }

  bool Initialize()
  {
    return true;
  }

  void OnComplete(const Aws::DataFlow::UploadStatus & upload_status, const FileObject<T> & message)
  {
    if (upload_status == Aws::DataFlow::UploadStatus::FAIL) {
      OutputStage<TaskPtr<T>>::GetSink()->Clear();
    }
    data_reader_->FileUploadCompleteStatus(upload_status, message);
  }

  /**
   * Start the upload thread.
   */
  bool Start() override
  {
    bool is_started = true;
    is_started &= data_reader_->Start();
    is_started &= RunnableService::Start();
    return is_started;
  }

  // todo this is a hack. Should just implement an extension in test
  inline void ForceWork()
  {
    this->Work();
  }

  void SetStatusMonitorTimeout(std::chrono::milliseconds new_timeout)
  {
    status_monitor_timeout_ = new_timeout;
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
  inline void Work() override
  {
    if (!stored_task_) {
      AWS_LOG_DEBUG(__func__, "Waiting for files and work.");
      auto wait_result = status_condition_monitor_.WaitForWork(status_monitor_timeout_);

      // is there data available?

      if (wait_result == std::cv_status::timeout) {

        if (!data_reader_->IsDataAvailableToRead()) {
          AWS_LOG_DEBUG(__func__, "Timed out when waiting for work, no data available to read");
          return;
        }
        AWS_LOG_DEBUG(__func__, "Timed out when waiting for work, but data available to read: attempting to publish");
        // otherwise attempt to publish as only the network is down but we have data to send
      }

      if (!OutputStage<TaskPtr<T>>::GetSink()) {
        AWS_LOG_WARN(__func__, "No Sink Configured");
        return;
      }
      AWS_LOG_DEBUG(__func__, "Found work, batching");
      FileObject<T> file_object = data_reader_->ReadBatch(batch_size_);
      total_logs_uploaded_ += file_object.batch_size;  // todo this is attempted, not truly uploaded
      stored_task_ = std::make_shared<FileUploadTask<T>>(
                       std::move(file_object),
                       std::bind(
                         &FileUploadStreamer<T>::OnComplete,
                         this,
                         std::placeholders::_1,
                         std::placeholders::_2));
    } else {
      AWS_LOG_DEBUG(__func__, "Previous task found, retrying upload.");
    }
    auto is_accepted = OutputStage<TaskPtr<T>>::GetSink()->TryEnqueue(stored_task_, kTimeout);
    if (is_accepted) {
      AWS_LOG_DEBUG(__func__, "Enqueue_accepted");
      stored_task_ = nullptr;
    } else {
      AWS_LOG_DEBUG(__func__, "Enqueue failed");
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
  size_t total_logs_uploaded_ = 0;

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

  /**
   * Timeout to wait for work.
   */
  std::chrono::milliseconds status_monitor_timeout_;
};

}  // namespace FileManagement
}  // namespace Aws
