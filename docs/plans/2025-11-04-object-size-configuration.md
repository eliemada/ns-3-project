# Object Size Configuration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add `--objectSize` command-line flag to configure the size of cached objects in bytes for realistic transfer time modeling.

**Architecture:** Add a member variable to each component (HttpClientApp, HttpCacheApp, HttpOriginApp) that controls packet payload size. Wire the command-line parameter through the scenario file to all three components using setter methods.

**Tech Stack:** ns-3 network simulator (C++17), UDP sockets, custom HTTP protocol headers

---

## Task 1: Add ObjectSize Support to HttpClientApp

**Files:**
- Modify: `model/http-client-app.h` (add member variable and setter declaration)
- Modify: `model/http-client-app.cc` (add setter implementation and modify packet creation)

**Step 1: Add member variable to header**

In `model/http-client-app.h`, after line 77 (before closing brace of class), add:

```cpp
  uint32_t m_objectSize = 1024;  ///< Object size in bytes
```

**Step 2: Add setter declaration to header**

In `model/http-client-app.h`, before line 43 (before the `private:` section), add:

```cpp
  /**
   * \brief Set the size of objects to request
   * \param size Object size in bytes
   */
  void SetObjectSize(uint32_t size);
```

**Step 3: Implement setter in .cc file**

In `model/http-client-app.cc`, after line 32 (after the other setter implementations), add:

```cpp
void HttpClientApp::SetObjectSize(uint32_t size) {
  m_objectSize = size;
}
```

**Step 4: Modify packet creation to include payload**

In `model/http-client-app.cc`, line 82, change:

```cpp
// OLD:
Ptr<Packet> p = Create<Packet>(0);

// NEW:
Ptr<Packet> p = Create<Packet>(m_objectSize);
```

**Step 5: Build and verify compilation**

```bash
./ns3 build
```

Expected: Build succeeds with no errors

**Step 6: Commit changes**

```bash
git add model/http-client-app.h model/http-client-app.cc
git commit -m "feat: add object size support to HttpClientApp"
```

---

## Task 2: Add ObjectSize Support to HttpCacheApp

**Files:**
- Modify: `model/http-cache-app.h` (add member variable and setter declaration)
- Modify: `model/http-cache-app.cc` (add setter implementation and modify packet creation in 2 places)

**Step 1: Add member variable to header**

In `model/http-cache-app.h`, after line 38 (before closing brace of class), add:

```cpp
  uint32_t m_objectSize = 1024;  ///< Object size in bytes
```

**Step 2: Add setter declaration to header**

In `model/http-cache-app.h`, before the `private:` section, add:

```cpp
  /**
   * \brief Set the size of objects being cached
   * \param size Object size in bytes
   */
  void SetObjectSize(uint32_t size);
```

**Step 3: Implement setter in .cc file**

In `model/http-cache-app.cc`, after line 24 (after SetCacheDelay implementation), add:

```cpp
void HttpCacheApp::SetObjectSize(uint32_t size) {
  m_objectSize = size;
}
```

**Step 4: Modify packet creation when forwarding to origin**

In `model/http-cache-app.cc`, line 65, change:

```cpp
// OLD:
Ptr<Packet> fwd = Create<Packet>(0);

// NEW:
Ptr<Packet> fwd = Create<Packet>(m_objectSize);
```

**Step 5: Modify packet creation when replying to client**

In `model/http-cache-app.cc`, line 90, change:

```cpp
// OLD:
Ptr<Packet> resp = Create<Packet>(0);

// NEW:
Ptr<Packet> resp = Create<Packet>(m_objectSize);
```

**Step 6: Build and verify compilation**

```bash
./ns3 build
```

Expected: Build succeeds with no errors

**Step 7: Commit changes**

```bash
git add model/http-cache-app.h model/http-cache-app.cc
git commit -m "feat: add object size support to HttpCacheApp"
```

---

## Task 3: Add ObjectSize Support to HttpOriginApp

**Files:**
- Modify: `model/http-origin-app.h` (add member variable and setter declaration)
- Modify: `model/http-origin-app.cc` (add setter implementation and modify packet creation)

**Step 1: Add member variable to header**

In `model/http-origin-app.h`, after line 20 (before closing brace of class), add:

```cpp
  uint32_t m_objectSize = 1024;  ///< Object size in bytes
```

**Step 2: Add setter declaration to header**

In `model/http-origin-app.h`, before line 14 (before the `private:` section), add:

```cpp
  /**
   * \brief Set the size of objects to serve
   * \param size Object size in bytes
   */
  void SetObjectSize(uint32_t size);
```

**Step 3: Implement setter in .cc file**

In `model/http-origin-app.cc`, after line 18 (after SetServiceDelay implementation), add:

```cpp
void HttpOriginApp::SetObjectSize(uint32_t size) {
  m_objectSize = size;
}
```

**Step 4: Modify packet creation in Respond method**

In `model/http-origin-app.cc`, line 36, change:

```cpp
// OLD:
Ptr<Packet> resp = Create<Packet>(0);

// NEW:
Ptr<Packet> resp = Create<Packet>(m_objectSize);
```

**Step 5: Build and verify compilation**

```bash
./ns3 build
```

Expected: Build succeeds with no errors

**Step 6: Commit changes**

```bash
git add model/http-origin-app.h model/http-origin-app.cc
git commit -m "feat: add object size support to HttpOriginApp"
```

---

## Task 4: Add Command-Line Parameter and Wire to Components

**Files:**
- Modify: `examples/http-cache-scenario.cc` (add variable, command-line flag, and setter calls)

**Step 1: Add objectSize variable declaration**

In `examples/http-cache-scenario.cc`, line 19, add to the variable declarations:

```cpp
uint32_t objectSize = 1024;  // Default 1 KB
```

**Step 2: Add command-line parameter**

In `examples/http-cache-scenario.cc`, after line 35 (after the last cmd.AddValue call), add:

```cpp
cmd.AddValue("objectSize", "Object size in bytes (default 1024)", objectSize);
```

**Step 3: Configure origin server with objectSize**

In `examples/http-cache-scenario.cc`, line 91 (after `origin->SetServiceDelay(...)`), add:

```cpp
origin->SetObjectSize(objectSize);
```

**Step 4: Configure cache server with objectSize**

In `examples/http-cache-scenario.cc`, line 101 (after `cache->SetCacheDelay(...)`), add:

```cpp
cache->SetObjectSize(objectSize);
```

**Step 5: Configure clients with objectSize**

In `examples/http-cache-scenario.cc`, after line 116 (inside the client creation loop, after `client->SetTotalRequests(nReq)`), add:

```cpp
client->SetObjectSize(objectSize);
```

**Step 6: Build the project**

```bash
./ns3 build
```

Expected: Build succeeds with no errors

**Step 7: Test with default objectSize**

```bash
./ns3 run http-cache-scenario -- --nReq=10 --numClients=5
```

Expected: Simulation runs successfully, CSV output generated

**Step 8: Test with custom objectSize (10 KB)**

```bash
./ns3 run http-cache-scenario -- --nReq=10 --numClients=5 --objectSize=10240
```

Expected: Simulation runs successfully, packets are 10 KB

**Step 9: Test with large objectSize (10 MB)**

```bash
./ns3 run http-cache-scenario -- --nReq=10 --numClients=5 --objectSize=10485760
```

Expected: Simulation runs successfully, latency increases proportionally

**Step 10: Verify help text shows new parameter**

```bash
./ns3 run http-cache-scenario -- --help
```

Expected: Output includes `--objectSize` with description

**Step 11: Commit changes**

```bash
git add examples/http-cache-scenario.cc
git commit -m "feat: add --objectSize command-line parameter"
```

---

## Task 5: Update Documentation

**Files:**
- Modify: `README.md` (add parameter documentation and usage examples)

**Step 1: Add objectSize to configuration parameters table**

In `README.md`, around line 122 (in the Configuration Parameters section), add a new row:

```markdown
| `--objectSize`       | Object size in bytes                              | `1024`    |
```

**Step 2: Add usage example with different object sizes**

In `README.md`, around line 59 (in the Common Configuration section), add:

```markdown
### Object Size Examples

```bash
# Small objects (images, API responses) - 10 KB
./ns3 run http-cache-scenario -- --objectSize=10240

# Medium objects (web pages) - 100 KB
./ns3 run http-cache-scenario -- --objectSize=102400

# Large objects (videos) - 10 MB
./ns3 run http-cache-scenario -- --objectSize=10485760
```
```

**Step 3: Add explanation of transfer time relationship**

In `README.md`, add a new section explaining the relationship:

```markdown
### Transfer Time and Object Size

The simulation models realistic transfer times based on:
- **Object size** (`--objectSize`): Size of each cached object in bytes
- **Network bandwidth** (future): Link capacity affects transfer speed

Transfer time is calculated by ns-3's packet transmission simulator based on payload size.
Larger objects result in higher latency, especially on cache misses.
```

**Step 4: Build and verify documentation renders correctly**

```bash
cat README.md | grep -A 5 "objectSize"
```

Expected: All new documentation appears correctly formatted

**Step 5: Commit documentation changes**

```bash
git add README.md
git commit -m "docs: add objectSize parameter documentation and examples"
```

---

## Task 6: Final Verification and Integration Testing

**Files:**
- Test: All components together with various objectSize values

**Step 1: Run comprehensive test with small objects**

```bash
./ns3 run http-cache-scenario -- --nReq=50 --numClients=10 --cacheCap=5 --objectSize=1024
```

Expected: Simulation completes, CSV shows reasonable latencies for 1 KB objects

**Step 2: Run test with medium objects**

```bash
./ns3 run http-cache-scenario -- --nReq=50 --numClients=10 --cacheCap=5 --objectSize=102400
```

Expected: Latencies are higher than small objects test

**Step 3: Run test with large objects**

```bash
./ns3 run http-cache-scenario -- --nReq=50 --numClients=10 --cacheCap=5 --objectSize=5242880
```

Expected: Latencies are significantly higher for 5 MB objects

**Step 4: Verify CSV output captures increased latency**

```bash
tail -20 http_cache_client_data.csv
```

Expected: Latency column shows values that increase with objectSize

**Step 5: Test with large-scale simulation (from Issue #3)**

```bash
./ns3 run http-cache-scenario -- --nReq=100 --numClients=1000 --objectSize=10240
```

Expected: Simulation handles large client count with 10 KB objects

**Step 6: Create final commit if any fixes were needed**

```bash
git add .
git commit -m "test: verify object size configuration with multiple scenarios"
```

---

## Task 7: Close Issue #9

**Step 1: Verify all acceptance criteria are met**

From Issue #9:
- [x] `--objectSize` flag implemented
- [x] Object size affects packet transmission time
- [x] Default value represents typical web content (1024 bytes)
- [x] Help text includes parameter description
- [x] Documentation includes example sizes

**Step 2: Add comment to issue with test results**

```bash
gh issue comment 9 --body "✅ Implementation complete!

**Changes:**
- Added \`--objectSize\` parameter (default: 1024 bytes)
- Modified HttpClientApp, HttpCacheApp, HttpOriginApp to use configurable object size
- Updated documentation with usage examples

**Test Results:**
- Small objects (1 KB): ✅ Works correctly
- Medium objects (100 KB): ✅ Latency increases proportionally
- Large objects (10 MB): ✅ Simulation handles large payloads
- Large-scale (1000 clients): ✅ Compatible with multi-client support

**Usage:**
\`\`\`bash
./ns3 run http-cache-scenario -- --objectSize=10240  # 10 KB objects
\`\`\`

See README.md for more examples."
```

**Step 3: Close the issue**

```bash
gh issue close 9 --comment "Resolved by implementing object size configuration across all components."
```

---

## Summary

This plan implements Issue #9 by:

1. **Adding `m_objectSize` member variable** to all three components (client, cache, origin)
2. **Implementing setter methods** for each component
3. **Modifying packet creation** from `Create<Packet>(0)` to `Create<Packet>(m_objectSize)` in 4 locations
4. **Adding `--objectSize` command-line flag** with default value 1024 bytes
5. **Wiring the parameter** through the scenario file to all components
6. **Updating documentation** with usage examples and explanation

**Key Design Decisions:**

- **Default value: 1024 bytes (1 KB)** - Represents typical small web content
- **Uniform object size** - All content has same size (future enhancement: variable sizes)
- **Applied to all packets** - Requests and responses include payload (realistic HTTP)

**Dependencies Unlocked:**

This implementation enables:
- **Issue #8** (bandwidth configuration) - Transfer time = objectSize / bandwidth
- **Issue #10** (GB-based capacity) - Capacity calculation: `capacity_bytes / objectSize`

**Testing Strategy:**

- Compile after each component modification
- Test with small/medium/large objects
- Verify latency increases with object size
- Ensure compatibility with large-scale simulations (Issue #3)

---

## Execution Options

**Plan complete and saved to `docs/plans/2025-11-04-object-size-configuration.md`.**

**Two execution options:**

**1. Subagent-Driven (this session)** - I dispatch fresh subagent per task, review between tasks, fast iteration

**2. Parallel Session (separate)** - Open new session with executing-plans, batch execution with checkpoints

**Which approach?**
