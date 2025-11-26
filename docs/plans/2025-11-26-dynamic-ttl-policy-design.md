# Dynamic TTL Policy Design

## Overview

A configurable policy that penalizes services dominating cache requests by reducing their TTL. The cache tracks request counts per service in fixed time buckets, and periodically evaluates if any service exceeds a threshold percentage of total requests.

## Parameters (command-line flags)

| Flag | Description | Default |
|------|-------------|---------|
| `--dynamicTtl` | Enable dynamic TTL policy | `false` |
| `--ttlWindow` | Sliding window duration in seconds | `300` (5 min) |
| `--ttlThreshold` | Request share threshold (0.0-1.0) | `0.5` (50%) |
| `--ttlReduction` | TTL reduction factor (0.0-1.0) | `0.5` (50%) |
| `--ttlEvalInterval` | Evaluation interval in seconds | `30` |

## Behavior

1. **Request tracking**: On each request, increment counter for that service in the current time bucket
2. **Periodic evaluation**: Every `ttlEvalInterval` seconds:
   - Sum requests per service over buckets within the window
   - If any service exceeds `ttlThreshold` of total requests, apply `ttlReduction` to its cached entries' TTL
   - If service drops below threshold, restore normal TTL (auto-recovery)
3. **Bucket cleanup**: Discard buckets older than the window

## Architecture

### New data structures in `HttpCacheApp`

```cpp
// Request tracking per time bucket
struct TimeBucket {
  Time startTime;
  std::unordered_map<std::string, uint32_t> serviceRequests; // service -> count
};
std::list<TimeBucket> m_buckets;        // ordered by time
std::unordered_set<std::string> m_penalizedServices;  // currently penalized

// Policy parameters
bool m_dynamicTtlEnabled = false;
Time m_ttlWindow{Seconds(300)};         // 5 minutes
double m_ttlThreshold = 0.5;            // 50%
double m_ttlReduction = 0.5;            // reduce by 50%
Time m_ttlEvalInterval{Seconds(30)};    // evaluate every 30s
```

### Key methods

| Method | Purpose |
|--------|---------|
| `RecordRequest(service)` | Called on each request; adds to current bucket |
| `EvaluatePolicy()` | Scheduled periodically; computes shares, applies penalties |
| `GetEffectiveTtl(service)` | Returns reduced TTL if service is penalized, else normal |
| `ExtractService(resource)` | Parses `/service-X/seg-Y` → `service-X` |

### Flow

1. `HandleClientRead` calls `RecordRequest(service)` on each request
2. `Insert()` uses `GetEffectiveTtl(service)` instead of `m_ttl`
3. `EvaluatePolicy()` runs on a timer, updates `m_penalizedServices`

## Evaluation Logic

### `EvaluatePolicy()` pseudocode

```cpp
void EvaluatePolicy() {
  // 1. Prune old buckets outside the window
  Time cutoff = Simulator::Now() - m_ttlWindow;
  while (!m_buckets.empty() && m_buckets.front().startTime < cutoff) {
    m_buckets.pop_front();
  }

  // 2. Aggregate requests per service
  std::unordered_map<std::string, uint32_t> totals;
  uint32_t grandTotal = 0;
  for (const auto& bucket : m_buckets) {
    for (const auto& [service, count] : bucket.serviceRequests) {
      totals[service] += count;
      grandTotal += count;
    }
  }

  // 3. Determine penalized services
  m_penalizedServices.clear();
  if (grandTotal > 0) {
    for (const auto& [service, count] : totals) {
      double share = static_cast<double>(count) / grandTotal;
      if (share > m_ttlThreshold) {
        m_penalizedServices.insert(service);
      }
    }
  }

  // 4. Schedule next evaluation
  Simulator::Schedule(m_ttlEvalInterval, &HttpCacheApp::EvaluatePolicy, this);
}
```

### `GetEffectiveTtl()` logic

```cpp
Time GetEffectiveTtl(const std::string& service) {
  if (m_penalizedServices.count(service) > 0) {
    return m_ttl * (1.0 - m_ttlReduction);  // e.g., 50% of original
  }
  return m_ttl;
}
```

## Future Alternative: Proportional to Fair Share

This section documents an alternative approach for future consideration.

### Concept

Instead of a fixed threshold, penalize services that exceed their "fair share" of requests. With N services, fair share = 1/N.

### Parameters

| Flag | Description | Example |
|------|-------------|---------|
| `--ttlFairShareMargin` | How much above fair share triggers penalty | `0.1` (10%) |

### Logic

```cpp
double fairShare = 1.0 / numServices;
double threshold = fairShare + m_fairShareMargin;

for (const auto& [service, count] : totals) {
  double share = static_cast<double>(count) / grandTotal;
  if (share > threshold) {
    // Proportional penalty: more excess = more reduction
    double excess = share - fairShare;
    double reductionFactor = std::min(1.0, excess / fairShare);
    m_serviceReduction[service] = reductionFactor;
  }
}
```

### Trade-offs

| Aspect | Single Threshold | Fair Share |
|--------|-----------------|------------|
| Simplicity | Simple, one parameter | Requires knowing N services |
| Adaptability | Fixed regardless of service count | Adapts to number of services |
| Granularity | Binary (penalized or not) | Proportional penalty |
