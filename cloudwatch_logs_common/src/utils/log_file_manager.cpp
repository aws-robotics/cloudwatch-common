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



#include <fstream>
#include <iostream>
#include <memory>	
#include <mutex>
#include <queue>
#include <tuple>

#include "cloudwatch_logs_common/utils/log_file_manager.h"
#include "file_management/file_upload/file_manager_strategy.h"
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <cloudwatch_logs_common/definitions/definitions.h>

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {


FileObject<LogCollection> LogFileManager::readBatch(
  size_t batch_size)
{
  FileManagement::DataToken data_token;
  AWS_LOG_INFO(__func__, "Reading Logbatch");
	
  std::priority_queue<std::tuple<Timestamp, std::string, FileManagement::DataToken>> pq;
  for (size_t i = 0; i < batch_size; ++i) {
    std::string line;
    if (!file_manager_strategy_->isDataAvailable()) {
      break;
    }
    data_token = read(line);
    Aws::String aws_line(line.c_str());
    Aws::Utils::Json::JsonValue value(aws_line);
    Aws::CloudWatchLogs::Model::InputLogEvent input_event(value);
    pq.push(std::make_tuple(input_event.GetTimestamp(), line, data_token));
  }
  
  latestTime = std::get<0>(pq.top());
  LogCollection log_data;
  std::list<FileManagement::DataToken> data_tokens;
  while(!pq.empty()){
    Timestamp curTime = std::get<0>(pq.top());
    std::string line = std::get<1>(pq.top());
    FileManagement::DataToken new_data_token = std::get<2>(pq.top());
    if(latestTime - curTime < ONE_DAY_IN_MILLISEC){
      Aws::String aws_line(line.c_str());
      Aws::Utils::Json::JsonValue value(aws_line);
      Aws::CloudWatchLogs::Model::InputLogEvent input_event(value);
      log_data.push_front(input_event);
      data_tokens.push_back(new_data_token);
    }
    else if(file_manager_strategy_->isDeleteStaleData() && latestTime - curTime > TWO_WEEK_IN_MILLISEC){
      {
        std::lock_guard<std::mutex> lock(active_delete_stale_data_mutex_);
        stale_data_.push_back(new_data_token);
      }
    }
    pq.pop();
  }

  if(batch_size != log_data.size()){
    AWS_LOG_WARN(__func__, "%d logs were not batched since the time"
      " difference was > 24 hours. Will try again in a separate batch."
      , batch_size - log_data.size()
      );
  }

  FileObject<LogCollection> file_object;
  file_object.batch_data = log_data;
  file_object.batch_size = log_data.size();
  file_object.data_tokens = data_tokens;
  return file_object;
}

void LogFileManager::write(const LogCollection & data) {
  for (const Model::InputLogEvent &log: data) {
    auto aws_str = log.Jsonize().View().WriteCompact();
    std::string str(aws_str.c_str());
    file_manager_strategy_->write(str);
  }
  if (FileManager::file_status_monitor_) {
    AWS_LOG_INFO(__func__,
                 "Set file status available");
    FileManager::file_status_monitor_->setStatus(Aws::DataFlow::Status::AVAILABLE);
  }
}

}  // namespace Utils
}  // namespace CloudWatchLogs
}  // namespace Aws
