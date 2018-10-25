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

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;

std::shared_ptr<LogManager> LogManagerFactory::CreateLogManager(
  const std::string & log_group, const std::string & log_stream,
  const Aws::Client::ClientConfiguration & client_config, const Aws::SDKOptions & sdk_options)
{
  Aws::InitAPI(sdk_options);
  auto cloudwatch_facade =
    std::make_shared<Aws::CloudWatchLogs::Utils::CloudWatchFacade>(client_config);
  auto publisher = std::make_shared<LogPublisher>(log_group, log_stream, cloudwatch_facade);
  if (CW_LOGS_SUCCEEDED != publisher->StartPublisherThread()) {
    AWS_LOG_FATAL(
      __func__,
      "Log publisher failed to start a publisher thread, the publisher thread is set to null");
    return nullptr;
  }
  return std::make_shared<LogManager>(publisher);
}
