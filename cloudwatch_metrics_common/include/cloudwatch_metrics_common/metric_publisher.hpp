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

#pragma once

#include <aws/core/Aws.h>
#include <aws/monitoring/CloudWatchClient.h>
#include <aws_common/sdk_utils/aws_error.h>

#include <cloudwatch_metrics_common/utils/cloudwatch_facade.hpp>
#include <cloudwatch_metrics_common/utils/shared_object.hpp>
#include <memory>
#include <thread>

namespace Aws {
namespace CloudWatch {
namespace Metrics {

/**
 *  @brief Class that handles sending metrics data to CloudWatch
 *  This class is responsible for emitting all the stored metrics to AWS CloudWatch.
 *  Metrics are published asynchronously using a thread. The thread waits on a condition
 *  variable and is signaled (by AWSCloudWatchMetricManager) whenever new metrics are
 *  available.
 */
class MetricPublisher
{
public:
  /**
   *  @brief Creates a MetricPublisher object that uses the provided client and SDK configuration
   *  Constructs a MetricPublisher object that will use the provided CloudWatchClient and SDKOptions
   * when it publishes metrics.
   *
   *  @param cw_client The CloudWatchClient that this publisher will use when pushing metrics to
   * CloudWatch.
   *  @param metrics_namespace The namespace used for all the metrics published through this
   */
  MetricPublisher(std::shared_ptr<Aws::CloudWatch::Utils::CloudWatchFacade> cw_client,
                  const std::string & metrics_namespace);

  /**
   *  @brief Tears down the MetricPublisher object
   */
  virtual ~MetricPublisher();

  /**
   *  @brief Asynchronously publishes the provided metrics to CloudWatch.
   *  This function will start an asynchronous request to CloudWatch to post the provided metrics.
   * If the publisher is already in the process of publishing a previous batch of metrics it will
   * fail with AWS_ERR_ALREADY.
   *
   *  @param shared_metrics A reference to a shared object that contains a map where the key is a
   * string representing metric namespaces and the value is a list that contains the metrics to be
   * published
   *  @return An error code that will be Aws::AwsError::AWS_ERR_OK if it started publishing
   * successfully, otherwise it will be an error code.
   */
  virtual Aws::AwsError PublishMetrics(
    Utils::SharedObject<std::list<Aws::CloudWatch::Model::MetricDatum> *> * shared_metrics);

  /**
   *  @brief Starts the background thread used by the publisher.
   *  Use this function in order to start the background thread that the publisher uses for the
   * asynchronous posting of metrics to CloudWatch
   *
   *  @return An error code that will be Aws::AwsError::AWS_ERR_OK if the thread was started
   * successfully
   */
  virtual Aws::AwsError StartPublisherThread();

  /**
   *  @brief Stops the background thread used by the publisher.
   *  Use this function in order to attempt a graceful shutdown of the background thread that the
   * publisher uses.
   *
   *  @return An error code that will be Aws::AwsError::AWS_ERR_OK if the thread was shutdown
   * successfully
   */
  virtual Aws::AwsError StopPublisherThread();

private:
  void Run();

  std::shared_ptr<Aws::CloudWatch::Utils::CloudWatchFacade> cw_client_;
  std::shared_ptr<Aws::CloudWatch::CloudWatchClient> cloudwatch_client_;
  Aws::SDKOptions aws_sdk_options_;
  std::thread * publisher_thread_;
  std::atomic_bool thread_keep_running_;
  std::string metrics_namespace_;
  std::atomic<Utils::SharedObject<std::list<Aws::CloudWatch::Model::MetricDatum> *> *>
    shared_metrics_;
};

}  // namespace Metrics
}  // namespace CloudWatch
}  // namespace Aws
