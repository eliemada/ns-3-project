#pragma once
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/nstime.h"

namespace ns3 {
class HttpOriginApp : public Application {
public:
  static TypeId GetTypeId();
  HttpOriginApp();
  void SetListenPort(uint16_t p);
  void SetServiceDelay(Time t);
  /**
   * \brief Set the size of objects to serve
   * \param size Object size in bytes
   */
  void SetObjectSize(uint32_t size);
private:
  void StartApplication() override;
  void StopApplication() override;
  void HandleRead(Ptr<Socket> sock);
  void Respond(uint32_t reqId, const Address& to, const std::string& resource);

  Ptr<Socket> m_sock; uint16_t m_port = 8081; Time m_delay{MilliSeconds(2)};
  uint32_t m_objectSize = 1024;  ///< Object size in bytes
};
}
