/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <cloudwatch_metrics_common/utils/metric_serialization.hpp>
#include <aws/core/utils/Array.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/monitoring/model/MetricDatum.h>
#include <aws/monitoring/model/StandardUnit.h>
#include <iterator>

namespace Aws {
namespace CloudWatch {
namespace Metrics {
namespace Utils {

static constexpr const char* kTimestampKey = "timestamp";
static constexpr const char* kMetricNameKey = "metric_name";
static constexpr const char* kCountsKey = "counts";
static constexpr const char* kCountKey = "count";
static constexpr const char* kDimensionsKey = "dimensions";
static constexpr const char* kStatisticValuesKey = "statisticvalues";
static constexpr const char* kValuesKey = "values";
static constexpr const char* kStorageResolutionKey = "storage_resolution";
static constexpr const char* kUnitKey = "unit";

Model::MetricDatum deserializeMetricDatum(const Aws::String  &basic_string) {
  Aws::String aws_str(basic_string.c_str());
  Aws::Utils::Json::JsonValue json_value(aws_str);
  auto view = json_value.View();
  Model::MetricDatum datum;
  datum.SetTimestamp(view.GetInt64(kTimestampKey));
  datum.SetMetricName(view.GetString(kMetricNameKey));

  if (view.KeyExists(kCountsKey)) {
    auto array = view.GetArray(kCountsKey);
    Aws::Vector<double> counts(array.GetLength());
    for (size_t i = 0; i < array.GetLength(); ++i) {
      counts[i] = array.GetItem(i).GetDouble(kCountKey);
    }
    datum.SetCounts(counts);
  }
  // @todo (rddesmon) deserialization of complex types
  // datum.SetDimensions();
  // datum.SetStatisticValues();
  // datum.SetValues();
  datum.SetStorageResolution(view.GetInteger(kStorageResolutionKey));
  datum.SetUnit(Model::StandardUnit(view.GetInteger(kUnitKey)));
  return datum;
}

Aws::String serializeMetricDatum(const Model::MetricDatum &datum) {
  Aws::Utils::Json::JsonValue json_value;
  // @todo (rddesmon) serialization of complex
  json_value
    .WithInt64(kTimestampKey, datum.GetTimestamp().CurrentTimeMillis())
    .WithString(kMetricNameKey, datum.GetMetricName())
    .WithInteger(kStorageResolutionKey, datum.GetStorageResolution())
    .WithInteger(kUnitKey, static_cast<int>(datum.GetUnit()));
  return json_value.View().WriteCompact();
}

}  // namespace Utils
}  // namespace Metrics
}  // namespace Cloudwatch
}  // namespace Aws