/**
 * @file http-client-app.h
 * @brief HTTP client application for ns-3 network simulation.
 *
 * This file defines the HttpClientApp class which simulates HTTP client behavior,
 * including request generation, latency measurement, and statistics collection.
 * Supports both regular and streaming (video) request patterns with Zipf distribution.
 *
 * @author Elie Bruno
 * @date 2025
 *
 * @see HttpCacheApp for the cache server implementation
 * @see HttpOriginApp for the origin server implementation
 */

#pragma once

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/event-id.h"
#include "ns3/address.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include <unordered_map>
#include <fstream>
#include <string>
#include <vector>

namespace ns3 {

/**
 * @class HttpClientApp
 * @brief Simulates an HTTP client that generates requests and collects metrics.
 *
 * HttpClientApp is an ns-3 Application that models HTTP client behavior. It supports:
 *
 * **Request Generation Modes:**
 * - **Regular mode**: Generates requests for content items at fixed intervals
 * - **Streaming mode**: Simulates video streaming by picking a service via Zipf
 *   distribution, then fetching segments sequentially
 *
 * **Content Popularity Modeling:**
 * - **Uniform distribution**: Each content item has equal probability
 * - **Zipf distribution**: Models realistic web traffic where popular content
 *   (low rank) receives disproportionately more requests. Follows Zipf's law:
 *   P(k) ∝ 1/k^s, where k is the rank and s is the skew parameter.
 *
 * **Metrics Collection:**
 * - Per-request latency tracking
 * - Cache hit/miss detection
 * - Per-content aggregate statistics (hit rate, avg/min/max latency)
 * - CSV output for analysis
 *
 * **Network Communication:**
 * - Uses UDP sockets for simplicity and efficiency
 * - Communicates with HttpCacheApp using HttpHeader protocol
 *
 * Example usage:
 * @code
 *   Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();
 *   client->SetRemote(cacheAddress, 8080);
 *   client->SetInterval(Seconds(0.5));
 *   client->SetNumContent(20);
 *   client->SetZipf(true);
 *   client->SetZipfS(1.2);
 *   client->SetTotalRequests(100);
 *   client->SetCsvPath("metrics.csv");
 *   node->AddApplication(client);
 * @endcode
 */
class HttpClientApp : public Application {
public:
  /**
   * @brief Get the TypeId for this class.
   * @return The TypeId for HttpClientApp.
   */
  static TypeId GetTypeId();

  /**
   * @brief Default constructor.
   */
  HttpClientApp();

  /**
   * @brief Destructor.
   */
  ~HttpClientApp() override;

  /**
   * @struct ContentStats
   * @brief Aggregate statistics for a single content item.
   *
   * Tracks cumulative metrics across all requests for a specific content key,
   * enabling per-content analysis of cache performance.
   */
  struct ContentStats {
    uint32_t totalRequests = 0;    ///< Total number of requests for this content
    uint32_t cacheHits = 0;        ///< Number of requests served from cache
    uint32_t cacheMisses = 0;      ///< Number of requests that went to origin
    double totalLatency = 0.0;     ///< Sum of all request latencies (ms)
    double totalHitLatency = 0.0;  ///< Sum of cache hit latencies (ms)
    double totalMissLatency = 0.0; ///< Sum of cache miss latencies (ms)
    double minLatency = 1e9;       ///< Minimum observed latency (ms)
    double maxLatency = 0.0;       ///< Maximum observed latency (ms)
  };

  // ==================== Configuration Methods ====================

  /**
   * @brief Set the remote cache server address and port.
   * @param address IP address of the cache server.
   * @param port UDP port of the cache server (typically 8080).
   */
  void SetRemote(Address address, uint16_t port);

  /**
   * @brief Set the interval between consecutive requests.
   * @param t Time interval (e.g., Seconds(0.5) for 2 requests/second).
   */
  void SetInterval(Time t);

  /**
   * @brief Set the default resource path for single-resource mode.
   * @param r Resource path string (e.g., "/video.mp4").
   */
  void SetResource(const std::string& r);

  /**
   * @brief Set the path for per-request CSV output.
   *
   * When set, each request's details are logged to this file.
   * CSV format: request_id,content,send_s,recv_s,latency_ms,cache_hit
   *
   * @param p File path for CSV output (empty string disables output).
   */
  void SetCsvPath(const std::string& p);

  /**
   * @brief Set the path for summary statistics CSV output.
   *
   * When set, aggregate statistics per content are written on application stop.
   *
   * @param p File path for summary CSV (empty string disables output).
   */
  void SetSummaryCsvPath(const std::string& p);

  /**
   * @brief Set the total number of requests to generate (regular mode).
   * @param n Total request count.
   */
  void SetTotalRequests(uint32_t n);

  // ==================== Content Distribution Settings ====================

  /**
   * @brief Set the number of distinct content items.
   *
   * Content items are named "/file-1", "/file-2", ..., "/file-N".
   * When n=1, all requests go to the default resource.
   *
   * @param n Number of content items (minimum 1).
   */
  void SetNumContent(uint32_t n);

  /**
   * @brief Enable or disable Zipf distribution for content selection.
   *
   * When enabled, content popularity follows Zipf's law where lower-ranked
   * items are exponentially more popular than higher-ranked items.
   *
   * @param z True to enable Zipf, false for uniform distribution.
   */
  void SetZipf(bool z);

  /**
   * @brief Set the Zipf skew parameter (exponent).
   *
   * Controls how skewed the popularity distribution is:
   * - s = 0: Uniform distribution
   * - s = 1: Standard Zipf (80/20 rule approximation)
   * - s > 1: More extreme concentration on popular items
   *
   * Typical web traffic uses s ∈ [0.8, 1.2].
   *
   * @param s Zipf exponent (must be positive, default 1.0).
   */
  void SetZipfS(double s);

  // ==================== Streaming Mode Settings ====================

  /**
   * @brief Set the number of streaming services.
   *
   * In streaming mode, services represent different video streams.
   * Each service contains multiple segments.
   *
   * @param n Number of services (minimum 1).
   */
  void SetNumServices(uint32_t n);

  /**
   * @brief Set the number of segments per service selection.
   *
   * When a service is selected, this many segments are fetched sequentially
   * before potentially selecting a new service.
   *
   * @param n Number of segments per viewing session (minimum 1).
   */
  void SetNumSegments(uint32_t n);

  /**
   * @brief Set the interval between segment requests in streaming mode.
   * @param t Time between consecutive segment requests.
   */
  void SetSegmentInterval(Time t);

  /**
   * @brief Set the total simulation time for streaming mode.
   *
   * In streaming mode, requests continue until this time is reached
   * (instead of using request count).
   *
   * @param t Total streaming duration.
   */
  void SetTotalTime(Time t);

  /**
   * @brief Enable or disable streaming mode.
   *
   * Streaming mode changes request patterns:
   * - Services are selected using Zipf distribution
   * - Segments within a service are fetched sequentially
   * - Simulates HTTP Adaptive Streaming (HAS) behavior
   *
   * @param s True for streaming mode, false for regular request mode.
   */
  void SetStreaming(bool s);

  /**
   * @brief Set the size of objects to request.
   *
   * This determines the packet payload size, affecting network load
   * and transmission time calculations.
   *
   * @param size Object size in bytes (default 1024).
   */
  void SetObjectSize(uint32_t size);

  // ==================== Statistics Access ====================

  /**
   * @brief Get per-content statistics for global aggregation.
   *
   * Used by scenario files to aggregate statistics across multiple clients.
   *
   * @return Const reference to the content statistics map.
   */
  const std::unordered_map<std::string, ContentStats>& GetContentStats() const;

private:
  // ==================== Application Lifecycle ====================

  /**
   * @brief Called when the application starts.
   *
   * Initializes the UDP socket, opens CSV files, builds the Zipf CDF,
   * and schedules the first request.
   */
  void StartApplication() override;

  /**
   * @brief Called when the application stops.
   *
   * Closes the socket and CSV files, writes summary statistics.
   */
  void StopApplication() override;

  // ==================== Request Generation ====================

  /**
   * @brief Schedule the next request based on current mode.
   *
   * In regular mode: schedules after m_interval until m_total reached.
   * In streaming mode: schedules after m_segmentInterval until m_totalTime.
   */
  void ScheduleNext();

  /**
   * @brief Send a single HTTP request.
   *
   * Creates a packet with HttpHeader, records send time, and transmits.
   */
  void SendOne();

  /**
   * @brief Select the next resource to request.
   *
   * Uses Zipf or uniform distribution based on configuration.
   * In streaming mode, handles service selection and segment sequencing.
   *
   * @return Resource path string (e.g., "/file-3" or "/service-1/seg-5").
   */
  std::string PickResource();

  // ==================== Response Handling ====================

  /**
   * @brief Handle incoming response packets.
   *
   * Extracts the HttpHeader, calculates latency, detects cache hit/miss,
   * updates statistics, and logs to CSV if enabled.
   *
   * @param socket The socket that received data.
   */
  void HandleRead(Ptr<Socket> socket);

  /**
   * @brief Write summary statistics to CSV file.
   *
   * Called on application stop. Writes per-content aggregate metrics
   * including hit rate, latency statistics.
   */
  void WriteSummary();

  // ==================== Member Variables ====================

  // Network communication
  Ptr<Socket> m_socket;           ///< UDP socket for communication
  Address m_peer;                 ///< Cache server address
  uint16_t m_port = 8080;         ///< Cache server port
  EventId m_event;                ///< Scheduled event for next request

  // Request timing and tracking
  Time m_interval{Seconds(1)};    ///< Interval between requests (regular mode)
  std::string m_resource{"/obj"}; ///< Default resource path
  uint32_t m_nextId = 1;          ///< Next request ID to assign
  uint32_t m_total = 10;          ///< Total requests to send (regular mode)
  uint32_t m_sent = 0;            ///< Number of requests sent

  /**
   * @brief Map of pending requests: request_id -> (send_time, content_key)
   *
   * Used to calculate latency when responses arrive.
   */
  std::unordered_map<uint32_t, std::pair<Time, std::string>> m_sendTimes;

  // CSV output
  std::ofstream m_csv;            ///< Per-request CSV file stream
  std::string m_csvPath{""};      ///< Path for per-request CSV
  std::string m_summaryCsvPath{""}; ///< Path for summary CSV

  /**
   * @brief Per-content aggregate statistics.
   *
   * Maps content key (e.g., "/file-3") to its cumulative statistics.
   */
  std::unordered_map<std::string, ContentStats> m_contentStats;

  // Content distribution (regular mode)
  uint32_t m_numContent = 1;      ///< Number of distinct content items
  bool m_zipf = false;            ///< Use Zipf distribution for content
  double m_zipfS = 1.0;           ///< Zipf skew parameter (exponent)
  Ptr<UniformRandomVariable> m_uni; ///< Uniform RNG for sampling

  /**
   * @brief Precomputed Zipf cumulative distribution function.
   *
   * Used for efficient O(n) sampling from Zipf distribution.
   * m_zipfCum[i] = P(rank <= i+1)
   */
  std::vector<double> m_zipfCum;

  // Streaming mode configuration
  uint32_t m_numServices = 1;     ///< Number of streaming services
  uint32_t m_numSegments = 1;     ///< Segments per service selection
  Time m_segmentInterval{Seconds(1)}; ///< Time between segment requests
  Time m_totalTime{Seconds(100)}; ///< Total streaming duration
  bool m_streaming = false;       ///< Streaming mode enabled

  // Streaming mode state
  uint32_t m_currentService = 0;  ///< Currently streaming service (1-indexed)
  uint32_t m_nextSegment = 1;     ///< Next segment number to request
  bool m_inSequence = false;      ///< Currently in a segment sequence

  uint32_t m_objectSize = 1024;   ///< Object size in bytes
};

} // namespace ns3
