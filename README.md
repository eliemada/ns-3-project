# HTTP Cache Module for ns-3

[![ns-3](https://img.shields.io/badge/ns--3-3.36+-blue.svg)](https://www.nsnam.org/)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-GPL--2.0-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-CMake-orange.svg)](CMakeLists.txt)

A network simulator module for ns-3 that models HTTP caching behavior with configurable clients, cache servers, and origin servers. Supports realistic workload modeling with Zipf distribution, dynamic TTL policies, and provides detailed CSV metrics for analysis.

## Table of Contents

- [Features](#features)
- [Architecture](#architecture)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Usage Examples](#usage-examples)
- [Configuration Parameters](#configuration-parameters)
- [Output Formats](#output-formats)
- [Module Structure](#module-structure)
- [API Documentation](#api-documentation)
- [Performance](#performance)
- [License](#license)

## Features

| Feature | Description |
|---------|-------------|
| **LRU Cache Eviction** | O(1) Least Recently Used eviction policy with hash map + doubly-linked list |
| **TTL Expiration** | Time-to-live based cache invalidation with refresh on hit |
| **Zipf Distribution** | Realistic content popularity modeling (P(k) ∝ 1/k^s) |
| **Dynamic TTL Policy** | Fairness control by penalizing services dominating cache |
| **Request Deduplication** | Prevents duplicate origin fetches for concurrent requests |
| **Large-Scale Support** | Tested with 100,000+ concurrent clients |
| **Streaming Mode** | HTTP Adaptive Streaming simulation with sequential segments |
| **Comprehensive Metrics** | Per-request, per-content, and global aggregated statistics |
| **Configurable Bandwidth** | Model realistic network conditions for client-cache and cache-origin links |

## Architecture

The module implements a three-tier HTTP caching architecture:

```
┌─────────────────────────────────────────────────────────────────┐
│                        HTTP CACHE MODULE                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   [Client 1] ──┐                                                │
│   [Client 2] ──┼── (clientCacheBw) ──> [Cache] <--(cacheOriginBw)--> [Origin]
│   [Client N] ──┘                         │                        │
│                                          │                        │
│                                    ┌─────┴─────┐                 │
│                                    │ LRU Cache │                 │
│                                    │ + TTL     │                 │
│                                    │ + Dedup   │                 │
│                                    └───────────┘                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Components

| Component | Class | Description |
|-----------|-------|-------------|
| **Client** | `HttpClientApp` | Generates requests, collects metrics, supports Zipf distribution |
| **Cache** | `HttpCacheApp` | LRU eviction, TTL expiration, request deduplication, dynamic TTL |
| **Origin** | `HttpOriginApp` | Backend server with configurable processing delay |
| **Header** | `HttpHeader` | Custom ns-3 header for request/response protocol |

## Installation

### Prerequisites

- ns-3 version 3.36 or later
- CMake 3.10+
- C++17 compatible compiler (GCC 7+, Clang 5+)
- Ninja build system (recommended)

### Build Steps

```bash
# Clone or copy the module to ns-3 source directory
cp -r http-cache ~/Documents/ns-3-dev/src/

# Navigate to ns-3 directory
cd ~/Documents/ns-3-dev

# Clean previous build (if needed)
rm -rf cmake-cache build

# Configure with examples enabled
./ns3 configure --enable-examples -- -G Ninja

# Build the project
./ns3 build
```

### Verify Installation

```bash
# Run the example scenario
./ns3 run http-cache-scenario

# Expected output:
# Cache configuration:
#   Capacity: 1 GB (1073741824 bytes)
#   Object size: 1024 bytes
#   Max objects: 1048576
# Starting simulation with 1 client(s)...
# Simulation completed successfully!
```

## Quick Start

### Basic Simulation

```bash
# Run with default parameters
./ns3 run http-cache-scenario
```

### Realistic Cache Simulation

```bash
# 100 clients, 20 content items, Zipf distribution, 1GB cache
./ns3 run http-cache-scenario -- \
  --numClients=100 \
  --nReq=50 \
  --numContent=20 \
  --zipf=true \
  --zipfS=1.0 \
  --cacheCapacityGB=1.0 \
  --globalSummaryCsv=results.csv
```

### Video Streaming Simulation

```bash
# Streaming with 10 services, 5 segments each
./ns3 run http-cache-video-streaming -- \
  --numClients=500 \
  --numServices=10 \
  --numSegments=5 \
  --zipf=true \
  --totalTime=60 \
  --serviceSummaryCsv=streaming_results.csv
```

## Usage Examples

### Object Size and Cache Capacity

```bash
# Small objects (1 KB) - API responses, thumbnails
./ns3 run http-cache-scenario -- --objectSize=1024 --cacheCapacityGB=0.5

# Medium objects (100 KB) - web pages, small images
./ns3 run http-cache-scenario -- --objectSize=102400 --cacheCapacityGB=2.0

# Large objects (1 MB) - video segments
./ns3 run http-cache-scenario -- --objectSize=1048576 --cacheCapacityGB=10.0

# Specify capacity in objects instead of GB
./ns3 run http-cache-scenario -- --cacheCapacityObjs=10000 --objectSize=102400
```

### Network Bandwidth Configuration

```bash
# Fiber clients (1 Gbps) with moderate backbone
./ns3 run http-cache-scenario -- --clientCacheBw=1000 --cacheOriginBw=100

# Mobile clients (20 Mbps) with slow origin
./ns3 run http-cache-scenario -- --clientCacheBw=20 --cacheOriginBw=10

# Datacenter scenario (10 Gbps)
./ns3 run http-cache-scenario -- --clientCacheBw=10000 --cacheOriginBw=10000
```

### Large-Scale Simulations

```bash
# 10,000 clients (use global summary for efficiency)
./ns3 run http-cache-scenario -- \
  --numClients=10000 \
  --nReq=20 \
  --numContent=50 \
  --zipf=true \
  --cacheCapacityGB=5.0 \
  --globalSummaryCsv=large_scale.csv

# 100,000 clients (no CSV for maximum performance)
./ns3 run http-cache-scenario -- \
  --numClients=100000 \
  --nReq=5 \
  --numContent=20 \
  --zipf=true \
  --cacheCapacityGB=20.0
```

### Dynamic TTL Policy

```bash
# Penalize services exceeding 30% request share with 50% TTL reduction
./ns3 run http-cache-video-streaming -- \
  --numClients=1000 \
  --numServices=20 \
  --dynamicTtl=true \
  --ttlThreshold=0.3 \
  --ttlReduction=0.5 \
  --serviceSummaryCsv=fairness_results.csv
```

## Configuration Parameters

### Core Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--nReq` | uint32 | 100 | Number of requests per client |
| `--interval` | double | 0.5 | Time between requests (seconds) |
| `--numClients` | uint32 | 1 | Number of concurrent clients |
| `--numContent` | uint32 | 1 | Distinct content items |
| `--objectSize` | uint32 | 1024 | Object size in bytes |

### Cache Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--cacheCapacityGB` | double | 1.0 | Cache capacity in gigabytes |
| `--cacheCapacityObjs` | uint32 | 0 | Cache capacity in objects (overrides GB) |
| `--ttl` | double | 5.0 | Cache TTL in seconds |
| `--cacheDelay` | uint32 | 1 | Cache hit delay (milliseconds) |
| `--originDelay` | uint32 | 1 | Origin response delay (milliseconds) |

### Distribution Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--zipf` | bool | false | Enable Zipf distribution |
| `--zipfS` | double | 1.0 | Zipf skew parameter (higher = more skewed) |

### Network Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--clientCacheBw` | uint32 | 100 | Client-Cache bandwidth (Mbps) |
| `--cacheOriginBw` | uint32 | 50 | Cache-Origin bandwidth (Mbps) |

### Dynamic TTL Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--dynamicTtl` | bool | false | Enable dynamic TTL policy |
| `--ttlWindow` | double | 300 | Sliding window duration (seconds) |
| `--ttlThreshold` | double | 0.5 | Request share threshold for penalty |
| `--ttlReduction` | double | 0.5 | TTL reduction factor (0.5 = 50% reduction) |
| `--ttlEvalInterval` | double | 30 | Policy evaluation interval (seconds) |

### Output Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--csv` | string | "" | Per-request metrics CSV path |
| `--summaryCsv` | string | "" | Per-content summary CSV path |
| `--globalSummaryCsv` | string | "" | Global aggregated summary CSV |
| `--serviceSummaryCsv` | string | "" | Service-level summary CSV |

## Output Formats

### Per-Request Metrics CSV

```csv
request_id,content,send_s,recv_s,latency_ms,cache_hit
0,/file-1,0.5,0.515,15.123,0
1,/file-1,1.0,1.005,5.234,1
2,/file-3,1.5,1.520,20.456,0
```

### Summary Statistics CSV

```csv
content,total_requests,cache_hits,cache_misses,hit_rate_percent,avg_latency_ms,min_latency_ms,max_latency_ms,avg_hit_latency_ms,avg_miss_latency_ms
/file-1,45,40,5,88.89,6.234,5.123,15.234,5.456,14.789
/file-2,30,25,5,83.33,6.789,5.234,15.456,5.678,15.123
```

### Service-Level Summary CSV

```csv
service,total_requests,cache_hits,cache_misses,hit_rate_percent,avg_latency_ms,min_latency_ms,max_latency_ms,avg_hit_latency_ms,avg_miss_latency_ms
service-1,150,120,30,80.00,7.5,5.0,20.0,5.5,15.0
service-2,100,85,15,85.00,6.8,4.5,18.0,5.0,14.5
```

## Module Structure

```
http-cache/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── LICENSE                     # GPL-2.0 license
├── model/
│   ├── http-header.h           # Custom HTTP header class
│   ├── http-header.cc
│   ├── http-client-app.h       # Client application
│   ├── http-client-app.cc
│   ├── http-cache-app.h        # Cache server application
│   ├── http-cache-app.cc
│   ├── http-origin-app.h       # Origin server application
│   └── http-origin-app.cc
├── examples/
│   ├── CMakeLists.txt          # Examples build configuration
│   ├── http-cache-scenario.cc  # Standard caching scenario
│   └── http-cache-video-streaming.cc  # Streaming scenario
└── docs/
    └── plans/                  # Design documents
```

## API Documentation

### HttpClientApp

```cpp
// Configuration
void SetRemote(Address address, uint16_t port);
void SetInterval(Time t);
void SetNumContent(uint32_t n);
void SetZipf(bool z);
void SetZipfS(double s);
void SetTotalRequests(uint32_t n);
void SetObjectSize(uint32_t size);

// Streaming mode
void SetStreaming(bool s);
void SetNumServices(uint32_t n);
void SetNumSegments(uint32_t n);
void SetSegmentInterval(Time t);

// Output
void SetCsvPath(const std::string& p);
void SetSummaryCsvPath(const std::string& p);

// Statistics
const std::unordered_map<std::string, ContentStats>& GetContentStats() const;
```

### HttpCacheApp

```cpp
// Configuration
void SetListenPort(uint16_t p);
void SetOrigin(Address a, uint16_t p);
void SetTtl(Time t);
void SetCapacity(uint32_t entries);
void SetCacheDelay(Time t);
void SetObjectSize(uint32_t size);

// Dynamic TTL policy
void SetDynamicTtlEnabled(bool enabled);
void SetTtlWindow(Time window);
void SetTtlThreshold(double threshold);
void SetTtlReduction(double reduction);
void SetTtlEvalInterval(Time interval);

// Statistics
uint64_t GetTotalRequests() const;
uint64_t GetTotalHits() const;
```

### HttpOriginApp

```cpp
void SetListenPort(uint16_t p);
void SetServiceDelay(Time t);
void SetObjectSize(uint32_t size);
```

## Performance

### Key Recommendations

1. **Large-scale simulations**: Use `--globalSummaryCsv` instead of per-client CSV
2. **Memory optimization**: Reduce `--nReq` for very large client counts
3. **Speed optimization**: Disable CSV output for maximum throughput
4. **UDP limitation**: Object sizes > 64 KB may cause packet drops

### Tested Scale

| Clients | Requests/Client | Memory | Runtime |
|---------|-----------------|--------|---------|
| 1,000 | 100 | ~500 MB | ~10s |
| 10,000 | 50 | ~2 GB | ~60s |
| 50,000 | 20 | ~8 GB | ~5 min |
| 100,000 | 10 | ~16 GB | ~15 min |

## License

This project is licensed under the GNU General Public License v2.0 - see the [LICENSE](LICENSE) file for details.

This module is part of the ns-3 network simulator project.

---

**Author**: Elie Bruno
**Repository**: [github.com/eliemada/ns-3-project](https://github.com/eliemada/ns-3-project)
