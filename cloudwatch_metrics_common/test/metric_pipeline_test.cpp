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
#include <condition_variable>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <map>

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>

#include <cloudwatch_metrics_common/metric_service.hpp>
#include <cloudwatch_metrics_common/metric_batcher.h>
#include <cloudwatch_metrics_common/metric_publisher.hpp>
#include <cloudwatch_metrics_common/definitions/definitions.h>

#include <file_management/file_upload/file_manager.h>

#include <dataflow_lite/dataflow/dataflow.h>
#include <dataflow_lite/utils/waiter.h>

using namespace Aws::CloudWatchMetrics;
using namespace Aws::CloudWatchMetrics::Utils;
using namespace std::chrono_literals;

/**
 * Test the publisher interface while ignoring all of the CloudWatch specific infrastructure.
 */
class TestPublisher : public Publisher<MetricDatumCollection>, public Waiter
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

  bool publishData(MetricDatumCollection&) override {
  {
    this->notify();
  }
  return !force_failure;
  }

  bool force_failure;
};

/**
 * Test File Manager
 */
class TestMetricFileManager : public FileManager<MetricDatumCollection>, public Waiter
{
public:

    TestMetricFileManager() : FileManager(nullptr) {
      written_count.store(0);
    }

    void write(const MetricDatumCollection & data) override {
      last_data_size = data.size();
      written_count++;
      this->notify();
    };

    FileObject<MetricDatumCollection> readBatch(size_t batch_size) {
      // do nothing
      FileObject<MetricDatumCollection> testFile;
      testFile.batch_size = batch_size;
      return testFile;
    }

    std::atomic<int> written_count;
    std::atomic<size_t> last_data_size;
    std::condition_variable cv; // todo think about adding this into the interface
    mutable std::mutex mtx; // todo think about adding this  into  the interface
};


class PipelineTest : public ::testing::Test {
public:
    void SetUp() override
    {
      test_publisher = std::make_shared<TestPublisher>();
      batcher = std::make_shared<MetricBatcher>();

      //  log service owns the streamer, batcher, and publisher
      cw_service = std::make_shared<MetricService>(test_publisher, batcher);

      stream_data_queue = std::make_shared<TaskObservedQueue<MetricDatumCollection>>();
      queue_monitor = std::make_shared<Aws::DataFlow::QueueMonitor<TaskPtr<MetricDatumCollection>>>();

      // create pipeline
      batcher->setSink(stream_data_queue);
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
    std::shared_ptr<TestPublisher> test_publisher;
    std::shared_ptr<MetricBatcher> batcher;
    std::shared_ptr<MetricService> cw_service;

    std::shared_ptr<TaskObservedQueue<MetricDatumCollection>> stream_data_queue;
    std::shared_ptr<Aws::DataFlow::QueueMonitor<TaskPtr<MetricDatumCollection>>>queue_monitor;
};

MetricObject createTestMetricObject(
        const std::string & name,
        const double value = 2.42,
        const std::string & unit = "gigawatts",
        const int64_t timestamp = 1234,
        const std::map<std::string, std::string> & dimensions = std::map<std::string, std::string>(),
        const int storage_resolution = 60
        ) {
  return MetricObject {name, value, unit, timestamp, dimensions, storage_resolution};
}

TEST_F(PipelineTest, Sanity) {
  ASSERT_TRUE(true);
}

TEST(MetricPipelineTest, TestCreateMetricObject) {

  std::string name("HeWhoShallNotBenamed");

  auto object = createTestMetricObject(name);
  auto empty = std::map<std::string, std::string>();

  EXPECT_EQ(name, object.metric_name);
  EXPECT_EQ(2.42, object.value);
  EXPECT_EQ("gigawatts", object.unit);
  EXPECT_EQ(1234, object.timestamp);
  EXPECT_EQ(empty, object.dimensions);
  EXPECT_EQ(60, object.storage_resolution);
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherManualPublish) {

  auto toBatch = createTestMetricObject(std::string("testMetric"));
  EXPECT_EQ(0, batcher->getCurrentBatchSize());
  bool b1 = cw_service->batchData(toBatch);
  EXPECT_EQ(1, batcher->getCurrentBatchSize());

  EXPECT_TRUE(b1);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  EXPECT_EQ(0, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, test_publisher->getPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher->getPublishSuccessPercentage());

  // force a publish
  bool b2 = cw_service->publishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(100.0f, test_publisher->getPublishSuccessPercentage());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherManualPublishMultipleItems) {

  auto toBatch = createTestMetricObject(std::string("TestBatcherManualPublish"));
  bool b1 = cw_service->batchData(toBatch);
  EXPECT_TRUE(b1);

  for(int i=99; i>0; i--) {
    auto batchedBottles = createTestMetricObject(std::to_string(99) + std::string(" bottles of beer on the wall"));
    b1 = cw_service->batchData(batchedBottles);
    EXPECT_TRUE(b1);
  }

  EXPECT_TRUE(b1);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  EXPECT_EQ(100, batcher->getCurrentBatchSize());

  // force a publish
  bool b2 = cw_service->publishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherSize) {

  EXPECT_EQ(SIZE_MAX, batcher->getTriggerBatchSize()); // not initialized
  size_t size = 5;
  batcher->setTriggerBatchSize(size); // setting the size will trigger a publish when the collection is full
  EXPECT_EQ(size, batcher->getTriggerBatchSize());

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());

  for(size_t i=1; i<size; i++) {
    auto toBatch = createTestMetricObject(std::string("test message ") + std::to_string(i));
    bool b1 = cw_service->batchData(toBatch);

    EXPECT_TRUE(b1);
    EXPECT_EQ(0, test_publisher->getPublishAttempts());
    EXPECT_EQ(i, batcher->getCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  }

  ASSERT_EQ(size, batcher->getTriggerBatchSize());
  auto toBatch = createTestMetricObject(("test message " + std::to_string(size)));
  bool b1 = cw_service->batchData(toBatch);

  EXPECT_TRUE(b1);

  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
}

TEST_F(PipelineTest, TestSinglePublisherFailureToFileManager) {

  std::shared_ptr<TestMetricFileManager> fileManager = std::make_shared<TestMetricFileManager>();
  // hookup to the service
  batcher->setMetricFileManager(fileManager);

  // batch
  auto toBatch = createTestMetricObject(std::string("TestBatcherManualPublish"));
  EXPECT_EQ(0, batcher->getCurrentBatchSize());
  bool b1 = cw_service->batchData(toBatch);
  EXPECT_EQ(1, batcher->getCurrentBatchSize());
  EXPECT_EQ(true, b1);

  // force failure
  test_publisher->setForceFailure(true);

  // publish
  bool b2 = cw_service->publishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0, batcher->getCurrentBatchSize());
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

  std::shared_ptr<TestMetricFileManager> fileManager = std::make_shared<TestMetricFileManager>();
  // hookup to the service
  batcher->setMetricFileManager(fileManager);

  // batch
  auto toBatch = createTestMetricObject(std::string("TestPublisherIntermittant"));
  EXPECT_EQ(0, batcher->getCurrentBatchSize());
  bool b1 = cw_service->batchData(toBatch);
  EXPECT_EQ(1, batcher->getCurrentBatchSize());
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
  EXPECT_EQ(0, batcher->getCurrentBatchSize());

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
  std::shared_ptr<TestMetricFileManager> fileManager = std::make_shared<TestMetricFileManager>();
  // hookup to the service
  batcher->setMetricFileManager(fileManager);
  batcher->setMaxAllowableBatchSize(max);

  EXPECT_EQ(max, batcher->getMaxAllowableBatchSize());

  for(size_t i=1; i<=max; i++) {
  auto toBatch = createTestMetricObject(std::string("test message " + std::to_string(i)));
  bool b1 = cw_service->batchData(toBatch);

  EXPECT_TRUE(b1);
  EXPECT_EQ(0, test_publisher->getPublishAttempts());
  EXPECT_EQ(i, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  }

  auto toBatch = createTestMetricObject(std::string("iocaine powder"));
  bool b = cw_service->batchData(toBatch);

  EXPECT_FALSE(b);
  EXPECT_EQ(0, batcher->getCurrentBatchSize());
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