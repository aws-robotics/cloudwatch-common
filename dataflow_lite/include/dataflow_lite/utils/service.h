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
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <typeinfo>

#include <dataflow_lite/utils/observable_object.h>

enum ServiceState {
    CREATED,  // created and ready to start
    STARTED,  // started and ready to use
    SHUTDOWN,  // clean shutdown, possibly restartable
};

/**
 * Map used to pretty print the ServiceState enum.
 */
static const std::map<ServiceState, std::string> kServiceStateNameMap = {{CREATED, "CREATED"}, {STARTED, "STARTED"},
                                                                         {SHUTDOWN,"SHUTDOWN"}};

/**
 * Interface that defines init, start, and shutdown methods for an implementing class to define.
 */
class Service {
public:

    Service() : state_(CREATED) {};
    virtual ~Service() = default;

    /**
     * Called to start doing work. The format overriding classes should use is the following:
     *
     *   virtual bool Start() {
     *       // do specific start logic here
     *
     *       // ensure the service state has been set to started
     *       bool b = Service::Start();
     *
     *       // return the result of Service::Start() or something else if desired
     *       return b;
     *   }
     *
     * @return
     */
    virtual bool Start() {
      state_.SetValue(STARTED);
      return true;
    }

    virtual bool start() {
      return Service::Start();
    }

    /**
     * Cleanup. Should be called before destruction. The format overriding classes should use is the following:
     *
     *   virtual bool Shutdown() {
     *       //  immediately set the shutdown state
     *       bool b = Service::Shutdown();
     *
     *       // do specific shutdown logic here
     *
     *       // return the result of Service::Shutdown() or something else if desired
     *       return b;
     *   }
     * @return
     */
    virtual bool Shutdown() {
      state_.SetValue(SHUTDOWN);
      return true;
    }

    virtual bool shutdown() {
      return Service::Shutdown();
    }

    /**
     * Return a descriptive string describing the service and it's state.
     * @return
     */
    virtual std::string GetStatusString() {
      // a more descriptive name (tag supplied on construction) would be ideal
      return typeid(this).name() + std::string(", state=") + kServiceStateNameMap.at(GetState());
    }

    /**
     * Return the current ServiceState if this service.
     * @return ServiceState
     */
    ServiceState GetState() {
      return state_.GetValue();
    }

protected:
    /**
     * Set the current state of the service. To be used by overriding classes.
     *
     * @param new_state
     */
    void SetState(ServiceState new_state) {
      state_.SetValue(new_state);
    }

private:
    /**
     * The current state of this service.
     */
    ObservableObject<ServiceState> state_;
};

/**
 * Interface that implements basic Service methods and provides a mechanism to start a thread. Any interesting work
 * in the implemented class should be implemented in the "work" method. Note: start and shutdown methods should be
 * override if the implementing class requires extra steps in either scenarios.
 */
//  consider extending the waiter interface
class RunnableService : public Service
{
public:
    RunnableService() {
      should_run_.store(false);
    }
    ~RunnableService() override = default;

    /**
     * Starts the worker thread. Should be overridden if other actions are necessary to start.
     * @return
     */
    bool Start() override {
      bool started = StartWorkerThread();
      started &= Service::Start();
      return started;
    }

    /**
     * Stops the worker thread. Should be overridden if other actions are necessary to stop.
     * @return
     */
    bool Shutdown() override {
      bool is_shutdown = Service::Shutdown();
      is_shutdown &= StopWorkerThread();
      return is_shutdown;
    }

    /**
     * Return if the RunnableService work thread is active
     * @return true if the work thread is active / running, false otherwise
     */
    virtual bool IsRunning() {
      return Service::GetState() == ServiceState::STARTED && should_run_.load();
    }

    /**
     * Wait for the work thread shutdown
     */
    void WaitForShutdown() {
      std::unique_lock <std::mutex> lck(this->mtx_);
      if (runnable_thread_.joinable()) {
        cv_.wait(lck); // todo guard against spurious wakeup, or could do while
      }
    }

    /**
     * Wait for the work thread shutdown
     * @param millis time to wait
     */
    void WaitForShutdown(std::chrono::milliseconds millis) {
      std::unique_lock <std::mutex> lck(this->mtx_);
      if (runnable_thread_.joinable()) {
        cv_.wait_for(lck, millis); // todo guard against spurious wakeup
      }
    }

    /**
     * Join the running thread if available.
     */
    void Join() {
      if (runnable_thread_.joinable()) {
        runnable_thread_.join();
      }
    }

    /**
     * Return a descriptive string describing the service and it's state.
     * @return
     */
    std::string GetStatusString() override {
      return Service::GetStatusString() + std::string(", IsRunning=") + (IsRunning()
        ? std::string("True") : std::string("False"));
    }

protected:

  /**
   * Start the worker thread if not already running.
   * @return true if the worker thread was started, false if already running
   */
  virtual bool StartWorkerThread() {
    if(!runnable_thread_.joinable()) {
      should_run_.store(true);
      runnable_thread_ = std::thread(&RunnableService::Run, this);
      return true;
    }
    return false;
  }

  /**
   * Set the should_run_ flag to false for the worker thread to exit
   * @return
   */
  virtual bool StopWorkerThread() {
    if(should_run_.load()) {
      should_run_.store(false);
      return true;
    }
    return false;
  }

  /**
   * Calls the abstract work method and notifies when shutdown was called.
   */
  virtual void Run() {
    while(should_run_.load() && ServiceState::STARTED == Service::GetState()) {
      Work();
    }
    // done, notify anyone waiting
    std::unique_lock <std::mutex> lck(this->mtx_);
    this->cv_.notify_all();
  }

  /**
   * Implement this method to do work. Note: this method is assumed to NOT block, otherwise shutdown does nothing.
   */
  virtual void Work() = 0;

private:
  std::thread runnable_thread_;
  std::atomic<bool> should_run_{};
  std::condition_variable cv_;
  mutable std::mutex mtx_;
};
