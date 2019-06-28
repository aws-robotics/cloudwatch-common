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
#include <fstream>
#include <file_management/file_upload/file_manager.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <iomanip>
#include "file_management/file_upload/file_manager_strategy.h"

namespace fs = std::experimental::filesystem;

namespace Aws {
namespace FileManagement {

static const std::string kConfigFile("file_management.info");
static const std::string kTokenStoreFile("token_store.info");

TokenStore::TokenStore(const TokenStoreOptions &options) : options_{options}{
  initializeBackupDirectory();
}

void TokenStore::initializeBackupDirectory() {
  // todo this needs stricter checking: throw if unrecoverable

  if (options_.backup_directory.back() != '/') {
    options_.backup_directory += '/';
  }
  auto backup_directory = std::experimental::filesystem::path(options_.backup_directory);
  if (!std::experimental::filesystem::exists(backup_directory)) {
    std::experimental::filesystem::create_directory(backup_directory);
  }
}

bool TokenStore::isTokenAvailable(const std::string &file_name) const {
  return !(staged_tokens_.find(file_name) == staged_tokens_.end());
}

FileTokenInfo TokenStore::popAvailableToken(const std::string &file_name) {
  auto file_token_info = staged_tokens_[file_name];
  staged_tokens_.erase(file_name);
  return file_token_info;
}

DataToken TokenStore::createToken(const std::string &file_name, const long & streampos, bool is_eof) {
  std::mt19937_64 rand( rand_device() );
  DataToken token = rand();
  token_store_.emplace(token, FileTokenInfo(file_name, streampos, is_eof));
  file_tokens_[file_name].push_back(token);
  return token;
}

FileTokenInfo TokenStore::fail(const DataToken &token) {
  if (token_store_.find(token) == token_store_.end()) {
    throw std::runtime_error("DataToken not found");
  }
  FileTokenInfo token_info = token_store_[token];
  if (file_tokens_.find(token_info.file_path_) == file_tokens_.end()) {
    throw std::runtime_error("File not found in cache: " + token_info.file_path_);
  }
  const std::string &file_path = token_info.file_path_;
  staged_tokens_[file_path] = token_info;
  file_tokens_.erase(file_path);
  token_store_.erase(token);
  return token_info;
}

FileTokenInfo TokenStore::resolve(const DataToken &token) {
  if (token_store_.find(token) == token_store_.end()) {
    throw std::runtime_error("DataToken not found");
  }
  FileTokenInfo token_info = token_store_[token];
  const std::string &file_path = token_info.file_path_;

  if (file_tokens_.find(file_path) == file_tokens_.end()) {
    throw std::runtime_error("Could not find token set for file: " + file_path);
  }
  // this find should be O(1), as we expect data to be resolved in order
  auto list = file_tokens_[file_path];
  list.erase(std::find(list.begin(), list.end(), token));

  if (file_tokens_[file_path].empty()) {
    file_tokens_.erase(file_path);
  }
  token_store_.erase(token);
  return token_info;
}

std::vector<FileTokenInfo> TokenStore::backup() {
  auto vector_size = file_tokens_.size() + staged_tokens_.size();
  std::vector<FileTokenInfo> token_backup(vector_size);
  auto it = token_backup.begin();
  for (auto& token : staged_tokens_) {
    *it++ = token.second;
  }
  for (auto& token : file_tokens_) {
    *it++ = token_store_[*token.second.begin()];
  }
  return token_backup;
}

void TokenStore::backupToDisk() {
  auto file_path = std::experimental::filesystem::path(options_.backup_directory + kTokenStoreFile);
  std::vector<FileTokenInfo> token_store_backup = backup();
  if (std::experimental::filesystem::exists(file_path)) {
    std::experimental::filesystem::remove(file_path);
  }
  std::ofstream token_store_file;
  token_store_file.open(file_path);
  if (token_store_file.bad()) {
    AWS_LOG_WARN(__func__,
                 "Unable to open file: %s", file_path.c_str());
  }
  for (const FileTokenInfo &token_info : token_store_backup) {
    token_store_file << token_info.serialize() << std::endl;
  }
  token_store_file.close();
}

void TokenStore::restore(const std::vector<FileTokenInfo> &file_tokens) {
  for (auto& file_token: file_tokens) {
    staged_tokens_[file_token.file_path_] = file_token;
  }
}

void TokenStore::restoreFromDisk() {
  // read through each line.
  // For each line the first 4 bytes are position, next byte is eof, the remainder are a string of file path
  // Will this change depending on OS / platform? Will that matter? Should I use another serialization library.
  auto file_path = std::experimental::filesystem::path(options_.backup_directory + kTokenStoreFile);
  if (!std::experimental::filesystem::exists(file_path)) {
    return;
  }
  std::ifstream token_store_read_stream = std::ifstream(file_path);
  std::vector<FileTokenInfo> file_tokens;
  std::string line;
  while (!token_store_read_stream.eof()) {
    std::getline(token_store_read_stream, line);
    if (!line.empty()) {
      FileTokenInfo token_info;
      try {
        token_info.deserialize(line);
      } catch (std::runtime_error e) {
        AWS_LOG_ERROR(__func__, "Unable to parse token backup line: %s. Skipping.", line.c_str());
        continue;
      }
      file_tokens.push_back(token_info);
    }
  }
  token_store_read_stream.close();
  restore(file_tokens);
  std::experimental::filesystem::remove(file_path);
}


FileManagerStrategy::FileManagerStrategy(const FileManagerStrategyOptions &options) {
  options_ = options;
  stored_files_size_ = 0;
  active_write_file_size_ = 0;
}

bool FileManagerStrategy::start() {
  initializeStorage();
  initializeTokenStore();
  discoverStoredFiles();
  rotateWriteFile();
  return true;
}

void FileManagerStrategy::initializeStorage() {
  if (options_.storage_directory.back() != '/') {
    options_.storage_directory += '/';
  }
  auto storage = std::experimental::filesystem::path(options_.storage_directory);
  if (!std::experimental::filesystem::exists(storage)) {
    std::experimental::filesystem::create_directory(storage);
    stored_files_size_ = 0;
  }
}

void FileManagerStrategy::initializeTokenStore() {

  TokenStoreOptions options{options_.storage_directory};
  token_store_ = std::make_unique<TokenStore>(options);
  token_store_->restoreFromDisk();
}

bool FileManagerStrategy::isDataAvailable() {
  return !active_read_file_.empty() || !stored_files_.empty() || active_write_file_size_ > 0;
}

// @todo (rddesmon) catch and wrap failure to open exceptions
// @todo Deal with race conditions if there are multiple writers
void FileManagerStrategy::write(const std::string &data) {
  checkIfFileShouldRotate(data.size());
  checkIfStorageLimitHasBeenReached(data.size());
  std::ofstream log_file;
  log_file.open(active_write_file_, std::ios_base::app);
  if (log_file.bad()) {
    AWS_LOG_WARN(__func__,
      "Unable to open file: %s", active_write_file_.c_str());
  }
  log_file << data << std::endl;
  log_file.close();
  active_write_file_size_ += data.size();
}

DataToken FileManagerStrategy::read(std::string &data) {
  if (active_read_file_.empty()) {
    active_read_file_ = getFileToRead();
    active_read_file_stream_ = std::make_unique<std::ifstream>(active_read_file_);
  }
  AWS_LOG_INFO(__func__,
               "Reading from active log file: %s", active_read_file_.c_str());
  DataToken token;
  if (token_store_->isTokenAvailable(active_read_file_)) {
    FileTokenInfo file_token = token_store_->popAvailableToken(active_read_file_);
    active_read_file_stream_->seekg(file_token.position_);
  }
  long position = active_read_file_stream_->tellg();
  auto file_size = active_read_file_stream_->seekg(0, std::ifstream::end).tellg();
  active_read_file_stream_->seekg(position, std::ifstream::beg);
  std::getline(*active_read_file_stream_, data);
  long next_position = active_read_file_stream_->tellg();
  token = token_store_->createToken(active_read_file_, position, next_position >= file_size);

  if (next_position >= file_size) {
    active_read_file_.clear();
    active_read_file_stream_ = nullptr;
  }
  return token;
}

void FileManagerStrategy::resolve(const DataToken &token, bool is_success) {
  if (is_success) {
    auto file_info = token_store_->resolve(token);
    if (file_info.eof_) {
      deleteFile(file_info.file_path_);
    }
  } else {
    auto file_info = token_store_->fail(token);
    if (file_info.eof_) {
      stored_files_.push_back(file_info.file_path_);
    }
  }
}

bool FileManagerStrategy::shutdown() {
  // todo can this stuff throw?
  token_store_->backupToDisk();
  auto config_file_path = std::experimental::filesystem::path(options_.storage_directory + kConfigFile);
  if (std::experimental::filesystem::exists(config_file_path)) {
    std::experimental::filesystem::remove(config_file_path);
  }
  std::ofstream config_file(config_file_path);
  // todo what is being written here?
  config_file.close();
  return true;
}


void FileManagerStrategy::discoverStoredFiles() {
  for (const auto &entry : fs::directory_iterator(options_.storage_directory)) {
    const fs::path &path = entry.path();
    std::regex name_expr(
      options_.file_prefix +
      "[0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2}-[0-9]{2}-[0-9]{1}" +
      options_.file_extension);
    if (std::regex_match(path.filename().string(), name_expr)) {
      addFilePathToStorage(path);
    }
  }
}

void FileManagerStrategy::deleteFile(const std::string &file_path) {
  // @todo (rddesmon) consider new thread for file deletion
  AWS_LOG_WARN(__func__,
    "Deleting file: %s", file_path.c_str());
  const uintmax_t file_size = fs::file_size(file_path);
  fs::remove(file_path);
  stored_files_size_ -= file_size;
}

std::string FileManagerStrategy::getFileToRead() {
  // @todo (rddesmond) return earliest file
  // if we have stored files, pop from the start of the list and return that filename
  // if we do not have stored files, and the active file has data, switch active file and return the existing active file.
  if (!stored_files_.empty()) {
    stored_files_.sort();
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

void FileManagerStrategy::addFilePathToStorage(const fs::path &file_path) {
  stored_files_.push_back(file_path);
  stored_files_size_ += fs::file_size(file_path);
}

void FileManagerStrategy::rotateWriteFile() {
  // TODO create using UUID or something.
  using std::chrono::system_clock;
  time_t tt = system_clock::to_time_t (system_clock::now());
  std::ostringstream oss;
  auto tm = *std::localtime(&tt);
  oss << std::put_time(&tm, "%F_%H-%M-%S");
  uint count = 0;
  std::string original_file_name = options_.file_prefix + oss.str();
  std::string file_name = original_file_name + "-" + std::to_string(count);
  std::string file_path = options_.storage_directory + file_name + options_.file_extension;
  while (fs::exists(file_path)) {
    ++count;
    file_name = original_file_name + "-" + std::to_string(count);
    file_path = options_.storage_directory + file_name + options_.file_extension;
  }

  if (!active_write_file_.empty()) {
    stored_files_.push_back(active_write_file_);
    stored_files_size_ += active_write_file_size_;
  }

  active_write_file_ = file_path;
  active_write_file_size_ = 0;
}

void FileManagerStrategy::checkIfFileShouldRotate(const uintmax_t &new_data_size) {
  const uintmax_t new_file_size = active_write_file_size_ + new_data_size;
  const uintmax_t max_file_size_in_bytes = options_.maximum_file_size_in_kb * 1024;
  if (new_file_size > max_file_size_in_bytes) {
    rotateWriteFile();
  }
}

void FileManagerStrategy::checkIfStorageLimitHasBeenReached(const uintmax_t &new_data_size) {
  const uintmax_t new_storage_size = stored_files_size_ + active_write_file_size_ + new_data_size;
  const uintmax_t max_storage_size_in_bytes = options_.storage_limit_in_kb * 1024;
  if (new_storage_size > max_storage_size_in_bytes) {
    deleteOldestFile();
  }
}

void FileManagerStrategy::deleteOldestFile() {
  if (!stored_files_.empty()) {
    stored_files_.sort();
    const std::string oldest_file = stored_files_.front();
    stored_files_.pop_front();
    deleteFile(oldest_file);
  }
}

}  // namespace FileManagement
}  // namespace Aws
