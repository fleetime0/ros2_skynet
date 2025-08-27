#ifndef STREAM_CONFIG_H
#define STREAM_CONFIG_H

#include <cstdint>
#include <string>

struct StreamConfig {
  int width = 640;
  int height = 480;
  int fps = 30;
  int bitrate = 1000;

  int peer_timeout = 10;
  uint16_t http_port = 8000;
};

#endif // STREAM_CONFIG_H
