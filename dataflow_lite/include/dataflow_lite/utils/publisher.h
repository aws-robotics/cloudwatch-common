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
#include <iostream>

#include <dataflow_lite/utils/observable_object.h>
#include <dataflow_lite/utils/service.h>

#include <file_management/file_upload/file_upload_task.h>

/**
 * State of the publisher representing the last publisher attempt status.
 */
enum PublisherState {
    UNKNOWN, // constructed
    CONNECTED, //configured, ready to receive data
    NOT_CONNECTED, //unable to send data
};

/**
 * Base class implementing the Aws::FileManagement::IPublisher interface.
 *
 * Overriding classes should provide the mechanisms to initialize / configure their respective components.
 *
 * @tparam T the type to publish
 */
template <typename T>
class Publisher : public Aws::FileManagement::IPublisher<T>, public Service
{

public:

    Publisher() : publisher_state_(UNKNOWN) {
      publish_successes_.store(0);
      publish_attempts_.store(0);
      last_publish_duration_.store(std::chrono::milliseconds(0));
    }

    virtual ~Publisher() = default;

    /**
     * Return the current state of the publisher.
     *
     * @return
     */
    PublisherState getPublisherState()
    {
      return publisher_state_.getValue();
    }

    /**
     * Attempt to publish data to CloudWatch.
     *
     * @param data the data to publish
     * @return true if the data was successfully published, false otherwise
     */
    Aws::FileManagement::UploadStatus attemptPublish(T &data) override
    {

      publish_attempts_++;

      auto start = std::chrono::high_resolution_clock::now();
      bool b = publishData(data); // always at least try
      last_publish_duration_.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start));

      publisher_state_.setValue(b ? CONNECTED : NOT_CONNECTED);
      if (b) {
        publish_successes_++;
        return Aws::FileManagement::UploadStatus::SUCCESS;
      }

      return Aws::FileManagement::UploadStatus::FAIL;
    }

    /**
     * Return true if this publisher can send data to CloudWatch, false otherwise.
     * @return
     */
    bool canPublish(){
      auto current_state = publisher_state_.getValue();
      return current_state == UNKNOWN || current_state == CONNECTED; //at least try once when configured
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
     * Return the time taken for the last publish attempt
     *
     * @return std::chrono::milliseconds the last publish attempt duration
     */
    std::chrono::milliseconds getLastPublishDuration() {
      return last_publish_duration_.load();
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
     * Actual publishing mechanism implemented by the agent.
     * @param data
     * @return
     */
    virtual bool publishData(T &data) = 0;

private:
    ObservableObject<PublisherState> publisher_state_;
    std::atomic<int> publish_successes_;
    std::atomic<int> publish_attempts_;
    std::atomic<std::chrono::milliseconds> last_publish_duration_;
};