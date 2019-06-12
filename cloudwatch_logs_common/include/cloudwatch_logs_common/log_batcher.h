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
/**
 * Abstract class used to define a batching interface.
 * @tparam T the type of data to be batched.
 */
template<typename T>
class DataBatcher : public Service {
public:
  static const size_t kDefaultBatchSize = SIZE_MAX;
  DataBatcher() {
    this->max_batch_size_.store(DataBatcher::kDefaultBatchSize);
  }
  DataBatcher(size_t size) {
    this->max_batch_size_.store(size);
  }
  virtual bool batchData(const T &data_to_batch) = 0;
  virtual bool batchData(const T &data_to_batch, const std::chrono::milliseconds & milliseconds) = 0;
  virtual bool publishBatchedData() = 0;
  virtual size_t getCurrentBatchSize() = 0;
  inline void setMaxBatchSize(size_t new_value) {
    this->max_batch_size_.store(new_value);
  }
  inline size_t getMaxBatchSize() {
    return this->max_batch_size_.load();
  }
  inline void resetSize(size_t new_value) {
      this->max_batch_size_.store(kDefaultBatchSize);
  }
private:
  /**
   * Size used for the internal storage
   */
  std::atomic<size_t> max_batch_size_;
};

using LogType = std::list<Aws::CloudWatchLogs::Model::InputLogEvent>;

class LogBatcher :
  public Aws::DataFlow::OutputStage<Aws::FileManagement::TaskPtr<LogType>>,
  public DataBatcher<std::string>
{
public:
  /**
   *  @brief Creates a new LogBatcher
   *  Creates a new LogBatcher that will group/buffer logs. Note: logs are only automatically published if the
   *  size is set, otherwise the publishBatchedData is necesary to push data to be published.
   */
  explicit LogBatcher();

  /**
   *  @brief Creates a new LogBatcher
   *  Creates a new LogBatcher that will group/buffer logs. Note: logs are only automatically published if the
   *  size is set, otherwise the publishBatchedData is necesary to push data to be published.
   *
   *  @param size of the batched data that will trigger a publish
   */
  explicit LogBatcher(size_t size);

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

  virtual size_t getCurrentBatchSize() override;

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
protected:
  virtual Aws::CloudWatchLogs::Model::InputLogEvent convertToLogEvent(const std::string & message,
          const std::chrono::milliseconds & milliseconds);

private:
  //todo should probably be atomic, but currently controlled by the publish mutex
  std::shared_ptr<LogType> batched_data_; //todo vector
  std::recursive_mutex batch_and_publish_lock_;
  //todo stats? how many times published? rate of publishing? throughput?
};

}  // namespace CloudWatchLogs
}  // namespace Aws
