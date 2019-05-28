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

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/logs/CloudWatchLogsErrors.h>
#include <aws/logs/model/CreateLogGroupRequest.h>
#include <aws/logs/model/CreateLogStreamRequest.h>
#include <aws/logs/model/DescribeLogGroupsRequest.h>
#include <aws/logs/model/DescribeLogGroupsResult.h>
#include <aws/logs/model/DescribeLogStreamsRequest.h>
#include <aws/logs/model/DescribeLogStreamsResult.h>
#include <aws/logs/model/InputLogEvent.h>
#include <aws/logs/model/LogStream.h>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/utils/cloudwatch_facade.h>

using namespace Aws::CloudWatchLogs::Utils;

constexpr uint16_t kMaxLogsPerRequest = 100;

CloudWatchFacade::CloudWatchFacade(const Aws::Client::ClientConfiguration & client_config)
{
  this->cw_client_ = std::make_unique<Aws::CloudWatchLogs::CloudWatchLogsClient>(client_config);
}

CloudWatchFacade::CloudWatchFacade(std::unique_ptr<Aws::CloudWatchLogs::CloudWatchLogsClient> cw_client)
: cw_client_(std::move(cw_client))
{
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CloudWatchFacade::SendLogsRequest(
  const Aws::CloudWatchLogs::Model::PutLogEventsRequest & request, Aws::String & next_token)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;
  auto response = this->cw_client_->PutLogEvents(request);
  if (!response.IsSuccess()) {
    status = CW_LOGS_FAILED;
    AWS_LOGSTREAM_ERROR(__func__, "Send log request failed due to: "
                                    << response.GetError().GetMessage() << ", with error code: "
                                    << static_cast<int>(response.GetError().GetErrorType()));
  } else {
    AWS_LOG_DEBUG(__func__, "Setting the sequence token to use for the next send log request.");
    next_token = response.GetResult().GetNextSequenceToken();
  }

  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CloudWatchFacade::SendLogsToCloudWatch(
  Aws::String & next_token, const std::string & log_group, const std::string & log_stream,
  std::list<Aws::CloudWatchLogs::Model::InputLogEvent> * logs)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;
  Aws::Vector<Aws::CloudWatchLogs::Model::InputLogEvent> events;

  if (nullptr == logs) {
    status = CW_LOGS_NULL_PARAMETER;
    AWS_LOGSTREAM_WARN(__func__, "Internal error occurred, error code: "
                                   << status
                                   << ", quit attempting to send logs to CloudWatch in Log Group: "
                                   << log_group << " Log Stream: " << log_stream << ".");
    return status;
  } else if (logs->empty()) {
    status = CW_LOGS_EMPTY_PARAMETER;
    AWS_LOGSTREAM_WARN(__func__,
                       "Log set is empty, quit attempting to send logs to CloudWatch in Log Group: "
                         << log_group << " Log Stream: " << log_stream << ".");
    return status;
  }

  Aws::CloudWatchLogs::Model::PutLogEventsRequest request;
  request.SetLogGroupName(log_group.c_str());
  request.SetLogStreamName(log_stream.c_str());

  if (next_token != "") {
    request.SetSequenceToken(next_token);
  }

  for (auto it = logs->begin(); it != logs->end(); ++it) {
    events.push_back(*it);
    if (events.size() >= kMaxLogsPerRequest) {
      request.SetLogEvents(events);
      status = SendLogsRequest(request, next_token);
      events.clear();
      request.SetSequenceToken(next_token);
    }
    if (CW_LOGS_SUCCEEDED != status) {
      AWS_LOGSTREAM_ERROR(__func__, "Failed to send to CloudWatch in Log Group: "
                                      << log_group << " Log Stream: " << log_stream
                                      << " with error code: " << status
                                      << ". Dropping this batch of logs.");
      return status;
    } else {
      AWS_LOGSTREAM_INFO(__func__,
                         "A batch of log was successfully sent to CloudWatch in Log Group: "
                           << log_group << " Log Stream: " << log_stream << ".");
    }
  }

  if (!events.empty()) {
    request.SetLogEvents(events);
    status = SendLogsRequest(request, next_token);
    if (CW_LOGS_SUCCEEDED != status) {
      AWS_LOGSTREAM_ERROR(__func__, "Failed to send to CloudWatch in Log Group: "
                                      << log_group << " Log Stream: " << log_stream
                                      << " with error code: " << status
                                      << ". Dropping the last bit of this batch of logs.");
    } else {
      AWS_LOGSTREAM_INFO(__func__, "All logs were successfully sent to CloudWatch in Log Group: "
                                     << log_group << " Log Stream: " << log_stream << ".");
    }
  }

  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CloudWatchFacade::CreateLogGroup(
  const std::string & log_group)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;

  Aws::CloudWatchLogs::Model::CreateLogGroupRequest log_group_request;
  log_group_request.SetLogGroupName(log_group.c_str());

  const auto & response = this->cw_client_->CreateLogGroup(log_group_request);
  if (!response.IsSuccess()) {
    AWS_LOGSTREAM_ERROR(
      __func__, "Failed to create Log Group :"
                  << log_group << " due to: " << response.GetError().GetMessage()
                  << ", with error code: " << static_cast<int>(response.GetError().GetErrorType()));
    if (response.GetError().GetErrorType() ==
        Aws::CloudWatchLogs::CloudWatchLogsErrors::RESOURCE_ALREADY_EXISTS) {
      status = CW_LOGS_LOG_GROUP_ALREADY_EXISTS;
    } else {
      status = CW_LOGS_CREATE_LOG_GROUP_FAILED;
    }
  }

  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CloudWatchFacade::CheckLogGroupExists(
  const std::string & log_group)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_LOG_GROUP_NOT_FOUND;
  Aws::CloudWatchLogs::Model::DescribeLogGroupsRequest describe_log_group_request;
  Aws::String next_token;

  describe_log_group_request.SetLogGroupNamePrefix(log_group.c_str());

  while (CW_LOGS_LOG_GROUP_NOT_FOUND == status) {
    if (next_token.size() != 0) {
      describe_log_group_request.SetNextToken(next_token);
    }

    const auto & response = this->cw_client_->DescribeLogGroups(describe_log_group_request);
    if (!response.IsSuccess()) {
      status = CW_LOGS_FAILED;
      AWS_LOGSTREAM_WARN(__func__, "Request to check if log group named "
              << log_group << " exists failed. Error message: "
              << response.GetError().GetMessage() << ", with error code: "
              << static_cast<int>(response.GetError().GetErrorType()));
      break;
    }

    auto & log_group_list = response.GetResult().GetLogGroups();
    next_token = response.GetResult().GetNextToken();

    for (auto it = log_group_list.begin(); it != log_group_list.end(); ++it) {
      Aws::CloudWatchLogs::Model::LogGroup curr_log_group = *it;
      if (curr_log_group.GetLogGroupName().c_str() == log_group) {
        AWS_LOGSTREAM_DEBUG(__func__, "Found Log Group named: " << log_group << ".");
        status = CW_LOGS_SUCCEEDED;
        break;
      }
    }
    if (CW_LOGS_SUCCEEDED != status && next_token.size() == 0) {
      AWS_LOGSTREAM_INFO(__func__, "Failed to find Log Group named: " << log_group << ".");
      break;
    }
  }

  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CloudWatchFacade::CreateLogStream(
  const std::string & log_group, const std::string & log_stream)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;

  Aws::CloudWatchLogs::Model::CreateLogStreamRequest log_stream_request;
  log_stream_request.SetLogGroupName(log_group.c_str());
  log_stream_request.SetLogStreamName(log_stream.c_str());

  const auto & response = this->cw_client_->CreateLogStream(log_stream_request);
  if (!response.IsSuccess()) {
    AWS_LOGSTREAM_ERROR(__func__, "Failed to create Log Stream :"
                                    << log_stream << " in Log Group :" << log_group << " due to: "
                                    << response.GetError().GetMessage() << ", with error code: "
                                    << static_cast<int>(response.GetError().GetErrorType()));
    if (response.GetError().GetErrorType() ==
        Aws::CloudWatchLogs::CloudWatchLogsErrors::RESOURCE_ALREADY_EXISTS) {
      status = CW_LOGS_LOG_STREAM_ALREADY_EXISTS;
    } else {
      status = CW_LOGS_CREATE_LOG_STREAM_FAILED;
    }
  }

  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CloudWatchFacade::CheckLogStreamExists(
  const std::string & log_group, const std::string & log_stream,
  Aws::CloudWatchLogs::Model::LogStream * log_stream_object)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_LOG_STREAM_NOT_FOUND;
  Aws::CloudWatchLogs::Model::DescribeLogStreamsRequest describe_log_stream_request;
  Aws::String next_token;

  describe_log_stream_request.SetLogGroupName(log_group.c_str());
  describe_log_stream_request.SetLogStreamNamePrefix(log_stream.c_str());

  while (CW_LOGS_LOG_STREAM_NOT_FOUND == status) {
    if (next_token.size() != 0) {
      describe_log_stream_request.SetNextToken(next_token);
    }

    const auto & response = this->cw_client_->DescribeLogStreams(describe_log_stream_request);
    if (!response.IsSuccess()) {
      status = CW_LOGS_FAILED;
      AWS_LOGSTREAM_WARN(
              __func__, "Request to check if log stream named "
                      << log_stream << " exists in log group named: " << log_group
                      << ". Error message: " << response.GetError().GetMessage()
                      << ", with error code: " << static_cast<int>(response.GetError().GetErrorType()));
      break;
    }

    auto & log_stream_list = response.GetResult().GetLogStreams();
    next_token = response.GetResult().GetNextToken();

    for (auto it = log_stream_list.begin(); it != log_stream_list.end(); ++it) {
      Aws::CloudWatchLogs::Model::LogStream curr_log_stream = *it;
      if (curr_log_stream.GetLogStreamName().c_str() == log_stream) {
        AWS_LOGSTREAM_DEBUG(__func__, "Found Log Stream named: " << log_stream << " in Log Group :"
                                                                 << log_group << ".");
        if (nullptr != log_stream_object) {
          *log_stream_object = curr_log_stream;
        }
        status = CW_LOGS_SUCCEEDED;
        break;
      }
    }
    if (CW_LOGS_SUCCEEDED != status && next_token.size() == 0) {
      AWS_LOGSTREAM_INFO(__func__, "Failed to find Log Stream named: " << log_stream
                                                                       << ".");
      break;
    }
  }

  return status;
}

Aws::CloudWatchLogs::ROSCloudWatchLogsErrors CloudWatchFacade::GetLogStreamToken(
  const std::string & log_group, const std::string & log_stream, Aws::String & next_token)
{
  Aws::CloudWatchLogs::ROSCloudWatchLogsErrors status = CW_LOGS_SUCCEEDED;
  Aws::CloudWatchLogs::Model::LogStream log_stream_object;
  if (CW_LOGS_SUCCEEDED != CheckLogStreamExists(log_group, log_stream, &log_stream_object)) {
    status = CW_LOGS_LOG_STREAM_NOT_FOUND;
    AWS_LOGSTREAM_ERROR(__func__, "Failed to obtain sequence token due to Log Stream: "
                                    << log_stream << " in Log Group :" << log_group
                                    << " doesn't exist.");
  } else {
    next_token = log_stream_object.GetUploadSequenceToken();
  }

  return status;
}
