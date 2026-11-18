// P35A item 6: the compiled shaders' cache (shader_cache.h).
#include "shader_cache.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3dcommon.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <vector>

namespace portapp {
namespace {

using pD3DCompile = HRESULT(WINAPI *)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT,
                                      ID3DBlob **, ID3DBlob **);
using pD3DCreateBlob = HRESULT(WINAPI *)(SIZE_T, ID3DBlob **);

struct State {
  std::mutex mu;
  std::string dir;
  pD3DCompile real = nullptr;
  pD3DCreateBlob make_blob = nullptr;
  uint64_t dll_id = 0;  // the compiler DLL's size and write time: another compiler, other files
  std::atomic<uint64_t> hits{0}, compiled{0}, bad{0};
  std::atomic<int64_t> compile_us{0}, read_us{0};
};
State &st() {
  static State s;
  return s;
}

constexpr uint32_t kMagic = 0x43534753u;  // "SGSC"
constexpr uint32_t kVersion = 1;

struct Fnv {
  uint64_t h;
  explicit Fnv(uint64_t basis) : h(basis) {}
  void add(const void *p, size_t n) {
    const unsigned char *b = static_cast<const unsigned char *>(p);
    for (size_t i = 0; i < n; ++i) {
      h ^= b[i];
      h *= 0x100000001b3ull;
    }
  }
  void str(const char *s) {
    const size_t n = s ? std::strlen(s) : 0;
    add(&n, sizeof n);
    if (n) add(s, n);
  }
  template <typename T>
  void val(const T &v) { add(&v, sizeof v); }
};

int64_t now_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool read_file(const std::filesystem::path &p, std::vector<unsigned char> &out) {
  FILE *f = _wfopen(p.c_str(), L"rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  bool ok = n > 0 && n < (64 << 20);
  if (ok) {
    out.resize(static_cast<size_t>(n));
    ok = std::fread(out.data(), 1, out.size(), f) == out.size();
  }
  std::fclose(f);
  return ok;
}

HRESULT WINAPI cached_compile(LPCVOID src, SIZE_T n, LPCSTR name, const D3D_SHADER_MACRO *defs, ID3DInclude *inc, LPCSTR entry,
                              LPCSTR target, UINT f1, UINT f2, ID3DBlob **code, ID3DBlob **err) {
  State &s = st();
  std::string dir;
  {
    std::lock_guard<std::mutex> lk(s.mu);
    dir = s.dir;
  }
  // an include handler's files are not part of the key: such a compile is never cached (the app has none)
  if (dir.empty() || !s.make_blob || inc || !code) {
    const int64_t t0 = now_us();
    const HRESULT hr = s.real(src, n, name, defs, inc, entry, target, f1, f2, code, err);
    s.compile_us += now_us() - t0;
    ++s.compiled;
    return hr;
  }
  Fnv a(0xcbf29ce484222325ull), b(0x84222325cbf29ce4ull);
  for (Fnv *h : {&a, &b}) {
    h->val(kVersion);
    h->val(s.dll_id);
    h->val(static_cast<uint64_t>(n));
    h->add(src, n);
    for (const D3D_SHADER_MACRO *m = defs; m && m->Name; ++m) {
      h->str(m->Name);
      h->str(m->Definition);
    }
    h->str("|");
    h->str(entry);
    h->str(target);
    h->val(f1);
    h->val(f2);
  }
  char fn[64];
  std::snprintf(fn, sizeof fn, "%016llx%016llx.sgsc", static_cast<unsigned long long>(a.h), static_cast<unsigned long long>(b.h));
  std::error_code ec;
  const std::filesystem::path path = std::filesystem::u8path(dir) / fn;
  {
    const int64_t t0 = now_us();
    std::vector<unsigned char> buf;
    if (read_file(path, buf)) {
      // header: magic, version, payload size, payload FNV (low dword, high dword)
      bool ok = buf.size() > 20;
      uint32_t w[5] = {0, 0, 0, 0, 0};
      if (ok) std::memcpy(w, buf.data(), 20);
      ok = ok && w[0] == kMagic && w[1] == kVersion && w[2] == buf.size() - 20;
      if (ok) {
        Fnv c(0xcbf29ce484222325ull);
        c.add(buf.data() + 20, buf.size() - 20);
        ok = w[3] == static_cast<uint32_t>(c.h) && w[4] == static_cast<uint32_t>(c.h >> 32);
      }
      ID3DBlob *blob = nullptr;
      if (ok && SUCCEEDED(s.make_blob(buf.size() - 20, &blob)) && blob) {
        std::memcpy(blob->GetBufferPointer(), buf.data() + 20, buf.size() - 20);
        *code = blob;
        if (err) *err = nullptr;
        s.read_us += now_us() - t0;
        ++s.hits;
        return S_OK;
      }
      ++s.bad;
      std::filesystem::remove(path, ec);
    }
  }
  const int64_t t0 = now_us();
  const HRESULT hr = s.real(src, n, name, defs, inc, entry, target, f1, f2, code, err);
  s.compile_us += now_us() - t0;
  ++s.compiled;
  if (SUCCEEDED(hr) && *code && (*code)->GetBufferSize() > 0) {
    std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
    const size_t sz = (*code)->GetBufferSize();
    Fnv c(0xcbf29ce484222325ull);
    c.add((*code)->GetBufferPointer(), sz);
    const uint32_t w[5] = {kMagic, kVersion, static_cast<uint32_t>(sz), static_cast<uint32_t>(c.h), static_cast<uint32_t>(c.h >> 32)};
    // written under a name of its own and renamed: a start that reads while another writes never sees half a file
    char tn[96];
    std::snprintf(tn, sizeof tn, "%s.%lu.%lu.tmp", fn, GetCurrentProcessId(), GetCurrentThreadId());
    const std::filesystem::path tmp = std::filesystem::u8path(dir) / tn;
    if (FILE *f = _wfopen(tmp.c_str(), L"wb")) {
      const bool ok = std::fwrite(w, 1, sizeof w, f) == sizeof w && std::fwrite((*code)->GetBufferPointer(), 1, sz, f) == sz;
      std::fclose(f);
      if (ok) std::filesystem::rename(tmp, path, ec);
      if (!ok || ec) std::filesystem::remove(tmp, ec);
    }
  }
  return hr;
}

}  // namespace

void shader_cache_set_dir(const std::string &dir) {
  State &s = st();
  std::lock_guard<std::mutex> lk(s.mu);
  s.dir = dir;
}

void *shader_cache_wrap(void *d3d_compile, void *module) {
  State &s = st();
  std::lock_guard<std::mutex> lk(s.mu);
  if (!d3d_compile) return d3d_compile;
  if (!s.real) {
    s.real = reinterpret_cast<pD3DCompile>(d3d_compile);
    const HMODULE m = static_cast<HMODULE>(module);
    if (m) {
      s.make_blob = reinterpret_cast<pD3DCreateBlob>(reinterpret_cast<void *>(GetProcAddress(m, "D3DCreateBlob")));
      wchar_t p[MAX_PATH];
      WIN32_FILE_ATTRIBUTE_DATA fa = {};
      const DWORD n = GetModuleFileNameW(m, p, MAX_PATH);
      if (n > 0 && n < MAX_PATH && GetFileAttributesExW(p, GetFileExInfoStandard, &fa))
        s.dll_id = (static_cast<uint64_t>(fa.ftLastWriteTime.dwHighDateTime) << 32 | fa.ftLastWriteTime.dwLowDateTime) ^
                   (static_cast<uint64_t>(fa.nFileSizeLow) * 0x9e3779b97f4a7c15ull);
    }
  }
  return reinterpret_cast<void *>(&cached_compile);
}

std::string shader_cache_json() {
  State &s = st();
  std::string dir;
  {
    std::lock_guard<std::mutex> lk(s.mu);
    dir = s.dir;
  }
  char b[256];
  std::snprintf(b, sizeof b, "{\"on\":%d,\"hits\":%llu,\"compiled\":%llu,\"bad\":%llu,\"compile_ms\":%.1f,\"read_ms\":%.1f}",
                dir.empty() ? 0 : 1, static_cast<unsigned long long>(s.hits.load()), static_cast<unsigned long long>(s.compiled.load()),
                static_cast<unsigned long long>(s.bad.load()), static_cast<double>(s.compile_us.load()) / 1e3,
                static_cast<double>(s.read_us.load()) / 1e3);
  return b;
}

std::string shader_cache_line() {
  State &s = st();
  char b[200];
  std::snprintf(b, sizeof b, "SHADERS %llu from the cache (%.1f ms), %llu compiled (%.1f ms)",
                static_cast<unsigned long long>(s.hits.load()), static_cast<double>(s.read_us.load()) / 1e3,
                static_cast<unsigned long long>(s.compiled.load()), static_cast<double>(s.compile_us.load()) / 1e3);
  return b;
}

}  // namespace portapp
