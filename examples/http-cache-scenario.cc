/**
 * @file http-cache-scenario.cc
 * @brief Standard HTTP cache simulation scenario for ns-3.
 *
 * This file implements a complete HTTP caching simulation with configurable
 * clients, cache server, and origin server. It demonstrates the HTTP cache
 * module's capabilities and provides a framework for cache performance testing.
 *
 * @author Elie Bruno
 * @date 2025
 *
 * ## Network Topology
 *
 * @code
 *   [Client 1] ──┐
 *   [Client 2] ──┼── [Cache Server] ── [Origin Server]
 *   [Client N] ──┘
 *        │                  │                  │
 *   configurable       port 8080          port 8081
 *     bandwidth
 * @endcode
 *
 * ## Features
 *
 * - Configurable number of concurrent clients
 * - Zipf popularity distribution for realistic workload modeling
 * - Configurable cache capacity (by size in GB or number of objects)
 * - Configurable network bandwidth and delays
 * - Multiple output formats: per-request CSV, per-content summary, global aggregation
 *
 * ## Usage Examples
 *
 * @code
 *   # Basic test with 100 requests
 *   ./ns3 run http-cache-scenario -- --nReq=100 --numContent=10 --zipf=true
 *
 *   # Large-scale test with 1000 clients
 *   ./ns3 run http-cache-scenario -- --numClients=1000 --nReq=10 \
 *       --globalSummaryCsv=results.csv
 *
 *   # Custom bandwidth configuration
 *   ./ns3 run http-cache-scenario -- --clientCacheBw=100 --cacheOriginBw=50
 * @endcode
 *
 * @see HttpClientApp for client implementation details
 * @see HttpCacheApp for cache server implementation details
 * @see HttpOriginApp for origin server implementation details
 */

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

/**
 * @brief Main entry point for the HTTP cache simulation scenario.
 *
 * This function sets up and runs a complete HTTP caching simulation with:
 * - Configurable number of clients sending requests
 * - A cache server with LRU eviction and TTL expiration
 * - An origin server for cache misses
 * - Comprehensive metrics collection and CSV output
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 * @return 0 on successful completion.
 */
int main(int argc, char** argv) {
  // Set simulation time resolution to nanoseconds for precise timing
  Time::SetResolution(Time::NS);

  // ============================================================================
  // Default Configuration Parameters
  // ============================================================================

  // Request generation parameters
  uint32_t nReq = 100;            // Total requests per client
  double interval = 0.5;          // Seconds between requests

  // Cache configuration
  double cacheCapacityGB = 1.0;   // Cache capacity in gigabytes
  uint32_t cacheCapacityObjs = 0; // If >0, overrides cacheCapacityGB
  double ttl = 5.0;               // Cache TTL in seconds

  // Content configuration
  std::string resource = "/file-A";  // Default resource path
  uint32_t numContent = 1;           // Number of distinct content items
  bool zipf = false;                 // Use Zipf popularity distribution
  double zipfS = 1.0;                // Zipf skew parameter

  // Latency configuration (milliseconds)
  uint32_t originDelay = 1;       // Origin server processing delay
  uint32_t cacheDelay = 1;        // Cache hit processing delay

  // Scale configuration
  uint32_t numClients = 1;        // Number of concurrent clients

  // Object size configuration
  uint32_t objectSize = 1024;     // Object size in bytes (default 1 KB)

  // Network bandwidth configuration (Mbps)
  uint32_t clientCacheBw = 1000000;  // Client-Cache link bandwidth
  uint32_t cacheOriginBw = 1000000;  // Cache-Origin link bandwidth

  // Output file paths
  std::string csv = "client_metrics.csv";  // Per-request metrics
  std::string summaryCsv = "";             // Per-content summary
  std::string globalSummaryCsv = "";       // Global aggregated summary
  std::string serviceSummaryCsv = "";      // Service-level aggregation

  // ============================================================================
  // Command Line Argument Parsing
  // ============================================================================

  CommandLine cmd;

  // Request parameters
  cmd.AddValue("nReq", "Total client requests", nReq);
  cmd.AddValue("interval", "Seconds between requests", interval);

  // Cache parameters
  cmd.AddValue("cacheCapacityGB", "Cache capacity in gigabytes", cacheCapacityGB);
  cmd.AddValue("cacheCapacityObjs", "Cache capacity in number of objects (overrides cacheCapacityGB)", cacheCapacityObjs);
  cmd.AddValue("ttl", "TTL seconds", ttl);

  // Content parameters
  cmd.AddValue("resource", "Resource path (default if numContent==1)", resource);
  cmd.AddValue("numContent", "Number of distinct content items (1 = fixed resource)", numContent);
  cmd.AddValue("zipf", "Use Zipf popularity over resources", zipf);
  cmd.AddValue("zipfS", "Zipf exponent s (>0)", zipfS);

  // Delay parameters
  cmd.AddValue("cacheDelay", "Cache processing delay for hits (ms)", cacheDelay);
  cmd.AddValue("originDelay", "Origin processing delay (ms)", originDelay);

  // Scale parameters
  cmd.AddValue("numClients", "Number of concurrent clients", numClients);
  cmd.AddValue("objectSize", "Object size in bytes (default 1024)", objectSize);

  // Network parameters
  cmd.AddValue("clientCacheBw", "Client-Cache link bandwidth (Mbps)", clientCacheBw);
  cmd.AddValue("cacheOriginBw", "Cache-Origin link bandwidth (Mbps)", cacheOriginBw);

  // Output parameters
  cmd.AddValue("csv", "Output CSV path", csv);
  cmd.AddValue("summaryCsv", "Summary statistics CSV path (optional)", summaryCsv);
  cmd.AddValue("globalSummaryCsv", "Global aggregated summary CSV path (optional)", globalSummaryCsv);
  cmd.AddValue("serviceSummaryCsv", "Service-level aggregated summary CSV path (optional)", serviceSummaryCsv);

  cmd.Parse(argc, argv);

  // ============================================================================
  // Node Creation
  // ============================================================================

  // Create client nodes (one per simulated client)
  NodeContainer clientNodes;
  clientNodes.Create(numClients);

  // Create server nodes: cache (index 0) and origin (index 1)
  NodeContainer serverNodes;
  serverNodes.Create(2);

  // Combine all nodes for internet stack installation
  NodeContainer allNodes;
  allNodes.Add(clientNodes);
  allNodes.Add(serverNodes);

  // Install IP stack on all nodes
  InternetStackHelper internet;
  internet.Install(allNodes);

  // ============================================================================
  // Network Topology Setup
  // ============================================================================

  // Configure client-to-cache links
  PointToPointHelper p2pClientCache;
  std::ostringstream clientCacheBwStr;
  clientCacheBwStr << clientCacheBw << "Mbps";
  p2pClientCache.SetDeviceAttribute("DataRate", StringValue(clientCacheBwStr.str()));
  p2pClientCache.SetChannelAttribute("Delay", StringValue("2ms"));

  // Configure cache-to-origin link (typically lower bandwidth, higher latency)
  PointToPointHelper p2pCacheOrigin;
  std::ostringstream cacheOriginBwStr;
  cacheOriginBwStr << cacheOriginBw << "Mbps";
  p2pCacheOrigin.SetDeviceAttribute("DataRate", StringValue(cacheOriginBwStr.str()));
  p2pCacheOrigin.SetChannelAttribute("Delay", StringValue("5ms"));

  // Create point-to-point links from each client to the cache server
  std::vector<NetDeviceContainer> clientCacheDevices(numClients);
  std::vector<Ipv4InterfaceContainer> clientCacheInterfaces(numClients);

  Ipv4AddressHelper ip;
  Ptr<Node> cacheNode = serverNodes.Get(0);
  Ptr<Node> originNode = serverNodes.Get(1);

  // Assign unique subnets to each client-cache link
  for (uint32_t i = 0; i < numClients; ++i) {
    clientCacheDevices[i] = p2pClientCache.Install(clientNodes.Get(i), cacheNode);

    // Generate unique subnet: 10.X.Y.0/24 where X.Y encodes client index
    std::ostringstream subnet;
    subnet << "10." << (i / 256) << "." << (i % 256) << ".0";
    ip.SetBase(subnet.str().c_str(), "255.255.255.0");
    clientCacheInterfaces[i] = ip.Assign(clientCacheDevices[i]);
  }

  // Create and configure cache-to-origin link
  NetDeviceContainer cacheOriginDevices = p2pCacheOrigin.Install(cacheNode, originNode);
  ip.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer cacheOriginInterfaces = ip.Assign(cacheOriginDevices);

  // Build routing tables for multi-hop communication
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // ============================================================================
  // Port Configuration
  // ============================================================================

  uint16_t clientToCachePort = 8080;  // Clients send requests here
  uint16_t cacheToOriginPort = 8081;  // Cache forwards misses here

  // ============================================================================
  // Origin Server Setup
  // ============================================================================

  Ptr<HttpOriginApp> origin = CreateObject<HttpOriginApp>();
  origin->SetListenPort(cacheToOriginPort);
  origin->SetServiceDelay(MilliSeconds(originDelay));
  origin->SetObjectSize(objectSize);
  originNode->AddApplication(origin);
  origin->SetStartTime(Seconds(0.1));  // Start early to be ready for requests
  origin->SetStopTime(Seconds(100));

  // ============================================================================
  // Cache Capacity Calculation
  // ============================================================================

  // Calculate cache capacity in number of objects
  uint64_t capacityBytes = 0;
  uint32_t maxObjects = 0;

  if (cacheCapacityObjs > 0) {
    // User specified capacity in number of objects
    maxObjects = cacheCapacityObjs;
    capacityBytes = static_cast<uint64_t>(maxObjects) * objectSize;
  } else {
    // Convert GB to number of objects based on object size
    capacityBytes = static_cast<uint64_t>(cacheCapacityGB * 1024 * 1024 * 1024);
    maxObjects = static_cast<uint32_t>(capacityBytes / objectSize);
  }

  // Print cache configuration for verification
  std::cout << "Cache configuration:" << std::endl;
  if (cacheCapacityObjs > 0) {
    std::cout << "  Capacity: " << maxObjects << " objects (" << capacityBytes << " bytes)" << std::endl;
  } else {
    std::cout << "  Capacity: " << cacheCapacityGB << " GB (" << capacityBytes << " bytes)" << std::endl;
  }
  std::cout << "  Object size: " << objectSize << " bytes" << std::endl;
  std::cout << "  Max objects: " << maxObjects << std::endl;

  // ============================================================================
  // Cache Server Setup
  // ============================================================================

  Ptr<HttpCacheApp> cache = CreateObject<HttpCacheApp>();
  cache->SetListenPort(clientToCachePort);
  cache->SetOrigin(Address(cacheOriginInterfaces.GetAddress(1)), cacheToOriginPort);
  cache->SetTtl(Seconds(ttl));
  cache->SetCapacity(maxObjects);
  cache->SetCacheDelay(MilliSeconds(cacheDelay));
  cache->SetObjectSize(objectSize);
  cacheNode->AddApplication(cache);
  cache->SetStartTime(Seconds(0.2));  // Start after origin is ready
  cache->SetStopTime(Seconds(100));

  // ============================================================================
  // Client Applications Setup
  // ============================================================================

  std::vector<Ptr<HttpClientApp>> clientApps;  // Store references for global summary

  for (uint32_t i = 0; i < numClients; ++i) {
    Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();

    // Network configuration
    client->SetRemote(Address(clientCacheInterfaces[i].GetAddress(1)), clientToCachePort);

    // Request configuration
    client->SetInterval(Seconds(interval));
    client->SetResource(resource);
    client->SetNumContent(numContent);
    client->SetZipf(zipf);
    client->SetZipfS(zipfS);
    client->SetTotalRequests(nReq);
    client->SetObjectSize(objectSize);

    // Per-request CSV output (append client index for multi-client scenarios)
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

    // Per-content summary CSV output
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

    // Install and schedule application
    clientNodes.Get(i)->AddApplication(client);
    client->SetStartTime(Seconds(0.3));  // Start after cache is ready
    client->SetStopTime(Seconds(99.9));  // Stop before simulation ends

    clientApps.push_back(client);
  }

  // ============================================================================
  // Simulation Execution
  // ============================================================================

  std::cout << "Starting simulation with " << numClients << " client(s)..." << std::endl;

  Simulator::Stop(Seconds(100));
  Simulator::Run();

  std::cout << "Simulation completed successfully!" << std::endl;

  // ============================================================================
  // Global Summary Generation
  // ============================================================================

  if (!globalSummaryCsv.empty()) {
    std::cout << "Writing global summary CSV..." << std::endl;

    // Aggregate statistics from all clients into a single summary
    std::unordered_map<std::string, HttpClientApp::ContentStats> globalStats;

    for (const auto& client : clientApps) {
      const auto& clientStats = client->GetContentStats();
      for (const auto& pair : clientStats) {
        const std::string& content = pair.first;
        const HttpClientApp::ContentStats& stats = pair.second;

        // Accumulate per-content statistics
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

    // Write global summary to CSV
    std::ofstream globalSummary(globalSummaryCsv, std::ios::out);
    globalSummary << "content,total_requests,cache_hits,cache_misses,hit_rate_percent,"
                  << "avg_latency_ms,min_latency_ms,max_latency_ms,"
                  << "avg_hit_latency_ms,avg_miss_latency_ms\n";

    for (const auto& pair : globalStats) {
      const std::string& content = pair.first;
      const HttpClientApp::ContentStats& stats = pair.second;

      // Calculate derived metrics
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

    // ========================================================================
    // Service-Level Aggregation (Optional)
    // ========================================================================

    if (!serviceSummaryCsv.empty()) {
      std::cout << "Writing service-level summary CSV..." << std::endl;

      // Aggregate by service (e.g., "/service-2/seg-1" -> "service-2")
      std::unordered_map<std::string, HttpClientApp::ContentStats> serviceStats;

      for (const auto& pair : globalStats) {
        const std::string& content = pair.first;
        const HttpClientApp::ContentStats& stats = pair.second;

        // Extract service key from content path
        std::string serviceKey;
        size_t lastSlash = content.find_last_of('/');
        if (lastSlash != std::string::npos) {
          if (!content.empty() && content[0] == '/') {
            if (lastSlash > 1) serviceKey = content.substr(1, lastSlash - 1);
            else serviceKey = content.substr(1);
          } else {
            if (lastSlash > 0) serviceKey = content.substr(0, lastSlash);
            else serviceKey = content;
          }
        } else {
          if (!content.empty() && content[0] == '/') serviceKey = content.substr(1);
          else serviceKey = content;
        }

        // Aggregate into service-level statistics
        auto& agg = serviceStats[serviceKey];
        agg.totalRequests += stats.totalRequests;
        agg.cacheHits += stats.cacheHits;
        agg.cacheMisses += stats.cacheMisses;
        agg.totalLatency += stats.totalLatency;
        agg.totalHitLatency += stats.totalHitLatency;
        agg.totalMissLatency += stats.totalMissLatency;
        agg.minLatency = std::min(agg.minLatency, stats.minLatency);
        agg.maxLatency = std::max(agg.maxLatency, stats.maxLatency);
      }

      // Write service-level summary to CSV
      std::ofstream serviceSummary(serviceSummaryCsv, std::ios::out);
      serviceSummary << "service,total_requests,cache_hits,cache_misses,hit_rate_percent,"
                     << "avg_latency_ms,min_latency_ms,max_latency_ms,"
                     << "avg_hit_latency_ms,avg_miss_latency_ms\n";

      for (const auto& pair : serviceStats) {
        const std::string& service = pair.first;
        const HttpClientApp::ContentStats& stats = pair.second;

        double hitRate = (stats.totalRequests > 0) ? (100.0 * stats.cacheHits / stats.totalRequests) : 0.0;
        double avgLatency = (stats.totalRequests > 0) ? (stats.totalLatency / stats.totalRequests) : 0.0;
        double avgHitLatency = (stats.cacheHits > 0) ? (stats.totalHitLatency / stats.cacheHits) : 0.0;
        double avgMissLatency = (stats.cacheMisses > 0) ? (stats.totalMissLatency / stats.cacheMisses) : 0.0;

        serviceSummary << service << ","
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

      serviceSummary.close();
      std::cout << "Service-level summary written to: " << serviceSummaryCsv << std::endl;
    }
  }

  // Clean up simulation resources
  Simulator::Destroy();

  return 0;
}
