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
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <aws/monitoring/model/StandardUnit.h>
#include <aws_common/sdk_utils/aws_error.h>

#include <cloudwatch_metrics_common/metric_manager.hpp>
#include <cloudwatch_metrics_common/metric_publisher.hpp>
#include <cloudwatch_metrics_common/utils/shared_object.hpp>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

using namespace Aws::CloudWatch::Metrics;

/* Contains a map of the ROS MetricData constant values to the AWS StandardUnits */
static std::unordered_map<std::string, Aws::CloudWatch::Model::StandardUnit> units_mapper = {
  {"sec", Aws::CloudWatch::Model::StandardUnit::Seconds},
  {"msec", Aws::CloudWatch::Model::StandardUnit::Milliseconds},
  {"usec", Aws::CloudWatch::Model::StandardUnit::Microseconds},
  {"percent", Aws::CloudWatch::Model::StandardUnit::Percent},
  {"count", Aws::CloudWatch::Model::StandardUnit::Count},
  {"count_per_sec", Aws::CloudWatch::Model::StandardUnit::Count_Second},
  {"bytes", Aws::CloudWatch::Model::StandardUnit::Bytes},
  {"kilobytes", Aws::CloudWatch::Model::StandardUnit::Kilobytes},
  {"megabytes", Aws::CloudWatch::Model::StandardUnit::Megabytes},
  {"gigabytes", Aws::CloudWatch::Model::StandardUnit::Gigabytes},
  {"terabytes", Aws::CloudWatch::Model::StandardUnit::Terabytes},
  {"bytes_per_sec", Aws::CloudWatch::Model::StandardUnit::Bytes_Second},
  {"kilobytes_per_sec", Aws::CloudWatch::Model::StandardUnit::Kilobytes_Second},
  {"megabytes_per_sec", Aws::CloudWatch::Model::StandardUnit::Megabytes_Second},
  {"gigabytes_per_sec", Aws::CloudWatch::Model::StandardUnit::Gigabytes_Second},
  {"terabytes_per_sec", Aws::CloudWatch::Model::StandardUnit::Terabytes_Second},
  {"none", Aws::CloudWatch::Model::StandardUnit::None},
  {"", Aws::CloudWatch::Model::StandardUnit::None},
};

MetricManager::MetricManager(const std::shared_ptr<MetricPublisher> metric_publisher,
                             int storage_resolution)
{
  this->metric_publisher_ = metric_publisher;
  this->storage_resolution_ = storage_resolution;
}

MetricManager::~MetricManager()
{
  this->metric_buffers_[0].clear();
  this->metric_buffers_[1].clear();
}

Aws::AwsError MetricManager::RecordMetric(const std::string & metric_name, double value,
                                          const std::string & unit, int64_t timestamp,
                                          const std::map<std::string, std::string> & dimensions)
{
  Aws::AwsError status = Aws::AwsError::AWS_ERR_OK;
  Aws::CloudWatch::Model::MetricDatum datum;
  Aws::String aws_metric_name(metric_name.c_str());
  Aws::Utils::DateTime date_time(timestamp);
  datum.WithMetricName(aws_metric_name).WithTimestamp(date_time).WithValue(value);
  auto mapped_unit = units_mapper.find(unit);
  if (units_mapper.end() != mapped_unit) {
    datum.WithUnit(mapped_unit->second);
  } else {
    Aws::String unit_name(unit.c_str());
    datum.WithUnit(Aws::CloudWatch::Model::StandardUnitMapper::GetStandardUnitForName(unit_name));
  }

  for (auto it = dimensions.begin(); it != dimensions.end(); ++it) {
    Aws::CloudWatch::Model::Dimension dimension;
    Aws::String name(it->first.c_str());
    Aws::String value(it->second.c_str());
    dimension.WithName(name.c_str()).WithValue(value);
    datum.AddDimensions(dimension);
  }
  datum.SetStorageResolution(this->storage_resolution_);
  this->metric_buffers_[active_buffer_indx_].push_back(datum);

  return status;
}

Aws::AwsError MetricManager::Service()
{
  Aws::AwsError status = Aws::AwsError::AWS_ERR_OK;

  // if the shared object is not still locked for publishing, then swap the buffers and publish the
  // new metrics
  if (!this->shared_object_.IsDataAvailable()) {
    uint8_t new_active_buffer_indx = 1 - active_buffer_indx_;
    // Clean any old metrics out the buffer that previously used for publishing
    this->metric_buffers_[new_active_buffer_indx].clear();

    status =
      this->shared_object_.SetDataAndMarkReady(&this->metric_buffers_[this->active_buffer_indx_]);
    // After this point no changes should be made to the active_buffer until it is swapped with the
    // secondary buffer

    if (Aws::AwsError::AWS_ERR_OK == status) {
      status = this->metric_publisher_->PublishMetrics(&this->shared_object_);

      if (Aws::AwsError::AWS_ERR_OK != status) {
        // For some reason we failed to publish, so mark the shared object as free so we can try
        // again later
        this->shared_object_.FreeDataAndUnlock();
      }
    }

    if (Aws::AwsError::AWS_ERR_OK == status) {
      /* If everything has been successful for publishing the buffer, then swap to the new buffer.
       */
      this->active_buffer_indx_ = new_active_buffer_indx;
    }
  }
  return status;
}
