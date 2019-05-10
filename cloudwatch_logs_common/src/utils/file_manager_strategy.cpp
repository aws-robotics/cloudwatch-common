#include <iostream>
#include <experimental/filesystem>
#include <fstream>
#include "cloudwatch_logs_common/utils/file_manager_strategy.h"

namespace fs = std::experimental::filesystem;

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

FileManagerStrategy::FileManagerStrategy() {
  rotateActiveFile();
}

void FileManagerStrategy::initialize() {
  discoverStoredFiles();
}

std::string FileManagerStrategy::read() {
  const std::string file_name = getFileToRead();
  std::cout << "Reading from " << file_name;
  std::string data;
  std::ifstream log_file(file_name);
  std::getline(log_file, data);
  log_file.close();
  return data;
}

void FileManagerStrategy::write(const std::string data) {
  std::ofstream log_file;
  log_file.open(getFileToWrite());
  log_file << data << std::endl;
  log_file.close();
}

void FileManagerStrategy::deleteFile(const std::string fileName) {
  std::remove(fileName.c_str());
}

std::string FileManagerStrategy::getFileToWrite() {
  return active_file_;
}

std::string FileManagerStrategy::getFileToRead() {
  // if we have stored files, pop from the start of the list and return that filename
  // if we do not have stored files, and the active file has data, switch active file and return the existing active file.
  if (!storage_files_.empty()) {
    const std::string oldest_file = storage_files_.front();
    storage_files_.pop_front();
    return oldest_file;
  }

  // TODO: Lock active file?
  if (getActiveFileSize() > 0) {
    const std::string file_path = active_file_;
    rotateActiveFile();
    return file_path;
  }

  throw "No files available for reading";
}

void FileManagerStrategy::discoverStoredFiles() {
  for (const auto & entry : fs::directory_iterator(storage_directory_)) {
    // TODO: Check file has correct headers to ensure it's the correct type for our system
    addFileNameToStorage(entry.path());
  }
}

void FileManagerStrategy::addFileNameToStorage(const std::string filename) {
  storage_files_.push_back(filename);
}

void FileManagerStrategy::rotateActiveFile() {
  // TODO create using UUID or something.
  active_file_ = storage_directory_ + "active_file.log";
}

/**
 * Returns the file size of the active file in bytes.
 * @return sizeInBytes
 */
uintmax_t FileManagerStrategy::getActiveFileSize() {
  return fs::file_size(active_file_);
}

}  // namespace Utils
}  // namespace CloudWatchLogs
}  // namespace Aws
