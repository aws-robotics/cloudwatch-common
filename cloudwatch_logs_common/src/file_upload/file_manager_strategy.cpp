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

#include <chrono>
#include <iostream>
#include <regex>
#include <experimental/filesystem>
#include <fstream>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <iomanip>
#include "cloudwatch_logs_common/file_upload/file_manager_strategy.h"

namespace fs = std::experimental::filesystem;

namespace Aws {
namespace FileManagement {

FileManagerStrategy::FileManagerStrategy(const FileManagerStrategyOptions &options) {
  options_ = options;
  rotateWriteFile();
}

void FileManagerStrategy::initialize() {
  validateOptions();
  discoverStoredFiles();
}

void FileManagerStrategy::validateOptions() {
  auto storage = std::experimental::filesystem::path(options_.storage_directory);
  if (!std::experimental::filesystem::exists(storage)) {
    std::experimental::filesystem::create_directory(storage);
  }
}

bool FileManagerStrategy::isDataAvailable() {
  if (!active_read_file_.empty()) {
    return true;
  }

  if (!stored_files_.empty()) {
    return true;
  }

  if (active_write_file_size_ > 0) {
    return true;
  }

  return false;
}

DataToken FileManagerStrategy::read(std::string &data) {
  AWS_LOG_INFO(__func__,
               "Reading from active log");
  if (active_read_file_.empty()) {
    active_read_file_ = getFileToRead();
    active_read_file_stream_ = std::make_unique<std::ifstream>(active_read_file_);
  }
  DataToken token = createToken(active_read_file_);
  std::getline(*active_read_file_stream_, data);
  if (active_read_file_stream_->eof()) {
    active_read_file_.clear();
    active_read_file_stream_ = nullptr;
  }
  return token;
}

void FileManagerStrategy::write(const std::string &data) {
  checkIfFileShouldRotate(data);
  std::ofstream log_file;
  log_file.open(active_write_file_, std::ios_base::app);
  log_file << data << std::endl;
  log_file.close();
  active_write_file_size_ += data.size();
}

void FileManagerStrategy::resolve(const DataToken &token) {
  if (token_store_.find(token) == token_store_.end()) {
    throw std::runtime_error("DataToken not found");
  }
  FileTokenInfo token_info = token_store_[token];
  std::string &file_name = token_info.file_name_;

  if (file_tokens_.find(file_name) == file_tokens_.end()) {
    throw std::runtime_error("Could not find token set for file: " + file_name);
  }
  file_tokens_[file_name].erase(token);
  if (file_tokens_.empty()) {
    deleteFile(file_name);
  }
  token_store_.erase(token);
}

void FileManagerStrategy::onShutdown() {
  // @todo: implement
}

void FileManagerStrategy::discoverStoredFiles() {
  for (const auto &entry : fs::directory_iterator(options_.storage_directory)) {
    const fs::path &path = entry.path();
    std::regex name_expr(
      options_.file_prefix +
      "[0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2}-[0-9]{2}" +
      options_.file_extension);
    if (std::regex_match(path.filename().string(), name_expr)) {
      addFileNameToStorage(path);
    }
  }
}

void FileManagerStrategy::deleteFile(const std::string &file_name) {
  std::remove(file_name.c_str());
}

std::string FileManagerStrategy::getFileToRead() {
  // if we have stored files, pop from the start of the list and return that filename
  // if we do not have stored files, and the active file has data, switch active file and return the existing active file.
  if (!stored_files_.empty()) {
    const std::string oldest_file = stored_files_.front();
    stored_files_.pop_front();
    return oldest_file;
  }

  // TODO: Deal with threads writing to active file. Lock it?
  if (active_write_file_size_ > 0) {
    const std::string file_path = active_write_file_;
    rotateWriteFile();
    return file_path;
  }

  throw "No files available for reading";
}

void FileManagerStrategy::addFileNameToStorage(const std::string &file_name) {
  stored_files_.push_back(file_name);
}

void FileManagerStrategy::rotateWriteFile() {
  // TODO create using UUID or something.
  using std::chrono::system_clock;
  time_t tt = system_clock::to_time_t (system_clock::now());
  std::ostringstream oss;
  auto tm = *std::localtime(&tt);
  oss << std::put_time(&tm, "%F_%H-%M-%S");
  active_write_file_ = options_.storage_directory + options_.file_prefix + oss.str() + options_.file_extension;
  active_write_file_size_ = 0;
}

void FileManagerStrategy::checkIfFileShouldRotate(const std::string &data) {
  const uintmax_t new_file_size = active_write_file_size_ + data.size();
  if (new_file_size >= options_.maximum_file_size_in_bytes) {
    rotateWriteFile();
  }
}

DataToken FileManagerStrategy::createToken(const std::string &file_name) {
  DataToken token = std::rand() % UINT64_MAX;
  FileTokenInfo token_info = FileTokenInfo(file_name);
  token_store_[token] = token_info;
  if (file_tokens_.find(file_name) == file_tokens_.end()) {
    file_tokens_[file_name] = std::set<DataToken>();
  }
  file_tokens_[file_name].insert(token);
  return token;
}

}  // namespace FileManagement
}  // namespace Aws
