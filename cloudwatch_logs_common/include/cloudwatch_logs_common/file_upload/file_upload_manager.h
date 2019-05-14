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

#include <cloudwatch_logs_common/file_upload/status_monitor.h>
#include <cloudwatch_logs_common/file_upload/observed_queue.h>
#include <cloudwatch_logs_common/file_upload/queue_monitor.h>
#include <cloudwatch_logs_common/file_upload/task_utils.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>


namespace Aws {
namespace FileManagement {

struct FileManagerOptions {

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
class FileUploadManager {
public:
  /**
   * Create a file upload manager.
   *
   * @param status_condition_monitor
   * @param file_manager
   * @param observed_queue
   * @param batch_size
   */
  explicit FileUploadManager(
    std::shared_ptr<MultiStatusConditionMonitor> status_condition_monitor,
    std::shared_ptr<FileManager<T>> file_manager,
    std::shared_ptr<TaskObservedQueue<T>> observed_queue,
    size_t batch_size)
  {
    status_condition_monitor_ = status_condition_monitor;
    file_manager_ = file_manager;
    observed_queue_ = observed_queue;
    batch_size_ = batch_size;
  }

  virtual ~FileUploadManager() {
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
  void addStatusMonitor(std::shared_ptr<StatusMonitor> &status_monitor) {
    status_condition_monitor_->addStatusMonitor(status_monitor);
  }

  void subscribe(
    IQueueMonitor<TaskPtr<T>> &queue_monitor,
    PriorityOptions options = PriorityOptions())
  {
    queue_monitor.addQueue(observed_queue_, options);
  }

  inline void startRun() {
    while (true) {
      run();
    }
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
    // @todo(rddesmon): add timeout on wait for work and break out if the thread is cancelled.
    AWS_LOG_INFO(__func__,
                 "Waiting for files and work.");
    status_condition_monitor_->waitForWork();
    AWS_LOG_INFO(__func__,
                 "Found work! Batching");
    FileObject<T> file_object = file_manager_->readBatch(batch_size_);
    total_logs_uploaded += file_object.batch_size;
    auto file_upload_task =
        std::make_shared<FileUploadTaskAsync<T>>(file_object);
    auto future_result = file_upload_task->getResult();
    observed_queue_->enqueue(file_upload_task);
    future_result.wait();
    if (future_result.valid()) {
      AWS_LOG_INFO(__func__, "Future is valid, call file upload complete status.")
      auto result = future_result.get();
      file_manager_->fileUploadCompleteStatus(result.second, result.first);
    }
    AWS_LOG_INFO(__func__,
                 "Total logs from file completed %i", total_logs_uploaded);
  }

  /**
   * Start the upload thread.
   */
  void start() {
    thread = std::make_shared<std::thread>(std::bind(&FileUploadManager::startRun, this));
  }

  /**
   * Join the running thread if available.
   */
  void join() {
    if (thread) {
        thread->join();
      }
  }

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
  std::shared_ptr<FileManager<T>> file_manager_;

  /**
   * The queue which to add tasks to.
   */
  std::shared_ptr<TaskObservedQueue<T>> observed_queue_;
};

}  // namespace FileManagement
}  // namespace Aws
