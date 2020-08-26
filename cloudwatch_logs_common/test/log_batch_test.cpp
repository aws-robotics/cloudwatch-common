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

    std::atomic<int> written_count{};
    std::atomic<size_t> last_data_size{};
    std::condition_variable cv;
    mutable std::mutex mtx;
};

class TestDataManagerStrategy : DataManagerStrategy, public Service {
public:
  DataManagerStrategy() = default;
  ~DataManagerStrategy() override = default;

  virtual DataToken read(std::string &data) = 0;
};

TEST_F(LogBatchTest, Sanity) {
  ASSERT_TRUE(true);
}
/**
 * Read 5 logs in batch
 * Expect one of them to be within 24 hour interval
 */
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

