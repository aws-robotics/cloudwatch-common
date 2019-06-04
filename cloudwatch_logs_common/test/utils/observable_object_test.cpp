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

#include <cloudwatch_logs_common/utils/observable_object.h>

class ObservableObjectTest : public ::testing::Test {
public:
    void SetUp() override
    {
      testIntObservable = std::make_shared<ObservableObject<int>>(0);
    }

    void TearDown() override
    {}

protected:
  std::shared_ptr<ObservableObject<int>> testIntObservable;
};

TEST_F(ObservableObjectTest, TestInit) {
  EXPECT_EQ(0, testIntObservable->getValue());
}

TEST_F(ObservableObjectTest, TestSet) {
  EXPECT_EQ(0, testIntObservable->getValue());
  testIntObservable->setValue(42);
  EXPECT_EQ(42, testIntObservable->getValue());
}

