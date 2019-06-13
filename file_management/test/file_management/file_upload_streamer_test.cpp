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


#include <tuple>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cloudwatch_logs_common/ros_cloudwatch_logs_errors.h>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <cloudwatch_logs_common/file_upload/file_management_factory.h>
#include <cloudwatch_logs_common/utils/log_file_manager.h>
#include <dataflow_lite/dataflow/dataflow.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;
using namespace Aws::DataFlow;

class MockDataReader :
  public DataReader<std::string>
{
public:
  MOCK_METHOD0(start, bool());
  MOCK_METHOD0(shutdown, bool());
  MOCK_METHOD1(readBatch, FileObject<std::string>(size_t batch_size));
  MOCK_METHOD2(fileUploadCompleteStatus,
    void(const UploadStatus& upload_status, const FileObject<std::string> &log_messages));
  MOCK_METHOD1(addStatusMonitor,
    void(std::shared_ptr<StatusMonitor> monitor));

  /**
   * Set the observer for the queue.
   *
   * @param status_monitor
   */
  inline void setStatusMonitor(std::shared_ptr<StatusMonitor> status_monitor) override {
    status_monitor_ = status_monitor;
  }

  /**
  * The status monitor observer.
  */
  std::shared_ptr<StatusMonitor> status_monitor_;
};

using SharedFileUploadTask = std::shared_ptr<Task<std::string>>;

class MockSink :
public Sink<SharedFileUploadTask>
{
public:
  MOCK_METHOD1(enqueue_rvr, bool (SharedFileUploadTask data));
  MOCK_METHOD1(enqueue, bool (SharedFileUploadTask& data));
  MOCK_METHOD2(tryEnqueue,
    bool (SharedFileUploadTask& data,
    const std::chrono::microseconds &duration));
  inline bool enqueue(SharedFileUploadTask&& value) override {
    return enqueue_rvr(value);
  }
  inline bool tryEnqueue(
    SharedFileUploadTask&& value,
    const std::chrono::microseconds &duration) override
  {
    return tryEnqueue(value, duration);
  }
  MOCK_METHOD0(clear, void (void));
};

class FileStreamerTest : public ::testing::Test {
public:
  void SetUp() override
  {
    file_manager = std::make_shared<::testing::StrictMock<MockDataReader>>();
    file_upload_streamer = createFileUploadStreamer<std::string>(file_manager);
    mock_sink = std::make_shared<::testing::StrictMock<MockSink>>();
  }

  void TearDown() override
  {
    file_manager.reset();
    file_upload_streamer.reset();
    mock_sink.reset();
  }

protected:
  std::shared_ptr<::testing::StrictMock<MockDataReader>> file_manager;
  std::shared_ptr<FileUploadStreamer<std::string>> file_upload_streamer;
  std::shared_ptr<MockSink> mock_sink;
};


TEST_F(FileStreamerTest, success_on_network_and_file) {
  // Create the pipeline
  file_upload_streamer->setSink(mock_sink);

  // Set the file and network available
  file_manager->status_monitor_->setStatus(AVAILABLE);
  file_upload_streamer->onPublisherStateChange(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  EXPECT_CALL(*mock_sink, tryEnqueue(testing::_, testing::_))
  .WillOnce(testing::Return(true));
  EXPECT_CALL(*file_manager, readBatch(testing::Eq(50)))
  .WillOnce(testing::Return(test_file_object));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer->forceWork();
}

TEST_F(FileStreamerTest, fail_enqueue_retry) {
  // Create the pipeline
  file_upload_streamer->setSink(mock_sink);

  // Set the file and network available
  file_manager->status_monitor_->setStatus(AVAILABLE);
  file_upload_streamer->onPublisherStateChange(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  SharedFileUploadTask task;
  // TODO: capture and test equivalence
  EXPECT_CALL(*mock_sink, tryEnqueue(testing::_, testing::_))
      .WillOnce(testing::Invoke([&task](SharedFileUploadTask& data,
                                        const std::chrono::microseconds&){
        task = data;
        return false;
      }))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*file_manager, readBatch(testing::Eq(50)))
      .WillOnce(testing::Return(test_file_object));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer->run();
  file_upload_streamer->run();
}

TEST_F(FileStreamerTest, fail_task_clears_queue) {
  // Create the pipeline
  file_upload_streamer->setSink(mock_sink);

  // Set the file and network available
  file_manager->status_monitor_->setStatus(AVAILABLE);
  file_upload_streamer->onPublisherStateChange(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  SharedFileUploadTask task;
  // TODO: capture and test equivalence
  EXPECT_CALL(*mock_sink, tryEnqueue(testing::_, testing::_))
    .WillOnce(testing::Invoke([&task](SharedFileUploadTask& data,
                                      const std::chrono::microseconds&){
      task = data;
      return true;
    }));
  EXPECT_CALL(*file_manager, readBatch(testing::Eq(50)))
      .WillOnce(testing::Return(test_file_object));
  EXPECT_CALL(*mock_sink, clear());
  EXPECT_CALL(*file_manager, fileUploadCompleteStatus(FAIL, testing::_));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer->run();
  task->onComplete(FAIL);
}

TEST_F(FileStreamerTest, success_task_does_not_clear_queue) {
  // Create the pipeline
  file_upload_streamer->setSink(mock_sink);

  // Set the file and network available
  file_manager->status_monitor_->setStatus(AVAILABLE);
  file_upload_streamer->onPublisherStateChange(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  SharedFileUploadTask task;
  EXPECT_CALL(*mock_sink, tryEnqueue(testing::_, testing::_))
          .WillOnce(testing::Invoke([&task](SharedFileUploadTask& data,
                                            const std::chrono::microseconds&){
            task = data;
            return true;
          }));
  EXPECT_CALL(*file_manager, readBatch(testing::Eq(50)))
          .WillOnce(testing::Return(test_file_object));
  EXPECT_CALL(*file_manager, fileUploadCompleteStatus(SUCCESS, testing::_));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer->run();
  task->onComplete(SUCCESS);
}

TEST_F(FileStreamerTest, block_on_no_network) {
  // Create the pipeline
  file_upload_streamer->setSink(mock_sink);

  // Set the file available, network is still unavailable
  file_manager->status_monitor_->setStatus(AVAILABLE);
  file_upload_streamer->onPublisherStateChange(UNAVAILABLE);

  // The strict mocks will throw an error should the run function pass the status monitor check
  file_upload_streamer->forceWork();
}

TEST_F(FileStreamerTest, block_on_file_not_available) {
  // Create the pipeline
  file_upload_streamer->setSink(mock_sink);
  file_upload_streamer->onPublisherStateChange(AVAILABLE);
  // The strict mocks will throw an error should the run function pass the status monitor check
  file_upload_streamer->forceWork();
}

TEST_F(FileStreamerTest, test_no_sink_installed) {

  // The strict mocks will throw an error should the run function pass the status monitor check
  file_upload_streamer->forceWork();
}
