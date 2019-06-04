
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

template<typename T>
class ObservableObject { // todo think about extending std::atomic
public:
    ObservableObject<T>(const T initialValue) {
      value_.store(initialValue);
    }
    virtual ~ObservableObject<T>() {
      clearListeners();
    }
    virtual T getValue() {
      return value_.load();
    }

    virtual void setValue(const T &v) {
      // todo validate value before storing
      value_.store(v);
      // todo if validated then broadcast
      broadcastToListeners(v);
    }
    virtual void addListener(const std::function<void(const T&)> & listener) {
      std::lock_guard<std::mutex> lk(listener_mutex_);
      listeners_.push_back(listener);
    }

    virtual void clearListeners() {
      listeners_.clear();
    }

protected:

    virtual void broadcastToListeners(const T &currentValue) {
      std::lock_guard<std::mutex> lk(listener_mutex_);

      auto it = listeners_.begin();
      while (it != listeners_.end()) {
        try {
          auto callback = *it;  // currently all listeners will block each other
          callback(currentValue);
        } catch(...) {
          //something bad happened, remove the faulty listener
          it = listeners_.erase(it);
        }
      }
    }
    // todo validate

private:
    std::mutex listener_mutex_;
    std::atomic<T> value_;
    std::list<std::function<void(T)>> listeners_;
    // todo can have a list of validators
};
