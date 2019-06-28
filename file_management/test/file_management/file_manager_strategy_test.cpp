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
#include <chrono>
#include <thread>
#include <experimental/filesystem>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <aws/logs/model/InputLogEvent.h>
#include <file_management/file_upload/file_manager_strategy.h>

using namespace Aws::FileManagement;

class FileManagerStrategyTest : public ::testing::Test {
public:
  void SetUp() override
  {
  }

  void TearDown() override
  {
    std::experimental::filesystem::path test_folder{"log_tests/"};
    std::experimental::filesystem::remove_all(test_folder);
  }

protected:
  std::string prefix = "test";
  std::string folder = "log_tests/";
  std::string extension = ".log";
  uint max_file_size = 1024 * 1024;
  uint storage_limit = max_file_size * 10;
  FileManagerStrategyOptions options{prefix, folder, extension, max_file_size, storage_limit};
};

TEST_F(FileManagerStrategyTest, restart_without_token) {
  const std::string data1 = "test_data_1";
  const std::string data2 = "test_data_2";
  {
    FileManagerStrategy file_manager_strategy(options);
    EXPECT_NO_THROW(file_manager_strategy.start());
    file_manager_strategy.write(data1);
    file_manager_strategy.write(data2);
  }
  {
    FileManagerStrategy file_manager_strategy(options);
    EXPECT_NO_THROW(file_manager_strategy.start());
    std::string result1, result2;
    file_manager_strategy.read(result1);
    file_manager_strategy.read(result2);
    EXPECT_EQ(data1, result1);
    EXPECT_EQ(data2, result2);
  }
}

// @todo (rddesmon) preserve file of tokens on restart
//TEST_F(FileManagerStrategyTest, restart_with_token) {
//  const std::string data1 = "test_data_1";
//  const std::string data2 = "test_data_2";
//  {
//    FileManagerStrategy file_manager_strategy(options);
//    EXPECT_NO_THROW(file_manager_strategy.start());
//    file_manager_strategy.write(data1);
//    file_manager_strategy.write(data2);
//    std::string result1;
//    DataToken token1 = file_manager_strategy.read(result1);
//    EXPECT_EQ(data1, result1);
//  }
//  {
//    FileManagerStrategy file_manager_strategy(options);
//    EXPECT_NO_THROW(file_manager_strategy.start());
//    std::string result2;
//    DataToken token2 = file_manager_strategy.read(result2);
//    EXPECT_EQ(data2, result2);
//  }
//}

TEST_F(FileManagerStrategyTest, fail_token_restart_from_last_location) {
  const std::string data1 = "test_data_1";
  const std::string data2 = "test_data_2";
  FileManagerStrategy file_manager_strategy(options);
  EXPECT_NO_THROW(file_manager_strategy.start());
  file_manager_strategy.write(data1);
  file_manager_strategy.write(data2);
  std::string result1;
  DataToken token1 = file_manager_strategy.read(result1);
  EXPECT_EQ(data1, result1);
  file_manager_strategy.resolve(token1, true);
  std::string result2, result3;
  DataToken token2 = file_manager_strategy.read(result2);
  EXPECT_EQ(data2, result2);

  file_manager_strategy.resolve(token2, false);
  // Token was failed, should be re-read.
  file_manager_strategy.read(result2);
  EXPECT_EQ(data2, result2);
}

/**
 * Test that the upload complete with CW Failure goes to a file.
 */
TEST_F(FileManagerStrategyTest, start_success) {
  FileManagerStrategy file_manager_strategy(options);
  EXPECT_NO_THROW(file_manager_strategy.start());
}

TEST_F(FileManagerStrategyTest, discover_stored_files) {
  const std::string test_data = "test_data";
  {
    FileManagerStrategy file_manager_strategy(options);
    EXPECT_NO_THROW(file_manager_strategy.start());
    file_manager_strategy.write(test_data);
  }
  {
    FileManagerStrategy file_manager_strategy(options);
    EXPECT_NO_THROW(file_manager_strategy.start());
    EXPECT_TRUE(file_manager_strategy.isDataAvailable());
    std::string result;
    DataToken token = file_manager_strategy.read(result);
    EXPECT_EQ(test_data, result);
    file_manager_strategy.resolve(token, true);
  }
}

TEST_F(FileManagerStrategyTest, rotate_large_files) {
  namespace fs = std::experimental::filesystem;
  const uint max_file_size_in_kb = 10;
  options.maximum_file_size_in_kb = max_file_size_in_kb;
  {
    FileManagerStrategy file_manager_strategy(options);
    file_manager_strategy.start();
    std::string data1 = "This is some long data that is longer than 10 bytes";
    file_manager_strategy.write(data1);
    long file_count = std::distance(fs::directory_iterator(folder), fs::directory_iterator{});
    EXPECT_EQ(1, file_count);
    std::string data2 = "This is some additional data that is also longer than 10 bytes";
    file_manager_strategy.write(data2);
    file_count = std::distance(fs::directory_iterator(folder), fs::directory_iterator{});
    EXPECT_EQ(2, file_count);
  }
}

TEST_F(FileManagerStrategyTest, resolve_token_deletes_file) {
  const std::string test_data = "test_data";
  {
    FileManagerStrategy file_manager_strategy(options);
    file_manager_strategy.start();
    EXPECT_FALSE(file_manager_strategy.isDataAvailable());
    file_manager_strategy.write(test_data);
    EXPECT_TRUE(file_manager_strategy.isDataAvailable());
    std::string result;
    DataToken token = file_manager_strategy.read(result);
    file_manager_strategy.resolve(token, true);
  }
  {
    FileManagerStrategy file_manager_strategy(options);
    file_manager_strategy.start();
    EXPECT_FALSE(file_manager_strategy.isDataAvailable());
  }
}

TEST_F(FileManagerStrategyTest, on_storage_limit_delete_oldest_file) {
  namespace fs = std::experimental::filesystem;
  const uint max_file_size_in_kb = 50;
  const uint storage_limit = 150;
  options.maximum_file_size_in_kb = max_file_size_in_kb;
  options.storage_limit_in_kb = storage_limit;
  {
    FileManagerStrategy file_manager_strategy(options);
    file_manager_strategy.start();
    const std::string string_25_bytes = "This is 25 bytes of data.";
    file_manager_strategy.write(string_25_bytes);
    long file_count = std::distance(fs::directory_iterator(folder), fs::directory_iterator{});
    EXPECT_EQ(1, file_count);
//    for (const auto &entry : fs::directory_iterator(folder)) {
//      const fs::path &path = entry.path();
//    }

    for (int i = 0; i < 5; i++) {
      file_manager_strategy.write(string_25_bytes);
    }

    file_count = std::distance(fs::directory_iterator(folder), fs::directory_iterator{});
    EXPECT_EQ(3, file_count);

    std::vector<std::string> file_paths;
    for (const auto &entry : fs::directory_iterator(folder)) {
      const fs::path &path = entry.path();
      file_paths.push_back(path);
    }

    std::sort(file_paths.begin(), file_paths.end());
    const std::string file_to_be_deleted = file_paths[0];

    file_manager_strategy.write(string_25_bytes);
    file_count = std::distance(fs::directory_iterator(folder), fs::directory_iterator{});
    EXPECT_EQ(3, file_count);

    for (const auto &entry : fs::directory_iterator(folder)) {
      const std::string file_path = entry.path();
      EXPECT_TRUE(file_path != file_to_be_deleted);
    }
  }
}
