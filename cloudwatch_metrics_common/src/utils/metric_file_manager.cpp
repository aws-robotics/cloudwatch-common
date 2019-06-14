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

#include <memory>
#include <iostream>
#include <fstream>

#include <aws/core/utils/xml/XmlSerializer.h>

#include "cloudwatch_metrics_common/utils/metric_file_manager.hpp"
#include <cloudwatch_metrics_common/utils/metric_serialization.hpp>
#include "file_management/file_upload/file_manager_strategy.h"
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/logging/LogMacros.h>

namespace Aws {
namespace CloudWatch {
namespace Metrics {
namespace Utils {

FileObject<MetricType> MetricFileManager::readBatch(
    size_t batch_size) {
  MetricType log_data;
  FileManagement::DataToken data_token;
  std::list<FileManagement::DataToken> data_tokens;
  AWS_LOG_INFO(__func__, "Reading Logbatch");
  size_t actual_batch_size = 0;
  for (size_t i = 0; i < batch_size; ++i) {
    std::string line;
    if (!file_manager_strategy_->isDataAvailable()) {
      break;
    }
    data_token = read(line);
    Aws::String aws_line(line.c_str());
    auto metric_datum = deserializeMetricDatum(aws_line);
    actual_batch_size++;
    log_data.push_back(metric_datum);
    data_tokens.push_back(data_token);
  }
  FileObject<MetricType> file_object;
  file_object.batch_data = log_data;
  file_object.batch_size = actual_batch_size;
  file_object.data_tokens = data_tokens;
  return file_object;
}

void MetricFileManager::write(const MetricType &data) {
  for (const Model::MetricDatum &model: data) {
    auto metric_serial = serializeMetricDatum(model);
    file_manager_strategy_->write(metric_serial.c_str());
  }
  if (FileManager::file_status_monitor_) {
    AWS_LOG_INFO(__func__,
                 "Set file status available");
    FileManager::file_status_monitor_->setStatus(Aws::DataFlow::Status::AVAILABLE);
  }
}

}  // namespace Utils
}  // namespace Metrics
}  // namespace Cloudwatch
}  // namespace Aws
