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
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>

#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Aws {
namespace CloudWatchLogs {

// todo this can be moved to utils for metrics
// todo could to a template of <T, D> where D is the data to be stored in the list / vector
// todo this SHOULD have a backing list / vector as act as a container, also handle drop
//   where drop is abstract (when max allowable size is reached)
/**
 * Abstract class used to define a batching interface.
 * @tparam T the type of data to be batched.
 */
template<typename T>
class DataBatcher : public Service {
public:

  static const size_t kDefaultTriggerSize = SIZE_MAX;

  DataBatcher() {
    this->publish_trigger_batch_size_.store(DataBatcher::kDefaultTriggerSize);
  }

  DataBatcher(size_t size) {
    this->publish_trigger_batch_size_.store(size);
  }

  ~DataBatcher() = default;

  virtual bool batchData(const T &data_to_batch) = 0;

  virtual bool batchData(const T &data_to_batch, const std::chrono::milliseconds & milliseconds) = 0;

  virtual bool publishBatchedData() = 0;

  virtual size_t getCurrentBatchSize() = 0;

  inline void setPublishTriggerBatchSize(size_t new_value) {
    this->publish_trigger_batch_size_.store(new_value);
  }

  inline size_t getPublishTriggerBatchSize() {
    return this->publish_trigger_batch_size_.load();

  }
  inline void resetPublishTriggerBatchSize(size_t new_value) {
      this->publish_trigger_batch_size_.store(kDefaultTriggerSize);
  }

private:
  /**
   * Size used for the internal storage
   */
  std::atomic<size_t> publish_trigger_batch_size_;
};

using LogType = std::list<Aws::CloudWatchLogs::Model::InputLogEvent>;

class LogBatcher :
  public Aws::DataFlow::OutputStage<Aws::FileManagement::TaskPtr<LogType>>,
  public DataBatcher<std::string>
{
public:

  static const size_t kDefaultMaxBatchSize = 1024; // todo is this even reasonable? need data

  /**
   *  @brief Creates a new LogBatcher
   *  Creates a new LogBatcher that will group/buffer logs. Note: logs are only automatically published if the
   *  size is set, otherwise the publishBatchedData is necesary to push data to be published.
   *
   *  @throws invalid argument if publish_trigger_size is strictly greater than max_allowable_batch_size
   *  @param size of the batched data that will trigger a publish
   */
  explicit LogBatcher(size_t max_allowable_batch_size = kDefaultMaxBatchSize,
                      size_t publish_trigger_size = DataBatcher::kDefaultTriggerSize);

  /**
   *  @brief Tears down a LogBatcher object
   */
  virtual ~LogBatcher();

  /**
   *  @brief Used to add a log event to the buffer
   *  Adds the given log event to the internal buffer that will be flushed periodically.
   *  @param a string containing the log msg
   *
   *  @return true if the log was batched successfully, false otherwise
   */
  virtual bool batchData(const std::string & log_msg_formatted) override;

    /**
     *  @brief Used to add a log event to the buffer
     *  Adds the given log event to the internal buffer that will be flushed periodically.
     *  @param a string containing the log msg
 *      @param a timestamp associated with this log event (in milliseconds)
     *
     *  @return true if the log was batched successfully, false otherwise
     */
  virtual bool batchData(const std::string & log_msg_formatted, const std::chrono::milliseconds & milliseconds) override;

  /**
   *  @brief Services the log manager by performing periodic tasks when called.
   *  Calling the Service function allows for periodic tasks associated with the log manager, such
   * as flushing buffered logs, to be performed.
   *
   *  @return true of the data was succesfully published, false otherwise
   */
  virtual bool publishBatchedData() override;

  virtual bool start() override;
  virtual bool shutdown() override;

  /**
   * Set the log file manager, used for task publishing failures (write to disk if unable to send to CloudWatch).
   *
   * @throws invalid argument if the input is null
   * @param log_file_manager
   */
  virtual void setLogFileManager(std::shared_ptr<Aws::CloudWatchLogs::Utils::FileManager<LogType>> log_file_manager);

  /**
   * Return the number of currently batched items.
   * @return
   */
  virtual size_t getCurrentBatchSize() override;

  size_t getMaxAllowableBatchSize(); //todo part of data batcher?
  void setMaxAllowableBatchSize(size_t new_value); //todo part of data batcher?

  bool handleSizeExceeded(); //todo part of data batcher?

protected:
  virtual Aws::CloudWatchLogs::Model::InputLogEvent convertToLogEvent(const std::string & message,
          const std::chrono::milliseconds & milliseconds);
  /**
   *
   * @throws invalid argument if the publish_trigger_size is strictly greater than max_allowable_batch_size
   * @param publish_trigger_size
   * @param max_allowable_batch_size
   */
  void validateConfigurableSizes(size_t publish_trigger_size, size_t max_allowable_batch_size);

private:
  //todo should probably be atomic, but currently controlled by the publish mutex
  std::shared_ptr<LogType> batched_data_; //todo vector, part of data batcher?
  std::recursive_mutex batch_and_publish_lock_;
  std::shared_ptr<Aws::CloudWatchLogs::Utils::FileManager<LogType>> log_file_manager_;
  std::atomic<size_t> max_allowable_batch_size_; //todo part of data batcher?

  //todo stats? how many times published? rate of publishing? throughput?
};

}  // namespace CloudWatchLogs
}  // namespace Aws
