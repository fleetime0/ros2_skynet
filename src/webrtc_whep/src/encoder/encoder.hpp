/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * encoder.hpp - Video encoder class.
 */

#pragma once

#include <sensor_msgs/msg/image.hpp>
#include <vector>
#include "common/h264_buffer.h"
#include "common/interface/subject.h"


class Encoder {
public:
  Encoder() = default;
  virtual ~Encoder() { h264_buffer_subject_.UnSubscribe(); }

  std::shared_ptr<Observable<std::shared_ptr<H264Buffer>>> AsBufferObservable() {
    return h264_buffer_subject_.AsObservable();
  }

  virtual void EncodeBuffer(const sensor_msgs::msg::Image &image_msg) = 0;

  void NextFrameBuffer(std::shared_ptr<H264Buffer> h264_buffer) { h264_buffer_subject_.Next(h264_buffer); }

private:
  Subject<std::shared_ptr<H264Buffer>> h264_buffer_subject_;
};
