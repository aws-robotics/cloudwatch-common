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

#include <list>

#include <aws/logs/model/InputLogEvent.h>
#include <aws/monitoring/model/MetricDatum.h>
#include <file_management/file_upload/file_manager.h>

namespace Aws {
namespace CloudWatch {
namespace Metrics {
namespace Utils {

//------------- Definitions --------------//
using MetricDatumList = std::list<Aws::CloudWatch::Model::MetricDatum>;
using MetricDatumListPtr = MetricDatumList *;
//----------------------------------------//

using FileManagement::FileManager;
using FileManagement::FileManagerStrategy;
using FileManagement::FileObject;

/**
 * The metric specific file manager. Handles the specific writes of metric data.
 */
class MetricFileManager :
    public FileManager<MetricDatumList> {
public:
  /**
   * Default Constructor.
   */
  MetricFileManager() = default;

  explicit MetricFileManager(
      const std::shared_ptr<FileManagerStrategy> &file_manager_strategy)
      : FileManager(file_manager_strategy) {
  }

  ~MetricFileManager() override = default;

  /**
   * Write data to disk
   * @param data - A reference to a list of metrics to write to disk
   */
  void write(const MetricDatumList &data) override;

  /**
   * Read a batch of data from disk
   * @param batch_size - The number of items to read
   */
  FileObject<MetricDatumList> readBatch(size_t batch_size) override;
};

}  // namespace Utils
}  // namespace Metrics
}  // namespace Cloudwatch
}  // namespace Aws
