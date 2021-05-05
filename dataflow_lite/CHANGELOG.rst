^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package dataflow_lite
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1.1.5 (2020-10-14)
------------------
No changes.

1.1.4 (2020-09-17)
------------------
* Bumping version to match bloom release (`#51 <https://github.com/aws-robotics/cloudwatch-common/issues/51>`_)
  Bumping version to 1.1.3
* Fix linting issues found by clang-tidy 6.0 (`#50 <https://github.com/aws-robotics/cloudwatch-common/issues/50>`_)
  * clang-tidy fixes
  * revert explicit constructor declarations to maintain API compatbility
  * clang-tidy linting issues fixed manually
  * fix unit tests build break
* Increase package version numbers to 1.1.2 (`#44 <https://github.com/aws-robotics/cloudwatch-common/issues/44>`_)
* Contributors: Miaofei Mei, Nick Burek, Ragha Prasad

1.1.1 (2019-09-10)
------------------
* Disable error on cast-align warning to support ARMhf builds (`#41 <https://github.com/aws-robotics/cloudwatch-common/issues/41>`_)
  * Remove -Wcast-align flag to support ARMhf builds
  *  - bumped versions to 1.1.1
  - restored cast-align, but added as a warning
  - removed unused headers
  * Removed duplicate Werror
* synchronize version of new packages with rest of the packages in the repo (`#38 <https://github.com/aws-robotics/cloudwatch-common/issues/38>`_)
  Signed-off-by: Miaofei  
* Merge pull request `#36 <https://github.com/aws-robotics/cloudwatch-common/issues/36>`_ from aws-robotics/guard_test_libs
  Added guards for test libraries
* Added guards for test libraries
* Offline logs feature (`#34 <https://github.com/aws-robotics/cloudwatch-common/issues/34>`_)
  * Add file manager and upload complete callback
  Signed-off-by: Miaofei
  * Fixed merge conflict
  Signed-off-by: Miaofei
  * Add file manager test
  Still deciding if the write should just go in the FileManagerStrategy.
  Signed-off-by: Miaofei
  * Add FileManager to log manager factory
  Signed-off-by: Miaofei 
  * Add generic file upload logic
  Signed-off-by: Miaofei
  * Add reading, discovery and rotation to FileManagerStrategy
  - Add function getFileToRead that returns the best file to start reading
  from when sending data from the file to CloudWatch.
  - Add function discoverStoredFiles which finds all files in the
  storage directory and keeps them, in case there were offline logs still
  hanging around from a previous run.
  - Add rotateActiveFile which creates a new file to write to. This is
  used when we want to read from the current active file, or when the file
  size limit has been reached.
  Signed-off-by: Miaofei
  * Add file management system.
  Signed-off-by: Miaofei
  * Link file management with publisher
  Signed-off-by: Miaofei
  * Add test for file management system
  Signed-off-by: Miaofei
  * Add reading and writing to FileManagerStrategy
  - This way the file manager strategy can handle all the core file data
  stuff, keeping track of the size of the active file and rotating it when
  neccessary.
  Signed-off-by: Miaofei
  * cleanup
  Signed-off-by: Miaofei
  * Don't initialize file_manager_strategy yet
  Signed-off-by: Miaofei
  * Add file upload statistics
  Signed-off-by: Miaofei
  * Move file management files to isolated package
  Signed-off-by: Miaofei
  * Moved tests to file_manager folder
  Signed-off-by: Miaofei  
  * Update offline test location
  Signed-off-by: Miaofei
  * Add comments to file management interfaces
  Signed-off-by: Miaofei  
  * Add more comments to file uploading
  Signed-off-by: Miaofei  
  * Add futures/promises to file upload tasks
  Signed-off-by: Miaofei
  * Add blocking and observed queue interface
  Signed-off-by: Miaofei
  * Add comments to observed_queue.h
  Signed-off-by: Miaofei
  * Make batch size configurable for file upload manager
  Signed-off-by: Miaofei
  * Add subscribe method to file upload manager
  Signed-off-by: Miaofei
  * Move queues and status monitor to dataflow folder
  Signed-off-by: Miaofei
  * Atomatize status monitor
  Signed-off-by: Miaofei
  * Add working dataflow pipeline
  Signed-off-by: Miaofei
  * Add template argument for file management factory
  Signed-off-by: Miaofei
  * Add priority between queues functionality
  Signed-off-by: Miaofei
  * Fix test and rename namespace for dataflow
  Signed-off-by: Miaofei
  * Cleanup test
  Signed-off-by: Miaofei
  * Organize priority options
  Signed-off-by: Miaofei
  * Rename FileUploadManager to FileUploadStreamer
  Signed-off-by: Miaofei
  * Add comments
  Signed-off-by: Miaofei
  * Convert LogManager to sink and ILogManager
  Signed-off-by: Miaofei
  * Amend UploadFunction status type
  Signed-off-by: Miaofei
  * Add milliseconds to dequeue
  Signed-off-by: Miaofei
  * Add timeout to dequeue()
  Signed-off-by: Miaofei
  * Add operator >> overload for source to input stage
  Signed-off-by: Miaofei
  * Adding tests for file manager strategy
  Signed-off-by: Miaofei
  * Use Tokens with FileManagerStrategy
  - FileManagerStrategy now only has basic read and write functions. When
  reading data you're given a token. When done sending this to CloudWatch
  you can call resolve() on the token which marks that section of the file
  as complete. After all sections of a file have uploaded the file is
  deleted.
  - FileManagerStrategy also now inherits from DataManagerStrategy so that
  in the future people can build new systems for managing their offline
  logs and easily hook them into the existing framework.
  Signed-off-by: Miaofei  
  * Fix operator overload for InputStage
  Signed-off-by: Miaofei  
  * Remove unnecessary external class declaration
  Signed-off-by: Miaofei  
  * Add tests for file upload streamer
  Signed-off-by: Miaofei  
  * Reformat file management
  Signed-off-by: Miaofei  
  * Reformat log file names and discovery
  Signed-off-by: Miaofei  
  * Cleanup, add Tests
  - Rename file_name to file_path to better describe what it represents
  - Add more tests for FileManagerStrategy
  Signed-off-by: Miaofei  
  * Delete old files when storage limit is reached
  - Add a storage_limit option. When this is reached the oldest file on
  disk will be deleted to clear space.
  - The storage limit is checked before data is written so that the size
  of files on disk can never be over the storage limit.
  Signed-off-by: Miaofei  
  * Task factory (`#6 <https://github.com/aws-robotics/cloudwatch-common/issues/6>`_)
  * Don't use atomic memory operations
  * generic observer definition
  * init connection monitor
  * Added publisher interface
  Refactored log publisher to use interface
  * Added IPublisher Interface definition and as Task member
  Added Publisher implementation and templated for data type
  * Added LogBatcher (renamed LogManager)
  Added DataStreamer interface
  * Rename DataStreamer interface to DataBatcher to reduce confusion
  * Added TaskFactory
  Added Publisher Interface
  Added factory to LogBatcher
  * BasicTask uses a shared pointer for templated data
  * Added publishing when the DataBatcher reaches a specific size
  * Use generic FileManager in the TaskFactory
  Add uploadCompleteStatus as an abstract FileManager method
  * Added Log Service
  - worker thread to dequeue tasks
  - common owner of various actors (file streaming, log batching)
  - templated to  be a generic interface
  * Added Task cancel method
  * minor change to push
  * Fix file permissions
  * Added dequeue with timeout for LogService
  * Finished publisher base class implementation
  Added new states for LogPublisher
  * Removed shared object from logs
  * Moved AWS SDK init and shutdown into publisher
  * uncomment file streamer code - does not compile!
  Signed-off-by: Miaofei  
  * Fixed build, still todo fix file streamer
  Signed-off-by: Miaofei  
  * Simple pipeline tests (`#9 <https://github.com/aws-robotics/cloudwatch-common/issues/9>`_)
  * initial commit for simple test
  * Added test for batched size
  * minor comments
  * Added ObservableObject class
  Added simple ObservableObject tests
  Integrated ObservableObject into base publisher class
  File Streamer uses ObservableObject registration on publisher owned
  state
  * Added basic service interface for generic init, start, and shutdown
  Signed-off-by: Miaofei
  * Remove task factory
  * Task change proposal
  * Remove the task factory
  * Working file management tests
  * Fix tests and minor logic due to merge
  Signed-off-by: Miaofei
  * Address comments for task change proposal
  Signed-off-by: Miaofei
  * Delete task_factory.h
  Signed-off-by: Miaofei
  * Add cloudwatch options
  * Use explict set/add sink and source functions
  Signed-off-by: Miaofei  
  * Cleanups (`#12 <https://github.com/aws-robotics/cloudwatch-common/issues/12>`_)
  * Added cancel flag
  * Added thread handling (same as log service) to file upload streamer
  * Added RunnableService
  * Added ObservableObject tests
  Added documentation
  * Added RunnableService test
  Added documentation
  Converted streamer and log service to runnables
  * Merge Conflict Fixes
  Added sanity (empty) tests
  * Fixed pipeline tests
  * Addressed review comments
  * fix for merge conflict
  Signed-off-by: Miaofei
  * Thorough testing of token system
  Signed-off-by: Miaofei
  * Clear file streamer queue on failure to upload
  * Add locks around dequeue
  Signed-off-by: Miaofei  
  * Add basic mutex synchronization for ObservedQueue
  Signed-off-by: Miaofei  
  * Remove uploadStatusComplete from FileManager
  Remove the uploadStatusComplete function from FileManager as it is not the responsibility of the file manager to determine if data should be written. Instead, a lambda should be used to first check for upload failure then write to the file manager.
  Signed-off-by: Miaofei  
  * Add construct from backup for TokenStore
  Signed-off-by: Miaofei  
  * Fix synchronized queue and address comments
  Signed-off-by: Miaofei  
  * Enable build flags (`#16 <https://github.com/aws-robotics/cloudwatch-common/issues/16>`_)
  * Added build flags per team process
  * Addressed some build fixes found by flags
  * Fix build issues with new build flags
  Signed-off-by: Miaofei  
  * Fix publishing (`#15 <https://github.com/aws-robotics/cloudwatch-common/issues/15>`_)
  * Removed initialize method (not needed) for service
  Fixed publishing
  Reinit AWS SDK each time we configure (needed if gone offline)
  * Addressed some ToDos
  Added publisher diagnostics
  Minor cleanups
  Added documentation
  * Fix issue with constant
  * Propgated no network connection state in publisher
  * fix pipeline test teardown
  * Addressed review comments
  * merge fixes
  * Added input checking for CloudWatchService
  Signed-off-by: Miaofei  
  * Don't clear sink on successful upload
  - Add test and fix bug so that the file upload sink is only cleared when
  an upload fails.
  Signed-off-by: Miaofei  
  * ROS-2000: [Test] Full pipeline when there is no internet
  - added input checking for various constructors
  Signed-off-by: Miaofei  
  * ROS-2136: Address migrating core classes to service interface
  - Define Defaults for File Strategy
  - Deleted files are deleted on a new thread
  - Removed code from destructors that may fail
  - CloudWatchService handles start / shutdown of all services
  Signed-off-by: Miaofei  
  * ROS-2001: [Test] Full pipeline when there is intermittent internet
  ROS-2002: [Test] Case when batched data is queued at an untenable rate
  Signed-off-by: Miaofei  
  * Addressed review comments
  Signed-off-by: Miaofei  
  * Move dataflow to separate library
  Signed-off-by: Miaofei  
  * Move file management to separate package directory
  * Modified onPublishStatusChanged in file streamer to remove dependency on cloudwatch
  Signed-off-by: Miaofei  
  * ROS-2147: Move DataBatcher to utils
  Signed-off-by: Miaofei  
  *  - addressed review comments
  - added documentation
  - moved waiter test utility to separate implementation
  Signed-off-by: Miaofei  
  * ROS-2166: I can check the state of the CloudWatch publishing service
  Signed-off-by: Miaofei  
  * Add Metric File Manager to Cloudwatch Metrics Common
  Signed-off-by: Miaofei  
  * Improve metric serialization, add tests.
  Signed-off-by: Miaofei  
  * Add Serialization of StatisticValues
  Signed-off-by: Miaofei  
  * Add serializing of Dimensions, Value and Values
  Signed-off-by: Miaofei  
  * Doc and coding style improvements
  Signed-off-by: Miaofei  
  * Squashed commit of the following:

  Author: Devin Bonnie
  Date:   Fri Jun 21 13:52:29 2019 -0700
  Various fixes from rebasing

  Author: Devin Bonnie
  Date:   Thu Jun 20 16:39:58 2019 -0700
  - addressed review comments
  - added metrics definition file
  - removed configure from publisher interface

  Author: Devin Bonnie 
  Date:   Mon Jun 17 11:43:57 2019 -0700
  ROS-2055: Implement DataBatcher for Metrics
  ROS-2056: Implement MetricService


  Author: Devin Bonnie 
  Date:   Fri Jun 14 23:55:23 2019 -0700
  ROS-2057: Create immutable metric container

  Author: Devin Bonnie 
  Date:   Fri Jun 14 16:50:48 2019 -0700
  Moved CloudwatchService to utils

  Author: Devin Bonnie 
  Date:   Fri Jun 14 11:08:40 2019 -0700
  ROS-2055: Implement Metric Publisher
  - moved Publisher to utilities
  - moved CloudWatchService to utilities
  - cleaned up headers
  - fixed namespace issues
  Signed-off-by: Miaofei  
  * ROS-2226: [Bug] Metrics Facade Class does not properly set network disconnected state
  Signed-off-by: Miaofei  
  * Backup TokenStore to disk
  - Add TokenStoreOptions so the user can configure the directory the token store is backed up to.
  - On shutdown save the token store and all active tokens out to disk in
  JSON format.
  - On startup load the tokenstore from the file saved on disk.
  - Tests for shutdown/startup
  Signed-off-by: Miaofei  
  * Improve serialize function, catch invalid JSON
  - Add a new serialize function instead of overloading << in TokenStore
  - Catch and continue if we have trouble parsing the TokenStore backup
  file.
  Signed-off-by: Miaofei  
  * Improve naming and initialization of variables
  Signed-off-by: Miaofei  
  * Add better random number generator
  Signed-off-by: Miaofei  
  * Code style fixes
  Signed-off-by: Miaofei  
  * ROS-2051: Add FileManagement Pipeline to CW Metrics
  Signed-off-by: Miaofei  
  * Moving options around
  - Moving TokenStore and FileManagerStrategy options to a separate file
  so that it can be included and set by the upstream packages.
  - Renaming the Dataflow options to UploaderOptions
  - Creating one main CloudwatchOptions in both logs and metrics that has FileManagerOptions and
  UploaderOptions inside it.
  Signed-off-by: Miaofei  
  * Change storage limits to kb instead of bytes

  Signed-off-by: Miaofei  
  * File upload streamer integration and unit tested
  *Summary*
  File upload and token cache manages failed and in flight tokens. Files are uploaded when the streamer is notified of an available file and network access.
  Files that are on the system are after FileStreamer shutdown are uploaded on restart.
  * Tested with cloudwatch logs
  * Tested with unit tests
  Signed-off-by: Miaofei  
  * Capitalize W in kDefaultCloudWatchOptions
  Signed-off-by: Miaofei  
  * Pass options correctly, fixing bugs
  - Pass options to the FileManager for logs and metrics
  - Add additional params to handle this option passing.
  Signed-off-by: Miaofei  
  * Add different file storage options for metrics by default
  - Metrics files now go in a metrics directory with metric prefix by
  default, so that they don't get mixed up with offline logs.
  Signed-off-by: Miaofei  
  * DRY'ify, remove magic numbers, fix tests
  - Consolidate duplicate path processing code into one area.
  - Fix magic numbers, move into defines.
  - Fix tests.
  Signed-off-by: Miaofei  
  * ROS-2249: [Bug] Log Publisher implementation does not properly handle token init
  ROS-2250: Restore CloudWatch Logs Facade Unit Test
  Signed-off-by: Miaofei  
  * Added relevant unit tests
  Minor fixes and cleanup
  Signed-off-by: Miaofei  
  *  - CloudWatchClients are now shared pointers instead of unique
  - addressed spacing issues
  - updated CloudWatchLogs facade naming to be consistent with Metrics
  Signed-off-by: Miaofei  
  * Include <random> in header file
  Signed-off-by: Miaofei  
  * Rename variables and error to match config
  - Rename the batch size variables to match the config file names.
  - Update error message so the end user knows what config options are
  wrong.
  Signed-off-by: Miaofei  
  * Allow batch_trigger_publish_size and batch_max_queue_size to be the same
  Signed-off-by: Miaofei  
  * Changing back ot publish size must be less than max queue size
  Signed-off-by: Miaofei  
  * Check batch trigger publish size against kDefaultTriggerSize
  Signed-off-by: Miaofei  
  * ROS-2231: [Bug] Potential locking issue with DataBatcher child classes
  - batcher attempt to flush batched data when shutting down
  - added documentation
  Signed-off-by: Miaofei  
  * Addressed review comments
  Signed-off-by: Miaofei  
  * Fix up param values
  - Remove stream_max_queue_size as it's no longer used.
  - Remove kDefaultUploaderOptions because it's not used as it's always
  replaced by the default values specified in uploader_options struct.
  - Pass batch_max_queue_size and batch_trigger_publish_size to the
  DataBatcher's so they're actually used
  Signed-off-by: Miaofei  
  * ROS-2338: I can configure the amount of streamed data to hold in memory
  Signed-off-by: Miaofei  
  * ROS-2240: Restore existing unit tests
  - added definitions header to logs
  Signed-off-by: Miaofei  
  * Removed extra definitions file
  Signed-off-by: Miaofei  
  * ROS-2341: Publisher state refactor
  Signed-off-by: Miaofei  
  * Fixes bug with trying to upload to cloudwatch in batches that aren't chronologically sorted. https://sim.amazon.com/issues/7cbe72f2-28c6-4771-a202-ab0d72587031
  Signed-off-by: Miaofei  
  * ROS-2346: [Bug] Don't set stats values in metric datums
  Signed-off-by: Miaofei  
  *  - doc additions
  Signed-off-by: Miaofei  
  *  - removed other unsupported types via review
  Signed-off-by: Miaofei  
  * ROS-2263: [Bug] Storage and retry behavior for failed requests
  Signed-off-by: Miaofei  
  * Addressed review comments
  Signed-off-by: Miaofei  
  * Added invalid data handling to metrics
  Signed-off-by: Miaofei  
  * ROS-2368: [Bug] Data is not attempted to be uploaded without an active input
  Signed-off-by: Miaofei  
  * ROS-2369: [Bug] Fix Metrics Serialization Unit Tests
  Signed-off-by: Miaofei  
  * Revert "ROS-2368: [Bug] Data is not attempted to be uploaded without an active input"
  Signed-off-by: Miaofei  
  * ROS-2368: [Bug] Data is not attempted to be uploaded without an active input
  Signed-off-by: Miaofei  
  * ROS-2380: [Bug] CloudWatch Service Shutdown
  Signed-off-by: Miaofei  
  * Fix bug - logs not being uploaded from disk after reconnecting
  - If all files on disk were added to the queue the status was set to
  UNAVAILABLE. Then if they failed to upload the status was never
  restored. This ensures that if a file fails to upload the status is set
  back to AVAILABLE so they can attempt to be uploaded again.
  - Add more DEBUG logs to file management.
  Signed-off-by: Miaofei  
  * Read the newest file in storage instead of the oldest, lock when
  deleting file
  - Read the newest file from storage instead of reading the oldest.
  - When deleting a file to free up storage space, add a lock to ensure
  we're not reading from that same file. If we are then stop reading from
  that file.
  Signed-off-by: Miaofei  
  * Add lock to active write file
  - When checking if the active file should be rotated first lock it to ensure it's not being written to as it's rotated.
  - Add new log to delete oldest file.
  Signed-off-by: Miaofei  
  * Add docs for FileManagerStrategy, cleanup unused code
  - Add documentation to all FileManagerStrategy functions
  - Remove some un-useful code for the FileManagerStrategy
  - Function renaming / cleanup to make more sense.
  Signed-off-by: Miaofei  
  * Remove todo and unused variable
  Signed-off-by: Miaofei  
  * Remove unneccessary initialization and commented out code
  Signed-off-by: Miaofei  
  * ROS-2381: [Bug] Items in memory lost on shutdown
  Signed-off-by: Miaofei  
  * ROS-2421: [Bug] Ensure FileManager thrown exceptions are handled
  Signed-off-by: Miaofei  
  *  - addressed review comments
  - changed file upload streamer wait timeout from 1 minute to 5 minutes
  Signed-off-by: Miaofei  
  * Addressed terse variable names
  Signed-off-by: Miaofei  
  * increment minor version
  Signed-off-by: Miaofei  
  * fix compilation errors in unit tests
  Signed-off-by: Miaofei  
  * fix more compilation errors found in dashing
  Signed-off-by: Miaofei  
  * fix unit test failures
  Signed-off-by: Miaofei  
* Contributors: Devin Bonnie, M. M
