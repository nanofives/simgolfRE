// ExeConstants (portsim/execonst.h): the user's exe, verified against the descriptor hashes, phase 3.0.
#define _CRT_SECURE_NO_WARNINGS  // std::fopen (MSVC /W4 /WX)
#include "portsim/execonst.h"

#include <cstdio>

#include "portsim/state_hash.h"

namespace portsim {

namespace {
uint32_t rd32(const std::vector<uint8_t> &f, size_t off) {
  uint32_t v = 0;
  if (off + 4 <= f.size()) std::memcpy(&v, f.data() + off, 4);
  return v;
}
uint16_t rd16(const std::vector<uint8_t> &f, size_t off) {
  uint16_t v = 0;
  if (off + 2 <= f.size()) std::memcpy(&v, f.data() + off, 2);
  return v;
}
}  // namespace

const std::vector<ExeConstDesc> &ExeConstants::descriptors() {
  static const std::vector<ExeConstDesc> all = {
#define SG_EXE_CONST(sym, off, size, elem, count, stride, kind, hash) {#sym, off, size, elem, count, stride, kind, hash},
#include "portsim/execonst_desc.inc"
#undef SG_EXE_CONST
  };
  return all;
}

const ExeConstDesc *ExeConstants::find(const char *name) {
  if (!name) return nullptr;
  for (const ExeConstDesc &d : descriptors())
    if (std::strcmp(d.name, name) == 0) return &d;
  return nullptr;
}

bool ExeConstants::load_file(const char *path, std::string *err) {
  std::vector<uint8_t> buf;
  FILE *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f) {
    if (err) *err = std::string("cannot open ") + (path ? path : "(null)");
    return false;
  }
  uint8_t chunk[65536];
  size_t n;
  while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) buf.insert(buf.end(), chunk, chunk + n);
  std::fclose(f);
  return load_bytes(buf.data(), buf.size(), err);
}

bool ExeConstants::load_bytes(const uint8_t *data, size_t n, std::string *err) {
  file_.clear();
  data_off_ = data_raw_size_ = 0;
  auto fail = [&](const std::string &why) {
    file_.clear();
    if (err) *err = why;
    return false;
  };
  if (!data || n < 0x40) return fail("not a PE file");
  file_.assign(data, data + n);
  const uint32_t pe = rd32(file_, 0x3c);
  if (rd16(file_, 0) != 0x5a4d || rd32(file_, pe) != 0x00004550u) return fail("not a PE file");
  const uint16_t nsec = rd16(file_, pe + 6), opt = rd16(file_, pe + 20);
  const size_t sec0 = pe + 24 + opt;
  bool found = false;
  for (uint16_t i = 0; i < nsec; ++i) {
    const size_t s = sec0 + 40u * i;
    if (s + 40 > file_.size()) return fail("truncated section table");
    if (std::memcmp(file_.data() + s, ".data\0\0\0", 8) != 0) continue;
    const uint32_t vsize = rd32(file_, s + 8), raw = rd32(file_, s + 16), ptr = rd32(file_, s + 20);
    if (vsize != sizeof(GameState)) return fail(".data virtual size differs from sizeof(GameState): another build");
    if (static_cast<uint64_t>(ptr) + raw > file_.size() || raw > vsize) return fail(".data raw range outside the file");
    data_off_ = ptr;
    data_raw_size_ = raw;
    found = true;
  }
  if (!found) return fail("no .data section");
  for (const ExeConstDesc &d : descriptors()) {
    if (static_cast<uint64_t>(d.file_off) + d.size > file_.size())
      return fail(std::string("descriptor outside the file: ") + d.name);
    if (sgh64(file_.data() + d.file_off, d.size) != d.hash)
      return fail(std::string("descriptor bytes differ from the anchored exe: ") + d.name);
  }
  return true;
}

const uint8_t *ExeConstants::bytes(const char *name, uint32_t *size) const {
  if (!loaded()) throw MissingResource{"exe constants not loaded"};
  const ExeConstDesc *d = find(name);
  if (!d) throw MissingResource{"unknown exe constant"};
  if (size) *size = d->size;
  return file_.data() + d->file_off;
}

void ExeConstants::fill_state(GameState &gs, Fill mode) const {
  if (!loaded()) throw MissingResource{"exe constants not loaded"};
  uint8_t *dst = reinterpret_cast<uint8_t *>(&gs);
  const uint8_t *src = data_raw();
  if (mode == Fill::InitialImage) {
    std::memcpy(dst, src, data_raw_size_);
    return;
  }
  for (const ObjectInfo *o : objects_of("const")) {
    if (o->off >= data_raw_size_) continue;  // a run-time table in the bss (g_sinTable): not in the file
    const uint32_t n = o->off + o->size <= data_raw_size_ ? o->size : data_raw_size_ - o->off;
    std::memcpy(dst + o->off, src + o->off, n);
  }
}

ExeConstants &default_exe_constants() {
  static ExeConstants instance;
  return instance;
}

}  // namespace portsim
