# HTTP Cache Module for ns-3

A network simulator module for ns-3 that models HTTP caching behavior with configurable clients, cache servers, and origin servers. Supports realistic workload modeling with Zipf distribution and provides detailed CSV metrics for analysis.

## Features

- **LRU Cache Eviction** - Least Recently Used eviction policy for cache management
- **Configurable TTL** - Time-to-live expiration for cached content
- **Zipf Distribution** - Realistic content popularity modeling with power-law distribution
- **Large-Scale Simulations** - Support for 50,000+ concurrent clients for scalability testing
- **Per-Request Metrics** - Detailed CSV with request ID, content, latency, and cache hit/miss
- **Summary Statistics** - Per-content aggregate metrics including hit rates and latency percentiles
- **Flexible Configuration** - Command-line parameters for cache capacity, delays, content count, and more
- **Multiple Request Patterns** - Fixed resource or multiple content items with Zipf popularity

## Architecture

The module consists of three main components:

- **HttpClientApp** - Generates HTTP requests, tracks metrics, writes CSV output
- **HttpCacheApp** - LRU cache with TTL expiration and configurable hit latency
- **HttpOriginApp** - Origin server with configurable response delay

Communication uses custom HTTP headers with request IDs, content keys, and cache hit indicators.

## Installation

Install the module into your ns-3 workspace:

```bash
# Remove old version (if exists)
rm -rf ~/Documents/ns-3-dev/src/http-cache

# Copy module to ns-3 source directory
cp -r http-cache ~/Documents/ns-3-dev/src/

# Configure and build ns-3
cd ~/Documents/ns-3-dev
rm -rf cmake-cache build
./ns3 configure --enable-examples -- -G Ninja
./ns3 build
```

## Usage

### Basic Example

Run the simulation with default parameters:

```bash
./ns3 run http-cache-scenario
```

### Common Configuration

Run with 100 requests, 10 content items, Zipf distribution, and output metrics:

```bash
./ns3 run http-cache-scenario -- --nReq=100 --interval=0.2 --ttl=3 --cacheCapacityGB=0.5 --numContent=10 --zipf=true --zipfS=1.0 --originDelay=5 --cacheDelay=1 --csv=metrics.csv --summaryCsv=summary.csv
```

### Object Size and Cache Capacity Examples

The cache capacity is now specified in gigabytes (GB), with the maximum number of cached objects calculated as: `capacity_bytes / objectSize`

```bash
# Small objects (images, API responses) - 1 KB with 500 MB cache = ~500,000 objects
./ns3 run http-cache-scenario -- --objectSize=1024 --cacheCapacityGB=0.5

# Medium objects (web pages) - 10 KB with 1 GB cache = ~100,000 objects
./ns3 run http-cache-scenario -- --objectSize=10240 --cacheCapacityGB=1.0

# Large objects (media files) - 10 MB with 10 GB cache = ~1,000 objects
./ns3 run http-cache-scenario -- --objectSize=10485760 --cacheCapacityGB=10
```

**⚠️ UDP Packet Size Limitation:** The current implementation uses UDP sockets, which have a maximum payload size of ~65,507 bytes (65,535 bytes minus IP and UDP headers). Object sizes exceeding this limit will cause packets to be silently dropped. For objects larger than 64 KB, consider implementing packet fragmentation or switching to TCP sockets.

### Performance Testing

Test cache hit rates with different parameters:

```bash
# Small cache (100 MB) with many content items (low hit rate)
./ns3 run http-cache-scenario -- --nReq=200 --cacheCapacityGB=0.1 --numContent=20 --zipf=true --zipfS=1.2

# Large cache (5 GB) with fewer items (high hit rate)
./ns3 run http-cache-scenario -- --nReq=200 --cacheCapacityGB=5.0 --numContent=10 --zipf=true --zipfS=0.8
```

### Transfer Time and Object Size

The simulation models realistic transfer times based on:
- **Object size** (`--objectSize`): Size of each cached object in bytes
- **Network bandwidth** (future): Link capacity affects transfer speed

Transfer time is calculated by ns-3's packet transmission simulator based on payload size.
Larger objects result in higher latency, especially on cache misses.

### Large-Scale Simulations

Simulate multiple concurrent clients (50k+ users):

```bash
# 1,000 concurrent clients with global summary (1 GB cache)
./ns3 run "http-cache-scenario --numClients=1000 --nReq=100 --numContent=10 --cacheCapacityGB=1.0 --zipf=true --globalSummaryCsv=global_summary.csv"

# 10,000 concurrent clients (global summary recommended, 5 GB cache)
./ns3 run "http-cache-scenario --numClients=10000 --nReq=50 --numContent=20 --cacheCapacityGB=5.0 --zipf=true --globalSummaryCsv=global_10k.csv"

# 50,000 concurrent clients (fastest - global summary only, 10 GB cache)
./ns3 run "http-cache-scenario --numClients=50000 --nReq=10 --numContent=20 --cacheCapacityGB=10 --zipf=true --zipfS=1.2 --globalSummaryCsv=global_50k.csv"

# 100,000 concurrent clients (extreme scale - no CSV for speed, 20 GB cache)
./ns3 run "http-cache-scenario --numClients=100000 --nReq=5 --numContent=15 --cacheCapacityGB=20 --zipf=true"
```

**CSV Output Options:**
- `--csv=file.csv` - Per-request metrics for each client (creates N files for N clients)
- `--summaryCsv=file.csv` - Per-content summary for each client (creates N files for N clients)
- `--globalSummaryCsv=file.csv` - **Single aggregated summary across all clients** (recommended for large-scale)
- Omit all CSV flags for maximum performance

**⚠️ WARNING - Large-Scale Simulations:**
- **DO NOT use `--csv` or `--summaryCsv` with many clients** - they create 1 CSV file per client which becomes impractical and useless (e.g., 50k clients = 50k files!)
- **ALWAYS use `--globalSummaryCsv`** for large-scale testing - creates ONE aggregated summary file across all clients
- `--csv` creates separate files per client: `<base>_client_<id>.csv`
- `--summaryCsv` creates separate summary files per client: `<base>_client_<id>.csv`
- Reduce `--nReq` for very large client counts to keep simulation time reasonable
- Memory usage scales with client count; monitor system resources

## Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--nReq` | uint32_t | 10 | Number of HTTP requests per client |
| `--interval` | double | 0.5 | Time interval between requests (seconds) |
| `--ttl` | uint32_t | 10 | Cache TTL for content (seconds) |
| `--cacheCapacityGB` | double | 1.0 | Cache capacity in gigabytes (max objects = capacity_bytes / objectSize) |
| `--numClients` | uint32_t | 1 | Number of concurrent clients (supports 50k+) |
| `--numContent` | uint32_t | 1 | Number of distinct content items (1 = fixed resource) |
| `--zipf` | bool | false | Use Zipf distribution for content popularity |
| `--zipfS` | double | 1.0 | Zipf skew parameter (higher = more skewed) |
| `--originDelay` | uint32_t | 10 | Origin server response delay (milliseconds) |
| `--cacheDelay` | uint32_t | 1 | Cache processing delay for hits (milliseconds) |
| `--csv` | string | "" | Per-request metrics CSV output path (optional) |
| `--summaryCsv` | string | "" | Per-client summary CSV path (optional) |
| `--globalSummaryCsv` | string | "" | Global aggregated summary CSV path (optional) |
| `--objectSize` | uint32_t | 1024 | Object size in bytes |

## Output Formats

### Per-Request Metrics CSV

Generated when `--csv` is specified. Contains one row per request:

```csv
request_id,content,send_s,recv_s,latency_ms,cache_hit
0,content_3,0.5,0.515,15.123,0
1,content_1,1.0,1.005,5.234,1
2,content_3,1.5,1.505,5.123,1
```

**Columns:**
- `request_id` - Unique identifier for each request
- `content` - Content key requested (e.g., "content_3")
- `send_s` - Request send time (seconds)
- `recv_s` - Response receive time (seconds)
- `latency_ms` - Round-trip latency (milliseconds)
- `cache_hit` - 1 if served from cache, 0 if origin server

### Summary Statistics CSV

Generated when `--summaryCsv` is specified. Contains one row per content item:

```csv
content,total_requests,cache_hits,cache_misses,hit_rate_percent,avg_latency_ms,min_latency_ms,max_latency_ms,avg_hit_latency_ms,avg_miss_latency_ms
content_1,45,40,5,88.89,6.234,5.123,15.234,5.456,14.789
content_2,30,25,5,83.33,6.789,5.234,15.456,5.678,15.123
```

**Columns:**
- `content` - Content key
- `total_requests` - Total number of requests for this content
- `cache_hits` - Number of cache hits
- `cache_misses` - Number of cache misses
- `hit_rate_percent` - Cache hit rate percentage
- `avg_latency_ms` - Average latency across all requests
- `min_latency_ms` - Minimum observed latency
- `max_latency_ms` - Maximum observed latency
- `avg_hit_latency_ms` - Average latency for cache hits
- `avg_miss_latency_ms` - Average latency for cache misses

## Module Files

```
http-cache/
├── model/
│   ├── http-header.{h,cc}        # Custom HTTP header for simulation
│   ├── http-client-app.{h,cc}    # HTTP client with metrics collection
│   ├── http-cache-app.{h,cc}     # LRU cache server with TTL
│   └── http-origin-app.{h,cc}    # Origin server with configurable delay
├── examples/
│   └── http-cache-scenario.cc    # Example simulation scenario
└── CMakeLists.txt                # Build configuration
```

## Contributing

Contributions are welcome! Please check the [issues](https://github.com/eliemada/ns-3-project/issues) for open tasks and feature requests.

## License

Part of the ns-3 network simulator project.
