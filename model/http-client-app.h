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

class HttpClientApp : public Application {
public:
  static TypeId GetTypeId();
  HttpClientApp();
  ~HttpClientApp() override;

  struct ContentStats {
    uint32_t totalRequests = 0;
    uint32_t cacheHits = 0;
    uint32_t cacheMisses = 0;
    double totalLatency = 0.0;
    double totalHitLatency = 0.0;
    double totalMissLatency = 0.0;
    double minLatency = 1e9;
    double maxLatency = 0.0;
  };

  void SetRemote(Address address, uint16_t port);
  void SetInterval(Time t);
  void SetResource(const std::string& r);
  void SetCsvPath(const std::string& p);
  void SetSummaryCsvPath(const std::string& p);
  void SetTotalRequests(uint32_t n);

  // Randomization controls
  void SetNumContent(uint32_t n);
  void SetZipf(bool z);
  void SetZipfS(double s);

  // Get statistics for global aggregation
  const std::unordered_map<std::string, ContentStats>& GetContentStats() const;

private:

  void StartApplication() override;
  void StopApplication() override;
  void ScheduleNext();
  void SendOne();
  void HandleRead(Ptr<Socket> socket);
  void WriteSummary();
  std::string PickResource();

  Ptr<Socket> m_socket;
  Address m_peer;
  uint16_t m_port = 8080;
  EventId m_event;
  Time m_interval{Seconds(1)};
  std::string m_resource{"/obj"};
  std::unordered_map<uint32_t, std::pair<Time, std::string>> m_sendTimes;
  std::ofstream m_csv;
  std::string m_csvPath{"client_metrics.csv"};
  std::string m_summaryCsvPath{""};
  std::unordered_map<std::string, ContentStats> m_contentStats;
  uint32_t m_nextId = 1;
  uint32_t m_total = 10;
  uint32_t m_sent = 0;

  // randomization
  uint32_t m_numContent = 1;
  bool m_zipf = false;
  double m_zipfS = 1.0;
  Ptr<UniformRandomVariable> m_uni;
  std::vector<double> m_zipfCum;
};

} // namespace ns3
