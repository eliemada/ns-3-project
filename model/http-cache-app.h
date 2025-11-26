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

class HttpCacheApp : public Application {
public:
  static TypeId GetTypeId();
  HttpCacheApp();

  void SetListenPort(uint16_t p);
  void SetOrigin(Address a, uint16_t p);
  void SetTtl(Time t);
  void SetCapacity(uint32_t entries);
  void SetCacheDelay(Time t);
  /**
   * \brief Set the size of objects being cached
   * \param size Object size in bytes
   */
  void SetObjectSize(uint32_t size);
  void SetDynamicTtlEnabled(bool enabled);
  void SetTtlWindow(Time window);
  void SetTtlThreshold(double threshold);
  void SetTtlReduction(double reduction);
  void SetTtlEvalInterval(Time interval);

  // Progress tracking getters
  uint64_t GetTotalRequests() const { return m_totalRequests; }
  uint64_t GetTotalHits() const { return m_totalHits; }

private:
  struct Entry { std::string value; Time expiry; std::list<std::string>::iterator it; };
  void StartApplication() override;
  void StopApplication() override;
  void HandleClientRead(Ptr<Socket> sock);
  void HandleOriginRead(Ptr<Socket> sock);
  void ReplyToClient(uint32_t reqId, const std::string& resource, bool hit, const Address& to);
  void Touch(const std::string& key);
  void Insert(const std::string& key, const std::string& val);
  void RecordRequest(const std::string& service);
  void EvaluatePolicy();
  Time GetEffectiveTtl(const std::string& service);
  std::string ExtractService(const std::string& resource);

  Ptr<Socket> m_clientSock; // listening for clients
  Ptr<Socket> m_originSock; // to talk to origin
  Address m_originAddr; uint16_t m_originPort = 8081;
  uint16_t m_listenPort = 8080;
  Time m_ttl{Seconds(5)}; uint32_t m_capacity = 64;
  Time m_cacheDelay{MilliSeconds(1)};
  uint32_t m_objectSize = 1024;  ///< Object size in bytes

  // LRU structures
  std::list<std::string> m_lru;
  std::unordered_map<std::string, Entry> m_map;

  // pending miss state: reqId -> client Address
  std::unordered_map<uint32_t, Address> m_waiting;
  // To avoid request id collisions across clients, use a unique forward id when
  // forwarding to origin: forwardId -> (originalReqId, client Address)
  uint32_t m_nextForwardId = 1;
  std::unordered_map<uint32_t, std::pair<uint32_t, Address>> m_forwarding;

  // Dynamic TTL policy
  struct TimeBucket {
    Time startTime;
    std::unordered_map<std::string, uint32_t> serviceRequests;
  };
  std::list<TimeBucket> m_buckets;
  std::unordered_set<std::string> m_penalizedServices;

  bool m_dynamicTtlEnabled = false;
  Time m_ttlWindow{Seconds(300)};
  double m_ttlThreshold = 0.5;
  double m_ttlReduction = 0.5;
  Time m_ttlEvalInterval{Seconds(30)};
  Time m_bucketDuration{Seconds(10)};

  // Progress tracking counters
  uint64_t m_totalRequests = 0;
  uint64_t m_totalHits = 0;
};

} // namespace ns3
