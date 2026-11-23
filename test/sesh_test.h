#ifndef VEX_SESH_TEST_H
#define VEX_SESH_TEST_H

#include "vex.h"

/**
 * Single suite entry in the consolidated vex/sesh automated test runner.
 * name: CLI filter token matching argv[1].
 * run: suite entrypoint returning 0 on pass or non-zero on failure.
 */
typedef struct SuiteEntry {
    const utf8 *name;
    i32 (*run)();
} SuiteEntry;

#endif
