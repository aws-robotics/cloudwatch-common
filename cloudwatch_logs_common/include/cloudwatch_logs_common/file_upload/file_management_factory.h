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

#include <cloudwatch_logs_common/utils/file_manager.h>
#include <cloudwatch_logs_common/file_upload/status_monitor.h>
#include <cloudwatch_logs_common/file_upload/file_upload_manager.h>
#include <cloudwatch_logs_common/file_upload/observed_queue.h>
#include <cloudwatch_logs_common/file_upload/queue_monitor.h>

namespace Aws {
namespace FileManagement {

template<typename T>
std::shared_ptr<FileUploadManager<T>> createFileUploadManager(
    std::shared_ptr<CloudWatchLogs::Utils::FileManager<T>> file_manager) {
  // File Management system
  // Create a file monitor to get notified if a file is ready to be read
  auto file_monitor =
      std::make_shared<Aws::FileManagement::StatusMonitor>();

  // Create a multi status condition to trigger on network status and file status
  auto multi_status_condition_monitor =
      std::make_shared<Aws::FileManagement::MultiStatusConditionMonitor>();
  multi_status_condition_monitor->addStatusMonitor(file_monitor);

  // Add the file monitor to the file manager to get notifications
  file_manager->addFileStatusMonitor(file_monitor);

  // Create an observed queue to trigger a publish when data is available
  auto observed_queue =
      std::make_shared<ObservedQueue<std::shared_ptr<Task<T>>>>();

  // Create a file upload manager to handle uploading a file.
  auto file_upload_manager =
      std::make_shared<Aws::FileManagement::FileUploadManager<T>>(
          multi_status_condition_monitor,
          file_manager,
          observed_queue,
          10);
  return file_upload_manager;
}

}  // namespace FileManagement
}  // namespace Aws

