#include "webrtc_whep.h"

#include "stream_config.h"

namespace webrtc_whep {

std::shared_ptr<RtcPeer> WebrtcWhep::CreatePeerConnection(PeerConfig peer_config) {
  std::string stun_server = "stun:stun.l.google.com:19302";
  peer_config.iceServers.emplace_back(stun_server);
  peer_config.disableAutoNegotiation = true;
  peer_config.timeout = config_.peer_timeout;
  auto peer = RtcPeer::Create(encoder_, peer_config);
  return peer;
}

WebrtcWhep::WebrtcWhep(const rclcpp::NodeOptions &options) : Node("webrtc_whep_node", options) {
  this->declare_parameter<std::string>("image_topic", "/image_raw");
  this->declare_parameter<std::string>("qos_reliability", "reliable");
  this->declare_parameter<int>("width", 640);
  this->declare_parameter<int>("height", 480);
  this->declare_parameter<int>("fps", 30);
  this->declare_parameter<int>("bitrate", 2000);
  this->declare_parameter<int>("http_port", 8000);
  this->declare_parameter<int>("peer_timeout", 10);

  image_topic_ = this->get_parameter("image_topic").as_string();
  qos_reliability_ = this->get_parameter("qos_reliability").as_string();

  config_.width = this->get_parameter("width").as_int();
  config_.height = this->get_parameter("height").as_int();
  config_.fps = this->get_parameter("fps").as_int();
  config_.bitrate = this->get_parameter("bitrate").as_int();

  config_.http_port = this->get_parameter("http_port").as_int();
  config_.peer_timeout = this->get_parameter("peer_timeout").as_int();

  encoder_ = LibAvEncoder::Create(config_);

  io_context_ = std::make_shared<boost::asio::io_context>();

  http_service_ = HttpService::Create(config_, this, *io_context_);

  rclcpp::QoS qos(10);
  if (qos_reliability_ == "reliable") {
    qos.reliable();
    RCLCPP_INFO(this->get_logger(), "Using RELIABLE QoS.");
  } else {
    qos.best_effort();
    RCLCPP_INFO(this->get_logger(), "Using BEST_EFFORT QoS.");
  }

  sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(
          image_topic_, qos, std::bind(&WebrtcWhep::image_callback, this, std::placeholders::_1));

  RCLCPP_INFO(this->get_logger(), "Subscribed to %s", image_topic_.c_str());

  io_thread_ = std::make_shared<std::thread>([this]() {
    http_service_->Start();
    io_context_->run();
  });
}

WebrtcWhep::~WebrtcWhep() {
  if (io_context_) {
    io_context_->stop();
  }
  if (io_thread_ && io_thread_->joinable()) {
    io_thread_->join();
  }
}

void WebrtcWhep::image_callback(const sensor_msgs::msg::Image::UniquePtr msg) { encoder_->EncodeBuffer(*msg); }

} // namespace webrtc_whep

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(webrtc_whep::WebrtcWhep)
