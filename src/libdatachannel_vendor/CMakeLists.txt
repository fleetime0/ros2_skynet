cmake_minimum_required(VERSION 3.8)
project(libdatachannel_vendor VERSION 0.1.0)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

add_subdirectory(libdatachannel)

install(DIRECTORY libdatachannel/include/ DESTINATION include)

add_library(libdatachannel_vendor INTERFACE)
target_link_libraries(libdatachannel_vendor INTERFACE datachannel)
target_include_directories(libdatachannel_vendor INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libdatachannel/include>
  $<INSTALL_INTERFACE:include>
)

install(TARGETS datachannel libdatachannel_vendor
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