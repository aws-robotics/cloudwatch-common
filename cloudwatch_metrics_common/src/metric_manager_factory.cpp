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

#include <cloudwatch_metrics_common/metric_manager.hpp>
#include <cloudwatch_metrics_common/metric_manager_factory.hpp>
#include <cloudwatch_metrics_common/metric_publisher.hpp>
#include <cloudwatch_metrics_common/utils/cloudwatch_facade.hpp>

using namespace Aws::CloudWatch::Metrics;

std::shared_ptr<MetricManager> MetricManagerFactory::CreateMetricManager(
  const Aws::Client::ClientConfiguration & client_config, const Aws::SDKOptions & sdk_options,
  const std::string & metrics_namespace, int storage_resolution)
{
  Aws::InitAPI(sdk_options);
  auto cw_client = std::make_shared<Aws::CloudWatch::Utils::CloudWatchFacade>(client_config);
  auto publisher = std::make_shared<MetricPublisher>(cw_client, metrics_namespace);
  publisher->StartPublisherThread();
  return std::make_shared<MetricManager>(publisher, storage_resolution);
}
