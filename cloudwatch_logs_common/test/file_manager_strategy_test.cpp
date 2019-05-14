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
#include <cloudwatch_logs_common/utils/file_manager_strategy.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;

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
  FileManagerStrategy file_manager_strategy;
  const std::string file_name = "test_file.cwlog";
  std::ofstream
  file_manager_strategy.discoverStoredFiles();
  const std::string stored_file = file_manager_strategy.getFileToRead();
  EXPECT_EQ(file_name, stored_file);

}
