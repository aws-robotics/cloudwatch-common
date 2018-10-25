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

#include <aws_common/sdk_utils/aws_error.h>
#include <gtest/gtest.h>

#include <cloudwatch_metrics_common/utils/shared_object.hpp>

using namespace Aws::CloudWatch::Metrics::Utils;

TEST(TestSharedObject, TestSharedObject_setDataAndMarkReady_marksDataAsReady)
{
  SharedObject<int> sharedObject;

  EXPECT_FALSE(sharedObject.IsDataAvailable());
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.SetDataAndMarkReady(1));
  EXPECT_TRUE(sharedObject.IsDataAvailable());
}

TEST(TestSharedObject, TestSharedObject_setDataAndMarkReady_failsIfAlreadyReady)
{
  SharedObject<int> sharedObject;

  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.SetDataAndMarkReady(1));
  EXPECT_TRUE(sharedObject.IsDataAvailable());
  EXPECT_EQ(Aws::AWS_ERR_ALREADY, sharedObject.SetDataAndMarkReady(2));
}

TEST(TestSharedObject, TestSharedObject_freeDataAndUnlock_marksDataNotReady)
{
  SharedObject<int> sharedObject;

  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.SetDataAndMarkReady(1));
  EXPECT_TRUE(sharedObject.IsDataAvailable());
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.FreeDataAndUnlock());
  EXPECT_FALSE(sharedObject.IsDataAvailable());
}

TEST(TestSharedObject, TestSharedObject_freeDataAndUnlock_failsIfDataAlreadyFree)
{
  SharedObject<int> sharedObject;

  EXPECT_FALSE(sharedObject.IsDataAvailable());
  EXPECT_EQ(Aws::AWS_ERR_ALREADY, sharedObject.FreeDataAndUnlock());
}

TEST(TestSharedObject, TestSharedObject_setDataAndMarkReady_setsTheData)
{
  SharedObject<int> sharedObject;
  int data = 0;

  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.SetDataAndMarkReady(2));
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.GetDataAndLock(data));
  EXPECT_EQ(2, data);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.FreeDataAndUnlock());
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.SetDataAndMarkReady(3));
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, sharedObject.GetDataAndLock(data));
  EXPECT_EQ(3, data);
}
