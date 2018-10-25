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

class MockMetricPublisher : public MetricPublisher
{
public:
  Utils::SharedObject<std::list<Aws::CloudWatch::Model::MetricDatum> *> * last_shared_metrics;
  Aws::AwsError publish_metric_ret_val;
  uint32_t public_metric_call_count;

  MockMetricPublisher() : MetricPublisher(nullptr, "") { reset(); }

  void reset()
  {
    this->last_shared_metrics = nullptr;
    this->publish_metric_ret_val = Aws::AwsError::AWS_ERR_OK;
    this->public_metric_call_count = 0;
  }

  Aws::AwsError PublishMetrics(
    Utils::SharedObject<std::list<Aws::CloudWatch::Model::MetricDatum> *> * shared_metrics) override
  {
    this->last_shared_metrics = shared_metrics;
    this->public_metric_call_count++;
    return this->publish_metric_ret_val;
  }

  Aws::AwsError StartPublisherThread() override { return Aws::AwsError::AWS_ERR_OK; }
};

/* This test requires a fixture to init and shutdown the SDK or else it causes seg faults when
 * trying to construct a CloudWatch client. All tests must use the TEST_F function with this fixutre
 * name.
 */
class TestMetricManager : public ::testing::Test
{
protected:
  Aws::SDKOptions options_;
  std::shared_ptr<MockMetricPublisher> mock_publisher_;
  std::shared_ptr<MetricManager> manager_;

  void SetUp() override
  {
    Aws::InitAPI(options_);
    mock_publisher_ = std::make_shared<MockMetricPublisher>();
    manager_ = std::make_shared<MetricManager>(mock_publisher_, 60);
  }

  void TearDown() override
  {
    manager_.reset();
    mock_publisher_.reset();
    Aws::ShutdownAPI(options_);
  }
};

TEST_F(TestMetricManager, TestMetricManager_Service_SharedObjectDataIsReady)
{
  std::map<std::string, std::string> dimensions;

  manager_->RecordMetric("Metric1", 1, "sec", 1000, dimensions);

  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  auto shared_object = mock_publisher_->last_shared_metrics;
  EXPECT_NE(nullptr, shared_object);
  EXPECT_TRUE(shared_object->IsDataAvailable());
}

TEST_F(TestMetricManager, TestMetricManager_Service_DoesntCallPublishIfStillOngoing)
{
  std::map<std::string, std::string> dimensions;

  manager_->RecordMetric("Metric1", 1, "sec", 1000, dimensions);

  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  EXPECT_EQ(1, mock_publisher_->public_metric_call_count);
}

TEST_F(TestMetricManager, TestMetricManager_Service_CallsPublishAgainAfterUnlock)
{
  std::map<std::string, std::string> dimensions;

  manager_->RecordMetric("Metric1", 1, "sec", 1000, dimensions);

  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  mock_publisher_->last_shared_metrics->FreeDataAndUnlock();
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  EXPECT_EQ(2, mock_publisher_->public_metric_call_count);
}

TEST_F(TestMetricManager, TestMetricManager_RecordMetric_AddsMetricsToList)
{
  std::map<std::string, std::string> dimensions;

  manager_->RecordMetric("Metric1", 1, "sec", 1000, dimensions);
  manager_->RecordMetric("Metric2", 2, "ms", 2000, dimensions);
  manager_->RecordMetric("Metric3", 3, "sec", 3000, dimensions);

  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  auto shared_object = mock_publisher_->last_shared_metrics;
  std::list<Aws::CloudWatch::Model::MetricDatum> * metrics_list = nullptr;
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, shared_object->GetDataAndLock(metrics_list));
  auto it = (*metrics_list).begin();

  Aws::Utils::DateTime expectedDateTime((int64_t)1000);
  EXPECT_EQ("Metric1", it->GetMetricName());
  EXPECT_EQ(expectedDateTime, (it)->GetTimestamp());
  EXPECT_EQ(1, (it++)->GetValue());
  EXPECT_EQ("Metric2", it->GetMetricName());
  EXPECT_EQ(2, (it++)->GetValue());
  EXPECT_EQ("Metric3", it->GetMetricName());
  EXPECT_EQ(3, (it++)->GetValue());
}

TEST_F(TestMetricManager, TestMetricManager_Service_SubsequentCallsUseDifferentLists)
{
  std::map<std::string, std::string> dimensions;

  manager_->RecordMetric("Metric1", 1, "Seconds", 1000, dimensions);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  std::list<Aws::CloudWatch::Model::MetricDatum> * metrics_list = nullptr;
  auto shared_object = mock_publisher_->last_shared_metrics;
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, shared_object->GetDataAndLock(metrics_list));
  auto metric = metrics_list->front();
  EXPECT_EQ("Metric1", metric.GetMetricName());
  EXPECT_EQ(1, metric.GetValue());
  EXPECT_EQ(Aws::CloudWatch::Model::StandardUnit::Seconds, metric.GetUnit());
  mock_publisher_->last_shared_metrics->FreeDataAndUnlock();

  manager_->RecordMetric("Metric2", 2, "Milliseconds", 2000, dimensions);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  shared_object = mock_publisher_->last_shared_metrics;
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, shared_object->GetDataAndLock(metrics_list));
  metric = metrics_list->front();
  EXPECT_EQ("Metric2", metric.GetMetricName());
  EXPECT_EQ(2, metric.GetValue());
  EXPECT_EQ(Aws::CloudWatch::Model::StandardUnit::Milliseconds, metric.GetUnit());
  mock_publisher_->last_shared_metrics->FreeDataAndUnlock();

  manager_->RecordMetric("Metric3", 3, "Seconds", 3000, dimensions);
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, manager_->Service());
  shared_object = mock_publisher_->last_shared_metrics;
  EXPECT_EQ(Aws::AwsError::AWS_ERR_OK, shared_object->GetDataAndLock(metrics_list));
  metric = metrics_list->front();
  EXPECT_EQ("Metric3", metric.GetMetricName());
  EXPECT_EQ(3, metric.GetValue());
  EXPECT_EQ(Aws::CloudWatch::Model::StandardUnit::Seconds, metric.GetUnit());
  mock_publisher_->last_shared_metrics->FreeDataAndUnlock();
}
