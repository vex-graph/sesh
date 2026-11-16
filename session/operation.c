#include "vex.h"
#include "lang/operation.h"
#include "session/text.h"
;;DEFINITION
/* Operation carries a client-scoped idempotency identity and workspace/resource
 * scope plus expected local revision. It owns no payload or identity storage.
 * A ledger copies it, so later edits to this caller value cannot alter history.
 */
;;OVERVIEW
/* CLASS: Operation. STRUCT FIELDS: u64 id: client-local unique operation;
 * u64 clientId: origin; u64 workspaceId: namespace; u64 resourceId:
 * target; u64 expectedRevision: optimistic local baseline.
 * PUBLIC: _0/_5/chooser/zero; setId/ClientId/WorkspaceId/ResourceId (null/zero reject);
 * setExpectedRevision (null rejects, zero/max representable); corresponding getters
 * (null=0); isValid (nonzero identities); equals (null=false); bounded projections.
 * No private helpers. Setters preserve rejection state; external serialization.
 */
// Returns an empty operation value with all identity fields unset.
Operation Operation_0() { return (Operation) {0}; }
// Constructs an operation when all required scope IDs are nonzero.
Operation Operation_5(u64 id, u64 clientId, u64 workspaceId, u64 resourceId, u64 expectedRevision) {
    Operation result = {id, clientId, workspaceId, resourceId, expectedRevision};
    if (!Operation_isValid(&result)) {
        THROW("operation identity: zero ID");
        return Operation_0();
    }
    return result;
}
// Sets the operation's own nonzero id.
;;SETTER
bool Operation_setId(Operation *self, u64 value) {
    if (self == nullptr || value == 0) { THROW("operation identity: null or zero ID"); return false; }
    (*self).id = value;
    return true;
}
// Sets the originating client's nonzero id.
;;SETTER
bool Operation_setClientId(Operation *self, u64 value) {
    if (self == nullptr || value == 0) { THROW("operation identity: null or zero ID"); return false; }
    (*self).clientId = value;
    return true;
}
// Sets the workspace's nonzero scope id.
;;SETTER
bool Operation_setWorkspaceId(Operation *self, u64 value) {
    if (self == nullptr || value == 0) { THROW("operation identity: null or zero ID"); return false; }
    (*self).workspaceId = value;
    return true;
}
// Sets the target resource's nonzero id.
;;SETTER
bool Operation_setResourceId(Operation *self, u64 value) {
    if (self == nullptr || value == 0) { THROW("operation identity: null or zero ID"); return false; }
    (*self).resourceId = value;
    return true;
}
// Sets the expected revision; zero is valid and null self is rejected.
bool Operation_setExpectedRevision(Operation *self, u64 expectedRevision) {
    if (self == nullptr) {
        THROW("operation revision: null operation");
        return false;
    }
    (*self).expectedRevision = expectedRevision;
    return true;
}
// Returns the operation ID, or zero for a null value.
;;GETTER
u64 Operation_getId(const Operation *self) { return self == nullptr ? 0 : (*self).id; }
// Returns the originating client ID, or zero for a null value.
;;GETTER
u64 Operation_getClientId(const Operation *self) { return self == nullptr ? 0 : (*self).clientId; }
// Returns the workspace scope ID, or zero for a null value.
;;GETTER
u64 Operation_getWorkspaceId(const Operation *self) { return self == nullptr ? 0 : (*self).workspaceId; }
// Returns the target resource ID, or zero for a null value.
;;GETTER
u64 Operation_getResourceId(const Operation *self) { return self == nullptr ? 0 : (*self).resourceId; }
// Returns the expected local revision, or zero for a null value.
;;GETTER
u64 Operation_getExpectedRevision(const Operation *self) { return self == nullptr ? 0 : (*self).expectedRevision; }
// Checks that all required identity and scope IDs are nonzero.
bool Operation_isValid(const Operation *self) {
    return self != nullptr && Operation_getId(self) != 0 && Operation_getClientId(self) != 0 &&
        Operation_getWorkspaceId(self) != 0 && Operation_getResourceId(self) != 0;
}
// Compares all identity, scope, and expected-revision fields.
bool Operation_equals(const Operation *left, const Operation *right) {
    return left != nullptr && right != nullptr && Operation_getId(left) == Operation_getId(right) &&
        Operation_getClientId(left) == Operation_getClientId(right) && Operation_getWorkspaceId(left) == Operation_getWorkspaceId(right) &&
        Operation_getResourceId(left) == Operation_getResourceId(right) && Operation_getExpectedRevision(left) == Operation_getExpectedRevision(right);
}
// Formats client/operation identity as a bounded value summary.
SESH_VALUE_STRING(Operation, SeshText_format(dest, cap, outTruncated, "operation %llu/%llu", (u64) Operation_getClientId(self), (u64) Operation_getId(self)))
// Formats all operation fields into a bounded structure summary.
bool Operation_toStringStruct(const Operation *self, char *dest, bytes cap, bool *outTruncated) {
    return self == nullptr ? SeshText_format(dest, cap, outTruncated, "nullptr") :
        SeshText_format(dest, cap, outTruncated, "Operation{id=%llu,clientId=%llu,workspaceId=%llu,resourceId=%llu,expectedRevision=%llu}",
                        (u64) Operation_getId(self), (u64) Operation_getClientId(self),
                        (u64) Operation_getWorkspaceId(self), (u64) Operation_getResourceId(self),
                        (u64) Operation_getExpectedRevision(self));
}
