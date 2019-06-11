cmake_minimum_required(VERSION 2.8.3)
project(cloudwatch_logs_common)
set(CLOUDWATCH_LOGS_COMMON_VERSION 1.0.0)

# Default to C11
if(NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 11)
endif()
# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()

# Enable strict compiler flags if possible.
include(CheckCXXCompilerFlag)
# Removed -Wmissing-declarations until gmock is ignored
set(FLAGS -pedantic -Wno-long-long -Wall -Wextra -Wcast-align -Wcast-qual -Wformat -Wwrite-strings -Wconversion)
foreach(FLAG ${FLAGS})
  check_cxx_compiler_flag(${FLAG} R${FLAG})
  if(${R${FLAG}})
    set(WARNING_CXX_FLAGS "${WARNING_CXX_FLAGS} ${FLAG}")
  endif()
endforeach()

if(NOT DEFINED CXX_DISABLE_WERROR)
  set(WARNING_CXX_FLAGS "-Werror ${WARNING_CXX_FLAGS}")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_CXX_FLAGS}")

find_package(aws_common REQUIRED)

if(AWSSDK_FOUND)
  set(SERVICE logs)
  AWSSDK_DETERMINE_LIBS_TO_LINK(SERVICE OUTPUT)
  link_directories("${AWSSDK_LIB_DIR}")
endif()

set(ALL_SRC_FILES
    src/log_batcher.cpp
    src/log_manager_factory.cpp
    src/utils/cloudwatch_facade.cpp
    src/utils/log_file_manager.cpp
    src/file_upload/file_manager_strategy.cpp
    src/dataflow/status_monitor.cpp
    src/log_publisher.cpp)

add_library(${PROJECT_NAME} SHARED ${ALL_SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${OUTPUT} stdc++fs)
target_include_directories(${PROJECT_NAME} PRIVATE ${AWSSDK_INCLUDE_DIRS} include ${aws_common_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)


#############
## Install ##
#############
install(TARGETS ${PROJECT_NAME}
        EXPORT "${PROJECT_NAME}-targets"
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(DIRECTORY include/
        DESTINATION include)

export(TARGETS ${PROJECT_NAME}
        FILE "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-targets.cmake")
export(PACKAGE ${PROJECT_NAME})

CONFIGURE_FILE(${PROJECT_NAME}Config.cmake.in
               ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake @ONLY)
CONFIGURE_FILE(${PROJECT_NAME}ConfigVersion.cmake.in
               ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake @ONLY)

INSTALL(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake DESTINATION share/${PROJECT_NAME}/cmake)
INSTALL(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake DESTINATION share/${PROJECT_NAME}/cmake)
INSTALL(EXPORT ${PROJECT_NAME}-targets DESTINATION share/${PROJECT_NAME}/cmake)


#############
## Testing ##
#############

enable_testing()
find_common_test_packages()
if(GMOCK_LIBRARY)
  message("Building tests")
  add_common_gtest(test_cloudwatch_logs_common
        test/main_test.cpp
        test/log_publisher_test.cpp
    #    test/cloudwatch_facade_test.cpp
        )

  target_include_directories(test_cloudwatch_logs_common
        PRIVATE include
        PRIVATE ${aws_common_INCLUDE_DIRS}
        PUBLIC ${PROJECT_SOURCE_DIR}/test/include
        )
  target_link_libraries(test_cloudwatch_logs_common
        ${GTEST_LIBRARIES}
        pthread
        ${GMOCK_LIBRARY}
        ${PROJECT_NAME})

  add_common_gtest(test_common_utilities
          test/file_management/file_manager_test.cpp
          test/file_management/file_manager_strategy_test.cpp
          test/file_management/file_upload_streamer_test.cpp
          test/file_management/token_test.cpp
          test/dataflow/observed_queue_test.cpp
          test/dataflow/queue_monitor_test.cpp
          )

  target_include_directories(test_common_utilities
        PRIVATE include
        PRIVATE ${aws_common_INCLUDE_DIRS}
        PUBLIC ${PROJECT_SOURCE_DIR}/test/include
        )
  target_link_libraries(test_common_utilities
        ${GTEST_LIBRARIES}
        pthread
        ${GMOCK_LIBRARY}
        ${PROJECT_NAME})

  add_common_gtest(test_pipelines
          test/pipeline_test.cpp
          test/utils/observable_object_test.cpp
          test/utils/runnable_service_test.cpp
          )

  target_include_directories(test_pipelines
          PRIVATE include
          PRIVATE ${aws_common_INCLUDE_DIRS}
          PUBLIC ${PROJECT_SOURCE_DIR}/test/include
          )
  target_link_libraries(test_pipelines
          ${GTEST_LIBRARIES}
          pthread
          ${GMOCK_LIBRARY}
          ${PROJECT_NAME})
endif()
