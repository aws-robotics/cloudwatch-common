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

#include <aws_common/sdk_utils/aws_error.h>

#include <atomic>
#include <mutex>

namespace Aws {
namespace CloudWatch {
namespace Metrics {
namespace Utils {

/**
 *  @brief This is a class used for sharing an object between two cooperative threads.
 *  This class provides a mechanism to share an object between two cooperative threads. It does not
 * inherently provide thread saftey via locking. Instead it relies on the two threads sharing the
 * object to cooperate on when they access the object. One thread can write the data and mark it as
 * locked. While it is locked the data can be read by the second thread. When the second thread is
 * done reading the data it can unlock the object, at which point the first thread will be able to
 * write a new value and lock it again.
 */
template <typename T>
class SharedObject
{
private:
  T data;
  std::atomic_bool data_ready;
  std::mutex m_lock;

public:
  /**
   *  @brief Creates a new SharedObject
   *  Creates a new SharedObject that will be initialized to be unlocked
   */
  SharedObject() : data_ready(false) {}
  ~SharedObject() = default;

  /**
   *  @brief Check to see if the shared object has data ready for reading.
   *  Checks if the shared object has data that's available to be read by the consumer. This
   * function is non-blocking
   *
   *  @return Returns true if the object has had data set on it
   */
  bool IsDataAvailable() { return this->data_ready.load(std::memory_order_acquire); }

  /**
   *  @brief Sets the shared data and marks it as ready for reading by the consumer
   *  This method is used to set the shared data and mark it as ready to read. This method blocks if
   * the object is locked for reading.
   *
   *  @return Returns an error code that will be Aws::AwsError::AWS_ERR_OK if the shared data is set
   * and marked ready.
   */
  Aws::AwsError SetDataAndMarkReady(T data)
  {
    std::lock_guard<std::mutex> lock(m_lock);
    Aws::AwsError status = Aws::AwsError::AWS_ERR_OK;
    if (IsDataAvailable()) {
      status = AWS_ERR_ALREADY;
    } else {
      this->data = data;
      this->data_ready.store(true, std::memory_order_release);
    }
    return status;
  }

  /**
   *  @brief Marks the data as no longer ready and unlocks the shared object
   *  This method is used to mark the shared object as unlocked. Only the secondary sharing thread
   * should call the unlock function.
   *
   *  @return Returns an error code that will be Aws::AwsError::AWS_ERR_OK if the shared object is
   * unlocked successfully.
   */
  Aws::AwsError FreeDataAndUnlock()
  {
    Aws::AwsError status = Aws::AwsError::AWS_ERR_OK;
    if (!IsDataAvailable()) {
      status = AWS_ERR_ALREADY;
    } else {
      this->data_ready.store(false, std::memory_order_release);
    }
    m_lock.unlock();
    return status;
  }

  /**
   *  @brief Gets the data for the Shared Object and marks the object as locked.
   *  This method is used to get a copy of the shared data and then mark the shared object as
   * locked. This function is blocking if the shared object is already locked
   *
   *  @param data (output) A reference to the data object that will be set to the data
   *  @return Returns an error code that will be Aws::AwsError::AWS_ERR_OK if the data was retrieved
   * and the shared object locked
   */
  Aws::AwsError GetDataAndLock(T & data)
  {
    m_lock.lock();
    if (!IsDataAvailable()) {
      m_lock.unlock();
      return AWS_ERR_FAILURE;
    }
    data = this->data;
    return Aws::AwsError::AWS_ERR_OK;
  }
};

}  // namespace Utils
}  // namespace Metrics
}  // namespace CloudWatch
}  // namespace Aws
