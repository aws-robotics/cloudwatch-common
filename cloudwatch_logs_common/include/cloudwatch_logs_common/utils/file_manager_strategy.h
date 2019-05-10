//
// Created by tim on 5/9/19.
//

#pragma once

#include <list>
#include "cloudwatch_logs_common/utils/task_utils.h"

namespace Aws {
namespace CloudWatchLogs {
namespace Utils {

/**
 * Manages how files are split up, which files to write to and read when requested.
 */
class FileManagerStrategy {
public:
    FileManagerStrategy();

    ~FileManagerStrategy() = default;

    virtual std::string read();

    virtual void write(std::string data);

    virtual void deleteFile(std::string fileName);

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

    /**
     * User configurable settings
     */
    std::string storage_directory_ = "/tmp/";
    int maximum_file_size_in_bytes_ = 1024 * 1024;
};


}  // namespace Utils
}  // namespace CloudwatchLogs
}  // namespace Aws
