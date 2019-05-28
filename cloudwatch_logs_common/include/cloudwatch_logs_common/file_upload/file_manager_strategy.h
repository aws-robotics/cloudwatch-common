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

#include <iostream>
#include <fstream>
#include <list>
#include <unordered_map>
#include <set>
#include <memory>
#include "cloudwatch_logs_common/file_upload/task_utils.h"

namespace Aws {
namespace FileManagement {

/**
 * The status of a file.
 */
enum FileStatus {
  END_OF_READ,
  GOOD
};

/**
 * File information struct.
 */
class FileInfo {
public:
  std::string file_location;
  std::string file_name;
  FileStatus file_status;
};

enum TokenStatus {
  ACTIVE,
  INACTIVE
};


using DataToken = uint64_t;

class FileTokenInfo {
public:
  FileTokenInfo() = default;
  explicit FileTokenInfo(std::string file_name) : file_name_{std::move(file_name)} {};
  std::string file_name_;
};

class DataManagerStrategy {
public:
  DataManagerStrategy() = default;
  ~DataManagerStrategy() = default;

  virtual void initialize() = 0;

  virtual bool isDataAvailable() = 0;

  virtual void onShutdown() = 0;

  virtual DataToken read(std::string &data) = 0;

  virtual void write(const std::string &data) = 0;

  /**
   * Mark a token as 'done' so the DataManager knows the piece of
   * data associated with that token can be cleaned up.
   * @param token
   */
  virtual void resolve(const DataToken &token) = 0;
};

/**
 * Manages how files are split up, which files to write to and read when requested.
 */
class FileManagerStrategy : public DataManagerStrategy {
public:
  FileManagerStrategy();

  ~FileManagerStrategy() = default;

  void initialize() override;

  bool isDataAvailable() override;

  DataToken read(std::string &data) override;

  void write(const std::string &data) override;

  void resolve(const DataToken &token) override;

  void resolve(const std::list<DataToken> &tokens);

  void onShutdown() override;

private:
  void discoverStoredFiles();

  void deleteFile(const std::string &file_name);

  std::string getFileToRead();

  void rotateWriteFile();

  void checkIfFileShouldRotate(const std::string &data);

  void addFileNameToStorage(const std::string &file_name);

  DataToken createToken(const std::string &file_name);

  /**
   * Current file name to write to.
   */
  std::list<std::string> stored_files_;

  std::string active_write_file_;
  uint active_write_file_size_;

  std::string active_read_file_;
  std::unique_ptr<std::ifstream> active_read_file_stream_ = nullptr;

  /**
   * User configurable settings
   */
  std::string storage_directory_ = "/tmp/";
  std::string file_extension_ = ".log";
  uint maximum_file_size_in_bytes_ = 1024 * 1024;

  /**
   * Size of each batch when reading from a file.
   * The Size corresponds to the number of lines read from the file
   */
  uint8_t batch_size = 1;

  std::unordered_map<DataToken, FileTokenInfo> token_store_;
  std::unordered_map<std::string, std::set<DataToken>> file_tokens_;

};

}  // namespace FileManagement
}  // namespace Aws
