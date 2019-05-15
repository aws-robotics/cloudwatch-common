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
#include <cloudwatch_logs_common/file_upload/file_manager_strategy.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::FileManagement;

class FileManagerStrategyTest : public ::testing::Test {
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
TEST_F(FileManagerStrategyTest, initializeWorks) {
  FileManagerStrategy file_manager_strategy;
  EXPECT_NO_THROW(file_manager_strategy.initialize());
}

TEST_F(FileManagerStrategyTest, discoverStoredFiles) {
  const std::string file_name = "/tmp/test_file.cwlog";
  std::ofstream write_stream(file_name);
  std::string test_data = "Some test log";
  write_stream << test_data << std::endl;
  write_stream.close();
  FileManagerStrategy file_manager_strategy;
  file_manager_strategy.initialize();
  const bool is_data_available = file_manager_strategy.isDataAvailable();
  EXPECT_EQ(is_data_available, true);
}
