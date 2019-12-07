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

#include <file_management/file_upload/file_manager.h>
#include <file_management/file_upload/file_management_factory.h>
#include <dataflow_lite/dataflow/dataflow.h>

using namespace Aws::FileManagement;
using namespace Aws::DataFlow;

class MockDataReader :
  public DataReader<std::string>
{
public:
  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Shutdown, bool());
  MOCK_METHOD1(ReadBatch, FileObject<std::string>(size_t batch_size));
  MOCK_METHOD2(FileUploadCompleteStatus,
    void(const UploadStatus& upload_status, const FileObject<std::string> &log_messages));
  MOCK_METHOD1(AddStatusMonitor,
    void(std::shared_ptr<StatusMonitor> monitor));
  MOCK_METHOD0(IsDataAvailableToRead, bool());

  /**
   * Set the observer for the queue.
   *
   * @param status_monitor
   */
  inline void SetStatusMonitor(std::shared_ptr<StatusMonitor> status_monitor) override {
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
  MOCK_METHOD1(Enqueue, bool (SharedFileUploadTask& data));
  MOCK_METHOD2(TryEnqueue,
    bool (SharedFileUploadTask& data,
    const std::chrono::microseconds &duration));
  inline bool Enqueue(SharedFileUploadTask&& value) override {
    return enqueue_rvr(value);
  }
  inline bool TryEnqueue(
    SharedFileUploadTask&& value,
    const std::chrono::microseconds &duration) override
  {
    return TryEnqueue(value, duration);
  }
  MOCK_METHOD0(Clear, void (void));
};

static const FileUploadStreamerOptions kFileManagerOptions{50, 5};

class FileStreamerTest : public ::testing::Test {
public:
  void SetUp() override
  {
    file_manager_ = std::make_shared<::testing::StrictMock<MockDataReader>>();
    file_upload_streamer_ = CreateFileUploadStreamer<std::string>(file_manager_, kFileManagerOptions);
    mock_sink_ = std::make_shared<::testing::StrictMock<MockSink>>();
  }

  void TearDown() override
  {
    file_manager_.reset();
    file_upload_streamer_.reset();
    mock_sink_.reset();
  }

protected:
  std::shared_ptr<::testing::StrictMock<MockDataReader>> file_manager_;
  std::shared_ptr<FileUploadStreamer<std::string>> file_upload_streamer_;
  std::shared_ptr<MockSink> mock_sink_;
};


TEST_F(FileStreamerTest, success_on_network_and_file) {
  // Create the pipeline
  file_upload_streamer_->SetSink(mock_sink_);

  // Set the file and network available
  file_manager_->status_monitor_->SetStatus(AVAILABLE);
  file_upload_streamer_->OnPublisherStateChange(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  EXPECT_CALL(*mock_sink_, TryEnqueue(testing::_, testing::_))
  .WillOnce(testing::Return(true));
  EXPECT_CALL(*file_manager_, ReadBatch(testing::Eq(50u)))
  .WillOnce(testing::Return(test_file_object));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer_->ForceWork();
}

TEST_F(FileStreamerTest, fail_enqueue_retry) {
  // Create the pipeline
  file_upload_streamer_->SetSink(mock_sink_);

  // Set the file and network available
  file_manager_->status_monitor_->SetStatus(AVAILABLE);
  file_upload_streamer_->OnPublisherStateChange(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  SharedFileUploadTask task;
  // TODO(unknown): capture and test equivalence
  EXPECT_CALL(*mock_sink_, TryEnqueue(testing::_, testing::_))
      .WillOnce(testing::Invoke([&task](SharedFileUploadTask& data,
                                        const std::chrono::microseconds&){
        task = data;
        return false;
      }))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*file_manager_, ReadBatch(testing::Eq(50u)))
      .WillOnce(testing::Return(test_file_object));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer_->ForceWork();
  file_upload_streamer_->ForceWork();
}

TEST_F(FileStreamerTest, fail_task_clears_queue) {
  // Create the pipeline
  file_upload_streamer_->SetSink(mock_sink_);

  // Set the file and network available
  file_manager_->status_monitor_->SetStatus(AVAILABLE);
  file_upload_streamer_->OnPublisherStateChange(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  SharedFileUploadTask task;
  // TODO(unknown): capture and test equivalence
  EXPECT_CALL(*mock_sink_, TryEnqueue(testing::_, testing::_))
    .WillOnce(testing::Invoke([&task](SharedFileUploadTask& data,
                                      const std::chrono::microseconds&){
      task = data;
      return true;
    }));
  EXPECT_CALL(*file_manager_, ReadBatch(testing::Eq(50u)))
      .WillOnce(testing::Return(test_file_object));
  EXPECT_CALL(*mock_sink_, Clear());
  EXPECT_CALL(*file_manager_, FileUploadCompleteStatus(FAIL, testing::_));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer_->ForceWork();
  task->OnComplete(FAIL);
}

TEST_F(FileStreamerTest, success_task_does_not_clear_queue) {
  // Create the pipeline
  file_upload_streamer_->SetSink(mock_sink_);

  // Set the file and network available
  file_manager_->status_monitor_->SetStatus(AVAILABLE);
  file_upload_streamer_->OnPublisherStateChange(AVAILABLE);

  FileObject<std::string> test_file_object;
  test_file_object.batch_data = "data";
  test_file_object.batch_size = 1;
  SharedFileUploadTask task;
  EXPECT_CALL(*mock_sink_, TryEnqueue(testing::_, testing::_))
          .WillOnce(testing::Invoke([&task](SharedFileUploadTask& data,
                                            const std::chrono::microseconds&){
            task = data;
            return true;
          }));
  EXPECT_CALL(*file_manager_, ReadBatch(testing::Eq(50u)))
          .WillOnce(testing::Return(test_file_object));
  EXPECT_CALL(*file_manager_, FileUploadCompleteStatus(SUCCESS, testing::_));
  // Expect a batch call and enqueue from the file upload streamer
  file_upload_streamer_->ForceWork();
  task->OnComplete(SUCCESS);
}
