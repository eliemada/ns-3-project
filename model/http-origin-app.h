/**
 * @file http-origin-app.h
 * @brief HTTP origin server application for ns-3 network simulation.
 *
 * This file defines the HttpOriginApp class which simulates an HTTP origin server.
 * The origin server responds to cache miss requests with configurable delay
 * to model backend server processing time.
 *
 * @author Elie Bruno
 * @date 2025
 *
 * @see HttpCacheApp for the cache server that forwards misses to this origin
 * @see HttpClientApp for the client implementation
 */

#pragma once

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/nstime.h"

namespace ns3 {

/**
 * @class HttpOriginApp
 * @brief Simulates an HTTP origin server with configurable response delay.
 *
 * HttpOriginApp is an ns-3 Application that models the backend origin server
 * in an HTTP caching scenario. It provides:
 *
 * **Core Functionality:**
 * - Listens for incoming requests from the cache server
 * - Simulates server processing with configurable delay
 * - Echoes the request ID and resource back in the response
 *
 * **Use Case:**
 * In the HTTP cache simulation topology, the origin server sits behind the
 * cache server. When the cache experiences a miss, it forwards the request
 * to the origin, which responds after a configurable delay. This delay
 * models the higher latency of fetching content from the origin compared
 * to serving from cache.
 *
 * **Typical Topology:**
 * @code
 *   [Clients] ---> [Cache Server] ---> [Origin Server]
 *                        |                    |
 *                   port 8080            port 8081
 * @endcode
 *
 * Example usage:
 * @code
 *   Ptr<HttpOriginApp> origin = CreateObject<HttpOriginApp>();
 *   origin->SetListenPort(8081);
 *   origin->SetServiceDelay(MilliSeconds(50));  // 50ms processing time
 *   origin->SetObjectSize(10240);               // 10KB objects
 *   node->AddApplication(origin);
 * @endcode
 */
class HttpOriginApp : public Application {
public:
  /**
   * @brief Get the TypeId for this class.
   * @return The TypeId for HttpOriginApp.
   */
  static TypeId GetTypeId();

  /**
   * @brief Default constructor.
   */
  HttpOriginApp();

  // ==================== Configuration Methods ====================

  /**
   * @brief Set the port to listen for incoming requests.
   *
   * The cache server should be configured to send requests to this port.
   *
   * @param p UDP port number (default 8081).
   */
  void SetListenPort(uint16_t p);

  /**
   * @brief Set the simulated server processing delay.
   *
   * This delay is applied before sending each response, modeling the
   * time required to fetch/generate content at the origin server.
   * This is typically much higher than cache hit delay.
   *
   * @param t Processing delay (e.g., MilliSeconds(50)).
   */
  void SetServiceDelay(Time t);

  /**
   * @brief Set the size of objects to serve.
   *
   * Determines the packet payload size in responses.
   *
   * @param size Object size in bytes (default 1024).
   */
  void SetObjectSize(uint32_t size);

private:
  // ==================== Application Lifecycle ====================

  /**
   * @brief Initialize the origin server.
   *
   * Creates a UDP socket and starts listening for incoming requests.
   */
  void StartApplication() override;

  /**
   * @brief Clean up on application stop.
   *
   * Closes the listening socket.
   */
  void StopApplication() override;

  // ==================== Request Handling ====================

  /**
   * @brief Handle incoming requests from the cache server.
   *
   * Extracts the HttpHeader from each packet and schedules a
   * delayed response to simulate server processing time.
   *
   * @param sock The socket that received data.
   */
  void HandleRead(Ptr<Socket> sock);

  /**
   * @brief Send a response to the cache server.
   *
   * Creates a response packet echoing the request ID and resource.
   * This is called after the service delay has elapsed.
   *
   * @param reqId The request ID to echo back.
   * @param to The address to send the response to.
   * @param resource The resource path to echo back.
   */
  void Respond(uint32_t reqId, const Address& to, const std::string& resource);

  // ==================== Member Variables ====================

  Ptr<Socket> m_sock;             ///< UDP socket for receiving requests
  uint16_t m_port = 8081;         ///< Listening port
  Time m_delay{MilliSeconds(2)};  ///< Simulated processing delay
  uint32_t m_objectSize = 1024;   ///< Object size in bytes
};

} // namespace ns3
