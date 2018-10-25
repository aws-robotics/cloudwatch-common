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

#include <cloudwatch_metrics_common/utils/cloudwatch_facade.hpp>

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
    // TODO: Better error handling and logging
    status = AWS_ERR_FAILURE;
  }
  return status;
}

Aws::AwsError CloudWatchFacade::SendMetricsToCloudWatch(
  const std::string & metric_namespace, std::list<Aws::CloudWatch::Model::MetricDatum> * metrics)
{
  Aws::AwsError status = Aws::AwsError::AWS_ERR_OK;
  Aws::CloudWatch::Model::PutMetricDataRequest request;
  Aws::Vector<Aws::CloudWatch::Model::MetricDatum> datums;
  if (!metrics) {
    return AWS_ERR_NULL_PARAM;
  } else if (metrics->empty()) {
    return AWS_ERR_EMPTY;
  }

  request.SetNamespace(metric_namespace.c_str());

  for (auto it = metrics->begin(); it != metrics->end(); ++it) {
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
