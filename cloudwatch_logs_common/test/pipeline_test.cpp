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
  TestPublisher() {
    force_failure_ = false;
    force_invalid_data_failure_ = false;
    last_upload_status_ = Aws::DataFlow::UploadStatus::UNKNOWN;
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
    force_failure_ = nv;
  }

  void SetForceInvalidDataFailure(bool nv) {
    force_invalid_data_failure_ = nv;
  }

  Aws::DataFlow::UploadStatus GetLastUploadStatus() {
    return last_upload_status_;
  }

protected:

  // override so we can notify when internal state changes, as attemptPublish sets state
  Aws::DataFlow::UploadStatus AttemptPublish(std::list<Aws::CloudWatchLogs::Model::InputLogEvent> &data) override {
    last_upload_status_ = Publisher::AttemptPublish(data);
    {
      this->notify();
    }
    return last_upload_status_;
  }

  Aws::DataFlow::UploadStatus PublishData(std::list<Aws::CloudWatchLogs::Model::InputLogEvent>& /*data*/) override {

    if (force_failure_) {
      return Aws::DataFlow::UploadStatus::FAIL;

    } else if (force_invalid_data_failure_) {
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

    FileObject<LogCollection> ReadBatch(size_t batch_size) override {
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
      test_publisher_ = std::make_shared<TestPublisher>();
      batcher_ = std::make_shared<LogBatcher>();

      //  log service owns the streamer, batcher, and publisher
      cw_service_ = std::make_shared<LogService>(test_publisher_, batcher_);

      stream_data_queue_ = std::make_shared<TaskObservedQueue<LogCollection>>();
      queue_monitor_ = std::make_shared<Aws::DataFlow::QueueMonitor<TaskPtr<LogCollection>>>();

      // create pipeline
      batcher_->SetSink(stream_data_queue_);
      queue_monitor_->AddSource(stream_data_queue_, Aws::DataFlow::PriorityOptions{Aws::DataFlow::HIGHEST_PRIORITY});
      cw_service_->SetSource(queue_monitor_);

      cw_service_->Start(); //this starts the worker thread
      EXPECT_EQ(Aws::DataFlow::UploadStatus::UNKNOWN, test_publisher_->GetLastUploadStatus());
    }

    void TearDown() override
    {
      if (cw_service_) {
        cw_service_->Shutdown();
        cw_service_->Join();
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
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  bool is_batched = cw_service_->BatchData(to_batch);
  EXPECT_EQ(1u, batcher_->GetCurrentBatchSize());

  EXPECT_TRUE(is_batched);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
  EXPECT_FALSE(cw_service_->IsConnected());
  EXPECT_EQ(0, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher_->GetPublishSuccessPercentage());

  // force a publish
  bool b2 = cw_service_->PublishBatchedData();
  test_publisher_->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher_->GetPublisherState());
  EXPECT_TRUE(cw_service_->IsConnected());
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(100.0f, test_publisher_->GetPublishSuccessPercentage());
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, test_publisher_->GetLastUploadStatus());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherManualPublishMultipleItems) {

  std::string to_batch("TestBatcherManualPublish");
  bool is_batched = cw_service_->BatchData(to_batch);
  EXPECT_TRUE(is_batched);

  for(int i=99; i>0; i--) {
    is_batched = cw_service_->BatchData(std::to_string(99) + std::string(" bottles of beer on the wall"));
    EXPECT_TRUE(is_batched);
  }

  EXPECT_TRUE(is_batched);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
  EXPECT_FALSE(cw_service_->IsConnected());
  EXPECT_EQ(100u, batcher_->GetCurrentBatchSize());

  // force a publish
  bool b2 = cw_service_->PublishBatchedData();
  test_publisher_->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher_->GetPublisherState());
  EXPECT_TRUE(cw_service_->IsConnected());
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, test_publisher_->GetLastUploadStatus());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherSize) {

  EXPECT_EQ(SIZE_MAX, batcher_->GetTriggerBatchSize()); // not initialized
  size_t size = 5;
  batcher_->SetTriggerBatchSize(size); // setting the size will trigger a publish when the collection is full
  EXPECT_EQ(size, batcher_->GetTriggerBatchSize());

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
  EXPECT_FALSE(cw_service_->IsConnected());

  for(size_t i=1; i<size; i++) {
    std::string to_batch("test message " + std::to_string(i));
    bool is_batched = cw_service_->BatchData(to_batch);

    EXPECT_TRUE(is_batched);
    EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
    EXPECT_EQ(i, batcher_->GetCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
    EXPECT_FALSE(cw_service_->IsConnected());
  }

  ASSERT_EQ(size, batcher_->GetTriggerBatchSize());
  std::string to_batch("test message publish trigger");
  bool is_batched = cw_service_->BatchData(to_batch);

  EXPECT_TRUE(is_batched);

  test_publisher_->wait_for(std::chrono::seconds(1));

  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher_->GetPublisherState());
  EXPECT_TRUE(cw_service_->IsConnected());
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, test_publisher_->GetLastUploadStatus());
}

TEST_F(PipelineTest, TestSinglePublisherFailureToFileManager) {

  std::shared_ptr<TestLogFileManager> file_manager = std::make_shared<TestLogFileManager>();
  // hookup to the service
  batcher_->SetLogFileManager(file_manager);

  // batch
  std::string to_batch("TestBatcherManualPublish");
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  bool is_batched = cw_service_->BatchData(to_batch);
  EXPECT_EQ(1u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(true, is_batched);

  // force failure
  test_publisher_->SetForceFailure(true);

  // publish
  bool b2 = cw_service_->PublishBatchedData();
  test_publisher_->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::NOT_CONNECTED, test_publisher_->GetPublisherState());
  EXPECT_FALSE(cw_service_->IsConnected());
  EXPECT_EQ(0, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher_->GetPublishSuccessPercentage());

  file_manager->wait_for(std::chrono::seconds(1));
  //check that the filemanger callback worked
  EXPECT_EQ(1, file_manager->written_count);
  EXPECT_EQ(Aws::DataFlow::UploadStatus::FAIL, test_publisher_->GetLastUploadStatus());
}

TEST_F(PipelineTest, TestInvalidDataNotPassedToFileManager) {

  std::shared_ptr<TestLogFileManager> file_manager = std::make_shared<TestLogFileManager>();
  // hookup to the service
  batcher_->SetLogFileManager(file_manager);

  // batch
  std::string to_batch("TestBatcherManualPublish");
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  bool is_batched = cw_service_->BatchData(to_batch);
  EXPECT_EQ(1u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(true, is_batched);

  // force failure
  test_publisher_->SetForceInvalidDataFailure(true);

  // publish
  bool b2 = cw_service_->PublishBatchedData();
  test_publisher_->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::NOT_CONNECTED, test_publisher_->GetPublisherState());
  EXPECT_FALSE(cw_service_->IsConnected());
  EXPECT_EQ(0, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher_->GetPublishSuccessPercentage());

  file_manager->wait_for(std::chrono::seconds(1));  // wait just in case for writes
  EXPECT_EQ(0, file_manager->written_count);
  EXPECT_EQ(Aws::DataFlow::UploadStatus::INVALID_DATA, test_publisher_->GetLastUploadStatus());
}

TEST_F(PipelineTest, TestPublisherIntermittant) {

  std::random_device rd;
  std::mt19937 gen(rd());
  // 50-50 chance
  std::bernoulli_distribution d(0.5);

  int expected_success = 0;

  for(int i=1; i<=50; i++) {

      bool force_failure_ = d(gen);

      std::shared_ptr<TestLogFileManager> file_manager = std::make_shared<TestLogFileManager>();
      // hookup to the service
      batcher_->SetLogFileManager(file_manager);

      // batch
      std::string to_batch("TestPublisherIntermittant");
      EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
      bool is_batched = cw_service_->BatchData(to_batch);
      EXPECT_EQ(1u, batcher_->GetCurrentBatchSize());
      EXPECT_EQ(true, is_batched);

      // force failure
      test_publisher_->SetForceFailure(force_failure_);

      if (!force_failure_) {
        expected_success++;
      }

      // publish
      is_batched = cw_service_->PublishBatchedData();
      test_publisher_->wait_for(std::chrono::seconds(1));

      EXPECT_TRUE(is_batched);
      EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());

      auto expected_state = force_failure_ ? PublisherState::NOT_CONNECTED : PublisherState::CONNECTED;

      EXPECT_EQ(expected_state, test_publisher_->GetPublisherState());
      EXPECT_EQ(!force_failure_, cw_service_->IsConnected());  // if failure forced then not connected
      EXPECT_EQ(force_failure_ ?
                 Aws::DataFlow::UploadStatus::FAIL: Aws::DataFlow::UploadStatus::SUCCESS,
                 test_publisher_->GetLastUploadStatus());

      EXPECT_EQ(expected_success, test_publisher_->GetPublishSuccesses());
      EXPECT_EQ(i, test_publisher_->GetPublishAttempts());

      float expected_percentage = static_cast<float>(expected_success) / static_cast<float>(i) * 100.0f;
      EXPECT_FLOAT_EQ(expected_percentage, test_publisher_->GetPublishSuccessPercentage());

      file_manager->wait_for_millis(std::chrono::milliseconds(100));
      //check that the filemanger callback worked
      EXPECT_EQ(force_failure_ ? 1 : 0, file_manager->written_count);
    }
}

TEST_F(PipelineTest, TestBatchDataTooFast) {

  size_t max = 10;
  std::shared_ptr<TestLogFileManager> file_manager = std::make_shared<TestLogFileManager>();
  // hookup to the service
  batcher_->SetLogFileManager(file_manager);
  batcher_->SetMaxAllowableBatchSize(max);

  EXPECT_EQ(max, batcher_->GetMaxAllowableBatchSize());

  for(size_t i=1; i<=max; i++) {
    std::string to_batch("test message " + std::to_string(i));
    bool is_batched = cw_service_->BatchData(to_batch);

    EXPECT_TRUE(is_batched);
    EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
    EXPECT_EQ(i, batcher_->GetCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
  }

  std::string to_batch("iocaine powder");
  bool is_batched = cw_service_->BatchData(to_batch);

  EXPECT_FALSE(is_batched);
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState()); // hasn't changed since not attempted
  EXPECT_FALSE(cw_service_->IsConnected());
  EXPECT_EQ(0, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher_->GetPublishSuccessPercentage());

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
