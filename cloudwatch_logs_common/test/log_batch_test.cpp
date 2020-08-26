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

#include <iostream>
#include <fstream>
#include <cstdio>
#include <experimental/filesystem>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <aws/logs/model/InputLogEvent.h>
//#include <aws/core/Aws.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/AWSLogging.h>

#include <file_management/file_upload/file_manager.h>
#include "file_management/file_upload/file_manager_strategy.h"

#include <cloudwatch_logs_common/log_service.h>
#include <cloudwatch_logs_common/log_batcher.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/utils/log_file_manager.h>

#include <dataflow_lite/utils/waiter.h>

#include <memory>
#include <chrono>
#include <queue>
#include <tuple>

#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <cloudwatch_logs_common/definitions/definitions.h>
#include "file_management/file_upload/file_manager_strategy.h"

using Aws::CloudWatchLogs::Utils::LogFileManager;
using namespace Aws::CloudWatchLogs;
using namespace Aws::FileManagement;

/**
 * Test the publisher interface while ignoring all of the CloudWatch specific infrastructure.
 */
class TestPublisher : public Publisher<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>, public Waiter
{
public:
  TestPublisher() : Publisher() {
    force_failure = false;
    force_invalid_data_failure = false;
    last_upload_status = Aws::DataFlow::UploadStatus::UNKNOWN;
  };
  ~TestPublisher() override = default;

  bool start() override {
    return Publisher::start();
  }

  // notify just in case anyone is waiting
  bool shutdown() override {
    bool is_shutdown = Publisher::shutdown();
    this->notify(); //don't leave anyone blocking
    return is_shutdown;
  };

  void setForceFailure(bool nv) {
    force_failure = nv;
  }

  void setForceInvalidDataFailure(bool nv) {
    force_invalid_data_failure = nv;
  }

  Aws::DataFlow::UploadStatus getLastUploadStatus() {
    return last_upload_status;
  }

protected:

  // override so we can notify when internal state changes, as attemptPublish sets state
  Aws::DataFlow::UploadStatus attemptPublish(std::list<Aws::CloudWatchLogs::Model::InputLogEvent> &data) override {
    last_upload_status = Publisher::attemptPublish(data);
    {
      this->notify();
    }
    return last_upload_status;
  }

  Aws::DataFlow::UploadStatus publishData(std::list<Aws::CloudWatchLogs::Model::InputLogEvent>&) override {

    if (force_failure) {
      return Aws::DataFlow::UploadStatus::FAIL;

    } else if (force_invalid_data_failure) {
      return Aws::DataFlow::UploadStatus::INVALID_DATA;

    } else {
      return Aws::DataFlow::UploadStatus::SUCCESS;
    }
  }

  bool force_failure;
  bool force_invalid_data_failure;
  Aws::DataFlow::UploadStatus last_upload_status;
};

class LogBatchTest : public ::testing::Test {
public:
  void SetUp() override
  {
      test_publisher = std::make_shared<TestPublisher>();
      batcher = std::make_shared<LogBatcher>();

      //  log service owns the streamer, batcher, and publisher
      cw_service = std::make_shared<LogService>(test_publisher, batcher);

      stream_data_queue = std::make_shared<TaskObservedQueue<LogCollection>>();
      queue_monitor = std::make_shared<Aws::DataFlow::QueueMonitor<TaskPtr<LogCollection>>>();

      // create pipeline
      batcher->setSink(stream_data_queue);
      queue_monitor->addSource(stream_data_queue, Aws::DataFlow::PriorityOptions{Aws::DataFlow::HIGHEST_PRIORITY});
      cw_service->setSource(queue_monitor);

      cw_service->start(); //this starts the worker thread
      EXPECT_EQ(Aws::DataFlow::UploadStatus::UNKNOWN, test_publisher->getLastUploadStatus());
  }

  void TearDown() override
  {
      if (cw_service) {
        cw_service->shutdown();
        cw_service->join();
      }
    //std::experimental::filesystem::path storage_path(options.storage_directory);
    //std::experimental::filesystem::remove_all(storage_path);
  }

protected:
  FileManagerStrategyOptions options{"test", "log_tests/", ".log", 1024*1024, 1024*1024};

  std::shared_ptr<TestPublisher> test_publisher;
  std::shared_ptr<LogBatcher> batcher;
  std::shared_ptr<LogService> cw_service;

  std::shared_ptr<TaskObservedQueue<LogCollection>> stream_data_queue;
  std::shared_ptr<Aws::DataFlow::QueueMonitor<TaskPtr<LogCollection>>>queue_monitor;
};

auto log_comparison = [](const LogType & log1, const LogType & log2)
  { return log1.GetTimestamp() < log2.GetTimestamp(); };

/**
 * Test File Manager
 */
class TestLogFileManager : public FileManager<LogCollection>, public Waiter
{
public:

    TestLogFileManager() : FileManager(nullptr) {
      written_count.store(0);
    }

    void write(const LogCollection & data) override {
      last_data_size = data.size();
      written_count++;
      this->notify();
    };

    //test that the readBatch function works with 24 hour interval
    FileObject<LogCollection> readBatch(size_t batch_size) override {
      std::priority_queue<std::tuple<long, std::string, uint64_t>> pq;
      long latestTime = 0;
      for (size_t i = 0; i < batch_size; ++i) {
        std::string line;
        if(i == batch_size-1){
            line = "{\"timestamp\":" + std::to_string(i + 86400000) + ",\"message\":\"Last log in batch file\"}";
        }
        else{
            line = "{\"timestamp\":" + std::to_string(i) + ",\"message\":\"Testing batch file\"}";
        }
        Aws::String aws_line(line.c_str());
        Aws::Utils::Json::JsonValue value(aws_line);
        Aws::CloudWatchLogs::Model::InputLogEvent input_event(value);
        pq.push(std::make_tuple(input_event.GetTimestamp(), line, 0));
        if(input_event.GetTimestamp() > latestTime){
          latestTime = input_event.GetTimestamp();
        }
      }

      std::set<LogType, decltype(log_comparison)> log_set(log_comparison);
      size_t actual_batch_size = 0;
      while(!pq.empty()){
        long curTime = std::get<0>(pq.top());
        std::string line = std::get<1>(pq.top());
        if(latestTime - curTime < 86400000){
          Aws::String aws_line(line.c_str());
          Aws::Utils::Json::JsonValue value(aws_line);
          Aws::CloudWatchLogs::Model::InputLogEvent input_event(value);
          actual_batch_size++;
          log_set.insert(input_event);
        }
        pq.pop();
      }

      LogCollection log_data(log_set.begin(), log_set.end());
      FileObject<LogCollection> file_object;
      file_object.batch_data = log_data;
      file_object.batch_size = actual_batch_size;
      return file_object;
    }

    std::atomic<int> written_count{};
    std::atomic<size_t> last_data_size{};
    std::condition_variable cv;
    mutable std::mutex mtx;
};

TEST_F(LogBatchTest, Sanity) {
  ASSERT_TRUE(true);
}

TEST_F(LogBatchTest, batch_test_24hours) {
  std::shared_ptr<TestLogFileManager> fileManager = std::make_shared<TestLogFileManager>();
  auto batch = fileManager->readBatch(5);
  ASSERT_EQ(1u, batch.batch_size);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

