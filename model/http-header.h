#pragma once
#include "ns3/header.h"
#include "ns3/type-id.h"
#include <string>

namespace ns3 {

class HttpHeader : public Header {
public:
  HttpHeader() = default;
  HttpHeader(uint32_t id, std::string res) : m_requestId(id), m_resource(std::move(res)) {}

  static TypeId GetTypeId() {
    static TypeId tid = TypeId("ns3::HttpHeader")
      .SetParent<Header>()
      .AddConstructor<HttpHeader>();
    return tid;
  }
  TypeId GetInstanceTypeId() const override { return GetTypeId(); }

  void Set(uint32_t id, const std::string& res) { m_requestId = id; m_resource = res; }
  uint32_t GetRequestId() const { return m_requestId; }
  const std::string& GetResource() const { return m_resource; }

  uint32_t GetSerializedSize() const override {
    return 4 + 2 + m_resource.size(); // id + length + chars
  }
  void Serialize(Buffer::Iterator it) const override {
    it.WriteHtonU32(m_requestId);
    it.WriteHtonU16(static_cast<uint16_t>(m_resource.size()));
    for (char c : m_resource) it.WriteU8(static_cast<uint8_t>(c));
  }
  uint32_t Deserialize(Buffer::Iterator it) override {
    m_requestId = it.ReadNtohU32();
    uint16_t len = it.ReadNtohU16();
    m_resource.resize(len);
    for (uint16_t i=0;i<len;++i) m_resource[i] = static_cast<char>(it.ReadU8());
    return GetSerializedSize();
  }
  void Print(std::ostream& os) const override {
    os << "HttpHeader{ id=" << m_requestId << ", res='" << m_resource << "' }";
  }
private:
  uint32_t m_requestId = 0;
  std::string m_resource;
};

} // namespace ns3
