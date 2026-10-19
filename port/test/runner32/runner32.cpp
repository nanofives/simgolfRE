// portsim Win32 runner (phase 3.R, 2026-10-19): serves the portsim C ABI (include/portsim_c.h) of the Win32 build of
// portsim_c.dll to the 64-bit Python harness over stdin / stdout, so port functions run with real x87 (fldcw) code.
//
//   portsim_runner32.exe <path to the Win32 portsim_c.dll>
//
// Protocol (all little endian; port/test/lib32.py is the client; documented in port/test/README.md):
//   request  u32 length, u32 op, payload       reply  u32 length, i32 status, payload
// Handle ops (state, call, hash, run_frames, exe fill, files) start with  u32 handle, then a PENDING list of address
// space edits the client queued (set base / map / unmap_all), applied in order first. The GameState lives in a named
// file mapping the client created: before every handle op it is copied into the DLL's state and after CALL / RUN_FRAMES /
// EXE_FILL it is copied back, so Python reads and writes the GameState through its own view with no pipe traffic.
// Blocks (sg_map) are runner-owned buffers; the client sends the writable ones with each CALL and gets them back.
// Read-only blocks with a nonzero token are cached across unmap_all (the exe sections are mapped for every vector).
// No port code and no original address is in this file.
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "portsim_c.h"

namespace {

// ------------------------------------------------------------------------------------------------ the DLL
struct Api {
#define F(ret, name, args) ret(__cdecl *name) args = nullptr;
  F(size_t, sg_state_size, (void))
  F(sg_ctx *, sg_state_new, (void))
  F(void, sg_state_free, (sg_ctx *))
  F(uint8_t *, sg_state_ptr, (sg_ctx *))
  F(int, sg_set_data_base, (sg_ctx *, uint32_t))
  F(int, sg_map, (sg_ctx *, uint32_t, void *, uint32_t, int32_t))
  F(void, sg_unmap_all, (sg_ctx *))
  F(int32_t, sg_func_count, (void))
  F(int, sg_func_info_get, (int32_t, sg_func_info *))
  F(int, sg_call, (sg_ctx *, const char *, const uint32_t *, int32_t, sg_ret *))
  F(int, sg_set_fpcw, (uint32_t))
  F(int, sg_negctl_set, (const char *, int32_t))
  F(int32_t, sg_negctl_get, (const char *))
  F(void, sg_negctl_reset, (void))
  F(int32_t, sg_object_count, (const char *))
  F(const char *, sg_object_name, (const char *, int32_t))
  F(int, sg_object_span, (const char *, int32_t, uint32_t *, uint32_t *))
  F(uint64_t, sg_hash_bytes, (const uint8_t *, size_t))
  F(int32_t, sg_hash_class, (sg_ctx *, const char *, uint64_t *, int32_t))
  F(int, sg_run_frames, (sg_ctx *, const uint8_t *, size_t, const sg_msg *, int32_t, int32_t, uint32_t *, uint64_t *, int32_t *))
  F(int32_t, sg_defect_count, (void))
  F(int, sg_defect_info_get, (int32_t, sg_defect_info *))
  F(int, sg_defect_set, (const char *, int32_t))
  F(int32_t, sg_defect_get, (const char *))
  F(void, sg_defect_reset, (void))
  F(int, sg_exe_load, (const char *, char *, size_t))
  F(int32_t, sg_exe_loaded, (void))
  F(int, sg_exe_const, (const char *, uint8_t *, uint32_t, uint32_t *))
  F(int, sg_exe_fill_state, (sg_ctx *, int32_t))
  F(int, sg_files_put, (sg_ctx *, const char *, const uint8_t *, size_t))
  F(void, sg_files_clear, (sg_ctx *))
  F(int, sg_files_root, (sg_ctx *, const char *))
  F(int64_t, sg_files_size, (sg_ctx *, const char *))
  F(const char *, sg_build_info, (void))
#undef F
};
Api A;

bool load_api(const char *path) {
  HMODULE h = LoadLibraryA(path);
  if (!h) return false;
  bool ok = true;
#define F(ret, name, args)                                                \
  A.name = reinterpret_cast<decltype(A.name)>(GetProcAddress(h, #name)); \
  ok = ok && A.name != nullptr;
  F(size_t, sg_state_size, (void))
  F(sg_ctx *, sg_state_new, (void))
  F(void, sg_state_free, (sg_ctx *))
  F(uint8_t *, sg_state_ptr, (sg_ctx *))
  F(int, sg_set_data_base, (sg_ctx *, uint32_t))
  F(int, sg_map, (sg_ctx *, uint32_t, void *, uint32_t, int32_t))
  F(void, sg_unmap_all, (sg_ctx *))
  F(int32_t, sg_func_count, (void))
  F(int, sg_func_info_get, (int32_t, sg_func_info *))
  F(int, sg_call, (sg_ctx *, const char *, const uint32_t *, int32_t, sg_ret *))
  F(int, sg_set_fpcw, (uint32_t))
  F(int, sg_negctl_set, (const char *, int32_t))
  F(int32_t, sg_negctl_get, (const char *))
  F(void, sg_negctl_reset, (void))
  F(int32_t, sg_object_count, (const char *))
  F(const char *, sg_object_name, (const char *, int32_t))
  F(int, sg_object_span, (const char *, int32_t, uint32_t *, uint32_t *))
  F(uint64_t, sg_hash_bytes, (const uint8_t *, size_t))
  F(int32_t, sg_hash_class, (sg_ctx *, const char *, uint64_t *, int32_t))
  F(int, sg_run_frames, (sg_ctx *, const uint8_t *, size_t, const sg_msg *, int32_t, int32_t, uint32_t *, uint64_t *, int32_t *))
  F(int32_t, sg_defect_count, (void))
  F(int, sg_defect_info_get, (int32_t, sg_defect_info *))
  F(int, sg_defect_set, (const char *, int32_t))
  F(int32_t, sg_defect_get, (const char *))
  F(void, sg_defect_reset, (void))
  F(int, sg_exe_load, (const char *, char *, size_t))
  F(int32_t, sg_exe_loaded, (void))
  F(int, sg_exe_const, (const char *, uint8_t *, uint32_t, uint32_t *))
  F(int, sg_exe_fill_state, (sg_ctx *, int32_t))
  F(int, sg_files_put, (sg_ctx *, const char *, const uint8_t *, size_t))
  F(void, sg_files_clear, (sg_ctx *))
  F(int, sg_files_root, (sg_ctx *, const char *))
  F(int64_t, sg_files_size, (sg_ctx *, const char *))
  F(const char *, sg_build_info, (void))
#undef F
  return ok;
}

// ------------------------------------------------------------------------------------------------ framing
bool read_all(void *buf, size_t n) {
  uint8_t *p = static_cast<uint8_t *>(buf);
  while (n) {
    DWORD got = 0;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), p, static_cast<DWORD>(n > 0x100000 ? 0x100000 : n), &got, nullptr) || got == 0)
      return false;
    p += got;
    n -= got;
  }
  return true;
}

bool write_all(const void *buf, size_t n) {
  const uint8_t *p = static_cast<const uint8_t *>(buf);
  while (n) {
    DWORD put = 0;
    if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), p, static_cast<DWORD>(n > 0x100000 ? 0x100000 : n), &put, nullptr)) return false;
    p += put;
    n -= put;
  }
  return true;
}

struct Reader {
  const uint8_t *p;
  const uint8_t *end;
  bool bad = false;
  template <class T> T get() {
    T v{};
    if (static_cast<size_t>(end - p) < sizeof(T)) {
      bad = true;
      return v;
    }
    memcpy(&v, p, sizeof(T));
    p += sizeof(T);
    return v;
  }
  const uint8_t *bytes(size_t n) {
    if (static_cast<size_t>(end - p) < n) {
      bad = true;
      return nullptr;
    }
    const uint8_t *r = p;
    p += n;
    return r;
  }
  std::string str() {
    uint32_t n = get<uint32_t>();
    const uint8_t *b = bytes(n);
    return b ? std::string(reinterpret_cast<const char *>(b), n) : std::string();
  }
};

struct Writer {
  std::vector<uint8_t> b;
  template <class T> void put(T v) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&v);
    b.insert(b.end(), p, p + sizeof(T));
  }
  void bytes(const void *d, size_t n) {
    const uint8_t *p = static_cast<const uint8_t *>(d);
    b.insert(b.end(), p, p + n);
  }
  void str(const char *s) {
    uint32_t n = s ? static_cast<uint32_t>(strlen(s)) : 0;
    put<uint32_t>(n);
    bytes(s, n);
  }
};

// ------------------------------------------------------------------------------------------------ state
struct Block {
  uint32_t base = 0, size = 0;
  bool ro = false;
  std::shared_ptr<std::vector<uint8_t>> buf;
};

struct State {
  sg_ctx *ctx = nullptr;
  uint8_t *dll_state = nullptr;
  uint8_t *shm = nullptr;
  HANDLE map = nullptr;
  size_t size = 0;
  std::vector<Block> blocks;
};

std::map<uint32_t, State> g_states;
uint32_t g_next_handle = 1;
std::map<uint64_t, std::shared_ptr<std::vector<uint8_t>>> g_ro_cache;
const uint32_t kDefaultCw = 0x027f;

// the x87 control word and tag word after a call (port code that leaves either changed is reported, then repaired)
void x87_state(uint16_t *cw, uint16_t *tag) {
  uint16_t c = 0, t = 0;
  uint8_t env[28];
  __asm {
    fnstcw c
    fnstenv env
    fldenv env
  }
  t = static_cast<uint16_t>(env[8] | (env[9] << 8));
  *cw = c;
  *tag = t;
}

void x87_reset() {
  __asm fninit
  A.sg_set_fpcw(kDefaultCw);
}

// apply the pending address-space edits; returns the first failing status (later edits are skipped)
int apply_pending(State &s, Reader &r) {
  uint32_t n = r.get<uint32_t>();
  int status = SG_OK;
  for (uint32_t i = 0; i < n && !r.bad; ++i) {
    uint32_t kind = r.get<uint32_t>();
    if (kind == 1) {  // set base
      uint32_t base = r.get<uint32_t>();
      if (status == SG_OK) status = A.sg_set_data_base(s.ctx, base);
    } else if (kind == 2) {  // map
      Block b;
      b.base = r.get<uint32_t>();
      b.size = r.get<uint32_t>();
      b.ro = r.get<uint32_t>() != 0;
      uint64_t token = r.get<uint64_t>();
      uint32_t has_data = r.get<uint32_t>();
      const uint8_t *d = has_data ? r.bytes(b.size) : nullptr;
      if (r.bad || status != SG_OK) continue;
      auto hit = token ? g_ro_cache.find(token) : g_ro_cache.end();
      if (hit != g_ro_cache.end()) {
        b.buf = hit->second;
      } else {
        b.buf = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(b.size ? b.size : 1u));
        if (d) memcpy(b.buf->data(), d, b.size);
        if (token && b.ro) g_ro_cache[token] = b.buf;
      }
      status = A.sg_map(s.ctx, b.base, b.buf->data(), b.size, b.ro ? 1 : 0);
      if (status == SG_OK) s.blocks.push_back(b);
    } else if (kind == 3) {  // unmap all
      A.sg_unmap_all(s.ctx);
      s.blocks.clear();
    } else {
      r.bad = true;
    }
  }
  return status;
}

void sync_in(State &s) { memcpy(s.dll_state, s.shm, s.size); }
void sync_out(State &s) { memcpy(s.shm, s.dll_state, s.size); }

void reply(int32_t status, const Writer &w) {
  uint32_t len = static_cast<uint32_t>(4 + w.b.size());
  write_all(&len, 4);
  write_all(&status, 4);
  if (!w.b.empty()) write_all(w.b.data(), w.b.size());
}

}  // namespace

int main(int argc, char **argv) {
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
  if (argc < 2 || !load_api(argv[1])) return 2;
  uint16_t cw0, tag0;
  x87_state(&cw0, &tag0);
  for (;;) {
    uint32_t len = 0;
    if (!read_all(&len, 4)) return 0;
    std::vector<uint8_t> req(len);
    if (len && !read_all(req.data(), len)) return 0;
    Reader r{req.data(), req.data() + len};
    uint32_t op = r.get<uint32_t>();
    Writer w;
    int32_t status = SG_OK;
    if (op == 0) return 0;  // quit
    switch (op) {
      case 1:  // hello
        w.put<uint32_t>(static_cast<uint32_t>(A.sg_state_size()));
        w.str(A.sg_build_info());
        w.put<uint32_t>(sizeof(void *));
        break;
      case 2: {  // functions
        int32_t n = A.sg_func_count();
        w.put<int32_t>(n);
        for (int32_t i = 0; i < n; ++i) {
          sg_func_info fi{};
          A.sg_func_info_get(i, &fi);
          w.str(fi.name);
          w.str(fi.module);
          w.str(fi.abi);
          w.str(fi.ret);
          w.put<int32_t>(fi.arity);
          w.put<uint32_t>(fi.fpcw);
        }
        break;
      }
      case 3: {
        std::string n = r.str();
        status = A.sg_negctl_set(n.c_str(), r.get<int32_t>());
        break;
      }
      case 4: status = A.sg_negctl_get(r.str().c_str()); break;
      case 5: A.sg_negctl_reset(); break;
      case 6: {
        int32_t n = A.sg_defect_count();
        w.put<int32_t>(n);
        for (int32_t i = 0; i < n; ++i) {
          sg_defect_info d{};
          A.sg_defect_info_get(i, &d);
          w.str(d.name);
          w.str(d.module);
          w.str(d.ref);
          w.str(d.what);
          w.put<int32_t>(A.sg_defect_get(d.name));
        }
        break;
      }
      case 7: {
        std::string n = r.str();
        status = A.sg_defect_set(n.c_str(), r.get<int32_t>());
        break;
      }
      case 8: status = A.sg_defect_get(r.str().c_str()); break;
      case 9: A.sg_defect_reset(); break;
      case 10: {
        std::string path = r.str();
        char err[512] = {0};
        status = A.sg_exe_load(path.c_str(), err, sizeof(err));
        w.str(err);
        break;
      }
      case 11: status = A.sg_exe_loaded(); break;
      case 12: {
        std::string n = r.str();
        uint32_t size = 0;
        status = A.sg_exe_const(n.c_str(), nullptr, 0, &size);
        if (status == SG_OK) {
          std::vector<uint8_t> d(size ? size : 1);
          A.sg_exe_const(n.c_str(), d.data(), size, &size);
          w.put<uint32_t>(size);
          w.bytes(d.data(), size);
        }
        break;
      }
      case 13: {
        std::string cls = r.str();
        uint32_t has = r.get<uint32_t>();
        const char *c = has ? cls.c_str() : nullptr;
        int32_t n = A.sg_object_count(c);
        w.put<int32_t>(n);
        for (int32_t i = 0; i < n; ++i) {
          uint32_t off = 0, sz = 0;
          A.sg_object_span(c, i, &off, &sz);
          w.str(A.sg_object_name(c, i));
          w.put<uint32_t>(off);
          w.put<uint32_t>(sz);
        }
        break;
      }
      case 14: {
        uint32_t n = r.get<uint32_t>();
        const uint8_t *d = r.bytes(n);
        w.put<uint64_t>(A.sg_hash_bytes(d, n));
        break;
      }
      case 15: {  // state new(shm name)
        std::string name = r.str();
        State s;
        s.size = A.sg_state_size();
        s.map = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
        if (!s.map) { status = SG_E_ARG; break; }
        s.shm = static_cast<uint8_t *>(MapViewOfFile(s.map, FILE_MAP_ALL_ACCESS, 0, 0, s.size));
        s.ctx = A.sg_state_new();
        if (!s.shm || !s.ctx) { status = SG_E_ARG; break; }
        s.dll_state = A.sg_state_ptr(s.ctx);
        uint32_t h = g_next_handle++;
        g_states[h] = s;
        w.put<uint32_t>(h);
        break;
      }
      case 16: {
        uint32_t h = r.get<uint32_t>();
        auto it = g_states.find(h);
        if (it != g_states.end()) {
          A.sg_state_free(it->second.ctx);
          UnmapViewOfFile(it->second.shm);
          CloseHandle(it->second.map);
          g_states.erase(it);
        }
        break;
      }
      default: {  // handle ops 17..24
        uint32_t h = r.get<uint32_t>();
        auto it = g_states.find(h);
        if (it == g_states.end()) { status = SG_E_ARG; break; }
        State &s = it->second;
        int pend = apply_pending(s, r);
        if (r.bad) { status = SG_E_ARG; break; }
        if (pend != SG_OK) { status = pend; break; }
        sync_in(s);
        if (op == 17) {  // call
          std::string name = r.str();
          uint32_t cw = r.get<uint32_t>();
          uint32_t nargs = r.get<uint32_t>();
          std::vector<uint32_t> args(nargs ? nargs : 1);
          for (uint32_t i = 0; i < nargs; ++i) args[i] = r.get<uint32_t>();
          for (Block &b : s.blocks)
            if (!b.ro) {
              const uint8_t *d = r.bytes(b.size);
              if (d) memcpy(b.buf->data(), d, b.size);
            }
          if (r.bad) { status = SG_E_ARG; break; }
          sg_ret out{};
          A.sg_set_fpcw(cw ? cw : kDefaultCw);
          status = A.sg_call(s.ctx, name.c_str(), args.data(), static_cast<int32_t>(nargs), &out);
          uint16_t cw_after = 0, tag_after = 0;
          x87_state(&cw_after, &tag_after);
          x87_reset();
          sync_out(s);
          w.put<uint32_t>(out.eax);
          w.put<uint32_t>(out.edx);
          w.put<double>(out.st0);
          w.put<int32_t>(out.has_st0);
          w.put<uint32_t>(cw_after);
          w.put<uint32_t>(tag_after);
          for (Block &b : s.blocks)
            if (!b.ro) w.bytes(b.buf->data(), b.size);
        } else if (op == 18) {  // hash class
          std::string cls = r.str();
          int32_t n = A.sg_object_count(cls.c_str());
          std::vector<uint64_t> hs(n > 0 ? n : 1);
          int32_t got = A.sg_hash_class(s.ctx, cls.c_str(), hs.data(), n);
          status = got;
          if (got >= 0) {
            w.put<int32_t>(got);
            w.bytes(hs.data(), sizeof(uint64_t) * static_cast<size_t>(got));
          }
        } else if (op == 19) {  // run frames
          uint32_t tn = r.get<uint32_t>();
          const uint8_t *trace = r.bytes(tn);
          int32_t nq = r.get<int32_t>();
          std::vector<sg_msg> q(nq > 0 ? nq : 1);
          for (int32_t i = 0; i < nq; ++i) {
            q[i].msg = r.get<uint32_t>();
            q[i].wp = r.get<uint32_t>();
            q[i].lp = r.get<uint32_t>();
            q[i].cls = r.get<uint32_t>();
          }
          int32_t maxf = r.get<int32_t>();
          int32_t nsim = A.sg_object_count("sim");
          std::vector<uint32_t> ts(maxf > 0 ? maxf : 1);
          std::vector<uint64_t> hs(static_cast<size_t>(maxf > 0 ? maxf : 1) * static_cast<size_t>(nsim > 0 ? nsim : 1));
          int32_t done = 0;
          status = A.sg_run_frames(s.ctx, trace, tn, q.data(), nq, maxf, ts.data(), hs.data(), &done);
          x87_reset();
          sync_out(s);
          w.put<int32_t>(done);
          w.bytes(ts.data(), sizeof(uint32_t) * static_cast<size_t>(done));
          w.bytes(hs.data(), sizeof(uint64_t) * static_cast<size_t>(done) * static_cast<size_t>(nsim));
        } else if (op == 20) {
          status = A.sg_exe_fill_state(s.ctx, r.get<int32_t>());
          sync_out(s);
        } else if (op == 21) {
          std::string n = r.str();
          uint32_t dn = r.get<uint32_t>();
          const uint8_t *d = r.bytes(dn);
          status = A.sg_files_put(s.ctx, n.c_str(), d, dn);
        } else if (op == 22) {
          A.sg_files_clear(s.ctx);
        } else if (op == 23) {
          uint32_t has = r.get<uint32_t>();
          std::string d = r.str();
          status = A.sg_files_root(s.ctx, has ? d.c_str() : nullptr);
        } else if (op == 24) {
          int64_t n = A.sg_files_size(s.ctx, r.str().c_str());
          w.put<int64_t>(n);
        } else if (op == 25) {  // apply pending only (map errors surface here)
        } else {
          status = SG_E_ARG;
        }
        break;
      }
    }
    reply(status, w);
  }
}
