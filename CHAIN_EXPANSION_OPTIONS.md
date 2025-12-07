# Chain Expansion Options

## Requirement
"Allow chain logic to add anime to view" - display complete anime chains even when some chain members aren't in the filtered mylist.

## Current Behavior
Chain mode only groups and sorts anime that are ALREADY in your filtered mylist. If you have anime S (sequel) in your list but not anime P (prequel), chain mode shows only S.

## Proposed Options

### Option A: Expand Chains to Include All Relations
**What it does**: When chain mode is enabled, automatically add related anime (prequels/sequels) to the view even if they're not in your mylist.

**Example**:
- Your mylist: [Anime C (Season 3)]
- Full chain: [Anime A (Season 1), Anime B (Season 2), Anime C (Season 3)]
- **Chain mode shows**: A → B → C (all three, even though only C is in mylist)

**Visual indicator**: Different styling for anime not in mylist (grayed out, different border, label "Not in MyList")

**Pros**:
- See complete series at a glance
- Discover missing seasons/prequels
- Better context for where you are in a series

**Cons**:
- More complex to implement
- Could clutter view with many anime
- Need to handle anime without cached data (requires DB query)
- Filtering becomes ambiguous (are added anime subject to filters?)

### Option B: Expand Only Direct Relations
**What it does**: Only add immediate prequel/sequel if they're in the database (but not necessarily in mylist).

**Example**:
- Your mylist: [Anime B (Season 2)]
- Direct relations: A (prequel), C (sequel)
- **Chain mode shows**: A → B → C

Does NOT recursively follow the entire chain, just one level.

**Pros**:
- Simpler to implement
- Less clutter
- Still shows immediate context

**Cons**:
- Incomplete chains for long series
- Still requires handling uncached anime

### Option C: Show Full Chain, Disable Incomplete Entries
**What it does**: Display full chains with grayed-out placeholders for anime not in mylist. Can't click/interact with placeholders.

**Example**:
- Your mylist: [Anime C]
- **Chain mode shows**: [A] → [B] → C (A and B are grayed placeholders with titles only)

**Pros**:
- Visual continuity of the chain
- Shows what's missing
- Minimal data loading (just titles)

**Cons**:
- Still need to query anime titles for non-mylist anime
- Can't interact with placeholders

### Option D: Show Chain Position Indicator Only
**What it does**: Keep current behavior but add visual indicator showing position in chain.

**Example**:
- Your mylist: [Anime C]
- **Chain mode shows**: C with badge "3/5 in series" or "Season 3"

**Pros**:
- Minimal changes
- No additional anime in view
- Shows context without clutter

**Cons**:
- Doesn't add anime to view (may not meet requirement)

## Recommendation

**Recommended: Option A with Smart Filtering**

Implement Option A with these constraints:
1. Only expand chains for anime already passing filters
2. Added anime inherit the "parent" anime's filter status
3. Visual distinction: border color or badge for non-mylist anime
4. Limit expansion depth (max 10 anime per chain to prevent performance issues)
5. Cache expanded anime data to avoid repeated queries

**Implementation approach**:
1. Modify `buildChainFromAid()` to NOT require `availableAids` check (optional parameter)
2. Add `expandChains` flag to `buildChainsFromAnimeIds()`
3. Query database for relation anime when not in cache
4. Add `isInMyList` flag to AnimeChain or track separately
5. Update card rendering to show non-mylist anime differently

## Questions to Resolve

1. **Which option do you prefer?**
2. **If Option A**: Should added anime be subject to filters (type, completion, etc.)?
3. **Visual distinction**: How should non-mylist anime look different?
4. **Interaction**: Can users click on non-mylist anime to view details/add to mylist?
5. **Performance**: Set a max chain length limit? (suggest 10-15 anime)

Please specify which option and answer the questions so I can implement accordingly.
