#include <iostream>
#include <experimental/filesystem>
#include <fstream>
#include <cloudwatch_logs_common/file_upload/file_manager.h>
#include <aws/core/utils/logging/LogMacros.h>
#include "cloudwatch_logs_common/file_upload/file_manager_strategy.h"

namespace fs = std::experimental::filesystem;

namespace Aws {
namespace FileManagement {

FileManagerStrategy::FileManagerStrategy() {
  rotateWriteFile();
}

void FileManagerStrategy::initialize() {
  discoverStoredFiles();
}

bool FileManagerStrategy::isDataAvailable() {
  if (!active_read_file_.empty()) {
    return true;
  }

  if (!stored_files_.empty()) {
    return true;
  }

  if (active_write_file_size_) {
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
  const std::string file_name = active_read_file_;
  DataToken token = createToken(file_name);
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

void FileManagerStrategy::resolve(const std::list<DataToken> &tokens) {
  for (const auto &token : tokens) {
    resolve(token);
  }
}

void FileManagerStrategy::onShutdown() {
  // @todo: implement
}

void FileManagerStrategy::discoverStoredFiles() {
  for (const auto &entry : fs::directory_iterator(storage_directory_)) {
    const fs::path &path = entry.path();
    if (path.extension() == file_extension_) {
      addFileNameToStorage(path.relative_path());
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
  uint64_t file_id = std::rand() % UINT64_MAX;
  std::stringstream conversion_stream;
  conversion_stream << std::hex << file_id;
  std::string file_name(conversion_stream.str());
  active_write_file_ = file_name;
  active_write_file_size_ = 0;
}

void FileManagerStrategy::checkIfFileShouldRotate(const std::string &data) {
  const uintmax_t new_file_size = active_write_file_size_ + data.size();
  if (new_file_size >= maximum_file_size_in_bytes_) {
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
