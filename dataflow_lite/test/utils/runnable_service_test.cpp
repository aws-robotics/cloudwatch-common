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

#include <chrono>
#include <thread>
#include <dataflow_lite/utils/service.h>

/**
 * Simple extension of the RunnableService used for testing
 */
class HardWorker : public RunnableService
{
public:
    HardWorker() {
      has_worked_ = false;
    };

    ~HardWorker() override = default;

    bool Shutdown() override {
      std::unique_lock <std::mutex> lck(this->test_mtx_);
      this->test_cv_.notify_all(); // stop blocking in the work thread
      return RunnableService::Shutdown();
    }

    void Work() override {
      this->has_worked_ = true;

      // manually wait for shutdown
      std::unique_lock <std::mutex> lck(this->test_mtx_);
      this->test_cv_.wait(lck);
    }

    bool GetHasWorked() {
      return this->has_worked_;
    }
private:
    bool has_worked_;
    std::condition_variable test_cv_;
    mutable std::mutex test_mtx_;
};

/**
 * Test fixture used to execture the HardWorker RunnableService
 */
class RunnableServiceTest : public ::testing::Test {
public:
    void SetUp() override
    {
      hard_worker_ = std::make_shared<HardWorker>();
      EXPECT_EQ(ServiceState::CREATED, hard_worker_->GetState());
      EXPECT_FALSE(hard_worker_->IsRunning());
    }

    void TearDown() override
    {
      hard_worker_->Shutdown();
      hard_worker_->Join();
      EXPECT_EQ(ServiceState::SHUTDOWN, hard_worker_->GetState());
      EXPECT_FALSE(hard_worker_->IsRunning());
      hard_worker_.reset();
    }

protected:
    std::shared_ptr<HardWorker> hard_worker_;
};

TEST_F(RunnableServiceTest, Sanity) {
  ASSERT_TRUE(true);
}

/**
 * Exercise the RunnableService start and shutdown. Verify that it ran.
 */
TEST_F(RunnableServiceTest, Test) {
  EXPECT_EQ(false, hard_worker_->GetHasWorked());
  EXPECT_EQ(false, hard_worker_->IsRunning());

  //  start the worker
  EXPECT_EQ(true, hard_worker_->Start());
  EXPECT_EQ(ServiceState::STARTED, hard_worker_->GetState());
  // expect false on subsequent start
  EXPECT_EQ(false, hard_worker_->Start());

  //  wait to make sure the thread was started
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  EXPECT_EQ(true, hard_worker_->IsRunning());
  EXPECT_EQ(ServiceState::STARTED, hard_worker_->GetState());

  EXPECT_EQ(true, hard_worker_->Shutdown());
  EXPECT_EQ(false, hard_worker_->Shutdown());

  hard_worker_->WaitForShutdown(std::chrono::milliseconds(1000)); // wait with timeout so we don't block other tests

  // did we at least work?
  EXPECT_EQ(true, hard_worker_->GetHasWorked());
}
