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

#include <aws/core/Aws.h>
#include <aws/logs/model/InputLogEvent.h>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>

#include <cloudwatch_logs_common/file_upload/file_upload_streamer.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>

#include <cloudwatch_logs_common/log_batcher.h>
#include <cloudwatch_logs_common/log_publisher.h>

#include <cloudwatch_logs_common/utils/service.h>

#include <chrono>
#include <stdexcept>

namespace Aws {
namespace CloudWatchLogs {

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;

static const std::chrono::milliseconds kDequeueDuration = std::chrono::milliseconds(100);

/**
 *
 * @tparam T the type to be published to cloudwatch
 * @tparam D the type to be batched and converted to the cloudwatch published type T
 */
template<typename T, typename D>
class CloudWatchService : public Aws::DataFlow::InputStage<TaskPtr<T>>, public RunnableService {
public:
  CloudWatchService(std::shared_ptr<Publisher<T>> publisher,
    std::shared_ptr<DataBatcher<D>> batcher) : RunnableService() {

    if (nullptr == publisher) {
      throw "Invalid argument: log_publisher cannot be null";
    }

    if (nullptr == batcher) {
      throw "Invalid argument: log_publisher cannot be null";
    }

    this->log_file_upload_streamer_ = nullptr;
    this->publisher_ = publisher;
    this->batcher_ = batcher;
    this->dequeue_duration_ = kDequeueDuration;
    this->number_dequeued_.store(0);
  }

  ~CloudWatchService() = default;

  /**
   * Start everything up.
   */
  virtual bool start() {

    publisher_->start();
    batcher_->start();

    if (log_file_upload_streamer_) {
      log_file_upload_streamer_->start();
    }

    //start the thread to dequeue
    return RunnableService::start();
  }

  /**
   * Shut everything down.
   */
  virtual inline bool shutdown() {
    bool b = true;

    b &= publisher_->shutdown();
    b &= batcher_->shutdown();

    if (log_file_upload_streamer_) {
      b &= log_file_upload_streamer_->shutdown();
    }

    b &= RunnableService::shutdown();

    return b;
  }

  /**
   * Userland entry point to batch data for publishing
   * @param data_to_batch
   */
  virtual inline bool batchData(const D &data_to_batch) {
    return batcher_->batchData(data_to_batch);
  }

  virtual inline bool batchData(const D &data_to_batch, const std::chrono::milliseconds &milliseconds) {
    return batcher_->batchData(data_to_batch, milliseconds);
  }

  /**
   * Publishing mechanism
   * @param data_to_batch
   */
  virtual inline bool publishBatchedData() {
    if (batcher_) {
      return batcher_->publishBatchedData();
    }
    return false;
  }

  virtual inline std::chrono::milliseconds getDequeueDuration() {
    return this->dequeue_duration_;
  }

  virtual inline bool setDequeueDuration(std::chrono::milliseconds new_value) {
    bool is_set = false;
    if (new_value.count() >= 0) {
      this->dequeue_duration_ = new_value;
      is_set = true;
    }
    return is_set;
  }

  /**
   * Get the total number of items dequeued (and sent to the publishing mechanism) from the work thread.
   */
  int getNumberDequeued() {
    return this->number_dequeued_.load();
  }

protected:

  /**
   * Main workhorse thread that dequeues from the source and calls Task run.
   */
  void work() {

    TaskPtr<T> task_to_publish;
    bool is_dequeued = Aws::DataFlow::InputStage<TaskPtr<T>>::getSource()->dequeue(task_to_publish, dequeue_duration_);

    if (is_dequeued) {
      AWS_LOGSTREAM_INFO(__func__, "Dequeued " << this->number_dequeued_++);

      if (task_to_publish) {
        this->number_dequeued_++;
        task_to_publish->run(publisher_); // publish mechanism via the TaskFactory
      }
    }
  }


  std::shared_ptr<FileUploadStreamer<T>> log_file_upload_streamer_; //kept here for startup and shutdown
  std::shared_ptr<Publisher<T>> publisher_; //kept here for startup shutdown
  std::shared_ptr<DataBatcher<D>> batcher_;
  /**
   * Duration to wait for work from the queue.
   */
  std::chrono::milliseconds dequeue_duration_;

private:
  /**
   * Count of total items dequeued.
   */
  std::atomic<int> number_dequeued_;
};

// hide the template from userland
class LogService : public CloudWatchService<LogType, std::string> {
public:

  LogService(std::shared_ptr<FileUploadStreamer<LogType>> log_file_upload_streamer,
          std::shared_ptr<Publisher<LogType>> log_publisher,
          std::shared_ptr<DataBatcher<std::string>> log_batcher)
          : CloudWatchService(log_publisher, log_batcher) {

    this->log_file_upload_streamer_ = log_file_upload_streamer;
  }

};

}
}

