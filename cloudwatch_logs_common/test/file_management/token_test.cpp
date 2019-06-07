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
#include <gmock/gmock.h>

#include <cloudwatch_logs_common/file_upload/file_manager_strategy.h>

using namespace Aws::FileManagement;

TEST(token_test, fail_unknown_token) {
  TokenStore token_store;
  EXPECT_THROW(token_store.fail(0), std::runtime_error);
}

TEST(token_test, fail_token_twice) {
  TokenStore token_store;
  auto test_file_name = "fake_file";
  auto position = 0;
  bool is_eof = false;
  auto token = token_store.createToken(test_file_name, position, is_eof);
  token_store.fail(token);
  EXPECT_THROW(token_store.fail(token), std::runtime_error);
}

TEST(token_test, resolve_then_fail_token) {
  TokenStore token_store;
  auto test_file_name = "fake_file";
  auto position = 0;
  bool is_eof = false;
  auto token = token_store.createToken(test_file_name, position, is_eof);
  token_store.resolve(token);
  EXPECT_THROW(token_store.fail(token), std::runtime_error);
}

TEST(token_test, fail_then_recover_token) {
  TokenStore token_store;
  auto test_file_name = "fake_file";
  auto position = 0;
  bool is_eof = false;
  auto token = token_store.createToken(test_file_name, position, is_eof);
  token_store.fail(token);
  EXPECT_TRUE(token_store.isTokenAvailable(test_file_name));
  auto popped_token = token_store.popAvailableToken(test_file_name);
  EXPECT_EQ(test_file_name, popped_token.file_path_);
  EXPECT_EQ(position, popped_token.position_);
  EXPECT_EQ(is_eof, popped_token.eof_);
}

TEST(token_test, resolve_unknown_token) {
  TokenStore token_store;
  EXPECT_THROW(token_store.resolve(0), std::runtime_error);
}

TEST(token_test, resolve_token) {
  TokenStore token_store;
  auto test_file_name = "fake_file";
  auto position = 0;
  bool is_eof = false;
  auto token = token_store.createToken(test_file_name, position, is_eof);
  auto resolved_token = token_store.resolve(token);
  EXPECT_EQ(test_file_name, resolved_token.file_path_);
  EXPECT_EQ(position, resolved_token.position_);
  EXPECT_EQ(is_eof, resolved_token.eof_);
}

TEST(token_test, resolve_token_twice) {
  TokenStore token_store;
  auto test_file_name = "fake_file";
  auto position = 0;
  bool is_eof = false;
  auto token = token_store.createToken(test_file_name, position, is_eof);
  token_store.resolve(token);
  EXPECT_THROW(token_store.resolve(token), std::runtime_error);
}

TEST(token_test, test_backup) {
  TokenStore token_store;
  FileTokenInfo test_token_1("fake_file", 0, false);
  FileTokenInfo test_token_2("fake_file", 10, true);

  auto token1 = token_store.createToken(test_token_1.file_path_, test_token_1.position_, test_token_1.eof_);
  auto token2 = token_store.createToken(test_token_2.file_path_, test_token_2.position_, test_token_2.eof_);
  auto backup = token_store.backup();

  EXPECT_THAT(backup, testing::ElementsAre(test_token_1));
}

TEST(token_test, test_backup_failed_file) {
  TokenStore token_store;
  FileTokenInfo test_token_1("fake_file", 0, false);
  FileTokenInfo test_token_2("fake_file", 10, true);

  auto token1 = token_store.createToken(test_token_1.file_path_, test_token_1.position_, test_token_1.eof_);
  auto token2 = token_store.createToken(test_token_2.file_path_, test_token_2.position_, test_token_2.eof_);
  token_store.fail(token1);
  auto backup = token_store.backup();

  EXPECT_THAT(backup, testing::ElementsAre(test_token_1));
}

TEST(token_test, test_backup_two_files) {
  TokenStore token_store;
  FileTokenInfo test_token_1("fake_file", 0, false);
  FileTokenInfo test_token_2("fake_file2", 10, true);

  auto token1 = token_store.createToken(test_token_1.file_path_, test_token_1.position_, test_token_1.eof_);
  auto token2 = token_store.createToken(test_token_2.file_path_, test_token_2.position_, test_token_2.eof_);
  auto backup = token_store.backup();

  EXPECT_THAT(backup, testing::UnorderedElementsAre(test_token_1, test_token_2));
}
