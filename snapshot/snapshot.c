#include "vex.h"
#include "snapshot/snapshot.h"
#include <stdio.h>
#include <string.h>

;;DEFINITION
/* SeshSnapshot turns explicitly supplied bytes into one immutable upload job.
 * It copies input into borrowed caller storage before admission, then retains
 * that exact key/content across pending steps and retries. API Haven supplies
 * provider vocabulary; R1 supplies execution, transport and resource lifetimes.
 * No engine allocation or filesystem dependency is needed to prove this core.
 * This is backup scheduling, not a durable local journal or collaborative sync.
 * The provider must deduplicate keys; Drive mapping/OAuth are not implemented.
 */
;;OVERVIEW
/* CLASS: SeshSnapshot
 * STRUCT FIELDS (header order):
 * u8 *buffer: borrowed staging bytes; bytes capacity: admitted storage size;
 * bytes length: immutable queued byte count; HavenSnapshotPutFn put: provider;
 * any context: borrowed provider context; u64 key: unique snapshot identity;
 * u64 nextAttemptMs: earliest retry; u64 retryDelayMs: configurable delay;
 * u32 attempts: RETRY outcomes; u32 maxAttempts: retry exhaustion bound;
 * i32 state: idle/queued/saved/failed/cancelled.
 * PUBLIC: _0/zero; setBuffer/setProvider/setRetryPolicy (reject while queued);
 * queue (reject busy, zero key, null nonempty bytes, oversized input);
 * step (pending/retry/durable done/terminal reject); cancel (quiet idempotent);
 * getBuffer/Capacity/Length/Provider/Context/Key/NextAttemptMs/RetryDelayMs/
 * Attempts/MaxAttempts/State; toString/toStringStruct (bounded cold projections).
 * PRIVATE: configurable validates cold mutations; stringify formats projections.
 * State-derived fields are
 * changed through queue/step/cancel only, never unconstrained scalar setters.
 * No private helper classes. No owned resources. No concurrency claim.
 */
;;INTENTION("per the Single Class Per File Law (Java Law) + Conflict Triage Law: configuration has validated setters; job-derived fields mutate only through queue/step/cancel so callers cannot forge acknowledgement or break provider borrows")

;;GETTER
static inline u8 *SeshSnapshot_bufferMut(SeshSnapshot *self)
{
    return (*self).buffer;
}
;;SETTER
static inline fn SeshSnapshot_setBufferFields(SeshSnapshot *self, u8 *buffer, bytes capacity, bytes length) {
    (*self).buffer = buffer;
    (*self).capacity = capacity;
    (*self).length = length;
}
;;SETTER
static inline fn SeshSnapshot_setProviderFields(SeshSnapshot *self, HavenSnapshotPutFn put, any context) {
    (*self).put = put;
    (*self).context = context;
}
;;SETTER
static inline fn SeshSnapshot_setRetryFields(SeshSnapshot *self, u64 delayMs, u32 maxAttempts) {
    (*self).retryDelayMs = delayMs;
    (*self).maxAttempts = maxAttempts;
}
;;SETTER
static inline fn SeshSnapshot_setLength(SeshSnapshot *self, bytes length)
{
    (*self).length = length;
}
;;SETTER
static inline fn SeshSnapshot_setKey(SeshSnapshot *self, u64 key)
{
    (*self).key = key;
}
;;SETTER
static inline fn SeshSnapshot_setAttempts(SeshSnapshot *self, u32 attempts)
{
    (*self).attempts = attempts;
}
;;SETTER
static inline fn SeshSnapshot_setNextAttemptMs(SeshSnapshot *self, u64 nextAttemptMs)
{
    (*self).nextAttemptMs = nextAttemptMs;
}
;;SETTER
static inline fn SeshSnapshot_setState(SeshSnapshot *self, i32 state)
{
    (*self).state = state;
}

/** Allows configuration changes only when a job is not queued. */
static bool configurable(SeshSnapshot *self) {
    if (self == nullptr || SeshSnapshot_getState(self) == SESH_SNAPSHOT_QUEUED) {
        THROW("snapshot configuration: null or active job");
        return false;
    }
    return true;
}

/** Returns an idle job with the documented default retry delay and attempt limit. */
SeshSnapshot SeshSnapshot_0() {
    return (SeshSnapshot) { .retryDelayMs = SESH_SNAPSHOT_RETRY_DELAY_DEFAULT,
                           .maxAttempts = SESH_SNAPSHOT_MAX_ATTEMPTS_DEFAULT };
}

/** Sets borrowed staging storage while idle and clears the queued byte length. */
bool SeshSnapshot_setBuffer(SeshSnapshot *self, u8 *buffer, bytes capacity) {
    if (!configurable(self))
        return false;
    if (buffer == nullptr || capacity == 0) {
        THROW("snapshot buffer: missing storage");
        return false;
    }
    SeshSnapshot_setBufferFields(self, buffer, capacity, 0);
    return true;
}

/** Sets the provider callback and borrowed context while idle. */
bool SeshSnapshot_setProvider(SeshSnapshot *self, HavenSnapshotPutFn put, any context) {
    if (!configurable(self))
        return false;
    if (put == nullptr) {
        THROW("snapshot provider: missing callback");
        return false;
    }
    SeshSnapshot_setProviderFields(self, put, context);
    return true;
}

/** Sets nonzero retry delay and attempt limit while idle. */
bool SeshSnapshot_setRetryPolicy(SeshSnapshot *self, u64 delayMs, u32 maxAttempts) {
    if (!configurable(self))
        return false;
    if (delayMs == 0 || maxAttempts == 0) {
        THROW("snapshot retry policy: zero delay or attempts");
        return false;
    }
    SeshSnapshot_setRetryFields(self, delayMs, maxAttempts);
    return true;
}

/** Copies bytes into staging storage and admits one keyed job if configured and idle. */
bool SeshSnapshot_queue(SeshSnapshot *self, u64 key, const u8 *data, bytes length) {
    if (self == nullptr || key == 0 || (data == nullptr && length != 0)) {
        THROW("snapshot queue: invalid input");
        return false;
    }
    if (SeshSnapshot_getState(self) == SESH_SNAPSHOT_QUEUED || SeshSnapshot_getBuffer(self) == nullptr ||
        SeshSnapshot_getProvider(self) == nullptr || length > SeshSnapshot_getCapacity(self)) {
        THROW("snapshot queue: busy, unconfigured or oversized");
        return false;
    }
    if (length != 0)
        memmove(SeshSnapshot_bufferMut(self), data, length);
    SeshSnapshot_setLength(self, length);
    SeshSnapshot_setKey(self, key);
    SeshSnapshot_setAttempts(self, 0);
    SeshSnapshot_setNextAttemptMs(self, 0);
    SeshSnapshot_setState(self, SESH_SNAPSHOT_QUEUED);
    return true;
}

/** Advances an eligible job once using caller time and the provider's result. */
i32 SeshSnapshot_step(SeshSnapshot *self, u64 nowMs) {
    if (self == nullptr) {
        THROW("snapshot step: null job");
        return SESH_SNAPSHOT_FAILED;
    }
    if (SeshSnapshot_getState(self) != SESH_SNAPSHOT_QUEUED || nowMs < SeshSnapshot_getNextAttemptMs(self))
        return SeshSnapshot_getState(self);
    i32 result = SeshSnapshot_getProvider(self)(SeshSnapshot_getContext(self), SeshSnapshot_getKey(self),
                                            SeshSnapshot_getBuffer(self), SeshSnapshot_getLength(self), 0);
    if (result == HAVEN_SNAPSHOT_DONE)
        SeshSnapshot_setState(self, SESH_SNAPSHOT_SAVED);
    else if (result == HAVEN_SNAPSHOT_RETRY) {
        u32 attempts = SeshSnapshot_getAttempts(self) + 1;
        SeshSnapshot_setAttempts(self, attempts);
        u64 retryDelay = SeshSnapshot_getRetryDelayMs(self);
        if (attempts >= SeshSnapshot_getMaxAttempts(self) || nowMs > UINT64_MAX - retryDelay) {
            SeshSnapshot_cancel(self);
            SeshSnapshot_setState(self, SESH_SNAPSHOT_FAILED);
            THROW("snapshot step: retry exhausted or clock overflow");
        } else
            SeshSnapshot_setNextAttemptMs(self, nowMs + retryDelay);
    } else if (result != HAVEN_SNAPSHOT_PENDING) {
        SeshSnapshot_cancel(self);
        SeshSnapshot_setState(self, SESH_SNAPSHOT_FAILED);
        THROW("snapshot step: provider rejected snapshot");
    }
    return SeshSnapshot_getState(self);
}

/** Cancels a queued job through the provider; null and inactive jobs are ignored. */
fn SeshSnapshot_cancel(SeshSnapshot *self) {
    if (self == nullptr || SeshSnapshot_getState(self) != SESH_SNAPSHOT_QUEUED)
        return;
    SeshSnapshot_getProvider(self)(SeshSnapshot_getContext(self), SeshSnapshot_getKey(self),
                                   SeshSnapshot_getBuffer(self), SeshSnapshot_getLength(self), 1);
    SeshSnapshot_setState(self, SESH_SNAPSHOT_CANCELLED);
}

/** Returns the borrowed staging buffer, or nullptr for null self. */
;;GETTER
const u8 *SeshSnapshot_getBuffer(const SeshSnapshot *self)
{
    return self == nullptr ? nullptr : (*self).buffer;
}
/** Returns the staging buffer capacity, or zero for null self. */
;;GETTER
bytes SeshSnapshot_getCapacity(const SeshSnapshot *self)
{
    return self == nullptr ? 0 : (*self).capacity;
}
/** Returns the queued byte length, or zero for null self. */
;;GETTER
bytes SeshSnapshot_getLength(const SeshSnapshot *self)
{
    return self == nullptr ? 0 : (*self).length;
}
/** Returns the provider callback, or nullptr for null self. */
;;GETTER
HavenSnapshotPutFn SeshSnapshot_getProvider(const SeshSnapshot *self)
{
    return self == nullptr ? nullptr : (*self).put;
}
/** Returns the borrowed provider context, or nullptr for null self. */
;;GETTER
any SeshSnapshot_getContext(const SeshSnapshot *self)
{
    return self == nullptr ? nullptr : (*self).context;
}
/** Returns the snapshot key, or zero for null self. */
;;GETTER
u64 SeshSnapshot_getKey(const SeshSnapshot *self)
{
    return self == nullptr ? 0 : (*self).key;
}
/** Returns the next eligible retry time, or zero for null self. */
;;GETTER
u64 SeshSnapshot_getNextAttemptMs(const SeshSnapshot *self)
{
    return self == nullptr ? 0 : (*self).nextAttemptMs;
}
/** Returns the configured retry delay, or zero for null self. */
;;GETTER
u64 SeshSnapshot_getRetryDelayMs(const SeshSnapshot *self)
{
    return self == nullptr ? 0 : (*self).retryDelayMs;
}
/** Returns the number of retry outcomes seen, or zero for null self. */
;;GETTER
u32 SeshSnapshot_getAttempts(const SeshSnapshot *self)
{
    return self == nullptr ? 0 : (*self).attempts;
}
/** Returns the retry attempt limit, or zero for null self. */
;;GETTER
u32 SeshSnapshot_getMaxAttempts(const SeshSnapshot *self)
{
    return self == nullptr ? 0 : (*self).maxAttempts;
}
/** Returns the current job state, or IDLE for null self. */
;;GETTER
i32 SeshSnapshot_getState(const SeshSnapshot *self)
{
    return self == nullptr ? SESH_SNAPSHOT_IDLE : (*self).state;
}

/** Writes either the value summary or one-level field dump into a bounded buffer. */
static bool stringify(const SeshSnapshot *self, bool structure, utf8 *dest, bytes cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("snapshot string: missing destination");
        return false;
    }
    i32 count;
    if (self == nullptr)
        count = snprintf(dest, cap, "nullptr");
    else if (structure)
        count = snprintf(dest, cap, "SeshSnapshot{buffer=%p,capacity=%zu,length=%zu,put=%s,context=%p,key=%llu,nextAttemptMs=%llu,retryDelayMs=%llu,attempts=%u,maxAttempts=%u,state=%d}",
                         (any) SeshSnapshot_getBuffer(self), SeshSnapshot_getCapacity(self), SeshSnapshot_getLength(self),
                         SeshSnapshot_getProvider(self) == nullptr ? "nullptr" : "bound", SeshSnapshot_getContext(self),
                         (u64) SeshSnapshot_getKey(self), (u64) SeshSnapshot_getNextAttemptMs(self),
                         (u64) SeshSnapshot_getRetryDelayMs(self), SeshSnapshot_getAttempts(self),
                         SeshSnapshot_getMaxAttempts(self), SeshSnapshot_getState(self));
    else
        count = snprintf(dest, cap, "snapshot %llu: %zu bytes, state %d",
                         (u64) SeshSnapshot_getKey(self), SeshSnapshot_getLength(self), SeshSnapshot_getState(self));
    if (count < 0 || (bytes) count >= cap) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("snapshot string: truncated");
        return false;
    }
    return true;
}

/** Formats the job's key, byte count, and state into a bounded value summary. */
bool SeshSnapshot_toString(const SeshSnapshot *self, utf8 *dest, bytes cap, bool *outTruncated) {
    return stringify(self, false, dest, cap, outTruncated);
}

/** Formats all snapshot fields into a bounded one-level structure summary. */
bool SeshSnapshot_toStringStruct(const SeshSnapshot *self, utf8 *dest, bytes cap, bool *outTruncated) {
    return stringify(self, true, dest, cap, outTruncated);
}
