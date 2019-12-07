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

#include <dataflow_lite/utils/publisher.h>
#include <thread>
#include <string>

/**
 * Simple test class to test the internals of the Publisher interface
 */
class SimpleTestPublisher : public Publisher<std::string>
{
public:

  SimpleTestPublisher() {should_succeed_ = true;}
  ~SimpleTestPublisher() override = default;

  Aws::DataFlow::UploadStatus PublishData(std::string &data) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    last_data = data;
    return  should_succeed_ ? Aws::DataFlow::UploadStatus::SUCCESS : Aws::DataFlow::UploadStatus::FAIL;
  }

  void SetShouldSucceed(bool nv) {
    should_succeed_ = nv;
  }

  std::string last_data;
  bool should_succeed_;
};

/**
 * Test fixture for the PublisherTest class.
 */
class PublisherTest : public ::testing::Test {
public:

    void SetUp() override {

      test_publisher_ = std::make_shared<SimpleTestPublisher>();

      EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
      EXPECT_EQ(ServiceState::CREATED, test_publisher_->GetState());
      EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
      EXPECT_EQ(0, test_publisher_->GetPublishSuccesses());
      EXPECT_EQ(0.0f, test_publisher_->GetPublishSuccessPercentage());
      EXPECT_EQ(std::chrono::milliseconds(0), test_publisher_->GetLastPublishDuration());
    }

    void TearDown() override {
      test_publisher_.reset();
    }
protected:
    std::shared_ptr<SimpleTestPublisher> test_publisher_;
};

/**
 * Test that there are no issues with the fixture
 */
TEST_F(PublisherTest, Sanity) {
  ASSERT_TRUE(true);
}

/**
 * Test publish failure if the publisher has not been started
 */
TEST_F(PublisherTest, TestPublishFailNotStarted) {

  std::string data("They're taking the hobbits to Isengard!");
  auto status = test_publisher_->AttemptPublish(data);

  EXPECT_EQ(Aws::DataFlow::UploadStatus::FAIL, status);
  EXPECT_FALSE(test_publisher_->CanPublish());
  EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(std::chrono::milliseconds(0), test_publisher_->GetLastPublishDuration());
  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
}

/**
 * Test that the publisher succeeds after starting
 */
TEST_F(PublisherTest, TestPublishSuccessWhenStarted) {

  std::string data("They're taking the hobbits to Isengard!");

  bool b = test_publisher_->Start();
  EXPECT_TRUE(b);
  EXPECT_TRUE(test_publisher_->CanPublish());

  auto status = test_publisher_->AttemptPublish(data);
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, status);
  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(100.0f, test_publisher_->GetPublishSuccessPercentage());
  EXPECT_LT(std::chrono::milliseconds(0), test_publisher_->GetLastPublishDuration());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher_->GetPublisherState());
}

/**
 * Test a publish failure
 */
TEST_F(PublisherTest, TestPublishFailure) {

  std::string data("They're taking the hobbits to Isengard!");

  bool b = test_publisher_->Start();
  EXPECT_TRUE(b);
  EXPECT_TRUE(test_publisher_->CanPublish());

  auto status = test_publisher_->AttemptPublish(data);

  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, status);
  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(100.0f, test_publisher_->GetPublishSuccessPercentage());
  EXPECT_LT(std::chrono::milliseconds(0), test_publisher_->GetLastPublishDuration());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher_->GetPublisherState());

  test_publisher_->SetShouldSucceed(false);
  status = test_publisher_->AttemptPublish(data);

  EXPECT_EQ(Aws::DataFlow::UploadStatus::FAIL, status);
  EXPECT_EQ(2, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(50.0f, test_publisher_->GetPublishSuccessPercentage());
  EXPECT_LT(std::chrono::milliseconds(0), test_publisher_->GetLastPublishDuration());
  EXPECT_EQ(PublisherState::NOT_CONNECTED, test_publisher_->GetPublisherState());
}

/**
 * Test that the publisher shutdown correctly and does not publish data
 * after shutting down
 */
TEST_F(PublisherTest, TestPublisherShutdown) {

  std::string data("They're taking the hobbits to Isengard!");

  bool b = test_publisher_->Start();
  EXPECT_TRUE(b);
  EXPECT_TRUE(test_publisher_->CanPublish());
  EXPECT_EQ(ServiceState::STARTED, test_publisher_->GetState());

  std::thread pub_thread(&SimpleTestPublisher::AttemptPublish, test_publisher_, std::ref(data));

  // let the AttemptPublish thread start and hold the lock
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  b = test_publisher_->Shutdown();

  EXPECT_TRUE(b);
  EXPECT_EQ(data, test_publisher_->last_data);
  EXPECT_EQ(ServiceState::SHUTDOWN, test_publisher_->GetState());
  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());

  // try to publish again but expect fast fail
  auto status = test_publisher_->AttemptPublish(data);

  EXPECT_EQ(Aws::DataFlow::UploadStatus::FAIL, status);
  EXPECT_LT(std::chrono::milliseconds(0), test_publisher_->GetLastPublishDuration());
  EXPECT_FALSE(test_publisher_->CanPublish());

  pub_thread.join();
}
