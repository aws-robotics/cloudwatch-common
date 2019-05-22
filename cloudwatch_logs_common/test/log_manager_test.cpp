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
#include <cloudwatch_logs_common/dataflow/dataflow.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace Aws::CloudWatchLogs;

class MockLogPublisher : public ILogPublisher
{
public:
  MOCK_METHOD0(StartPublisherThread, Aws::CloudWatchLogs::ROSCloudWatchLogsErrors());
  MOCK_METHOD0(StopPublisherThread, Aws::CloudWatchLogs::ROSCloudWatchLogsErrors());
};

using LogTaskPtr = Aws::FileManagement::TaskPtr<Utils::LogType>;

class MockSink :
  public Aws::DataFlow::Sink<LogTaskPtr>
{
public:
  MOCK_METHOD1(enqueue, bool (LogTaskPtr& data));
  MOCK_METHOD2(tryEnqueue,
    bool (LogTaskPtr& data,
    const std::chrono::microseconds &duration));

  inline bool enqueue(LogTaskPtr&& value) override {
    return false;
  }
  inline bool tryEnqueue(
    LogTaskPtr&& value,
    const std::chrono::microseconds &duration) override
  {
    return enqueue(value);
  }
};

class TestLogManager : public ::testing::Test {
public:
public:
  void SetUp() override
  {
    mock_publisher = std::make_shared<MockLogPublisher>();
    log_manager = std::make_shared<LogManager>(mock_publisher);
  }

  void TearDown() override
  {

  }

protected:
  std::shared_ptr<MockLogPublisher> mock_publisher;
  std::shared_ptr<LogManager> log_manager;
};

TEST_F(TestLogManager, test_log_manager_not_configured)
{
  log_manager->RecordLog("TEST");
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_LOG_STREAM_NOT_CONFIGURED, log_manager->Service());
}

TEST_F(TestLogManager, test_log_manager_send_task)
{
  using namespace ::testing;
  log_manager->RecordLog("TEST");
  auto mock_sink = std::make_shared<MockSink>();
  EXPECT_CALL(*mock_sink, enqueue(_)).WillOnce(Invoke([](LogTaskPtr& data) -> bool {
    EXPECT_EQ(1, data->getBatchData().size());
    EXPECT_EQ("TEST", data->getBatchData().front().GetMessage());
    return true;
  }));
  *log_manager >> mock_sink;
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED, log_manager->Service());
}
