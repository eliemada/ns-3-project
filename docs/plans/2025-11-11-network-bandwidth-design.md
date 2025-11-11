# Network Bandwidth Configuration Design

**Date:** 2025-11-11
**Issue:** #8 - Feature: Configure network bandwidth for client-cache and cache-origin links
**Status:** Approved

## Overview

Add command-line flags to configure network bandwidth separately for (i) client-to-cache links and (ii) cache-to-origin links, enabling realistic network modeling in the HTTP cache simulation.

## Requirements

- Add `--clientCacheBw` flag for client ↔ cache link bandwidth
- Add `--cacheOriginBw` flag for cache ↔ origin link bandwidth
- Use simple numeric values in Mbps
- Maintain current default values (100 Mbps client-cache, 50 Mbps cache-origin)
- Apply bandwidth constraints using ns-3 PointToPointHelper API
- Network delays remain hardcoded (not configurable)

## Command-Line Interface

### New Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--clientCacheBw` | uint32_t | 100 | Client-Cache link bandwidth (Mbps) |
| `--cacheOriginBw` | uint32_t | 50 | Cache-Origin link bandwidth (Mbps) |

### Usage Examples

```bash
# Slow client connections (10Mbps), fast backbone (10Gbps)
./ns3 run http-cache-scenario -- --clientCacheBw=10 --cacheOriginBw=10000

# Symmetric 1Gbps links
./ns3 run http-cache-scenario -- --clientCacheBw=1000 --cacheOriginBw=1000

# Congested cache uplink scenario
./ns3 run http-cache-scenario -- --clientCacheBw=100 --cacheOriginBw=50
```

## Implementation Details

### Code Changes in `http-cache-scenario.cc`

**1. Add variable declarations** (around line 21):
```cpp
uint32_t clientCacheBw = 100;  // Mbps
uint32_t cacheOriginBw = 50;   // Mbps
```

**2. Add command-line parsing** (around line 37):
```cpp
cmd.AddValue("clientCacheBw", "Client-Cache link bandwidth (Mbps)", clientCacheBw);
cmd.AddValue("cacheOriginBw", "Cache-Origin link bandwidth (Mbps)", cacheOriginBw);
```

**3. Replace hardcoded bandwidth values** (lines 56 and 60):
```cpp
// Build bandwidth strings dynamically
std::ostringstream clientCacheBwStr, cacheOriginBwStr;
clientCacheBwStr << clientCacheBw << "Mbps";
cacheOriginBwStr << cacheOriginBw << "Mbps";

// Apply to point-to-point helpers
p2pClientCache.SetDeviceAttribute("DataRate", StringValue(clientCacheBwStr.str()));
p2pCacheOrigin.SetDeviceAttribute("DataRate", StringValue(cacheOriginBwStr.str()));
```

### Network Topology

```
[Clients] <--clientCacheBw (default: 100Mbps)--> [Cache] <--cacheOriginBw (default: 50Mbps)--> [Origin]
          <--2ms delay-->                                 <--5ms delay-->
```

Delays remain hardcoded as they are not part of this feature scope.

## Documentation Updates

### README.md Changes

**1. Configuration Parameters table** (add after line 149):
- Add `--clientCacheBw` parameter
- Add `--cacheOriginBw` parameter

**2. New section: "Network Bandwidth Examples"** (after "Transfer Time and Object Size" section):
- DSL/Cable clients with fiber backbone
- Mobile clients with moderate backbone
- Fiber clients with congested uplink
- Symmetric gigabit links
- Explanation of bandwidth impact on transfer time
- Combination examples with `--objectSize`

**3. Update "Transfer Time and Object Size" section**:
- Change "Network bandwidth (future)" to "Network bandwidth (`--clientCacheBw`, `--cacheOriginBw`)"
- Note that bandwidth is now configurable

**4. Update Architecture section**:
- Mention configurable bandwidth between components

## Testing & Validation

### Build Verification
- Compile code without errors: `./ns3 build`

### Functionality Tests
1. Run with default values (should match current behavior)
2. Run with custom bandwidth values
3. Test extreme values (1 Mbps, 10000 Mbps)
4. Test with large object sizes to observe bandwidth impact

### CSV Validation
- Verify `latency_ms` reflects bandwidth constraints
- Lower bandwidth → higher latency for large objects
- Compare hit vs miss latencies with different configurations

### Help Text Verification
- Run `./ns3 run http-cache-scenario -- --help`
- Verify new flags appear with clear descriptions

### Test Scenarios
```bash
# Test 1: Slow clients, fast backbone
./ns3 run http-cache-scenario -- --clientCacheBw=10 --cacheOriginBw=10000 --objectSize=1048576 --csv=test1.csv

# Test 2: Congested cache uplink
./ns3 run http-cache-scenario -- --clientCacheBw=100 --cacheOriginBw=50 --objectSize=1048576 --csv=test2.csv

# Test 3: Default behavior
./ns3 run http-cache-scenario -- --csv=test3.csv
```

## Acceptance Criteria

- [x] Design approved
- [ ] Both bandwidth flags implemented
- [ ] Bandwidth correctly applied to network links in ns-3
- [ ] Latency in CSV reflects bandwidth constraints
- [ ] Works with large numbers of clients
- [ ] Documentation includes realistic bandwidth values
- [ ] Help text explains units and typical values

## Use Cases

- Simulate mobile vs broadband clients
- Model CDN edge-to-origin bottlenecks
- Test cache effectiveness under bandwidth constraints
- Analyze impact of network capacity on hit/miss latency

## Related Issues

- Issue #8 (this implementation)
- Related to object size feature - bandwidth × object size determines transfer time
