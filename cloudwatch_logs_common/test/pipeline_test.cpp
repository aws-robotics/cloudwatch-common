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
#include <aws/core/Aws.h>
#include <chrono>
#include "cloudwatch_logs_common/log_service.h"
#include "cloudwatch_logs_common/log_batcher.h"
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/utils/publisher.h>
#include <cloudwatch_logs_common/dataflow/dataflow.h>
#include <mutex>
#include <condition_variable>
#include <string>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace std::chrono_literals;

/**
 * Test the publisher interface while ignoring all of the CloudWatch specific infrastructure.
 */
class TestPublisher : public Publisher<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>
{
public:
  TestPublisher() {};
  // no-op
  bool initialize() override {return true;}
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

  void wait_for(std::chrono::seconds seconds) {
    std::unique_lock<std::mutex> lck(this->mtx);
    cv.wait_for(lck, seconds);
  }

protected:
  bool configure() {};
  bool publishData(std::list<Aws::CloudWatchLogs::Model::InputLogEvent> & data) {
    {
      std::unique_lock <std::mutex> lck(this->mtx);
      this->cv.notify_all();
    }
    return true;
  }

  std::condition_variable cv; // todo think about adding this into the interface
  std::mutex mtx; // todo think about adding this  into  the interface
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
      cw_service->shutdown();
      cw_service->join(); // todo wait for shutdown is broken
    }

protected:
    std::shared_ptr<CloudWatchService<LogType, std::string>> cw_service;
    std::shared_ptr<LogBatcher> log_batcher;
    std::shared_ptr<TestPublisher> test_publisher;
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

  // force a publish
  bool b2 = cw_service->publishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(1, test_publisher->getPublishedCount());
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
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
  EXPECT_EQ(1, test_publisher->getPublishedCount());
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherSize) {

  bool b = log_batcher->setMaxBatchSize(-1); // setting the size will trigger a publish when the collection is full
  EXPECT_FALSE(b);
  EXPECT_EQ(-1, log_batcher->getMaxBatchSize());

  b = log_batcher->setMaxBatchSize(5); // setting the size will trigger a publish when the collection is full
  EXPECT_TRUE(b);
  EXPECT_EQ(5, log_batcher->getMaxBatchSize());

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());

  for(int i=1; i<5; i++) {
    std::string toBatch("test message " + std::to_string(i));
    bool b1 = cw_service->batchData(toBatch);

    EXPECT_TRUE(b1);
    EXPECT_EQ(0, test_publisher->getPublishedCount());
    EXPECT_EQ(i, log_batcher->getCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  }

  ASSERT_EQ(5, log_batcher->getMaxBatchSize());
  std::string toBatch("test message publish trigger");
  bool b1 = cw_service->batchData(toBatch);

  EXPECT_TRUE(b1);

  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_EQ(1, test_publisher->getPublishedCount());
  EXPECT_EQ(0, log_batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
}