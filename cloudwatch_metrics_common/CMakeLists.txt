cmake_minimum_required(VERSION 2.8.3)
project(cloudwatch_metrics_common)
set(CLOUDWATCH_METRICS_COMMON_VERSION 1.0.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++14 ")

## Enable strict compiler flags if possible.
#include(CheckCXXCompilerFlag)
#set(FLAGS -pedantic -Wno-long-long -Wall -Wextra -Wcast-align -Wcast-qual -Wformat -Wwrite-strings -Wconversion -Wmissing-declarations)
#foreach(FLAG ${FLAGS})
#  check_cxx_compiler_flag(${FLAG} R${FLAG})
#  if(${R${FLAG}})
#    set(WARNING_CXX_FLAGS "${WARNING_CXX_FLAGS} ${FLAG}")
#  endif()
#endforeach()

if(NOT DEFINED CXX_DISABLE_WERROR)
  set(WARNING_CXX_FLAGS "-Werror ${WARNING_CXX_FLAGS}")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_CXX_FLAGS}")

find_package(dataflow_lite REQUIRED)
find_package(file_management REQUIRED)
find_package(aws_common REQUIRED)

if(AWSSDK_FOUND)
  set(SERVICE monitoring)
  AWSSDK_DETERMINE_LIBS_TO_LINK(SERVICE OUTPUT)
  link_directories("${AWSSDK_LIB_DIR}")
endif()

set(ALL_SRC_FILES
        src/utils/cloudwatch_facade.cpp
        src/metric_manager.cpp
        src/metric_manager_factory.cpp
        src/metric_publisher.cpp
        src/utils/metric_file_manager.cpp
        src/utils/metric_serialization.cpp
        )

add_library(${PROJECT_NAME} ${ALL_SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${OUTPUT} ${file_management_LIBRARIES} ${dataflow_lite_LIBRARIES} )
target_include_directories(${PROJECT_NAME} PRIVATE
        ${aws_common_INCLUDE_DIRS}
        ${AWSSDK_INCLUDE_DIR}
        ${file_management_INCLUDE_DIRS}
        ${dataflow_lite_INCLUDE_DIRS} )
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)


#############
## Testing ##
#############

enable_testing()
find_common_test_packages()
if(GMOCK_LIBRARY)
  add_common_gtest(test_cloudwatch_metrics
          test/main_test.cpp
          test/shared_object_test.cpp
          test/metric_manager_test.cpp
          test/metric_publisher_test.cpp
          test/file_manager_test.cpp
          test/metric_serialization_test.cpp
          )
  target_include_directories(test_cloudwatch_metrics
          PRIVATE include
          PRIVATE ${aws_common_INCLUDE_DIRS}
          )
  target_link_libraries(test_cloudwatch_metrics
          ${GTEST_LIBRARIES}
          pthread
          ${GMOCK_LIBRARY}
          ${PROJECT_NAME})
endif()

#############
## Install ##
#############

# Mark executables and/or libraries for installation
install(TARGETS ${PROJECT_NAME}
  EXPORT "${PROJECT_NAME}-targets"
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

## Mark cpp header files for installation
install(
  DIRECTORY include/
  DESTINATION include/
)

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
