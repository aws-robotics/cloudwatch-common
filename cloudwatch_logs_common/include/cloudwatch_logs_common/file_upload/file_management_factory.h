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

#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/dataflow/status_monitor.h>
#include <cloudwatch_logs_common/file_upload/file_upload_streamer.h>
#include <cloudwatch_logs_common/dataflow/observed_queue.h>
#include <cloudwatch_logs_common/dataflow/queue_monitor.h>

namespace Aws {
namespace FileManagement {

static const FileManagerOptions kDefaultFileManagerOptions{50, 5};

/**
 * Create a file upload manager complete with a file status monitor attached to the file_manager,
 * and a task based queue.
 *
 * @tparam T the type of messages the file uploader will handle
 * @param file_manager to use as the source of these messages
 * @return a shared pointer to a configured file upload manager.
 */
template<
  typename T,
  typename O,
  class = typename std::enable_if<std::is_base_of<DataReader<T>, O>::value, O>::type>
std::shared_ptr<FileUploadStreamer<T>> createFileUploadStreamer(
  std::shared_ptr<O> file_manager, FileManagerOptions file_manager_options = kDefaultFileManagerOptions)
  {

  // File Management system
  // Create a file monitor to get notified if a file is ready to be read
  auto file_monitor =
      std::make_shared<Aws::DataFlow::StatusMonitor>();

  // Create a multi status condition to trigger on network status and file status
  auto multi_status_condition_monitor =
      std::make_shared<Aws::DataFlow::MultiStatusConditionMonitor>();
  multi_status_condition_monitor->addStatusMonitor(file_monitor);

  // Add the file monitor to the file manager to get notifications
  file_manager->setStatusMonitor(file_monitor);

  // Create a file upload manager to handle uploading a file.
  auto file_upload_manager =
      std::make_shared<Aws::FileManagement::FileUploadStreamer<T>>(
          multi_status_condition_monitor,
          file_manager,
          file_manager_options.batch_size);
  return file_upload_manager;
}

}  // namespace FileManagement
}  // namespace Aws

