/**
 * @file http-origin-app.cc
 * @brief Implementation of the HTTP origin server application for ns-3.
 *
 * This file implements the HttpOriginApp class which simulates an HTTP origin
 * server that responds to cache miss requests with configurable processing delay.
 *
 * @author Elie Bruno
 * @date 2025
 */

#include "http-origin-app.h"
#include "http-header.h"
#include "ns3/inet-socket-address.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4-address.h"

namespace ns3 {

// Define logging component for this class
NS_LOG_COMPONENT_DEFINE("HttpOriginApp");

// ============================================================================
// TypeId Registration
// ============================================================================

TypeId HttpOriginApp::GetTypeId() {
  static TypeId tid = TypeId("ns3::HttpOriginApp")
    .SetParent<Application>()
    .AddConstructor<HttpOriginApp>();
  return tid;
}

// ============================================================================
// Constructor
// ============================================================================

HttpOriginApp::HttpOriginApp() = default;

// ============================================================================
// Configuration Setters
// ============================================================================

void HttpOriginApp::SetListenPort(uint16_t p) {
  m_port = p;
}

void HttpOriginApp::SetServiceDelay(Time t) {
  m_delay = t;
}

void HttpOriginApp::SetObjectSize(uint32_t size) {
  m_objectSize = size;
}

// ============================================================================
// Application Lifecycle
// ============================================================================

/**
 * @brief Initialize the origin server.
 *
 * Creates a UDP socket bound to the configured port and sets up
 * the receive callback for handling incoming requests.
 */
void HttpOriginApp::StartApplication() {
  // Create and configure UDP socket
  m_sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_sock->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port));
  m_sock->SetRecvCallback(MakeCallback(&HttpOriginApp::HandleRead, this));
}

/**
 * @brief Clean up when application stops.
 */
void HttpOriginApp::StopApplication() {
  if (m_sock) {
    m_sock->Close();
  }
}

// ============================================================================
// Request Handling
// ============================================================================

/**
 * @brief Handle incoming requests from the cache server.
 *
 * For each request:
 * 1. Extracts the HttpHeader to get request ID and resource
 * 2. Schedules a delayed response using the configured service delay
 *
 * The delay simulates the time required to fetch/generate content at
 * the origin server, which is typically much higher than cache hit latency.
 *
 * @param sock The socket that received data.
 */
void HttpOriginApp::HandleRead(Ptr<Socket> sock) {
  Address from;
  Ptr<Packet> p;

  while ((p = sock->RecvFrom(from))) {
    // Extract request information from header
    HttpHeader hdr;
    p->RemoveHeader(hdr);

    // Schedule delayed response to simulate processing time
    Simulator::Schedule(m_delay, &HttpOriginApp::Respond, this,
                        hdr.GetRequestId(), from, hdr.GetResource());
  }
}

/**
 * @brief Send a response to the cache server.
 *
 * Creates a response packet with the configured object size and
 * an HttpHeader echoing the request ID and resource path.
 *
 * The cache server uses the echoed resource path to identify which
 * content to cache, and the request ID to route the response back
 * to the correct waiting client.
 *
 * @param reqId The request ID to echo back.
 * @param to The address to send the response to.
 * @param resource The resource path to echo back.
 */
void HttpOriginApp::Respond(uint32_t reqId, const Address& to, const std::string& resource) {
  // Create response packet with configured payload size
  Ptr<Packet> resp = Create<Packet>(m_objectSize);

  // Add header echoing request ID and resource
  HttpHeader hdr(reqId, resource);
  resp->AddHeader(hdr);

  // Send response to cache server
  m_sock->SendTo(resp, 0, to);
}

} // namespace ns3
