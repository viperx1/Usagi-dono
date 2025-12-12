# Chain Ordering Fix - Summary

## Issue Description

Anime chains were being ordered incorrectly. For the Inuyasha series:

**Expected order:**
1. 144 - Inuyasha (first in series)
2. 6716 - Inuyasha Kanketsuhen (sequel of 144)
3. 15546 - Han'you no Yashahime (sequel of 6716)
4. 16141 - Han'you no Yashahime S2 (sequel of 15546)

**Actual order:**
15546, 144, 16141, 6716 (completely wrong!)

## Root Cause

The issue was in `usagi/src/animechain.cpp` in two places:

### Issue 1: `mergeWith()` function (lines 70-75)

```cpp
// OLD CODE - BUGGY
QSet<int> uniqueAnime(m_animeIds.begin(), m_animeIds.end());
for (int aid : other.m_animeIds) {
    uniqueAnime.insert(aid);
}
m_animeIds = uniqueAnime.values();  // ❌ BUG: returns arbitrary order!
```

**Problem:** `QSet::values()` returns anime IDs in **arbitrary order** (typically sorted by hash value). This destroys any existing chronological order before `orderChain()` is called.

### Issue 2: `orderChain()` function (lines 175-179)

```cpp
// OLD CODE - NON-DETERMINISTIC
for (int aid : m_animeIds) {
    if (inDegree[aid] == 0) {
        queue.append(aid);  // ❌ Order depends on arbitrary m_animeIds order
    }
}
```

**Problem:** When multiple anime have `inDegree == 0` (e.g., disconnected chains accidentally merged), they're processed in whatever arbitrary order they appear in `m_animeIds` (which was already randomized by Issue 1).

## Solution

### Fix 1: Preserve order during merge

```cpp
// NEW CODE - PRESERVES ORDER
QSet<int> existingAnime(m_animeIds.begin(), m_animeIds.end());
for (int aid : other.m_animeIds) {
    if (!existingAnime.contains(aid)) {
        m_animeIds.append(aid);        // ✅ Append in order
        existingAnime.insert(aid);     // ✅ Track for deduplication
    }
}
// NO call to QSet::values() - order is preserved!
```

**Benefit:** Maintains existing order from both chains before reordering.

### Fix 2: Sort roots deterministically

```cpp
// NEW CODE - DETERMINISTIC
QList<int> roots;
for (int aid : m_animeIds) {
    if (inDegree[aid] == 0) {
        roots.append(aid);
    }
}
std::sort(roots.begin(), roots.end());  // ✅ Sort by ID
queue = roots;
```

**Benefit:** When multiple roots exist (disconnected components), they're processed in a predictable order (lowest ID first).

## How It Works

The topological sort in `orderChain()` works as follows:

1. Build a directed graph where each edge represents a sequel relationship
   - Edge A→B means "A has sequel B"

2. Calculate in-degree for each anime
   - in-degree = number of prequels
   - in-degree == 0 means "this is the first in a chain"

3. Process anime in topological order:
   - Start with all roots (in-degree == 0), sorted by ID
   - For each anime processed, reduce in-degree of its sequels
   - When a sequel's in-degree reaches 0, it can be processed next

For the Inuyasha chain:
- 144 has in-degree 0 (no prequel) → processed first
- 6716 has in-degree 1 (prequel=144) → processed after 144
- 15546 has in-degree 1 (prequel=6716) → processed after 6716
- 16141 has in-degree 1 (prequel=15546) → processed last

Result: **144 → 6716 → 15546 → 16141** ✅

## Testing

Added comprehensive test suite (`tests/test_animechain.cpp`):

1. **testSimpleChainOrder**: Verify basic 3-anime chain orders correctly
2. **testMergePreservesOrder**: Verify merge maintains prequel→sequel order
3. **testInuyashaChainOrdering**: Reproduce exact bug scenario and verify fix
4. **testMultipleRootsOrdered**: Verify disconnected chains are ordered by ID
5. **testDisconnectedComponents**: Verify two separate chains merge correctly

All tests verify that after expansion and merging, chains are ordered from prequel to sequel.

## Files Changed

- `usagi/src/animechain.cpp`: Fixed `mergeWith()` and `orderChain()`
- `tests/test_animechain.cpp`: New comprehensive test suite
- `tests/CMakeLists.txt`: Added new test to build

## Testing Notes

- Tests require Qt 6.8 to build and run
- Qt 6.8 is not available in the current CI environment
- Manual testing with Qt 6.8 will be required to fully verify the fix
- The logic is sound based on code analysis and theoretical verification
