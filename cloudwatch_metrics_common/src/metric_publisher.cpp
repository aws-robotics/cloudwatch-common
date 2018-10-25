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
#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <aws_common/sdk_utils/aws_error.h>

#include <cloudwatch_metrics_common/metric_publisher.hpp>
#include <cloudwatch_metrics_common/utils/shared_object.hpp>
#include <memory>

using namespace Aws::CloudWatch::Metrics;

MetricPublisher::MetricPublisher(
  std::shared_ptr<Aws::CloudWatch::Utils::CloudWatchFacade> cw_client,
  const std::string & metrics_namespace)
{
  this->cw_client_ = cw_client;
  this->shared_metrics_.store(nullptr, std::memory_order_release);
  this->publisher_thread_ = nullptr;
  this->metrics_namespace_ = metrics_namespace;
}

MetricPublisher::~MetricPublisher()
{
  if (nullptr != this->publisher_thread_) {
    this->MetricPublisher::StopPublisherThread();
  }
}

Aws::AwsError MetricPublisher::PublishMetrics(
  Utils::SharedObject<std::list<Aws::CloudWatch::Model::MetricDatum> *> * shared_metrics)
{
  Aws::AwsError status = Aws::AwsError::AWS_ERR_OK;
  if (nullptr == shared_metrics) {
    return AWS_ERR_NULL_PARAM;
  } else if (!shared_metrics->IsDataAvailable()) {
    return AWS_ERR_PARAM;
  } else if (nullptr == this->publisher_thread_) {
    return AWS_ERR_NOT_INITIALIZED;
  }
  if (nullptr != this->shared_metrics_.load(std::memory_order_acquire)) {
    return AWS_ERR_ALREADY;
  }

  this->shared_metrics_.store(shared_metrics, std::memory_order_release);

  return status;
}

Aws::AwsError MetricPublisher::StartPublisherThread()
{
  if (nullptr != this->publisher_thread_) {
    return AWS_ERR_ALREADY;
  }
  this->thread_keep_running_.store(true, std::memory_order_release);
  this->publisher_thread_ = new std::thread(&MetricPublisher::Run, this);
  return Aws::AwsError::AWS_ERR_OK;
}

Aws::AwsError MetricPublisher::StopPublisherThread()
{
  if (nullptr == this->publisher_thread_) {
    return AWS_ERR_NOT_INITIALIZED;
  }
  this->thread_keep_running_.store(false, std::memory_order_release);
  this->publisher_thread_->join();
  delete this->publisher_thread_;
  this->publisher_thread_ = nullptr;
  return Aws::AwsError::AWS_ERR_OK;
}

void MetricPublisher::Run()
{
  while (this->thread_keep_running_.load(std::memory_order_acquire)) {
    // TODO: Replace this with a condition_variable or other event notification mechanism to
    // optimize
    auto shared_metrics_obj = this->shared_metrics_.load(std::memory_order_acquire);
    if (nullptr != shared_metrics_obj) {
      std::list<Aws::CloudWatch::Model::MetricDatum> * metrics;
      Aws::AwsError status = shared_metrics_obj->GetDataAndLock(metrics);

      if (Aws::AwsError::AWS_ERR_OK == status) {
        /* status = */ this->cw_client_->SendMetricsToCloudWatch(this->metrics_namespace_, metrics);
      }
      // TODO: For now we're just going to discard metrics that fail to get sent. Later we may add
      // some retry strategy

      /* Null out the class reference before unlocking the object. The external thread will be
         looking to key off if the shared object is locked or not to know it can start again, but
         the PublishMetrics function checks if the the pointer is null or not. */
      this->shared_metrics_.store(nullptr, std::memory_order_release);
      shared_metrics_obj->FreeDataAndUnlock();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
