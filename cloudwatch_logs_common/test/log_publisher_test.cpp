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

constexpr int WAIT_TIME =
  2000;  // the amount of time (ms) to wait for publisher thread to do its work

class MockCloudWatchFacade : public Aws::CloudWatchLogs::Utils::CloudWatchFacade
{
public:
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors send_logs_ret_val;
  uint32_t send_logs_call_count;
  std::string last_log_group;
  std::string last_log_stream;
  std::list<Aws::CloudWatchLogs::Model::InputLogEvent> * last_logs;

  MockCloudWatchFacade() { Reset(); }

  void Reset()
  {
    this->last_log_group = "";
    this->last_log_stream = "";
    this->last_logs = nullptr;
    this->send_logs_ret_val = Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED;
    this->send_logs_call_count = 0;
  }

  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CreateLogGroup(
    const std::string & log_group) override
  {
    return Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED;
  }

  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CreateLogStream(
    const std::string & log_group, const std::string & log_stream) override
  {
    return Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED;
  }

  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors SendLogsToCloudWatch(
    Aws::String & next_token, const std::string & last_log_group,
    const std::string & last_log_stream,
    std::list<Aws::CloudWatchLogs::Model::InputLogEvent> * logs) override
  {
    this->last_log_group = last_log_group;
    this->last_log_stream = last_log_stream;
    this->last_logs = logs;
    this->send_logs_call_count++;
    return send_logs_ret_val;
  }
};

/**
 * @brief This test requires a fixture to init and shutdown the SDK or else it causes seg faults
 * when trying to construct a CloudWatch client. All tests must use the TEST_F function with this
 * fixture name.
 */
class TestLogPublisher : public ::testing::Test
{
protected:
  Utils::SharedObject<std::list<Aws::CloudWatchLogs::Model::InputLogEvent> *> shared_logs_obj_;
  std::list<Aws::CloudWatchLogs::Model::InputLogEvent> logs_list_;
  Aws::SDKOptions options_;
  std::shared_ptr<MockCloudWatchFacade> mock_cw_;
  std::shared_ptr<LogPublisher> publisher_;
  bool publisher_started_ = false;

  void StartPublisher()
  {
    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
              publisher_->StartPublisherThread());
    publisher_started_ = true;
  }

  void SetUp() override
  {
    // the tests require non-empty logs_list_
    logs_list_.emplace_back();
    logs_list_.emplace_back();

    Aws::InitAPI(options_);
    mock_cw_ = std::make_shared<MockCloudWatchFacade>();
    std::shared_ptr<Aws::CloudWatchLogs::Utils::CloudWatchFacade> cw = mock_cw_;
    publisher_ = std::make_shared<LogPublisher>("test_log_group", "test_log_stream", cw);
  }

  void TearDown() override
  {
    if (publisher_started_) {
      publisher_->StopPublisherThread();
    }
    Aws::ShutdownAPI(options_);
  }
};

TEST_F(TestLogPublisher, TestLogPublisher_PublishLogs_ReturnsErrorWhenLogsNull)
{
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_NULL_PARAMETER,
            publisher_->PublishLogs(nullptr));
}

TEST_F(TestLogPublisher, TestLogPublisher_PublishLogs_ReturnsErrorWhenSharedObjectUnlocked)
{
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_DATA_LOCKED,
            publisher_->PublishLogs(&shared_logs_obj_));
}

TEST_F(TestLogPublisher, TestLogPublisher_PublishLogs_ReturnsErrorWhenThreadNotStarted)
{
  shared_logs_obj_.setDataAndMarkReady(&logs_list_);

  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_PUBLISHER_THREAD_NOT_INITIALIZED,
            publisher_->PublishLogs(&shared_logs_obj_));
}

TEST_F(TestLogPublisher, TestLogPublisher_PublishLogs_ReturnsSuccessWhenListIngested)
{
  shared_logs_obj_.setDataAndMarkReady(&logs_list_);
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            publisher_->StartPublisherThread());
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            publisher_->PublishLogs(&shared_logs_obj_));
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            publisher_->StopPublisherThread());
}

TEST_F(TestLogPublisher, TestLogPublisher_BackgroundThread_CallsSendLogsToCW)
{
  StartPublisher();
  shared_logs_obj_.setDataAndMarkReady(&logs_list_);
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            publisher_->PublishLogs(&shared_logs_obj_));
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
  if (mock_cw_->send_logs_call_count < 1) {
    ADD_FAILURE() << "SendLogsToCloudWatch() was not called after " << WAIT_TIME
                  << " ms, possible machine performance issue";
  }
  EXPECT_EQ(1, mock_cw_->send_logs_call_count);
  EXPECT_EQ(&logs_list_, mock_cw_->last_logs);
}

TEST_F(TestLogPublisher, TestLogPublisher_BackgroundThread_UnlocksSharedObjectAfterPublish)
{
  StartPublisher();
  shared_logs_obj_.setDataAndMarkReady(&logs_list_);
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            publisher_->PublishLogs(&shared_logs_obj_));
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
  if (mock_cw_->send_logs_call_count < 1) {
    ADD_FAILURE() << "SendLogsToCloudWatch() was not called after " << WAIT_TIME
                  << " ms, possible machine performance issue";
  }
  EXPECT_EQ(1, mock_cw_->send_logs_call_count);
  EXPECT_FALSE(shared_logs_obj_.isDataAvailable());
}

TEST_F(TestLogPublisher, TestLogPublisher_BackgroundThread_MultiplePublishes)
{
  StartPublisher();

  shared_logs_obj_.setDataAndMarkReady(&logs_list_);
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            publisher_->PublishLogs(&shared_logs_obj_));
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
  if (mock_cw_->send_logs_call_count < 1) {
    ADD_FAILURE() << "SendLogsToCloudWatch() was not called after " << WAIT_TIME
                  << " ms, possible machine performance issue";
  }
  EXPECT_EQ(1, mock_cw_->send_logs_call_count);

  shared_logs_obj_.setDataAndMarkReady(&logs_list_);
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            publisher_->PublishLogs(&shared_logs_obj_));
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
  if (mock_cw_->send_logs_call_count < 2) {
    ADD_FAILURE() << "SendLogsToCloudWatch() was not called after " << WAIT_TIME
                  << " ms, possible machine performance issue";
  }
  EXPECT_EQ(2, mock_cw_->send_logs_call_count);
}
