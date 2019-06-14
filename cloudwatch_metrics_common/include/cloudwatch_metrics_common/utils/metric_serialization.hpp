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

#pragma once

#include <aws/core/utils/StringUtils.h>
#include <aws/monitoring/model/MetricDatum.h>

namespace Aws {
namespace CloudWatch {
namespace Metrics {
namespace Utils {

Model::MetricDatum deserializeMetricDatum(const Aws::String &basic_string);
Aws::String  serializeMetricDatum(const Model::MetricDatum &datum);

}  // namespace Utils
}  // namespace Metrics
}  // namespace CloudWatch
}  // namespace Aws
