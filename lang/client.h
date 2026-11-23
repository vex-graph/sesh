#ifndef SESH_LANG_CLIENT_H
#define SESH_LANG_CLIENT_H

#include "vex.h"
#include "lang/arity.h"
/* Graphvex exports Client; this session identity is deliberately SeshClient.
 * Host assigns/persists a nonzero installation ID. No hardware/IP identity and
 * no automatic global uniqueness or persistence is claimed. External serialization.
 */
typedef struct SeshClient { u64 id; } SeshClient;
SeshClient SeshClient_0();
SeshClient SeshClient_1(u64 id);
#define SeshClient(...) SESH_CONSTRUCT(SeshClient, __VA_ARGS__)
#define SeshClient_zero() SeshClient_0()
bool SeshClient_setId(SeshClient *self, u64 id);
u64 SeshClient_getId(const SeshClient *self);
bool SeshClient_toString(const SeshClient *self, utf8 *dest, bytes cap, bool *outTruncated);
bool SeshClient_toStringStruct(const SeshClient *self, utf8 *dest, bytes cap, bool *outTruncated);
#endif
