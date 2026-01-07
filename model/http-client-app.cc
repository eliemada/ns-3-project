/**
 * @file http-client-app.cc
 * @brief Implementation of the HTTP client application for ns-3.
 *
 * This file implements the HttpClientApp class which simulates HTTP client
 * behavior including request generation with Zipf distribution, latency
 * measurement, and comprehensive metrics collection.
 *
 * @author Elie Bruno
 * @date 2025
 */

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

// Define logging component for this class
NS_LOG_COMPONENT_DEFINE("HttpClientApp");

// ============================================================================
// TypeId Registration
// ============================================================================

TypeId HttpClientApp::GetTypeId() {
  static TypeId tid = TypeId("ns3::HttpClientApp")
    .SetParent<Application>()
    .AddConstructor<HttpClientApp>();
  return tid;
}

// ============================================================================
// Constructor and Destructor
// ============================================================================

HttpClientApp::HttpClientApp() = default;
HttpClientApp::~HttpClientApp() = default;

// ============================================================================
// Configuration Setters
// ============================================================================

void HttpClientApp::SetRemote(Address address, uint16_t port) {
  m_peer = address;
  m_port = port;
}

void HttpClientApp::SetInterval(Time t) {
  m_interval = t;
}

void HttpClientApp::SetResource(const std::string& r) {
  m_resource = r;
}

void HttpClientApp::SetCsvPath(const std::string& p) {
  m_csvPath = p;
}

void HttpClientApp::SetSummaryCsvPath(const std::string& p) {
  m_summaryCsvPath = p;
}

void HttpClientApp::SetTotalRequests(uint32_t n) {
  m_total = n;
}

void HttpClientApp::SetNumContent(uint32_t n) {
  // Ensure at least 1 content item
  m_numContent = std::max(1u, n);
}

void HttpClientApp::SetZipf(bool z) {
  m_zipf = z;
}

void HttpClientApp::SetZipfS(double s) {
  // Ensure positive skew parameter
  m_zipfS = s > 0 ? s : 1.0;
}

void HttpClientApp::SetNumServices(uint32_t n) {
  m_numServices = std::max(1u, n);
}

void HttpClientApp::SetNumSegments(uint32_t n) {
  m_numSegments = std::max(1u, n);
}

void HttpClientApp::SetSegmentInterval(Time t) {
  m_segmentInterval = t;
}

void HttpClientApp::SetTotalTime(Time t) {
  m_totalTime = t;
}

void HttpClientApp::SetStreaming(bool s) {
  m_streaming = s;
}

void HttpClientApp::SetObjectSize(uint32_t size) {
  m_objectSize = size;
}

// ============================================================================
// Application Lifecycle
// ============================================================================

/**
 * @brief Initialize the client application.
 *
 * This method:
 * 1. Creates and configures the UDP socket
 * 2. Opens the per-request CSV file if specified
 * 3. Builds the Zipf cumulative distribution function (CDF) for efficient sampling
 * 4. Schedules the first request
 *
 * The Zipf CDF is precomputed to allow O(n) sampling where n is the number of
 * content items or services. For each rank k, P(k) = (1/k^s) / sum(1/i^s).
 */
void HttpClientApp::StartApplication() {
  // Create UDP socket if not already created
  if (!m_socket) {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind();
    m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peer), m_port));
    m_socket->SetRecvCallback(MakeCallback(&HttpClientApp::HandleRead, this));
  }

  // Open per-request CSV file and write header
  if (!m_csvPath.empty()) {
    m_csv.open(m_csvPath, std::ios::out);
    m_csv << "request_id,content,send_s,recv_s,latency_ms,cache_hit\n";
  }

  // Initialize uniform random number generator for sampling
  m_uni = CreateObject<UniformRandomVariable>();

  // Build Zipf CDF depending on mode: services (streaming) or content (regular)
  // The CDF allows efficient sampling: generate U(0,1), find first index where CDF >= U
  if (m_streaming) {
    // Streaming mode: build CDF for services
    if (m_numServices > 1 && m_zipf) {
      m_zipfCum.resize(m_numServices);

      // Calculate normalization constant: sum of 1/k^s for all ranks
      double sum = 0.0;
      for (uint32_t k = 1; k <= m_numServices; ++k) {
        sum += 1.0 / std::pow(static_cast<double>(k), m_zipfS);
      }

      // Build cumulative distribution
      double run = 0.0;
      for (uint32_t k = 1; k <= m_numServices; ++k) {
        run += (1.0 / std::pow(static_cast<double>(k), m_zipfS)) / sum;
        m_zipfCum[k - 1] = run;
      }
    }
  } else {
    // Regular mode: build CDF for content items
    if (m_numContent > 1 && m_zipf) {
      m_zipfCum.resize(m_numContent);

      // Calculate normalization constant
      double sum = 0.0;
      for (uint32_t k = 1; k <= m_numContent; ++k) {
        sum += 1.0 / std::pow(static_cast<double>(k), m_zipfS);
      }

      // Build cumulative distribution
      double run = 0.0;
      for (uint32_t k = 1; k <= m_numContent; ++k) {
        run += (1.0 / std::pow(static_cast<double>(k), m_zipfS)) / sum;
        m_zipfCum[k - 1] = run;
      }
    }
  }

  // Schedule the first request
  ScheduleNext();
}

/**
 * @brief Clean up when application stops.
 *
 * Closes the socket and CSV files, then writes summary statistics.
 */
void HttpClientApp::StopApplication() {
  if (m_socket) {
    m_socket->Close();
  }
  if (m_csv.is_open()) {
    m_csv.close();
  }
  WriteSummary();
}

// ============================================================================
// Request Scheduling
// ============================================================================

/**
 * @brief Schedule the next request based on current mode.
 *
 * In regular mode, stops after m_total requests are sent.
 * In streaming mode, stops when simulation time reaches m_totalTime.
 */
void HttpClientApp::ScheduleNext() {
  if (m_streaming) {
    // Streaming mode: stop when simulation time is reached
    if (Simulator::Now() >= m_totalTime) {
      return;
    }
    m_event = Simulator::Schedule(m_segmentInterval, &HttpClientApp::SendOne, this);
  } else {
    // Regular mode: stop after sending m_total requests
    if (m_sent >= m_total) {
      return;
    }
    m_event = Simulator::Schedule(m_interval, &HttpClientApp::SendOne, this);
  }
}

// ============================================================================
// Resource Selection
// ============================================================================

/**
 * @brief Select the next resource to request.
 *
 * In regular mode:
 * - If numContent == 1: returns the default resource
 * - If Zipf enabled: samples from Zipf distribution using precomputed CDF
 * - Otherwise: samples uniformly from available content
 *
 * In streaming mode:
 * - If not in a sequence: selects a new service using Zipf/uniform
 * - Returns the next segment in the current service sequence
 * - When all segments are fetched, marks sequence as complete
 *
 * @return Resource path (e.g., "/file-3" or "/service-1/seg-5")
 */
std::string HttpClientApp::PickResource() {
  if (!m_streaming) {
    // Regular mode: select a content item
    if (m_numContent <= 1) {
      return m_resource;
    }

    uint32_t idx = 0;
    if (m_zipf && !m_zipfCum.empty()) {
      // Sample from Zipf distribution using inverse CDF method
      double r = m_uni->GetValue(0.0, 1.0);
      for (uint32_t i = 0; i < m_zipfCum.size(); ++i) {
        if (m_zipfCum[i] >= r) {
          idx = i;
          break;
        }
      }
    } else {
      // Uniform random selection
      idx = static_cast<uint32_t>(m_uni->GetInteger(0, static_cast<int64_t>(m_numContent) - 1));
    }
    return std::string("/file-") + std::to_string(idx + 1);
  }

  // Streaming mode: pick or continue a service sequence
  if (!m_inSequence) {
    // Select a new service
    uint32_t sidx = 0;
    if (m_zipf && !m_zipfCum.empty()) {
      // Sample service from Zipf distribution
      double r = m_uni->GetValue(0.0, 1.0);
      for (uint32_t i = 0; i < m_zipfCum.size(); ++i) {
        if (m_zipfCum[i] >= r) {
          sidx = i;
          break;
        }
      }
    } else {
      // Uniform random service selection
      sidx = static_cast<uint32_t>(m_uni->GetInteger(0, static_cast<int64_t>(m_numServices) - 1));
    }
    m_currentService = sidx + 1;  // Services are 1-indexed in names
    m_nextSegment = 1;
    m_inSequence = true;
  }

  // Build resource name: /service-X/seg-Y
  std::string res = std::string("/service-") + std::to_string(m_currentService)
                    + std::string("/seg-") + std::to_string(m_nextSegment);

  // Advance segment index for next call
  m_nextSegment++;
  if (m_nextSegment > m_numSegments) {
    m_inSequence = false;  // Finished this service sequence
  }

  return res;
}

// ============================================================================
// Request Transmission
// ============================================================================

/**
 * @brief Send a single HTTP request to the cache server.
 *
 * Creates a packet with the configured object size, adds the HttpHeader
 * with request ID and resource path, records the send time for latency
 * calculation, and transmits via UDP.
 */
void HttpClientApp::SendOne() {
  // Generate unique request ID
  uint32_t id = m_nextId++;

  // Create packet with specified object size
  Ptr<Packet> p = Create<Packet>(m_objectSize);

  // Select resource and add header
  std::string res = PickResource();
  HttpHeader hdr(id, res);
  p->AddHeader(hdr);

  // Record send time and content for latency calculation
  m_sendTimes[id] = std::make_pair(Simulator::Now(), res);

  NS_LOG_INFO("Client sending id=" << id << " res=" << res);
  m_socket->Send(p);

  // Increment sent counter in non-streaming mode
  if (!m_streaming) {
    m_sent++;
  }

  ScheduleNext();
}

// ============================================================================
// Response Handling
// ============================================================================

/**
 * @brief Handle incoming response packets from the cache server.
 *
 * For each received packet:
 * 1. Extracts the HttpHeader to get request ID and resource
 * 2. Calculates round-trip latency from recorded send time
 * 3. Detects cache hit/miss from the 'H' or 'M' suffix in resource
 * 4. Updates per-content statistics
 * 5. Logs to per-request CSV if enabled
 *
 * @param socket The socket that received data
 */
void HttpClientApp::HandleRead(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> p;

  while ((p = socket->RecvFrom(from))) {
    // Extract header from packet
    HttpHeader hdr;
    p->RemoveHeader(hdr);

    // Find the corresponding send time
    auto it = m_sendTimes.find(hdr.GetRequestId());
    if (it != m_sendTimes.end()) {
      Time s = it->second.first;            // Send time
      std::string content = it->second.second;  // Content key
      Time r = Simulator::Now();            // Receive time
      double lat_ms = (r - s).GetMilliSeconds();

      // Cache hit is indicated by 'H' suffix, miss by 'M' suffix
      bool hit = (!hdr.GetResource().empty() && hdr.GetResource().back() == 'H');

      NS_LOG_INFO("Client recv id=" << hdr.GetRequestId() << " hit=" << (hit ? 1 : 0));

      // Log to per-request CSV if enabled
      if (m_csv.is_open()) {
        m_csv << hdr.GetRequestId() << ","
              << content << ","
              << s.GetSeconds() << ","
              << r.GetSeconds() << ","
              << lat_ms << ","
              << (hit ? 1 : 0) << "\n";
      }

      // Update per-content statistics
      auto& stats = m_contentStats[content];
      stats.totalRequests++;

      if (hit) {
        stats.cacheHits++;
        stats.totalHitLatency += lat_ms;
      } else {
        stats.cacheMisses++;
        stats.totalMissLatency += lat_ms;
      }

      stats.totalLatency += lat_ms;
      stats.minLatency = std::min(stats.minLatency, lat_ms);
      stats.maxLatency = std::max(stats.maxLatency, lat_ms);

      // Remove processed request from pending map
      m_sendTimes.erase(it);
    }
  }
}

// ============================================================================
// Summary Statistics
// ============================================================================

/**
 * @brief Write per-content summary statistics to CSV file.
 *
 * Generates aggregate statistics for each content item including:
 * - Total requests, cache hits, cache misses
 * - Hit rate percentage
 * - Average, minimum, and maximum latency
 * - Separate average latency for hits vs misses
 */
void HttpClientApp::WriteSummary() {
  if (m_summaryCsvPath.empty()) {
    return;
  }

  std::ofstream summary(m_summaryCsvPath, std::ios::out);
  summary << "content,total_requests,cache_hits,cache_misses,hit_rate_percent,"
          << "avg_latency_ms,min_latency_ms,max_latency_ms,"
          << "avg_hit_latency_ms,avg_miss_latency_ms\n";

  for (const auto& pair : m_contentStats) {
    const std::string& content = pair.first;
    const ContentStats& stats = pair.second;

    // Calculate derived metrics
    double hitRate = (stats.totalRequests > 0)
                     ? (100.0 * stats.cacheHits / stats.totalRequests)
                     : 0.0;
    double avgLatency = (stats.totalRequests > 0)
                        ? (stats.totalLatency / stats.totalRequests)
                        : 0.0;
    double avgHitLatency = (stats.cacheHits > 0)
                           ? (stats.totalHitLatency / stats.cacheHits)
                           : 0.0;
    double avgMissLatency = (stats.cacheMisses > 0)
                            ? (stats.totalMissLatency / stats.cacheMisses)
                            : 0.0;

    summary << content << ","
            << stats.totalRequests << ","
            << stats.cacheHits << ","
            << stats.cacheMisses << ","
            << hitRate << ","
            << avgLatency << ","
            << stats.minLatency << ","
            << stats.maxLatency << ","
            << avgHitLatency << ","
            << avgMissLatency << "\n";
  }

  summary.close();
}

/**
 * @brief Get per-content statistics for external aggregation.
 *
 * Used by scenario files to aggregate statistics across multiple clients
 * for global analysis.
 *
 * @return Const reference to the content statistics map
 */
const std::unordered_map<std::string, HttpClientApp::ContentStats>&
HttpClientApp::GetContentStats() const {
  return m_contentStats;
}

} // namespace ns3
