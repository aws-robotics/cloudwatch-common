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


#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_management_factory.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;

TEST(test_file_upload_manager, create_file_upload_manager) {
  auto file_manager = std::make_shared<LogFileManager>();
  auto file_upload_manager = createFileUploadManager(
      static_cast<std::shared_ptr<FileManager<LogType>>>(file_manager));
  auto observed_queue = file_upload_manager->getObservedQueue();
  auto queue_monitor = std::make_shared<QueueMonitor<std::shared_ptr<Task<LogType>>>>();
  queue_monitor->add_queue(observed_queue);
  EXPECT_TRUE(observed_queue->empty());
  LogType log_data;
  Aws::CloudWatchLogs::Model::InputLogEvent input_event;
  input_event.SetTimestamp(0);
  input_event.SetMessage("Hello my name is foo");
  log_data.push_back(input_event);
  file_manager->uploadCompleteStatus(ROSCloudWatchLogsErrors::CW_LOGS_FAILED, log_data);
  file_upload_manager->run();
  EXPECT_FALSE(observed_queue->empty());
  auto task = queue_monitor->dequeue();
  auto data = task->getBatchData();
  task->onComplete(UploadStatus::SUCCESS);
  EXPECT_EQ("Hello my name is foo", data.front().GetMessage());
  EXPECT_FALSE(std::ifstream("/tmp/active_file.log").good());
}