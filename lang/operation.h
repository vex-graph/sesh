#ifndef SESH_LANG_OPERATION_H
#define SESH_LANG_OPERATION_H
#include "lang/resource.h"
/* Immutable intent once copied into Operations: identity is (clientId,id).
 * The first operation form is a local revision advance, not file/SQL payload.
 * UINT64_MAX expectedRevision cannot advance and rejects in Sesh_submit.
 */
typedef struct Operation {
    u64 id;
    u64 clientId;
    u64 workspaceId;
    u64 resourceId;
    u64 expectedRevision;
} Operation;
Operation Operation_0(void);
Operation Operation_5(u64 id, u64 clientId, u64 workspaceId, u64 resourceId, u64 expectedRevision);
#define Operation(...) SESH_CONSTRUCT(Operation, __VA_ARGS__)
#define Operation_zero() Operation_0()
bool Operation_setId(Operation *self, u64 id);
bool Operation_setClientId(Operation *self, u64 clientId);
bool Operation_setWorkspaceId(Operation *self, u64 workspaceId);
bool Operation_setResourceId(Operation *self, u64 resourceId);
bool Operation_setExpectedRevision(Operation *self, u64 expectedRevision);
u64 Operation_getId(const Operation *self);
u64 Operation_getClientId(const Operation *self);
u64 Operation_getWorkspaceId(const Operation *self);
u64 Operation_getResourceId(const Operation *self);
u64 Operation_getExpectedRevision(const Operation *self);
bool Operation_isValid(const Operation *self);
bool Operation_equals(const Operation *left, const Operation *right);
bool Operation_toString(const Operation *self, char *dest, bytes cap, bool *outTruncated);
bool Operation_toStringStruct(const Operation *self, char *dest, bytes cap, bool *outTruncated);
#endif
