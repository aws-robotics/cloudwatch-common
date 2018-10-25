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

#include <cloudwatch_metrics_common/metric_manager.hpp>
#include <cloudwatch_metrics_common/metric_publisher.hpp>

namespace Aws {
namespace CloudWatch {
namespace Metrics {

class MetricManagerFactory
{
public:
  MetricManagerFactory() = default;
  ~MetricManagerFactory() = default;

  /**
   *  @brief Creates a new MetricManager object
   *  Factory method used to create a new MetricManager object, along with a creating and starting a
   * MetricPublisher for use with the MetricManager.
   *
   *  @param client_config The client configuration to use when creating the CloudWatch clients
   *  @param options The options used for the AWS SDK when creating and publishing with a CloudWatch
   * client
   *  @param metrics_namespace The namespace that will be used for all metrics that are published
   * using the MetricManager/MetricPublisher created by this function
   *  @param storage_resolution the storage resolution level for presenting metrics in CloudWatch
   */
  std::shared_ptr<MetricManager> CreateMetricManager(
    const Aws::Client::ClientConfiguration & client_config, const Aws::SDKOptions & sdk_options,
    const std::string & metrics_namespace, int storage_resolution);

private:
  /**
   * Block copy constructor and assignment operator for Factory object.
   */
  MetricManagerFactory(const MetricManagerFactory &) = delete;
  MetricManagerFactory & operator=(const MetricManagerFactory &) = delete;
};

}  // namespace Metrics
}  // namespace CloudWatch
}  // namespace Aws
