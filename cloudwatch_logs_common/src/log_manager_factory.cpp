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

#include <list>
#include <string>
#include <cloudwatch_logs_common/log_batcher.h>
#include <cloudwatch_logs_common/log_manager_factory.h>
#include <cloudwatch_logs_common/log_publisher.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/cloudwatch_facade.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_management_factory.h>
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>

#include <cloudwatch_logs_common/dataflow/dataflow.h>

#include <cloudwatch_logs_common/log_service.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;


// why not do all of this in the setup of LogService? it has ownership of everything
std::shared_ptr<LogService> LogManagerFactory::CreateLogManager(
  const std::string & log_group,
  const std::string & log_stream,
  const Aws::Client::ClientConfiguration & client_config,
  const Aws::SDKOptions & sdk_options)
{
  auto cloudwatch_facade = std::make_shared<Aws::CloudWatchLogs::Utils::CloudWatchFacade>(client_config);
  auto file_manager= std::make_shared<LogFileManager>();

  auto publisher = std::make_shared<LogPublisher>(log_group, log_stream, cloudwatch_facade, sdk_options);
  // todo this should be a service to subscribe and status part of the publisher, too much is dependent in File utils / tasks and seems brittle

  auto queue_monitor =
      std::make_shared<Aws::DataFlow::QueueMonitor<TaskPtr<LogType>>>();
  auto file_upload_streamer =
      Aws::FileManagement::createFileUploadStreamer<LogType>(file_manager);

  // connect publisher state changes to the File Streamer
  publisher->addPublisherStateListener(std::bind(&FileUploadStreamer<LogType>::onPublisherStateChange, file_upload_streamer, std::placeholders::_1));

  // Create an observed queue to trigger a publish when data is available
  auto file_data_queue =
      std::make_shared<TaskObservedQueue<LogType>>();

  auto stream_data_queue =
    std::make_shared<TaskObservedQueue<LogType>>();

  auto log_batcher = std::make_shared<LogBatcher>();

  *file_upload_streamer >> file_data_queue >> Aws::DataFlow::LOWEST_PRIORITY >> queue_monitor;
  *log_batcher >> stream_data_queue >> Aws::DataFlow::HIGHEST_PRIORITY >> queue_monitor;

  auto ls = std::make_shared<LogService>(file_upload_streamer, publisher, log_batcher);
  queue_monitor >> *ls;
  ls->start(); // todo should we allow the user to start?

  return ls;
}
