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

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>

//enum ServiceState {
//    INITIALIZED, // initialized and ready to start
//    STARTED,     // started and ready to use
//    RUNNING,     // actively running (thread, etc)
//    SHUTDOWN,    // clean shutdown, possibly restartable
//    TERMINATED   // failure, not restartable
//};

/**
 * Interface that defines init, start, and shutdown methods for an implementing class to define.
 */
class Service {
public:

    Service() = default;
    virtual ~Service() = default;
    /**
     * Called to start doing work.
     * @return
     */
    virtual bool start() = 0;
    /**
     * Cleanup. Should be called before destruction.
     * @return
     */
    virtual bool shutdown() = 0;

    // todo getStatusString : useful for debugging

//protected:
//    void getState() {
//      return state_.load();
//    }
//private:
//    std::atomic<ServiceState> state_; // todo use observer
};

/**
 * Interface that implements basic Service methods and provides a mechanism to start a thread. Any interesting work
 * in the implemented class should be implemented in the "work" method. Note: start and shutdown methods should be
 * override if the implementing class requires extra steps in either scenarios.
 */
 //todo consider extending the waiter interface (pipeline test)
class RunnableService : public Service {
public:
    RunnableService() {
      should_run_.store(false);
    }

    //todo
//    inline bool restart() {
//
//    }

    /**
     * Starts the worker thread. Should be overridden if other actions are necessary to start.
     * @return
     */
    inline bool start() {
      return startWorkerThread();
    }

    /**
     * Stops the worker thread. Should be overridden if other actions are necessary to stop.
     * @return
     */
    inline bool shutdown() {
      if(should_run_.load()) {
        should_run_.store(false);
        return true;
      }
      return false;
    }

    inline bool isRunning() {
      return should_run_.load(); //todo this is an overload of state, actually track state
    }

    void waitForShutdown() {
      if (runnable_thread_.joinable()) {
        std::unique_lock <std::mutex> lck(this->mtx);
        cv.wait(lck); // todo guard against spurious wakeup, or could do while
      }
    }

    void waitForShutdown(std::chrono::milliseconds millis) {
      if (runnable_thread_.joinable()) {
        std::unique_lock <std::mutex> lck(this->mtx);
        cv.wait_for(lck, millis); // todo guard against spurious wakeup
      }
    }

    /**
     * Join the running thread if available.
     */
    void join() {
      if (runnable_thread_.joinable()) {
        runnable_thread_.join();
      }
    }

protected:

  /**
   * Start the worker thread if not already running.
   * @return true if the worker thread was started, false if already running
   */
  virtual inline bool startWorkerThread() {
    //todo lock
    if(!runnable_thread_.joinable()) {
      should_run_.store(true);
      runnable_thread_ = std::thread(&RunnableService::run, this);
      return true;
    }
    return false;
  }

  /**
   * Calls the abstract work method and notifies when shutdown was called.
   */
  virtual void run() {
    while(should_run_.load()) {
      work();
    }
    //done
    std::unique_lock <std::mutex> lck(this->mtx);
    this->cv.notify_all();
  }

  /**
   * Implement this method to do work. Note: this method is assumed to NOT block, otherwise shutdown does nothing.
   */
  virtual void work() = 0;

private:
  std::thread runnable_thread_;
  std::atomic<bool> should_run_;
  std::condition_variable cv;
  mutable std::mutex mtx;
};
