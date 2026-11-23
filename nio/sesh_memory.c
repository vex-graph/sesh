#include "vex.h"
#include "nio/sesh_memory.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SeshMemory (sesh/nio/sesh_memory.c)
 * ============================================================================
 * Per-library MemoryArena owner for Sesh, backed by Relational Engine's
 * MemoryPool. Starts at zero OS pages and grows on demand until OOM.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: SeshMemory (nio/sesh_memory)
 * ============================================================================
 * Lazily creates and shuts down Sesh's dedicated MemoryArena.
 * ============================================================================
 */

static MemoryArena *s_seshArena = nullptr;

/** Lazily create and return Sesh's page-pooled MemoryArena. */
MemoryArena *Sesh_arena() {
    if (s_seshArena == nullptr)
        s_seshArena = MemoryArena();
    return s_seshArena;
}

/** Destroy Sesh's MemoryArena and return all pages to MemoryPool. */
fn Sesh_arenaShutdown() {
    if (s_seshArena != nullptr) {
        MemoryArena_destroy(s_seshArena);
        s_seshArena = nullptr;
    }
}
