#include <iostream>
#include <experimental/filesystem>
#include "cloudwatch_logs_common/utils/file_manager_strategy.h"

namespace fs = std::experimental::filesystem;

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

void FileManagerStrategy::initialize() {
  discoverStoredFiles();
  rotateActiveFile();
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

  return nullptr;
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
