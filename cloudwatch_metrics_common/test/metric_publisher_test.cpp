/*
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <aws/core/Aws.h>
#include <aws/monitoring/model/MetricDatum.h>
#include <aws_common/sdk_utils/aws_error.h>
#include <gtest/gtest.h>

#include <cloudwatch_metrics_common/metric_manager.hpp>
#include <cloudwatch_metrics_common/metric_publisher.hpp>

using namespace Aws::CloudWatch::Metrics;

constexpr int WAIT_TIME =
  2000;  // the amount of time (ms) to wait for publisher thread to do its work

class MockCloudWatchFacade : public Aws::CloudWatch::Utils::CloudWatchFacade
{
public:
  Aws::AwsError send_metrics_ret_val;
  uint32_t send_metrics_call_count;
  std::string last_metric_namespace;
  std::list<Aws::CloudWatch::Model::MetricDatum> * last_metrics;

  MockCloudWatchFacade() { reset(); }

  void reset()
  {
    this->last_metric_namespace = "";
    this->last_metrics = nullptr;
    this->send_metrics_ret_val = Aws::AwsError::AWS_ERR_OK;
    this->send_metrics_call_count = 0;
  }

  Aws::AwsError SendMetricsToCloudWatch(
    const std::string & metric_namespace,
    std::list<Aws::CloudWatch::Model::MetricDatum> * metrics) override
  {
    this->last_metric_namespace = metric_namespace;
    this->last_metrics = metrics;
    this->send_metrics_call_count++;
    return this->send_metrics_ret_val;
  }
};

/* This test requires a fixture to init and shutdown the SDK or else it causes seg faults when
 * trying to construct a CloudWatch client. All tests must use the TEST_F function with this fixutre
 * name.
 */
class TestMetricPublisher : public ::testing::Test
{
protected:
  Utils::SharedObject<std::list<Aws::CloudWatch::Model::MetricDatum> *> shared_metrics_obj_;
  std::list<Aws::CloudWatch::Model::MetricDatum> metrics_list_;
  Aws::SDKOptions options_;
  std::shared_ptr<MockCloudWatchFacade> mock_cw_;
  MetricPublisher * publisher_;
  bool publisher_started_ = false;

  void StartPublisher()
  {
    EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, publisher_->StartPublisherThread());
    publisher_started_ = true;
  }

  void SetUp() override
  {
    Aws::InitAPI(options_);
    mock_cw_ = std::make_shared<MockCloudWatchFacade>();
    std::shared_ptr<Aws::CloudWatch::Utils::CloudWatchFacade> cw = mock_cw_;
    publisher_ = new MetricPublisher(cw, "test_namespace");
  }

  void TearDown() override
  {
    if (publisher_started_) {
      publisher_->StopPublisherThread();
    }
    delete publisher_;
    Aws::ShutdownAPI(options_);
  }
};

TEST_F(TestMetricPublisher, TestMetricPublisher_PublishMetrics_ReturnsErrorWhenMetricsNull)
{
  EXPECT_EQ(Aws::AWS_ERR_NULL_PARAM, publisher_->PublishMetrics(nullptr));
}

TEST_F(TestMetricPublisher, TestMetricPublisher_PublishMetrics_ReturnsErrorWhenSharedObjectUnlocked)
{
  EXPECT_EQ(Aws::AWS_ERR_PARAM, publisher_->PublishMetrics(&shared_metrics_obj_));
}

TEST_F(TestMetricPublisher, TestMetricPublisher_PublishMetrics_ReturnsErrorWhenThreadNotStarted)
{
  shared_metrics_obj_.SetDataAndMarkReady(&metrics_list_);

  EXPECT_EQ(Aws::AWS_ERR_NOT_INITIALIZED, publisher_->PublishMetrics(&shared_metrics_obj_));
}

TEST_F(TestMetricPublisher, TestMetricPublisher_PublishMetrics_ReturnsSuccessWhenListIngested)
{
  shared_metrics_obj_.SetDataAndMarkReady(&metrics_list_);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, publisher_->StartPublisherThread());
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, publisher_->PublishMetrics(&shared_metrics_obj_));
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, publisher_->StopPublisherThread());
}

TEST_F(TestMetricPublisher, TestMetricPublisher_BackgroundThread_CallsSendMetricsToCW)
{
  StartPublisher();
  shared_metrics_obj_.SetDataAndMarkReady(&metrics_list_);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, publisher_->PublishMetrics(&shared_metrics_obj_));
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
  if (mock_cw_->send_metrics_call_count < 1) {
    ADD_FAILURE() << "SendMetricsToCloudWatch() was not called after " << WAIT_TIME
                  << " ms, possible machine performance issue";
  }
  EXPECT_EQ(1, mock_cw_->send_metrics_call_count);
  EXPECT_EQ(&metrics_list_, mock_cw_->last_metrics);
}

TEST_F(TestMetricPublisher, TestMetricPublisher_BackgroundThread_UnlocksSharedObjectAfterPublish)
{
  StartPublisher();
  shared_metrics_obj_.SetDataAndMarkReady(&metrics_list_);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, publisher_->PublishMetrics(&shared_metrics_obj_));
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
  if (mock_cw_->send_metrics_call_count < 1) {
    ADD_FAILURE() << "SendMetricsToCloudWatch() was not called after " << WAIT_TIME
                  << " ms, possible machine performance issue";
  }
  EXPECT_EQ(1, mock_cw_->send_metrics_call_count);
  EXPECT_FALSE(shared_metrics_obj_.IsDataAvailable());
}

TEST_F(TestMetricPublisher, TestMetricPublisher_BackgroundThread_MultiplePublishes)
{
  StartPublisher();

  shared_metrics_obj_.SetDataAndMarkReady(&metrics_list_);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, publisher_->PublishMetrics(&shared_metrics_obj_));
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
  if (mock_cw_->send_metrics_call_count < 1) {
    ADD_FAILURE() << "SendMetricsToCloudWatch() was not called after " << WAIT_TIME
                  << " ms, possible machine performance issue";
  }
  EXPECT_EQ(1, mock_cw_->send_metrics_call_count);

  shared_metrics_obj_.SetDataAndMarkReady(&metrics_list_);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, publisher_->PublishMetrics(&shared_metrics_obj_));
  std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME));
  if (mock_cw_->send_metrics_call_count < 2) {
    ADD_FAILURE() << "SendMetricsToCloudWatch() was not called after " << WAIT_TIME
                  << " ms, possible machine performance issue";
  }
  EXPECT_EQ(2, mock_cw_->send_metrics_call_count);
}
