#pragma once
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/nstime.h"
#include <unordered_map>
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

private:
  struct Entry { std::string value; Time expiry; std::list<std::string>::iterator it; };
  void StartApplication() override;
  void StopApplication() override;
  void HandleClientRead(Ptr<Socket> sock);
  void HandleOriginRead(Ptr<Socket> sock);
  void ReplyToClient(uint32_t reqId, const std::string& resource, bool hit, const Address& to);
  void Touch(const std::string& key);
  void Insert(const std::string& key, const std::string& val);

  Ptr<Socket> m_clientSock; // listening for clients
  Ptr<Socket> m_originSock; // to talk to origin
  Address m_originAddr; uint16_t m_originPort = 8081;
  uint16_t m_listenPort = 8080;
  Time m_ttl{Seconds(5)}; uint32_t m_capacity = 64;
  Time m_cacheDelay{MilliSeconds(1)};

  // LRU structures
  std::list<std::string> m_lru;
  std::unordered_map<std::string, Entry> m_map;

  // pending miss state: reqId -> client Address
  std::unordered_map<uint32_t, Address> m_waiting;
};

} // namespace ns3
