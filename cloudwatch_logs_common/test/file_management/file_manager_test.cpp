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
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_manager_strategy.h>
#include <cloudwatch_logs_common/utils/log_file_manager.h>


using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;

class FileManagerTest : public ::testing::Test {
public:
  void SetUp() override
  {
    // extra setup goes here.
  }

  void TearDown() override
  {
    std::experimental::filesystem::path("log_tests").remove_filename();
  }

protected:
  FileManagerStrategyOptions options{"test", "log_tests/", ".log", 1024*1024};
};

/**
 * Test that the upload complete with CW Failure goes to a file.
 */
TEST_F(FileManagerTest, file_manager_write_on_fail) {
  std::shared_ptr<FileManagerStrategy> file_manager_strategy = std::make_shared<FileManagerStrategy>(options);
  LogFileManager file_manager(file_manager_strategy);
  LogType log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;
  input_event.SetTimestamp(0);
  input_event.SetMessage("Hello my name is foo");
  log_data.push_back(input_event);
  file_manager.uploadCompleteStatus(UploadStatus::FAIL, log_data);
  std::string line;
  file_manager_strategy->read(line);
  EXPECT_EQ(line, "{\"timestamp\":0,\"message\":\"Hello my name is foo\"}");
}

/**
 * Test that the upload complete with CW success does not go to a file.
 */
TEST_F(FileManagerTest, file_manager_no_write_on_success) {
  std::shared_ptr<FileManagerStrategy> file_manager_strategy = std::make_shared<FileManagerStrategy>(options);
  LogFileManager file_manager(file_manager_strategy);
  LogType log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;
  input_event.SetTimestamp(0);
  input_event.SetMessage("Hello my name is bar");
  log_data.push_back(input_event);
  file_manager.uploadCompleteStatus(Aws::FileManagement::SUCCESS, log_data);
  std::string line;
  EXPECT_ANY_THROW(file_manager_strategy->read(line));
}

int main(int argc, char** argv) {
  // The following line must be executed to initialize Google Mock
  // (and Google Test) before running the tests.
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
