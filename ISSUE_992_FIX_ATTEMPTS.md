# Issue #992 Fix Attempts

| Timestamp (UTC) | Attempt | Change | Result |
|---|---|---|---|
| 2026-02-24T01:56:40Z | Analyze CI logs | Read issue #992 details/comments via GitHub MCP and extracted failing assertions (`spy.count()==2`, expected `1`) in `test_missing_anime_data_request` variants. | Root cause still points to duplicate anime metadata request path. |
| 2026-02-24T01:48:43Z | API-level in-flight dedupe | Added `AniDBApi::Anime(aid)` in-flight dedupe and stale-timeout cleanup in `anidbapi.cpp`; clear in-flight on `230 ANIME`. | Did not resolve issue #992 failures (based on issue logs). |
| 2026-02-24T01:56:40Z | Add deeper in-flight diagnostics | Added detailed AniDB logs for in-flight insert, duplicate-block age/size, and clear-on-230 with set size. | Pending CI rerun evidence. |
| 2026-02-24T02:04:16Z | Issue #993 follow-up diagnostics | Added manager-level log of `AniDBApi::Anime(aid)` returned tag and explicit marker detection for duplicate-block return values. | Pending CI rerun evidence. |
| 2026-02-24T02:11:00Z | Issue #994 identity diagnostics | Added `MyListCardManager` instance/adbapi pointer identity logs on construction and at request entry, including adbapi-null abort logging. | Pending CI rerun evidence; intended to confirm whether duplicate requests come from multiple manager instances. |
| 2026-02-24T02:54:29Z | Issue #995 queue/emit diagnostics | Added AniDB logs for ANIME packet queue success/failure (with AID+tag), 230 ANIME re-request queue with AID+tag, and `notifyAnimeUpdated` emit with AID+tag. | Pending CI rerun evidence; intended to reveal whether duplicate count comes from re-request/emit path. |
| 2026-02-24T12:41:41Z | Issue #996 build fix | Fixed compile errors in `AniDBApi::Mylist(int)` where new debug logs incorrectly referenced undefined `aid`; switched logs to correct `lid` and `MYLIST` wording. | Build-failure root cause addressed (compile error on multiple test targets). |
| 2026-02-24T12:52:35Z | Issue #997 request sequence tracing | Added monotonic request sequence IDs to `MyListCardManager::requestAnimeMetadata` logs and propagated them through local dedupe/global dedupe/dispatch/return logs. | Pending CI rerun evidence; intended to prove whether duplicate count comes from two independent request entries or downstream duplicate handling. |
| 2026-02-24T13:04:40Z | Issue #998 chain-preload boundary diagnostics | Added chain-preload start/no-op logs and per-missing-AID `alreadyRequested` trace to confirm whether second request path originates from chain-preload discovery vs other caller. | Pending CI rerun evidence. |

## Current hypothesis

Duplicate request count likely occurs before/around ANIME command dispatch and may involve rapid multi-path invocation. Additional targeted logging is needed to correlate manager-level request path and API-level dedupe path in the same failing run.
