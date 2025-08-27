#ifndef H264_BUFFER_H
#define H264_BUFFER_H

#include <functional>
#include <memory>

class H264Buffer {
public:
  using Deleter = std::function<void(uint8_t *)>;

  static std::shared_ptr<H264Buffer> Create(uint8_t *data, size_t size, bool keyframe, int64_t timestamp);

  H264Buffer(uint8_t *data, size_t size, bool keyframe, int64_t timestamp);

  ~H264Buffer() = default;

  const uint8_t *data() const;
  size_t size() const;
  bool isKeyFrame() const;
  int64_t timestamp() const;

private:
  uint8_t *data_;
  size_t size_;
  bool keyframe_;
  int64_t timestamp_;
};

#endif // H264_BUFFER_H
