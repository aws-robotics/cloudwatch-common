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
#include <experimental/filesystem>
#include <dataflow_lite/utils/service.h>
#include <file_management/file_upload/task_utils.h>
#include <aws/core/utils/json/JsonSerializer.h>

namespace Aws {
namespace FileManagement {

enum TokenStatus {
  ACTIVE,
  INACTIVE
};

using DataToken = uint64_t;

static constexpr const char* kPositionKey = "position";
static constexpr const char* kEofKey = "eof";
static constexpr const char* kFilePathKey = "file_path";

/**
 * Stores file token information for the purpose of tracking read locations.
 */
class FileTokenInfo {
public:

  FileTokenInfo() = default;

  explicit FileTokenInfo(const std::string &file_path, const long position, const bool eof) :
  file_path_{file_path},
  position_(position),
  eof_(eof)
  {

  };

  explicit FileTokenInfo(std::string &&file_path, const long position, const bool eof) :
      file_path_{std::move(file_path)},
      position_(position),
      eof_(eof)
  {

  };

  FileTokenInfo(const FileTokenInfo &info) :
      file_path_{info.file_path_},
      position_(info.position_),
      eof_(info.eof_)
  {

  };

  std::string serialize() const {
    Aws::Utils::Json::JsonValue json_value;
    const Aws::String file_path(file_path_.c_str());
    json_value
        .WithInt64(kPositionKey, position_)
        .WithBool(kEofKey, eof_)
        .WithString(kFilePathKey, file_path);
    return std::string(json_value.View().WriteCompact().c_str());
  }

  void deserialize(const std::string token_info_json) {
    const Aws::String aws_str(token_info_json.c_str());
    const Aws::Utils::Json::JsonValue json_value(aws_str);
    if (!json_value.WasParseSuccessful()) {
      throw std::runtime_error("Unable to parse JSON");
    }
    auto view = json_value.View();
    position_ = view.GetInt64(kPositionKey);
    eof_ = view.GetBool(kEofKey);
    file_path_ = view.GetString(kFilePathKey).c_str();
  }


  std::string file_path_;
  long position_ = 0;
  bool eof_;
};

inline bool operator==(const FileTokenInfo& lhs, const FileTokenInfo& rhs){
  return lhs.eof_ == rhs.eof_ && lhs.position_ == rhs.position_ && lhs.file_path_ == rhs.file_path_;
}

inline bool operator!=(const FileTokenInfo& lhs, const FileTokenInfo& rhs){ return !(lhs == rhs); }

class DataManagerStrategy : public Service {
public:
  DataManagerStrategy() = default;
  virtual ~DataManagerStrategy() = default;

  virtual bool isDataAvailable() = 0;

  virtual DataToken read(std::string &data) = 0;

  virtual void write(const std::string &data) = 0;

  /**
   * Mark a token as 'done' so the DataManager knows the piece of
   * data associated with that token can be cleaned up.
   * @param token
   * @throws std::runtime_exception for token not found
   */
  virtual void resolve(const DataToken &token, bool is_success) = 0;
};

struct TokenStoreOptions {
  std::string backup_directory;
};

/**
 * Stores all tokens and manages failed or loaded tokens.
 */
class TokenStore {
public:

  TokenStore() = default;

  explicit TokenStore(const TokenStoreOptions &options);

  /**
   * @param file_name to lookup
   * @return true if a staged token is available to read for that file
   */
  bool isTokenAvailable(const std::string &file_name) const;

  /**
   * @param file_name to lookup
   * @return the file token for that file
   */
  FileTokenInfo popAvailableToken(const std::string &file_name);

  /**
   * Create a token with the file name, stream position, and whether or not this is the last token in the file.
   *
   * @param file_name
   * @param streampos
   * @param is_eof
   * @return
   */
  DataToken createToken(const std::string &file_name, const long & streampos, bool is_eof);

  /**
   * Fail a token.
   *
   * @param token to fail
   * @return token info that was failed
   * @throws std::runtime_exception if token not found
   */
  FileTokenInfo fail(const DataToken &token);

  /**
   * Return the file path
   * @param token
   * @return token info which was resolved
   * @throws std::runtime_exception if token not found
   */
  FileTokenInfo resolve(const DataToken &token);

  /**
   * Backup the first unacked token and all failed tokens into a vector.
   * @return vector to tokens
   */
  std::vector<FileTokenInfo> backup();

  /**
   * Backup the token store to a file on disk
   * @param directory
   */
  void backupToDisk();

  /**
   * Restore tokens from a vector
   * @param std::vector<FileTokenInfo>
   */
  void restore(const std::vector<FileTokenInfo> &file_tokens);

  /**
   * Restore the token store from a file saved to disk
   */
  void restoreFromDisk();



private:
  void initializeBackupDirectory();

  std::unordered_map<DataToken, FileTokenInfo> token_store_;
  std::unordered_map<std::string, std::list<DataToken>> file_tokens_;
  std::unordered_map<std::string, FileTokenInfo> staged_tokens_;

  TokenStoreOptions options_;

  std::random_device rand_device;
};

/**
 * File manager strategy options.
 */
struct FileManagerStrategyOptions {
  std::string file_prefix;
  std::string storage_directory;
  std::string file_extension;
  uint maximum_file_size_in_bytes;
  uint storage_limit_in_bytes;
};


/**
 * Manages how files are split up, which files to write to and read when requested.
 */
class FileManagerStrategy : public DataManagerStrategy {
public:
  explicit FileManagerStrategy(const FileManagerStrategyOptions &options);

  ~FileManagerStrategy() = default;

  bool start() override;

  bool isDataAvailable() override;

  DataToken read(std::string &data) override;

  void write(const std::string &data) override;

  void resolve(const DataToken &token, bool is_success) override;

  bool shutdown() override;

private:
  void initializeStorage();

  void initializeTokenStore();

  void discoverStoredFiles();

  void deleteFile(const std::string &file_path);

  std::string getFileToRead();

  void checkIfFileShouldRotate(const uintmax_t &new_data_size);

  void rotateWriteFile();

  void checkIfStorageLimitHasBeenReached(const uintmax_t &new_data_size);

  void deleteOldestFile();

  void addFilePathToStorage(const std::experimental::filesystem::path &file_path);

  /**
   * Stored files to read from in order from most recent to oldest.
   */
  std::list<std::string> stored_files_;

  /**
   * Disk space used by all stored files. Does not include active_write_file_size_.
   */
  std::atomic<size_t> stored_files_size_;

  /**
   * Current file name to write to.
   */
  std::string active_write_file_;
  std::atomic<size_t> active_write_file_size_;

  std::string active_read_file_;
  std::unique_ptr<std::ifstream> active_read_file_stream_ = nullptr;

  /**
   * User configurable settings
   */
  FileManagerStrategyOptions options_;

  /**
   * Size of each batch when reading from a file.
   * The Size corresponds to the number of lines read from the file
   */
  uint8_t batch_size = 1;

  /**
   * Stores which tokens to read from.
   */
  std::unique_ptr<TokenStore> token_store_ = nullptr;
};

}  // namespace FileManagement
}  // namespace Aws
