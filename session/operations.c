#include "vex.h"
#include "lang/operations.h"
#include "session/text.h"
#include <string.h>
;;DEFINITION
/* Operations is a flat copied-intent ledger with explicit host-driven growth.
 * Capacity is current borrowed storage, never a permanent entity ceiling. The
 * host reserves larger rows cold; failure preserves history and resource state.
 * Replays are idempotent only while history is retained. clear discards dedup
 * memory and requires exclusion/new identity epoch before sessions resume.
 */
;;OVERVIEW
/* CLASS: Operations. STRUCT FIELDS: OperationsSlot *items: borrowed contiguous rows;
 * bytes count: initialized rows; bytes capacity: current admitted row capacity.
 * PUBLIC: _0/_2/chooser/zero; reserve (reject null, zero, undersized, overflow);
 * add (copied admission, identical replay, changed-identity rejection, full storage);
 * find (copies output, absence unchanged/quiet, invalid input rejects);
 * isApplied (null/absent=false), apply (local revision+receipt, replay/conflict/reject);
 * getCount/getCapacity (null=0), clear (quiet, external exclusion required), projections.
 * SLOT RECORD: OperationsSlot: Operation operation: copied intent; bool applied:
 * receipt for one local revision advance (false until apply succeeds).
 * No private helpers. Count/items controlled through reserve/add/clear, not raw setters.
 */
;;INTENTION("per the Conflict Triage Law + Single Class Per File Law (Java Law): reserve/add/clear own backing/count updates; no scalar setter may forge deduplication history")
;;GETTER
static inline OperationsSlot *Operations_items(const Operations *self) { return (*self).items; }
;;SETTER
static inline fn Operations_setItems(Operations *self, OperationsSlot *items) { (*self).items = items; }
;;SETTER
static inline fn Operations_setCount(Operations *self, bytes count) { (*self).count = count; }
;;SETTER
static inline fn Operations_setCapacity(Operations *self, bytes capacity) { (*self).capacity = capacity; }
;;GETTER
static inline Operation *OperationsSlot_operation(OperationsSlot *self) { return &(*self).operation; }
;;GETTER
static inline const Operation *OperationsSlot_operationConst(const OperationsSlot *self) { return &(*self).operation; }
;;SETTER
static inline fn OperationsSlot_setOperation(OperationsSlot *self, Operation operation) { (*self).operation = operation; }
;;GETTER
static inline bool OperationsSlot_applied(const OperationsSlot *self) { return (*self).applied; }
;;SETTER
static inline fn OperationsSlot_setApplied(OperationsSlot *self, bool applied) { (*self).applied = applied; }

// Returns an empty ledger with no borrowed backing storage.
Operations Operations_0() { return (Operations) {0}; }
// Binds caller-provided storage as the ledger's initial capacity.
Operations Operations_2(OperationsSlot *items, bytes capacity) {
    Operations result = Operations_0();
    Operations_reserve(&result, items, capacity);
    return result;
}
// Rebinds to equal-or-larger caller storage, preserving all admitted rows.
bool Operations_reserve(Operations *self, OperationsSlot *items, bytes capacity) {
    if (self == nullptr || items == nullptr || capacity == 0 || capacity > SIZE_MAX / sizeof(OperationsSlot)) {
        THROW("operations reserve: invalid storage");
        return false;
    }
    if (capacity < Operations_getCapacity(self)) {
        THROW("operations reserve: cannot shrink storage");
        return false;
    }
    if (Operations_getCount(self) != 0)
        memmove(items, Operations_items(self), Operations_getCount(self) * sizeof(OperationsSlot));
    Operations_setItems(self, items);
    Operations_setCapacity(self, capacity);
    return true;
}
// Finds a client-scoped operation ID and copies it to dest; absence leaves dest unchanged.
bool Operations_find(const Operations *self, u64 clientId, u64 id, Operation *dest) {
    if (self == nullptr || dest == nullptr || clientId == 0 || id == 0) {
        THROW("operations find: invalid input");
        return false;
    }
    bytes count = Operations_getCount(self);
    OperationsSlot *items = Operations_items(self);
    for (bytes i = 0; i < count; i++) {
        const OperationsSlot *slot = &items[i];
        const Operation *row = OperationsSlot_operationConst(slot);
        if (Operation_getClientId(row) == clientId && Operation_getId(row) == id) {
            *dest = *row;
            return true;
        }
    }
    return false;
}
// Copies new intent, reports identical replay, and rejects changed or full admissions.
i32 Operations_add(Operations *self, const Operation *operation) {
    if (self == nullptr || !Operation_isValid(operation)) {
        THROW("operations add: invalid operation");
        return OPERATIONS_REJECTED;
    }
    Operation previous;
    if (Operations_find(self, Operation_getClientId(operation), Operation_getId(operation), &previous)) {
        if (Operation_equals(&previous, operation))
            return OPERATIONS_REPLAY;
        THROW("operations add: identity reused with changed intent");
        return OPERATIONS_REJECTED;
    }
    bytes count = Operations_getCount(self);
    if (count == Operations_getCapacity(self)) {
        THROW("operations add: reserve larger storage");
        return OPERATIONS_REJECTED;
    }
    OperationsSlot *slot = &Operations_items(self)[count];
    OperationsSlot_setOperation(slot, *operation);
    OperationsSlot_setApplied(slot, false);
    Operations_setCount(self, count + 1);
    return OPERATIONS_ADDED;
}
// Reports whether the matching admitted intent has a successful apply receipt.
bool Operations_isApplied(const Operations *self, u64 clientId, u64 id) {
    if (self == nullptr)
        return false;
    bytes count = Operations_getCount(self);
    OperationsSlot *items = Operations_items(self);
    for (bytes i = 0; i < count; i++) {
        const OperationsSlot *slot = &items[i];
        const Operation *row = OperationsSlot_operationConst(slot);
        if (Operation_getClientId(row) == clientId && Operation_getId(row) == id)
            return OperationsSlot_applied(slot);
    }
    return false;
}
// Applies admitted intent once when scope and expected revision match the resource.
i32 Operations_apply(Operations *self, const Operation *operation, Resource *resource) {
    if (self == nullptr || !Operation_isValid(operation) || resource == nullptr ||
        Operation_getResourceId(operation) != Resource_getId(resource) ||
        Operation_getWorkspaceId(operation) != Resource_getWorkspaceId(resource)) {
        THROW("operations apply: invalid scope");
        return OPERATIONS_REJECTED;
    }
    bytes count = Operations_getCount(self);
    OperationsSlot *items = Operations_items(self);
    for (bytes i = 0; i < count; i++) {
        OperationsSlot *slot = &items[i];
        Operation *row = OperationsSlot_operation(slot);
        if (Operation_getClientId(row) != Operation_getClientId(operation) || Operation_getId(row) != Operation_getId(operation))
            continue;
        if (!Operation_equals(row, operation)) {
            THROW("operations apply: changed intent");
            return OPERATIONS_REJECTED;
        }
        if (OperationsSlot_applied(slot))
            return OPERATIONS_REPLAY;
        u64 revision = Resource_getRevision(resource);
        if (revision != Operation_getExpectedRevision(operation)) {
            THROW("operations apply: revision conflict");
            return OPERATIONS_CONFLICT;
        }
        if (revision == UINT64_MAX) {
            THROW("operations apply: revision overflow");
            return OPERATIONS_REJECTED;
        }
        Resource_setRevision(resource, revision + UINT64_C(1));
        OperationsSlot_setApplied(slot, true);
        return OPERATIONS_ADDED;
    }
    THROW("operations apply: intent not admitted");
    return OPERATIONS_REJECTED;
}
// Returns the number of initialized ledger rows, or zero for null.
;;GETTER
bytes Operations_getCount(const Operations *self) { return self == nullptr ? 0 : (*self).count; }
// Returns the current caller-provided row capacity, or zero for null.
;;GETTER
bytes Operations_getCapacity(const Operations *self) { return self == nullptr ? 0 : (*self).capacity; }
// Forgets all rows without releasing the borrowed storage; caller must exclude users.
fn Operations_clear(Operations *self) {
    if (self != nullptr)
        Operations_setCount(self, 0);
}
// Formats the current initialized-row count as a bounded value summary.
SESH_VALUE_STRING(Operations, SeshText_format(dest, cap, outTruncated, "%zu operations", Operations_getCount(self)))
// Formats the backing pointer and ledger counts into a bounded structure summary.
bool Operations_toStringStruct(const Operations *self, char *dest, bytes cap, bool *outTruncated) {
    return self == nullptr ? SeshText_format(dest, cap, outTruncated, "nullptr") :
        SeshText_format(dest, cap, outTruncated, "Operations{items=%p,count=%zu,capacity=%zu}",
                        (any) Operations_items(self), Operations_getCount(self), Operations_getCapacity(self));
}
