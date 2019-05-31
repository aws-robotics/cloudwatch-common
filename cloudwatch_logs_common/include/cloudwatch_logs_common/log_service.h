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
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/task_factory.h>

#include <cloudwatch_logs_common/log_batcher.h>
#include <cloudwatch_logs_common/log_publisher.h>

namespace Aws {
namespace CloudWatchLogs {

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;


/**
 *
 * @tparam T
 * @tparam D
 */
template<typename T, typename D>
class CloudWatchService : public Aws::DataFlow::InputStage<TaskPtr<T>> {
public:

    /**
     *
     */
    virtual inline void start() {
      //todo atomic
      if(should_run_.load()) {
        return;
      }

      log_publisher_->initialize();

      log_file_upload_streamer_->start();
      //start the thread to dequeue
      if(!worker_thread_.joinable()) {
        should_run_.store(true);
        worker_thread_ = std::thread(&CloudWatchService::Work, this);
      }
    }

    /**
     *
     */
    virtual inline void shutdown() {
      //todo atomic
      //stop publisher
      //clear batcher
      //streamer stop (owns file manager, so it's shutdown?)
      log_publisher_->shutdown();
      should_run_.store(false);
    }

    //entry point to batch data
    //todo generic function
    virtual inline void batchData(const D &data_to_batch) {
      log_batcher_->batchData(data_to_batch);
    }
    //todo interface?
    virtual inline void publishBatchedData() {
      log_batcher_->publishBatchedData();
    }


    virtual inline std::chrono::milliseconds getDequeueDuration() {
      return this->dequeue_duration_;
    }

    virtual inline bool setDequeueDuration(std::chrono::milliseconds new_value) {
      if (new_value.count() >= 0) {
        this->dequeue_duration_ = new_value;
      }
    }

protected:

    inline void Work() {
      //take input and publish it
      while(should_run_.load()) {
        TaskPtr<LogType> t;
        bool b = Aws::DataFlow::InputStage<TaskPtr<T>>::getSource()->dequeue(t, dequeue_duration_);
        if (b) {
          t->run();
        }
      }

      // todo we fell through, cancel all the tasks in the queue

    }
    std::shared_ptr<FileUploadStreamer<T>> log_file_upload_streamer_; //kept here for startup and shutdown
    std::shared_ptr<Publisher<T>> log_publisher_; //kept here for startup shutdown
    std::thread worker_thread_;
    std::atomic<bool> should_run_;
    std::shared_ptr<DataBatcher<D>> log_batcher_;
    std::chrono::milliseconds dequeue_duration_;
};

// hide the template from userland
class LogService : public CloudWatchService<LogType, std::string> {
public:

    LogService(std::shared_ptr<FileUploadStreamer<LogType>> log_file_upload_streamer,
            std::shared_ptr<Publisher<LogType>> log_publisher,
            std::shared_ptr<DataBatcher<std::string>> log_batcher) {

      this->log_file_upload_streamer_ = log_file_upload_streamer;
      this->log_publisher_ = log_publisher;
      this->log_batcher_ = log_batcher;
      this->dequeue_duration_ = std::chrono::milliseconds(100); //todo magic constant
    }
};

}
}

