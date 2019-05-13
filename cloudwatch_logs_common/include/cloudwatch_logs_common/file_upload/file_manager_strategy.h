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

/**
 * Manages how files are split up, which files to write to and read when requested.
 */
class FileManagerStrategy {
public:
    FileManagerStrategy();

    ~FileManagerStrategy() = default;

    virtual FileInfo read(std::string &data);

    virtual void write(const std::string &data);

    virtual void deleteFile(const std::string &fileName);

    virtual void initialize();

    virtual void rotateActiveFile() ;

    virtual void discoverStoredFiles() ;

    virtual void addFileNameToStorage(std::string filename);

    virtual uintmax_t getActiveFileSize() ;
    /**
     * Get the file name to write to.
     *
     * @return current file name
     */
    virtual std::string getFileToWrite() ;

    virtual std::string getFileToRead();

private:
    /**
     * Current file name to write to.
     */
    std::string active_file_;
    std::list<std::string> storage_files_;

    std::unique_ptr<std::ifstream> current_log_file_= nullptr;

    /**
     * User configurable settings
     */
    std::string storage_directory_ = "/tmp/";
    int maximum_file_size_in_bytes_ = 1024 * 1024;
};


}  // namespace FileManagement
}  // namespace Aws
