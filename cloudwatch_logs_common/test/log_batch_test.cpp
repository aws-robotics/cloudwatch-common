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

#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <cloudwatch_logs_common/utils/log_file_manager.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using Aws::CloudWatchLogs::Utils::LogFileManager;
using namespace Aws::CloudWatchLogs;
using namespace Aws::FileManagement;

class TestStrategy : public DataManagerStrategy {
public:
  TestStrategy(const FileManagerStrategyOptions &options) {
    options_ = options;
  }

  bool isDataAvailable(){
    return !logs.empty() && !data_tokens.empty();
  }

  bool isDeleteStaleData(){
    return options_.delete_stale_data;
  }

  DataToken read(std::string &data) override{
    if(isDataAvailable()){
      it++;
      data = logs[it-1];
      return data_tokens[it-1];
    }

    return 0;
  }

  void write(const std::string &data){
      logs.push_back(data);
      data_tokens.push_back(data_token);
      data_token++;
  }

  void resolve(const DataToken &token, bool is_success){
    if(is_success){
      for(int i = 0; i < (int)data_tokens.size(); i++){
        if(data_tokens[i] == token){
          data_tokens.erase(data_tokens.begin()+i);
          logs.erase(logs.begin()+i);
          return;
        }
      }
    }
    return;
  }

  std::vector<std::string> logs;
  std::vector<uint64_t> data_tokens;
  uint64_t data_token = 1;
  int it = 0;
  FileManagerStrategyOptions options_;
};

class LogBatchTest : public ::testing::Test{
public:
  void SetUp() override {
    test_strategy = std::make_shared<TestStrategy>(options);
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

  void readLogs(){
    while(test_strategy->isDataAvailable()){
      test_strategy->it = 0;
      batch = file_manager->readBatch(test_strategy->logs.size());
      resolveBatch();
      file_manager->deleteStaleData();
      ASSERT_TRUE(validateBatch());
    }
  }

  void resolveBatch(){
    for (auto dt : batch.data_tokens){
      test_strategy->resolve(dt, true);
    }
  }

  //validate that the batch produced matches with the user's expectation
  bool validateBatch(){
    for(auto bd : batch.batch_data){
      if(expectedTimeStamps.empty())
        return false;

      long expectedTimestamp = expectedTimeStamps.front().back();
      expectedTimeStamps.front().pop_back();

      if(expectedTimeStamps.front().empty())
        expectedTimeStamps.erase(expectedTimeStamps.begin());

      if(bd.GetTimestamp() != expectedTimestamp){
        return false;
      }
    }

    return true;
  }

  std::shared_ptr<TestStrategy> test_strategy;
  std::unique_ptr<LogFileManager> file_manager;
  LogCollection log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;
  std::vector<long> timestamps;
  FileObject<LogCollection> batch;

  //FileManagerStrategyOptions includes a new option "delete_stale_data" which we will
  //set to true for testing
  FileManagerStrategyOptions options{"test", "log_tests/", ".log", 1024*1024, 1024*1024, true};

  //the expecteTimeStamps will hold the log batch results we expect from completion of
  //the readBatch function
  std::vector<std::vector<long>> expectedTimeStamps;
};

/**
 * This test will group all logs into one batch since they are all
 * within 24 hours of each other
 */
TEST_F(LogBatchTest, test_readBatch_1_batch) {
  timestamps = {1, 3, 0, ONE_DAY_IN_MILLISEC-1, 4, 2};
  expectedTimeStamps = {{ONE_DAY_IN_MILLISEC-1, 4, 3, 2, 1, 0}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
/**
 * This test will group half the logs into one batch and half into another
 * since there is > 24 hour difference from ONE_DAY_IN_MILLISEC+2 and 2
 */
TEST_F(LogBatchTest, test_readBatch_2_batches) {
  timestamps = {ONE_DAY_IN_MILLISEC+1, 2, ONE_DAY_IN_MILLISEC+2, 1, 0, ONE_DAY_IN_MILLISEC};
  expectedTimeStamps = {{ONE_DAY_IN_MILLISEC+2, ONE_DAY_IN_MILLISEC+1, ONE_DAY_IN_MILLISEC}, {2, 1, 0}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
/**
 * This test will group three different batches since ONE_DAY_IN_MILLISEC*2+10, 
 * ONE_DAY_IN_MILLISEC+5, and 4 are all > 24 hours apart
 */
TEST_F(LogBatchTest, test_readBatch_3_batches) {
  timestamps = {1, ONE_DAY_IN_MILLISEC+5, 4, 2, 0, ONE_DAY_IN_MILLISEC*2+10};
  expectedTimeStamps = {{ONE_DAY_IN_MILLISEC*2+10}, {ONE_DAY_IN_MILLISEC+5},{4, 2, 1, 0}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
/**
 * We defined delete_stale_data as true in our FileManagerStrategyOptions
 * In this test we expect that there will be two separate batches
 * separated by > 24 hours and we expect that timestamp 0 will be deleted
 * since it is > 14 days old.
 */
TEST_F(LogBatchTest, test_2_week_delete_1_of_6) {
  timestamps = {15, ONE_DAY_IN_MILLISEC, 0, TWO_WEEK_IN_MILLISEC+5, TWO_WEEK_IN_MILLISEC+1, 10};
  expectedTimeStamps = {{TWO_WEEK_IN_MILLISEC+5, TWO_WEEK_IN_MILLISEC+1}, {ONE_DAY_IN_MILLISEC, 15, 10}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
/**
 * We defined delete_stale_data as true in our FileManagerStrategyOptions
 * In this test we expect that there will be two separate batches
 * separated by > 24 hours and we expect that timestamp 0, 1, 3, 4  will be
 * deleted since it is > 14 days old.
 */
TEST_F(LogBatchTest, test_2_week_delete_4_of_6) {
  timestamps = {1, ONE_DAY_IN_MILLISEC, 4, TWO_WEEK_IN_MILLISEC+5, 0, 3};
  expectedTimeStamps = {{TWO_WEEK_IN_MILLISEC+5}, {ONE_DAY_IN_MILLISEC}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
/**
 * We defined delete_stale_data as true in our FileManagerStrategyOptions
 * In this test we expect that there will be two separate batches
 * separated by > 24 hours and we expect that timestamp 0, 1, 3, 4, and
 * ONE_DAY_IN_MILLISEC will be deleted since it is > 14 days old.
 */
TEST_F(LogBatchTest, test_2_week_delete_5_of_6) {
  timestamps = {1, ONE_DAY_IN_MILLISEC, 4, TWO_WEEK_IN_MILLISEC+ONE_DAY_IN_MILLISEC+5, 0, 3};
  expectedTimeStamps = {{TWO_WEEK_IN_MILLISEC+ONE_DAY_IN_MILLISEC+5}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
/**
 * We defined delete_stale_data as true in our FileManagerStrategyOptions
 * In this test, we set the option delete_stale_data to false and expect
 * that none of the logs will be deleted. We expect three batches separated
 * by > 24 hours
 */
TEST_F(LogBatchTest, test_2_week_no_delete) {
  test_strategy->options_.delete_stale_data = false;
  timestamps = {1, ONE_DAY_IN_MILLISEC, 4, TWO_WEEK_IN_MILLISEC+5, 0, 3};
  expectedTimeStamps = {{TWO_WEEK_IN_MILLISEC+5},{ONE_DAY_IN_MILLISEC, 4, 3, 1}, {0}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
/**
 * FileManagerStrategyOptions defined with delete_stale_data set to true.
 * We expect isDeleteStaleData to return true.
 */
TEST(DeleteOptionTest, file_manager_delete_true) {
  FileManagerStrategyOptions options{"test", "log_tests/", ".log", 1024*1024, 1024*1024, true};
  std::shared_ptr<FileManagerStrategy> file_manager_strategy = std::make_shared<FileManagerStrategy>(options);
  LogFileManager file_manager(file_manager_strategy);
  ASSERT_TRUE(file_manager_strategy->isDeleteStaleData());
}
/**
 * FileManagerStrategyOptions defined with delete_stale_data set to false.
 * We expect isDeleteStaleData to return false.
 */
TEST(DeleteOptionTest, file_manager_delete_false) {
  FileManagerStrategyOptions options{"test", "log_tests/", ".log", 1024*1024, 1024*1024, false};
  std::shared_ptr<FileManagerStrategy> file_manager_strategy = std::make_shared<FileManagerStrategy>(options);
  LogFileManager file_manager(file_manager_strategy);
  ASSERT_FALSE(file_manager_strategy->isDeleteStaleData());
}

int main(int argc, char** argv)
{
  Aws::Utils::Logging::InitializeAWSLogging(
      Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
          "RunUnitTests", Aws::Utils::Logging::LogLevel::Trace));
  ::testing::InitGoogleMock(&argc, argv);
  int exitCode = RUN_ALL_TESTS();
  Aws::Utils::Logging::ShutdownAWSLogging();
  return exitCode;
}

