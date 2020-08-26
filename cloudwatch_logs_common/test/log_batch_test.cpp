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

#include <chrono>
#include <memory>
#include <queue>
#include <tuple>

#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <cloudwatch_logs_common/definitions/definitions.h>
#include "file_management/file_upload/file_manager_strategy.h"

using Aws::CloudWatchLogs::Utils::LogFileManager;
using namespace Aws::CloudWatchLogs;
using namespace Aws::FileManagement;

#define 24HOURS 86400000

class LogBatchTest : public ::testing::Test {
public:
  void SetUp() override
  {
  }

  void TearDown() override
  {
  }

protected:
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
        if(latestTime - curTime < 24HOURS){
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

