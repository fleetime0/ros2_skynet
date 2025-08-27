#ifndef RTC_PEER_H_
#define RTC_PEER_H_

#include <atomic>
#include <thread>

#include "common/h264_buffer.h"
#include "common/interface/subject.h"
#include "encoder/encoder.hpp"
#include "rtc/rtc.hpp"

struct PeerConfig : public rtc::Configuration {
  int timeout = 10;
  bool has_candidates_in_sdp = false;
};

class SignalingMessageObserver {
public:
  using OnLocalSdpFunc =
          std::function<void(const std::string &peer_id, const std::string &sdp, const std::string &type)>;
  using OnLocalIceFunc =
          std::function<void(const std::string &peer_id, const std::string &sdp_mid, const std::string &candidate)>;

  virtual void SetRemoteSdp(const std::string &sdp, const std::string &type) = 0;
  virtual void SetRemoteIce(const std::string &sdp_mid, const std::string &candidate) = 0;

  void OnLocalSdp(OnLocalSdpFunc func) { on_local_sdp_fn_ = std::move(func); }
  void OnLocalIce(OnLocalIceFunc func) { on_local_ice_fn_ = std::move(func); }

protected:
  OnLocalSdpFunc on_local_sdp_fn_ = nullptr;
  OnLocalIceFunc on_local_ice_fn_ = nullptr;
};

class RtcPeer : public SignalingMessageObserver {
public:
  static std::shared_ptr<RtcPeer> Create(std::shared_ptr<Encoder> encoder, PeerConfig config);

  RtcPeer(PeerConfig config);
  ~RtcPeer();
  void CreateOffer();
  void Terminate();

  bool isConnected() const;
  std::string id() const;

  void SetPeer(std::shared_ptr<rtc::PeerConnection> peer);
  std::shared_ptr<rtc::PeerConnection> GetPeer();
  void SetTrack(std::shared_ptr<rtc::Track> track);
  std::shared_ptr<rtc::Track> GetTrack();
  std::string RestartIce(std::string ice_ufrag, std::string ice_pwd);

  // SignalingMessageObserver implementation.
  void SetRemoteSdp(const std::string &sdp, const std::string &type) override;
  void SetRemoteIce(const std::string &sdp_mid, const std::string &candidate) override;

protected:
  void SubscribeEncoder(std::shared_ptr<Encoder> encoder);

  std::shared_ptr<Observable<std::shared_ptr<H264Buffer>>> encoder_observer_;

private:
  void OnSignalingStateChange(rtc::PeerConnection::SignalingState state);
  void OnIceGatheringChange(rtc::PeerConnection::GatheringState state);
  void OnConnectionChange(rtc::PeerConnection::State state);
  void OnIceCandidate(rtc::Candidate candidate);
  void OnLocalDescription(rtc::Description desc);

  void EmitLocalSdp(int delay_sec = 0);

  int timeout_;
  std::string id_;
  bool has_candidates_in_sdp_;
  std::atomic<bool> is_connected_;
  std::atomic<bool> is_complete_;
  std::thread peer_timeout_;
  std::thread sent_sdp_timeout_;

  std::string modified_sdp_;
  rtc::PeerConnection::SignalingState signaling_state_;
  std::unique_ptr<rtc::Description> modified_desc_;

  std::shared_ptr<rtc::Track> track_;
  std::shared_ptr<rtc::PeerConnection> peer_connection_;

  int64_t start_ts_;
  std::chrono::steady_clock::time_point wall_start_tp_;
};

#endif // RTC_PEER_H_
