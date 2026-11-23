#ifndef SESH_OWNER_SUPPORT_H
#define SESH_OWNER_SUPPORT_H

#include "vex.h"
#include "lang/sesh.h"
#include "primitive/char_sequence.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>

/**
 * Serialized concurrency test worker state for Sesh owner tests.
 * Sesh session - per-worker session handle
 * pthread_mutex_t *mutex - shared serialization lock
 * u64 clientId - worker client identity
 * pthread_cond_t *condition - barrier condition variable
 * bytes *ready - count of ready workers
 * bool *start - shared start latch
 */
typedef struct Worker {
    Sesh session;
    pthread_mutex_t *mutex;
    u64 clientId;
    pthread_cond_t *condition;
    bytes *ready;
    bool *start;
} Worker;

/**
 * Verifies the fixture credential by reading the expected principal ID from context.
 * @param auth    caller credential
 * @param context pointer to u64 principal ID
 * @param dest    destination principal ID output
 * @return true when principal ID is nonzero
 */
static inline bool verify(const ApiAuth *auth, any context, u64 *dest) {
    assert(auth != nullptr && context != nullptr && dest != nullptr);
    *dest = *(u64*) context;
    return *dest != 0;
}

#define PROJECTIONS(Class, object, fieldText) do { \
    utf8 text[512]; bool truncated = true; \
    assert(Class##_toString(&(object), text, sizeof(text), &truncated) && !truncated); \
    assert(Class##_toStringStruct(&(object), text, sizeof(text), nullptr)); \
    assert(CharSequence_find(CharSequence_cstr(text), CharSequence_cstr(fieldText)) != SIZE_MAX); \
    assert(Class##_toString(nullptr, text, 8, &truncated) && !truncated); \
    assert(CharSequence_compare(CharSequence_cstr(text), CharSequence_cstr("nullptr")) == 0); \
    assert(Class##_toStringStruct(nullptr, text, 8, nullptr)); \
} while (0)

#define BAD_PROJECTIONS(Class, object) do { \
    utf8 text[1]; bool truncated = false; \
    assert(!Class##_toString(&(object), nullptr, 1, &truncated) && truncated); \
    assert(!Class##_toStringStruct(&(object), text, 0, &truncated) && truncated); \
    assert(!Class##_toString(&(object), text, 1, &truncated) && truncated && text[0] == '\0'); \
} while (0)

#define INVALID_MODE (argc == 2 && CharSequence_compare(CharSequence_cstr(argv[1]), CharSequence_cstr("invalid")) == 0)

#endif
