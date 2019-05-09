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

#include <list>

#include <iostream>
#include <fstream>
#include <cstdio>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <aws/logs/model/InputLogEvent.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/file_manager.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;

class MockFileManagerStrategy : public FileManagerStrategy {
public:
  MOCK_CONST_METHOD0(getFileToWrite, std::string(void));
};

class FileManagerTest : public ::testing::Test {
public:
  void SetUp() override
  {
    // extra setup goes here.
  }

  void TearDown() override
  {
    remove(test_file_name.c_str());
  }

protected:
  std::string test_file_name = "test_file.txt";
};

/**
 * Test that the upload complete with CW Failure goes to a file.
 */
TEST_F(FileManagerTest, file_manager_write_on_fail) {
  std::shared_ptr<MockFileManagerStrategy> mock_file_manager_strategy = std::make_shared<MockFileManagerStrategy>();
  EXPECT_CALL(*mock_file_manager_strategy, getFileToWrite())
    .Times(1)
    .WillOnce(testing::Return(test_file_name));
  LogFileManager file_manager(mock_file_manager_strategy);
  LogType log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;
  input_event.SetTimestamp(0);
  input_event.SetMessage("Hello my name is foo");
  log_data.push_back(input_event);
  file_manager.uploadCompleteStatus(ROSCloudWatchLogsErrors::CW_LOGS_FAILED, log_data);
  std::ifstream log_file(test_file_name);
  std::string line;
  std::getline(log_file, line);
  log_file.close();
  EXPECT_EQ(line, "{\"timestamp\":0,\"message\":\"Hello my name is foo\"}");
}

/**
 * Test that the upload complete with CW success does not go to a file.
 */
TEST_F(FileManagerTest, file_manager_no_write_on_success) {
  std::shared_ptr<MockFileManagerStrategy> mock_file_manager_strategy = std::make_shared<MockFileManagerStrategy>();
  LogFileManager file_manager(mock_file_manager_strategy);
  LogType log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;
  input_event.SetTimestamp(0);
  input_event.SetMessage("Hello my name is foo");
  log_data.push_back(input_event);
  file_manager.uploadCompleteStatus(ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, log_data);
  std::ifstream log_file(test_file_name);
  EXPECT_FALSE(log_file);
}

int main(int argc, char** argv) {
  // The following line must be executed to initialize Google Mock
  // (and Google Test) before running the tests.
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
