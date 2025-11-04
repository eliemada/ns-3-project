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
      // miss -> forward to origin
      m_waiting[hdr.GetRequestId()] = from;
      Ptr<Packet> fwd = Create<Packet>(m_objectSize);
      fwd->AddHeader(hdr);
      m_originSock->Send(fwd);
    }
  }
}

void HttpCacheApp::HandleOriginRead(Ptr<Socket> sock){
  Address from; Ptr<Packet> p;
  while ((p = sock->RecvFrom(from))){
    HttpHeader hdr; p->RemoveHeader(hdr);
    std::string key = hdr.GetResource(); // origin echoes key
    // store and reply to waiting client
    Insert(key, "data");
    auto it = m_waiting.find(hdr.GetRequestId());
    if (it != m_waiting.end()){
      ReplyToClient(hdr.GetRequestId(), key, false, it->second);
      m_waiting.erase(it);
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
