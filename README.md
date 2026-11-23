# sesh

R4 session composition and caller-buffer snapshot core for Vexgraph: local revision admission, copied intent/receipt ledger, and explicit snapshot backup jobs.

```bash
python3 tests/sesh/run.py
```

## About

`sesh` provides the local session state machine and snapshot staging core beneath future cloud backup and collaboration workflows. It borrows credentials, backing storage, and provider callbacks from the host and `api-haven`, owning no allocator, socket, filesystem, worker thread, secret, or OS clock itself.

## Features

- **Session composition (`src/lang/`, `src/session/`, entry `lang/sesh.h`)**:
  - `AuthService`: borrows `api-haven` credentials and verifies a principal via an injected verifier; reconfiguration or failure revokes identity.
  - `SeshClient` (`lang/client.h`): host-assigned installation identity (coexists with `graphvex`'s `Client`).
  - `Workspace` & `Resource`: private principal-owned project identity and local revision metadata.
  - `Operation` & `Operations`: copied intent records and a flat, caller-backed growable intent/receipt ledger (`reserve` copies into larger caller storage).
  - `Sesh`: coordinates auth/scope checks, stale-revision conflict rejection (`SESH_CONFLICT`), duplicate replay detection (`SESH_REPLAY`), and local revision advance (`SESH_APPLIED`).
- **Snapshot backup core (`src/snapshot/snapshot.{h,c}`)**: Caller-owned staging buffer for one upload job at a time with nonzero job identity across retries, configurable retry delay/exhaustion, cancellation before releasing borrows, and injected `api-haven` `storage/snapshot_io.h` put/get callbacks.

## Usage example

```c
Sesh session = Sesh(authService, client, workspace, resource, operations);
Operation intent = Operation(operationId, SeshClient_getId(client),
                             Workspace_getId(workspace), Resource_getId(resource),
                             Resource_getRevision(resource));
int result = Sesh_submit(&session, &intent);
```

## Build and test

From the `worktree/` root:

```bash
python3 tests/sesh/run.py
```

Runs ten session owner targets plus the snapshot owner under strict C23 and ASan/UBSan, and a four-client synchronized TSan run on macOS arm64.

## Future

- Implement `FileSession` and `DirectorySession` (`sync`, `clone`, `rebase`) composed over `Sesh` once R2 directory enumeration and verified TLS land.
- Wire Google Drive (`drive.file` scope + OAuth desktop flow) first, followed by iCloud/CloudKit.
- Add durable local journaling and planned networked collaboration (`seshd` relay, presence, telemetry/crash ingestion).

## Scope and Limitations

- **Offline core only**: No Google Drive, iCloud, HTTPS/OAuth, durable journal, `seshd` relay, Cloudflare edge, or realtime presence exists yet; snapshot tests use an in-memory fake.
- **Local intent only**: `SESH_APPLIED` confirms a local revision advance, never remote upload or distributed commit.
- **External serialization required**: All operations across sessions, resources, auth, and ledgers require one shared external lock; no internal mutex or distributed CAS exists.
- **macOS arm64 only**: Offline C23/ASan/UBSan/TSan proof is macOS arm64 only; Windows/Linux and live host integration are unproven.
