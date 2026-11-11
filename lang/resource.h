#ifndef SESH_LANG_RESOURCE_H
#define SESH_LANG_RESOURCE_H
#include "lang/workspace.h"
/* Revision is local serialized metadata, NOT a remote transaction/CAS token.
 * Zero revision is a valid baseline. Host restores through setRevision only
 * while externally excluding borrowers; submit advances it with overflow guard.
 */
typedef struct Resource { u64 id; u64 workspaceId; u64 revision; } Resource;
Resource Resource_0(void);
Resource Resource_3(u64 id, u64 workspaceId, u64 revision);
#define Resource(...) SESH_CONSTRUCT(Resource, __VA_ARGS__)
#define Resource_zero() Resource_0()
bool Resource_setId(Resource *self, u64 id);
bool Resource_setWorkspaceId(Resource *self, u64 workspaceId);
bool Resource_setRevision(Resource *self, u64 revision);
u64 Resource_getId(const Resource *self);
u64 Resource_getWorkspaceId(const Resource *self);
u64 Resource_getRevision(const Resource *self);
bool Resource_toString(const Resource *self, char *dest, bytes cap, bool *outTruncated);
bool Resource_toStringStruct(const Resource *self, char *dest, bytes cap, bool *outTruncated);
#endif
