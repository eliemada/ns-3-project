#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/http-client-app.h"
#include "ns3/http-cache-app.h"
#include "ns3/http-origin-app.h"
#include <sstream>
#include <vector>

using namespace ns3;

int main(int argc, char** argv){
  Time::SetResolution(Time::NS);
  uint32_t nReq = 100; double interval=0.5; double cacheCapacityGB=1.0; double ttl=5.0;
  std::string resource = "/file-A"; std::string csv = "client_metrics.csv"; std::string summaryCsv = "";
  std::string globalSummaryCsv = "";
  uint32_t numContent = 1; bool zipf = false; double zipfS = 1.0; uint32_t originDelay = 1; uint32_t cacheDelay = 1;
  uint32_t numClients = 1;
  uint32_t objectSize = 1024;  // Default 1 KB
  uint32_t clientCacheBw = 100;  // Client-Cache link bandwidth (Mbps)
  uint32_t cacheOriginBw = 50;   // Cache-Origin link bandwidth (Mbps)
  CommandLine cmd;
  cmd.AddValue("nReq", "Total client requests", nReq);
  cmd.AddValue("interval", "Seconds between requests", interval);
  cmd.AddValue("cacheCapacityGB", "Cache capacity in gigabytes", cacheCapacityGB);
  cmd.AddValue("ttl", "TTL seconds", ttl);
  cmd.AddValue("resource", "Resource path (default if numContent==1)", resource);
  cmd.AddValue("csv", "Output CSV path", csv);
  cmd.AddValue("summaryCsv", "Summary statistics CSV path (optional)", summaryCsv);
  cmd.AddValue("globalSummaryCsv", "Global aggregated summary CSV path (optional)", globalSummaryCsv);
  cmd.AddValue("numContent", "Number of distinct content items (1 = fixed resource)", numContent);
  cmd.AddValue("zipf", "Use Zipf popularity over resources", zipf);
  cmd.AddValue("zipfS", "Zipf exponent s (>0)", zipfS);
  cmd.AddValue("cacheDelay", "Cache processing delay for hits (ms)", cacheDelay);
  cmd.AddValue("originDelay", "Origin processing delay (ms)", originDelay);
  cmd.AddValue("numClients", "Number of concurrent clients", numClients);
  cmd.AddValue("objectSize", "Object size in bytes (default 1024)", objectSize);
  cmd.AddValue("clientCacheBw", "Client-Cache link bandwidth (Mbps)", clientCacheBw);
  cmd.AddValue("cacheOriginBw", "Cache-Origin link bandwidth (Mbps)", cacheOriginBw);
  cmd.Parse(argc, argv);

  // Create nodes: numClients client nodes + 1 cache node + 1 origin node
  NodeContainer clientNodes;
  clientNodes.Create(numClients);

  NodeContainer serverNodes;
  serverNodes.Create(2);  // cache and origin

  NodeContainer allNodes;
  allNodes.Add(clientNodes);
  allNodes.Add(serverNodes);

  InternetStackHelper internet;
  internet.Install(allNodes);

  // Setup point-to-point links
  PointToPointHelper p2pClientCache;
  std::ostringstream clientCacheBwStr;
  clientCacheBwStr << clientCacheBw << "Mbps";
  p2pClientCache.SetDeviceAttribute("DataRate", StringValue(clientCacheBwStr.str()));
  p2pClientCache.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper p2pCacheOrigin;
  std::ostringstream cacheOriginBwStr;
  cacheOriginBwStr << cacheOriginBw << "Mbps";
  p2pCacheOrigin.SetDeviceAttribute("DataRate", StringValue(cacheOriginBwStr.str()));
  p2pCacheOrigin.SetChannelAttribute("Delay", StringValue("5ms"));

  // Create links from each client to cache
  std::vector<NetDeviceContainer> clientCacheDevices(numClients);
  std::vector<Ipv4InterfaceContainer> clientCacheInterfaces(numClients);

  Ipv4AddressHelper ip;
  Ptr<Node> cacheNode = serverNodes.Get(0);
  Ptr<Node> originNode = serverNodes.Get(1);

  for (uint32_t i = 0; i < numClients; ++i) {
    clientCacheDevices[i] = p2pClientCache.Install(clientNodes.Get(i), cacheNode);
    std::ostringstream subnet;
    subnet << "10." << (i / 256) << "." << (i % 256) << ".0";
    ip.SetBase(subnet.str().c_str(), "255.255.255.0");
    clientCacheInterfaces[i] = ip.Assign(clientCacheDevices[i]);
  }

  // Create link from cache to origin
  NetDeviceContainer cacheOriginDevices = p2pCacheOrigin.Install(cacheNode, originNode);
  ip.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer cacheOriginInterfaces = ip.Assign(cacheOriginDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t clientToCachePort = 8080;
  uint16_t cacheToOriginPort = 8081;

  // Setup origin server
  Ptr<HttpOriginApp> origin = CreateObject<HttpOriginApp>();
  origin->SetListenPort(cacheToOriginPort);
  origin->SetServiceDelay(MilliSeconds(originDelay));
  origin->SetObjectSize(objectSize);
  originNode->AddApplication(origin);
  origin->SetStartTime(Seconds(0.1));
  origin->SetStopTime(Seconds(100));

  // Calculate max objects from cache capacity in GB
  uint64_t capacityBytes = static_cast<uint64_t>(cacheCapacityGB * 1024 * 1024 * 1024);
  uint32_t maxObjects = static_cast<uint32_t>(capacityBytes / objectSize);

  std::cout << "Cache configuration:" << std::endl;
  std::cout << "  Capacity: " << cacheCapacityGB << " GB (" << capacityBytes << " bytes)" << std::endl;
  std::cout << "  Object size: " << objectSize << " bytes" << std::endl;
  std::cout << "  Max objects: " << maxObjects << std::endl;

  // Setup cache server
  Ptr<HttpCacheApp> cache = CreateObject<HttpCacheApp>();
  cache->SetListenPort(clientToCachePort);
  cache->SetOrigin(Address(cacheOriginInterfaces.GetAddress(1)), cacheToOriginPort);
  cache->SetTtl(Seconds(ttl));
  cache->SetCapacity(maxObjects);
  cache->SetCacheDelay(MilliSeconds(cacheDelay));
  cache->SetObjectSize(objectSize);
  cacheNode->AddApplication(cache);
  cache->SetStartTime(Seconds(0.2));
  cache->SetStopTime(Seconds(100));

  // Setup client applications
  std::vector<Ptr<HttpClientApp>> clientApps;  // Store for global summary
  for (uint32_t i = 0; i < numClients; ++i) {
    Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();
    client->SetRemote(Address(clientCacheInterfaces[i].GetAddress(1)), clientToCachePort);
    client->SetInterval(Seconds(interval));
    client->SetResource(resource);
    client->SetNumContent(numContent);
    client->SetZipf(zipf);
    client->SetZipfS(zipfS);
    client->SetTotalRequests(nReq);
    client->SetObjectSize(objectSize);

    // Set CSV paths with client index if multiple clients
    if (!csv.empty()) {
      if (numClients > 1) {
        size_t dotPos = csv.find_last_of('.');
        std::string baseName = (dotPos != std::string::npos) ? csv.substr(0, dotPos) : csv;
        std::string extension = (dotPos != std::string::npos) ? csv.substr(dotPos) : "";
        std::ostringstream csvPath;
        csvPath << baseName << "_client_" << i << extension;
        client->SetCsvPath(csvPath.str());
      } else {
        client->SetCsvPath(csv);
      }
    }

    // Set summary CSV paths with client index if multiple clients
    if (!summaryCsv.empty()) {
      if (numClients > 1) {
        size_t dotPos = summaryCsv.find_last_of('.');
        std::string baseName = (dotPos != std::string::npos) ? summaryCsv.substr(0, dotPos) : summaryCsv;
        std::string extension = (dotPos != std::string::npos) ? summaryCsv.substr(dotPos) : "";
        std::ostringstream summaryPath;
        summaryPath << baseName << "_client_" << i << extension;
        client->SetSummaryCsvPath(summaryPath.str());
      } else {
        client->SetSummaryCsvPath(summaryCsv);
      }
    }

    clientNodes.Get(i)->AddApplication(client);
    client->SetStartTime(Seconds(0.3));
    client->SetStopTime(Seconds(99.9));

    clientApps.push_back(client);  // Store for global summary
  }

  std::cout << "Starting simulation with " << numClients << " client(s)..." << std::endl;

  Simulator::Stop(Seconds(100));
  Simulator::Run();
  std::cout << "Simulation completed successfully!" << std::endl;

  // Write global summary CSV if requested
  if (!globalSummaryCsv.empty()) {
    std::cout << "Writing global summary CSV..." << std::endl;

    // Aggregate statistics from all clients
    std::unordered_map<std::string, HttpClientApp::ContentStats> globalStats;

    for (const auto& client : clientApps) {
      const auto& clientStats = client->GetContentStats();
      for (const auto& pair : clientStats) {
        const std::string& content = pair.first;
        const HttpClientApp::ContentStats& stats = pair.second;

        auto& global = globalStats[content];
        global.totalRequests += stats.totalRequests;
        global.cacheHits += stats.cacheHits;
        global.cacheMisses += stats.cacheMisses;
        global.totalLatency += stats.totalLatency;
        global.totalHitLatency += stats.totalHitLatency;
        global.totalMissLatency += stats.totalMissLatency;
        global.minLatency = std::min(global.minLatency, stats.minLatency);
        global.maxLatency = std::max(global.maxLatency, stats.maxLatency);
      }
    }

    // Write global summary CSV
    std::ofstream globalSummary(globalSummaryCsv, std::ios::out);
    globalSummary << "content,total_requests,cache_hits,cache_misses,hit_rate_percent,avg_latency_ms,min_latency_ms,max_latency_ms,avg_hit_latency_ms,avg_miss_latency_ms\n";

    for (const auto& pair : globalStats) {
      const std::string& content = pair.first;
      const HttpClientApp::ContentStats& stats = pair.second;

      double hitRate = (stats.totalRequests > 0) ? (100.0 * stats.cacheHits / stats.totalRequests) : 0.0;
      double avgLatency = (stats.totalRequests > 0) ? (stats.totalLatency / stats.totalRequests) : 0.0;
      double avgHitLatency = (stats.cacheHits > 0) ? (stats.totalHitLatency / stats.cacheHits) : 0.0;
      double avgMissLatency = (stats.cacheMisses > 0) ? (stats.totalMissLatency / stats.cacheMisses) : 0.0;

      globalSummary << content << ","
                    << stats.totalRequests << ","
                    << stats.cacheHits << ","
                    << stats.cacheMisses << ","
                    << hitRate << ","
                    << avgLatency << ","
                    << stats.minLatency << ","
                    << stats.maxLatency << ","
                    << avgHitLatency << ","
                    << avgMissLatency << "\n";
    }

    globalSummary.close();
    std::cout << "Global summary written to: " << globalSummaryCsv << std::endl;
  }

  Simulator::Destroy();
  return 0;
}
