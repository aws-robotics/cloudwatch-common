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

#include <gtest/gtest.h>
#include <cloudwatch_logs_common/utils/log_file_manager.h>

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
        data = logs.back();
        logs.pop_back();
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

  std::vector<std::string> logs;

protected:

  uint64_t data_token = 0;

  /**
   * Options for how and where to store files, and maximum file sizes.
   */
  FileManagerStrategyOptions options_;
};

class LogBatchTest : public ::testing::Test{
public:
  void SetUp() override {
    test_strategy = std::make_shared<TestStrategy>();
    file_manager = std::make_unique<LogFileManager>(test_strategy);
  }

  void TearDown() override {
    log_data.clear();
    timestamps.clear();
  }

  void createLogs(const std::vector<long> & timestamps){
    for (auto ts : timestamps){
        input_event.SetTimestamp(ts);
        input_event.SetMessage("Testing readBatch");
        log_data.push_back(input_event);
    }
  }

  void validateBatch(const std::vector<long> & timestamps){
    auto it = batch.batch_data.begin();
    for (auto ts : timestamps){
        ASSERT_EQ(ts, (*it).GetTimestamp());
        it++;
    }
  }

  //use test_strategy to mock read/write functions from data_manager_strategy
  std::shared_ptr<TestStrategy> test_strategy;
  std::unique_ptr<LogFileManager> file_manager;
  LogCollection log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;
  std::vector<long> timestamps;
  FileObject<LogCollection> batch;
};

/**
 * Test that the upload complete with CW Failure goes to a file.
 */
TEST_F(LogBatchTest, test_readBatch_3_of_6_pass) {
  timestamps = {ONE_DAY_IN_SEC+1, 2, ONE_DAY_IN_SEC+2, 1, 0, ONE_DAY_IN_SEC};
  createLogs(timestamps);
  file_manager->write(log_data);
  batch = file_manager->readBatch(test_strategy->logs.size());
  ASSERT_EQ(3u, batch.batch_size);
  timestamps = {ONE_DAY_IN_SEC, ONE_DAY_IN_SEC+1, ONE_DAY_IN_SEC+2};
  validateBatch(timestamps);
}
TEST_F(LogBatchTest, test_readBatch_6_of_6_pass) {
  timestamps = {1, 3, 0, ONE_DAY_IN_SEC-1, 4, 2};
  createLogs(timestamps);
  file_manager->write(log_data);
  batch = file_manager->readBatch(test_strategy->logs.size());
  ASSERT_EQ(6u, batch.batch_size);
  timestamps = {0, 1, 2, 3, 4, ONE_DAY_IN_SEC-1};
  validateBatch(timestamps);
}
TEST_F(LogBatchTest, test_readBatch_1_of_6_pass) {
  timestamps = {1, ONE_DAY_IN_SEC+5, 4, 2, 0, 3};
  createLogs(timestamps);
  file_manager->write(log_data);
  batch = file_manager->readBatch(test_strategy->logs.size());
  ASSERT_EQ(1u, batch.batch_size);
  timestamps = {ONE_DAY_IN_SEC+5};
  validateBatch(timestamps);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

