// GameState object table and per-object hashes (phase 2.6, 2026-10-19). The table is the generated
// gamestate_meta.inc; the hash is sgh64 of re/tools/state_image.py (the original-side tracer computes the same), so a
// port state compares with the recorded frame hashes of the segment corpus.
#ifndef PORTSIM_STATE_HASH_H
#define PORTSIM_STATE_HASH_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace portsim {

struct ObjectInfo {
  const char *name;
  uint32_t off, size;
  const char *cls;  // sim / hidden / ui / render / audio / pointer / engine / const
};

// sgh64: dwords (last one zero-padded) w_i, s = sum (w_i ^ 0x5bd1e995) * ((2i + 1) * 0x9e3779b97f4a7c15) mod 2^64,
// then splitmix64's finaliser of s ^ n.
uint64_t sgh64(const uint8_t *data, size_t n);

const std::vector<ObjectInfo> &objects();                      // every object, meta order
const std::vector<const ObjectInfo *> &objects_of(const char *cls);  // one class (nullptr = all); empty if unknown
// sgh64 of one object of a GameState image, SG_POINTER_WORD dwords hashed as 0
uint64_t object_hash(const uint8_t *state, const ObjectInfo &o);

}  // namespace portsim

#endif  // PORTSIM_STATE_HASH_H
