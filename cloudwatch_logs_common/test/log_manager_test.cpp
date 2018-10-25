/*
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <aws/core/Aws.h>
#include <aws/logs/model/InputLogEvent.h>
#include <cloudwatch_logs_common/log_manager.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <gtest/gtest.h>

using namespace Aws::CloudWatchLogs;

class MockLogPublisher : public LogPublisher
{
public:
  Utils::SharedObject<std::list<Aws::CloudWatchLogs::Model::InputLogEvent> *> * last_shared_logs;
  ROSCloudWatchLogsErrors publish_logs_ret_val;
  uint32_t public_logs_call_count;

  MockLogPublisher() : LogPublisher("", "", nullptr) { Reset(); }

  void Reset()
  {
    this->last_shared_logs = nullptr;
    this->publish_logs_ret_val = Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED;
    this->public_logs_call_count = 0;
  }

  ROSCloudWatchLogsErrors PublishLogs(
    Utils::SharedObject<std::list<Aws::CloudWatchLogs::Model::InputLogEvent> *> * shared_logs)
    override
  {
    this->last_shared_logs = shared_logs;
    this->public_logs_call_count++;
    return this->publish_logs_ret_val;
  }
};

/**
 * @brief This test requires a fixture to init and shutdown the SDK or else it causes seg faults
 * when trying to construct a CloudWatch client. All tests must use the TEST_F function with this
 * fixture name.
 */
class TestLogManager : public ::testing::Test
{
protected:
  Aws::SDKOptions options_;
  std::shared_ptr<MockLogPublisher> mock_publisher_;
  std::shared_ptr<LogManager> manager_;

  void SetUp() override
  {
    Aws::InitAPI(options_);
    mock_publisher_ = std::make_shared<MockLogPublisher>();
    manager_ = std::make_shared<LogManager>(mock_publisher_);
  }

  void TearDown() override
  {
    manager_.reset();
    mock_publisher_.reset();
    Aws::ShutdownAPI(options_);
  }
};

TEST_F(TestLogManager, TestLogManager_Service_SharedObjectDataIsReady)
{
  manager_->RecordLog("TEST");

  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  auto shared_object = mock_publisher_->last_shared_logs;
  EXPECT_NE(nullptr, shared_object);
  EXPECT_TRUE(shared_object->isDataAvailable());
}

TEST_F(TestLogManager, TestLogManager_Service_DoesntCallPublishIfStillOngoing)
{
  manager_->RecordLog("TEST");

  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  EXPECT_EQ(1, mock_publisher_->public_logs_call_count);
}

TEST_F(TestLogManager, TestLogManager_Service_CallsPublishAgainAfterUnlock)
{
  manager_->RecordLog("TEST");
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  mock_publisher_->last_shared_logs->freeDataAndUnlock();
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  EXPECT_EQ(2, mock_publisher_->public_logs_call_count);
}

TEST_F(TestLogManager, TestLogManager_RecordLog_AddsLogsToList)
{
  manager_->RecordLog("TEST1");
  manager_->RecordLog("TEST2");

  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  auto shared_object = mock_publisher_->last_shared_logs;
  std::list<Aws::CloudWatchLogs::Model::InputLogEvent> * logs_list = nullptr;
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object->getDataAndLock(logs_list));

  auto it = logs_list->begin();
  EXPECT_EQ("TEST1", it->GetMessage());
  EXPECT_EQ("TEST2", (++it)->GetMessage());
}

TEST_F(TestLogManager, TestLogManager_Service_SubsequentCallsUseDifferentLists)
{
  manager_->RecordLog("TEST1");
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  std::list<Aws::CloudWatchLogs::Model::InputLogEvent> * logs_list = nullptr;
  auto shared_object = mock_publisher_->last_shared_logs;
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object->getDataAndLock(logs_list));
  auto log = logs_list->front();
  EXPECT_EQ("TEST1", log.GetMessage());
  mock_publisher_->last_shared_logs->freeDataAndUnlock();

  manager_->RecordLog("TEST2");
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  shared_object = mock_publisher_->last_shared_logs;
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object->getDataAndLock(logs_list));
  log = logs_list->front();
  EXPECT_EQ("TEST2", log.GetMessage());
  mock_publisher_->last_shared_logs->freeDataAndUnlock();

  manager_->RecordLog("TEST3");
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, manager_->Service());
  shared_object = mock_publisher_->last_shared_logs;
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object->getDataAndLock(logs_list));
  log = logs_list->front();
  EXPECT_EQ("TEST3", log.GetMessage());
  mock_publisher_->last_shared_logs->freeDataAndUnlock();
}
