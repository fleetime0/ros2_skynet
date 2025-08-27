#ifndef HTTP_SERVICE_H_
#define HTTP_SERVICE_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include "rtc/rtc_peer.h"
#include "stream_config.h"

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

struct IceCandidates {
  std::string ice_ufrag;
  std::string ice_pwd;
  std::vector<std::string> candidates;
};

namespace webrtc_whep {
class WebrtcWhep;
};

class HttpService : public std::enable_shared_from_this<HttpService> {
public:
  static std::shared_ptr<HttpService> Create(StreamConfig config, webrtc_whep::WebrtcWhep *webrtc_whep,
                                             boost::asio::io_context &ioc);

  HttpService(StreamConfig config, webrtc_whep::WebrtcWhep *webrtc_whep, boost::asio::io_context &ioc);
  ~HttpService();

  void Start();

  void Connect();
  void Disconnect();

  std::shared_ptr<RtcPeer> CreatePeer(PeerConfig config = PeerConfig{});

  std::shared_ptr<RtcPeer> GetPeer(const std::string &peer_id);

  void RemovePeerFromMap(const std::string &peer_id);

protected:
  webrtc_whep::WebrtcWhep *webrtc_whep_;

  void RefreshPeerMap();

private:
  std::atomic_bool cleaner_stop_;
  std::thread cleaner_;

  uint16_t port_;
  tcp::acceptor acceptor_;

  std::unordered_map<std::string, std::shared_ptr<RtcPeer>> peer_map_;

  void AcceptConnection();
};

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
  static std::shared_ptr<HttpSession> Create(tcp::socket socket, std::shared_ptr<HttpService> http_service);

  HttpSession(tcp::socket socket, std::shared_ptr<HttpService> http_service) :
      http_service_(http_service), stream_(std::move(socket)) {}
  ~HttpSession();

  void Start() { ReadRequest(); }

private:
  std::shared_ptr<HttpService> http_service_;

  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  http::request<http::string_body> req_;
  std::shared_ptr<http::response<http::string_body>> res_;
  std::string content_type_;

  void ReadRequest();
  void WriteResponse();
  void CloseConnection();

  void HandleRequest();
  void HandlePostRequest();
  void HandlePatchRequest();
  void HandleOptionsRequest();
  void HandleDeleteRequest();
  void ResponseUnprocessableEntity(const char *message);
  void ResponseMethodNotAllowed();
  void ResponsePreconditionFailed();
  void SetCommonHeader(std::shared_ptr<boost::beast::http::response<boost::beast::http::string_body>> req);
  std::vector<std::string> ParseRoutes(std::string target);
  IceCandidates ParseCandidates(const std::string &sdp);
};

#endif
