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

#include <aws/core/utils/logging/LogMacros.h>
#include <cloudwatch_logs_common/log_manager.h>
#include <cloudwatch_logs_common/log_manager_factory.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/cloudwatch_facade.h>
#include <cloudwatch_logs_common/utils/file_manager.h>
#include <cloudwatch_logs_common/file_upload/status_monitor.h>
#include <cloudwatch_logs_common/file_upload/file_upload_manager.h>
#include <cloudwatch_logs_common/file_upload/observed_queue.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;

std::shared_ptr<LogManager> LogManagerFactory::CreateLogManager(
  const std::string & log_group,
  const std::string & log_stream,
  const Aws::Client::ClientConfiguration & client_config,
  const Aws::SDKOptions & sdk_options)
{
  Aws::InitAPI(sdk_options);
  // Streamer system
  auto cloudwatch_facade =
    std::make_shared<Aws::CloudWatchLogs::Utils::CloudWatchFacade>(client_config);
  auto file_manager=
    std::make_shared<LogFileManager>();
  auto publisher = std::make_shared<LogPublisher>(log_group, log_stream, cloudwatch_facade);
  publisher->SetLogFileManager(file_manager);
  auto network_monitor=
      std::make_shared<Aws::FileManagement::StatusMonitor>();
  publisher->SetNetworkMonitor(network_monitor);

  // File Management system
  // Create a file monitor to get notified if a file is ready to be read
  auto file_monitor=
      std::make_shared<Aws::FileManagement::StatusMonitor>();

  // Create a multi status condition to trigger on network status and file status
  auto multi_status_condition_monitor =
      std::make_shared<Aws::FileManagement::MultiStatusConditionMonitor>();
  multi_status_condition_monitor->addStatusMonitor(network_monitor);
  multi_status_condition_monitor->addStatusMonitor(file_monitor);

  // Add the file monitor to the file manager to get notifications
  file_manager->addFileStatusMonitor(file_monitor);

  // Create an observed queue to trigger a publish when data is available
  auto observed_queue =
      std::make_shared<ObservedQueue<Task<LogType>>>();

  // Create a file upload manager to handle uploading a file.
  auto file_upload_manager =
    std::make_shared<Aws::FileManagement::FileUploadManager<LogType>>(
        multi_status_condition_monitor,
        file_manager,
        observed_queue,
        10);

  if (CW_LOGS_SUCCEEDED != publisher->StartPublisherThread()) {
    AWS_LOG_FATAL(
      __func__,
      "Log publisher failed to start a publisher thread, the publisher thread is set to null");
    return nullptr;
  }
  return std::make_shared<LogManager>(publisher);
}
