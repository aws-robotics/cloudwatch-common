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
const long TWO_WEEK_IN_SEC = 1209600000;

class TestStrategy : public DataManagerStrategy {
public:
  TestStrategy(const FileManagerStrategyOptions &options) {
    options_ = options;
  }

  bool isDataAvailable(){
    return !logs.empty() && !data_tokens.empty();
  }

  bool discardOldLogs(){
    return options_.discard_2_week_logs;
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
  FileManagerStrategyOptions options{"test", "log_tests/", ".log", 1024*1024, 1024*1024, true};
  std::vector<std::vector<long>> expectedTimeStamps;
};

/**
 * Test that the upload complete with CW Failure goes to a file.
 */
TEST_F(LogBatchTest, test_readBatch_1_batch) {
  timestamps = {1, 3, 0, ONE_DAY_IN_SEC-1, 4, 2};
  expectedTimeStamps = {{ONE_DAY_IN_SEC-1, 4, 3, 2, 1, 0}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
TEST_F(LogBatchTest, test_readBatch_2_batches) {
  timestamps = {ONE_DAY_IN_SEC+1, 2, ONE_DAY_IN_SEC+2, 1, 0, ONE_DAY_IN_SEC};
  expectedTimeStamps = {{ONE_DAY_IN_SEC+2, ONE_DAY_IN_SEC+1, ONE_DAY_IN_SEC}, {2, 1, 0}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
TEST_F(LogBatchTest, test_readBatch_3_batches) {
  timestamps = {1, ONE_DAY_IN_SEC+5, 4, 2, 0, ONE_DAY_IN_SEC*2+10};
  expectedTimeStamps = {{ONE_DAY_IN_SEC*2+10}, {ONE_DAY_IN_SEC+5},{4, 2, 1, 0}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
TEST_F(LogBatchTest, test_2_week_discard_1_of_6) {
  timestamps = {15, ONE_DAY_IN_SEC, 0, TWO_WEEK_IN_SEC+5, TWO_WEEK_IN_SEC+1, 10};
  expectedTimeStamps = {{TWO_WEEK_IN_SEC+5, TWO_WEEK_IN_SEC+1}, {ONE_DAY_IN_SEC, 15, 10}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
TEST_F(LogBatchTest, test_2_week_discard_4_of_6) {
  timestamps = {1, ONE_DAY_IN_SEC, 4, TWO_WEEK_IN_SEC+5, 0, 3};
  expectedTimeStamps = {{TWO_WEEK_IN_SEC+5}, {ONE_DAY_IN_SEC}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
TEST_F(LogBatchTest, test_2_week_discard_5_of_6) {
  timestamps = {1, ONE_DAY_IN_SEC, 4, TWO_WEEK_IN_SEC+ONE_DAY_IN_SEC+5, 0, 3};
  expectedTimeStamps = {{TWO_WEEK_IN_SEC+ONE_DAY_IN_SEC+5}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}
TEST_F(LogBatchTest, test_2_week_no_discard) {
  test_strategy->options_.discard_2_week_logs = false;
  timestamps = {1, ONE_DAY_IN_SEC, 4, TWO_WEEK_IN_SEC+5, 0, 3};
  expectedTimeStamps = {{TWO_WEEK_IN_SEC+5},{ONE_DAY_IN_SEC, 4, 3, 1}, {0}};
  createLogs(timestamps);
  file_manager->write(log_data);
  readLogs();
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

