# Progress Tracking for Large Simulations

## Problem
When running simulations with large numbers of clients (50k+), the terminal shows no output for extended periods, making it appear stuck.

## Solution
Add time-based progress tracking that prints periodic updates showing simulation progress, request counts, and cache hit rates.

## Design

### Architecture
1. Add request/hit counters to `HttpCacheApp`
2. Schedule periodic progress callbacks during simulation
3. Print formatted progress to stdout

### Implementation

**HttpCacheApp changes:**
- Add `m_totalRequests` and `m_totalHits` counters
- Add getter methods `GetTotalRequests()` and `GetTotalHits()`
- Increment counters in `HandleClientRead()`

**Video streaming example changes:**
- Add `--progressInterval` flag (default: 10% of totalTime)
- Add `PrintProgress()` callback function
- Schedule first progress callback at simulation start

### Output Format
```
[10%] 100s/1000s | 98,432 requests | 45.2% hit rate
```

### Command Line
```bash
./ns3 run "http-cache-video-streaming --numClients=1000 --totalTime=1000 --progressInterval=5"
```
- `--progressInterval`: Percentage interval for progress updates (default: 10)

## Alternatives Considered
- **Event-based tracking**: Print every N requests. Rejected because output frequency varies with load.
- **Detailed stats**: Include cache size, memory. Rejected to keep output clean.
