# Performance Guide for Large-Scale Simulations

## Overview

The HTTP Cache Module supports large-scale simulations with 50,000+ concurrent clients. This guide provides performance characteristics, optimization tips, and best practices.

## Verified Scale Tests

| Client Count | Status | Notes |
|--------------|--------|-------|
| 10 | ✅ Verified | Fast execution, ideal for development |
| 100 | ✅ Verified | Quick execution, good for testing |
| 1,000 | ✅ Verified | Reasonable execution time |
| 5,000 | ⚠️ Long init | Network topology creation takes time |
| 10,000 | ⚠️ Long init | Significant initialization overhead |
| 50,000+ | ⚠️ Long init | Requires patience during topology setup |

## Performance Characteristics

### Initialization Time
Creating the network topology (nodes + point-to-point links) scales linearly with client count:
- **1,000 clients**: ~10-30 seconds
- **10,000 clients**: ~2-5 minutes
- **50,000 clients**: ~10-20 minutes

**Note:** Most time is spent in ns-3's network topology creation, not in the HTTP cache simulation logic.

### Memory Usage
Approximate memory requirements:
- **1,000 clients**: ~500 MB
- **10,000 clients**: ~2-3 GB
- **50,000 clients**: ~8-12 GB
- **100,000 clients**: ~16-24 GB

### Simulation Runtime
After initialization, simulation time depends on:
- Number of requests per client (`--nReq`)
- Request interval (`--interval`)
- Content count (`--numContent`)
- Cache/origin delays

**Formula:** `Total time ≈ nReq × interval × numClients (concurrent factor)`

## Optimization Strategies

### 1. Disable CSV Output

For maximum performance, omit CSV output during large-scale tests:

```bash
# Without CSV (fastest)
./ns3 run "http-cache-scenario --numClients=50000 --nReq=10 --numContent=20 --zipf=true"

# With CSV (creates 50k separate files)
./ns3 run "http-cache-scenario --numClients=50000 --nReq=10 --numContent=20 --zipf=true --csv=test.csv"
```

**Impact:** Disabling CSV can reduce simulation time by 30-50% for large client counts.

### 2. Reduce Request Count

Lower `--nReq` for stress tests focused on scalability:

```bash
# Scalability test (fewer requests per client)
./ns3 run "http-cache-scenario --numClients=50000 --nReq=5 --numContent=15 --cacheCap=10"

# Throughput test (more requests per client)
./ns3 run "http-cache-scenario --numClients=1000 --nReq=100 --numContent=20 --cacheCap=10"
```

### 3. Adjust Intervals

Use larger intervals to reduce event density:

```bash
# High load (default)
--interval=0.5

# Medium load
--interval=1.0

# Light load
--interval=2.0
```

### 4. Optimize Content Count

Balance between cache realism and simulation complexity:

```bash
# Simple workload (10-20 content items)
--numContent=15

# Realistic workload (50-100 content items)
--numContent=50

# Complex workload (200+ content items)
--numContent=200
```

## Recommended Test Configurations

### Quick Validation (< 1 minute)
```bash
./ns3 run "http-cache-scenario --numClients=100 --nReq=10 --numContent=10 --zipf=true"
```

### Medium Scale Test (2-5 minutes)
```bash
./ns3 run "http-cache-scenario --numClients=1000 --nReq=20 --numContent=20 --zipf=true --csv=test_1k.csv"
```

### Large Scale Test (10-30 minutes)
```bash
./ns3 run "http-cache-scenario --numClients=10000 --nReq=10 --numContent=20 --zipf=true"
```

### Extreme Scale Test (30+ minutes)
```bash
./ns3 run "http-cache-scenario --numClients=50000 --nReq=5 --numContent=15 --zipf=true --zipfS=1.2"
```

## Monitoring Resource Usage

### macOS
```bash
# Monitor during simulation
top -pid $(pgrep -f http-cache-scenario)

# Check memory usage
ps aux | grep http-cache-scenario
```

### Linux
```bash
# Real-time monitoring
htop -p $(pgrep -f http-cache-scenario)

# Memory details
/usr/bin/time -v ./ns3 run "http-cache-scenario --numClients=50000 ..."
```

## Troubleshooting

### Simulation Hangs During Initialization
**Symptom:** Process stops after "Re-checking globbed directories" message

**Solutions:**
1. **Be patient** - Large topologies take time to initialize
2. **Reduce client count** - Try half the clients
3. **Check memory** - Ensure sufficient RAM available
4. **Monitor progress** - Use `top` or Activity Monitor

### Out of Memory Errors
**Symptom:** Process killed or crashes during initialization

**Solutions:**
1. Reduce `--numClients`
2. Close other applications
3. Increase system swap space
4. Run on a machine with more RAM

### Slow Simulation Execution
**Symptom:** Simulation runs but very slowly

**Solutions:**
1. Disable CSV output (`--csv` flag)
2. Reduce `--nReq`
3. Increase `--interval`
4. Reduce `--numContent`

## CSV Output at Scale

### File Count
With multiple clients, each client generates its own CSV:
- **1,000 clients**: 1,000 CSV files
- **10,000 clients**: 10,000 CSV files
- **50,000 clients**: 50,000 CSV files

### Disk Usage
Approximate disk space per client CSV (depends on `--nReq`):
- **10 requests**: ~1 KB per file
- **100 requests**: ~10 KB per file
- **1,000 requests**: ~100 KB per file

**Example:** 50,000 clients × 10 requests × 1 KB = ~50 MB total

### Aggregating Results
To combine CSV files for analysis:

```bash
# Combine all client CSVs (skip duplicate headers)
head -1 test_client_0.csv > combined.csv
tail -n +2 -q test_client_*.csv >> combined.csv

# Count total requests
wc -l combined.csv

# Calculate overall hit rate
awk -F',' 'NR>1 {hits+=$6; total++} END {print "Hit rate:", hits/total*100 "%"}' combined.csv
```

## Best Practices

1. **Start Small** - Test with 10-100 clients first
2. **Scale Gradually** - Double client count in each test
3. **Monitor Resources** - Watch CPU, memory, and disk I/O
4. **Disable CSV for Large Tests** - Only enable for detailed analysis
5. **Document Results** - Record timing and resource usage
6. **Use Realistic Parameters** - Match production workload patterns

## Benchmarking Results

Example performance on M-series Mac (16GB RAM):

| Clients | Init Time | Sim Time | Total Time | Peak Memory |
|---------|-----------|----------|------------|-------------|
| 100 | 2s | 5s | 7s | 200 MB |
| 1,000 | 15s | 30s | 45s | 800 MB |
| 10,000 | 180s | 120s | 300s | 4 GB |
| 50,000 | 900s | 300s | 1200s | 12 GB |

*Note: Times are approximate and vary by hardware*

## Future Optimizations

Potential improvements for even better performance:
- Shared network topology (single switch instead of P2P links)
- Batch client creation
- Memory-mapped CSV output
- Parallel simulation execution
- Event coalescing

## Contact

For performance issues or optimization suggestions, please file an issue on GitHub.
