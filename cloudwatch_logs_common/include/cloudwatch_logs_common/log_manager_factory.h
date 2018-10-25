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

#pragma once

#include <cloudwatch_logs_common/log_manager.h>
#include <cloudwatch_logs_common/log_publisher.h>

namespace Aws {
namespace CloudWatchLogs {

class LogManagerFactory
{
public:
  LogManagerFactory() = default;
  ~LogManagerFactory() = default;

  /**
   *  @brief Creates a new LogManager object
   *  Factory method used to create a new LogManager object, along with a creating and starting a
   * LogPublisher for use with the LogManager.
   *
   *  @param client_config The client configuration to use when creating the CloudWatch clients
   *  @param options The options used for the AWS SDK when creating and publishing with a CloudWatch
   * client
   *
   *  @return An instance of LogManager
   */
  std::shared_ptr<LogManager> CreateLogManager(
    const std::string & log_group, const std::string & log_stream,
    const Aws::Client::ClientConfiguration & client_config, const Aws::SDKOptions & sdk_options);

private:
  /**
   * Block copy constructor and assignment operator for Factory object.
   */
  LogManagerFactory(const LogManagerFactory &) = delete;
  LogManagerFactory & operator=(const LogManagerFactory &) = delete;
};

}  // namespace CloudWatchLogs
}  // namespace Aws
