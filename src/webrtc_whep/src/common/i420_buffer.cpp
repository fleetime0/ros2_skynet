#include "common/i420_buffer.h"

#include <cstring>
#include <iostream>

#include "libyuv.h"
#include "rclcpp/rclcpp.hpp"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

static inline std::size_t AlignUp(std::size_t value, std::size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

static uint32_t encoding_to_fourcc(const std::string &encoding) {
  if (encoding == "rgb8") {
    return libyuv::FOURCC_RGB3;
  } else if (encoding == "bgr8") {
    return libyuv::FOURCC_BGR3;
  } else if (encoding == "yuyv" || encoding == "yuv422_yuy2") {
    return libyuv::FOURCC_YUY2;
  } else if (encoding == "nv12") {
    return libyuv::FOURCC_NV12;
  } else if (encoding == "yuv420" || encoding == "i420") {
    return libyuv::FOURCC_I420;
  } else {
    return libyuv::FOURCC_ANY;
  }
}

std::shared_ptr<I420Buffer> ImageToI420(const sensor_msgs::msg::Image &image_msg) {
  const auto width = image_msg.width;
  const auto height = image_msg.height;
  const auto format = encoding_to_fourcc(image_msg.encoding);
  const uint8_t *src_data = image_msg.data.data();
  const size_t src_size = image_msg.data.size();
  std::shared_ptr<I420Buffer> i420_buffer(I420Buffer::Create(width, height, kBufferAlignment));

  if (format == libyuv::FOURCC_I420) {
    memcpy(i420_buffer->MutableDataY(), src_data, src_size);
  } else {
    if (libyuv::ConvertToI420(src_data, src_size, i420_buffer.get()->MutableDataY(), i420_buffer.get()->StrideY(),
                              i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
                              i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(), 0, 0, width, height,
                              width, height, libyuv::kRotate0, format) < 0) {
      RCLCPP_ERROR(rclcpp::get_logger("ImageToI420"), "ConvertToI420 failed, format=%u", format);
    }
  }

  return i420_buffer;
}

std::shared_ptr<I420Buffer> I420Buffer::Create(int width, int height, int align) {
  if (width <= 0 || height <= 0)
    throw std::invalid_argument("I420Buffer: invalid size");
  if (align <= 0 || (align & (align - 1)))
    throw std::invalid_argument("I420Buffer: alignment must be power of two");

  const int stride_y = std::max(AlignUp(width, align), 16);
  const int stride_u = stride_y / 2;
  const int stride_v = stride_y / 2;

  if (size_t(stride_y) > SIZE_MAX / size_t(height))
    throw std::overflow_error("I420Buffer: size overflow (Y)");

  const int chroma_h = (height + 1) / 2;

  if (size_t(stride_u) > SIZE_MAX / size_t(chroma_h))
    throw std::overflow_error("I420Buffer: size overflow (U/V)");

  const size_t y_bytes = size_t(stride_y) * size_t(height);
  const size_t u_bytes = size_t(stride_u) * size_t(chroma_h);
  const size_t v_bytes = u_bytes;

  size_t total_raw = y_bytes;
  if (total_raw > SIZE_MAX - u_bytes)
    throw std::overflow_error("I420Buffer: size overflow (Y+U)");
  total_raw += u_bytes;
  if (total_raw > SIZE_MAX - v_bytes)
    throw std::overflow_error("I420Buffer: size overflow (Y+U+V)");
  total_raw += v_bytes;

  const size_t total = (total_raw + align - 1) / align * align;

  auto mem = std::unique_ptr<uint8_t, BoostAlignedFree>(
          static_cast<uint8_t *>(boost::alignment::aligned_alloc(align, total)), BoostAlignedFree{});
  if (!mem)
    throw std::bad_alloc();

  auto buf = std::shared_ptr<I420Buffer>(new I420Buffer(width, height, stride_y, stride_u, stride_v, align));
  buf->mem_ = std::move(mem);
  buf->y_ = buf->mem_.get();
  buf->u_ = buf->y_ + y_bytes;
  buf->v_ = buf->u_ + u_bytes;
  return buf;
}
