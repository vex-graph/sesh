#ifndef SESH_SNAPSHOT_TEST_H
#define SESH_SNAPSHOT_TEST_H

#include "vex.h"

/**
 * In-memory fake snapshot provider record for SeshSnapshot owner tests.
 * u8 bytes[256] - stored snapshot payload bytes
 * bytes length - stored payload byte length
 * u64 key - key of the stored snapshot
 * u32 calls - count of non-cancelled put invocations
 * u32 cancels - count of cancelled put invocations
 * i32 result - scripted provider return status
 */
typedef struct Fake {
    u8 bytes[256];
    bytes length;
    u64 key;
    u32 calls;
    u32 cancels;
    i32 result;
} Fake;

#endif
