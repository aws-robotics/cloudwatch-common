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
#include <file_management/file_upload/file_manager.h>
#include <file_management/file_upload/file_upload_task.h>
#include <dataflow_lite/utils/observable_object.h>
#include <dataflow_lite/utils/service.h>

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
    publish_successes_.store(0);
    publish_attempts_.store(0);
  }

  virtual ~Publisher() = default;

  /**
   * Return the current state of the publisher.
   * @return
   */
  PublisherState getPublisherState()
  {
    return publisher_state_.getValue();
  }

  /**
   * Attempt to publish data to CloudWatch.
   * @param data
   * @return
   */
  UploadStatus attemptPublish(T &data) override
  {
    //todo this should probably be locked
    // TODO time this function, store the average
    // todo lock?
    publish_attempts_++;
    bool b = publishData(data); // always at least try
    publisher_state_.setValue(b ? CONNECTED : NOT_CONNECTED);
    if (b) {
      publish_successes_++;
      return UploadStatus::SUCCESS;
    }
    return UploadStatus::FAIL;
  }

  /**
   * Return true if this publisher can send data to CloudWatch, false otherwise.
   * @return
   */
  bool canPublish(){
    auto curent_state = publisher_state_.getValue();
    return curent_state == UNKNOWN || curent_state == CONNECTED; //at least try once when configured
  }

  /**
   * Return the number of publish successes
   * @return
   */
  int getPublishSuccesses() {
    return publish_successes_.load();
  }

  /**
   * Return the number of attempts made to publish
   * @return
   */
  int getPublishAttempts() {
    return publish_attempts_.load();
  }

  /**
   * Calculate and return the success rate of this publisher.
   * @return the number of sucesses divided by the number of attempts
   */
  float getPublishSuccessPercentage() {
    int attempts = publish_attempts_.load();
    if (attempts == 0) {
      return 0;
    }
    int successes = publish_successes_.load();
    return (float) successes / (float) attempts * 100.0f;
  }

  /**
   * Provide the regristration mechanism for ObservableObject (in this case PublisherState) changes.
   *
   * @param listener the PublisherState listener
   * @return true if the listener was added, false otherwise
   */
  virtual void addPublisherStateListener(const std::function<void(const PublisherState&)> & listener) {
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
  std::atomic<int> publish_successes_;
  std::atomic<int> publish_attempts_;

  //todo time last published?
};

}  // namespace CloudWatchLogs
}  // namespace AWS
