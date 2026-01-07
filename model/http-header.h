/**
 * @file http-header.h
 * @brief Custom HTTP header class for ns-3 network simulation.
 *
 * This file defines the HttpHeader class which encapsulates HTTP request/response
 * metadata for transmission over ns-3's packet-based network simulation. The header
 * supports serialization and deserialization for network transmission.
 *
 * @author Elie Bruno
 * @date 2025
 *
 * @see ns3::Header for the base class interface
 */

#pragma once

#include "ns3/header.h"
#include "ns3/type-id.h"
#include <string>

namespace ns3 {

/**
 * @class HttpHeader
 * @brief A custom ns-3 Header for HTTP request/response simulation.
 *
 * HttpHeader encapsulates the essential metadata needed to simulate HTTP
 * requests and responses in an ns-3 network simulation. It stores:
 * - A unique request identifier for tracking requests across the network
 * - A resource string representing the requested URL/content path
 *
 * The header is designed for efficient serialization with a compact binary format:
 * - 4 bytes: Request ID (network byte order)
 * - 2 bytes: Resource string length (network byte order)
 * - N bytes: Resource string characters (UTF-8)
 *
 * @note This header is used by HttpClientApp, HttpCacheApp, and HttpOriginApp
 *       to communicate request and response information.
 *
 * Example usage:
 * @code
 *   // Creating a header for a request
 *   HttpHeader header(42, "/videos/segment_001.ts");
 *
 *   // Adding to a packet
 *   Ptr<Packet> packet = Create<Packet>(1024);
 *   packet->AddHeader(header);
 *
 *   // Extracting from a received packet
 *   HttpHeader receivedHeader;
 *   packet->RemoveHeader(receivedHeader);
 *   uint32_t reqId = receivedHeader.GetRequestId();
 *   std::string resource = receivedHeader.GetResource();
 * @endcode
 */
class HttpHeader : public Header {
public:
  /**
   * @brief Default constructor.
   *
   * Creates an empty HttpHeader with request ID 0 and empty resource string.
   */
  HttpHeader() = default;

  /**
   * @brief Parameterized constructor.
   *
   * @param id Unique identifier for this HTTP request.
   * @param res Resource path/URL being requested (moved for efficiency).
   */
  HttpHeader(uint32_t id, std::string res)
    : m_requestId(id), m_resource(std::move(res)) {}

  /**
   * @brief Get the TypeId for this class.
   *
   * This is required by the ns-3 object system for runtime type identification
   * and object creation through the factory pattern.
   *
   * @return The TypeId for HttpHeader.
   */
  static TypeId GetTypeId() {
    static TypeId tid = TypeId("ns3::HttpHeader")
      .SetParent<Header>()
      .AddConstructor<HttpHeader>();
    return tid;
  }

  /**
   * @brief Get the instance TypeId.
   * @return The TypeId of this instance.
   */
  TypeId GetInstanceTypeId() const override { return GetTypeId(); }

  /**
   * @brief Set the header fields.
   *
   * @param id Request identifier.
   * @param res Resource path/URL.
   */
  void Set(uint32_t id, const std::string& res) {
    m_requestId = id;
    m_resource = res;
  }

  /**
   * @brief Get the request identifier.
   * @return The unique request ID.
   */
  uint32_t GetRequestId() const { return m_requestId; }

  /**
   * @brief Get the resource path.
   * @return Const reference to the resource string.
   */
  const std::string& GetResource() const { return m_resource; }

  /**
   * @brief Calculate the serialized size of this header.
   *
   * The serialized format is:
   * - 4 bytes for request ID (uint32_t in network byte order)
   * - 2 bytes for string length (uint16_t in network byte order)
   * - N bytes for the resource string characters
   *
   * @return Total size in bytes when serialized.
   */
  uint32_t GetSerializedSize() const override {
    // 4 bytes (request ID) + 2 bytes (string length) + string characters
    return 4 + 2 + m_resource.size();
  }

  /**
   * @brief Serialize the header to a buffer.
   *
   * Writes the header data in network byte order for transmission.
   * The format ensures cross-platform compatibility.
   *
   * @param it Buffer iterator pointing to the write position.
   */
  void Serialize(Buffer::Iterator it) const override {
    // Write request ID in network byte order (big-endian)
    it.WriteHtonU32(m_requestId);

    // Write string length in network byte order
    it.WriteHtonU16(static_cast<uint16_t>(m_resource.size()));

    // Write string characters byte-by-byte
    for (char c : m_resource) {
      it.WriteU8(static_cast<uint8_t>(c));
    }
  }

  /**
   * @brief Deserialize the header from a buffer.
   *
   * Reads header data from network byte order format.
   *
   * @param it Buffer iterator pointing to the read position.
   * @return Number of bytes read (same as GetSerializedSize()).
   */
  uint32_t Deserialize(Buffer::Iterator it) override {
    // Read request ID from network byte order
    m_requestId = it.ReadNtohU32();

    // Read string length from network byte order
    uint16_t len = it.ReadNtohU16();

    // Read string characters
    m_resource.resize(len);
    for (uint16_t i = 0; i < len; ++i) {
      m_resource[i] = static_cast<char>(it.ReadU8());
    }

    return GetSerializedSize();
  }

  /**
   * @brief Print header information to an output stream.
   *
   * Provides human-readable representation for debugging and logging.
   *
   * @param os Output stream to write to.
   */
  void Print(std::ostream& os) const override {
    os << "HttpHeader{ id=" << m_requestId << ", res='" << m_resource << "' }";
  }

private:
  uint32_t m_requestId = 0;   ///< Unique identifier for the HTTP request
  std::string m_resource;      ///< Resource path/URL being requested
};

} // namespace ns3
