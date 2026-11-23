#include "vex.h"
#include "lang/sesh.h"
#include "session/text.h"
;;DEFINITION
/* Sesh composes independently owned authentication, installation, workspace,
 * resource and operation-history objects. Cold submit validates that verified
 * principal, client and namespace agree, deduplicates immutable intent, rejects
 * stale revisions, and advances one local revision under external serialization.
 * It neither executes a network transfer nor owns database transactions. The
 * same model can later bind real providers without making local counters a
 * distributed atomicity claim. Closing merely detaches borrowed objects.
 */
;;OVERVIEW
/* CLASS: Sesh. STRUCT FIELDS: AuthService *authService: borrowed verified identity;
 * SeshClient *client: borrowed installation; Workspace *workspace: borrowed namespace;
 * Resource *resource: borrowed target/revision; Operations *operations: borrowed ledger.
 * PUBLIC: _0/_5/chooser/zero; setAuthService/Client/Workspace/Resource/Operations
 * (null rejects unchanged; binding alone admits no operation); matching getters
 * (null safe); submit (applied/replay/conflict/rejected); close (quiet/idempotent);
 * toString/toStringStruct (child VALUE projections, never credential dumps).
 * No private helpers. Every shared component requires external serialization.
 */
/** Returns an unbound session composition. */
Sesh Sesh_0()
{
    return (Sesh) {0};
}
/** Binds all borrowed components; rejects any missing component. */
Sesh Sesh_5(AuthService *authService, SeshClient *client, Workspace *workspace, Resource *resource, Operations *operations) {
    if (authService == nullptr || client == nullptr || workspace == nullptr || resource == nullptr || operations == nullptr) {
        THROW("sesh binding: missing component");
        return Sesh_0();
    }
    return (Sesh) {authService, client, workspace, resource, operations};
}
/** Binds and retrieves the borrowed authentication service. */
;;SETTER
bool Sesh_setAuthService(Sesh *self, AuthService *value) {
    if (self == nullptr || value == nullptr) { THROW("sesh binding: null component"); return false; }
    (*self).authService = value; return true;
}
;;GETTER
AuthService *Sesh_getAuthService(const Sesh *self)
{
    return self == nullptr ? nullptr : (*self).authService;
}
/** Binds and retrieves the borrowed installation identity. */
;;SETTER
bool Sesh_setClient(Sesh *self, SeshClient *value) {
    if (self == nullptr || value == nullptr) { THROW("sesh binding: null component"); return false; }
    (*self).client = value; return true;
}
;;GETTER
SeshClient *Sesh_getClient(const Sesh *self)
{
    return self == nullptr ? nullptr : (*self).client;
}
/** Binds and retrieves the borrowed workspace namespace. */
;;SETTER
bool Sesh_setWorkspace(Sesh *self, Workspace *value) {
    if (self == nullptr || value == nullptr) { THROW("sesh binding: null component"); return false; }
    (*self).workspace = value; return true;
}
;;GETTER
Workspace *Sesh_getWorkspace(const Sesh *self)
{
    return self == nullptr ? nullptr : (*self).workspace;
}
/** Binds and retrieves the borrowed resource/revision record. */
;;SETTER
bool Sesh_setResource(Sesh *self, Resource *value) {
    if (self == nullptr || value == nullptr) { THROW("sesh binding: null component"); return false; }
    (*self).resource = value; return true;
}
;;GETTER
Resource *Sesh_getResource(const Sesh *self)
{
    return self == nullptr ? nullptr : (*self).resource;
}
/** Binds and retrieves the borrowed operation-history ledger. */
;;SETTER
bool Sesh_setOperations(Sesh *self, Operations *value) {
    if (self == nullptr || value == nullptr) { THROW("sesh binding: null component"); return false; }
    (*self).operations = value; return true;
}
;;GETTER
Operations *Sesh_getOperations(const Sesh *self)
{
    return self == nullptr ? nullptr : (*self).operations;
}
/** Validates identity/scope and revision, records intent, then advances one local revision. */
i32 Sesh_submit(Sesh *self, const Operation *operation) {
    if (self == nullptr || !Operation_isValid(operation)) {
        THROW("sesh submit: invalid operation");
        return SESH_REJECTED;
    }
    AuthService *auth = Sesh_getAuthService(self);
    SeshClient *client = Sesh_getClient(self);
    Workspace *workspace = Sesh_getWorkspace(self);
    Resource *resource = Sesh_getResource(self);
    Operations *operations = Sesh_getOperations(self);
    if (auth == nullptr || client == nullptr || workspace == nullptr || resource == nullptr || operations == nullptr ||
        !AuthService_isAuthenticated(auth) || AuthService_getPrincipalId(auth) != Workspace_getPrincipalId(workspace) ||
        Operation_getClientId(operation) != SeshClient_getId(client) ||
        Operation_getWorkspaceId(operation) != Workspace_getId(workspace) ||
        Resource_getWorkspaceId(resource) != Workspace_getId(workspace) ||
        Operation_getResourceId(operation) != Resource_getId(resource)) {
        THROW("sesh submit: unauthenticated or wrong identity scope");
        return SESH_REJECTED;
    }
    Operation previous;
    if (Operations_find(operations, Operation_getClientId(operation), Operation_getId(operation), &previous)) {
        if (!Operation_equals(operation, &previous)) {
            THROW("sesh submit: identity reused with changed intent");
            return SESH_REJECTED;
        }
        if (Operations_isApplied(operations, Operation_getClientId(operation), Operation_getId(operation)))
            return SESH_REPLAY;
    }
    u64 revision = Resource_getRevision(resource);
    if (Operation_getExpectedRevision(operation) != revision) {
        THROW("sesh submit: revision conflict");
        return SESH_CONFLICT;
    }
    if (revision == UINT64_MAX) {
        THROW("sesh submit: revision overflow");
        return SESH_REJECTED;
    }
    if (Operations_add(operations, operation) == OPERATIONS_REJECTED)
        return SESH_REJECTED;
    return Operations_apply(operations, operation, resource) == OPERATIONS_ADDED ? SESH_APPLIED : SESH_REJECTED;
}
/** Detaches all borrowed components without destroying or mutating them. */
fn Sesh_close(Sesh *self) {
    if (self != nullptr)
        *self = Sesh_0();
}
/** Formats the bound workspace and resource identities as a bounded summary. */
SESH_VALUE_STRING(Sesh, SeshText_format(dest, cap, outTruncated, "sesh workspace %llu resource %llu", (u64) Workspace_getId(Sesh_getWorkspace(self)), (u64) Resource_getId(Sesh_getResource(self))))
/** Formats bounded one-level VALUE projections of the borrowed components. */
bool Sesh_toStringStruct(const Sesh *self, utf8 *dest, bytes cap, bool *outTruncated) {
    if (self == nullptr)
        return SeshText_format(dest, cap, outTruncated, "nullptr");
    utf8 auth[96], client[96], workspace[96], resource[96], operations[96];
    if (!AuthService_toString(Sesh_getAuthService(self), auth, sizeof(auth), nullptr) ||
        !SeshClient_toString(Sesh_getClient(self), client, sizeof(client), nullptr) ||
        !Workspace_toString(Sesh_getWorkspace(self), workspace, sizeof(workspace), nullptr) ||
        !Resource_toString(Sesh_getResource(self), resource, sizeof(resource), nullptr) ||
        !Operations_toString(Sesh_getOperations(self), operations, sizeof(operations), nullptr))
        return false;
    return SeshText_format(dest, cap, outTruncated, "Sesh{authService=%s,client=%s,workspace=%s,resource=%s,operations=%s}",
                           auth, client, workspace, resource, operations);
}
