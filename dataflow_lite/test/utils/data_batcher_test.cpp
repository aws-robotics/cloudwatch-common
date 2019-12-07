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

#include <dataflow_lite/utils/data_batcher.h>
#include <memory>
#include <string>
#include <stdexcept>

/**
 * Simple extension to test basic functionality
 */
class TestBatcher : public DataBatcher<int> {
public:

  explicit TestBatcher(size_t max_allowable_batch_size = DataBatcher::kDefaultMaxBatchSize,
          size_t publish_trigger_size = DataBatcher::kDefaultTriggerSize) : DataBatcher(max_allowable_batch_size, publish_trigger_size) {
    pub_called = 0;
  }

  bool Start() override {
    return true;
  }

  bool Shutdown() override {
    this->ResetBatchedData();
    return true;
  }

  bool PublishBatchedData() override {
    std::lock_guard<std::recursive_mutex> lk(mtx_);
    pub_called++;
    this->ResetBatchedData();
    return true;
  }
  int pub_called;
};

/**
 * Test fixture
 */
class DataBatcherTest : public ::testing::Test {
public:
    void SetUp() override
    {
      test_batcher_ = std::make_shared<TestBatcher>();
      test_batcher_->Start();
    }

    void TearDown() override
    {
      test_batcher_->Shutdown();
    }

protected:
    std::shared_ptr<TestBatcher> test_batcher_;
};

TEST_F(DataBatcherTest, Sanity) {
  ASSERT_TRUE(true);
}

TEST_F(DataBatcherTest, Init) {

  EXPECT_EQ((size_t) TestBatcher::kDefaultTriggerSize,  test_batcher_->GetTriggerBatchSize());
  EXPECT_EQ((size_t) TestBatcher::kDefaultMaxBatchSize, test_batcher_->GetMaxAllowableBatchSize());
  EXPECT_EQ(0u, test_batcher_->GetCurrentBatchSize());
}

TEST_F(DataBatcherTest, TestMaxSizeClear) {

  size_t new_max = 10;
  test_batcher_->SetMaxAllowableBatchSize(new_max);
  EXPECT_EQ(new_max, test_batcher_->GetMaxAllowableBatchSize());

  for(size_t i=0; i<new_max; i++) {
    test_batcher_->BatchData(i);
    EXPECT_EQ(i+1, test_batcher_->GetCurrentBatchSize());
  }

  test_batcher_->BatchData(42);
  EXPECT_EQ(0u, test_batcher_->GetCurrentBatchSize());
  EXPECT_EQ(0, test_batcher_->pub_called);
}

TEST_F(DataBatcherTest, TestPublishTrigger) {

  size_t new_max = 10;
  test_batcher_->SetTriggerBatchSize(new_max);
  EXPECT_EQ(new_max, test_batcher_->GetTriggerBatchSize());

  for(size_t i=0; i<new_max-1; i++) {
  test_batcher_->BatchData(i);
  EXPECT_EQ(i+1, test_batcher_->GetCurrentBatchSize());
  }

  test_batcher_->BatchData(42);
  EXPECT_EQ(0u, test_batcher_->GetCurrentBatchSize());
  EXPECT_EQ(1, test_batcher_->pub_called);
}

TEST_F(DataBatcherTest, TestValidateArguments) {

  EXPECT_THROW(TestBatcher::ValidateConfigurableSizes(0, 0), std::invalid_argument);
  EXPECT_THROW(TestBatcher::ValidateConfigurableSizes(1, 0), std::invalid_argument);
  EXPECT_THROW(TestBatcher::ValidateConfigurableSizes(0, 1), std::invalid_argument);
  EXPECT_THROW(TestBatcher::ValidateConfigurableSizes(1, 1), std::invalid_argument);
  EXPECT_THROW(TestBatcher::ValidateConfigurableSizes(1, 2), std::invalid_argument);

  EXPECT_NO_THROW(TestBatcher::ValidateConfigurableSizes(2, 1));
}

TEST_F(DataBatcherTest, TestBatcherArguments) {

  size_t max = 10;
  EXPECT_THROW(test_batcher_->SetMaxAllowableBatchSize(0), std::invalid_argument);
  EXPECT_NO_THROW(test_batcher_->SetMaxAllowableBatchSize(max));
  EXPECT_EQ(max, test_batcher_->GetMaxAllowableBatchSize());

  size_t trigger = 5;
  EXPECT_THROW(test_batcher_->SetTriggerBatchSize(0), std::invalid_argument);
  EXPECT_THROW(test_batcher_->SetTriggerBatchSize(trigger + max), std::invalid_argument);
  EXPECT_NO_THROW(test_batcher_->SetTriggerBatchSize(trigger));
  EXPECT_EQ(trigger, test_batcher_->GetTriggerBatchSize());

  EXPECT_THROW(TestBatcher(100, 200), std::invalid_argument);
}
