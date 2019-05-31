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

#include <atomic>
#include <list>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <aws/core/Aws.h>
#include <aws/logs/CloudWatchLogsClient.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>

using namespace Aws::FileManagement;

namespace Aws {
namespace CloudWatchLogs { //todo should be cloudwatch (generic)

enum PublisherState {
    CREATED, // constructed
    CONNECTED, //configured, ready to receive data
    NOT_CONNECTED, //unable to send data
};

template <typename T>
class Publisher : public IPublisher<T> //todo extend service
{

public:

  Publisher() {
    current_state_.store(CREATED);
  }
  /**
   * Return the current state of the publisher.
   * @return
   */
  inline PublisherState getPublisherState()
    {
      return current_state_.load();
    }

  /**
   * Attempt to publish data to CloudWatch.
   * @param data
   * @return
   */
  inline UploadStatus attemptPublish(T &data)
  {
    bool b = publishData(data);
    current_state_.store(b ? CONNECTED : NOT_CONNECTED);
    return b ? UploadStatus::SUCCESS : UploadStatus::FAIL;
  }
  /**
   * Return true if this publisher can send data to CloudWatch, false otherwise.
   * @return
   */
  inline bool canPublish(){
    auto curent_state = current_state_.load();
    return curent_state == CREATED || curent_state == CONNECTED; //at least try once when configured
  }

  virtual bool initialize() = 0;
  virtual bool shutdown() = 0;


protected:
  /**
   * Configuration for the publishing agent that needs to be called before publishing.
   * @return
   */
  virtual bool configure() = 0;
  /**
   * Actual publishing mechanism implemented by the agent.
   * @param data
   * @return
   */
  virtual bool publishData(T &data) = 0;

private:
  std::atomic<PublisherState> current_state_;
};

}  // namespace CloudWatchLogs
}  // namespace AWS
