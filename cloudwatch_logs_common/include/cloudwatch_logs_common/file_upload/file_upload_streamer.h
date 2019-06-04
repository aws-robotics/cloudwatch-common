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

#include <cloudwatch_logs_common/dataflow/pipeline.h>
#include <cloudwatch_logs_common/dataflow/status_monitor.h>
#include <cloudwatch_logs_common/dataflow/observed_queue.h>
#include <cloudwatch_logs_common/dataflow/queue_monitor.h>
#include <cloudwatch_logs_common/file_upload/task_utils.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>
#include <cloudwatch_logs_common/file_upload/task_factory.h>
#include <cloudwatch_logs_common/utils/publisher.h>
#include <cloudwatch_logs_common/utils/service.h>

namespace Aws {
namespace FileManagement {

using Aws::DataFlow::MultiStatusConditionMonitor;
using Aws::DataFlow::OutputStage;

static constexpr std::chrono::milliseconds kTimeout = std::chrono::milliseconds(100);

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
  public OutputStage<TaskPtr<T>>, public Service {
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
    std::shared_ptr<MultiStatusConditionMonitor> status_condition_monitor,
    std::shared_ptr<DataReader<T>> file_manager,
    std::shared_ptr<ITaskFactory<T>> task_factory,
    FileUploadStreamerOptions options)
  {
    status_condition_monitor_ = status_condition_monitor;
    data_reader_ = file_manager;
    batch_size_ = options.batch_size;
    task_factory_ = task_factory;
    network_monitor_ = std::make_shared<Aws::FileManagement::StatusMonitor>();
    status_condition_monitor_->addStatusMonitor(network_monitor_);
  }

  virtual ~FileUploadStreamer() {
    if (thread) {
      AWS_LOG_INFO(__func__,
                   "Shutting down FileUploader thread.");
      thread->join();
      AWS_LOG_INFO(__func__,
                   "FileUploader successfully shutdown");
    }
  }

  /**
   * Add a status monitor for the file upload manager to wait for work on.
   *
   * @param status_monitor to add
   */
  inline void addStatusMonitor(std::shared_ptr<StatusMonitor> &status_monitor) {
    status_condition_monitor_->addStatusMonitor(status_monitor);
  }

  inline bool shutdown() {
    // set that the thread should no longer run
    return true;
  }

  // todo this should be protected or private
  inline bool startRun() {
    //todo while should run
    while (true) {
      run();
    }
  }

  void onPublisherStateChange(const Aws::CloudWatchLogs::PublisherState &newState) {
    //set the status_condition_monitor_
    auto network_status = newState == PublisherState::CONNECTED ?
                          Aws::DataFlow::Status::UNAVAILABLE : Aws::DataFlow::Status::AVAILABLE;
    network_monitor_->setStatus(network_status);
  }

  /**
   * Attempt to start uploading.
   *
   * 1. First wait for work on all the status conditions. (i.e wait until files are available to upload)
   * 2. Read a batch of data from the file_manager
   * 3. Queue up the task to be worked on.
   * 4. Wait for the task to be completed to continue.
   */
  inline void run() {
    AWS_LOG_INFO(__func__,
                 "Waiting for files and work.");
    auto wait_result = status_condition_monitor_->waitForWork(kTimeout);
    if (wait_result == std::cv_status::timeout) {
      return;
    }
    if (!OutputStage<TaskPtr<T>>::getSink()) {
      return;
    }
    AWS_LOG_INFO(__func__,
                 "Found work! Batching");
    FileObject<T> file_object = data_reader_->readBatch(batch_size_);
    total_logs_uploaded += file_object.batch_size;
    auto file_upload_task = task_factory_->createFileUploadTaskAsync(std::move(file_object));
    auto future_result = file_upload_task->getResult();
    auto is_accepted = OutputStage<TaskPtr<T>>::getSink()->enqueue(file_upload_task);
    std::future_status status = std::future_status::timeout;
    if (is_accepted) {
      status = future_result.wait_for(kTimeout);
    }
    if (status == std::future_status::ready) {
      AWS_LOG_INFO(__func__, "Future is valid, call file upload complete status.")
      auto result = future_result.get();
      data_reader_->fileUploadCompleteStatus(result.second, result.first);
    }
    AWS_LOG_INFO(__func__,
                 "Total logs from file completed %i", total_logs_uploaded);
  }

  bool initialize() {
    return true;
  }

  /**
   * Start the upload thread.
   */
  bool start() {
    // todo check if joinable, then don't start again
    thread = std::make_shared<std::thread>(std::bind(&FileUploadStreamer::startRun, this));
    return true;
  }

  /**
   * Join the running thread if available.
   */
  void join() {
    if (thread) {
      thread->join();
    }
  }

  // todo join wait?

private:
  /**
   * Metric on number of logs queued in the TaskObservedQueue.
   */
  size_t total_logs_uploaded = 0;

  /**
   * Current thread working on file upload management.
   */
  std::shared_ptr<std::thread> thread;

  /**
   * The configured batch size to use when uploading.
   */
  size_t batch_size_;

  /**
   * The status condition monitor to wait on before uploading.
   */
  std::shared_ptr<MultiStatusConditionMonitor> status_condition_monitor_;

  /**
   * The file manager to read data from.
   */
  std::shared_ptr<DataReader<T>> data_reader_;

  /**
   * Task factory to create tasks.
   */
  std::shared_ptr<ITaskFactory<T>> task_factory_;

  std::shared_ptr<Aws::FileManagement::StatusMonitor> network_monitor_;
};

}  // namespace FileManagement
}  // namespace Aws
