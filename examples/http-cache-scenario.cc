#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/http-client-app.h"
#include "ns3/http-cache-app.h"
#include "ns3/http-origin-app.h"

using namespace ns3;

int main(int argc, char** argv){
  Time::SetResolution(Time::NS);
  uint32_t nReq = 100; double interval=0.5; uint32_t cacheCap=4; double ttl=5.0;
  std::string resource = "/file-A"; std::string csv = "client_metrics.csv";
  uint32_t numContent = 1; bool zipf = false; double zipfS = 1.0; uint32_t originDelay = 1; uint32_t cacheDelay = 1;
  CommandLine cmd;
  cmd.AddValue("nReq", "Total client requests", nReq);
  cmd.AddValue("interval", "Seconds between requests", interval);
  cmd.AddValue("cacheCap", "Cache capacity (entries)", cacheCap);
  cmd.AddValue("ttl", "TTL seconds", ttl);
  cmd.AddValue("resource", "Resource path (default if numContent==1)", resource);
  cmd.AddValue("csv", "Output CSV path", csv);
  cmd.AddValue("numContent", "Number of distinct content items (1 = fixed resource)", numContent);
  cmd.AddValue("zipf", "Use Zipf popularity over resources", zipf);
  cmd.AddValue("zipfS", "Zipf exponent s (>0)", zipfS);
  cmd.AddValue("cacheDelay", "Cache processing delay for hits (ms)", cacheDelay);
  cmd.AddValue("originDelay", "Origin processing delay (ms)", originDelay);
  cmd.Parse(argc, argv);

  NodeContainer nodes; nodes.Create(3);
  InternetStackHelper internet; internet.Install(nodes);

  PointToPointHelper p2p1; p2p1.SetDeviceAttribute("DataRate", StringValue("100Mbps")); p2p1.SetChannelAttribute("Delay", StringValue("2ms"));
  PointToPointHelper p2p2; p2p2.SetDeviceAttribute("DataRate", StringValue("50Mbps")); p2p2.SetChannelAttribute("Delay", StringValue("5ms"));
  NetDeviceContainer d01 = p2p1.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d12 = p2p2.Install(nodes.Get(1), nodes.Get(2));

  Ipv4AddressHelper ip;
  ip.SetBase("10.0.1.0", "255.255.255.0"); Ipv4InterfaceContainer if01 = ip.Assign(d01);
  ip.SetBase("10.0.2.0", "255.255.255.0"); Ipv4InterfaceContainer if12 = ip.Assign(d12);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t clientToCachePort = 8080; uint16_t cacheToOriginPort = 8081;

  Ptr<HttpOriginApp> origin = CreateObject<HttpOriginApp>();
  origin->SetListenPort(cacheToOriginPort);
  origin->SetServiceDelay(MilliSeconds(originDelay));
  nodes.Get(2)->AddApplication(origin);
  origin->SetStartTime(Seconds(0.1)); origin->SetStopTime(Seconds(100));

  Ptr<HttpCacheApp> cache = CreateObject<HttpCacheApp>();
  cache->SetListenPort(clientToCachePort);
  cache->SetOrigin(Address(if12.GetAddress(1)), cacheToOriginPort);
  cache->SetTtl(Seconds(ttl));
  cache->SetCapacity(cacheCap);
  cache->SetCacheDelay(MilliSeconds(cacheDelay));
  nodes.Get(1)->AddApplication(cache);
  cache->SetStartTime(Seconds(0.2)); cache->SetStopTime(Seconds(100));

  Ptr<HttpClientApp> client = CreateObject<HttpClientApp>();
  client->SetRemote(Address(if01.GetAddress(1)), clientToCachePort);
  client->SetInterval(Seconds(interval));
  client->SetResource(resource);
  client->SetNumContent(numContent);
  client->SetZipf(zipf);
  client->SetZipfS(zipfS);
  client->SetCsvPath(csv);
  client->SetTotalRequests(nReq);
  nodes.Get(0)->AddApplication(client);
  client->SetStartTime(Seconds(0.3)); client->SetStopTime(Seconds(100));

  Simulator::Stop(Seconds(100));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
