/**
 * Consolidated automated owner test suite for vex/sesh.
 * Usage: sesh_test [snapshot|arity|interop|auth_service|client|operation|operations|resource|sesh|text|workspace]
 */

#include "vex.h"
#include "re/primitive/vex-string.h"
#include "snapshot/snapshot.h"
#include "test/snapshot/snapshot_test.h"
#include <assert.h>
#include <stdlib.h>
#include "lang/arity.h"
#include "graphvex/api/graphvex.h"
#include "lang/sesh.h"
#include "test/session/support.h"
#include "session/text.h"
#include <stdio.h>
#include <string.h>
#include "test/sesh_test.h"

/** Suite: snapshot (from snapshot/snapshot_test.c). */
/* SeshSnapshot owner: a user explicitly saves bytes, survives transient failure,
 * and retrieves identical content through the API Haven provider vocabulary.
 * All public forms/getters, cold rejection preservation, empty/exact/overflow,
 * retry timing/exhaustion, pending/cancel and string boundaries are exercised.
 * External runner supplies watchdog + ASan/UBSan. Provider is an in-memory fake,
 * not Google or durable disk. Thread safety, stale pointer admission, schema
 * migration and concurrent merge are not offered. Storage is caller-owned.
 */


;;GETTER
static inline u8 *Fake_bytes(Fake *self)
{
    return (*self).bytes;
}
;;GETTER
static inline bytes Fake_length(const Fake *self)
{
    return (*self).length;
}
;;SETTER
static inline fn Fake_setLength(Fake *self, bytes length)
{
    (*self).length = length;
}
;;GETTER
static inline u64 Fake_key(const Fake *self)
{
    return (*self).key;
}
;;SETTER
static inline fn Fake_setKey(Fake *self, u64 key)
{
    (*self).key = key;
}
;;GETTER
static inline u32 Fake_calls(const Fake *self)
{
    return (*self).calls;
}
;;SETTER
static inline fn Fake_setCalls(Fake *self, u32 calls)
{
    (*self).calls = calls;
}
;;GETTER
static inline u32 Fake_cancels(const Fake *self)
{
    return (*self).cancels;
}
;;SETTER
static inline fn Fake_setCancels(Fake *self, u32 cancels)
{
    (*self).cancels = cancels;
}
;;GETTER
static inline i32 Fake_result(const Fake *self)
{
    return (*self).result;
}

/* Fake provider stores completed snapshots and records cancellation requests. */
static i32 put(any context, u64 key, const u8 *data, bytes length, i32 cancel) {
    Fake *fake = context;
    if (cancel) {
        Fake_setCancels(fake, Fake_cancels(fake) + 1);
        return HAVEN_SNAPSHOT_REJECT;
    }
    Fake_setCalls(fake, Fake_calls(fake) + 1);
    assert(length <= sizeof(((Fake) {0}).bytes));
    if (Fake_result(fake) == HAVEN_SNAPSHOT_DONE) {
        memcpy(Fake_bytes(fake), data, length);
        Fake_setLength(fake, length);
        Fake_setKey(fake, key);
    }
    return Fake_result(fake);
}

/* Copies a stored snapshot into the caller's buffer when its key and capacity are valid. */
static i32 get(any context, u64 key, u8 *dest, bytes capacity, bytes *outLength) {
    Fake *fake = context;
    if (key != Fake_key(fake) || Fake_length(fake) > capacity)
        return HAVEN_SNAPSHOT_REJECT;
    memcpy(dest, Fake_bytes(fake), Fake_length(fake));
    *outLength = Fake_length(fake);
    return HAVEN_SNAPSHOT_DONE;
}

/* Covers public configuration, fixture round-trip, retry timing, cancellation, boundaries, and projections. */
static fn normal(string fixture) {
    SeshSnapshot job = SeshSnapshot();
    SeshSnapshot direct = SeshSnapshot_0();
    SeshSnapshot zero = SeshSnapshot_zero();
    assert(direct.state == zero.state && zero.state == SESH_SNAPSHOT_IDLE);
    u8 staging[256];
    Fake fake = {0};
    assert(SeshSnapshot_setBuffer(&job, staging, sizeof(staging)));
    assert(SeshSnapshot_setProvider(&job, put, &fake));
    assert(SeshSnapshot_setRetryPolicy(&job, 10, 2));
    assert(SeshSnapshot_getBuffer(&job) == staging);
    assert(SeshSnapshot_getCapacity(&job) == sizeof(staging));
    assert(SeshSnapshot_getProvider(&job) == put);
    assert(SeshSnapshot_getContext(&job) == &fake);
    assert(SeshSnapshot_getRetryDelayMs(&job) == 10);
    assert(SeshSnapshot_getMaxAttempts(&job) == 2);
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_IDLE);

    FILE *file = fopen(fixture, "rb");
    assert(file != nullptr);
    u8 input[256];
    bytes length = fread(input, 1, sizeof(input), file);
    assert(!ferror(file) && length != 0 && length < sizeof(input));
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
    assert(SeshSnapshot_queue(&job, 1, input, length));
    memset(input, 0, length);
    fake.result = HAVEN_SNAPSHOT_PENDING;
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_QUEUED);
    fake.result = HAVEN_SNAPSHOT_RETRY;
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_QUEUED);
    assert(SeshSnapshot_getAttempts(&job) == 1);
    assert(SeshSnapshot_getNextAttemptMs(&job) == 10);
    u32 calls = fake.calls;
    assert(SeshSnapshot_step(&job, 9) == SESH_SNAPSHOT_QUEUED && fake.calls == calls);
    fake.result = HAVEN_SNAPSHOT_DONE;
    assert(SeshSnapshot_step(&job, 10) == SESH_SNAPSHOT_SAVED);
    assert(SeshSnapshot_getKey(&job) == 1 && SeshSnapshot_getLength(&job) == length);
    assert(SeshSnapshot_getState(&job) == SESH_SNAPSHOT_SAVED);
    HavenSnapshotGetFn download = get;
    bytes restored = 0;
    assert(download(&fake, 1, input, sizeof(input), &restored) == HAVEN_SNAPSHOT_DONE);
    assert(restored == length && memcmp(input, staging, length) == 0);
    assert(download(&fake, 1, input, length - 1, &restored) == HAVEN_SNAPSHOT_REJECT);
    assert(restored == length && memcmp(input, staging, length) == 0);
    calls = fake.calls;
    SeshSnapshot_cancel(&job);
    assert(SeshSnapshot_step(&job, 11) == SESH_SNAPSHOT_SAVED && fake.calls == calls);
    assert(SeshSnapshot_queue(&job, 2, nullptr, 0));
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_SAVED && fake.length == 0);
    u8 one = 0;
    assert(SeshSnapshot_queue(&job, UINT64_MAX, &one, 1));
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_SAVED);
    assert(fake.key == UINT64_MAX && fake.length == 1 && fake.bytes[0] == 0);
    memset(input, 0xA5, sizeof(input));
    assert(SeshSnapshot_queue(&job, 3, input, sizeof(input)));
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_SAVED);
    assert(SeshSnapshot_queue(&job, 4, staging + 1, sizeof(staging) - 1));
    fake.result = HAVEN_SNAPSHOT_PENDING;
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_QUEUED);
    SeshSnapshot_cancel(&job);
    SeshSnapshot_cancel(&job);
    SeshSnapshot_cancel(nullptr);
    assert(fake.cancels == 1 && SeshSnapshot_getState(&job) == SESH_SNAPSHOT_CANCELLED);

    assert(SeshSnapshot_getBuffer(nullptr) == nullptr);
    assert(SeshSnapshot_getCapacity(nullptr) == 0 && SeshSnapshot_getLength(nullptr) == 0);
    assert(SeshSnapshot_getProvider(nullptr) == nullptr && SeshSnapshot_getContext(nullptr) == nullptr);
    assert(SeshSnapshot_getKey(nullptr) == 0 && SeshSnapshot_getNextAttemptMs(nullptr) == 0);
    assert(SeshSnapshot_getRetryDelayMs(nullptr) == 0 && SeshSnapshot_getAttempts(nullptr) == 0);
    assert(SeshSnapshot_getMaxAttempts(nullptr) == 0 && SeshSnapshot_getState(nullptr) == SESH_SNAPSHOT_IDLE);
    utf8 text[512];
    bool truncated = true;
    assert(SeshSnapshot_toString(&job, text, sizeof(text), &truncated) && !truncated);
    assert(string_contains(text, "snapshot 4"));
    assert(SeshSnapshot_toStringStruct(&job, text, sizeof(text), nullptr));
    string fields[] = {"buffer=", "capacity=", "length=", "put=", "context=", "key=", "nextAttemptMs=", "retryDelayMs=", "attempts=", "maxAttempts=", "state="};
    string cursor = text;
    for (bytes i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        cursor = string_find(cursor, fields[i]);
        assert(cursor != nullptr);
        cursor += strlen(fields[i]);
    }
    assert(SeshSnapshot_toString(nullptr, text, 8, &truncated) && string_equals(text, "nullptr"));
    assert(SeshSnapshot_toStringStruct(nullptr, text, 8, nullptr));
    puts("PASS: fixture round trip, pending/retry/cancel, boundaries and public projections");
}

/* Checks rejected admissions preserve state, report failures, and permit later valid recovery. */
static fn invalid() {
    SeshSnapshot job = SeshSnapshot();
    u8 bytes[2] = {1, 2};
    Fake fake = {0};
    fake.result = HAVEN_SNAPSHOT_RETRY;
    assert(!SeshSnapshot_setBuffer(nullptr, bytes, 2));
    assert(!SeshSnapshot_setBuffer(&job, nullptr, 2));
    assert(!SeshSnapshot_setBuffer(&job, bytes, 0));
    assert(!SeshSnapshot_setProvider(nullptr, put, &fake));
    assert(!SeshSnapshot_setProvider(&job, nullptr, &fake));
    assert(!SeshSnapshot_setRetryPolicy(nullptr, 1, 1));
    assert(!SeshSnapshot_setRetryPolicy(&job, 0, 1));
    assert(!SeshSnapshot_setRetryPolicy(&job, 1, 0));
    assert(!SeshSnapshot_queue(nullptr, 1, bytes, 2));
    assert(!SeshSnapshot_queue(&job, 0, bytes, 2));
    assert(!SeshSnapshot_queue(&job, 1, nullptr, 2));
    assert(!SeshSnapshot_queue(&job, 1, bytes, 2));
    assert(SeshSnapshot_setBuffer(&job, bytes, 2));
    assert(SeshSnapshot_setProvider(&job, put, &fake));
    SeshSnapshot before = job;
    assert(!SeshSnapshot_queue(&job, 1, bytes, SIZE_MAX));
    assert(memcmp(&before, &job, sizeof(job)) == 0);
    assert(!SeshSnapshot_queue(&job, 1, bytes, 3));
    assert(memcmp(&before, &job, sizeof(job)) == 0 && bytes[0] == 1);
    assert(SeshSnapshot_queue(&job, 1, bytes, 2));
    before = job;
    assert(!SeshSnapshot_queue(&job, 2, bytes, 2));
    assert(!SeshSnapshot_setBuffer(&job, bytes, 2));
    assert(!SeshSnapshot_setProvider(&job, put, &fake));
    assert(!SeshSnapshot_setRetryPolicy(&job, 1, 1));
    assert(memcmp(&before, &job, sizeof(job)) == 0);
    assert(SeshSnapshot_step(nullptr, 0) == SESH_SNAPSHOT_FAILED);
    assert(SeshSnapshot_step(&job, UINT64_MAX) == SESH_SNAPSHOT_FAILED);
    assert(fake.cancels == 1);
    assert(SeshSnapshot_setRetryPolicy(&job, 1, 1));
    assert(SeshSnapshot_queue(&job, 2, bytes, 2));
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_FAILED && fake.cancels == 2);
    fake.result = HAVEN_SNAPSHOT_REJECT;
    assert(SeshSnapshot_queue(&job, 3, bytes, 2));
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_FAILED);
    fake.result = 99;
    assert(SeshSnapshot_queue(&job, 4, bytes, 2));
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_FAILED);
    fake.result = HAVEN_SNAPSHOT_DONE;
    assert(SeshSnapshot_queue(&job, 5, bytes, 2));
    assert(SeshSnapshot_step(&job, 0) == SESH_SNAPSHOT_SAVED);
    utf8 text[8];
    bool truncated = false;
    assert(!SeshSnapshot_toString(nullptr, nullptr, 8, &truncated) && truncated);
    assert(!SeshSnapshot_toString(nullptr, text, 0, &truncated) && truncated);
    assert(!SeshSnapshot_toString(nullptr, text, 7, &truncated) && truncated);
    assert(!SeshSnapshot_toStringStruct(&job, text, 1, nullptr) && text[0] == '\0');
    puts("PASS: 27 loud cold rejections; preserved admission and recovery");
}

/* Selects the normal snapshot contract suite or its cold-rejection suite. */
static i32 suite_snapshot(i32 argc, utf8 **argv) {
    assert(argc == 2);
    if (string_equals(argv[1], "invalid"))
        invalid();
    else
        normal(argv[1]);
    return 0;
}

/** Zero-arg adapter for snapshot suite. */
static i32 suite_snapshot_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_snapshot(1, argv);
}

/** Suite: arity (from lang/arity_test.c). */
/* Header-only arity owner. Every advertised class chooser is exercised by its
 * class owner; unsupported counts are compile-negative probes in session_run.py. */

static i32 suite_arity() {
    _Static_assert(SESH_ARITY() == 0);
    _Static_assert(SESH_ARITY(1) == 1);
    _Static_assert(SESH_ARITY(1, 2) == 2);
    _Static_assert(SESH_ARITY(1, 2, 3) == 3);
    _Static_assert(SESH_ARITY(1, 2, 3, 4) == 4);
    _Static_assert(SESH_ARITY(1, 2, 3, 4, 5) == 5);
    _Static_assert(SESH_ARITY(1, 2, 3, 4, 5, 6) == 6);
    assert(SESH_ARITY() == 0);
    return 0;
}

/** Suite: interop (from lang/interop_test.c). */
/** Public header composition only: R5 may include both Graphvex and Sesh. */


static i32 suite_interop()
{
    GraphicsRect rect = GraphicsRect_0();
    SeshClient sessionClient = SeshClient(1);
    assert(GraphicsRect_isEmpty(rect) && SeshClient_getId(&sessionClient) == 1);
    return 0;
}

/** Suite: auth_service (from session/auth_service_test.c). */
/* Trusted-verifier binding, revocation and secret-free projections. Offline only. */

/** Accepts the fixture credential and supplies the zero-identity rejection case. */
static bool verifyZero(const ApiAuth *auth, any context, u64 *dest) {
    (discard) auth; (discard) context;
    *dest = 0;
    return true;
}
static i32 suite_auth_service(i32 argc, utf8 **argv) {
    ApiAuth credential = {0};
    credential.kind = API_AUTH_API_KEY;
    credential.credential = "DO-NOT-PRINT-SECRET";
    u64 principal = 7;
    AuthService auth = AuthService(&credential, verify, &principal);
    assert(AuthService_0().principalId == 0 && AuthService().principalId == 0 && AuthService_zero().principalId == 0);
    AuthService direct = AuthService_3(&credential, verify, &principal);
    assert(AuthService_getAuth(&direct) == &credential);
    assert(AuthService_getVerifier(&auth) == verify && AuthService_getContext(&auth) == &principal);
    assert(AuthService_getAuth(nullptr) == nullptr && AuthService_getVerifier(nullptr) == nullptr &&
           AuthService_getContext(nullptr) == nullptr && AuthService_getPrincipalId(nullptr) == 0 && !AuthService_isAuthenticated(nullptr));
    assert(!AuthService_isAuthenticated(&auth));
    assert(AuthService_authenticate(&auth) && AuthService_getPrincipalId(&auth) == 7);
    utf8 text[512];
    assert(AuthService_toStringStruct(&auth, text, sizeof(text), nullptr));
    assert(!string_contains(text, credential.credential) && string_find(text, "auth=redacted,verifier=bound,context=redacted,principalId=7"));
    PROJECTIONS(AuthService, auth, "principalId=7");
    assert(AuthService_setAuth(&auth, &credential) && !AuthService_isAuthenticated(&auth));
    assert(AuthService_authenticate(&auth));
    assert(AuthService_setContext(&auth, nullptr) && !AuthService_isAuthenticated(&auth));
    assert(AuthService_getContext(&auth) == nullptr);
    assert(AuthService_setContext(&auth, &principal) && AuthService_authenticate(&auth));
    assert(AuthService_setVerifier(&auth, verify, &principal) && !AuthService_isAuthenticated(&auth));
    assert(AuthService_authenticate(&auth));
    AuthService_revoke(&auth); AuthService_revoke(&auth); AuthService_revoke(nullptr);
    assert(!AuthService_isAuthenticated(&auth));
    if (INVALID_MODE) {
        assert(AuthService_authenticate(&auth));
        assert(AuthService(nullptr, verify, &principal).auth == nullptr);
        assert(AuthService(&credential, nullptr, &principal).auth == nullptr);
        assert(!AuthService_setAuth(nullptr, &credential));
        assert(!AuthService_setAuth(&auth, nullptr) && AuthService_getAuth(&auth) == &credential);
        assert(!AuthService_setVerifier(nullptr, verify, &principal));
        assert(!AuthService_setVerifier(&auth, nullptr, &principal) && AuthService_getVerifier(&auth) == verify);
        assert(!AuthService_setContext(nullptr, &principal));
        assert(AuthService_getPrincipalId(&auth) == 7);
        assert(!AuthService_authenticate(nullptr));
        assert(AuthService_authenticate(&direct));
        principal = 0;
        assert(!AuthService_authenticate(&direct) && !AuthService_isAuthenticated(&direct));
        assert(AuthService_setVerifier(&direct, verifyZero, nullptr));
        assert(!AuthService_authenticate(&direct) && !AuthService_isAuthenticated(&direct));
        AuthService unbound = AuthService();
        assert(!AuthService_authenticate(&unbound));
        BAD_PROJECTIONS(AuthService, auth);
    }
    return 0;
}

/** Zero-arg adapter for auth_service suite. */
static i32 suite_auth_service_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_auth_service(1, argv);
}

/** Suite: client (from session/client_test.c). */
/* SeshClient public identity/value owner; host persistence is not implemented. */

static i32 suite_client(i32 argc, utf8 **argv) {
    SeshClient client = SeshClient(1);
    assert(SeshClient_getId(&client) == 1);
    assert(SeshClient_getId(nullptr) == 0);
    assert(SeshClient_0().id == 0 && SeshClient().id == 0 && SeshClient_zero().id == 0);
    assert(SeshClient_1(2).id == 2);
    assert(SeshClient_setId(&client, UINT64_MAX));
    PROJECTIONS(SeshClient, client, "id=18446744073709551615");
    if (INVALID_MODE) {
        assert(!SeshClient_setId(nullptr, 1));
        assert(!SeshClient_setId(&client, 0) && SeshClient_getId(&client) == UINT64_MAX);
        assert(SeshClient(0).id == 0);
        BAD_PROJECTIONS(SeshClient, client);
    }
    return 0;
}

/** Zero-arg adapter for client suite. */
static i32 suite_client_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_client(1, argv);
}

/** Suite: operation (from session/operation_test.c). */
/* All operation identity forms, boundaries, equality and copied intent vocabulary. */

static i32 suite_operation(i32 argc, utf8 **argv) {
    Operation operation = Operation(1, 2, 3, 4, 0);
    assert(!Operation_isValid(nullptr));
    Operation empty = Operation();
    assert(!Operation_isValid(&empty) && Operation_0().id == 0 && Operation_zero().id == 0);
    Operation direct = Operation_5(1, 2, 3, 4, 0);
    assert(Operation_equals(&direct, &operation));
    assert(!Operation_equals(nullptr, &operation) && !Operation_equals(&operation, nullptr));
    assert(Operation_getId(nullptr) == 0 && Operation_getClientId(nullptr) == 0 &&
           Operation_getWorkspaceId(nullptr) == 0 && Operation_getResourceId(nullptr) == 0 && Operation_getExpectedRevision(nullptr) == 0);
#define ROUNDTRIP(Name) assert(Operation_set##Name(&operation, UINT64_MAX)); assert(Operation_get##Name(&operation) == UINT64_MAX)
    ROUNDTRIP(Id); ROUNDTRIP(ClientId); ROUNDTRIP(WorkspaceId); ROUNDTRIP(ResourceId); ROUNDTRIP(ExpectedRevision);
#undef ROUNDTRIP
    assert(!Operation_equals(&direct, &operation));
    PROJECTIONS(Operation, operation, "resourceId=18446744073709551615,expectedRevision=");
    if (INVALID_MODE) {
#define BAD_SET(Name) assert(!Operation_set##Name(nullptr, 1)); assert(!Operation_set##Name(&operation, 0)); assert(Operation_get##Name(&operation) == UINT64_MAX)
        BAD_SET(Id); BAD_SET(ClientId); BAD_SET(WorkspaceId); BAD_SET(ResourceId);
#undef BAD_SET
        assert(!Operation_setExpectedRevision(nullptr, 0));
        assert(Operation(0, 1, 1, 1, 0).id == 0);
        assert(Operation(1, 0, 1, 1, 0).id == 0);
        assert(Operation(1, 1, 0, 1, 0).id == 0);
        assert(Operation(1, 1, 1, 0, 0).id == 0);
        BAD_PROJECTIONS(Operation, operation);
    }
    return 0;
}
#undef ROUNDTRIP
#undef BAD_SET

/** Zero-arg adapter for operation suite. */
static i32 suite_operation_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_operation(1, argv);
}

/** Suite: operations (from session/operations_test.c). */
/* Flat ledger owner: growth, copied admission, replay/reuse, preserved outputs.
 * No arbitrary-pointer rejection, allocation ownership or concurrency promised. */

static i32 suite_operations(i32 argc, utf8 **argv) {
    OperationsSlot first[1], grown[16];
    Operations operations = Operations(first, 1);
    assert(Operations_0().count == 0 && Operations().count == 0 && Operations_zero().count == 0);
    Operations direct = Operations_2(grown, 16);
    assert(Operations_getCapacity(&direct) == 16 && Operations_getCount(nullptr) == 0 && Operations_getCapacity(nullptr) == 0);
    Operation value = Operation(1, 1, 1, 1, 0);
    assert(Operations_add(&operations, &value) == OPERATIONS_ADDED);
    assert(Operations_add(&operations, &value) == OPERATIONS_REPLAY);
    assert(!Operations_isApplied(&operations, 1, 1) && !Operations_isApplied(nullptr, 1, 1));
    Resource resource = Resource(1, 1, 0);
    assert(Operations_apply(&operations, &value, &resource) == OPERATIONS_ADDED);
    assert(Operations_isApplied(&operations, 1, 1) && Resource_getRevision(&resource) == 1);
    assert(Operations_apply(&operations, &value, &resource) == OPERATIONS_REPLAY);
    Operation copy = Operation(99, 1, 1, 1, 0);
    assert(!Operations_find(&operations, 1, 99, &copy) && copy.id == 99);
    assert(Operations_find(&operations, 1, 1, &copy) && Operation_equals(&copy, &value));
    assert(Operation_setExpectedRevision(&value, 100));
    assert(Operations_find(&operations, 1, 1, &copy) && copy.expectedRevision == 0);
    assert(Operations_reserve(&operations, grown, 16));
    first[0].operation.id = 99;
    assert(Operations_find(&operations, 1, 1, &copy) && copy.id == 1);
    for (u64 id = 2; id <= 16; id++) {
        value = Operation(id, 1, 1, 1, id);
        assert(Operations_add(&operations, &value) == OPERATIONS_ADDED);
    }
    assert(Operations_getCount(&operations) == 16 && Operations_getCapacity(&operations) == 16);
    assert(Operations_isApplied(&operations, 1, 1) && !Operations_isApplied(&operations, 1, 99));
    PROJECTIONS(Operations, operations, "count=16,capacity=16");
    if (INVALID_MODE) {
        assert(Operations(nullptr, 1).capacity == 0);
        assert(Operations(first, 0).capacity == 0);
        assert(!Operations_reserve(nullptr, first, 1));
        assert(!Operations_reserve(&operations, nullptr, 16));
        assert(!Operations_reserve(&operations, first, SIZE_MAX));
        assert(!Operations_reserve(&operations, first, 1));
        assert(Operations_getCount(&operations) == 16);
        value = Operation(17, 1, 1, 1, 0);
        assert(Operations_add(&operations, &value) == OPERATIONS_REJECTED);
        assert(Operations_add(nullptr, &value) == OPERATIONS_REJECTED);
        assert(Operations_add(&operations, nullptr) == OPERATIONS_REJECTED);
        value = Operation(1, 1, 1, 1, 99);
        assert(Operations_add(&operations, &value) == OPERATIONS_REJECTED);
        assert(!Operations_find(nullptr, 1, 1, &copy));
        assert(!Operations_find(&operations, 1, 1, nullptr));
        assert(!Operations_find(&operations, 0, 1, &copy));
        assert(!Operations_find(&operations, 1, 0, &copy));
        assert(copy.id == 1 && copy.expectedRevision == 0);
        assert(Operations_apply(nullptr, &value, &resource) == OPERATIONS_REJECTED);
        assert(Operations_apply(&operations, nullptr, &resource) == OPERATIONS_REJECTED);
        assert(Operations_apply(&operations, &value, nullptr) == OPERATIONS_REJECTED);
        assert(Operations_apply(&operations, &value, &resource) == OPERATIONS_REJECTED);
        value = Operation(17, 1, 1, 1, 0);
        assert(Operations_apply(&operations, &value, &resource) == OPERATIONS_REJECTED);
        value = Operation(2, 1, 1, 1, 2);
        assert(Operations_apply(&operations, &value, &resource) == OPERATIONS_CONFLICT);
        assert(Resource_setRevision(&resource, 2));
        assert(Operations_apply(&operations, &value, &resource) == OPERATIONS_ADDED);
        assert(Resource_getRevision(&resource) == 3);
        value = Operation(17, 1, 9, 1, 0);
        assert(Operations_apply(&operations, &value, &resource) == OPERATIONS_REJECTED);
        value = Operation(17, 1, 1, 9, 0);
        assert(Operations_apply(&operations, &value, &resource) == OPERATIONS_REJECTED);
        OperationsSlot overflowSlot[1];
        Operations overflow = Operations(overflowSlot, 1);
        value = Operation(1, 1, 1, 1, UINT64_MAX);
        assert(Operations_add(&overflow, &value) == OPERATIONS_ADDED);
        assert(Resource_setRevision(&resource, UINT64_MAX));
        assert(Operations_apply(&overflow, &value, &resource) == OPERATIONS_REJECTED);
        assert(!Operations_isApplied(&overflow, 1, 1) && Resource_getRevision(&resource) == UINT64_MAX);
        BAD_PROJECTIONS(Operations, operations);
    }
    Operations_clear(&operations); Operations_clear(&operations); Operations_clear(nullptr);
    assert(Operations_getCount(&operations) == 0);
    value = Operation(1, 2, 1, 1, 0);
    assert(Operations_add(&operations, &value) == OPERATIONS_ADDED);
    return 0;
}

/** Zero-arg adapter for operations suite. */
static i32 suite_operations_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_operations(1, argv);
}

/** Suite: resource (from session/resource_test.c). */
/* Resource metadata owner; revisions are local externally serialized values. */

static i32 suite_resource(i32 argc, utf8 **argv) {
    Resource resource = Resource(1, 2, 0);
    assert(Resource_0().id == 0 && Resource().id == 0 && Resource_zero().id == 0);
    assert(Resource_3(3, 4, 5).revision == 5);
    assert(Resource_getId(nullptr) == 0 && Resource_getWorkspaceId(nullptr) == 0 && Resource_getRevision(nullptr) == 0);
    assert(Resource_setId(&resource, UINT64_MAX));
    assert(Resource_setWorkspaceId(&resource, UINT64_MAX));
    assert(Resource_setRevision(&resource, UINT64_MAX));
    assert(Resource_getId(&resource) == UINT64_MAX && Resource_getWorkspaceId(&resource) == UINT64_MAX && Resource_getRevision(&resource) == UINT64_MAX);
    PROJECTIONS(Resource, resource, "workspaceId=18446744073709551615,revision=");
    if (INVALID_MODE) {
        assert(!Resource_setId(nullptr, 1));
        assert(!Resource_setWorkspaceId(nullptr, 1));
        assert(!Resource_setRevision(nullptr, 0));
        assert(!Resource_setId(&resource, 0));
        assert(!Resource_setWorkspaceId(&resource, 0));
        assert(resource.id == UINT64_MAX && resource.workspaceId == UINT64_MAX && resource.revision == UINT64_MAX);
        assert(Resource(0, 1, 0).id == 0);
        assert(Resource(1, 0, 0).id == 0);
        BAD_PROJECTIONS(Resource, resource);
    }
    return 0;
}

/** Zero-arg adapter for resource suite. */
static i32 suite_resource_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_resource(1, argv);
}

/** Suite: sesh (from session/sesh_test.c). */
/* Sesh composition owner: all parts/constructors, scoped authenticated admission,
 * copied history and replay, stale revisions, failure atomicity and borrow detach.
 * Multiple Sesh objects share state only under one caller-owned serialization
 * domain. This tests local metadata, not OAuth, data sync or distributed CAS.
 */


;;GETTER
static inline Sesh *Worker_session(Worker *self)
{
    return &(*self).session;
}
;;GETTER
static inline pthread_mutex_t *Worker_mutex(const Worker *self)
{
    return (*self).mutex;
}
;;GETTER
static inline u64 Worker_clientId(const Worker *self)
{
    return (*self).clientId;
}
;;GETTER
static inline pthread_cond_t *Worker_condition(const Worker *self)
{
    return (*self).condition;
}
;;GETTER
static inline bytes *Worker_ready(const Worker *self)
{
    return (*self).ready;
}
;;GETTER
static inline bool *Worker_start(const Worker *self)
{
    return (*self).start;
}

/* Applies and immediately replays 32 operations while holding the caller's shared serialization mutex. */
static any submitWorker(any context) {
    Worker *worker = context;
    pthread_mutex_t *mutex = Worker_mutex(worker);
    pthread_cond_t *condition = Worker_condition(worker);
    assert(pthread_mutex_lock(mutex) == 0);
    ++*Worker_ready(worker);
    assert(pthread_cond_broadcast(condition) == 0);
    while (!*Worker_start(worker))
        assert(pthread_cond_wait(condition, mutex) == 0);
    assert(pthread_mutex_unlock(mutex) == 0);
    for (u64 id = 1; id <= 32; id++) {
        assert(pthread_mutex_lock(mutex) == 0);
        Sesh *session = Worker_session(worker);
        Resource *resource = Sesh_getResource(session);
        Operation operation = Operation(id, Worker_clientId(worker), 3, 4, Resource_getRevision(resource));
        assert(Sesh_submit(session, &operation) == SESH_APPLIED);
        assert(Sesh_submit(session, &operation) == SESH_REPLAY);
        assert(pthread_mutex_unlock(mutex) == 0);
    }
    return nullptr;
}

/* Starts four clients together and verifies serialized application and replay accounting. */
static fn serializedConcurrency(AuthService *auth, Workspace *workspace) {
    Resource resource = Resource(4, 3, 0);
    OperationsSlot rows[128];
    Operations ledger = Operations(rows, 128);
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    bytes ready = 0;
    bool start = false;
    SeshClient clients[4];
    Worker workers[4];
    thread threads[4];
    for (bytes i = 0; i < 4; i++) {
        clients[i] = SeshClient(i + 1);
        workers[i] = (Worker) {Sesh(auth, &clients[i], workspace, &resource, &ledger), &mutex, i + 1, &condition, &ready, &start};
        assert(pthread_create(&threads[i], nullptr, submitWorker, &workers[i]) == 0);
    }
    assert(pthread_mutex_lock(&mutex) == 0);
    while (ready != 4)
        assert(pthread_cond_wait(&condition, &mutex) == 0);
    start = true;
    assert(pthread_cond_broadcast(&condition) == 0);
    assert(pthread_mutex_unlock(&mutex) == 0);
    for (bytes i = 0; i < 4; i++)
        assert(pthread_join(threads[i], nullptr) == 0);
    assert(Resource_getRevision(&resource) == 128 && Operations_getCount(&ledger) == 128);
    assert(pthread_mutex_destroy(&mutex) == 0);
    assert(pthread_cond_destroy(&condition) == 0);
}

/* Exercises Sesh composition, valid admission, replay, caller-serialized clients, and optional rejection cases. */
static i32 suite_sesh(i32 argc, utf8 **argv) {
    ApiAuth credential = {0};
    credential.kind = API_AUTH_BEARER;
    credential.credential = "NEVER-PRINT-TOKEN";
    u64 principal = 7;
    AuthService auth = AuthService(&credential, verify, &principal);
    assert(AuthService_authenticate(&auth));
    SeshClient client = SeshClient(2);
    Workspace workspace = Workspace(3, 7);
    Resource resource = Resource(4, 3, 0);
    OperationsSlot storage[2];
    OperationsSlot grown[4];
    Operations operations = Operations(storage, 2);
    Sesh session = Sesh(&auth, &client, &workspace, &resource, &operations);
    Sesh direct = Sesh_5(&auth, &client, &workspace, &resource, &operations);
    Sesh empty = Sesh();
    assert(Sesh_0().client == nullptr && Sesh_zero().client == nullptr);
    assert(Sesh_setAuthService(&empty, &auth));
    assert(Sesh_setClient(&empty, &client));
    assert(Sesh_setWorkspace(&empty, &workspace));
    assert(Sesh_setResource(&empty, &resource));
    assert(Sesh_setOperations(&empty, &operations));
    assert(Sesh_getAuthService(&empty) == &auth && Sesh_getClient(&empty) == &client &&
           Sesh_getWorkspace(&empty) == &workspace && Sesh_getResource(&empty) == &resource && Sesh_getOperations(&empty) == &operations);
    assert(Sesh_getAuthService(nullptr) == nullptr && Sesh_getClient(nullptr) == nullptr &&
           Sesh_getWorkspace(nullptr) == nullptr && Sesh_getResource(nullptr) == nullptr && Sesh_getOperations(nullptr) == nullptr);
    Operation operation = Operation(1, 2, 3, 4, 0);
    assert(Operations_add(&operations, &operation) == OPERATIONS_ADDED);
    assert(!Operations_isApplied(&operations, 2, 1));
    assert(Sesh_submit(&session, &operation) == SESH_APPLIED);
    assert(Resource_getRevision(&resource) == 1 && Operations_getCount(&operations) == 1);
    assert(Sesh_submit(&direct, &operation) == SESH_REPLAY);
    assert(Resource_getRevision(&resource) == 1 && Operations_getCount(&operations) == 1);
    PROJECTIONS(Sesh, session, "authService=principal 7,client=client 2,workspace=workspace 3,resource=resource 4 revision 1,operations=1 operations");
    serializedConcurrency(&auth, &workspace);
    if (INVALID_MODE) {
        Resource shared = Resource(4, 3, 0);
        OperationsSlot raceSlots[2];
        Operations raceLedger = Operations(raceSlots, 2);
        SeshClient secondClient = SeshClient(3);
        Sesh firstSession = Sesh(&auth, &client, &workspace, &shared, &raceLedger);
        Sesh secondSession = Sesh(&auth, &secondClient, &workspace, &shared, &raceLedger);
        Operation firstIntent = Operation(1, 2, 3, 4, 0);
        Operation secondIntent = Operation(1, 3, 3, 4, 0);
        assert(Sesh_submit(&firstSession, &firstIntent) == SESH_APPLIED);
        assert(Sesh_submit(&secondSession, &secondIntent) == SESH_CONFLICT);
        assert(Resource_getRevision(&shared) == 1 && Operations_getCount(&raceLedger) == 1);
        assert(Operation_setExpectedRevision(&secondIntent, 1));
        assert(Sesh_submit(&secondSession, &secondIntent) == SESH_APPLIED);
        assert(Resource_getRevision(&shared) == 2 && Operations_getCount(&raceLedger) == 2);
        assert(Sesh(nullptr, &client, &workspace, &resource, &operations).client == nullptr);
        assert(Sesh(&auth, nullptr, &workspace, &resource, &operations).client == nullptr);
        assert(Sesh(&auth, &client, nullptr, &resource, &operations).client == nullptr);
        assert(Sesh(&auth, &client, &workspace, nullptr, &operations).client == nullptr);
        assert(Sesh(&auth, &client, &workspace, &resource, nullptr).client == nullptr);
#define BAD_PART(Name, value) assert(!Sesh_set##Name(nullptr, value)); assert(!Sesh_set##Name(&session, nullptr)); assert(Sesh_get##Name(&session) == value)
        BAD_PART(AuthService, &auth); BAD_PART(Client, &client); BAD_PART(Workspace, &workspace);
        BAD_PART(Resource, &resource); BAD_PART(Operations, &operations);
#undef BAD_PART
        assert(Sesh_submit(nullptr, &operation) == SESH_REJECTED);
        assert(Sesh_submit(&session, nullptr) == SESH_REJECTED);
        Sesh unbound = Sesh();
        assert(Sesh_submit(&unbound, &operation) == SESH_REJECTED);
        AuthService_revoke(&auth);
        assert(Sesh_submit(&session, &operation) == SESH_REJECTED);
        assert(AuthService_authenticate(&auth));
        assert(Workspace_setPrincipalId(&workspace, 8));
        assert(Sesh_submit(&session, &operation) == SESH_REJECTED);
        assert(Workspace_setPrincipalId(&workspace, 7));
        Operation wrong = Operation(2, 9, 3, 4, 1);
        assert(Sesh_submit(&session, &wrong) == SESH_REJECTED);
        wrong = Operation(2, 2, 9, 4, 1);
        assert(Sesh_submit(&session, &wrong) == SESH_REJECTED);
        wrong = Operation(2, 2, 3, 9, 1);
        assert(Sesh_submit(&session, &wrong) == SESH_REJECTED);
        assert(Resource_setWorkspaceId(&resource, 9));
        assert(Sesh_submit(&session, &operation) == SESH_REJECTED);
        assert(Resource_setWorkspaceId(&resource, 3));
        wrong = Operation(2, 2, 3, 4, 0);
        assert(Sesh_submit(&session, &wrong) == SESH_CONFLICT);
        wrong = Operation(1, 2, 3, 4, 1);
        assert(Sesh_submit(&session, &wrong) == SESH_REJECTED);
        assert(Resource_getRevision(&resource) == 1 && Operations_getCount(&operations) == 1);
        operation = Operation(2, 2, 3, 4, 1);
        assert(Sesh_submit(&session, &operation) == SESH_APPLIED);
        operation = Operation(3, 2, 3, 4, 2);
        assert(Sesh_submit(&session, &operation) == SESH_REJECTED);
        assert(Resource_getRevision(&resource) == 2 && Operations_getCount(&operations) == 2);
        assert(Operations_reserve(&operations, grown, 4));
        assert(Sesh_submit(&session, &operation) == SESH_APPLIED);
        assert(Resource_setRevision(&resource, UINT64_MAX));
        operation = Operation(4, 2, 3, 4, UINT64_MAX);
        assert(Sesh_submit(&session, &operation) == SESH_REJECTED);
        assert(Resource_getRevision(&resource) == UINT64_MAX && Operations_getCount(&operations) == 3);
        BAD_PROJECTIONS(Sesh, session);
        Sesh_close(&session);
        assert(Sesh_submit(&session, &operation) == SESH_REJECTED);
    }
    Sesh_close(&session); Sesh_close(&session); Sesh_close(nullptr);
    Sesh_close(&direct); Sesh_close(&empty);
    assert(Sesh_getResource(&session) == nullptr);
    assert(SeshClient_getId(&client) == 2 && AuthService_isAuthenticated(&auth));
    return 0;
}
#undef BAD_PART

/** Zero-arg adapter for sesh suite. */
static i32 suite_sesh_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_sesh(1, argv);
}

/** Suite: text (from session/text_test.c). */
/* Internal bounded formatter owner: optional flag, exact capacity, escaping
 * responsibility at caller (only numeric/public metadata currently supplied). */

static i32 suite_text(i32 argc, utf8 **argv) {
    utf8 text[5]; bool truncated = true;
    assert(SeshText_format(text, sizeof(text), &truncated, "%s", "text") && !truncated);
    assert(string_equals(text, "text"));
    assert(SeshText_format(text, sizeof(text), nullptr, "%d", 0));
    utf8 ten[11];
    assert(SeshText_format(ten, sizeof(ten), nullptr, "%d%d%d%d%d%d%d%d%d%d", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
    assert(string_equals(ten, "0123456789"));
    if (argc == 2 && string_equals(argv[1], "invalid")) {
        assert(!SeshText_format(nullptr, 5, &truncated, "%s", "text") && truncated);
        assert(!SeshText_format(text, 0, &truncated, "%s", "text") && truncated);
        assert(!SeshText_format(text, 4, &truncated, "%s", "text") && truncated);
        assert(string_equals(text, "tex"));
        assert(!SeshText_format(text, 1, nullptr, "%s", "text") && text[0] == '\0');
        assert(!SeshText_format(text, sizeof(text), &truncated, nullptr) && !truncated && text[0] == '\0');
    }
    return 0;
}

/** Zero-arg adapter for text suite. */
static i32 suite_text_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_text(1, argv);
}

/** Suite: workspace (from session/workspace_test.c). */
/* Private-workspace identity owner; no remote ACL verification claim. */

static i32 suite_workspace(i32 argc, utf8 **argv) {
    Workspace workspace = Workspace(1, 2);
    assert(Workspace_0().id == 0 && Workspace().id == 0 && Workspace_zero().id == 0);
    assert(Workspace_2(3, 4).principalId == 4);
    assert(Workspace_getId(nullptr) == 0 && Workspace_getPrincipalId(nullptr) == 0);
    assert(Workspace_setId(&workspace, UINT64_MAX));
    assert(Workspace_setPrincipalId(&workspace, UINT64_MAX));
    assert(Workspace_getId(&workspace) == UINT64_MAX && Workspace_getPrincipalId(&workspace) == UINT64_MAX);
    PROJECTIONS(Workspace, workspace, "id=18446744073709551615,principalId=");
    if (INVALID_MODE) {
        assert(!Workspace_setId(nullptr, 1));
        assert(!Workspace_setPrincipalId(nullptr, 1));
        assert(!Workspace_setId(&workspace, 0));
        assert(!Workspace_setPrincipalId(&workspace, 0));
        assert(workspace.id == UINT64_MAX && workspace.principalId == UINT64_MAX);
        assert(Workspace(0, 1).id == 0);
        assert(Workspace(1, 0).id == 0);
        BAD_PROJECTIONS(Workspace, workspace);
    }
    return 0;
}

/** Zero-arg adapter for workspace suite. */
static i32 suite_workspace_entry() {
    utf8 *argv[] = { "sesh_test", nullptr };
    return suite_workspace(1, argv);
}

/** Runs all automated vex/sesh suites or a single named suite passed as argv[1]. */
i32 main(i32 argc, utf8 **argv) {
    const SuiteEntry suites[] = {
        { "snapshot", suite_snapshot_entry },
        { "arity", suite_arity },
        { "interop", suite_interop },
        { "auth_service", suite_auth_service_entry },
        { "client", suite_client_entry },
        { "operation", suite_operation_entry },
        { "operations", suite_operations_entry },
        { "resource", suite_resource_entry },
        { "sesh", suite_sesh_entry },
        { "text", suite_text_entry },
        { "workspace", suite_workspace_entry },
    };
    string filter = argc > 1 ? argv[1] : nullptr;
    bytes ran = 0;
    i32 failed = 0;

    for (bytes i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        if (filter && !string_equals(filter, suites[i].name))
            continue;
        if (suites[i].run() != 0)
            failed++;
        ran++;
    }

    if (filter && ran == 0) {
        fprint(stderr, "sesh_test: unknown suite '", filter, "'");
        return 2;
    }
    if (failed != 0) {
        fprint(stderr, "sesh_test: ", failed, " suite(s) FAILED");
        return 1;
    }
    puts("sesh_test: PASS");
    return 0;
}
