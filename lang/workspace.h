#ifndef SESH_LANG_WORKSPACE_H
#define SESH_LANG_WORKSPACE_H

#include "vex.h"
#include "lang/client.h"
/* One private principal-owned workspace. Sharing/roles are future contracts,
 * not implied by matching a caller-supplied ID. IDs are live metadata, not secrets.
 */
typedef struct Workspace { u64 id; u64 principalId; } Workspace;
Workspace Workspace_0();
Workspace Workspace_2(u64 id, u64 principalId);
#define Workspace(...) SESH_CONSTRUCT(Workspace, __VA_ARGS__)
#define Workspace_zero() Workspace_0()
bool Workspace_setId(Workspace *self, u64 id);
bool Workspace_setPrincipalId(Workspace *self, u64 principalId);
u64 Workspace_getId(const Workspace *self);
u64 Workspace_getPrincipalId(const Workspace *self);
bool Workspace_toString(const Workspace *self, utf8 *dest, bytes cap, bool *outTruncated);
bool Workspace_toStringStruct(const Workspace *self, utf8 *dest, bytes cap, bool *outTruncated);
#endif
