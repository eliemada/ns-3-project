#include "http-cache-app.h"
#include "http-header.h"
#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/simulator.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE("HttpCacheApp");

TypeId HttpCacheApp::GetTypeId(){
  static TypeId tid = TypeId("ns3::HttpCacheApp")
    .SetParent<Application>()
    .AddConstructor<HttpCacheApp>();
  return tid;
}

HttpCacheApp::HttpCacheApp() = default;
void HttpCacheApp::SetListenPort(uint16_t p){ m_listenPort = p; }
void HttpCacheApp::SetOrigin(Address a, uint16_t p){ m_originAddr = a; m_originPort = p; }
void HttpCacheApp::SetTtl(Time t){ m_ttl = t; }
void HttpCacheApp::SetCapacity(uint32_t c){ m_capacity = c; }
void HttpCacheApp::SetCacheDelay(Time t){ m_cacheDelay = t; }
void HttpCacheApp::SetObjectSize(uint32_t size) {
  m_objectSize = size;
}

void HttpCacheApp::SetDynamicTtlEnabled(bool enabled) {
  m_dynamicTtlEnabled = enabled;
}

void HttpCacheApp::SetTtlWindow(Time window) {
  m_ttlWindow = window;
}

void HttpCacheApp::SetTtlThreshold(double threshold) {
  m_ttlThreshold = threshold;
}

void HttpCacheApp::SetTtlReduction(double reduction) {
  m_ttlReduction = reduction;
}

void HttpCacheApp::SetTtlEvalInterval(Time interval) {
  m_ttlEvalInterval = interval;
}

std::string HttpCacheApp::ExtractService(const std::string& resource) {
  // Parse "/service-X/seg-Y" -> "service-X"
  // Or "/service-X" -> "service-X"
  if (resource.empty()) return "";

  size_t start = (resource[0] == '/') ? 1 : 0;
  size_t end = resource.find('/', start);

  if (end == std::string::npos) {
    return resource.substr(start);
  }
  return resource.substr(start, end - start);
}

void HttpCacheApp::RecordRequest(const std::string& service) {
  if (!m_dynamicTtlEnabled || service.empty()) return;

  Time now = Simulator::Now();

  // Create new bucket if needed
  if (m_buckets.empty() || (now - m_buckets.back().startTime) >= m_bucketDuration) {
    TimeBucket bucket;
    bucket.startTime = now;
    m_buckets.push_back(bucket);
  }

  // Increment count for this service in current bucket
  m_buckets.back().serviceRequests[service]++;
}

Time HttpCacheApp::GetEffectiveTtl(const std::string& service) {
  if (!m_dynamicTtlEnabled) return m_ttl;

  if (m_penalizedServices.count(service) > 0) {
    return m_ttl * (1.0 - m_ttlReduction);
  }
  return m_ttl;
}

void HttpCacheApp::StartApplication(){
  m_clientSock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_clientSock->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_listenPort));
  m_clientSock->SetRecvCallback(MakeCallback(&HttpCacheApp::HandleClientRead, this));

  m_originSock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_originSock->Bind();
  m_originSock->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_originAddr), m_originPort));
  m_originSock->SetRecvCallback(MakeCallback(&HttpCacheApp::HandleOriginRead, this));
}
void HttpCacheApp::StopApplication(){ if (m_clientSock) m_clientSock->Close(); if (m_originSock) m_originSock->Close(); }

void HttpCacheApp::Touch(const std::string& key){
  auto it = m_map.find(key); if (it==m_map.end()) return; m_lru.erase(it->second.it); m_lru.push_front(key); it->second.it = m_lru.begin();
}
void HttpCacheApp::Insert(const std::string& key, const std::string& val){
  auto now = Simulator::Now();
  if (m_map.size() >= m_capacity){ // evict LRU
    std::string evictKey = m_lru.back(); m_lru.pop_back(); m_map.erase(evictKey);
  }
  m_lru.push_front(key);
  m_map[key] = Entry{val, now + m_ttl, m_lru.begin()};
}

void HttpCacheApp::HandleClientRead(Ptr<Socket> sock){
  Address from; Ptr<Packet> p;
  while ((p = sock->RecvFrom(from))){
    HttpHeader hdr; p->RemoveHeader(hdr);
    std::string key = hdr.GetResource();
    auto it = m_map.find(key);
    auto now = Simulator::Now();
    if (it != m_map.end() && it->second.expiry > now){
      NS_LOG_INFO("Cache HIT key=" << key);
      Touch(key);
      Simulator::Schedule(m_cacheDelay, &HttpCacheApp::ReplyToClient, this, hdr.GetRequestId(), key, true, from);
    } else {
      NS_LOG_INFO("Cache MISS key=" << key);
      // miss -> forward to origin. Use a unique forward id to avoid collisions
      // between clients that may reuse the same numeric request id.
      uint32_t origReqId = hdr.GetRequestId();
      uint32_t fid = m_nextForwardId++;
      m_forwarding[fid] = std::make_pair(origReqId, from);
      // replace header request id with forward id when sending to origin
      HttpHeader fhdr(fid, key);
      Ptr<Packet> fwd = Create<Packet>(m_objectSize);
      fwd->AddHeader(fhdr);
      m_originSock->Send(fwd);
    }
  }
}

void HttpCacheApp::HandleOriginRead(Ptr<Socket> sock){
  Address from; Ptr<Packet> p;
  while ((p = sock->RecvFrom(from))){
    HttpHeader hdr; p->RemoveHeader(hdr);
    std::string key = hdr.GetResource(); // origin echoes key
    // store and reply to waiting client. The origin returns the forward id as
    // the request id; look up the original client request id and address.
    Insert(key, "data");
    uint32_t fid = hdr.GetRequestId();
    auto itf = m_forwarding.find(fid);
    if (itf != m_forwarding.end()){
      uint32_t origReqId = itf->second.first;
      Address clientAddr = itf->second.second;
      ReplyToClient(origReqId, key, false, clientAddr);
      m_forwarding.erase(itf);
    }
  }
}

void HttpCacheApp::ReplyToClient(uint32_t reqId, const std::string& resource, bool hit, const Address& to){
  // Encode hit/miss by suffixing resource with 'H' or 'M'
  std::string res = resource + (hit?"H":"M");
  Ptr<Packet> resp = Create<Packet>(m_objectSize);
  HttpHeader hdr(reqId, res);
  resp->AddHeader(hdr);
  m_clientSock->SendTo(resp, 0, to);
}

} // namespace ns3
