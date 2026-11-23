#include "vex.h"
#include "lang/auth_service.h"
#include "session/text.h"
;;DEFINITION
/* AuthService binds borrowed API Haven credentials to a trusted verifier. A
 * principal is admitted only through successful verification, never merely by
 * possessing an API key string. Authentication failure revokes local identity.
 * Host/API owns actual verification, secure storage, expiry and issuer mapping.
 */
;;OVERVIEW
/* CLASS: AuthService. STRUCT FIELDS: const ApiAuth *auth: borrowed credential
 * descriptor; AuthServiceVerifyFn verifier: trusted bounded identity callback;
 * any context: borrowed verifier context; u64 principalId: admitted identity.
 * PUBLIC: _0/_3/chooser/zero; setAuth/setVerifier/setContext (reject missing required inputs; successful
 * reconfiguration revokes principal); getAuth/Verifier/Context/PrincipalId;
 * isAuthenticated; authenticate (failure revokes); revoke (quiet/idempotent);
 * toString/toStringStruct (credentials and context redacted, no dereference).
 * No private helpers. External serialization; verification is not sandboxed.
 */
;;INTENTION("per the Conflict Triage Law + Single Class Per File Law (Java Law): principalId has no arbitrary setter; authenticate/revoke alone establish verification state")
/** Returns an empty service with no credentials, verifier, or admitted principal. */
AuthService AuthService_0()
{
    return (AuthService) {0};
}
/** Binds borrowed credentials and verifier state; rejects missing required inputs. */
AuthService AuthService_3(const ApiAuth *auth, AuthServiceVerifyFn verifier, any context) {
    AuthService result = AuthService_0();
    if (auth == nullptr || verifier == nullptr) {
        THROW("auth service: missing credential or verifier");
        return result;
    }
    result.auth = auth;
    result.verifier = verifier;
    result.context = context;
    return result;
}
;;SETTER
static inline fn AuthService_setPrincipalId(AuthService *self, u64 principalId) {
    (*self).principalId = principalId;
}

/** Replaces borrowed credentials and clears any previously admitted principal. */
bool AuthService_setAuth(AuthService *self, const ApiAuth *auth) {
    if (self == nullptr || auth == nullptr) {
        THROW("auth service credentials: null input");
        return false;
    }
    (*self).auth = auth;
    AuthService_setPrincipalId(self, 0);
    return true;
}
/** Replaces the verifier and its borrowed context, revoking the current identity. */
bool AuthService_setVerifier(AuthService *self, AuthServiceVerifyFn verifier, any context) {
    if (self == nullptr || verifier == nullptr) {
        THROW("auth service verifier: null input");
        return false;
    }
    (*self).verifier = verifier;
    return AuthService_setContext(self, context);
}
/** Replaces the borrowed verifier context and revokes the current identity. */
bool AuthService_setContext(AuthService *self, any context) {
    if (self == nullptr) {
        THROW("auth service context: null service");
        return false;
    }
    (*self).context = context;
    AuthService_setPrincipalId(self, 0);
    return true;
}
/** Returns the borrowed credential descriptor, or nullptr for a null service. */
;;GETTER
const ApiAuth *AuthService_getAuth(const AuthService *self)
{
    return self == nullptr ? nullptr : (*self).auth;
}
/** Returns the configured verifier callback, or nullptr for a null service. */
;;GETTER
AuthServiceVerifyFn AuthService_getVerifier(const AuthService *self)
{
    return self == nullptr ? nullptr : (*self).verifier;
}
/** Returns the borrowed verifier context, or nullptr for a null service. */
;;GETTER
any AuthService_getContext(const AuthService *self)
{
    return self == nullptr ? nullptr : (*self).context;
}
/** Returns the admitted principal ID, or zero for a null service. */
;;GETTER
u64 AuthService_getPrincipalId(const AuthService *self)
{
    return self == nullptr ? 0 : (*self).principalId;
}
/** Reports whether a nonzero principal has been admitted. */
bool AuthService_isAuthenticated(const AuthService *self)
{
    return AuthService_getPrincipalId(self) != 0;
}
/** Clears the admitted principal; null input is ignored. */
fn AuthService_revoke(AuthService *self) {
    if (self != nullptr)
        AuthService_setPrincipalId(self, 0);
}
/** Calls the configured verifier; admits its nonzero principal or revokes on failure. */
bool AuthService_authenticate(AuthService *self) {
    const ApiAuth *auth = AuthService_getAuth(self);
    AuthServiceVerifyFn verifier = AuthService_getVerifier(self);
    if (self == nullptr || auth == nullptr || verifier == nullptr) {
        AuthService_revoke(self);
        THROW("auth service authenticate: unconfigured");
        return false;
    }
    u64 principalId = 0;
    if (!verifier(auth, AuthService_getContext(self), &principalId) || principalId == 0) {
        AuthService_revoke(self);
        THROW("auth service authenticate: rejected identity");
        return false;
    }
    AuthService_setPrincipalId(self, principalId);
    return true;
}
/** Formats the admitted principal as a bounded value summary. */
SESH_VALUE_STRING(AuthService, SeshText_format(dest, cap, outTruncated, "principal %llu", (u64) AuthService_getPrincipalId(self)))
/** Formats one-level fields while redacting credential and context contents. */
bool AuthService_toStringStruct(const AuthService *self, utf8 *dest, bytes cap, bool *outTruncated) {
    return self == nullptr ? SeshText_format(dest, cap, outTruncated, "nullptr") :
        SeshText_format(dest, cap, outTruncated, "AuthService{auth=%s,verifier=%s,context=%s,principalId=%llu}",
                        AuthService_getAuth(self) == nullptr ? "nullptr" : "redacted", AuthService_getVerifier(self) == nullptr ? "nullptr" : "bound",
                        AuthService_getContext(self) == nullptr ? "nullptr" : "redacted", (u64) AuthService_getPrincipalId(self));
}
