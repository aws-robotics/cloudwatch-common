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
#include <condition_variable> // std::condition_variable
#include <mutex>              // std::mutex, std::unique_lock
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace Aws {
namespace DataFlow {

/**
 * Status.
 */
enum Status : uint {
  UNAVAILABLE = 0,
  AVAILABLE
};

class MultiStatusConditionMonitor;

/**
 * Handle a single set/get status. Notify the multi_status_condition when the status is changed.
 */
class StatusMonitor {
public:
  virtual ~StatusMonitor() = default;
  void SetStatus(const Status &status);

  inline Status GetStatus() const {
    return status_;
  }
private:
  friend MultiStatusConditionMonitor;
  inline void SetStatusObserver(MultiStatusConditionMonitor *multi_status_cond) {
    multi_status_cond_ = multi_status_cond;
  }
  Status status_ = UNAVAILABLE;
  MultiStatusConditionMonitor *multi_status_cond_ = nullptr;
};

class MaskFactory {
public:

  /**
   * Generate a new mask that is no longer in use by this factor.
   *
   * @return mask
   */
  uint64_t GetNewMask() {
    uint64_t current_mask = 0, new_mask;
    size_t shift = 0;
    while (current_mask == 0) {
      new_mask = static_cast<uint64_t>(1) << shift++;
      current_mask = (collective_mask_ & new_mask) == 0u ? new_mask : 0;
      if (shift > kMaxSize) {
        throw std::overflow_error("No more masks available");
      }
    }
    collective_mask_ |= current_mask;
    return current_mask;
  }

  /**
   * Remove a mask from use.
   *
   * @param mask to remove
   */
  void RemoveMask(uint64_t mask) {
    collective_mask_ &= ~mask;
  }

  /**
   * @return Get the collective mask.
   */
  uint64_t GetCollectiveMask() const {
    return collective_mask_;
  }
private:
  static constexpr size_t kMaxSize = sizeof(uint64_t) * 8;
  uint64_t collective_mask_ = 0;
};

class ThreadMonitor {
public:
  virtual ~ThreadMonitor() = default;
  void WaitForWork();
  std::cv_status WaitForWork(const std::chrono::microseconds &duration);
  void Notify();
private:
  virtual bool HasWork() = 0;
  std::mutex idle_mutex_;
  std::condition_variable work_condition_;
};

/**
 * Multi Status Condition Monitor listens to N StatusMonitors and determines whether to trigger wait for work
 * based on the HasWork() function.
 */
class MultiStatusConditionMonitor : public ThreadMonitor {
public:
  MultiStatusConditionMonitor() {
    mask_ = 0;
  }
  ~MultiStatusConditionMonitor() override = default;
  void AddStatusMonitor(const std::shared_ptr<StatusMonitor> & status_monitor);
protected:
  friend StatusMonitor;
  virtual void SetStatus(const Status &status, StatusMonitor *status_monitor);
  bool HasWork() override;
  MaskFactory mask_factory_;
  std::atomic<uint64_t> mask_{};
  std::unordered_map<StatusMonitor*, uint64_t> status_monitors_;
};

}  // namespace DataFlow
}  // namespace Aws
