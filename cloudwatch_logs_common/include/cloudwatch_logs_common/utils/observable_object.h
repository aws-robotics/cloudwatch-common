
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
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>

/**
 * Class used as an atomic container of type T. Provides a listener registration and
 * broadcast mechanism for this container's updates.
 *
 * @tparam T
 */
template<typename T>
class ObservableObject { // todo think about extending std::atomic
public:
    /**
     *
     * @param initialValue
     */
    ObservableObject<T>(const T initialValue) {
      value_.store(initialValue);
    }
    /**
     *
     */
    virtual ~ObservableObject<T>() {
      clearListeners();
    }
    /**
     * Get the current value
     * @return the current value
     */
    virtual T getValue() {
      return value_.load();
    }

    /**
     * Set the current value
     * @param v the value to set
     */
    virtual void setValue(const T &v) {

      // todo: we should set the value iff the value changed

      // todo validate value before storing
      value_.store(v);
      // todo if validated then broadcast
      {
        std::lock_guard<std::recursive_mutex> lk(listener_mutex_);
        broadcastToListeners(v); // todo flag to broadcast on a new thread (if so desired)
      }
    }
    /**
     * Add a listener that will be called when the current value changes. Note: any listener
     * that throws an exception will be removed from the broadcast list.
     * @param listener
     */
    virtual void addListener(const std::function<void(const T&)> & listener) {
      std::lock_guard<std::recursive_mutex> lk(listener_mutex_);
      listeners_.push_back(listener);
    }

    /**
     * Clear all active listeners
     */
    virtual void clearListeners() {
      std::lock_guard<std::recursive_mutex> lk(listener_mutex_);
      listeners_.clear();
    }

    /**
     * Get the current number of listeners
     * @return
     */
    virtual int getNumberOfListeners() {
      return listeners_.size();
    }

protected:

    /**
     * Broadcast value updates to all registered listeners. Removes faulty listeners
     * (if an exception is thrown).
     *
     * @param currentValue
     */
    virtual void broadcastToListeners(const T &currentValue) {
      std::lock_guard<std::recursive_mutex> lk(listener_mutex_);

      for (auto i = listeners_.begin(); i != listeners_.end();) {
        try {
          auto callback = *i;  // currently all listeners will block each other
          callback(currentValue);
          ++i;
        } catch(...) {
          //something bad happened, remove the faulty listener
          i = listeners_.erase(i);
        }
      }
    }

    // todo validate

private:
    std::recursive_mutex listener_mutex_;
    std::atomic<T> value_;
    std::list<std::function<void(T)>> listeners_;
    // todo can have a list of validators
};
