"""P3 anchored edits (log/roadmap/P3/notes.md): apply to a clean `git archive e9f59779` tree.

    ~/bin/py -3.12 log/roadmap/P3/edits.py <tree root>

The device sink (audio_exe.cpp DeviceSink, S4 / S7.10) kept every sound buffer the engine released: IDirectSoundBuffer
Release answered 1 and left the buffer's storage in the 16 MB-chunk arena and its interface object in the audio core's
64 KB object block, and HeapReAlloc left the old block allocated. With a sink that reports a buffer's end (Capture, or the
real DirectSound device of `--audio on`), the engine reaps idle sounds and creates their buffers again on the next play, so
both blocks fill with the session's wall time until MissingResource stops the shadow (silence for the rest of the session).
Now: a buffer's last Release frees its storage (reused by the next allocation of that rounded size, the arena's existing
rule) and its interface object (reused by the next object of the same interface and slot count); HeapReAlloc frees the old
block. The answers the engine sees (Release / AddRef 1) are unchanged.
Negative control: registry entry `audio_exe.deviceHeap` mode 1 = the old behaviour (nothing freed).
Also writes port/test/test_audio_device_heap.py from log/roadmap/P3/test_audio_device_heap.py and adds `--negctl
<name>=<mode>` to simgolf_audio_check (port/app/tools/audio_check.cpp) for that test.
"""
import pathlib
import shutil
import sys

ROOT = pathlib.Path(sys.argv[1])
HERE = pathlib.Path(__file__).resolve().parent


class File:
    def __init__(self, rel):
        self.p = ROOT / rel
        self.s = self.p.read_bytes().decode("utf-8")
        self.crlf = "\r\n" in self.s
        self.s = self.s.replace("\r\n", "\n")

    def rep(self, a, b):
        assert self.s.count(a) == 1, (self.p, a[:120])
        self.s = self.s.replace(a, b)

    def save(self):
        out = self.s.replace("\n", "\r\n") if self.crlf else self.s
        self.p.write_bytes(out.encode("utf-8"))


# ---- audio_sink.h / audio_core.cpp: interface objects can be returned and reused
h = File("port/include/portsim/sim/audio_sink.h")
h.rep("uint32_t audio_sink_object(Ctx &ctx, uint32_t iface, uint32_t nslots);\n",
      "uint32_t audio_sink_object(Ctx &ctx, uint32_t iface, uint32_t nslots);\n"
      "// P3: returns an object audio_sink_object made; the next object of the same interface and slot count reuses it (a sink\n"
      "// whose objects are released and created again over a session then needs no more of the block than it holds at once)\n"
      "void audio_sink_object_free(Ctx &ctx, uint32_t obj, uint32_t iface, uint32_t nslots);\n")
h.save()

c = File("port/src/sim/audio_core.cpp")
c.rep("  std::vector<uint8_t> sink_block;  // interface objects (audio_sink_object)\n"
      "  uint32_t sink_used = 0;\n",
      "  std::vector<uint8_t> sink_block;  // interface objects (audio_sink_object)\n"
      "  uint32_t sink_used = 0;\n"
      "  std::multimap<uint64_t, uint32_t> sink_free;  // P3: returned objects, keyed by iface << 32 | nslots\n")
c.rep("  const uint32_t need = 4 + 4 * nslots;\n"
      "  if (iface >= (kSinkTrapSize >> 12) || nslots > 0x400 || a.sink_used + need > kSinkObjSize)\n"
      "    throw MissingResource{\"sink object block full\"};\n"
      "  const uint32_t obj = kSinkObjBase + a.sink_used;\n",
      "  const uint32_t need = 4 + 4 * nslots;\n"
      "  if (iface >= (kSinkTrapSize >> 12) || nslots > 0x400) throw MissingResource{\"sink object block full\"};\n"
      "  if (auto it = a.sink_free.find((uint64_t{iface} << 32) | nslots); it != a.sink_free.end()) {  // P3: a returned object\n"
      "    const uint32_t obj = it->second;\n"
      "    a.sink_free.erase(it);\n"
      "    st<uint32_t>(ctx, obj, obj + 4);\n"
      "    for (uint32_t s = 0; s < nslots; ++s) st<uint32_t>(ctx, obj + 4 + 4 * s, kSinkTrapBase + (iface << 12) + 4 * s);\n"
      "    return obj;\n"
      "  }\n"
      "  if (a.sink_used + need > kSinkObjSize) throw MissingResource{\"sink object block full\"};\n"
      "  const uint32_t obj = kSinkObjBase + a.sink_used;\n")
c.rep("  a.sink_used += (need + 15u) & ~15u;\n"
      "  return obj;\n"
      "}\n",
      "  a.sink_used += (need + 15u) & ~15u;\n"
      "  return obj;\n"
      "}\n"
      "\n"
      "void audio_sink_object_free(Ctx &ctx, uint32_t obj, uint32_t iface, uint32_t nslots) {\n"
      "  AudioCtx &a = actx(ctx);\n"
      "  if (obj < kSinkObjBase || obj >= kSinkObjBase + a.sink_used) return;  // not one of this block's objects\n"
      "  a.sink_free.emplace((uint64_t{iface} << 32) | nslots, obj);\n"
      "}\n")
c.save()

# ---- audio_exe.cpp: the device sink frees a released buffer's storage and object, and HeapReAlloc's old block
e = File("port/src/sim/audio_exe.cpp")
e.rep("class DeviceSink : public DeviceModel {\n"
      " public:\n",
      "// P3 (log/roadmap/P3/notes.md): negative control of the device sink's frees (registry entry audio_exe.deviceHeap):\n"
      "// 1 = nothing freed (a released buffer's storage and object, HeapReAlloc's old block stay allocated: the old behaviour)\n"
      "int g_neg_deviceHeap = 0;\n"
      "\n"
      "class DeviceSink : public DeviceModel {\n"
      " public:\n")
e.rep("    U4 storage = 0, size = 0, play = 0, status = 0;\n"
      "    U4 freq = 22050, vol = 0, pan = 0;\n",
      "    U4 storage = 0, size = 0, play = 0, status = 0;\n"
      "    U4 freq = 22050, vol = 0, pan = 0;\n"
      "    U4 refs = 1;  // P3: AddRef / Release; the last Release frees the storage and the object\n")
e.rep("  U4 obj(Ctx &c, U4 iface, U4 nslots) { return audio_sink_object(c, iface, nslots); }\n",
      "  U4 obj(Ctx &c, U4 iface, U4 nslots) { return audio_sink_object(c, iface, nslots); }\n"
      "  // P3: a block back to the arena (reused by the next allocation of its rounded size), as HeapFree does\n"
      "  void free_block(U4 p) {\n"
      "    if (auto it = sizes.find(p); it != sizes.end()) {\n"
      "      freed.emplace((std::max<U4>(it->second, 1) + 15u) & ~15u, p);\n"
      "      sizes.erase(it);\n"
      "    }\n"
      "  }\n")
e.rep("      const U4 k = std::min(sizes.count(old) ? sizes[old] : 0u, arg(3));\n"
      "      for (U4 i = 0; i < k; ++i) S<uint8_t>(c, p + i, L<uint8_t>(c, old + i));\n"
      "      return ok(p);\n",
      "      const U4 k = std::min(sizes.count(old) ? sizes[old] : 0u, arg(3));\n"
      "      for (U4 i = 0; i < k; ++i) S<uint8_t>(c, p + i, L<uint8_t>(c, old + i));\n"
      "      if (NG(deviceHeap) != 1 && old != p) free_block(old);  // P3: the old block, as HeapReAlloc frees it\n"
      "      return ok(p);\n")
e.rep("      if (auto it = sizes.find(p); it != sizes.end()) {\n"
      "        freed.emplace((std::max<U4>(it->second, 1) + 15u) & ~15u, p);\n"
      "        sizes.erase(it);\n"
      "      }\n"
      "      return ok(1);\n",
      "      free_block(p);\n"
      "      return ok(1);\n")
# A2: freed blocks of other sizes are used before a new chunk is opened. With freeing alone a live session still opened a
# chunk every few thousand frames (P3 gate test_app_present: +103,481,344 B with a device), because the engine loads sounds of
# many sizes and a freed block was reused only by a request of its exact rounded size. Inside the first chunk nothing changes
# (the exact-size reuse and the bump as before), so a run that never fills a chunk (the pinned $device stream) is unchanged.
e.rep("    U4 p;\n"
      "    if (auto it = freed.find(need); it != freed.end()) {\n"
      "      p = it->second;\n"
      "      freed.erase(it);\n"
      "    } else {\n"
      "      if (chunks.empty() || used + need > static_cast<U4>(chunks.size()) * kChunk) {   // the block opens a new chunk\n",
      "    U4 p = 0;\n"
      "    if (auto it = freed.find(need); it != freed.end()) {\n"
      "      p = it->second;\n"
      "      freed.erase(it);\n"
      "    } else if (!chunks.empty() && used + need > static_cast<U4>(chunks.size()) * kChunk && NG(deviceHeap) != 1 &&\n"
      "               take_free(need, p)) {\n"
      "      // P3: a coalesced freed block (or the bump, given back a free run at its top) instead of a new chunk\n"
      "    } else {\n"
      "      if (chunks.empty() || used + need > static_cast<U4>(chunks.size()) * kChunk) {   // the block opens a new chunk\n")
e.rep("  U4 obj(Ctx &c, U4 iface, U4 nslots) { return audio_sink_object(c, iface, nslots); }\n"
      "  // P3: a block back to the arena",
      "  U4 obj(Ctx &c, U4 iface, U4 nslots) { return audio_sink_object(c, iface, nslots); }\n"
      "  // P3: before a new chunk: merge address-adjacent freed blocks of one chunk, give a free run that ends at the bump top of\n"
      "  // the last chunk back to the bump, then take the bump if it has room again, else the smallest freed block that fits (its\n"
      "  // rest stays freed). false = nothing fits (the caller opens a chunk).\n"
      "  bool take_free(U4 need, U4 &p) {\n"
      "    std::map<U4, U4> by_addr;  // address -> rounded size\n"
      "    for (const auto &kv : freed) by_addr[kv.second] = kv.first;\n"
      "    std::vector<std::pair<U4, U4>> runs;\n"
      "    for (const auto &kv : by_addr) {\n"
      "      if (!runs.empty() && runs.back().first + runs.back().second == kv.first &&\n"
      "          (runs.back().first - kBase) / kChunk == (kv.first - kBase) / kChunk) {\n"
      "        runs.back().second += kv.second;\n"
      "        continue;\n"
      "      }\n"
      "      runs.push_back(kv);\n"
      "    }\n"
      "    const U4 last = static_cast<U4>(chunks.size() - 1);\n"
      "    while (!runs.empty() && runs.back().first + runs.back().second == kBase + used && (runs.back().first - kBase) / kChunk == last) {\n"
      "      used -= runs.back().second;\n"
      "      runs.pop_back();\n"
      "    }\n"
      "    freed.clear();\n"
      "    for (const auto &r : runs) freed.emplace(r.second, r.first);\n"
      "    if (used + need <= static_cast<U4>(chunks.size()) * kChunk) {\n"
      "      p = kBase + used;\n"
      "      used += need;\n"
      "      return true;\n"
      "    }\n"
      "    auto best = freed.lower_bound(need);\n"
      "    if (best == freed.end()) return false;\n"
      "    p = best->second;\n"
      "    const U4 sz = best->first;\n"
      "    freed.erase(best);\n"
      "    if (sz > need) freed.emplace(sz - need, p + need);\n"
      "    return true;\n"
      "  }\n"
      "  // P3: a block back to the arena")
# A3: mmio frees the buffers it allocated itself, as winmm does: mmioClose drops the file's own buffer (MMIO_ALLOCBUF, or a
# 'MEM ' file opened with no buffer), mmioSetBuffer drops the own buffer it replaces. With a device the P3 fix run fix2_dev1
# still held 104,031,974 live bytes in 2,411 blocks after one course_season_6y session (audio.json heap_live_bytes).
e.rep("    if (name == \"mmioClose\") {\n"
      "      mm.erase(a[0]);\n"
      "      return ok(0);\n"
      "    }\n",
      "    if (name == \"mmioClose\") {\n"
      "      if (auto it = mm.find(a[0]); it != mm.end() && it->second.own && it->second.buf && NG(deviceHeap) != 1)\n"
      "        free_block(it->second.buf);  // P3: mmio's own buffer, as winmm frees it\n"
      "      mm.erase(a[0]);\n"
      "      return ok(0);\n"
      "    }\n")
e.rep("      if (a[2] == 0) {\n"
      "        if (f->buffered) f->pos = f->buf_off + f->next;\n"
      "        f->buffered = false;\n",
      "      if (a[2] == 0) {\n"
      "        if (f->buffered) f->pos = f->buf_off + f->next;\n"
      "        f->buffered = false;\n"
      "        if (f->own && f->buf && !f->mem && NG(deviceHeap) != 1) {  // P3: unbuffered from now on: mmio's own buffer goes\n"
      "          free_block(f->buf);\n"
      "          f->buf = 0;\n"
      "          f->own = false;\n"
      "        }\n")
e.rep("        const U4 at = mm_pos(*f);\n"
      "        f->buffered = true;\n"
      "        f->cch = a[2];\n"
      "        f->buf = a[1] ? a[1] : alloc(c, a[2]);\n",
      "        const U4 at = mm_pos(*f);\n"
      "        if (f->own && f->buf && f->buf != a[1] && NG(deviceHeap) != 1) free_block(f->buf);  // P3: the own buffer it replaces\n"
      "        f->buffered = true;\n"
      "        f->cch = a[2];\n"
      "        f->own = a[1] == 0;  // P3: mmio allocates (and later frees) the buffer only when the caller gives none\n"
      "        f->buf = a[1] ? a[1] : alloc(c, a[2]);\n")
e.rep("    if (cmd.iface == kDsBuf) {\n"
      "      Buf &b = bufs[arg(0)];\n"
      "      switch (cmd.slot) {\n"
      "        case 0: return ok(0x80004002u);\n"
      "        case 1: case 2: return ok(1);\n",
      "    if (cmd.iface == kDsBuf) {\n"
      "      Buf &b = bufs[arg(0)];\n"
      "      switch (cmd.slot) {\n"
      "        case 0: return ok(0x80004002u);\n"
      "        case 1: ++b.refs; return ok(1);                    // AddRef (the answer stays 1)\n"
      "        case 2:                                             // Release (the answer stays 1)\n"
      "          // P3: the last Release frees the storage and the object (the engine creates the buffer again on its next play)\n"
      "          if (--b.refs == 0 && NG(deviceHeap) != 1) {\n"
      "            const U4 o = arg(0);\n"
      "            if (b.storage) free_block(b.storage);\n"
      "            bufs.erase(o);\n"
      "            audio_sink_object_free(c, o, kDsBuf, 24);\n"
      "          }\n"
      "          return ok(1);\n")
# the registry entry carrying the control
e.rep('    {"audio_exe.deviceStep", "default", "uint32", 2, 0, a_device_step, &g_neg_none},\n',
      '    {"audio_exe.deviceStep", "default", "uint32", 2, 0, a_device_step, &g_neg_none},\n'
      '    {"audio_exe.deviceHeap", "default", "uint32", 0, 0, a_device_heap, &g_neg_deviceHeap},  // P3: the control\'s switch\n')
e.save()

# the adapter (no effect: the entry only carries g_neg_deviceHeap for `--negctl reg:audio_exe.deviceHeap=1` / lib.negctl)
t = (ROOT / "port/src/sim/audio_exe.cpp").read_bytes().decode("utf-8")
crlf = "\r\n" in t
t = t.replace("\r\n", "\n")
anchor = "// audio_exe.deviceStep(step, n):"
assert t.count(anchor) == 1
t = t.replace(anchor,
              "// audio_exe.deviceHeap(): returns 0; the entry carries the device sink's free control g_neg_deviceHeap (P3)\n"
              "void a_device_heap(Ctx &, const uint32_t *, Ret &r) { r.set_u32(0); }\n"
              "\n" + anchor)
(ROOT / "port/src/sim/audio_exe.cpp").write_bytes((t.replace("\n", "\r\n") if crlf else t).encode("utf-8"))

# ---- simgolf_audio_check: --negctl <registry name>=<mode> (script and device modes)
a = File("port/app/tools/audio_check.cpp")
a.rep('#include "../audio_out.h"\n#include "portsim/execonst.h"\n',
      '#include "../audio_out.h"\n#include "portsim/execonst.h"\n#include "portsim/registry_list.h"\n')
a.rep("uint32_t num(const std::string &s) { return static_cast<uint32_t>(std::strtoll(s.c_str(), nullptr, 0)); }\n",
      "uint32_t num(const std::string &s) { return static_cast<uint32_t>(std::strtoll(s.c_str(), nullptr, 0)); }\n"
      "\n"
      "// P3: --negctl <registry name>=<mode> sets that entry's negative-control switch for the process (false: no such entry)\n"
      "bool set_negctl(const std::string &spec) {\n"
      "  const size_t eq = spec.find('=');\n"
      "  if (eq == std::string::npos) return false;\n"
      "  const int i = portsim::registry_find(spec.substr(0, eq).c_str());\n"
      "  if (i < 0) return false;\n"
      "  int *sw = portsim::registry_at(i, nullptr)->negctl;\n"
      "  if (!sw) return false;\n"
      "  *sw = std::atoi(spec.c_str() + eq + 1);\n"
      "  return true;\n"
      "}\n")
a.rep("  const std::string cmd = argv[1];\n",
      "  const std::string cmd = argv[1];\n"
      "  if (a.count(\"--negctl\") && !set_negctl(a[\"--negctl\"]))  // P3\n"
      "    return std::fprintf(stderr, \"--negctl: no registry entry with a switch: %s\\n\", a[\"--negctl\"].c_str()), 2;\n")
a.rep("// Exit code 0 when every request ran (stats.failed 0), 1 otherwise, 2 on bad arguments.\n",
      "// P3: [--negctl <registry name>=<mode>] sets that entry's negative-control switch (both commands).\n"
      "// Exit code 0 when every request ran (stats.failed 0), 1 otherwise, 2 on bad arguments.\n")
a.save()

# ---- B (separable): test_long_session_memory_bounded measures the session, not the teardown. The app takes its last memory
# sample after audio.finish() (main.cpp live-trace block), which frees the shadow audio context; with an audio device the
# shadow is alive at frame 2,000 and freed at the end, so the old rule read the sim's growth minus the shadow's footprint
# (-19,914,752 B in P3's base1) and could not see the shadow's own growth (+121 MB before teardown there).
p = File("port/test/test_app_present.py")
p.rep('def _growth(mem: list[list[int]]) -> int:\n'
      '    """private bytes at the end minus at the first sample from frame 2,000 on (the world is loaded by then)"""\n'
      '    after = [m for m in mem if m[0] >= 2000]\n'
      '    return after[-1][3] - after[0][3]\n',
      'def _growth(mem: list[list[int]]) -> int:\n'
      '    """private bytes at the last sample taken during the session minus at the first sample from frame 2,000 on (the world\n'
      '    is loaded by then). P3 (log/roadmap/P3/notes.md): the end sample is taken after audio.finish(), which frees the shadow\n'
      '    audio context, so with an audio device it left out what the shadow holds (and would have read the sim\'s growth minus\n'
      '    the shadow\'s footprint); the last periodic sample still has it."""\n'
      '    after = [m for m in mem if 2000 <= m[0] < mem[-1][0]] or [m for m in mem if m[0] >= 2000]\n'
      '    return after[-1][3] - after[0][3]\n')
p.save()

# ---- C: LiveTracePresenter reserves its present log once. Every in-game frame presents since S3 animated the course (51,109
# presents over course_season_6y), so the log (24 B per present) grew by MSVC's 1.5x reallocation through blocks of 655,440 and
# 983,160 B between the three 960,000-B buffers each present makes; whether the 983,160-B block found a free hole decided one
# more ~0.97 MB of heap commit (P3 runs nodev1 3,407,872 / nodev2 4,300,800, the two modes of I21 / I22). The trace's frame
# count bounds the log, so it is reserved at the start (before the frame-2,000 sample). Control `--negctl presents-no-reserve`.
hh = File("port/app/host.h")
hh.rep("  const std::vector<Present> &presents() const { return presents_; }\n",
       "  const std::vector<Present> &presents() const { return presents_; }\n"
       "  void reserve_presents(size_t n) { presents_.reserve(n); }  // P3: the log reserved once (no reallocation in a session)\n")
hh.save()
mm = File("port/app/main.cpp")
mm.rep("        host.no_present_point = o.negctl == \"present-point-off\";\n"
       "        TerrainLogBound tlog;  // S7.8 (U-1643)\n",
       "        host.no_present_point = o.negctl == \"present-point-off\";\n"
       "        // P3: a present per frame at most, plus the boot's and modal screens' (51,109 for course_season_6y's 51,092 records)\n"
       "        if (o.negctl != \"presents-no-reserve\") host.reserve_presents(static_cast<size_t>(std::max<int32_t>(0, count_frames(trace))) + 4096);\n"
       "        TerrainLogBound tlog;  // S7.8 (U-1643)\n")
mm.save()

# ---- the test
shutil.copyfile(HERE / "test_audio_device_heap.py", ROOT / "port/test/test_audio_device_heap.py")
(ROOT / "port/src/registry.cpp").touch()
print("P3 edits applied to", ROOT)
