/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2022, Raspberry Pi Ltd
 *
 * libav_encoder.hpp - libav video encoder.
 */

#pragma once

#include <condition_variable>
#include <memory>
#include <sensor_msgs/msg/image.hpp>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
}

#include "encoder/encoder.hpp"
#include "stream_config.h"

class LibAvEncoder : public Encoder {
public:
  static std::shared_ptr<LibAvEncoder> Create(StreamConfig config);

  LibAvEncoder(StreamConfig config);
  ~LibAvEncoder();

  void EncodeBuffer(const sensor_msgs::msg::Image &image_msg) override;

private:
  void initVideoCodec();
  void encode(AVPacket *pkt, unsigned int stream_id);

  static void releaseBuffer(void *opaque, uint8_t *data);

  StreamConfig config_;

  int64_t video_start_ts_;

  enum Context { Video = 0, Audio = 1 };
  AVCodecContext *codec_ctx_[2];

  AVPacket *pkt_[2];
};
