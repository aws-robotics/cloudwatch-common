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


#include <cloudwatch_logs_common/log_manager.h>
#include <cloudwatch_logs_common/log_manager_factory.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/cloudwatch_facade.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_management_factory.h>
#include <cloudwatch_logs_common/dataflow/dataflow.h>

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

  auto queue_monitor =
      std::make_shared<Aws::DataFlow::QueueMonitor<TaskPtr<LogType>>>();
  auto file_upload_streamer =
      Aws::FileManagement::createFileUploadStreamer<LogType>(file_manager);

  file_upload_streamer->addStatusMonitor(network_monitor);
  // Create an observed queue to trigger a publish when data is available
  auto file_data_queue =
      std::make_shared<TaskObservedQueue<LogType>>();

  auto stream_data_queue =
    std::make_shared<TaskObservedQueue<LogType>>();

  if (CW_LOGS_SUCCEEDED != publisher->StartPublisherThread()) {
    AWS_LOG_FATAL(
      __func__,
      "Log publisher failed to start a publisher thread, the publisher thread is set to null");
    return nullptr;
  }

  auto log_manager = std::make_shared<LogManager>(publisher);
  log_manager->SetFileUploadStreamer(file_upload_streamer);
  log_manager->SetLogFileManager(file_manager);

  // @todo(rddesmond)  the following
  *file_upload_streamer >> file_data_queue >> Aws::DataFlow::LOWEST_PRIORITY >> queue_monitor;
  *log_manager >> stream_data_queue >> Aws::DataFlow::HIGHEST_PRIORITY >> queue_monitor;
  // queue_monitor >> publisher;

  file_upload_streamer->start();
  return log_manager;
}
