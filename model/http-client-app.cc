#include <cmath>
#include "http-client-app.h"
#include "http-header.h"
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/uinteger.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/simulator.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE("HttpClientApp");

TypeId HttpClientApp::GetTypeId() {
  static TypeId tid = TypeId("ns3::HttpClientApp")
    .SetParent<Application>()
    .AddConstructor<HttpClientApp>();
  return tid;
}

HttpClientApp::HttpClientApp() = default;
HttpClientApp::~HttpClientApp() = default;

void HttpClientApp::SetRemote(Address address, uint16_t port){ m_peer = address; m_port = port; }
void HttpClientApp::SetInterval(Time t){ m_interval = t; }
void HttpClientApp::SetResource(const std::string& r){ m_resource = r; }
void HttpClientApp::SetCsvPath(const std::string& p){ m_csvPath = p; }
void HttpClientApp::SetTotalRequests(uint32_t n){ m_total = n; }
void HttpClientApp::SetNumContent(uint32_t n){ m_numContent = std::max(1u, n); }
void HttpClientApp::SetZipf(bool z){ m_zipf = z; }
void HttpClientApp::SetZipfS(double s){ m_zipfS = s > 0 ? s : 1.0; }

void HttpClientApp::StartApplication(){
  if (!m_socket){
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind();
    m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peer), m_port));
    m_socket->SetRecvCallback(MakeCallback(&HttpClientApp::HandleRead, this));
  }
  m_csv.open(m_csvPath, std::ios::out);
  m_csv << "request_id,send_s,recv_s,latency_ms,cache_hit\n";

  m_uni = CreateObject<UniformRandomVariable>();
  if (m_numContent > 1 && m_zipf){
    m_zipfCum.resize(m_numContent);
    double sum = 0.0;
    for (uint32_t k=1; k<=m_numContent; ++k) sum += 1.0 / std::pow((double)k, m_zipfS);
    double run = 0.0;
    for (uint32_t k=1; k<=m_numContent; ++k){
      run += (1.0 / std::pow((double)k, m_zipfS)) / sum;
      m_zipfCum[k-1] = run;
    }
  }
  ScheduleNext();
}
void HttpClientApp::StopApplication(){ if (m_socket) m_socket->Close(); if (m_csv.is_open()) m_csv.close(); }

void HttpClientApp::ScheduleNext(){
  if (m_sent >= m_total) return;
  m_event = Simulator::Schedule(m_interval, &HttpClientApp::SendOne, this);
}

std::string HttpClientApp::PickResource(){
  if (m_numContent <= 1) return m_resource;
  uint32_t idx = 0;
  if (m_zipf && !m_zipfCum.empty()){
    double r = m_uni->GetValue(0.0, 1.0);
    for (uint32_t i=0;i<m_zipfCum.size();++i){ if (m_zipfCum[i] >= r){ idx = i; break; } }
  } else {
    idx = (uint32_t) m_uni->GetInteger(0, (int64_t)m_numContent-1);
  }
  return std::string("/file-") + std::to_string(idx+1);
}

void HttpClientApp::SendOne(){
  uint32_t id = m_nextId++;
  Ptr<Packet> p = Create<Packet>(0);
  std::string res = PickResource();
  HttpHeader hdr(id, res);
  p->AddHeader(hdr);
  m_sendTimes[id] = Simulator::Now();
  NS_LOG_INFO("Client sending id=" << id << " res=" << res);
  m_socket->Send(p);
  m_sent++;
  ScheduleNext();
}

void HttpClientApp::HandleRead(Ptr<Socket> socket){
  Address from; Ptr<Packet> p;
  while ((p = socket->RecvFrom(from))){
    HttpHeader hdr; p->RemoveHeader(hdr);
    auto it = m_sendTimes.find(hdr.GetRequestId());
    if (it != m_sendTimes.end()){
      Time s = it->second; Time r = Simulator::Now();
      double lat_ms = (r - s).GetMilliSeconds();
      bool hit = (!hdr.GetResource().empty() && hdr.GetResource().back()=='H');
      NS_LOG_INFO("Client recv id=" << hdr.GetRequestId() << " hit=" << (hit?1:0));
      m_csv << hdr.GetRequestId() << "," << s.GetSeconds() << "," << r.GetSeconds()
            << "," << lat_ms << "," << (hit?1:0) << "\n";
      m_sendTimes.erase(it);
    }
  }
}

} // namespace ns3
