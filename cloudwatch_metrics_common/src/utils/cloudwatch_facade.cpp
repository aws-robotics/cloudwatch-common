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
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <aws_common/sdk_utils/aws_error.h>

#include <cloudwatch_metrics_common/utils/cloudwatch_metrics_facade.hpp>

#include <cloudwatch_metrics_common/definitions/definitions.h>

#include <string>

using namespace Aws::CloudWatch::Utils;

// https://docs.aws.amazon.com/AmazonCloudWatch/latest/APIReference/API_PutMetricData.html
#define MAX_METRIC_DATUMS_PER_REQUEST 20

CloudWatchFacade::CloudWatchFacade(const Aws::Client::ClientConfiguration & client_config)
: cw_client_(client_config)
{
}

Aws::AwsError CloudWatchFacade::SendMetricsRequest(
  const Aws::CloudWatch::Model::PutMetricDataRequest & request)
{
  Aws::AwsError status = Aws::AwsError::AWS_ERR_OK;
  auto response = this->cw_client_.PutMetricData(request);
  if (!response.IsSuccess()) {

    AWS_LOGSTREAM_DEBUG( __func__, "CloudWatchMetricsFacade: failed to send metric request: "
                         << static_cast<int>(response.GetError().GetErrorType()));

    switch(response.GetError().GetErrorType()) {
      //case Aws::CloudWatch::CloudWatchErrors::REQUEST_TIMEOUT:
      case Aws::CloudWatch::CloudWatchErrors::NETWORK_CONNECTION:
        status =  NETWORK_FAILURE;
        break;
      default:
        status =  FAILURE;
    }
  }
  return status;
}

CloudWatchMetricsStatus CloudWatchMetricsFacade::SendMetricsToCloudWatch(
  const std::string & metric_namespace, MetricDatumCollection &metrics)
{
  Aws::AwsError status = Aws::AwsError::AWS_ERR_OK;
  Aws::CloudWatch::Model::PutMetricDataRequest request;
  Aws::Vector<Aws::CloudWatch::Model::MetricDatum> datums;

  if (metrics.empty()) {
    AWS_LOGSTREAM_DEBUG( __func__, "CloudWatchMetricsFacade: no metrics to send");
    return FAILURE;
  }

  request.SetNamespace(metric_namespace.c_str());

  // Note: this fails an entire set of metrics, even if some are sent back successfully
  for (auto it = metrics.begin(); it != metrics.end(); ++it) {
    datums.push_back(*it);
    if (datums.size() >= MAX_METRIC_DATUMS_PER_REQUEST) {
      request.SetMetricData(datums);
      if (Aws::AwsError::AWS_ERR_OK != SendMetricsRequest(request)) {
        status = AWS_ERR_FAILURE;
      }
      datums.clear();
    }
  }

  if (datums.size() > 0) {
    request.SetMetricData(datums);
    if (Aws::AwsError::AWS_ERR_OK != SendMetricsRequest(request)) {
      status = AWS_ERR_FAILURE;
    }
  }

  return status;
}
