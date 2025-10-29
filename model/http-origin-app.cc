#include "http-origin-app.h"
#include "http-header.h"
#include "ns3/inet-socket-address.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4-address.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE("HttpOriginApp");

TypeId HttpOriginApp::GetTypeId(){
  static TypeId tid = TypeId("ns3::HttpOriginApp").SetParent<Application>().AddConstructor<HttpOriginApp>();
  return tid;
}
HttpOriginApp::HttpOriginApp() = default;
void HttpOriginApp::SetListenPort(uint16_t p){ m_port = p; }
void HttpOriginApp::SetServiceDelay(Time t){ m_delay = t; }

void HttpOriginApp::StartApplication(){
  m_sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_sock->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port));
  m_sock->SetRecvCallback(MakeCallback(&HttpOriginApp::HandleRead, this));
}
void HttpOriginApp::StopApplication(){ if (m_sock) m_sock->Close(); }

void HttpOriginApp::HandleRead(Ptr<Socket> sock){
  Address from; Ptr<Packet> p;
  while ((p = sock->RecvFrom(from))){
    HttpHeader hdr; p->RemoveHeader(hdr);
    Simulator::Schedule(m_delay, &HttpOriginApp::Respond, this, hdr.GetRequestId(), from, hdr.GetResource());
  }
}

void HttpOriginApp::Respond(uint32_t reqId, const Address& to, const std::string& resource){
  Ptr<Packet> resp = Create<Packet>(0);
  HttpHeader hdr(reqId, resource);
  resp->AddHeader(hdr);
  m_sock->SendTo(resp, 0, to);
}

} // namespace ns3
