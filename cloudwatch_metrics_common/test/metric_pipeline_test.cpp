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
#include <dataflow_lite/task/task.h>
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
  TestPublisher() {
    force_failure_ = false;
    force_invalid_data_failure_ = false;
    last_upload_status_ = Aws::DataFlow::UploadStatus::UNKNOWN;
  };

  ~TestPublisher() override = default    ;

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
  Aws::DataFlow::UploadStatus AttemptPublish(MetricDatumCollection &data) override {
    last_upload_status_ = Publisher::AttemptPublish(data);
    {
      this->notify();
    }
    return last_upload_status_;
  }

  Aws::DataFlow::UploadStatus PublishData(MetricDatumCollection& /*data*/) override {

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
class TestMetricFileManager : public FileManager<MetricDatumCollection>, public Waiter
{
public:

    TestMetricFileManager() : FileManager(nullptr) {
      written_count.store(0);
    }

    void Write(const MetricDatumCollection & data) override {
      last_data_size = data.size();
      written_count++;
      this->notify();
    };

    FileObject<MetricDatumCollection> ReadBatch(size_t batch_size) override {
      // do nothing
      FileObject<MetricDatumCollection> test_file;
      test_file.batch_size = batch_size;
      return test_file;
    }

    std::atomic<int> written_count{};
    std::atomic<size_t> last_data_size{};
    std::condition_variable cv; // todo think about adding this into the interface
    mutable std::mutex mtx; // todo think about adding this  into  the interface
};


class PipelineTest : public ::testing::Test {
public:
    void SetUp() override
    {
      test_publisher_ = std::make_shared<TestPublisher>();
      batcher_ = std::make_shared<MetricBatcher>();

      //  log service owns the streamer, batcher, and publisher
      cw_service_ = std::make_shared<MetricService>(test_publisher_, batcher_);

      stream_data_queue_ = std::make_shared<TaskObservedQueue<MetricDatumCollection>>();
      queue_monitor_ = std::make_shared<Aws::DataFlow::QueueMonitor<TaskPtr<MetricDatumCollection>>>();

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
        cw_service_->Join(); // todo wait for shutdown is broken
      }
    }

protected:
    std::shared_ptr<TestPublisher> test_publisher_;
    std::shared_ptr<MetricBatcher> batcher_;
    std::shared_ptr<MetricService> cw_service_;

    std::shared_ptr<TaskObservedQueue<MetricDatumCollection>> stream_data_queue_;
    std::shared_ptr<Aws::DataFlow::QueueMonitor<TaskPtr<MetricDatumCollection>>>queue_monitor_;
};

MetricObject CreateTestMetricObject(
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

  auto object = CreateTestMetricObject(name);
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

  auto to_batch = CreateTestMetricObject(std::string("testMetric"));
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  bool is_batched = cw_service_->BatchData(to_batch);
  EXPECT_EQ(1u, batcher_->GetCurrentBatchSize());

  EXPECT_TRUE(is_batched);

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
  EXPECT_EQ(0, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher_->GetPublishSuccessPercentage());

  // force a publish
  bool b2 = cw_service_->PublishBatchedData();
  test_publisher_->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(b2);
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher_->GetPublisherState());
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(100.0f, test_publisher_->GetPublishSuccessPercentage());
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, test_publisher_->GetLastUploadStatus());
}

/**
 * Simple Pipeline test to check that everything was hooked up correctly.
 */
TEST_F(PipelineTest, TestBatcherManualPublishMultipleItems) {

  auto to_batch = CreateTestMetricObject(std::string("TestBatcherManualPublish"));
  bool is_batched = cw_service_->BatchData(to_batch);
  EXPECT_TRUE(is_batched);

  for(int i=99; i>0; i--) {
    auto batched_bottles = CreateTestMetricObject(std::to_string(99) + std::string(" bottles of beer on the wall"));
    is_batched = cw_service_->BatchData(batched_bottles);
    EXPECT_TRUE(is_batched);
  }

  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
  EXPECT_EQ(100u, batcher_->GetCurrentBatchSize());

  // force a publish
  is_batched = cw_service_->PublishBatchedData();
  test_publisher_->wait_for(std::chrono::seconds(1));

  EXPECT_TRUE(is_batched);
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher_->GetPublisherState());
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

  for(size_t i=1; i<size; i++) {
    auto to_batch = CreateTestMetricObject(std::string("test message ") + std::to_string(i));
    bool is_batched = cw_service_->BatchData(to_batch);

    EXPECT_TRUE(is_batched);
    EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
    EXPECT_EQ(i, batcher_->GetCurrentBatchSize());
    EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
  }

  ASSERT_EQ(size, batcher_->GetTriggerBatchSize());
  auto to_batch = CreateTestMetricObject(("test message " + std::to_string(size)));
  bool is_batched = cw_service_->BatchData(to_batch);

  EXPECT_TRUE(is_batched);

  test_publisher_->wait_for(std::chrono::seconds(1));

  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(1, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::CONNECTED, test_publisher_->GetPublisherState());
  EXPECT_EQ(Aws::DataFlow::UploadStatus::SUCCESS, test_publisher_->GetLastUploadStatus());
}

TEST_F(PipelineTest, TestSinglePublisherFailureToFileManager) {

  std::shared_ptr<TestMetricFileManager> file_manager = std::make_shared<TestMetricFileManager>();
  // hookup to the service
  batcher_->SetMetricFileManager(file_manager);

  // batch
  auto to_batch = CreateTestMetricObject(std::string("TestBatcherManualPublish"));
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
  EXPECT_EQ(0, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher_->GetPublishSuccessPercentage());

  file_manager->wait_for(std::chrono::seconds(1));
  //check that the filemanger callback worked
  EXPECT_EQ(1, file_manager->written_count);
  EXPECT_EQ(Aws::DataFlow::UploadStatus::FAIL, test_publisher_->GetLastUploadStatus());
}


TEST_F(PipelineTest, TestInvalidDataNotPassedToFileManager) {

  std::shared_ptr<TestMetricFileManager> file_manager = std::make_shared<TestMetricFileManager>();
  // hookup to the service
  batcher_->SetMetricFileManager(file_manager);

  // batch
  auto to_batch = CreateTestMetricObject(std::string("TestBatcherManualPublish"));
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
  EXPECT_EQ(0, test_publisher_->GetPublishSuccesses());
  EXPECT_EQ(1, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(0.0f, test_publisher_->GetPublishSuccessPercentage());

  file_manager->wait_for(std::chrono::seconds(1));
  //check that the filemanger callback worked
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

    std::shared_ptr<TestMetricFileManager> file_manager = std::make_shared<TestMetricFileManager>();
    // hookup to the service
    batcher_->SetMetricFileManager(file_manager);

    // batch
    auto to_batch = CreateTestMetricObject(std::string("TestPublisherIntermittant"));
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
    bool b2 = cw_service_->PublishBatchedData();
    test_publisher_->wait_for(std::chrono::seconds(1));

    EXPECT_TRUE(b2);
    EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());

    auto expected_state = force_failure_ ? PublisherState::NOT_CONNECTED : PublisherState::CONNECTED;

    EXPECT_EQ(expected_state, test_publisher_->GetPublisherState());
    EXPECT_EQ(expected_success, test_publisher_->GetPublishSuccesses());
    EXPECT_EQ(i, test_publisher_->GetPublishAttempts());
    EXPECT_EQ(force_failure_ ?
              Aws::DataFlow::UploadStatus::FAIL: Aws::DataFlow::UploadStatus::SUCCESS,
              test_publisher_->GetLastUploadStatus());

    float expected_percentage = static_cast<float>(expected_success) / static_cast<float>(i) * 100.0f;
    EXPECT_FLOAT_EQ(expected_percentage, test_publisher_->GetPublishSuccessPercentage());

    file_manager->wait_for_millis(std::chrono::milliseconds(100));
    //check that the filemanger callback worked
    EXPECT_EQ(force_failure_ ? 1 : 0, file_manager->written_count);
  }
}

TEST_F(PipelineTest, TestBatchDataTooFast) {

  size_t max = 10;
  std::shared_ptr<TestMetricFileManager> file_manager = std::make_shared<TestMetricFileManager>();
  // hookup to the service
  batcher_->SetMetricFileManager(file_manager);
  batcher_->SetMaxAllowableBatchSize(max);

  EXPECT_EQ(max, batcher_->GetMaxAllowableBatchSize());

  for(size_t i=1; i<=max; i++) {
  auto to_batch = CreateTestMetricObject(std::string("test message " + std::to_string(i)));
  bool is_batched = cw_service_->BatchData(to_batch);

  EXPECT_TRUE(is_batched);
  EXPECT_EQ(0, test_publisher_->GetPublishAttempts());
  EXPECT_EQ(i, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState());
  }

  auto to_batch = CreateTestMetricObject(std::string("iocaine powder"));
  bool b = cw_service_->BatchData(to_batch);

  EXPECT_FALSE(b);
  EXPECT_EQ(0u, batcher_->GetCurrentBatchSize());
  EXPECT_EQ(PublisherState::UNKNOWN, test_publisher_->GetPublisherState()); // hasn't changed since not attempted
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
