#ifndef NIO_SESH_MEMORY_H
#define NIO_SESH_MEMORY_H

#include "vex.h"
#include "nio/mem.h"

MemoryArena *Sesh_arena();
fn Sesh_arenaShutdown();

#endif
