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

const long ONE_DAY_IN_SEC = 86400000;

class TestStrategy : public DataManagerStrategy {
public:
  bool isDataAvailable(){
    return true;
  }

  DataToken read(std::string &data) override{
    if(!logs.empty()){
        data = logs.front();
        logs.pop_front();
    }

    data_token++;
    return data_token;
  }

  void write(const std::string &data){
      logs.push_back(data);
  }

  void resolve(const DataToken &token, bool is_success){
    if(is_success && token)
        return;
    else
        return;
    return;
  }

  std::list<std::string> logs;

protected:

  uint64_t data_token = 0;

  /**
   * Options for how and where to store files, and maximum file sizes.
   */
  FileManagerStrategyOptions options_;
};

/**
 * Test that the upload complete with CW Failure goes to a file.
 */
TEST(log_batch_test, 3PASS) {
  //use test_strategy to mock read/write functions from data_manager_strategy
  std::shared_ptr<TestStrategy> test_strategy = std::make_shared<TestStrategy>();
  LogFileManager file_manager(test_strategy);
  LogCollection log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;

  //add test data to logs
  input_event.SetTimestamp(0);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(1);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(2);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(ONE_DAY_IN_SEC+2);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(ONE_DAY_IN_SEC+1);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(ONE_DAY_IN_SEC);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  file_manager.write(log_data);

  //read the batch
  auto batch = file_manager.readBatch(test_strategy->logs.size());

  //only the latest logs should be included in batch
  ASSERT_EQ(3u, batch.batch_size);

  //iterate through the logs in batch
  auto it = batch.batch_data.begin();

  //validate that they are the latest timestamps
  ASSERT_EQ(ONE_DAY_IN_SEC, (*it).GetTimestamp());
  it++;
  ASSERT_EQ(ONE_DAY_IN_SEC+1, (*it).GetTimestamp());
  it++;
  ASSERT_EQ(ONE_DAY_IN_SEC+2, (*it).GetTimestamp());
}
TEST(log_batch_test, ALLPASS) {
  //use test_strategy to mock read/write functions from data_manager_strategy
  std::shared_ptr<TestStrategy> test_strategy = std::make_shared<TestStrategy>();
  LogFileManager file_manager(test_strategy);
  LogCollection log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;

  //add test data to logs
  input_event.SetTimestamp(0);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(1);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(2);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(3);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(4);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(ONE_DAY_IN_SEC-1);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  file_manager.write(log_data);

  //read the batch
  auto batch = file_manager.readBatch(test_strategy->logs.size());

  //only the latest logs should be included in batch
  ASSERT_EQ(6u, batch.batch_size);

  //iterate through the logs in batch
  auto it = batch.batch_data.begin();

  //validate that they are the latest timestamps
  ASSERT_EQ(0, (*it).GetTimestamp());
  it++;
  ASSERT_EQ(1, (*it).GetTimestamp());
  it++;
  ASSERT_EQ(2, (*it).GetTimestamp());
  it++;
  ASSERT_EQ(3, (*it).GetTimestamp());
  it++;
  ASSERT_EQ(4, (*it).GetTimestamp());
  it++;
  ASSERT_EQ(ONE_DAY_IN_SEC-1, (*it).GetTimestamp());
}
TEST(log_batch_test, ONEPASS) {
  //use test_strategy to mock read/write functions from data_manager_strategy
  std::shared_ptr<TestStrategy> test_strategy = std::make_shared<TestStrategy>();
  LogFileManager file_manager(test_strategy);
  LogCollection log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;

  //add test data to logs
  input_event.SetTimestamp(0);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(1);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(ONE_DAY_IN_SEC+5);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(2);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(3);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  input_event.SetTimestamp(4);
  input_event.SetMessage("Testing readBatch");
  log_data.push_back(input_event);
  file_manager.write(log_data);

  //read the batch
  auto batch = file_manager.readBatch(test_strategy->logs.size());

  //only the latest logs should be included in batch
  ASSERT_EQ(1u, batch.batch_size);

  //iterate through the logs in batch
  auto it = batch.batch_data.begin();

  //validate that they are the latest timestamps
  ASSERT_EQ(ONE_DAY_IN_SEC+5, (*it).GetTimestamp());
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

