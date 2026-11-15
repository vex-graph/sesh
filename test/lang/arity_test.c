/* Header-only arity owner. Every advertised class chooser is exercised by its
 * class owner; unsupported counts are compile-negative probes in session_run.py. */

#include "vex.h"
#include "lang/arity.h"
#include <assert.h>
i32 main() {
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
