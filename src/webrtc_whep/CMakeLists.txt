cmake_minimum_required(VERSION 3.8)
project(webrtc_whep)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(JPEG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
find_package(libyuv_vendor REQUIRED)
find_package(libdatachannel_vendor REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/webrtc_whep.cpp
  src/common/i420_buffer.cpp
  src/common/h264_buffer.cpp
  src/encoder/libav_encoder.cpp
  src/rtc/rtc_peer.cpp
  src/signaling/http_service.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  ${Boost_INCLUDE_DIRS}
  ${AVCODEC_INCLUDE_DIRS}
  ${AVUTIL_INCLUDE_DIRS}
)

target_compile_features(${PROJECT_NAME} PUBLIC c_std_99 cxx_std_17)  # Require C99 and C++17
ament_target_dependencies(
  ${PROJECT_NAME}
  "rclcpp"
  "sensor_msgs"
  "rclcpp_components"
  "libyuv_vendor"
  "libdatachannel_vendor"
)

target_link_libraries(${PROJECT_NAME}
  Boost::system
  Boost::thread
  JPEG::JPEG
  ${AVCODEC_LIBRARIES}
  ${AVUTIL_LIBRARIES}
)

rclcpp_components_register_node(${PROJECT_NAME}
  PLUGIN "${PROJECT_NAME}::WebrtcWhep"
  EXECUTABLE ${PROJECT_NAME}_node
)

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib
)
ament_export_targets(${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
