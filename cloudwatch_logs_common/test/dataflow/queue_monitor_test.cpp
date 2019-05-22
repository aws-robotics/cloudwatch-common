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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cloudwatch_logs_common/dataflow/observed_queue.h>
#include <cloudwatch_logs_common/dataflow/queue_monitor.h>

using namespace Aws::DataFlow;
using namespace ::testing;

class MockObservedQueue :
  public IObservedQueue<std::string>
{
public:

  MOCK_CONST_METHOD0(size, size_t (void));
  MOCK_CONST_METHOD0(empty, bool (void));
  MOCK_METHOD1(dequeue, bool (std::string& data));
  MOCK_METHOD1(enqueue, bool (std::string& data));
  MOCK_METHOD2(tryEnqueue,
    bool (std::string& data,
    const std::chrono::microseconds &duration));
  inline bool enqueue(std::string&& value) override {
    return false;
  }

  /**
   * Set the observer for the queue.
   *
   * @param status_monitor
   */
  inline void setStatusMonitor(std::shared_ptr<StatusMonitor> status_monitor) override {
    status_monitor_ = status_monitor;
  }

  inline bool tryEnqueue(
      std::string&& value,
      const std::chrono::microseconds &duration) override
  {
    return enqueue(value);
  }

 /**
 * The status monitor observer.
 */
  std::shared_ptr<StatusMonitor> status_monitor_;
};

bool dequeueFunc(std::string& data, std::string actual) {
  data = std::move(actual);
  return true;
}

using std::placeholders::_1;

TEST(queue_demux_test, single_source_test)
{
  auto observed_queue = std::make_shared<MockObservedQueue>();
  std::shared_ptr<StatusMonitor> monitor;
  std::string actual = "test_string";
  EXPECT_CALL(*observed_queue, dequeue(_))
    .WillOnce(Invoke(std::bind(dequeueFunc, _1, actual)));
  QueueMonitor<std::string> queue_monitor;
  queue_monitor.addSource(observed_queue, PriorityOptions());
  std::string data;
  EXPECT_TRUE(queue_monitor.dequeue(data), std::chrono::microseconds(0));
  EXPECT_EQ(actual, data);
}

TEST(queue_demux_test, multi_source_test)
{
  QueueMonitor<std::string> queue_monitor;
  auto low_priority_queue = std::make_shared<StrictMock<MockObservedQueue>>();
  EXPECT_CALL(*low_priority_queue, dequeue(_))
    .WillOnce(Invoke(std::bind(dequeueFunc, _1, "low_priority")))
    .WillRepeatedly(Return(false));
  queue_monitor.addSource(low_priority_queue, PriorityOptions(LOWEST_PRIORITY));

  auto high_priority_observed_queue = std::make_shared<StrictMock<MockObservedQueue>>();
  std::shared_ptr<StatusMonitor> monitor;
  EXPECT_CALL(*high_priority_observed_queue, dequeue(_))
    .WillOnce(Invoke(std::bind(dequeueFunc, _1, "high_priority")))
    .WillRepeatedly(Return(false));;
  queue_monitor.addSource(high_priority_observed_queue, PriorityOptions(HIGHEST_PRIORITY));
  std::string data;
  EXPECT_TRUE(queue_monitor.dequeue(data), std::chrono::microseconds(0));
  EXPECT_EQ("high_priority", data);
  EXPECT_TRUE(queue_monitor.dequeue(data), std::chrono::microseconds(0));
  EXPECT_EQ("low_priority", data);
  EXPECT_FALSE(queue_monitor.dequeue(data), std::chrono::microseconds(0));
}

