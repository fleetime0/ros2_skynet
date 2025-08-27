#ifndef I420_BUFFER_H
#define I420_BUFFER_H

#include <memory>
#include <sensor_msgs/msg/image.hpp>

#include <boost/align/aligned_alloc.hpp>

class I420Buffer;
std::shared_ptr<I420Buffer> ImageToI420(const sensor_msgs::msg::Image &image_msg);

struct BoostAlignedFree {
  void operator()(uint8_t *p) const noexcept { boost::alignment::aligned_free(p); }
};

class I420Buffer {
public:
  static std::shared_ptr<I420Buffer> Create(int width, int height, int align);

  int width() const noexcept { return w_; }
  int height() const noexcept { return h_; }
  int StrideY() const noexcept { return sy_; }
  int StrideU() const noexcept { return su_; }
  int StrideV() const noexcept { return sv_; }

  const uint8_t *DataY() const noexcept { return y_; }
  const uint8_t *DataU() const noexcept { return u_; }
  const uint8_t *DataV() const noexcept { return v_; }

  uint8_t *MutableDataY() { return y_; }
  uint8_t *MutableDataU() { return u_; }
  uint8_t *MutableDataV() { return v_; }

  size_t ByteSize() const noexcept { return size_t(sy_) * h_ + size_t(su_) * (h_ / 2) + size_t(sv_) * (h_ / 2); }

  ~I420Buffer() = default;

  I420Buffer(const I420Buffer &) = delete;
  I420Buffer &operator=(const I420Buffer &) = delete;

private:
  I420Buffer(int w, int h, int sy, int su, int sv, int align) :
      w_(w), h_(h), sy_(sy), su_(su), sv_(sv), align_(align) {}

  static int AlignUp(int v, int a) { return (v + a - 1) / a * a; }

private:
  int w_{};
  int h_{};
  int sy_{}, su_{}, sv_{};
  int align_{};

  std::unique_ptr<uint8_t, BoostAlignedFree> mem_{nullptr, BoostAlignedFree{}};
  uint8_t *y_{nullptr};
  uint8_t *u_{nullptr};
  uint8_t *v_{nullptr};
};

#endif // I420_BUFFER_H
