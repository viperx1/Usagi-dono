# Issue #992 Fix Attempts

| Timestamp (UTC) | Attempt | Change | Result |
|---|---|---|---|
| 2026-02-24T01:56:40Z | Analyze CI logs | Read issue #992 details/comments via GitHub MCP and extracted failing assertions (`spy.count()==2`, expected `1`) in `test_missing_anime_data_request` variants. | Root cause still points to duplicate anime metadata request path. |
| 2026-02-24T01:48:43Z | API-level in-flight dedupe | Added `AniDBApi::Anime(aid)` in-flight dedupe and stale-timeout cleanup in `anidbapi.cpp`; clear in-flight on `230 ANIME`. | Did not resolve issue #992 failures (based on issue logs). |
| 2026-02-24T01:56:40Z | Add deeper in-flight diagnostics | Added detailed AniDB logs for in-flight insert, duplicate-block age/size, and clear-on-230 with set size. | Pending CI rerun evidence. |
| 2026-02-24T02:04:16Z | Issue #993 follow-up diagnostics | Added manager-level log of `AniDBApi::Anime(aid)` returned tag and explicit marker detection for duplicate-block return values. | Pending CI rerun evidence. |

## Current hypothesis

Duplicate request count likely occurs before/around ANIME command dispatch and may involve rapid multi-path invocation. Additional targeted logging is needed to correlate manager-level request path and API-level dedupe path in the same failing run.
