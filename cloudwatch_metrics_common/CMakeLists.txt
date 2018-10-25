cmake_minimum_required(VERSION 2.8.3)
project(cloudwatch_metrics_common)
set(CLOUDWATCH_METRICS_COMMON_VERSION 1.0.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++14 ")

find_package(aws_common REQUIRED)
if(AWSSDK_FOUND)
  set(SERVICE monitoring)
  AWSSDK_DETERMINE_LIBS_TO_LINK(SERVICE OUTPUT)
  link_directories("${AWSSDK_LIB_DIR}")
endif()

set(ALL_SRC_FILES src/utils/cloudwatch_facade.cpp src/metric_manager.cpp src/metric_manager_factory.cpp src/metric_publisher.cpp)

add_library(${PROJECT_NAME} ${ALL_SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${OUTPUT})
target_include_directories(${PROJECT_NAME} PRIVATE ${aws_common_INCLUDE_DIRS} ${AWSSDK_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)


#############
## Testing ##
#############

enable_testing()
find_package(GTest QUIET)
if(NOT GTEST_FOUND)
  message(WARNING "Could not find GTest. Not building unit tests.")
else()
  add_executable(test_cloudwatch_metrics_common
    test/main_test.cpp
    test/shared_object_test.cpp
    test/metric_manager_test.cpp
    test/metric_publisher_test.cpp)
  target_include_directories(test_cloudwatch_metrics_common PRIVATE ${aws_common_INCLUDE_DIRS} ${AWSSDK_INCLUDE_DIR})
  target_link_libraries(test_cloudwatch_metrics_common
    ${GTEST_LIBRARIES}
    pthread
    ${aws_common_LIBRARIES}
    ${PROJECT_NAME}
    ${rclcpp_LIBRARIES})
  add_test(NAME test_cloudwatch_metrics_common COMMAND test_cloudwatch_metrics_common --gtest_output=xml:test_results/)
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
