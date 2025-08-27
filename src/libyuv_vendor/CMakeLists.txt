cmake_minimum_required(VERSION 3.8)
project(libyuv_vendor VERSION 0.1.0)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

add_subdirectory(libyuv)

install(DIRECTORY libyuv/include/ DESTINATION include)

add_library(libyuv_vendor INTERFACE)
target_link_libraries(libyuv_vendor INTERFACE yuv)
target_include_directories(libyuv_vendor INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libyuv/include>
  $<INSTALL_INTERFACE:include>
)

install(TARGETS yuv libyuv_vendor
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

install(EXPORT export_${PROJECT_NAME}
  DESTINATION share/${PROJECT_NAME}/cmake
)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)

ament_package()