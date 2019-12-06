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

#include <aws/core/Aws.h>
#include <aws/core/client/AWSClient.h>
#include <aws/core/NoResult.h>
#include <cloudwatch_logs_common/cloudwatch_logs_client_mock.h>
#include <cloudwatch_logs_common/definitions/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/cloudwatch_logs_facade.h>
#include <gtest/gtest.h>

using namespace Aws::CloudWatchLogs::Utils;

constexpr char kLogGroupName1[] = "TestGroup1";
constexpr char kLogGroupName2[] = "TestGroup2";
constexpr char kLogStreamName1[] = "TestStream1";
constexpr char kLogStreamName2[] = "TestStream2";

class TestCloudWatchFacade : public ::testing::Test
{
protected:
  std::list<Aws::CloudWatchLogs::Model::InputLogEvent> logs_list_;
  Aws::SDKOptions options_;
  std::shared_ptr<CloudWatchLogsFacade> facade_;
  std::shared_ptr<CloudWatchLogsClientMock> mock_client_;
  CloudWatchLogsClientMock* mock_client_p_{};

  void SetUp() override
  {
    // the tests require non-empty logs_list_
    logs_list_.emplace_back();
    logs_list_.emplace_back();

    Aws::InitAPI(options_);
    mock_client = std::make_shared<CloudWatchLogsClientMock>();
    mock_client_p = mock_client.get();
    facade_ = std::make_shared<CloudWatchLogsFacade>(mock_client);
  }

  void TearDown() override
  {
    logs_list_.clear();
    Aws::ShutdownAPI(options_);
  }
};

/*
 * SendLogsToCloudWatch Tests
 */
TEST_F(TestCloudWatchFacade, TestCWLogsFacade_SendLogsToCloudWatch_EmptyLogs)
{
    std::list<Aws::CloudWatchLogs::Model::InputLogEvent> empty_logs_list;
    Aws::String next_token;
    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_EMPTY_PARAMETER,
        facade_->SendLogsToCloudWatch(next_token, "", "", empty_logs_list));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_SendLogsToCloudWatch_FailedResponse)
{
    Aws::CloudWatchLogs::Model::PutLogEventsOutcome failed_outcome;
    EXPECT_CALL(*mock_client_p, PutLogEvents(testing::_))
        .WillOnce(testing::Return(failed_outcome));
    Aws::String next_token;

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_FAILED,
        facade_->SendLogsToCloudWatch(next_token, "", "", logs_list_));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_SendLogsToCloudWatch_SuccessResponse)
{
    Aws::CloudWatchLogs::Model::PutLogEventsResult success_result;
    Aws::CloudWatchLogs::Model::PutLogEventsOutcome success_outcome(success_result);
    EXPECT_CALL(*mock_client_p, PutLogEvents(testing::_))
        .WillOnce(testing::Return(success_outcome));
    Aws::String next_token;

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
        facade_->SendLogsToCloudWatch(next_token, "", "", logs_list_));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_SendLogsToCloudWatch_LongSuccessResponse)
{
    Aws::CloudWatchLogs::Model::PutLogEventsResult success_result;
    Aws::CloudWatchLogs::Model::PutLogEventsOutcome success_outcome(success_result);
    EXPECT_CALL(*mock_client_p, PutLogEvents(testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(success_outcome));

    Aws::String next_token;
    std::list<Aws::CloudWatchLogs::Model::InputLogEvent> logs_list;
    for (int i=0;i<200;i++) {
        logs_list.emplace_back();
    }

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
        facade_->SendLogsToCloudWatch(next_token, "", "", logs_list));
}


/*
 * CreateLogGroup Tests
 */

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CreateLogGroup_SuccessResponse)
{
    Aws::CloudWatchLogs::Model::CreateLogGroupOutcome* success_outcome =
        new Aws::CloudWatchLogs::Model::CreateLogGroupOutcome(Aws::NoResult());

    EXPECT_CALL(*mock_client_p, CreateLogGroup(testing::_))
        .WillOnce(testing::Return(*success_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
        facade_->CreateLogGroup(kLogGroupName1));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CreateLogGroup_FailedResponse)
{
    auto* failed_outcome =
        new Aws::CloudWatchLogs::Model::CreateLogGroupOutcome();

    EXPECT_CALL(*mock_client_p, CreateLogGroup(testing::_))
        .WillOnce(testing::Return(*failed_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_CREATE_LOG_GROUP_FAILED,
        facade_->CreateLogGroup(kLogGroupName1));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CreateLogGroup_AlreadyExists)
{
    Aws::Client::AWSError<Aws::CloudWatchLogs::CloudWatchLogsErrors> error
    (Aws::CloudWatchLogs::CloudWatchLogsErrors::RESOURCE_ALREADY_EXISTS, false);

    auto* failed_outcome =
        new Aws::CloudWatchLogs::Model::CreateLogGroupOutcome(error);

    EXPECT_CALL(*mock_client_p, CreateLogGroup(testing::_))
        .WillOnce(testing::Return(*failed_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_LOG_GROUP_ALREADY_EXISTS,
        facade_->CreateLogGroup(kLogGroupName1));
}

/*
 * CheckLogGroupExists Tests
 */

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CheckLogGroupExists_FailedResponse)
{
    Aws::CloudWatchLogs::Model::DescribeLogGroupsOutcome failed_outcome;
    EXPECT_CALL(*mock_client_p, DescribeLogGroups(testing::_))
        .WillOnce(testing::Return(failed_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_FAILED,
        facade_->CheckLogGroupExists(kLogGroupName1));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CheckLogGroupExists_LogGroupExists)
{
    Aws::CloudWatchLogs::Model::LogGroup test_group;
    test_group.SetLogGroupName(kLogGroupName1);
    Aws::CloudWatchLogs::Model::DescribeLogGroupsResult exists_result;
    exists_result.AddLogGroups(test_group);
    exists_result.SetNextToken("token");
    Aws::CloudWatchLogs::Model::DescribeLogGroupsOutcome exists_outcome(exists_result);
    EXPECT_CALL(*mock_client_p, DescribeLogGroups(testing::_))
        .WillOnce(testing::Return(exists_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
        facade_->CheckLogGroupExists(kLogGroupName1));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CheckLogGroupExists_LogGroupDoesntExist)
{   Aws::CloudWatchLogs::Model::LogGroup test_group;
    test_group.SetLogGroupName(kLogGroupName1);
    Aws::CloudWatchLogs::Model::DescribeLogGroupsResult doesnt_exist_result;
    doesnt_exist_result.AddLogGroups(test_group);
    doesnt_exist_result.SetNextToken("");
    Aws::CloudWatchLogs::Model::DescribeLogGroupsOutcome doesnt_exist_outcome(doesnt_exist_result);
    EXPECT_CALL(*mock_client_p, DescribeLogGroups(testing::_))
        .WillOnce(testing::Return(doesnt_exist_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_LOG_GROUP_NOT_FOUND,
        facade_->CheckLogGroupExists(kLogGroupName2));
}

/*
 * CreateLogStream Tests
 */

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CreateLogStream_SuccessResponse)
{
    Aws::CloudWatchLogs::Model::CreateLogStreamOutcome* success_outcome =
        new Aws::CloudWatchLogs::Model::CreateLogStreamOutcome(Aws::NoResult());

    EXPECT_CALL(*mock_client_p, CreateLogStream(testing::_))
        .WillOnce(testing::Return(*success_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
        facade_->CreateLogStream(kLogGroupName1, kLogStreamName1));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CreateLogStream_FailedResponse)
{
    auto* failed_outcome =
        new Aws::CloudWatchLogs::Model::CreateLogStreamOutcome();

    EXPECT_CALL(*mock_client_p, CreateLogStream(testing::_))
        .WillOnce(testing::Return(*failed_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_CREATE_LOG_STREAM_FAILED,
        facade_->CreateLogStream(kLogGroupName1, kLogStreamName1));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CreateLogStream_AlreadyExists)
{
    Aws::Client::AWSError<Aws::CloudWatchLogs::CloudWatchLogsErrors> error
    (Aws::CloudWatchLogs::CloudWatchLogsErrors::RESOURCE_ALREADY_EXISTS, false);

    auto* failed_outcome =
        new Aws::CloudWatchLogs::Model::CreateLogStreamOutcome(error);

    EXPECT_CALL(*mock_client_p, CreateLogStream(testing::_))
        .WillOnce(testing::Return(*failed_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_LOG_STREAM_ALREADY_EXISTS,
        facade_->CreateLogStream(kLogGroupName1, kLogStreamName1));
}

/*
 * CheckLogStreamExists Tests
 */

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CheckLogStreamExists_FailedResponse)
{
    Aws::CloudWatchLogs::Model::DescribeLogStreamsOutcome failed_outcome;
    Aws::CloudWatchLogs::Model::LogStream * log_stream_object = nullptr;
    EXPECT_CALL(*mock_client_p, DescribeLogStreams(testing::_))
        .WillOnce(testing::Return(failed_outcome));

    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_FAILED,
        facade_->CheckLogStreamExists(kLogGroupName1, kLogStreamName1, log_stream_object));
}

TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CheckLogStreamExists_LogStreamExists)
{
    Aws::CloudWatchLogs::Model::LogStream test_stream;
    test_stream.SetLogStreamName(kLogStreamName1);
    Aws::CloudWatchLogs::Model::DescribeLogStreamsResult exists_result;
    exists_result.AddLogStreams(test_stream);
    exists_result.SetNextToken("token");
    Aws::CloudWatchLogs::Model::DescribeLogStreamsOutcome exists_outcome(exists_result);
    EXPECT_CALL(*mock_client_p, DescribeLogStreams(testing::_))
        .WillOnce(testing::Return(exists_outcome));

    auto * log_stream_object = new Aws::CloudWatchLogs::Model::LogStream();
    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
        facade_->CheckLogStreamExists(kLogGroupName1, kLogStreamName1, log_stream_object));
}


TEST_F(TestCloudWatchFacade, TestCWLogsFacade_CheckLogStreamExists_LogStreamDoesntExist)
{   Aws::CloudWatchLogs::Model::LogStream test_stream;
    test_stream.SetLogStreamName(kLogStreamName2);
    Aws::CloudWatchLogs::Model::DescribeLogStreamsResult doesnt_exist_result;
    doesnt_exist_result.AddLogStreams(test_stream);
    Aws::CloudWatchLogs::Model::DescribeLogStreamsOutcome doesnt_exist_outcome(doesnt_exist_result);
    EXPECT_CALL(*mock_client_p, DescribeLogStreams(testing::_))
        .WillOnce(testing::Return(doesnt_exist_outcome));

    Aws::CloudWatchLogs::Model::LogStream * log_stream_object = nullptr;
    EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_LOG_STREAM_NOT_FOUND,
        facade_->CheckLogStreamExists(kLogGroupName1, kLogStreamName1, log_stream_object));
}
