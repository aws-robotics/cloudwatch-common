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
#include <cloudwatch_logs_common/dataflow/dataflow.h>

using namespace Aws::CloudWatchLogs;
using namespace Aws::CloudWatchLogs::Utils;
using namespace Aws::FileManagement;
using namespace Aws::DataFlow;


class MockTaskFactory :
  public ITaskFactory<std::string> {
public:
  MOCK_METHOD1(createFileUploadTaskAsync_rvr,
    std::shared_ptr<FileUploadTaskAsync<std::string>>(FileObject<std::string> batch_data));

  std::shared_ptr<FileUploadTaskAsync<std::string>>
  createFileUploadTaskAsync(
      FileObject<std::string>&& batch_data) override {
    return createFileUploadTaskAsync_rvr(batch_data);
  }
};

class MockDataReader :
  public DataReader<std::string>
{
public:
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
    return enqueue(value);
  }
};

class FileStreamerTest : public ::testing::Test {
public:
  void SetUp() override
  {
    file_manager = std::make_shared<::testing::StrictMock<MockDataReader>>();
    mock_task_factory = std::make_shared<MockTaskFactory>();
    file_upload_streamer = createFileUploadStreamer<std::string>(file_manager, mock_task_factory);
    mock_sink = std::make_shared<MockSink>();
    network_status_monitor = std::make_shared<StatusMonitor>();
  }

  void TearDown() override
  {
  }

protected:
  std::shared_ptr<::testing::StrictMock<MockDataReader>> file_manager;
  std::shared_ptr<FileUploadStreamer<std::string>> file_upload_streamer;
  std::shared_ptr<MockSink> mock_sink;
  std::shared_ptr<StatusMonitor> network_status_monitor;
  std::shared_ptr<MockTaskFactory> mock_task_factory;
};


TEST_F(FileStreamerTest, success_on_network_and_file) {
  // Create the pipeline
  *file_upload_streamer >> mock_sink;
  file_upload_streamer->addStatusMonitor(network_status_monitor);

  // Set the file and network available
  file_manager->status_monitor_->setStatus(AVAILABLE);
  network_status_monitor->setStatus(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  EXPECT_CALL(*mock_sink, enqueue_rvr(testing::_))
  .WillOnce(
    testing::Invoke([](SharedFileUploadTask data) -> bool {
      data->run();
      return true;
    }));
  EXPECT_CALL(*file_manager, readBatch(testing::Eq(50)))
  .WillOnce(testing::Return(test_file_object));
  EXPECT_CALL(*file_manager, fileUploadCompleteStatus(testing::Eq(SUCCESS), testing::_))
  .Times(1);
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer->run();
}

TEST_F(FileStreamerTest, fail_upload) {
  // Create the pipeline
  *file_upload_streamer >> mock_sink;

  // Set the file and network available
  file_manager->status_monitor_->setStatus(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  EXPECT_CALL(*mock_sink, enqueue_rvr(testing::_))
      .WillOnce(
          testing::Invoke([](SharedFileUploadTask data) -> bool {
            return true;
          }));
  EXPECT_CALL(*file_manager, readBatch(testing::Eq(50)))
      .WillOnce(testing::Return(test_file_object));
  EXPECT_CALL(*file_manager, fileUploadCompleteStatus(testing::Eq(UploadStatus::FAIL), testing::_))
      .Times(1);
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer->run();
}

TEST_F(FileStreamerTest, fail_enqueue) {
  // Create the pipeline
  *file_upload_streamer >> mock_sink;

  // Set the file and network available
  file_manager->status_monitor_->setStatus(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  EXPECT_CALL(*mock_sink, enqueue_rvr(testing::_))
      .WillOnce(
          testing::Invoke([](SharedFileUploadTask data) -> bool {
            return false;
          }));
  EXPECT_CALL(*file_manager, readBatch(testing::Eq(50)))
      .WillOnce(testing::Return(test_file_object));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer->run();
}

TEST_F(FileStreamerTest, block_on_no_network) {
  // Create the pipeline
  *file_upload_streamer >> mock_sink;
  file_upload_streamer->addStatusMonitor(network_status_monitor);

  // Set the file available, network is still unavailable
  file_manager->status_monitor_->setStatus(AVAILABLE);
  network_status_monitor->setStatus(UNAVAILABLE);

  // The strict mocks will throw an error should the run function pass the status monitor check
  file_upload_streamer->run();
}

TEST_F(FileStreamerTest, block_on_file_not_available) {
  // Create the pipeline
  *file_upload_streamer >> mock_sink;

  // The strict mocks will throw an error should the run function pass the status monitor check
  file_upload_streamer->run();
}

TEST_F(FileStreamerTest, test_no_sink_installed) {

  // The strict mocks will throw an error should the run function pass the status monitor check
  file_upload_streamer->run();
}
