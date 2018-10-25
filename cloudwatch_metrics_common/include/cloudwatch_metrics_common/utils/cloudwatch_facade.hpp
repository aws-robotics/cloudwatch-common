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
#include <aws/monitoring/model/MetricDatum.h>
#include <aws_common/sdk_utils/aws_error.h>

namespace Aws {
namespace CloudWatch {
namespace Utils {

/**
 *  @brief This class is a simple Facade over the CloudWatch client.
 *  This class is a very small abstraction over the CloudWatch client. It allows us to change the
 * details of how we're communicating with CloudWatch without the need to expose this in the rest of
 * our code. It also provides a shim for us to be able to Mock to unit test the rest of the code.
 *
 *  This class expects Aws::InitAPI() to have already been called before an instance is constructed
 *
 */
class CloudWatchFacade
{
public:
  /**
   *  @brief Creates a new CloudWatchFacade
   *  @param client_config The configuration for the cloudwatch client
   */
  CloudWatchFacade(const Aws::Client::ClientConfiguration & client_config);
  virtual ~CloudWatchFacade() = default;

  /**
   *  @brief Sends a list of metrics to CloudWatch
   *  Used to send a list of metrics to CloudWatch
   *
   *  @param metric_namespace A reference to a string with the namespace for all the metrics being
   * posted
   *  @param metrics A reference to a list of metrics that you want sent to CloudWatch
   *  @return An error code that will be Aws::AwsError::AWS_ERR_OK if all metrics were sent
   * successfully.
   */
  virtual Aws::AwsError SendMetricsToCloudWatch(
    const std::string & metric_namespace, std::list<Aws::CloudWatch::Model::MetricDatum> * metrics);

protected:
  CloudWatchFacade() = default;

private:
  Aws::CloudWatch::CloudWatchClient cw_client_;
  Aws::AwsError SendMetricsRequest(const Aws::CloudWatch::Model::PutMetricDataRequest & request);
};

}  // namespace Utils
}  // namespace CloudWatch
}  // namespace Aws
