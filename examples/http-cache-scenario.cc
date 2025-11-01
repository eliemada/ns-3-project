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
  uint32_t nReq = 100; double interval=0.5; uint32_t cacheCap=4; double ttl=5.0;
  std::string resource = "/file-A"; std::string csv = "client_metrics.csv"; std::string summaryCsv = "";
  uint32_t numContent = 1; bool zipf = false; double zipfS = 1.0; uint32_t originDelay = 1; uint32_t cacheDelay = 1;
  uint32_t numClients = 1;
  CommandLine cmd;
  cmd.AddValue("nReq", "Total client requests", nReq);
  cmd.AddValue("interval", "Seconds between requests", interval);
  cmd.AddValue("cacheCap", "Cache capacity (entries)", cacheCap);
  cmd.AddValue("ttl", "TTL seconds", ttl);
  cmd.AddValue("resource", "Resource path (default if numContent==1)", resource);
  cmd.AddValue("csv", "Output CSV path", csv);
  cmd.AddValue("summaryCsv", "Summary statistics CSV path (optional)", summaryCsv);
  cmd.AddValue("numContent", "Number of distinct content items (1 = fixed resource)", numContent);
  cmd.AddValue("zipf", "Use Zipf popularity over resources", zipf);
  cmd.AddValue("zipfS", "Zipf exponent s (>0)", zipfS);
  cmd.AddValue("cacheDelay", "Cache processing delay for hits (ms)", cacheDelay);
  cmd.AddValue("originDelay", "Origin processing delay (ms)", originDelay);
  cmd.AddValue("numClients", "Number of concurrent clients", numClients);
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
  p2pClientCache.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2pClientCache.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper p2pCacheOrigin;
  p2pCacheOrigin.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
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
  originNode->AddApplication(origin);
  origin->SetStartTime(Seconds(0.1));
  origin->SetStopTime(Seconds(100));

  // Setup cache server
  Ptr<HttpCacheApp> cache = CreateObject<HttpCacheApp>();
  cache->SetListenPort(clientToCachePort);
  cache->SetOrigin(Address(cacheOriginInterfaces.GetAddress(1)), cacheToOriginPort);
  cache->SetTtl(Seconds(ttl));
  cache->SetCapacity(cacheCap);
  cache->SetCacheDelay(MilliSeconds(cacheDelay));
  cacheNode->AddApplication(cache);
  cache->SetStartTime(Seconds(0.2));
  cache->SetStopTime(Seconds(100));

  // Setup client applications
  for (uint32_t i = 0; i < numClients; ++i) {
    Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();
    client->SetRemote(Address(clientCacheInterfaces[i].GetAddress(1)), clientToCachePort);
    client->SetInterval(Seconds(interval));
    client->SetResource(resource);
    client->SetNumContent(numContent);
    client->SetZipf(zipf);
    client->SetZipfS(zipfS);
    client->SetTotalRequests(nReq);

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
  }

  std::cout << "Starting simulation with " << numClients << " client(s)..." << std::endl;

  Simulator::Stop(Seconds(100));
  Simulator::Run();
  std::cout << "Simulation completed successfully!" << std::endl;
  Simulator::Destroy();
  return 0;
}
