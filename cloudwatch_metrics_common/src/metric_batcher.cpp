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

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogMacros.h>

#include <cloudwatch_metrics_common/metric_batcher.h>

#include <file_management/file_upload/file_upload_task.h>

#include <dataflow_lite/utils/data_batcher.h>

#include <chrono>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>

using namespace Aws::CloudWatchMetrics;
using namespace Aws::FileManagement;

MetricBatcher::MetricBatcher(size_t max_allowable_batch_size,
                       size_t publish_trigger_size)
        : DataBatcher(max_allowable_batch_size, publish_trigger_size) {
}

MetricBatcher::~MetricBatcher() = default;

bool MetricBatcher::publishBatchedData() {
  std::lock_guard<std::recursive_mutex> lk(mtx);

  // is there anything to send?
  if (getCurrentBatchSize() == 0) {
    AWS_LOGSTREAM_DEBUG(__func__, "Nothing batched to publish");
    return false;
  }

  if (getSink()) {

    auto metrics_to_publish = this->batched_data_;
    std::shared_ptr<BasicTask<MetricDatumCollection>> metric_task = std::make_shared<BasicTask<MetricDatumCollection>>(metrics_to_publish);

    if (metric_file_manager_ ) {

      // register the task failure function
      auto function = [&metric_file_manager = this->metric_file_manager_](const FileManagement::UploadStatus &upload_status,
                                                                    const MetricDatumCollection &metrics_to_publish)
      {
          if (!metrics_to_publish.empty()) {
            if (FileManagement::SUCCESS != upload_status) {
              AWS_LOG_INFO(__func__, "Task failed: writing metrics to file");
              metric_file_manager->write(metrics_to_publish);
            }
          }
      };

      metric_task->setOnCompleteFunction(function);
    }

    bool enqueue_success = getSink()->tryEnqueue(metric_task, this->getTryEnqueueDuration());

    if (!enqueue_success) {
      AWS_LOG_WARN(__func__, "Unable to enqueue data, marking as failed");
      metric_task->onComplete(FileManagement::UploadStatus::FAIL);
    }

    this->resetBatchedData();
    return enqueue_success;

  } else {
    AWS_LOGSTREAM_WARN(__func__, "Unable to obtain queue");
    return false;
  }
}

void MetricBatcher::handleSizeExceeded() {
  std::lock_guard<std::recursive_mutex> lk(mtx);

  if (this->metric_file_manager_) {
    AWS_LOG_INFO(__func__, "Writing data to file");
    metric_file_manager_->write(*this->batched_data_);
  } else {
    AWS_LOG_WARN(__func__, "Dropping data");
  }
  this->resetBatchedData();
}


bool MetricBatcher::start() {
  if (metric_file_manager_ == nullptr) {
    AWS_LOGSTREAM_WARN(__func__, "FileManager not found: data will be dropped on failure.");
  }
  return true;
}

bool MetricBatcher::shutdown() {
  if (mtx.try_lock()) {
    this->handleSizeExceeded(); // attempt to write to disk before discarding
    return true;
  }
  return false;
}

void MetricBatcher::setMetricFileManager(std::shared_ptr<Aws::FileManagement::FileManager<MetricDatumCollection>> metric_file_manager)
{
  if (nullptr == metric_file_manager) {
    throw std::invalid_argument("input FileManager cannot be null");
  }
  this->metric_file_manager_ = metric_file_manager;
}