# Performance Optimization Proposal: Large-Scale HTTP Cache Simulations

## Executive Summary

Our ns-3 HTTP cache simulation experiences significant slowdowns with large client counts. This document analyzes the root cause and proposes two optimization approaches that could improve setup time by 10-50x.

## Problem Statement

### Observed Performance

| Clients | Total Time | Setup Phase | Simulation Phase |
|---------|------------|-------------|------------------|
| 1,000   | ~3s        | ~2s         | ~1s              |
| 5,000   | ~2min      | ~90s        | ~30s             |
| 10,000  | >3.5min    | (mostly setup) | -             |

**Key Finding**: The setup phase dominates execution time, not the actual simulation.

### Current Architecture

```
                    ┌─────────────────────────────────────────┐
                    │           Current Topology              │
                    └─────────────────────────────────────────┘

Client-1  ═══[P2P Link 1]═══╗
Client-2  ═══[P2P Link 2]═══╬═══ Cache ═══[P2P]═══ Origin
Client-3  ═══[P2P Link 3]═══╣
   ...           ...        ║
Client-N  ═══[P2P Link N]═══╝

Each P2P link requires:
- 2 NetDevice objects
- 1 Channel object
- 1 IP subnet (10.x.y.0/24)
- Queue objects, error models, etc.
```

### Root Cause Analysis

**1. O(n) Link Creation with High Constant Factor**

For each of n clients, we execute:
```cpp
clientCacheDevices[i] = p2pClientCache.Install(clientNodes.Get(i), cacheNode);
ip.SetBase(subnet.str().c_str(), "255.255.255.0");
clientCacheInterfaces[i] = ip.Assign(clientCacheDevices[i]);
```

Each `Install()` call creates multiple objects and performs validation.

**2. O(n²) Routing Table Computation**

```cpp
Ipv4GlobalRoutingHelper::PopulateRoutingTables();
```

This runs Dijkstra's algorithm from every node to every other node. With 10,000 clients + 2 servers = 10,002 nodes, this becomes the dominant bottleneck.

**3. Memory Allocation Overhead**

Each P2P link allocates:
- ~2KB for NetDevice objects
- ~1KB for Channel
- ~500B for IP interface

For 10,000 clients: ~35MB just for link infrastructure.

---

## Proposed Solutions

### Approach 1: CSMA Bus Topology (Recommended)

**Concept**: Replace individual P2P links with a single shared CSMA (Carrier Sense Multiple Access) bus.

```
                    ┌─────────────────────────────────────────┐
                    │           CSMA Bus Topology             │
                    └─────────────────────────────────────────┘

Client-1  ───┐
Client-2  ───┼───[Shared CSMA Bus]─── Cache ═══[P2P]═══ Origin
Client-3  ───┤
   ...       │
Client-N  ───┘

All clients share ONE broadcast segment
```

**Implementation Changes**:

```cpp
// BEFORE: Individual P2P links
PointToPointHelper p2pClientCache;
for (uint32_t i = 0; i < numClients; ++i) {
    clientCacheDevices[i] = p2pClientCache.Install(clientNodes.Get(i), cacheNode);
    // ... per-client IP setup
}

// AFTER: Single CSMA bus
CsmaHelper csma;
csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

NodeContainer csmaNodes;
csmaNodes.Add(clientNodes);
csmaNodes.Add(cacheNode);

NetDeviceContainer csmaDevices = csma.Install(csmaNodes);
ip.SetBase("10.0.0.0", "255.0.0.0");  // Single large subnet
Ipv4InterfaceContainer csmaInterfaces = ip.Assign(csmaDevices);
```

**Expected Benefits**:

| Metric | Before (P2P) | After (CSMA) | Improvement |
|--------|--------------|--------------|-------------|
| Link objects | n | 1 | O(n) → O(1) |
| IP subnets | n | 1 | O(n) → O(1) |
| Routing complexity | O(n²) | O(n) | Significant |
| Setup time (10k) | >3.5min | ~20-30s | ~10x |

**Trade-offs**:
- CSMA is a broadcast medium (all nodes see all traffic) - slightly more realistic for a LAN scenario
- Collision detection adds small overhead during simulation (negligible for our request-response pattern)
- Bandwidth is shared (but we can set it high enough, e.g., 1Gbps)

**Simulation Validity**: The cache behavior (hit/miss, latency) remains identical. Only the network layer changes.

---

### Approach 2: Static Routing (Skip Global Routing)

**Concept**: For our simple star topology, manually configure routes instead of computing them.

**Current Problem**:
```cpp
Ipv4GlobalRoutingHelper::PopulateRoutingTables();
// Runs Dijkstra from ALL nodes - O(n²) with 10,000+ nodes
```

**Solution**: Use static routes for our known topology.

```cpp
// Instead of PopulateRoutingTables(), manually set routes:
Ipv4StaticRoutingHelper staticRouting;

// For each client: default route to cache
for (uint32_t i = 0; i < numClients; ++i) {
    Ptr<Ipv4StaticRouting> clientRouting = staticRouting.GetStaticRouting(
        clientNodes.Get(i)->GetObject<Ipv4>());
    // Route all traffic through the cache interface
    clientRouting->SetDefaultRoute(cacheAddress, 1);
}

// Cache: route to origin for origin subnet
Ptr<Ipv4StaticRouting> cacheRouting = staticRouting.GetStaticRouting(
    cacheNode->GetObject<Ipv4>());
cacheRouting->AddNetworkRouteTo(originSubnet, originMask, originInterface);

// Origin: route back to cache for client subnets
// (may need multiple entries or a default route)
```

**Expected Benefits**:

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Routing computation | O(n²) | O(n) | ~5-10x for large n |
| Memory for routes | Full tables | Minimal | Reduced |

**Trade-offs**:
- More code to maintain
- Must manually update if topology changes
- Less flexible for complex topologies

**Best Combined With**: Approach 1 (CSMA) for maximum benefit.

---

## Comparison Summary

| Approach | Speedup | Implementation Effort | Risk |
|----------|---------|----------------------|------|
| CSMA Bus | ~10x | Medium (network layer change) | Low |
| Static Routing | ~5x | Low (replace one function) | Low |
| Both Combined | ~15-20x | Medium | Low |

## Recommendation

**Implement Approach 1 (CSMA) first**, as it provides the largest speedup and is a clean architectural change. The routing optimization can be added later if needed.

**Expected Result**: 10,000 clients should complete setup in ~20-30 seconds instead of >3.5 minutes.

---

## Implementation Plan

### Phase 1: CSMA Migration
1. Add CSMA include and helper
2. Replace P2P link creation loop with single CSMA install
3. Update IP addressing to use single large subnet
4. Test with small client count to verify behavior
5. Benchmark with large client count

### Phase 2: Optional Static Routing
1. Replace `PopulateRoutingTables()` with static route configuration
2. Test connectivity and verify simulation results match

### Verification
- Compare simulation outputs (hit rates, latencies) before/after
- Ensure identical cache behavior
- Benchmark setup time improvements

---

## Appendix: Profiling Data

### Setup Phase Breakdown (5,000 clients)

| Phase | Time | Percentage |
|-------|------|------------|
| Node creation | ~2s | 2% |
| Internet stack | ~5s | 6% |
| **P2P link creation** | **~45s** | **50%** |
| **Routing tables** | **~30s** | **33%** |
| Client apps | ~8s | 9% |

The two highlighted phases (link creation + routing) account for **83%** of setup time and are directly addressed by the proposed optimizations.
