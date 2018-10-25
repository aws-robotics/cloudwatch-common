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
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <aws_common/sdk_utils/aws_error.h>

#include <cloudwatch_metrics_common/metric_publisher.hpp>
#include <cloudwatch_metrics_common/utils/shared_object.hpp>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>

namespace Aws {
namespace CloudWatch {
namespace Metrics {

/**
 *  @brief The MetricsManager class is responsible for buffering metrics and sending them in bulk to
 * a MetricsPublisher. The MetricsManager handles grouping metrics into buffers by namespace. It
 * will periodically pass these buffers to a MetricsPublisher in order to send them out to
 * CloudWatch. In order for periodic tasks to be executed the user should call the Service()
 * function.
 *
 *  This class is not thread safe. Metrics should only be enqueued from a single thread at a time.
 *
 *  The class will only flush a single set of buffers at a time. So while you can still enqueue new
 * metrics while the previously buffered metrics are being published in the background, the
 * MetricManage will not attempt to publish again until after the ongoing publish finishes.
 *
 */
class MetricManager
{
public:
  /**
   *  @brief Creates a new MetricManager
   *  Creates a new MetricManager that will group/buffer metrics and then send them to the provided
   * metric_publisher to be sent out to CloudWatch
   *
   *  @param metric_publisher A shared pointer to a MetricPublisher that will be used to publish the
   * buffered metrics
   */
  explicit MetricManager(const std::shared_ptr<MetricPublisher> metric_publisher,
                         int storage_resolution);

  /**
   *  @brief Tears down a MetricManager object
   */
  ~MetricManager();

  /**
   *  @brief Used to add a metric to the buffer
   *  Adds the given metric to the internal buffer that will be flushed periodically.
   *
   *  @param metric_name The name of the metric being recorded
   *  @param value The value of the metric being recorded
   *  @param unit A string representing the unit of for the metric. If this can't be mapped to an
   * AWS unit type it will be set to None internally. The unit name should match the string value
   * for StandardUnit
   *      https://github.com/aws/aws-sdk-cpp/blob/0912003eae920b53661ef6287b7b97d16a507b67/aws-cpp-sdk-monitoring/source/model/StandardUnit.cpp
   *  @param timestamp A timestamp representing the number of milliseconds since the epoch when this
   * metric was recorded
   *  @param dimensions A map of key/value pairs representing the dimensions for the metric
   *  @return An error code that will be Aws::AwsError::AWS_ERR_OK if the metric was recorded
   * successfully
   */
  virtual Aws::AwsError RecordMetric(const std::string & metric_name, double value,
                                     const std::string & unit, int64_t timestamp,
                                     const std::map<std::string, std::string> & dimensions);

  /**
   *  @brief Services the metric manager by performing periodic tasks when called.
   *  Calling the Service function allows for periodic tasks associated with the metrics manager,
   * such as flushing buffered metrics, to be performed.
   *
   *  @return An error code that will be Aws::AwsError::AWS_ERR_OK if all periodic tasks are
   * executed successfully
   */
  virtual Aws::AwsError Service();

private:
  std::shared_ptr<MetricPublisher> metric_publisher_ = nullptr;
  Utils::SharedObject<std::list<Aws::CloudWatch::Model::MetricDatum> *> shared_object_;
  std::list<Aws::CloudWatch::Model::MetricDatum> metric_buffers_[2];
  uint8_t active_buffer_indx_ = 0;
  int storage_resolution_ = 0;
};

}  // namespace Metrics
}  // namespace CloudWatch
}  // namespace Aws
