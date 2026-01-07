/**
 * @file http-cache-app.cc
 * @brief Implementation of the HTTP cache server application for ns-3.
 *
 * This file implements the HttpCacheApp class which provides an HTTP cache
 * with LRU eviction, TTL expiration, request deduplication, and an optional
 * dynamic TTL policy for fairness control among services.
 *
 * @author Elie Bruno
 * @date 2025
 */

#include "http-cache-app.h"
#include "http-header.h"
#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/simulator.h"

namespace ns3 {

// Define logging component for this class
NS_LOG_COMPONENT_DEFINE("HttpCacheApp");

// ============================================================================
// TypeId Registration
// ============================================================================

TypeId HttpCacheApp::GetTypeId() {
  static TypeId tid = TypeId("ns3::HttpCacheApp")
    .SetParent<Application>()
    .AddConstructor<HttpCacheApp>();
  return tid;
}

// ============================================================================
// Constructor
// ============================================================================

HttpCacheApp::HttpCacheApp() = default;

// ============================================================================
// Configuration Setters
// ============================================================================

void HttpCacheApp::SetListenPort(uint16_t p) {
  m_listenPort = p;
}

void HttpCacheApp::SetOrigin(Address a, uint16_t p) {
  m_originAddr = a;
  m_originPort = p;
}

void HttpCacheApp::SetTtl(Time t) {
  m_ttl = t;
}

void HttpCacheApp::SetCapacity(uint32_t c) {
  m_capacity = c;
}

void HttpCacheApp::SetCacheDelay(Time t) {
  m_cacheDelay = t;
}

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

// ============================================================================
// Dynamic TTL Policy Helper Methods
// ============================================================================

/**
 * @brief Extract the service identifier from a resource path.
 *
 * Parses resource paths in the format "/service-X/seg-Y" or "/service-X"
 * to extract the service identifier "service-X".
 *
 * @param resource The full resource path.
 * @return The service identifier, or empty string if parsing fails.
 */
std::string HttpCacheApp::ExtractService(const std::string& resource) {
  if (resource.empty()) {
    return "";
  }

  // Skip leading slash if present
  size_t start = (resource[0] == '/') ? 1 : 0;

  // Find the next slash (end of service name)
  size_t end = resource.find('/', start);

  if (end == std::string::npos) {
    // No second slash, entire remainder is service name
    return resource.substr(start);
  }

  return resource.substr(start, end - start);
}

/**
 * @brief Record a request for dynamic TTL policy tracking.
 *
 * Maintains a sliding window of time buckets, each tracking request counts
 * per service. Creates new buckets as needed when time advances.
 *
 * @param service The service identifier.
 */
void HttpCacheApp::RecordRequest(const std::string& service) {
  if (!m_dynamicTtlEnabled || service.empty()) {
    return;
  }

  Time now = Simulator::Now();

  // Create new bucket if none exist or current bucket has expired
  if (m_buckets.empty() || (now - m_buckets.back().startTime) >= m_bucketDuration) {
    TimeBucket bucket;
    bucket.startTime = now;
    m_buckets.push_back(bucket);
  }

  // Increment count for this service in current bucket
  m_buckets.back().serviceRequests[service]++;
}

/**
 * @brief Get the effective TTL for a service, considering penalties.
 *
 * If the service is in the penalized set, returns reduced TTL.
 * Otherwise returns the standard TTL.
 *
 * @param service The service identifier.
 * @return The TTL to use for this service.
 */
Time HttpCacheApp::GetEffectiveTtl(const std::string& service) {
  if (!m_dynamicTtlEnabled) {
    return m_ttl;
  }

  if (m_penalizedServices.count(service) > 0) {
    // Apply TTL reduction for penalized services
    return m_ttl * (1.0 - m_ttlReduction);
  }

  return m_ttl;
}

/**
 * @brief Evaluate and update the dynamic TTL policy.
 *
 * This method runs periodically to:
 * 1. Remove old time buckets outside the sliding window
 * 2. Aggregate request counts across all active buckets
 * 3. Identify services exceeding the threshold share
 * 4. Update the penalized services set
 *
 * Services receiving more than m_ttlThreshold fraction of total requests
 * are penalized with reduced TTL to improve cache fairness.
 */
void HttpCacheApp::EvaluatePolicy() {
  if (!m_dynamicTtlEnabled) {
    return;
  }

  Time now = Simulator::Now();
  Time cutoff = now - m_ttlWindow;

  // Step 1: Prune old buckets outside the sliding window
  while (!m_buckets.empty() && m_buckets.front().startTime < cutoff) {
    m_buckets.pop_front();
  }

  // Step 2: Aggregate request counts per service across all buckets
  std::unordered_map<std::string, uint32_t> totals;
  uint32_t grandTotal = 0;

  for (const auto& bucket : m_buckets) {
    for (const auto& pair : bucket.serviceRequests) {
      totals[pair.first] += pair.second;
      grandTotal += pair.second;
    }
  }

  // Step 3: Determine which services should be penalized
  m_penalizedServices.clear();

  if (grandTotal > 0) {
    for (const auto& pair : totals) {
      double share = static_cast<double>(pair.second) / grandTotal;
      if (share > m_ttlThreshold) {
        m_penalizedServices.insert(pair.first);
        NS_LOG_INFO("Dynamic TTL: service " << pair.first
                    << " penalized (share=" << share << ")");
      }
    }
  }

  // Step 4: Schedule next evaluation
  Simulator::Schedule(m_ttlEvalInterval, &HttpCacheApp::EvaluatePolicy, this);
}

// ============================================================================
// Application Lifecycle
// ============================================================================

/**
 * @brief Initialize the cache application.
 *
 * Creates and configures two UDP sockets:
 * 1. Client socket: Listens for incoming client requests
 * 2. Origin socket: Connects to origin server for cache misses
 *
 * Also starts the dynamic TTL policy evaluation if enabled.
 */
void HttpCacheApp::StartApplication() {
  // Create socket for client connections
  m_clientSock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_clientSock->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_listenPort));
  m_clientSock->SetRecvCallback(MakeCallback(&HttpCacheApp::HandleClientRead, this));

  // Create socket for origin server communication
  m_originSock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_originSock->Bind();
  m_originSock->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_originAddr), m_originPort));
  m_originSock->SetRecvCallback(MakeCallback(&HttpCacheApp::HandleOriginRead, this));

  // Start dynamic TTL policy evaluation if enabled
  if (m_dynamicTtlEnabled) {
    Simulator::Schedule(m_ttlEvalInterval, &HttpCacheApp::EvaluatePolicy, this);
  }
}

/**
 * @brief Clean up when application stops.
 */
void HttpCacheApp::StopApplication() {
  if (m_clientSock) {
    m_clientSock->Close();
  }
  if (m_originSock) {
    m_originSock->Close();
  }
}

// ============================================================================
// Cache Operations
// ============================================================================

/**
 * @brief Move a cache entry to the front of the LRU list and refresh its TTL.
 *
 * This method is called on cache hits to:
 * 1. Update the entry's position in the LRU list (move to front = most recently used)
 * 2. Refresh the entry's TTL based on current policy settings
 *
 * The LRU update is O(1) because we store an iterator to the entry's list position.
 *
 * @param key The cache key to touch.
 */
void HttpCacheApp::Touch(const std::string& key) {
  auto it = m_map.find(key);
  if (it == m_map.end()) {
    return;
  }

  // Update LRU position: remove from current position, add to front
  m_lru.erase(it->second.it);
  m_lru.push_front(key);
  it->second.it = m_lru.begin();

  // Refresh TTL on access (important for keeping popular items cached)
  std::string service = ExtractService(key);
  Time oldExpiry = it->second.expiry;
  Time newTtl = GetEffectiveTtl(service);
  it->second.expiry = Simulator::Now() + newTtl;

  NS_LOG_INFO("TOUCH: Refreshed TTL for " << key
              << " from " << oldExpiry.GetSeconds() << "s to "
              << it->second.expiry.GetSeconds() << "s (new TTL="
              << newTtl.GetSeconds() << "s)");
}

/**
 * @brief Insert a new entry into the cache.
 *
 * If the cache is at capacity, evicts the least recently used entry first
 * (the entry at the back of the LRU list).
 *
 * The new entry is placed at the front of the LRU list (most recently used).
 * TTL is determined by the dynamic TTL policy if enabled.
 *
 * @param key The cache key.
 * @param val The value to cache.
 */
void HttpCacheApp::Insert(const std::string& key, const std::string& val) {
  auto now = Simulator::Now();

  // Evict LRU entry if cache is at capacity
  if (m_map.size() >= m_capacity) {
    std::string evictKey = m_lru.back();
    NS_LOG_INFO("EVICTION: Evicting " << evictKey << " to make room for " << key
                << " (cache full at " << m_capacity << " items)");
    m_lru.pop_back();
    m_map.erase(evictKey);
  }

  // Insert new entry at front of LRU list
  m_lru.push_front(key);

  // Determine TTL based on service and policy
  std::string service = ExtractService(key);
  Time ttl = GetEffectiveTtl(service);

  // Create cache entry with expiration time and LRU iterator
  m_map[key] = Entry{val, now + ttl, m_lru.begin()};

  NS_LOG_INFO("INSERT: Cached " << key << " with TTL=" << ttl.GetSeconds()
              << "s (expires at " << (now + ttl).GetSeconds() << "s)");
}

// ============================================================================
// Request Handling
// ============================================================================

/**
 * @brief Handle incoming requests from clients.
 *
 * For each received request:
 * 1. Extract the resource key from the HttpHeader
 * 2. Record the request for dynamic TTL policy (if enabled)
 * 3. Check if resource is in cache and not expired:
 *    - HIT: Refresh TTL, schedule delayed response
 *    - MISS: Check if already pending, forward to origin if not
 *
 * The pending request deduplication prevents multiple origin fetches when
 * several clients request the same missing resource simultaneously.
 *
 * @param sock The socket that received data.
 */
void HttpCacheApp::HandleClientRead(Ptr<Socket> sock) {
  Address from;
  Ptr<Packet> p;

  while ((p = sock->RecvFrom(from))) {
    // Extract header from packet
    HttpHeader hdr;
    p->RemoveHeader(hdr);

    std::string key = hdr.GetResource();
    std::string service = ExtractService(key);

    // Record request for dynamic TTL policy
    RecordRequest(service);
    m_totalRequests++;

    // Check cache for this resource
    auto it = m_map.find(key);
    auto now = Simulator::Now();

    if (it != m_map.end() && it->second.expiry > now) {
      // CACHE HIT: Entry exists and is not expired
      NS_LOG_INFO("Cache HIT key=" << key << " at time=" << now.GetSeconds()
                  << "s (expires at " << it->second.expiry.GetSeconds() << "s)");

      m_totalHits++;
      Touch(key);  // Refresh LRU position and TTL

      // Schedule response with simulated cache processing delay
      Simulator::Schedule(m_cacheDelay, &HttpCacheApp::ReplyToClient, this,
                          hdr.GetRequestId(), key, true, from);
    } else {
      // CACHE MISS: Entry doesn't exist or is expired
      if (it != m_map.end()) {
        NS_LOG_INFO("Cache MISS (EXPIRED) key=" << key << " at time="
                    << now.GetSeconds() << "s (expired at "
                    << it->second.expiry.GetSeconds() << "s, age="
                    << (now - it->second.expiry).GetSeconds() << "s)");
      } else {
        NS_LOG_INFO("Cache MISS (NOT FOUND) key=" << key
                    << " at time=" << now.GetSeconds() << "s");
      }

      // Check if this resource is already being fetched from origin
      auto pendingIt = m_pendingRequests.find(key);
      if (pendingIt != m_pendingRequests.end()) {
        // Already pending: add this client to the waiting list
        NS_LOG_INFO("PENDING: Request for " << key
                    << " already in flight, adding client to waiting list (now "
                    << (pendingIt->second.size() + 1) << " waiting)");
        pendingIt->second.push_back(std::make_pair(hdr.GetRequestId(), from));
      } else {
        // First request for this resource: forward to origin
        NS_LOG_INFO("FORWARD: Sending first request for " << key << " to origin");

        // Generate unique forward ID to avoid collisions between clients
        uint32_t origReqId = hdr.GetRequestId();
        uint32_t fid = m_nextForwardId++;
        m_forwarding[fid] = std::make_pair(origReqId, from);

        // Mark this resource as pending (empty waiting list)
        m_pendingRequests[key] = std::vector<std::pair<uint32_t, Address>>();

        // Create and send request to origin with forward ID
        HttpHeader fhdr(fid, key);
        Ptr<Packet> fwd = Create<Packet>(m_objectSize);
        fwd->AddHeader(fhdr);
        m_originSock->Send(fwd);
      }
    }
  }
}

/**
 * @brief Handle responses from the origin server.
 *
 * When the origin responds:
 * 1. Insert the resource into the cache
 * 2. Look up the original client that initiated the request
 * 3. Reply to the original client
 * 4. Reply to all other clients waiting for this resource
 *
 * This completes the pending request deduplication by serving all waiting
 * clients from a single origin fetch.
 *
 * @param sock The socket that received data.
 */
void HttpCacheApp::HandleOriginRead(Ptr<Socket> sock) {
  Address from;
  Ptr<Packet> p;

  while ((p = sock->RecvFrom(from))) {
    // Extract header from packet (origin echoes the key)
    HttpHeader hdr;
    p->RemoveHeader(hdr);

    std::string key = hdr.GetResource();

    // Insert resource into cache
    Insert(key, "data");

    // Look up the original client using forward ID
    uint32_t fid = hdr.GetRequestId();
    auto itf = m_forwarding.find(fid);

    if (itf != m_forwarding.end()) {
      uint32_t origReqId = itf->second.first;
      Address clientAddr = itf->second.second;

      // Reply to the original client (cache miss = false)
      ReplyToClient(origReqId, key, false, clientAddr);
      m_forwarding.erase(itf);
    }

    // Serve all clients waiting for this resource
    auto pendingIt = m_pendingRequests.find(key);
    if (pendingIt != m_pendingRequests.end()) {
      NS_LOG_INFO("SERVING PENDING: Responding to " << pendingIt->second.size()
                  << " waiting clients for " << key);

      for (const auto& waiting : pendingIt->second) {
        ReplyToClient(waiting.first, key, false, waiting.second);
      }
      m_pendingRequests.erase(pendingIt);
    }
  }
}

/**
 * @brief Send a response to a client.
 *
 * Creates a response packet with the HttpHeader containing the request ID
 * and resource path. The cache hit/miss status is encoded by appending
 * 'H' (hit) or 'M' (miss) to the resource string.
 *
 * @param reqId The original request ID from the client.
 * @param resource The resource path.
 * @param hit True if served from cache, false if from origin.
 * @param to The client address to send the response to.
 */
void HttpCacheApp::ReplyToClient(uint32_t reqId, const std::string& resource,
                                  bool hit, const Address& to) {
  // Encode hit/miss by suffixing resource with 'H' or 'M'
  std::string res = resource + (hit ? "H" : "M");

  // Create response packet with header
  Ptr<Packet> resp = Create<Packet>(m_objectSize);
  HttpHeader hdr(reqId, res);
  resp->AddHeader(hdr);

  // Send to client
  m_clientSock->SendTo(resp, 0, to);
}

} // namespace ns3
