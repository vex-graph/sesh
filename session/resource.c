#include "lang/resource.h"
#include "session/text.h"
#include "annotation/annotation.h"
;;DEFINITION
/* Resource names one workspace-scoped synchronization target and its local
 * revision. It does not own a file, database row, lock, backend or data bytes.
 */
;;OVERVIEW
/* CLASS: Resource. STRUCT FIELDS: u64 id: target; u64 workspaceId:
 * owning namespace; u64 revision: local baseline, zero valid.
 * PUBLIC: _0/_3/chooser/zero; setId/setWorkspaceId (reject zero/null);
 * setRevision (null rejects); getId/getWorkspaceId/getRevision (null=0);
 * toString/toStringStruct. Setters preserve state on rejection. No private helpers.
 * All mutations require external serialization with sessions and consumers.
 */
// Returns an empty resource with no identity or workspace scope.
Resource Resource_0(void) { return (Resource) {0}; }
// Constructs a workspace-scoped resource with its supplied local revision.
Resource Resource_3(u64 id, u64 workspaceId, u64 revision) {
    Resource result = Resource_0();
    if (id == 0 || workspaceId == 0) {
        THROW("resource identity: zero ID");
        return result;
    }
    result.id = id;
    result.workspaceId = workspaceId;
    result.revision = revision;
    return result;
}
// Sets a nonzero resource ID; invalid input preserves the previous value.
bool Resource_setId(Resource *self, u64 id) {
    if (self == nullptr || id == 0) {
        THROW("resource identity: null or zero ID");
        return false;
    }
    (*self).id = id;
    return true;
}
// Sets a nonzero workspace ID; invalid input preserves the previous value.
bool Resource_setWorkspaceId(Resource *self, u64 workspaceId) {
    if (self == nullptr || workspaceId == 0) {
        THROW("resource workspace: null or zero ID");
        return false;
    }
    (*self).workspaceId = workspaceId;
    return true;
}
// Sets the local revision, including zero, when self is nonnull.
bool Resource_setRevision(Resource *self, u64 revision) {
    if (self == nullptr) {
        THROW("resource revision: null resource");
        return false;
    }
    (*self).revision = revision;
    return true;
}
// Returns the resource ID, or zero for null.
SESH_GETTER(Resource, u64, Id, id, 0)
// Returns the owning workspace ID, or zero for null.
SESH_GETTER(Resource, u64, WorkspaceId, workspaceId, 0)
// Returns the local revision, or zero for null.
SESH_GETTER(Resource, u64, Revision, revision, 0)
// Formats resource identity and revision as a bounded value summary.
SESH_VALUE_STRING(Resource, SeshText_format(dest, cap, outTruncated, "resource %llu revision %llu", (unsigned long long) (*self).id, (unsigned long long) (*self).revision))
// Formats the resource's fields into a bounded structure summary.
bool Resource_toStringStruct(const Resource *self, char *dest, bytes cap, bool *outTruncated) {
    return self == nullptr ? SeshText_format(dest, cap, outTruncated, "nullptr") :
        SeshText_format(dest, cap, outTruncated, "Resource{id=%llu,workspaceId=%llu,revision=%llu}",
                        (unsigned long long) (*self).id, (unsigned long long) (*self).workspaceId, (unsigned long long) (*self).revision);
}
