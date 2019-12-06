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

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>

#include <cloudwatch_logs_common/log_service.h>
#include <cloudwatch_logs_common/log_batcher.h>
#include <cloudwatch_logs_common/log_publisher.h>

#include <file_management/file_upload/file_manager.h>

#include <dataflow_lite/dataflow/dataflow.h>
#include <dataflow_lite/utils/waiter.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace std::chrono_literals;

/**
 * Test the publisher interface while ignoring all of the CloudWatch specific infrastructure.
 */
class TestPublisher : public Publisher<std::list<Aws::CloudWatchLogs::Model::InputLogEvent>>, public Waiter
{
public:
  TestPublisher() : {
    force_failure = false;
    force_invalid_data_failure = false;
    last_upload_status = Aws::DataFlow::UploadStatus::UNKNOWN;
  };
  ~TestPublisher() override = default;

  bool Start() override {
    return Publisher::Start();
  }

  // notify just in case anyone is waiting
  bool Shutdown() override {
    bool is_shutdown = Publisher::Shutdown();
    this->notify(); //don't leave anyone blocking
    return is_shutdown;
  };

  void SetForceFailure(bool nv) {
    force_failure = nv;
  }

  void SetForceInvalidDataFailure(bool nv) {
    force_invalid_data_failure = nv;
  }

  Aws::DataFlow::UploadStatus GetLastUploadStatus() {
    return last_upload_status;
  }

protected:

  // override so we can notify when internal state changes, as attemptPublish sets state
  Aws::DataFlow::UploadStatus attemptPublish(std::list<Aws::CloudWatchLogs::Model::InputLogEvent> &data) override {
    last_upload_status = Publisher::attemptPublish(data);
    {
      this->notify();
    }
    return last_upload_status;
  }

  Aws::DataFlow::UploadStatus PublishData(std::list<Aws::CloudWatchLogs::Model::InputLogEvent>& /*data*/) override {

    if (force_failure) {
      return Aws::DataFlow::UploadStatus::FAIL;

    } else if (force_invalid_data_failure) {
      return Aws::DataFlow::UploadStatus::INVALID_DATA;

    } else {
      return Aws::DataFlow::UploadStatus::SUCCESS;
    }
  }

  bool force_failure_;
  bool force_invalid_data_failure_;
  Aws::DataFlow::UploadStatus last_upload_status_;
};

/**
 * Test File Manager
 */
class TestLogFileManager : public FileManager<LogCollection>, public Waiter
{
public:

    TestLogFileManager() : FileManager(nullptr) {
      written_count.store(0);
    }

    void Write(const LogCollection & data) override {
      last_data_size = data.size();
      written_count++;
      this->notify();
    };

    FileObject<LogCollection> readBatch(size_t batch_size) override {
      // do nothing
      FileObject<LogCollection> test_file;
      test_file.batch_size = batch_size;
      return test_file;
    }

    std::atomic<int> written_count{};
    std::atomic<size_t> last_data_size{};
    std::condition_variable cv;
    mutable std::mutex mtx;
};


class PipelineTest : public ::testing::Test {
public:
    void SetUp() override
    {
      test_publisher = std::make_shared<TestPublisher>();
      batcher = std::make_shared<LogBatcher>();

      //  log service owns the streamer, batcher, and publisher
      cw_service = std::make_shared<LogService>(test_publisher, batcher);

      stream_data_queue = std::make_shared<TaskObservedQueue<LogCollection>>();
      queue_monitor = std::make_shared<Aws::DataFlow::QueueMonitor<TaskPtr<LogCollection>>>();

      // create pipeline
      batcher->SetSink(stream_data_queue);
      queue_monitor->AddSource(stream_data_queue, Aws::DataFlow::PriorityOptions{Aws::DataFlow::HIGHEST_PRIORITY});
      cw_service->setSource(queue_monitor);

      cw_service->Start(); //this starts the worker thread
      EXPECT_EQ(Aws::DataFlow::UploadStatus::UNKNOWN, test_publisher->getLastUploadStatus());
    }

    void TearDown() override
    {
      if (cw_service) {
        cw_service->Shutdown();
        cw_service->Join();
      }
    }

protected:
    std::shared_ptr<TestPublisher> test_publisher_;
    std::shared_ptr<LogBatcher> batcher_;
    std::shared_ptr<LogService> cw_service_;

    std::shared_ptr<TaskObservedQueue<LogCollection>> stream_data_queue_;
    std::shared_ptr<Aws::DataFlow::QueueMonitor<TaskPtr<LogCollection>>>queue_monitor_;
};

TEST_F(PipelineTest, Sanity) {
  ASSERT_TRUE(true);
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherManualPublish) {

  std::string to_batch("TestBatcherManualPublish");
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  bool is_batched = cw_service->BatchData(to_batch);
  EXPECT_EQ(1u, batcher->getCurrentBatchSize());

  EXPECT_TRUE(is_batched);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  EXPECT_FALSE(cw_service->IsConnected());
  EXPECT_EQ(0, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, test_publisher->getPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher->getPublishSuccessPercentage());

  // force a publish
  bool b2 = cw_service->PublishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
  EXPECT_TRUE(cw_service->IsConnected());
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(100.0f, test_publisher->getPublishSuccessPercentage());
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, test_publisher->getLastUploadStatus());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherManualPublishMultipleItems) {

  std::string to_batch("TestBatcherManualPublish");
  bool is_batched = cw_service->BatchData(to_batch);
  EXPECT_TRUE(is_batched);

  for(int i=99; i>0; i--) {
    is_batched = cw_service->BatchData(std::to_string(99) + std::string(" bottles of beer on the wall"));
    EXPECT_TRUE(is_batched);
  }

  EXPECT_TRUE(is_batched);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  EXPECT_FALSE(cw_service->IsConnected());
  EXPECT_EQ(100u, batcher->getCurrentBatchSize());

  // force a publish
  bool b2 = cw_service->PublishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
  EXPECT_TRUE(cw_service->IsConnected());
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, test_publisher->getLastUploadStatus());
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
  EXPECT_FALSE(cw_service->IsConnected());

  for(size_t i=1; i<size; i++) {
    std::string to_batch("test message " + std::to_string(i));
    bool is_batched = cw_service->BatchData(to_batch);

    EXPECT_TRUE(is_batched);
    EXPECT_EQ(0, test_publisher->getPublishAttempts());
    EXPECT_EQ(i, batcher->getCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
    EXPECT_FALSE(cw_service->IsConnected());
  }

  ASSERT_EQ(size, batcher->getTriggerBatchSize());
  std::string to_batch("test message publish trigger");
  bool is_batched = cw_service->BatchData(to_batch);

  EXPECT_TRUE(is_batched);

  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(1, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher->getPublisherState());
  EXPECT_TRUE(cw_service->IsConnected());
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, test_publisher->getLastUploadStatus());
}

TEST_F(PipelineTest, TestSinglePublisherFailureToFileManager) {

  std::shared_ptr<TestLogFileManager> file_manager = std::make_shared<TestLogFileManager>();
  // hookup to the service
  batcher->SetLogFileManager(file_manager);

  // batch
  std::string to_batch("TestBatcherManualPublish");
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  bool is_batched = cw_service->BatchData(to_batch);
  EXPECT_EQ(1u, batcher->getCurrentBatchSize());
  EXPECT_EQ(true, is_batched);

  // force failure
  test_publisher->setForceFailure(true);

  // publish
  bool b2 = cw_service->PublishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::NOT_CONNECTED, test_publisher->getPublisherState());
  EXPECT_FALSE(cw_service->IsConnected());
  EXPECT_EQ(0, test_publisher->getPublishSuccesses());
  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher->getPublishSuccessPercentage());

  file_manager->wait_for(std::chrono::seconds(1));
  //check that the filemanger callback worked
  EXPECT_EQ(1, file_manager->written_count);
  EXPECT_EQ(Aws::DataFlow::UploadStatus::FAIL, test_publisher->getLastUploadStatus());
}

TEST_F(PipelineTest, TestInvalidDataNotPassedToFileManager) {

  std::shared_ptr<TestLogFileManager> file_manager = std::make_shared<TestLogFileManager>();
  // hookup to the service
  batcher->SetLogFileManager(file_manager);

  // batch
  std::string to_batch("TestBatcherManualPublish");
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  bool is_batched = cw_service->BatchData(to_batch);
  EXPECT_EQ(1u, batcher->getCurrentBatchSize());
  EXPECT_EQ(true, is_batched);

  // force failure
  test_publisher->setForceInvalidDataFailure(true);

  // publish
  bool b2 = cw_service->PublishBatchedData();
  test_publisher->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::NOT_CONNECTED, test_publisher->getPublisherState());
  EXPECT_FALSE(cw_service->IsConnected());
  EXPECT_EQ(0, test_publisher->getPublishSuccesses());
  EXPECT_EQ(1, test_publisher->getPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher->getPublishSuccessPercentage());

  file_manager->wait_for(std::chrono::seconds(1));  // wait just in case for writes
  EXPECT_EQ(0, file_manager->written_count);
  EXPECT_EQ(Aws::DataFlow::UploadStatus::INVALID_DATA, test_publisher->getLastUploadStatus());
}

TEST_F(PipelineTest, TestPublisherIntermittant) {

  std::random_device rd;
  std::mt19937 gen(rd());
  // 50-50 chance
  std::bernoulli_distribution d(0.5);

  int expected_success = 0;

  for(int i=1; i<=50; i++) {

      bool force_failure = d(gen);

      std::shared_ptr<TestLogFileManager> file_manager = std::make_shared<TestLogFileManager>();
      // hookup to the service
      batcher->SetLogFileManager(file_manager);

      // batch
      std::string to_batch("TestPublisherIntermittant");
      EXPECT_EQ(0u, batcher->getCurrentBatchSize());
      bool is_batched = cw_service->BatchData(to_batch);
      EXPECT_EQ(1u, batcher->getCurrentBatchSize());
      EXPECT_EQ(true, is_batched);

      // force failure
      test_publisher->setForceFailure(force_failure);

      if (!force_failure) {
        expected_success++;
      }

      // publish
      is_batched = cw_service->PublishBatchedData();
      test_publisher->wait_for(std::chrono::seconds(1));

      EXPECT_TRUE(is_batched);
      EXPECT_EQ(0u, batcher->getCurrentBatchSize());

      auto expected_state = force_failure ? PublisherState::NOT_CONNECTED : PublisherState::CONNECTED;

      EXPECT_EQ(expected_state, test_publisher->getPublisherState());
      EXPECT_EQ(!force_failure, cw_service->IsConnected());  // if failure forced then not connected
      EXPECT_EQ(force_failure ?
                 Aws::DataFlow::UploadStatus::FAIL: Aws::DataFlow::UploadStatus::SUCCESS,
                 test_publisher->getLastUploadStatus());

      EXPECT_EQ(expected_success, test_publisher->getPublishSuccesses());
      EXPECT_EQ(i, test_publisher->getPublishAttempts());

      float expected_percentage = static_cast<float>(expected_success) / static_cast<float>(i) * 100.0f;
      EXPECT_FLOAT_EQ(expected_percentage, test_publisher->getPublishSuccessPercentage());

      file_manager->wait_for_millis(std::chrono::milliseconds(100));
      //check that the filemanger callback worked
      EXPECT_EQ(force_failure ? 1 : 0, file_manager->written_count);
    }
}

TEST_F(PipelineTest, TestBatchDataTooFast) {

  size_t max = 10;
  std::shared_ptr<TestLogFileManager> file_manager = std::make_shared<TestLogFileManager>();
  // hookup to the service
  batcher->SetLogFileManager(file_manager);
  batcher->setMaxAllowableBatchSize(max);

  EXPECT_EQ(max, batcher->getMaxAllowableBatchSize());

  for(size_t i=1; i<=max; i++) {
    std::string to_batch("test message " + std::to_string(i));
    bool is_batched = cw_service->BatchData(to_batch);

    EXPECT_TRUE(is_batched);
    EXPECT_EQ(0, test_publisher->getPublishAttempts());
    EXPECT_EQ(i, batcher->getCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState());
  }

  std::string to_batch("iocaine powder");
  bool is_batched = cw_service->BatchData(to_batch);

  EXPECT_FALSE(is_batched);
  EXPECT_EQ(0u, batcher->getCurrentBatchSize());
  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher->getPublisherState()); // hasn't changed since not attempted
  EXPECT_FALSE(cw_service->IsConnected());
  EXPECT_EQ(0, test_publisher->getPublishSuccesses());
  EXPECT_EQ(0, test_publisher->getPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher->getPublishSuccessPercentage());

  file_manager->wait_for_millis(std::chrono::milliseconds(200));

  // check that the filemanger callback worked
  EXPECT_EQ(1, file_manager->written_count);
  EXPECT_EQ(max + 1, file_manager->last_data_size);
}

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}