#ifndef WEBRTC_WHEP_H
#define WEBRTC_WHEP_H

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>

#include "encoder/libav_encoder.hpp"
#include "signaling/http_service.h"

namespace webrtc_whep {

class WebrtcWhep : public rclcpp::Node {
public:
  explicit WebrtcWhep(const rclcpp::NodeOptions &options);
  ~WebrtcWhep();

  std::shared_ptr<RtcPeer> CreatePeerConnection(PeerConfig peer_config);


private:
  void image_callback(const sensor_msgs::msg::Image::UniquePtr msg);

private:
  std::string image_topic_;
  std::string qos_reliability_;

  StreamConfig config_;

  std::shared_ptr<Encoder> encoder_;
  std::shared_ptr<HttpService> http_service_;
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<std::thread> io_thread_;

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_image_;
};

} // namespace webrtc_whep

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(webrtc_whep::WebrtcWhep)

#endif // WEBRTC_WHEP_H
