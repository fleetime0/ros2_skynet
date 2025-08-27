#include "signaling/http_service.h"

#include <iostream>
#include <regex>

#include "rclcpp/rclcpp.hpp"
#include "webrtc_whep.h"

std::shared_ptr<HttpService> HttpService::Create(StreamConfig config, webrtc_whep::WebrtcWhep *webrtc_whep,
                                                 boost::asio::io_context &ioc) {
  return std::make_shared<HttpService>(config, webrtc_whep, ioc);
}

HttpService::HttpService(StreamConfig config, webrtc_whep::WebrtcWhep *webrtc_whep, boost::asio::io_context &ioc) :
    webrtc_whep_(webrtc_whep), cleaner_stop_(false), port_(config.http_port),
    acceptor_({ioc, {boost::asio::ip::address_v6::any(), port_}}) {}

HttpService::~HttpService() {
  cleaner_stop_.store(true);
  if (cleaner_.joinable()) {
    cleaner_.join();
  }
}

void HttpService::Start() {
  cleaner_ = std::thread([&] {
    while (!cleaner_stop_.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(60));
      if (cleaner_stop_.load())
        break;
      RefreshPeerMap();
    }
  });

  Connect();
}

void HttpService::Connect() {
  RCLCPP_INFO(rclcpp::get_logger("HttpService"), "Http server is running on http://*:%d", port_);
  AcceptConnection();
}

void HttpService::Disconnect() {}

std::shared_ptr<RtcPeer> HttpService::CreatePeer(PeerConfig config) {
  if (!webrtc_whep_) {
    RCLCPP_ERROR(rclcpp::get_logger("HttpService"), "V4L2Webrtc is not initialized.");
    return nullptr;
  }

  auto peer = webrtc_whep_->CreatePeerConnection(config);
  peer_map_[peer->id()] = peer;
  return peer;
}

std::shared_ptr<RtcPeer> HttpService::GetPeer(const std::string &peer_id) {
  auto it = peer_map_.find(peer_id);
  if (it != peer_map_.end()) {
    return it->second;
  }
  return nullptr;
}

void HttpService::RemovePeerFromMap(const std::string &peer_id) { peer_map_.erase(peer_id); }

void HttpService::AcceptConnection() {
  acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
    if (!ec) {
      auto session = HttpSession::Create(std::move(socket), shared_from_this());
      session->Start();
    } else {
      std::cerr << "Accept error: " << ec.message() << "\n";
    }
    AcceptConnection();
  });
}

void HttpService::RefreshPeerMap() {
  auto pm_it = peer_map_.begin();
  while (pm_it != peer_map_.end()) {
    auto peer_id = pm_it->second->id();

    if (pm_it->second && !pm_it->second->isConnected()) {
      RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "peer_map (%s) was erased.", peer_id.c_str());
      pm_it = peer_map_.erase(pm_it);
      RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "peer_map (%s) was erased.", peer_id.c_str());
    } else {
      ++pm_it;
    }
  }
}

std::shared_ptr<HttpSession> HttpSession::Create(tcp::socket socket, std::shared_ptr<HttpService> http_service) {
  return std::make_shared<HttpSession>(std::move(socket), http_service);
}

HttpSession::~HttpSession() {}

void HttpSession::ReadRequest() {
  auto self = shared_from_this();
  http::async_read(stream_, buffer_, req_,
                   [self](beast::error_code ec, [[maybe_unused]] std::size_t bytes_transferred) {
                     if (!ec) {
                       self->HandleRequest();
                     } else {
                       std::cerr << "Read error: " << ec.message() << "\n";
                     }
                   });
}

void HttpSession::WriteResponse() {
  auto self = shared_from_this();
  http::async_write(stream_, *res_, [self](beast::error_code ec, [[maybe_unused]] std::size_t bytes_transferred) {
    if (!ec) {
      RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "Successfully response!");
      self->CloseConnection();
    } else {
      std::cerr << "Write error: " << ec.message() << "\n";
    }
  });
}

void HttpSession::CloseConnection() {
  beast::error_code ec;
  stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
  if (ec) {
    std::cerr << "Shutdown error: " << ec.message() << "\n";
  }
}

void HttpSession::HandleRequest() {
  RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "Receive http method: %d", static_cast<int>(req_.method()));
  if (req_.method() != http::verb::options && req_.find("Content-Type") == req_.end()) {
    ResponseUnprocessableEntity("Without content type.");
    return;
  } else {
    content_type_.assign(req_["Content-Type"].begin(), req_["Content-Type"].size());
  }

  switch (req_.method()) {
    case http::verb::post:
      HandlePostRequest();
      break;
    case http::verb::patch:
      HandlePatchRequest();
      break;
    case http::verb::options:
      HandleOptionsRequest();
      break;
    case http::verb::delete_:
      HandleDeleteRequest();
      break;
    default:
      ResponseMethodNotAllowed();
      break;
  }
}

void HttpSession::HandlePostRequest() {
  if (content_type_ == "application/sdp") {
    PeerConfig config;
    config.has_candidates_in_sdp = true;
    auto peer = http_service_->CreatePeer(config);
    peer->OnLocalSdp([self = shared_from_this()](const std::string &peer_id, const std::string &sdp,
                                                 [[maybe_unused]] const std::string &type) {
      std::string host(self->req_["Host"].begin(), self->req_["Host"].size());
      std::string location = "https://" + host + "/resource/" + peer_id;
      self->res_ = std::make_shared<http::response<http::string_body>>(http::status::created, self->req_.version());
      self->SetCommonHeader(self->res_);
      self->res_->set(http::field::content_type, "application/sdp");
      self->res_->set(http::field::location, location);
      self->res_->body() = sdp;
      self->res_->prepare_payload();
      self->WriteResponse();
    });

    auto sdp = std::string(req_.body());
    peer->SetRemoteSdp(sdp, "offer");
  } else {
    ResponseUnprocessableEntity("The Content-Type only allow `application/sdp`.");
  }
}

void HttpSession::HandlePatchRequest() {
  auto routes = ParseRoutes(std::string(req_.target().data(), req_.target().size()));

  if (content_type_ != "application/trickle-ice-sdpfrag" || (routes.size() < 2 && routes[0] != "resource")) {
    ResponseUnprocessableEntity("The Content-Type only allow `trickle-ice-sdpfrag`.");
    return;
  }

  if (req_.find("If-Match") == req_.end()) {
    ResponsePreconditionFailed();
    return;
  }
  auto if_match = std::string(req_["If-Match"].data(), req_["If-Match"].size());

  auto peer_id = routes[1];
  auto peer = http_service_->GetPeer(peer_id);
  if (!peer) {
    ResponseUnprocessableEntity("The peer does not exist.");
    return;
  }

  auto sdp = std::string(req_.body());
  auto ice_group = ParseCandidates(sdp);
  for (const auto &candidate: ice_group.candidates) {
    RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "  Set remote ice: %s", candidate.c_str());
    peer->SetRemoteIce("0", candidate);
  }

  RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "Set received candidates into peer (%s)!", peer_id.c_str());

  if (if_match == "*") {
    RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "peer (%s) ice restart!", peer_id.c_str());
    auto local_sdp = peer->RestartIce(ice_group.ice_ufrag, ice_group.ice_pwd);
    res_ = std::make_shared<http::response<http::string_body>>(http::status::ok, req_.version());

    res_->set(http::field::content_type, "application/trickle-ice-sdpfrag");
    res_->body() = local_sdp;

  } else {
    res_ = std::make_shared<http::response<http::string_body>>(http::status::no_content, req_.version());
  }

  SetCommonHeader(res_);
  res_->prepare_payload();
  WriteResponse();
}

void HttpSession::HandleOptionsRequest() {
  res_ = std::make_shared<http::response<http::string_body>>(http::status::no_content, req_.version());
  SetCommonHeader(res_);
  res_->set(http::field::access_control_allow_headers, "Origin, X-Requested-With, Content-Type, Accept, Authorization");
  res_->set(http::field::access_control_allow_methods, "DELETE, OPTIONS, PATCH, POST");
  res_->set(http::field::access_control_allow_origin, "*");
  res_->prepare_payload();
  WriteResponse();
}

void HttpSession::HandleDeleteRequest() {
  auto routes = ParseRoutes(std::string(req_.target().data(), req_.target().size()));

  if (routes.size() < 2 && routes[0] != "resource") {
    ResponseUnprocessableEntity("The resource is not appplicable.");
    return;
  }

  auto peer_id = routes[1];
  auto peer = http_service_->GetPeer(peer_id);
  if (!peer) {
    ResponseUnprocessableEntity("The peer does not exist.");
    return;
  }

  peer->Terminate();
  http_service_->RemovePeerFromMap(peer_id);
  RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "Close peer (%s)!", peer_id.c_str());


  res_ = std::make_shared<http::response<http::string_body>>(http::status::accepted, req_.version());
  SetCommonHeader(res_);
  res_->prepare_payload();
  WriteResponse();
}

void HttpSession::ResponseUnprocessableEntity(const char *message) {
  res_ = std::make_shared<http::response<http::string_body>>(http::status::unprocessable_entity, req_.version());
  SetCommonHeader(res_);
  res_->set(http::field::content_type, "text/plain");
  res_->body() = message;
  res_->prepare_payload();
  WriteResponse();
}

void HttpSession::ResponseMethodNotAllowed() {
  res_ = std::make_shared<http::response<http::string_body>>(http::status::method_not_allowed, req_.version());
  SetCommonHeader(res_);
  res_->set(http::field::content_type, "text/plain");
  res_->body() = "Only POST, DELETE, OPTIONS and PATCH method are allowed.";
  res_->prepare_payload();
  WriteResponse();
}

void HttpSession::ResponsePreconditionFailed() {
  res_ = std::make_shared<http::response<http::string_body>>(http::status::precondition_failed, req_.version());
  SetCommonHeader(res_);
  res_->prepare_payload();
  WriteResponse();
}

void HttpSession::SetCommonHeader(std::shared_ptr<boost::beast::http::response<boost::beast::http::string_body>> res) {
  res->set(http::field::server, "piwebrtc.whep");
  res->set(http::field::access_control_allow_origin, "*");
}

std::vector<std::string> HttpSession::ParseRoutes(std::string target) {
  std::string tmp;
  std::vector<std::string> routes;
  std::stringstream ss(target);
  while (std::getline(ss, tmp, '/')) {
    if (!tmp.empty()) {
      routes.push_back(tmp);
    }
  }
  return routes;
}

IceCandidates HttpSession::ParseCandidates(const std::string &sdp) {
  std::regex midRegex(R"(a=mid:(\d+))");
  std::regex iceUfragRegex(R"(a=ice-ufrag:([^\s]+))");
  std::regex icePwdRegex(R"(a=ice-pwd:([^\s]+))");
  std::regex candidateRegex(R"(a=candidate:(.*))");

  std::smatch match;
  auto sdpBegin = sdp.begin();
  auto sdpEnd = sdp.end();

  IceCandidates result;

  while (std::regex_search(sdpBegin, sdpEnd, match, candidateRegex)) {
    std::string candidate = match[1].str();
    result.candidates.push_back("candidate:" + candidate);
    sdpBegin = match.suffix().first;
  }

  if (std::regex_search(sdp, match, iceUfragRegex)) {
    result.ice_ufrag = match[1].str();
    RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "ice-ufrag: %s", result.ice_ufrag.c_str());
  }

  if (std::regex_search(sdp, match, icePwdRegex)) {
    result.ice_pwd = match[1].str();
    RCLCPP_DEBUG(rclcpp::get_logger("HttpService"), "ice-pwd: %s", result.ice_pwd.c_str());
  }

  return result;
}
