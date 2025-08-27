/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2022, Raspberry Pi Ltd
 *
 * libav_encoder.cpp - libav video encoder.
 */

#include <iostream>

#include "common/i420_buffer.h"
#include "encoder/libav_encoder.hpp"
#include "rclcpp/rclcpp.hpp"

namespace {

void encoderOptionsGeneral(StreamConfig config, AVCodecContext *codec) {
  codec->framerate = {config.fps * 1000, 1000};
  codec->profile = FF_PROFILE_UNKNOWN;

  std::string h264_profile = "constrained baseline";
  const AVCodecDescriptor *desc = avcodec_descriptor_get(codec->codec_id);
  for (const AVProfile *profile = desc->profiles; profile && profile->profile != FF_PROFILE_UNKNOWN; profile++) {
    if (!strncasecmp(h264_profile.c_str(), profile->name, h264_profile.size())) {
      codec->profile = profile->profile;
      break;
    }
  }
  if (codec->profile == FF_PROFILE_UNKNOWN)
    throw std::runtime_error("libav: no such profile " + h264_profile);

  codec->level = 31;
  codec->gop_size = config.fps;

  const int kbps = config.bitrate;
  const int64_t br = (int64_t) kbps * 1000;
  codec->bit_rate = br;
  codec->rc_max_rate = br;
  codec->rc_min_rate = br;
  codec->rc_buffer_size = br * 2;

  codec->max_b_frames = 0;
}

void encoderOptionsLibx264([[maybe_unused]] StreamConfig config, AVCodecContext *codec) {
  codec->me_range = 16;
  codec->me_cmp = 1; // No chroma ME
  codec->me_subpel_quality = 0;
  codec->thread_count = 0;

  codec->thread_type = FF_THREAD_SLICE;
  codec->slices = 4;
  codec->refs = 1;
  av_opt_set(codec->priv_data, "preset", "ultrafast", 0);
  av_opt_set(codec->priv_data, "tune", "zerolatency", 0);

  av_opt_set(codec->priv_data, "weightp", "none", 0);
  av_opt_set(codec->priv_data, "weightb", "0", 0);
  av_opt_set(codec->priv_data, "motion-est", "dia", 0);
  av_opt_set(codec->priv_data, "sc_threshold", "0", 0);
  av_opt_set(codec->priv_data, "rc-lookahead", "0", 0);
  av_opt_set(codec->priv_data, "mixed_ref", "0", 0);

  av_opt_set(codec->priv_data, "nal-hrd", "cbr", 0);
  av_opt_set(codec->priv_data, "annexb", "1", 0);
  av_opt_set(codec->priv_data, "repeat-headers", "1", 0);
}

} // namespace

std::shared_ptr<LibAvEncoder> LibAvEncoder::Create(StreamConfig config) {
  return std::make_shared<LibAvEncoder>(config);
}

LibAvEncoder::LibAvEncoder(StreamConfig config) : config_(config), video_start_ts_(0) {
  av_log_set_level(AV_LOG_INFO);

  initVideoCodec();

  pkt_[Video] = av_packet_alloc();
  RCLCPP_DEBUG(rclcpp::get_logger("LibAvEncoder"), "libav: codec init completed");
}

LibAvEncoder::~LibAvEncoder() {
  avcodec_free_context(&codec_ctx_[Video]);
  av_packet_free(&pkt_[Video]);
  RCLCPP_DEBUG(rclcpp::get_logger("LibAvEncoder"), "libav: codec closed");
}

void LibAvEncoder::EncodeBuffer(const sensor_msgs::msg::Image &image_msg) {
  // auto t_start = std::chrono::high_resolution_clock::now();
  AVFrame *frame = av_frame_alloc();
  if (!frame)
    throw std::runtime_error("libav: could not allocate AVFrame");

  auto ros_time_to_us = [](const builtin_interfaces::msg::Time &t) {
    return static_cast<int64_t>(t.sec) * 1000000LL + static_cast<int64_t>(t.nanosec) / 1000LL;
  };
  if (!video_start_ts_) {
    video_start_ts_ = ros_time_to_us(image_msg.header.stamp);

    RCLCPP_DEBUG(rclcpp::get_logger("LibAvEncoder"), "Video start timestamp : %" PRId64 "", video_start_ts_);
  }

  auto i420_buffer = ImageToI420(image_msg);

  frame->format = codec_ctx_[Video]->pix_fmt;
  frame->width = i420_buffer->width();
  frame->height = i420_buffer->height();

  frame->linesize[0] = i420_buffer->StrideY();
  frame->linesize[1] = i420_buffer->StrideU();
  frame->linesize[2] = i420_buffer->StrideV();

  const int64_t ts_us = ros_time_to_us(image_msg.header.stamp);
  frame->pts = ts_us - video_start_ts_;

  auto *holder = new std::shared_ptr<I420Buffer>(i420_buffer);

  frame->buf[0] = av_buffer_create(i420_buffer->MutableDataY(), i420_buffer->ByteSize(), &LibAvEncoder::releaseBuffer,
                                   holder, 0);
  av_image_fill_pointers(frame->data, AV_PIX_FMT_YUV420P, frame->height, frame->buf[0]->data, frame->linesize);
  av_frame_make_writable(frame);

  int ret = avcodec_send_frame(codec_ctx_[Video], frame);
  if (ret < 0)
    throw std::runtime_error("libav: error encoding frame: " + std::to_string(ret));

  encode(pkt_[Video], Video);

  av_frame_free(&frame);

  // auto t_end = std::chrono::high_resolution_clock::now();
  // double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

  // std::cout << "[media] EncodeBuffer耗时: " << elapsed_ms << " ms" << std::endl;
}

void LibAvEncoder::initVideoCodec() {
  const AVCodec *codec = avcodec_find_encoder_by_name("libx264");
  if (!codec)
    throw std::runtime_error("libav: cannot find video encoder libx264");

  codec_ctx_[Video] = avcodec_alloc_context3(codec);
  if (!codec_ctx_[Video])
    throw std::runtime_error("libav: Cannot allocate video context");

  codec_ctx_[Video]->width = config_.width;
  codec_ctx_[Video]->height = config_.height;
  // usec timebase
  codec_ctx_[Video]->time_base = {1, 1000 * 1000};
  codec_ctx_[Video]->sw_pix_fmt = AV_PIX_FMT_YUV420P;
  codec_ctx_[Video]->pix_fmt = AV_PIX_FMT_YUV420P;

  // Apply specific options.
  encoderOptionsLibx264(config_, codec_ctx_[Video]);

  // Apply general options.
  encoderOptionsGeneral(config_, codec_ctx_[Video]);

  int ret = avcodec_open2(codec_ctx_[Video], codec, nullptr);
  if (ret < 0)
    throw std::runtime_error("libav: unable to open video codec: " + std::to_string(ret));
}

void LibAvEncoder::encode(AVPacket *pkt, unsigned int stream_id) {
  int ret = 0;

  while (ret >= 0) {
    ret = avcodec_receive_packet(codec_ctx_[stream_id], pkt);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      av_packet_unref(pkt);
      break;
    } else if (ret < 0)
      throw std::runtime_error("libav: error receiving packet: " + std::to_string(ret));

    bool key = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
    auto frame_buffer = H264Buffer::Create(pkt->data, pkt->size, key, pkt->pts);
    NextFrameBuffer(frame_buffer);

    av_packet_unref(pkt);
  }
}

extern "C" void LibAvEncoder::releaseBuffer(void *opaque, uint8_t *) {
  auto *p = static_cast<std::shared_ptr<I420Buffer> *>(opaque);
  delete p;
}
