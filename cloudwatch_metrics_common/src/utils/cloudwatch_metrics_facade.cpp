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
#include <aws/core/utils/logging/LogMacros.h>

#include <aws/monitoring/model/MetricDatum.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <aws_common/sdk_utils/aws_error.h>

#include <cloudwatch_metrics_common/utils/cloudwatch_metrics_facade.hpp>

#include <string>

using namespace Aws::CloudWatchMetrics::Utils;

// https://docs.aws.amazon.com/AmazonCloudWatch/latest/APIReference/API_PutMetricData.html
#define MAX_METRIC_DATUMS_PER_REQUEST 20

CloudWatchMetricsFacade::CloudWatchMetricsFacade(const Aws::Client::ClientConfiguration & client_config)
{
  this->cw_client_ = std::make_shared<Aws::CloudWatch::CloudWatchClient>(client_config);
}

CloudWatchMetricsFacade::CloudWatchMetricsFacade(const std::shared_ptr<Aws::CloudWatch::CloudWatchClient> cw_client)
{
  this->cw_client_ = cw_client;
}

CloudWatchMetricsStatus CloudWatchMetricsFacade::SendMetricsRequest(
  const Aws::CloudWatch::Model::PutMetricDataRequest & request)
{

  auto status = SUCCESS;
  auto response = this->cw_client_->PutMetricData(request);

  if (!response.IsSuccess()) {

    AWS_LOGSTREAM_DEBUG( __func__, "CloudWatchMetricsFacade: failed to send metric request: "
                         << static_cast<int>(response.GetError().GetErrorType()));

    switch(response.GetError().GetErrorType()) {
      //case Aws::CloudWatch::CloudWatchErrors::REQUEST_TIMEOUT:
      case Aws::CloudWatch::CloudWatchErrors::NETWORK_CONNECTION:
        status = NETWORK_FAILURE;
        break;
      default:
        status = FAILURE;
    }
  }
  return status;
}

CloudWatchMetricsStatus CloudWatchMetricsFacade::SendMetricsToCloudWatch(
  const std::string & metric_namespace, std::list<Aws::CloudWatch::Model::MetricDatum> &metrics)
{
  auto status = SUCCESS;
  Aws::CloudWatch::Model::PutMetricDataRequest request;
  Aws::Vector<Aws::CloudWatch::Model::MetricDatum> datums;

  if (metrics.empty()) {
    //todo log
    return FAILURE;
  }

  request.SetNamespace(metric_namespace.c_str());

  for (auto it = metrics.begin(); it != metrics.end(); ++it) {
    datums.push_back(*it);
    if (datums.size() >= MAX_METRIC_DATUMS_PER_REQUEST) {

      request.SetMetricData(datums);
      status = SendMetricsRequest(request);

      // if offline don't attempt to send again (fail fast)
      if (status == NETWORK_FAILURE) {
        return status;
      }
      datums.clear();
    }
  }

  if (datums.size() > 0) {
    request.SetMetricData(datums);
    status = SendMetricsRequest(request);
  }

  return status;
}
