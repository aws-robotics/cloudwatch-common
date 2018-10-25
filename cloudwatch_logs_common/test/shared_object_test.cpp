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

#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/shared_object.h>
#include <gtest/gtest.h>

using namespace Aws::CloudWatchLogs::Utils;

TEST(TestSharedObject, TestSharedObject_setDataAndMarkReady_marksDataAsReady)
{
  SharedObject<int> shared_object;

  EXPECT_FALSE(shared_object.isDataAvailable());
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.setDataAndMarkReady(1));
  EXPECT_TRUE(shared_object.isDataAvailable());
}

TEST(TestSharedObject, TestSharedObject_setDataAndMarkReady_failsIfAlreadyReady)
{
  SharedObject<int> shared_object;

  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.setDataAndMarkReady(1));
  EXPECT_TRUE(shared_object.isDataAvailable());
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_DATA_LOCKED,
            shared_object.setDataAndMarkReady(2));
}

TEST(TestSharedObject, TestSharedObject_freeDataAndUnlock_marksDataNotReady)
{
  SharedObject<int> shared_object;

  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.setDataAndMarkReady(1));
  EXPECT_TRUE(shared_object.isDataAvailable());
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.freeDataAndUnlock());
  EXPECT_FALSE(shared_object.isDataAvailable());
}

TEST(TestSharedObject, TestSharedObject_freeDataAndUnlock_failsIfDataAlreadyFree)
{
  SharedObject<int> shared_object;

  EXPECT_FALSE(shared_object.isDataAvailable());
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_DATA_LOCKED,
            shared_object.freeDataAndUnlock());
}

TEST(TestSharedObject, TestSharedObject_setDataAndMarkReady_setsTheData)
{
  SharedObject<int> shared_object;
  int data = 0;

  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.setDataAndMarkReady(2));
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.getDataAndLock(data));
  EXPECT_EQ(2, data);
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.freeDataAndUnlock());
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.setDataAndMarkReady(3));
  EXPECT_EQ(Aws::CloudWatchLogs::ROSCloudWatchLogsErrors::CW_LOGS_SUCCEEDED,
            shared_object.getDataAndLock(data));
  EXPECT_EQ(3, data);
}