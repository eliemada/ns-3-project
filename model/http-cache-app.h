/**
 * @file http-cache-app.h
 * @brief HTTP cache server application for ns-3 network simulation.
 *
 * This file defines the HttpCacheApp class which implements an HTTP cache server
 * with LRU eviction, TTL-based expiration, request deduplication, and an optional
 * dynamic TTL policy for fairness control.
 *
 * @author Elie Bruno
 * @date 2025
 *
 * @see HttpClientApp for the client implementation
 * @see HttpOriginApp for the origin server implementation
 */

#pragma once

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/nstime.h"
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <string>

namespace ns3 {

/**
 * @class HttpCacheApp
 * @brief Simulates an HTTP cache server with LRU eviction and TTL expiration.
 *
 * HttpCacheApp is an ns-3 Application that models HTTP cache behavior. It provides:
 *
 * **Cache Management:**
 * - LRU (Least Recently Used) eviction policy
 * - TTL (Time-To-Live) based expiration with refresh on hit
 * - Configurable cache capacity (number of entries)
 * - O(1) cache lookup, insertion, and eviction
 *
 * **Request Handling:**
 * - Listens for client requests on configurable port
 * - Forwards cache misses to origin server
 * - Pending request deduplication to avoid duplicate origin fetches
 * - Simulated cache hit delay for realistic latency modeling
 *
 * **Dynamic TTL Policy (Optional):**
 * - Monitors request distribution across services
 * - Penalizes services that exceed a threshold share of requests
 * - Reduces TTL for penalized services to improve fairness
 * - Uses sliding time window for request tracking
 *
 * **Data Structures:**
 * The cache uses a combination of:
 * - `std::list<std::string>`: Doubly-linked list for LRU ordering
 * - `std::unordered_map<std::string, Entry>`: Hash map for O(1) lookup
 *
 * This design achieves O(1) complexity for all cache operations:
 * - Lookup: Hash map access
 * - Insert: Hash map insert + list push_front
 * - Touch (move to front): List splice using stored iterator
 * - Evict: List pop_back + hash map erase
 *
 * Example usage:
 * @code
 *   Ptr<HttpCacheApp> cache = CreateObject<HttpCacheApp>();
 *   cache->SetListenPort(8080);
 *   cache->SetOrigin(originAddress, 8081);
 *   cache->SetTtl(Seconds(30));
 *   cache->SetCapacity(1000);
 *   cache->SetCacheDelay(MilliSeconds(1));
 *   node->AddApplication(cache);
 * @endcode
 */
class HttpCacheApp : public Application {
public:
  /**
   * @brief Get the TypeId for this class.
   * @return The TypeId for HttpCacheApp.
   */
  static TypeId GetTypeId();

  /**
   * @brief Default constructor.
   */
  HttpCacheApp();

  // ==================== Configuration Methods ====================

  /**
   * @brief Set the port to listen for client requests.
   * @param p UDP port number (typically 8080).
   */
  void SetListenPort(uint16_t p);

  /**
   * @brief Set the origin server address and port.
   *
   * Cache misses are forwarded to this server.
   *
   * @param a IP address of the origin server.
   * @param p UDP port of the origin server (typically 8081).
   */
  void SetOrigin(Address a, uint16_t p);

  /**
   * @brief Set the Time-To-Live for cached entries.
   *
   * Entries expire after this duration unless refreshed by access.
   *
   * @param t TTL duration (e.g., Seconds(30)).
   */
  void SetTtl(Time t);

  /**
   * @brief Set the maximum number of entries in the cache.
   *
   * When capacity is reached, the least recently used entry is evicted.
   *
   * @param entries Maximum number of cached entries.
   */
  void SetCapacity(uint32_t entries);

  /**
   * @brief Set the simulated cache hit processing delay.
   *
   * This delay is added to cache hit responses to model realistic
   * cache lookup and response preparation time.
   *
   * @param t Cache hit delay (e.g., MilliSeconds(1)).
   */
  void SetCacheDelay(Time t);

  /**
   * @brief Set the size of objects being cached.
   *
   * Affects packet payload size in responses.
   *
   * @param size Object size in bytes.
   */
  void SetObjectSize(uint32_t size);

  // ==================== Dynamic TTL Policy Settings ====================

  /**
   * @brief Enable or disable the dynamic TTL policy.
   *
   * When enabled, services that receive disproportionate request share
   * receive reduced TTL to improve fairness.
   *
   * @param enabled True to enable dynamic TTL policy.
   */
  void SetDynamicTtlEnabled(bool enabled);

  /**
   * @brief Set the time window for request tracking.
   *
   * Only requests within this window are considered for policy evaluation.
   *
   * @param window Time window duration (e.g., Seconds(300) for 5 minutes).
   */
  void SetTtlWindow(Time window);

  /**
   * @brief Set the threshold for TTL reduction.
   *
   * Services with request share exceeding this threshold are penalized.
   *
   * @param threshold Threshold as fraction (e.g., 0.5 for 50%).
   */
  void SetTtlThreshold(double threshold);

  /**
   * @brief Set the TTL reduction factor for penalized services.
   *
   * Penalized services receive TTL * (1 - reduction).
   *
   * @param reduction Reduction factor (e.g., 0.5 for 50% reduction).
   */
  void SetTtlReduction(double reduction);

  /**
   * @brief Set the interval between policy evaluations.
   * @param interval Evaluation interval (e.g., Seconds(30)).
   */
  void SetTtlEvalInterval(Time interval);

  // ==================== Statistics Access ====================

  /**
   * @brief Get the total number of requests received.
   * @return Total request count.
   */
  uint64_t GetTotalRequests() const { return m_totalRequests; }

  /**
   * @brief Get the total number of cache hits.
   * @return Total cache hit count.
   */
  uint64_t GetTotalHits() const { return m_totalHits; }

private:
  /**
   * @struct Entry
   * @brief Cache entry storing value, expiration time, and LRU position.
   *
   * Each entry maintains an iterator to its position in the LRU list,
   * enabling O(1) LRU updates without searching the list.
   */
  struct Entry {
    std::string value;                        ///< Cached value (placeholder)
    Time expiry;                              ///< Expiration time
    std::list<std::string>::iterator it;      ///< Iterator to LRU list position
  };

  // ==================== Application Lifecycle ====================

  /**
   * @brief Initialize sockets and start listening.
   *
   * Creates client-facing and origin-facing sockets, binds them,
   * and starts the dynamic TTL policy evaluation if enabled.
   */
  void StartApplication() override;

  /**
   * @brief Clean up on application stop.
   *
   * Closes all sockets.
   */
  void StopApplication() override;

  // ==================== Request Handling ====================

  /**
   * @brief Handle incoming requests from clients.
   *
   * For each request:
   * 1. Check if resource is in cache and not expired
   * 2. If hit: refresh TTL, schedule delayed response
   * 3. If miss: check if already pending, otherwise forward to origin
   *
   * @param sock The socket that received data.
   */
  void HandleClientRead(Ptr<Socket> sock);

  /**
   * @brief Handle responses from the origin server.
   *
   * When origin responds:
   * 1. Insert the resource into cache
   * 2. Reply to the original requesting client
   * 3. Reply to all clients waiting for this resource
   *
   * @param sock The socket that received data.
   */
  void HandleOriginRead(Ptr<Socket> sock);

  /**
   * @brief Send a response to a client.
   *
   * Encodes cache hit/miss status by appending 'H' or 'M' to resource.
   *
   * @param reqId Original request ID from client.
   * @param resource Resource path.
   * @param hit True if served from cache, false if from origin.
   * @param to Client address to send response to.
   */
  void ReplyToClient(uint32_t reqId, const std::string& resource, bool hit, const Address& to);

  // ==================== Cache Operations ====================

  /**
   * @brief Move an entry to the front of the LRU list and refresh TTL.
   *
   * Called on cache hit to:
   * 1. Update LRU position (most recently used)
   * 2. Refresh TTL based on current policy
   *
   * @param key Cache key to touch.
   */
  void Touch(const std::string& key);

  /**
   * @brief Insert a new entry into the cache.
   *
   * If cache is at capacity, evicts the least recently used entry first.
   * Applies dynamic TTL policy if enabled.
   *
   * @param key Cache key.
   * @param val Value to cache.
   */
  void Insert(const std::string& key, const std::string& val);

  // ==================== Dynamic TTL Policy ====================

  /**
   * @brief Record a request for dynamic TTL policy tracking.
   *
   * Adds the request to the current time bucket for later analysis.
   *
   * @param service Service identifier extracted from resource.
   */
  void RecordRequest(const std::string& service);

  /**
   * @brief Evaluate and update the dynamic TTL policy.
   *
   * Periodically called to:
   * 1. Prune old time buckets outside the window
   * 2. Aggregate request counts per service
   * 3. Identify services exceeding threshold
   * 4. Update penalized services set
   */
  void EvaluatePolicy();

  /**
   * @brief Get the effective TTL for a service.
   *
   * Returns reduced TTL for penalized services, normal TTL otherwise.
   *
   * @param service Service identifier.
   * @return Effective TTL duration.
   */
  Time GetEffectiveTtl(const std::string& service);

  /**
   * @brief Extract service name from a resource path.
   *
   * Parses "/service-X/seg-Y" to extract "service-X".
   *
   * @param resource Full resource path.
   * @return Service identifier.
   */
  std::string ExtractService(const std::string& resource);

  // ==================== Member Variables ====================

  // Network sockets
  Ptr<Socket> m_clientSock;       ///< Socket for client connections
  Ptr<Socket> m_originSock;       ///< Socket for origin server communication

  // Origin server configuration
  Address m_originAddr;           ///< Origin server IP address
  uint16_t m_originPort = 8081;   ///< Origin server port

  // Cache configuration
  uint16_t m_listenPort = 8080;   ///< Port to listen for clients
  Time m_ttl{Seconds(5)};         ///< Default TTL for cache entries
  uint32_t m_capacity = 64;       ///< Maximum cache entries
  Time m_cacheDelay{MilliSeconds(1)}; ///< Simulated cache hit delay
  uint32_t m_objectSize = 1024;   ///< Object size in bytes

  // LRU cache data structures
  std::list<std::string> m_lru;   ///< LRU list (MRU at front)
  std::unordered_map<std::string, Entry> m_map; ///< Key -> Entry map

  /**
   * @brief Legacy waiting map (deprecated, kept for compatibility).
   */
  std::unordered_map<uint32_t, Address> m_waiting;

  /**
   * @brief Forward ID counter for origin requests.
   *
   * Used to generate unique IDs when forwarding to origin,
   * avoiding collisions between clients.
   */
  uint32_t m_nextForwardId = 1;

  /**
   * @brief Mapping from forward ID to original request info.
   *
   * Maps forwardId -> (originalReqId, clientAddress).
   * Used to route origin responses back to correct client.
   */
  std::unordered_map<uint32_t, std::pair<uint32_t, Address>> m_forwarding;

  /**
   * @brief Pending request tracking for deduplication.
   *
   * Maps resource -> list of (reqId, clientAddr) waiting for it.
   * Prevents duplicate origin fetches when multiple clients request
   * the same missing resource simultaneously.
   */
  std::unordered_map<std::string, std::vector<std::pair<uint32_t, Address>>> m_pendingRequests;

  // ==================== Dynamic TTL Policy State ====================

  /**
   * @struct TimeBucket
   * @brief Time bucket for tracking requests in sliding window.
   *
   * Each bucket covers a fixed duration and tracks request counts
   * per service during that period.
   */
  struct TimeBucket {
    Time startTime;               ///< Start time of this bucket
    std::unordered_map<std::string, uint32_t> serviceRequests; ///< Service -> count
  };

  std::list<TimeBucket> m_buckets;                ///< Sliding window of time buckets
  std::unordered_set<std::string> m_penalizedServices; ///< Currently penalized services

  // Dynamic TTL policy configuration
  bool m_dynamicTtlEnabled = false;   ///< Whether dynamic TTL is enabled
  Time m_ttlWindow{Seconds(300)};     ///< Sliding window duration
  double m_ttlThreshold = 0.5;        ///< Request share threshold for penalty
  double m_ttlReduction = 0.5;        ///< TTL reduction factor (0.5 = 50% reduction)
  Time m_ttlEvalInterval{Seconds(30)}; ///< Policy evaluation interval
  Time m_bucketDuration{Seconds(10)}; ///< Duration of each time bucket

  // Statistics counters
  uint64_t m_totalRequests = 0;       ///< Total requests received
  uint64_t m_totalHits = 0;           ///< Total cache hits
};

} // namespace ns3
