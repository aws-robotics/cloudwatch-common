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

const int INITIAL_VALUE = 0;

class ObservableObjectTest : public ::testing::Test {
public:
    void SetUp() override
    {
      testIntObservable = std::make_shared<ObservableObject<int>>(INITIAL_VALUE);
    }

    void TearDown() override
    {}

protected:
  std::shared_ptr<ObservableObject<int>> testIntObservable;
};

TEST_F(ObservableObjectTest, Sanity) {
  ASSERT_TRUE(true);
}

TEST_F(ObservableObjectTest, TestInit) {
  EXPECT_EQ(INITIAL_VALUE, testIntObservable->getValue());
}

TEST_F(ObservableObjectTest, TestSet) {
  EXPECT_EQ(INITIAL_VALUE, testIntObservable->getValue());
  int first_set = 42;
  testIntObservable->setValue(first_set);
  EXPECT_EQ(first_set, testIntObservable->getValue());
}

TEST_F(ObservableObjectTest, TestListener) {
  EXPECT_EQ(INITIAL_VALUE, testIntObservable->getValue());
  int first_set = 42;
  testIntObservable->setValue(first_set);
  EXPECT_EQ(first_set, testIntObservable->getValue());

  // register listener
  int listened_value;
  std::function<void(const int&)> lambda = [&listened_value](const int &currentValue){listened_value = currentValue;};
  testIntObservable->addListener(lambda);
  EXPECT_EQ(1, testIntObservable->getNumberOfListeners());

  int second_set = 242;
  testIntObservable->setValue(second_set); // currently synchronous

  EXPECT_EQ(second_set, listened_value);
  EXPECT_EQ(second_set, testIntObservable->getValue());

  // test clear

  testIntObservable->clearListeners();
  EXPECT_EQ(0, testIntObservable->getNumberOfListeners());

  int third_set = 1999;
  testIntObservable->setValue(third_set); // currently synchronous

  EXPECT_EQ(second_set, listened_value);
  EXPECT_EQ(third_set, testIntObservable->getValue());
}

TEST_F(ObservableObjectTest, TestFaultyListener) {
  EXPECT_EQ(INITIAL_VALUE, testIntObservable->getValue());
  int first_set = 42;
  testIntObservable->setValue(first_set);
  EXPECT_EQ(first_set, testIntObservable->getValue());

  enum Value {
      VALID = 1,
      INVALID = 999999999
  };

  // register listener
  int listened_value;
  auto bad_lambda = [&listened_value](const int &currentValue){
    switch(currentValue) {
      case VALID:
        listened_value = currentValue;
        break;
      default:
        throw "imsorryjon";
    }
  };

  int good_listened_value;
  auto lambda = [&good_listened_value](const int &currentValue) {good_listened_value = currentValue;};

  testIntObservable->addListener(bad_lambda);
  EXPECT_EQ(1, testIntObservable->getNumberOfListeners());

  testIntObservable->addListener(lambda);
  EXPECT_EQ(2, testIntObservable->getNumberOfListeners());

  testIntObservable->setValue(1); // currently synchronous

  EXPECT_EQ(VALID, listened_value);
  EXPECT_EQ(VALID, good_listened_value);
  EXPECT_EQ(VALID, testIntObservable->getValue());

  testIntObservable->setValue(INVALID); // currently synchronous

  EXPECT_EQ(1, testIntObservable->getNumberOfListeners());
  EXPECT_EQ(INVALID, testIntObservable->getValue());
  EXPECT_EQ(INVALID, good_listened_value);
  EXPECT_EQ(VALID , listened_value);
}

