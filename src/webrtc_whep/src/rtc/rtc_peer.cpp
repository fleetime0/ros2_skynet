#include "rtc/rtc_peer.h"

#include <regex>

#include <boost/beast/core/detail/base64.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "rclcpp/rclcpp.hpp"

namespace utils {
std::string GenerateUuid() {
  static boost::uuids::random_generator generator;
  boost::uuids::uuid id = generator();
  return boost::uuids::to_string(id);
}
} // namespace utils

void RtcPeer::SubscribeEncoder(std::shared_ptr<Encoder> encoder) {
  encoder_observer_ = encoder->AsBufferObservable();
  encoder_observer_->Subscribe([this](std::shared_ptr<H264Buffer> buffer) {
    if (!track_ || !track_->isOpen())
      return;

    const int64_t ts_us = buffer->timestamp();
    if (!start_ts_) {
      start_ts_ = ts_us;
      wall_start_tp_ = std::chrono::steady_clock::now();
    }

    int64_t rel_us = ts_us - start_ts_;
    if (rel_us < 0)
      rel_us = 0;

    const auto target_tp = wall_start_tp_ + std::chrono::microseconds(rel_us);
    const auto now = std::chrono::steady_clock::now();
    if (target_tp > now) {
      // std::cout << "-------------- id: " << id_
      //           << " sleep: " << std::chrono::duration_cast<std::chrono::microseconds>(target_tp - now).count()
      //           << " us -----------" << std::endl;
      std::this_thread::sleep_until(target_tp);
    }

    const auto rel_dur = std::chrono::duration<double, std::micro>(rel_us);
    try {
      track_->sendFrame(reinterpret_cast<const rtc::byte *>(buffer->data()), buffer->size(), rel_dur);
    } catch (const std::exception &e) {
      RCLCPP_ERROR(rclcpp::get_logger("RtcPeer"), "Failed to send video frame: %s", e.what());
    }
  });
}

std::shared_ptr<RtcPeer> RtcPeer::Create(std::shared_ptr<Encoder> encoder, PeerConfig config) {
  auto ptr = std::make_shared<RtcPeer>(config);
  auto pc = std::make_shared<rtc::PeerConnection>(config);
  ptr->SetPeer(pc);

  ptr->SubscribeEncoder(encoder);

  return ptr;
}

RtcPeer::RtcPeer(PeerConfig config) :
    timeout_(config.timeout), id_(utils::GenerateUuid()), has_candidates_in_sdp_(config.has_candidates_in_sdp),
    is_connected_(false), is_complete_(false), start_ts_(0) {}

RtcPeer::~RtcPeer() {
  Terminate();
  encoder_observer_.reset();
  RCLCPP_DEBUG(rclcpp::get_logger("RtcPeer"), "peer connection (%s) was destroyed!", id_.c_str());
}

void RtcPeer::CreateOffer() {
  if (signaling_state_ == rtc::PeerConnection::SignalingState::HaveLocalOffer) {
    return;
  }

  peer_connection_->createOffer();
}

void RtcPeer::Terminate() {
  is_connected_.store(false);
  is_complete_.store(true);

  if (peer_timeout_.joinable()) {
    peer_timeout_.join();
  }
  if (sent_sdp_timeout_.joinable()) {
    sent_sdp_timeout_.join();
  }

  on_local_sdp_fn_ = nullptr;
  on_local_ice_fn_ = nullptr;
  if (peer_connection_) {
    peer_connection_->close();
    peer_connection_ = nullptr;
  }
  modified_desc_.release();
}

std::string RtcPeer::id() const { return id_; }

bool RtcPeer::isConnected() const { return is_connected_.load(); }

void RtcPeer::SetPeer(std::shared_ptr<rtc::PeerConnection> peer) {
  peer_connection_ = std::move(peer);
  peer_connection_->onSignalingStateChange(std::bind(&RtcPeer::OnSignalingStateChange, this, std::placeholders::_1));
  peer_connection_->onGatheringStateChange(std::bind(&RtcPeer::OnIceGatheringChange, this, std::placeholders::_1));
  peer_connection_->onStateChange(std::bind(&RtcPeer::OnConnectionChange, this, std::placeholders::_1));
  peer_connection_->onLocalCandidate(std::bind(&RtcPeer::OnIceCandidate, this, std::placeholders::_1));
  peer_connection_->onLocalDescription(std::bind(&RtcPeer::OnLocalDescription, this, std::placeholders::_1));
}

std::shared_ptr<rtc::PeerConnection> RtcPeer::GetPeer() { return peer_connection_; }

void RtcPeer::SetTrack(std::shared_ptr<rtc::Track> track) { track_ = std::move(track); }
std::shared_ptr<rtc::Track> RtcPeer::GetTrack() { return track_; }

std::string RtcPeer::RestartIce(std::string ice_ufrag, std::string ice_pwd) {
  rtc::Description remote_desc = peer_connection_->remoteDescription().value();
  std::string remote_sdp = std::string(remote_desc);

  // replace all ice_ufrag and ice_pwd in sdp.
  std::regex ufrag_regex(R"(a=ice-ufrag:([^\r\n]+))");
  std::regex pwd_regex(R"(a=ice-pwd:([^\r\n]+))");
  remote_sdp = std::regex_replace(remote_sdp, ufrag_regex, "a=ice-ufrag:" + ice_ufrag);
  remote_sdp = std::regex_replace(remote_sdp, pwd_regex, "a=ice-pwd:" + ice_pwd);
  SetRemoteSdp(remote_sdp, "offer");

  rtc::Description local_desc = peer_connection_->localDescription().value();
  std::string local_sdp = std::string(local_desc);

  return local_sdp;
}

void RtcPeer::OnSignalingStateChange(rtc::PeerConnection::SignalingState state) {
  signaling_state_ = state;
  RCLCPP_DEBUG(rclcpp::get_logger("RtcPeer"), "OnSignalingChange => %d", static_cast<int>(state));
  if (state == rtc::PeerConnection::SignalingState::HaveRemoteOffer) {
    peer_timeout_ = std::thread([this]() {
      std::this_thread::sleep_for(std::chrono::seconds(timeout_));
      if (peer_connection_ && !is_complete_.load() && !is_connected_.load()) {
        RCLCPP_DEBUG(rclcpp::get_logger("RtcPeer"), "Connection timeout after kConnecting. Closing connection.");
        peer_connection_->close();
      }
    });
  }
}

void RtcPeer::OnIceGatheringChange(rtc::PeerConnection::GatheringState state) {
  RCLCPP_DEBUG(rclcpp::get_logger("RtcPeer"), "OnIceGatheringChange => %d", static_cast<int>(state));
}

void RtcPeer::OnConnectionChange(rtc::PeerConnection::State state) {
  RCLCPP_DEBUG(rclcpp::get_logger("RtcPeer"), "OnConnectionChange => %d", static_cast<int>(state));
  if (state == rtc::PeerConnection::State::Connected) {
    is_connected_.store(true);
    on_local_ice_fn_ = nullptr;
    on_local_sdp_fn_ = nullptr;
  } else if (state == rtc::PeerConnection::State::Failed) {
    is_connected_.store(false);
    peer_connection_->close();
  } else if (state == rtc::PeerConnection::State::Closed) {
    is_connected_.store(false);
    is_complete_.store(true);
  }
}

void RtcPeer::OnIceCandidate(rtc::Candidate candidate) {
  if (has_candidates_in_sdp_ && modified_desc_) {
    modified_desc_->addCandidate(candidate);
  }

  if (on_local_ice_fn_) {
    std::string candidate_str = candidate.candidate();
    on_local_ice_fn_(id_, candidate.mid(), candidate_str);
  }
}

void RtcPeer::OnLocalDescription(rtc::Description desc) {
  modified_sdp_ = std::string(desc);

  modified_desc_ = std::make_unique<rtc::Description>(desc);

  if (has_candidates_in_sdp_) {
    EmitLocalSdp(1);
  } else {
    EmitLocalSdp();
  }
}

void RtcPeer::EmitLocalSdp(int delay_sec) {
  if (!on_local_sdp_fn_) {
    return;
  }

  if (sent_sdp_timeout_.joinable()) {
    sent_sdp_timeout_.join();
  }

  auto send_sdp = [this]() {
    std::string type = modified_desc_->typeString();
    modified_sdp_ = std::string(*modified_desc_);
    on_local_sdp_fn_(id_, modified_sdp_, type);
    on_local_sdp_fn_ = nullptr;
  };

  if (delay_sec > 0) {
    sent_sdp_timeout_ = std::thread([this, send_sdp, delay_sec]() {
      std::this_thread::sleep_for(std::chrono::seconds(delay_sec));
      send_sdp();
    });
  } else {
    send_sdp();
  }
}

static std::optional<int> FindH264PT(const std::string &sdp) {
  std::regex re(R"(a=rtpmap:(\d+)\s+H264/90000)", std::regex::icase);
  std::smatch m;
  if (std::regex_search(sdp, m, re))
    return std::stoi(m[1]);
  return std::nullopt;
}

void RtcPeer::SetRemoteSdp(const std::string &sdp, const std::string &sdp_type) {
  if (is_connected_.load()) {
    return;
  }

  rtc::Description remote_desc(sdp, sdp_type);
  rtc::Description::Type type = remote_desc.type();
  if (type == rtc::Description::Type::Unspec) {
    RCLCPP_ERROR(rclcpp::get_logger("RtcPeer"), "Unknown SDP type: %s", sdp_type.c_str());
    return;
  }

  try {
    peer_connection_->setRemoteDescription(remote_desc);
  } catch (const std::exception &e) {
    RCLCPP_ERROR(rclcpp::get_logger("RtcPeer"), "Failed to set remote SDP: %s", e.what());
  }

  if (type == rtc::Description::Type::Offer) {
    auto pt = FindH264PT(sdp);
    if (!pt) {
      RCLCPP_ERROR(rclcpp::get_logger("RtcPeer"), "Offer SDP missing H.264. Rejecting: remote must offer H.264.");
      return;
    }

    rtc::Description::Video video("0", rtc::Description::Direction::SendOnly);
    video.addH264Codec(*pt);
    video.addSSRC(42, "video-send");
    auto track = peer_connection_->addTrack(video);
    // create RTP configuration
    auto rtpConfig =
            std::make_shared<rtc::RtpPacketizationConfig>(42, "video-send", *pt, rtc::H264RtpPacketizer::ClockRate);
    // create packetizer
    auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::ShortStartSequence, rtpConfig);
    // add RTCP SR handler
    auto srReporter = std::make_shared<rtc::RtcpSrReporter>(rtpConfig);
    packetizer->addToChain(srReporter);
    // add RTCP NACK handler
    auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
    packetizer->addToChain(nackResponder);
    // set handler
    track->setMediaHandler(packetizer);
    SetTrack(track);

    peer_connection_->createAnswer();
    peer_connection_->setLocalDescription();
  }
}

void RtcPeer::SetRemoteIce(const std::string &sdp_mid, const std::string &candidate) {
  if (is_connected_.load()) {
    return;
  }

  try {
    rtc::Candidate ice(sdp_mid, candidate);
    peer_connection_->addRemoteCandidate(ice);
  } catch (const std::exception &e) {
    RCLCPP_ERROR(rclcpp::get_logger("RtcPeer"), "Failed to apply remote ICE candidate: %s", e.what());
  }
}
