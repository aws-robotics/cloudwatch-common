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

#include <atomic>
#include <chrono>
#include <list>
#include <functional>
#include <aws/logs/model/PutLogEventsRequest.h>
#include <aws/core/Aws.h>
#include <aws/logs/CloudWatchLogsClient.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_upload_task.h>
#include "cloudwatch_logs_common/utils/observable_object.h"
#include <cloudwatch_logs_common/utils/service.h>

using namespace Aws::FileManagement;

namespace Aws {
namespace CloudWatchLogs { //todo should be cloudwatch (generic)

enum PublisherState {
    UNKNOWN, // constructed
    CONNECTED, //configured, ready to receive data
    NOT_CONNECTED, //unable to send data
};

template <typename T>
class Publisher : public IPublisher<T>, public Service
{

public:

  Publisher() : publisher_state_(UNKNOWN) {
    published_count_ = 0; //todo publish attempts
  }
  /**
   * Return the current state of the publisher.
   * @return
   */
  inline PublisherState getPublisherState()
    {
      return publisher_state_.getValue();
    }

  /**
   * Attempt to publish data to CloudWatch.
   * @param data
   * @return
   */
  inline UploadStatus attemptPublish(T &data) override
  {
    bool b = publishData(data); // always at least try
    publisher_state_.setValue(b ? CONNECTED : NOT_CONNECTED);
    if (b) {
      published_count_++;
      return UploadStatus::SUCCESS;
    }
    UploadStatus::FAIL;
  }
  /**
   * Return true if this publisher can send data to CloudWatch, false otherwise.
   * @return
   */
  inline bool canPublish(){
    auto curent_state = publisher_state_.getValue();
    return curent_state == UNKNOWN || curent_state == CONNECTED; //at least try once when configured
  }

  int getPublishedCount() {
    return published_count_; //todo atomic?
  }

  virtual bool addPublisherStateListener(const std::function<void(const PublisherState&)> & listener) {
    publisher_state_.addListener(listener);
  }

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
  ObservableObject<PublisherState> publisher_state_;
  int published_count_;
  //todo time last published?
};

}  // namespace CloudWatchLogs
}  // namespace AWS
