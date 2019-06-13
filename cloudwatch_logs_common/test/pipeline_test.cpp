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
#include <mutex>
#include <condition_variable>
#include <string>
#include <random>

#include <aws/core/Aws.h>

#include "cloudwatch_logs_common/log_service.h"
#include "cloudwatch_logs_common/log_batcher.h"
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/utils/publisher.h>
#include <cloudwatch_logs_common/dataflow/dataflow.h>

#include <aws/core/utils/logging/ConsoleLogSystem.h>

#include <thread>         // std::this_thread::sleep_for
#include <chrono>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace std::chrono_literals;

/**
 * Class used to provide easy mechanisn to wait and notify
 */
class Waiter
{
public:
  TestPublisher() : Publisher() {};
  virtual ~TestPublisher() {
    shutdown();
  };

  bool start() override {return true;}

  // notify just in case anyone is waiting
  bool shutdown() override {
    std::unique_lock<std::mutex> lck(this->mtx);
    this->cv.notify_all(); //don't leave anyone blocking
    return true;
  };

  void wait() {
    std::unique_lock<std::mutex> lck(this->mtx);
    cv.wait(lck);
  }

  void wait_for_millis(std::chrono::milliseconds millis) {
    std::unique_lock<std::mutex> lck(this->mtx);
    cv.wait_for(lck, millis);
  }

  void wait_for(std::chrono::seconds seconds) {
    std::unique_lock<std::mutex> lck(this->mtx);
    cv.wait_for(lck, seconds);
  }

  void notify() {
    std::unique_lock<std::mutex> lck(this->mtx);
    this->cv.notify_all(); //don't leave anyone blocking
  }

private:
  std::condition_variable cv;
  mutable std::mutex mtx;
};

/**
 * Test the publisher interface while ignoring all of the CloudWatch specific infrastructure.
 */
class TestPublisher : public Publisher<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>, public Waiter
{
public:
  TestPublisher() : Publisher() {
    force_failure = false;
  };
  virtual ~TestPublisher() {
    shutdown();
  };

  bool start() override {return true;}
  // notify just in case anyone is waiting
  bool shutdown() override {
    this->notify(); //don't leave anyone blocking
    return true;
  };

  void setForceFailure(bool nv) {
    force_failure = nv;
  }

protected:

  bool configure() override { return false; };
  bool publishData(std::list<Aws::CloudWatchLogs::Model::InputLogEvent>&) override {
    {
      this->notify();
    }
    return !force_failure;
  }

  std::condition_variable cv; // todo think about adding this into the interface
  mutable std::mutex mtx; // todo think about adding this  into  the interface
};


class PipelineTest : public ::testing::Test {
public:
    void SetUp() override
    {
      test_publisher = std::make_shared<TestPublisher>();
      log_batcher = std::make_shared<LogBatcher>();

      //  log service owns the streamer, batcher, and publisher
      cw_service = std::make_shared<CloudWatchService<LogType, std::string>> (test_publisher, log_batcher);

      stream_data_queue = std::make_shared<TaskObservedQueue<LogType>>();
      queue_monitor = std::make_shared<Aws::DataFlow::QueueMonitor<TaskPtr<LogType>>>();

      // create pipeline
      log_batcher->setSink(stream_data_queue);
      queue_monitor->addSource(stream_data_queue, Aws::DataFlow::PriorityOptions{Aws::DataFlow::HIGHEST_PRIORITY});
      cw_service->setSource(queue_monitor);

      cw_service->start(); //this starts the worker thread
    }

    void TearDown() override
    {
      if (cw_service) {
        cw_service->shutdown();
        cw_service->join(); // todo wait for shutdown is broken
      }
    }

protected:
    std::shared_ptr<LogBatcher> log_batcher;
    std::shared_ptr<TestPublisher> test_publisher;
    std::shared_ptr<CloudWatchService<LogType, std::string>> cw_service;

    std::shared_ptr<TaskObservedQueue<LogType>> stream_data_queue;
    std::shared_ptr<Aws::DataFlow::QueueMonitor<TaskPtr<LogType>>>queue_monitor;
};

TEST_F(PipelineTest, Sanity) {
  ASSERT_TRUE(true);
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherManualPublish) {

  std::string toBatch("TestBatcherManualPublish");
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  bool b1 = cw_service->batchData(toBatch);
  EXPECT_EQ(1, log_batcher->getCurrentBatchSize());

  EXPECT_TRUE(b1);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  EXPECT_EQ(0, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, test_publisher->getPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher->getPublishSuccessPercentage());

  // force a publish
  bool b2 = cw_service->publishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(100.0f, test_publisher->getPublishSuccessPercentage());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherManualPublishMultipleItems) {

  std::string toBatch("TestBatcherManualPublish");
  bool b1 = cw_service->batchData(toBatch);
  EXPECT_TRUE(b1);

  for(int i=99; i>0; i--) {
    b1 = cw_service->batchData(std::to_string(99) + std::string(" bottles of beer on the wall"));
    EXPECT_TRUE(b1);
  }

  EXPECT_TRUE(b1);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  EXPECT_EQ(100, log_batcher->getCurrentBatchSize());

  // force a publish
  bool b2 = cw_service->publishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherSize) {

  EXPECT_EQ(SIZE_MAX, log_batcher->getPublishTriggerBatchSize()); // not initialized
  size_t size = 5;
  log_batcher->setPublishTriggerBatchSize(size); // setting the size will trigger a publish when the collection is full
  EXPECT_EQ(size, log_batcher->getPublishTriggerBatchSize());

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());

  for(size_t i=1; i<size; i++) {
    std::string toBatch("test message " + std::to_string(i));
    bool b1 = cw_service->batchData(toBatch);

    EXPECT_TRUE(b1);
    EXPECT_EQ(0, test_publisher->getPublishAttempts());
    EXPECT_EQ(i, log_batcher->getCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  }

  ASSERT_EQ(size, log_batcher->getPublishTriggerBatchSize());
  std::string toBatch("test message publish trigger");
  bool b1 = cw_service->batchData(toBatch);

  EXPECT_TRUE(b1);

  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
}

TEST_F(PipelineTest, TestSinglePublisherFailureToFileManager) {

  std::shared_ptr<TestLogFileManager> fileManager = std::make_shared<TestLogFileManager>();
  // hookup to the service
  log_batcher->setLogFileManager(fileManager);

  // batch
  std::string toBatch("TestBatcherManualPublish");
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  bool b1 = cw_service->batchData(toBatch);
  EXPECT_EQ(1, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(true, b1);

  // force failure
  test_publisher->setForceFailure(true);

  // publish
  bool b2 = cw_service->publishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::NOT_CONNECTED, test_publisher->getPublisherState());
  EXPECT_EQ(0, test_publisher->getPublishSuccesses());
  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher->getPublishSuccessPercentage());

  fileManager->wait_for(std::chrono::seconds(1));
  //check that the filemanger callback worked
  EXPECT_EQ(1, fileManager->written_count);
}

TEST_F(PipelineTest, TestPublisherIntermittant) {

  std::random_device rd;
  std::mt19937 gen(rd());
  // 50-50 chance
  std::bernoulli_distribution d(0.5);

  int expected_success = 0;

  for(int i=1; i<=50; i++) {

      bool force_failure = d(gen);

      std::shared_ptr<TestLogFileManager> fileManager = std::make_shared<TestLogFileManager>();
      // hookup to the service
      log_batcher->setLogFileManager(fileManager);

      // batch
      std::string toBatch("TestPublisherIntermittant");
      EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
      bool b1 = cw_service->batchData(toBatch);
      EXPECT_EQ(1, log_batcher->getCurrentBatchSize());
      EXPECT_EQ(true, b1);

      // force failure
      test_publisher->setForceFailure(force_failure);

      if (!force_failure) {
        expected_success++;
      }

      // publish
      bool b2 = cw_service->publishBatchedData();
      test_publisher->wait_for(std::chrono::seconds(1));

      EXPECT_TRUE(b2);
      EXPECT_EQ(0, log_batcher->getCurrentBatchSize());

      auto expected_state = force_failure ? PublisherState::NOT_CONNECTED : PublisherState::CONNECTED;
      EXPECT_EQ(expected_state, test_publisher->getPublisherState());
      EXPECT_EQ(expected_success, test_publisher->getPublishSuccesses());
      EXPECT_EQ(i, test_publisher->getPublishAttempts());

      float expected_percentage = (float) expected_success / (float) i * 100.0f;
      EXPECT_FLOAT_EQ(expected_percentage, test_publisher->getPublishSuccessPercentage());

      fileManager->wait_for_millis(std::chrono::milliseconds(100));
      //check that the filemanger callback worked
      EXPECT_EQ(force_failure ? 1 : 0, fileManager->written_count);
    }
}

TEST_F(PipelineTest, TestBatchDataTooFast) {

  size_t max = 10;
  std::shared_ptr<TestLogFileManager> fileManager = std::make_shared<TestLogFileManager>();
  // hookup to the service
  log_batcher->setLogFileManager(fileManager);
  log_batcher->setMaxAllowableBatchSize(max);

  EXPECT_EQ(max, log_batcher->getMaxAllowableBatchSize());

  for(size_t i=1; i<=max; i++) {
    std::string toBatch("test message " + std::to_string(i));
    bool b1 = cw_service->batchData(toBatch);

    EXPECT_TRUE(b1);
    EXPECT_EQ(0, test_publisher->getPublishAttempts());
    EXPECT_EQ(i, log_batcher->getCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  }

  std::string toBatch("iocaine powder");
  bool b = cw_service->batchData(toBatch);

  EXPECT_TRUE(b);
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState()); // hasn't changed since not attempted
  EXPECT_EQ(0, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, test_publisher->getPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher->getPublishSuccessPercentage());

  fileManager->wait_for_millis(std::chrono::milliseconds(200));

  // check that the filemanger callback worked
  EXPECT_EQ(1, fileManager->written_count);
  EXPECT_EQ(max + 1, fileManager->last_data_size);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}