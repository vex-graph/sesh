# sesh — Repo-Local Living Preferences

## Constitution Link

gist.md (`../../metadata/gists/preferences/gist.md`) is the compressed law list; all universal laws bind this repository. Read one on demand: `./tools/prefs law "<title>"`. Universal laws are inherited, never restated.

Tier: core (R4 Session Relay). Owns: session objects, local operation admission, snapshot backup jobs. Borrows: RE IO/NIO, Vexspoke CPU contracts, api-haven and hotcwap's one-seam window; implements no graphics of its own.

## Exclusive Laws

| Law Title | Scope | Enforcement |
| **Single-Transaction Live Coordination Law** | R4 Session Relay | Mandatory for `sesh` |
| **No-Transaction-Across-Event-Dispatch Law** | R4 Session Relay | Mandatory for `sesh` |
| **Snapshot Backup Boundary Law** | R4 explicit backup scheduling | Mandatory for `sesh` |
| **Session Composition and Identity Law** | R4 session objects | Mandatory for `sesh` |

### Single-Transaction Live Coordination Law

* On live resize, every seam-canvas and DIRECT pane-layer change lands in ONE explicit `CATransaction`.
* Frame in logical points, `drawableSize` in native px; implicit layer actions disabled.
* `presentsWithTransaction YES` makes drawable swaps atomic with their motion; blur + seam + panes commit together on one vsync.

### No-Transaction-Across-Event-Dispatch Law

* Never span `[NSApp sendEvent:]` with a transaction.
* The event pump commits and re-begins around each dispatch; AppKit commits each drag step on its runloop turn.

### Snapshot Backup Boundary Law

* `SeshSnapshot` owns job policy; borrows caller-owned staging storage and an API Haven provider callback/context.
* Admission copies bytes once; input may then be released.
* Only one active job; overlapping admission rejects observably and preserves prior bytes/state.
* Provider callbacks return within bounded slices and retain key/content across pending/retry.
* DONE means durable acknowledgement; retries are idempotent; exhaustion, rejection and cancellation release outstanding borrows before staging reuse.
* No core overall deadline exists for pending progress; caller must cancel by its deadline; teardown cancellation never logs.

### Session Composition and Identity Law

* `lang/sesh.h` owns composition via borrowed AuthService, SeshClient, Workspace, Resource and Operations references.
* `SeshClient` is prefixed to avoid collision with Graphvex's public `Client`.
* Operations is a flat copied-intent/receipt ledger in host-owned storage; queued intent is not an applied receipt.
* Count/backing/receipt flags change only through reserve/add/apply/clear, not setters.
* `Sesh_submit` validates admitted principal/client/workspace/resource scope and expected local revision, copies intent and advances local revision exactly once.
* Replays do not advance; conflicts, overflow and exhausted current storage preserve revision/history.
* Close detaches, never destroys borrows.

## Repo-Local Extensions

* None — no managed exceptions to universal laws.

## Readiness Cross-Reference

* Feature readiness: sesh readiness gist.
* Open blockers: ecosystem blockers gist.
* Proof lawbook: `tests/test-preferences.md` — `./tools/prefs test "<title>"`.
* Evidence ledger: `metadata/gists/test-checklist.md` — `python3 tools/test_checklist.py check`.
