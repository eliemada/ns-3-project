/**
 * @file http-cache-video-streaming.cc
 * @brief Video streaming simulation scenario for HTTP cache in ns-3.
 *
 * This file implements a specialized HTTP caching simulation for video streaming
 * workloads. It models HTTP Adaptive Streaming (HAS) behavior where clients:
 * 1. Select a video service using Zipf distribution (popular videos get more views)
 * 2. Fetch segments sequentially from that service
 * 3. Repeat for the duration of the simulation
 *
 * @author Elie Bruno
 * @date 2025
 *
 * ## Video Streaming Model
 *
 * Unlike the standard scenario where each request is independent, this scenario
 * models realistic video consumption patterns:
 *
 * @code
 *   Client picks Service-3 (via Zipf) -> fetches seg-1, seg-2, ..., seg-N
 *   Client picks Service-1 (via Zipf) -> fetches seg-1, seg-2, ..., seg-N
 *   ...continues for totalTime seconds...
 * @endcode
 *
 * ## Features
 *
 * - **Streaming mode**: Sequential segment fetching within services
 * - **Zipf service popularity**: Realistic video popularity distribution
 * - **Dynamic TTL policy**: Fairness control for cache resources
 * - **Real-time progress tracking**: Monitor simulation status
 * - **Large-scale support**: Tested with 100,000+ concurrent clients
 *
 * ## Usage Examples
 *
 * @code
 *   # Basic streaming test
 *   ./ns3 run http-cache-video-streaming -- --numClients=100 --numServices=20 \
 *       --numSegments=10 --zipf=true
 *
 *   # With dynamic TTL policy
 *   ./ns3 run http-cache-video-streaming -- --numClients=1000 --dynamicTtl=true \
 *       --ttlThreshold=0.3 --serviceSummaryCsv=services.csv
 *
 *   # Large-scale simulation
 *   ./ns3 run http-cache-video-streaming -- --numClients=10000 --totalTime=300 \
 *       --progressInterval=5
 * @endcode
 *
 * @see HttpClientApp::SetStreaming for streaming mode implementation
 * @see HttpCacheApp::SetDynamicTtlEnabled for fairness policy
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
#include <iomanip>

using namespace ns3;

// ============================================================================
// Global Variables for Progress Tracking
// ============================================================================

/**
 * @brief Pointer to the cache application for statistics access.
 */
static Ptr<HttpCacheApp> g_cache;

/**
 * @brief Total simulation duration in seconds.
 */
static double g_totalTime;

/**
 * @brief Interval between progress updates in seconds.
 */
static double g_progressInterval;

/**
 * @brief Print simulation progress to console.
 *
 * Displays current simulation time, percentage complete, total requests,
 * and cache hit rate. Automatically schedules the next progress update.
 */
void PrintProgress() {
  double now = Simulator::Now().GetSeconds();
  double pct = (now / g_totalTime) * 100.0;
  uint64_t reqs = g_cache->GetTotalRequests();
  uint64_t hits = g_cache->GetTotalHits();
  double hitRate = (reqs > 0) ? (100.0 * hits / reqs) : 0.0;

  std::cout << "[" << std::fixed << std::setprecision(0) << pct << "%] "
            << std::setprecision(1) << now << "s/" << g_totalTime << "s | "
            << reqs << " requests | "
            << std::setprecision(1) << hitRate << "% hit rate"
            << std::endl;

  // Schedule next progress update if not at end
  double nextTime = now + g_progressInterval;
  if (nextTime <= g_totalTime) {
    Simulator::Schedule(Seconds(g_progressInterval), &PrintProgress);
  }
}

/**
 * @brief Main entry point for the video streaming simulation scenario.
 *
 * This function sets up and runs a video streaming HTTP caching simulation with:
 * - Configurable number of clients streaming content
 * - Zipf-distributed service selection (modeling video popularity)
 * - Sequential segment fetching within each service
 * - Optional dynamic TTL policy for cache fairness
 * - Real-time progress tracking for long-running simulations
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 * @return 0 on successful completion.
 */
int main(int argc, char** argv) {
  // Set simulation time resolution to nanoseconds
  Time::SetResolution(Time::NS);

  // ============================================================================
  // Default Configuration Parameters
  // ============================================================================

  // Scale and timing configuration
  uint32_t numClients = 1;          // Number of concurrent streaming clients
  double totalTime = 100.0;         // Total simulation duration (seconds)

  // Streaming configuration
  uint32_t numServices = 1;         // Number of video services (channels)
  uint32_t numSegments = 1;         // Segments to fetch per service selection
  double segmentInterval = 1.0;     // Time between segment requests (seconds)

  // Cache configuration
  double cacheCapacityGB = 1.0;     // Cache capacity in gigabytes
  uint32_t cacheCapacityObjs = 0;   // If >0, overrides cacheCapacityGB
  double ttl = 5.0;                 // Cache TTL in seconds

  // Content distribution
  bool zipf = false;                // Use Zipf distribution for services
  double zipfS = 1.0;               // Zipf skew parameter

  // Latency configuration (milliseconds)
  uint32_t originDelay = 1;         // Origin server processing delay
  uint32_t cacheDelay = 1;          // Cache hit processing delay

  // Object and network configuration
  uint32_t objectSize = 1024;       // Object size in bytes
  uint32_t clientCacheBw = 100;     // Client-Cache bandwidth (Mbps)
  uint32_t cacheOriginBw = 50;      // Cache-Origin bandwidth (Mbps)

  // Dynamic TTL policy configuration
  bool dynamicTtl = false;          // Enable dynamic TTL policy
  double ttlWindow = 300.0;         // Sliding window for request tracking (seconds)
  double ttlThreshold = 0.5;        // Request share threshold for penalty
  double ttlReduction = 0.5;        // TTL reduction factor when penalized
  double ttlEvalInterval = 30.0;    // Policy evaluation interval (seconds)

  // Progress tracking
  double progressInterval = 10.0;   // Progress update interval (percentage)

  // Output file paths
  std::string csv = "";             // Per-request CSV (disabled by default for performance)
  std::string summaryCsv = "";      // Per-content summary
  std::string globalSummaryCsv = ""; // Global aggregated summary
  std::string serviceSummaryCsv = ""; // Service-level aggregation

  // ============================================================================
  // Command Line Argument Parsing
  // ============================================================================

  CommandLine cmd;

  // Scale parameters
  cmd.AddValue("numClients", "Number of concurrent clients", numClients);
  cmd.AddValue("totalTime", "Total simulation time (seconds)", totalTime);

  // Streaming parameters
  cmd.AddValue("numServices", "Number of streaming services (Zipf pick among these)", numServices);
  cmd.AddValue("numSegments", "Number of sequential segments per selection", numSegments);
  cmd.AddValue("segmentInterval", "Seconds between sequential segments", segmentInterval);

  // Cache parameters
  cmd.AddValue("cacheCapacityGB", "Cache capacity in gigabytes", cacheCapacityGB);
  cmd.AddValue("cacheCapacityObjs", "Cache capacity in number of objects (overrides cacheCapacityGB)", cacheCapacityObjs);
  cmd.AddValue("ttl", "TTL seconds", ttl);

  // Content distribution
  cmd.AddValue("zipf", "Use Zipf popularity over services", zipf);
  cmd.AddValue("zipfS", "Zipf exponent s (>0)", zipfS);

  // Delay parameters
  cmd.AddValue("cacheDelay", "Cache processing delay for hits (ms)", cacheDelay);
  cmd.AddValue("originDelay", "Origin processing delay (ms)", originDelay);

  // Object and network parameters
  cmd.AddValue("objectSize", "Object size in bytes (default 1024)", objectSize);
  cmd.AddValue("clientCacheBw", "Client-Cache link bandwidth (Mbps)", clientCacheBw);
  cmd.AddValue("cacheOriginBw", "Cache-Origin link bandwidth (Mbps)", cacheOriginBw);

  // Dynamic TTL policy parameters
  cmd.AddValue("dynamicTtl", "Enable dynamic TTL policy", dynamicTtl);
  cmd.AddValue("ttlWindow", "Dynamic TTL sliding window (seconds)", ttlWindow);
  cmd.AddValue("ttlThreshold", "Request share threshold for TTL reduction (0.0-1.0)", ttlThreshold);
  cmd.AddValue("ttlReduction", "TTL reduction factor when penalized (0.0-1.0)", ttlReduction);
  cmd.AddValue("ttlEvalInterval", "Policy evaluation interval (seconds)", ttlEvalInterval);

  // Progress tracking
  cmd.AddValue("progressInterval", "Progress update interval (percentage, default 10)", progressInterval);

  // Output parameters
  cmd.AddValue("csv", "Per-client CSV path (optional, disabled by default)", csv);
  cmd.AddValue("summaryCsv", "Summary statistics CSV path (optional)", summaryCsv);
  cmd.AddValue("globalSummaryCsv", "Global aggregated summary CSV path (optional)", globalSummaryCsv);
  cmd.AddValue("serviceSummaryCsv", "Service-level aggregated summary CSV path (optional)", serviceSummaryCsv);

  cmd.Parse(argc, argv);

  // ============================================================================
  // Setup Progress Tracking for Large Simulations
  // ============================================================================

  // Enable verbose setup messages for large-scale simulations
  bool verboseSetup = (numClients >= 1000);
  uint32_t progressStep = std::max(1u, numClients / 10);

  // ============================================================================
  // Node Creation
  // ============================================================================

  if (verboseSetup) std::cout << "Setup: Creating " << numClients << " client nodes..." << std::flush;

  NodeContainer clientNodes;
  clientNodes.Create(numClients);

  NodeContainer serverNodes;
  serverNodes.Create(2);  // cache and origin

  NodeContainer allNodes;
  allNodes.Add(clientNodes);
  allNodes.Add(serverNodes);

  Ptr<Node> cacheNode = serverNodes.Get(0);
  Ptr<Node> originNode = serverNodes.Get(1);

  if (verboseSetup) std::cout << " done" << std::endl;

  // ============================================================================
  // Internet Stack Installation
  // ============================================================================

  if (verboseSetup) std::cout << "Setup: Installing internet stack..." << std::flush;
  InternetStackHelper internet;
  internet.Install(allNodes);
  if (verboseSetup) std::cout << " done" << std::endl;

  // ============================================================================
  // Network Topology Setup
  // ============================================================================

  // Configure client-to-cache links
  PointToPointHelper p2pClientCache;
  std::ostringstream clientCacheBwStr;
  clientCacheBwStr << clientCacheBw << "Mbps";
  p2pClientCache.SetDeviceAttribute("DataRate", StringValue(clientCacheBwStr.str()));
  p2pClientCache.SetChannelAttribute("Delay", StringValue("2ms"));

  // Configure cache-to-origin link
  PointToPointHelper p2pCacheOrigin;
  std::ostringstream cacheOriginBwStr;
  cacheOriginBwStr << cacheOriginBw << "Mbps";
  p2pCacheOrigin.SetDeviceAttribute("DataRate", StringValue(cacheOriginBwStr.str()));
  p2pCacheOrigin.SetChannelAttribute("Delay", StringValue("5ms"));

  // Create client-cache links with unique subnets
  std::vector<NetDeviceContainer> clientCacheDevices(numClients);
  std::vector<Ipv4InterfaceContainer> clientCacheInterfaces(numClients);
  Ipv4AddressHelper ip;

  if (verboseSetup) std::cout << "Setup: Creating " << numClients << " client-cache links..." << std::flush;

  for (uint32_t i = 0; i < numClients; ++i) {
    clientCacheDevices[i] = p2pClientCache.Install(clientNodes.Get(i), cacheNode);

    // Generate unique subnet for each client
    std::ostringstream subnet;
    subnet << "10." << (i / 256) << "." << (i % 256) << ".0";
    ip.SetBase(subnet.str().c_str(), "255.255.255.0");
    clientCacheInterfaces[i] = ip.Assign(clientCacheDevices[i]);

    // Show progress for large-scale setups
    if (verboseSetup && (i + 1) % progressStep == 0) {
      std::cout << " " << ((i + 1) * 100 / numClients) << "%" << std::flush;
    }
  }

  if (verboseSetup) std::cout << " done" << std::endl;

  // Create cache-to-origin link
  NetDeviceContainer cacheOriginDevices = p2pCacheOrigin.Install(cacheNode, originNode);
  ip.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer cacheOriginInterfaces = ip.Assign(cacheOriginDevices);

  // Build routing tables
  if (verboseSetup) std::cout << "Setup: Building routing tables..." << std::flush;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  if (verboseSetup) std::cout << " done" << std::endl;

  // ============================================================================
  // Port Configuration
  // ============================================================================

  uint16_t clientToCachePort = 8080;
  uint16_t cacheToOriginPort = 8081;

  // ============================================================================
  // Origin Server Setup
  // ============================================================================

  Ptr<HttpOriginApp> origin = CreateObject<HttpOriginApp>();
  origin->SetListenPort(cacheToOriginPort);
  origin->SetServiceDelay(MilliSeconds(originDelay));
  origin->SetObjectSize(objectSize);
  originNode->AddApplication(origin);
  origin->SetStartTime(Seconds(0.1));
  origin->SetStopTime(Seconds(totalTime + 1.0));

  // ============================================================================
  // Cache Capacity Calculation
  // ============================================================================

  uint64_t capacityBytes = 0;
  uint32_t maxObjects = 0;

  if (cacheCapacityObjs > 0) {
    maxObjects = cacheCapacityObjs;
    capacityBytes = static_cast<uint64_t>(maxObjects) * objectSize;
  } else {
    capacityBytes = static_cast<uint64_t>(cacheCapacityGB * 1024 * 1024 * 1024);
    maxObjects = static_cast<uint32_t>(capacityBytes / objectSize);
  }

  // Print cache configuration
  std::cout << "Cache configuration:" << std::endl;
  if (cacheCapacityObjs > 0) {
    std::cout << "  Capacity: " << maxObjects << " objects (" << capacityBytes << " bytes)" << std::endl;
  } else {
    std::cout << "  Capacity: " << cacheCapacityGB << " GB (" << capacityBytes << " bytes)" << std::endl;
  }
  std::cout << "  Object size: " << objectSize << " bytes" << std::endl;
  std::cout << "  Max objects: " << maxObjects << std::endl;

  // Print dynamic TTL configuration if enabled
  if (dynamicTtl) {
    std::cout << "  Dynamic TTL: enabled" << std::endl;
    std::cout << "    Window: " << ttlWindow << "s" << std::endl;
    std::cout << "    Threshold: " << (ttlThreshold * 100) << "%" << std::endl;
    std::cout << "    Reduction: " << (ttlReduction * 100) << "%" << std::endl;
    std::cout << "    Eval interval: " << ttlEvalInterval << "s" << std::endl;
  }

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

  // Configure dynamic TTL policy
  cache->SetDynamicTtlEnabled(dynamicTtl);
  cache->SetTtlWindow(Seconds(ttlWindow));
  cache->SetTtlThreshold(ttlThreshold);
  cache->SetTtlReduction(ttlReduction);
  cache->SetTtlEvalInterval(Seconds(ttlEvalInterval));

  cacheNode->AddApplication(cache);
  cache->SetStartTime(Seconds(0.2));
  cache->SetStopTime(Seconds(totalTime + 1.0));

  // ============================================================================
  // Client Applications Setup (Streaming Mode)
  // ============================================================================

  if (verboseSetup) std::cout << "Setup: Creating " << numClients << " client applications..." << std::flush;

  std::vector<Ptr<HttpClientApp>> clientApps;

  for (uint32_t i = 0; i < numClients; ++i) {
    Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();

    // Network configuration
    client->SetRemote(Address(clientCacheInterfaces[i].GetAddress(1)), clientToCachePort);
    client->SetObjectSize(objectSize);

    // Streaming-specific configuration
    client->SetNumServices(numServices);
    client->SetNumSegments(numSegments);
    client->SetSegmentInterval(Seconds(segmentInterval));
    client->SetZipf(zipf);
    client->SetZipfS(zipfS);
    client->SetStreaming(true);  // Enable streaming mode
    client->SetTotalTime(Seconds(totalTime));

    // CSV output paths (with client index for multi-client scenarios)
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
    client->SetStartTime(Seconds(0.3));
    client->SetStopTime(Seconds(totalTime + 1.0));
    clientApps.push_back(client);

    // Show progress for large-scale setups
    if (verboseSetup && (i + 1) % progressStep == 0) {
      std::cout << " " << ((i + 1) * 100 / numClients) << "%" << std::flush;
    }
  }

  if (verboseSetup) std::cout << " done" << std::endl;

  // ============================================================================
  // Simulation Execution
  // ============================================================================

  std::cout << "Starting streaming simulation with " << numClients
            << " client(s) for " << totalTime << "s..." << std::endl;

  // Configure and start progress tracking
  g_cache = cache;
  g_totalTime = totalTime;
  g_progressInterval = (progressInterval / 100.0) * totalTime;

  if (g_progressInterval > 0) {
    Simulator::Schedule(Seconds(g_progressInterval), &PrintProgress);
  }

  Simulator::Stop(Seconds(totalTime + 1.0));
  Simulator::Run();

  std::cout << "Simulation completed successfully!" << std::endl;

  // ============================================================================
  // Global Summary Generation
  // ============================================================================

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

    // Write global summary to CSV
    std::ofstream globalSummary(globalSummaryCsv, std::ios::out);
    globalSummary << "content,total_requests,cache_hits,cache_misses,hit_rate_percent,"
                  << "avg_latency_ms,min_latency_ms,max_latency_ms,"
                  << "avg_hit_latency_ms,avg_miss_latency_ms\n";

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
