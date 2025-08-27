#include "common/h264_buffer.h"

std::shared_ptr<H264Buffer> H264Buffer::Create(uint8_t *data, size_t size, bool keyframe, int64_t timestamp) {
  return std::make_shared<H264Buffer>(data, size, keyframe, timestamp);
}

H264Buffer::H264Buffer(uint8_t *data, size_t size, bool keyframe, int64_t timestamp) :
    data_(data), size_(size), keyframe_(keyframe), timestamp_(timestamp) {}

const uint8_t *H264Buffer::data() const { return data_; }

size_t H264Buffer::size() const { return size_; }

bool H264Buffer::isKeyFrame() const { return keyframe_; }

int64_t H264Buffer::timestamp() const { return timestamp_; }
