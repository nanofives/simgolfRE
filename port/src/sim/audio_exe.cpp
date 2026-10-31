// Module audio_exe (roadmap step S4.10): golf_clean.exe's sound layer on the port's sound.dll engine. Header:
// portsim/sim/audio_exe.h (addresses, calls, heap, the command stream). Written from the C3 / C4 notes
// (re/analysis/audio/<addr>_<name>.md), the hook sources as reading reference (shim/src/re/c3dm / c3dy / c3eu / c3fc / c3dl /
// c3f / c3j / c3c / c3ev / c3dx / c3fi / c3cy / c3dd / c3fm / c3cc / c3fu.cpp) and, for the C2 rows and the device-holder
// helpers with no hook, the instructions (log/roadmap/S4/10_dis.py). No original address in this file: exe code is cited by
// function name (+ offset), data by GameState member, constants of .rdata by the exe_rva / class-table offsets below.
// Notes: log/roadmap/S4/10_notes.md.
//
// NEGATIVE CONTROLS: g_neg_<fn> (registered as the FuncDef's negctl) mode 1 = one wrong detail named at the function.
#include "portsim/sim/audio_exe.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/audio_sink.h"
#include "portsim/sim/commands.h"
#include "tick_hook.h"

namespace portsim {

namespace {

using U4 = uint32_t;
using I4 = int32_t;

// ---- the exe image (anchored layout) ---------------------------------------------------------------------------------
constexpr U4 kExeImageSize = 0x445u << 12;  // the anchored golf_clean.exe's image, section-aligned (0x445 pages)
constexpr U4 kExeSizeOfImage = (0x44u << 16) | 0x414eu;  // its header's SizeOfImage (unaligned: the cleaned file's value)
// class tables (.rdata; re/model/e2_vtables.tsv): image-relative
constexpr U4 kVtSndBase = 0xbac68, kVtSnd = 0xbacf8, kVtSndRoot = 0xbadec, kVtVoiceRx = 0xbadf8;
constexpr U4 kKVolumeScale = 0xbacf0;  // f_k_dbl_4bacf0: the double 1/127 Snd::setVolume multiplies by
// .data strings (GameState f_strpool_4e43ac): "sound.dll" at +0x10 (soundVersionCheck), VoiceRx's 8-character name at +0x1c
constexpr U4 kPoolSoundDll = 0x10, kPoolVoiceName = 0x1c;
// soundVersionCheck's message (f_strpool_4babd0) and caption (f_strpool_4bac50): .rdata, image-relative
constexpr U4 kMsgText = 0xbabd0, kMsgCaption = 0xbac50;

// ---- per-context state ------------------------------------------------------------------------------------------------
constexpr U4 kArenaBase = 0xFE000000u, kArenaSize = 0x100000u;  // the exe heap without a host (U-1495)

struct ExeCtx {
  bool replay = false;
  std::set<U4> claims;
  AudioExeCounters counters;
  int depth = 0;  // nesting of calls into the module (0 = a request from another module)
  bool frame_seen = false;
  U4 last_frame = 0;
  AudioSink *sink = nullptr;
  std::unique_ptr<RecordingSink> owned_sink;
  std::vector<uint8_t> arena;
  U4 arena_used = 0;
  // test-only (audio_exe.stubRequests): playSound / startSoundSlot / Snd::status return 0 and run nothing, the port-side
  // counterpart of the stubs the UI tests' emulator oracles put on those addresses (S4.2b phase 3, 2b_notes.md)
  bool stub_requests = false;
  AudioRequestObserver observer;  // S7.10: the app's request mirror (audio_exe_set_observer)
};

std::mutex g_mu;
std::map<const Ctx *, std::unique_ptr<ExeCtx>> &table() {
  static std::map<const Ctx *, std::unique_ptr<ExeCtx>> t;
  return t;
}
ExeCtx &ectx(const Ctx &c) {
  std::lock_guard<std::mutex> lk(g_mu);
  auto &p = table()[&c];
  if (!p) p = std::make_unique<ExeCtx>();
  return *p;
}

// the read-only image of the user's exe, shared by every context that maps it (immutable)
std::mutex g_img_mu;
std::vector<uint8_t> &shared_image() {
  static std::vector<uint8_t> img;
  return img;
}

template <class T> T L(const Ctx &c, U4 va) { return c.mem.load<T>(va); }
template <class T> void S(const Ctx &c, U4 va, T v) { c.mem.store<T>(va, v); }
inline U4 L32(const Ctx &c, U4 va) { return c.mem.load<U4>(va); }
inline void S32(const Ctx &c, U4 va, U4 v) { c.mem.store<U4>(va, v); }

U4 gs_va(Ctx &c, U4 off) { return c.mem.data_base() + off; }
template <class M> U4 GSV(Ctx &c, M GameState::*m) { return state_va(c, m); }

// g_soundReady / g_soundDll / g_soundFns[i]
U4 sound_ready(Ctx &c) { return L32(c, GSV(c, &GameState::g_soundReady)); }
U4 sound_fn(Ctx &c, int i) { return L32(c, GSV(c, &GameState::g_soundFns) + 4u * static_cast<U4>(i)); }
U4 music_device(Ctx &c) { return GSV(c, &GameState::g_musicDevice); }

bool mapped(const Ctx &c, U4 va, U4 n) {
  try {
    c.mem.at(va, n);
    return true;
  } catch (const Fault &) {
    return false;
  }
}

// ---- exe functions the module dispatches to ----------------------------------------------------------------------------
// rva and hooks.csv name (rva = the anchored VA minus the exe's image base): every backend-audio row, the device-holder helpers and the
// class-table entries the Snd / VoiceRx tables hold (re/model/e2_vtables.tsv). tests/test_audio_exe.py checks each pair
// against hooks.csv.
const ExeFn kExeFns[] = {
    {0x5a00, "playHabitatSound"},
    {0xc500, "playSoundAt"},
    {0x3cce0, "playAudioFile"},
    {0x48190, "gSoundsDtor_448190"},
    {0x48160, "arrayStaticInit_448160"},
    {0x481b0, "playSound"},
    {0x48200, "startSoundSlot"},
    {0x48220, "loadSoundTable"},
    {0x490b0, "shutdownSoundSystem"},
    {0x83e00, "gMusicDeviceDtor_483e00"},
    {0x83e40, "gSfxDeviceDtor_483e40"},
    {0x83e80, "gVoiceDeviceDtor_483e80"},
    {0x83e90, "initSound"},
    {0x83f10, "shutdownSound"},
    {0x83fc0, "soundVersionCheck"},
    {0x84060, "freeSoundLib"},
    {0x84090, "callSoundHook74"},
    {0x840c0, "callSoundHook94"},
    {0x840e0, "createSound4840e0"},
    {0x84110, "releaseSoundObj"},
    {0x84130, "clearSoundPtrs"},
    {0x84150, "Snd::ctorBase"},
    {0x841c0, "Snd::scalarDtor4841c0"},
    {0x841e0, "Snd::dtor"},
    {0x84260, "Snd::setMode"},
    {0x842f0, "SndMusic::setFile"},
    {0x843e0, "unlinkSound"},
    {0x84440, "Snd::isPlaying"},
    {0x84450, "Snd::linkIntoList"},
    {0x844b0, "Snd::getNext"},
    {0x844c0, "Snd::getPrev"},
    {0x844d0, "Snd::getMode"},
    {0x844e0, "Snd::unlinkFromList"},
    {0x84540, "returnB_484540"},
    {0x84550, "Snd::stop484550"},
    {0x84590, "Snd::fwdVoice1c"},
    {0x845b0, "Snd::fwdVoice18"},
    {0x845d0, "Snd::queryDevice4845d0"},
    {0x845e0, "Snd::status"},
    {0x84600, "Snd::fwdVoice38"},
    {0x84620, "Snd::setVolumeBase"},
    {0x84690, "Snd::setLoop"},
    {0x846b0, "Snd::setField34"},
    {0x846d0, "Snd::setField38"},
    {0x84700, "Snd::setField38Voice54"},
    {0x84730, "Snd::applyField38Voice28"},
    {0x84750, "Snd::applyField484750"},
    {0x84770, "Snd::applyField38Voice30"},
    {0x84790, "Snd::fwdVoice34"},
    {0x847b0, "Snd::applyField4847b0"},
    {0x847d0, "Snd::fwdVoice58"},
    {0x847e0, "Snd::fwdVoice74"},
    {0x847f0, "Snd::setPan"},
    {0x84820, "Snd::ctorDerived"},
    {0x84870, "Snd::fwdVoice60"},
    {0x84880, "SndMusic::scalarDtor"},
    {0x848a0, "SndMusic::dtor"},
    {0x84920, "Snd::fwdVoice98"},
    {0x84940, "Snd::play"},
    {0x849e0, "Snd::fwdVoice94"},
    {0x84a00, "Snd::fwdVoiceD0"},
    {0x84a20, "Snd::fwdVoiceD4"},
    {0x84a40, "Snd::configure"},
    {0x84b00, "Snd::fwdVoiceCC"},
    {0x84b20, "Snd::getLengthMs"},
    {0x84b30, "Snd::busy"},
    {0x84b80, "Snd::setName"},
    {0x84bf0, "Snd::fwdVoiceF0"},
    {0x84c20, "SndStream::open484c20"},
    {0x84c80, "Snd::open"},
    {0x84d60, "Snd::reopen"},
    {0x84e30, "Snd::applyMode484e30"},
    {0x84e70, "Snd::start"},
    {0x84f00, "Snd::stop484f00"},
    {0x84f40, "Snd::setPitch"},
    {0x84f80, "Snd::setFlags"},
    {0x84ff0, "Snd::flags"},
    {0x85040, "Snd::fwdVoiceB8"},
    {0x85060, "Snd::fwdVoiceBC"},
    {0x85080, "Snd::fwdVoice3c"},
    {0x85090, "Snd::fwdDev48"},
    {0x850b0, "Snd::fwdDev49"},
    {0x850f0, "Snd::setField60"},
    {0x85120, "returnB_485120"},
    {0x85130, "returnBPop12_485130"},
    {0x85140, "Snd::setVolume"},
    {0x851b0, "Snd::fwdDev54"},
    {0x851e0, "Snd::fwdDev55"},
    {0x85200, "Snd::fwdDev56"},
    {0x85220, "Snd::fwdDev57"},
    {0x85240, "Snd::fwdDev58"},
    {0x85260, "Snd::ctor485260"},
    {0x85290, "SndBase::setField38"},
    {0x852b0, "Snd::scalarDtor4852b0"},
    {0x852d0, "Snd::setVtbl4852d0"},
    {0x852e0, "VoiceRx::ctor"},
    {0x85380, "returnBPop4_485380"},
    {0x85390, "VoiceRx::fwdDev37"},
    {0x85450, "VoiceRx::start"},
    {0x85500, "VoiceRx::callDev34"},
    {0x85520, "VoiceRx::callDev8"},
    {0x85530, "VoiceRx::fwdDev5"},
    {0x85550, "VoiceRx::fwdDev35"},
    {0x85570, "VoiceRx::fwdDev36"},
    {0x873f0, "openHookA"},
    {0x87500, "NodeList::playChildSuspendingSounds"},
    {0x879b0, "openHookB"},
    {0x87b90, "openHookC"},
    {0x87bd0, "closeHookC"},
    // device-holder helpers (engine-glue / boot-only rows of re/port/scope.tsv)
    {0x87770, "SndGroup_isDisabled"},
    {0x877a0, "SndGroup_isMuted"},
    {0x876c0, "HashTable::remove"},
    {0x87630, "resourceQuery"},
    {0x87c00, "resourceLoad"},
    {0x87a70, "resourceLoadB"},
    {0x87460, "releaseWithHookA"},
    {0x87ac0, "releaseWithHookB"},
    {0x87c40, "releaseWithHookC"},
    {0x87430, "closeHookA"},
    {0x879f0, "closeHookB"},
    {0x85590, "showMessageBox"},
    // class-table entries outside the module (dispatched by name when another module registers them)
    {0x853b0, "Screen::scalarDtor"},
    {0x854c0, "Screen::close"},
    {0x79820, "return0_479820"},
    {0x87390, "NodeList::dtor"},
    {0x87a60, "NodeListB::dtor"},
    {0x87b80, "NodeListC::dtor"},
    {0xa6ab8, "_purecall"},
    // the exe's static CRT allocator (exe_new / exe_delete: the replay host's heap, else the module's arena)
    {0xa57f1, "operator_new"},
    {0xa4ffc, "operatorDelete_4a4ffc"},
};
constexpr size_t kExeFnCount = sizeof(kExeFns) / sizeof(kExeFns[0]);

const FuncDef *port_fn(const char *name) {
  const int k = registry_find(name);
  return k >= 0 ? registry_at(k, nullptr) : nullptr;
}

// ---- the command stream ------------------------------------------------------------------------------------------------
void emit(Ctx &c, ExeCtx &e, const char *name, const U4 *a, int n) {
  if (!e.sink) return;
  const U4 t = tk::cur_io().t_head;
  if (!e.frame_seen || t != e.last_frame) {
    AudioCommand f;
    f.dll = "portsim";
    f.name = "frame";
    f.args = {t};
    e.sink->import(c, f);
    e.frame_seen = true;
    e.last_frame = t;
  }
  AudioCommand cmd;
  cmd.dll = "golf_clean.exe";
  cmd.name = name;
  cmd.args.assign(a, a + n);
  e.sink->import(c, cmd);
}

// RAII: a call into the module; at depth 0 it is a request of another module (stream + counter)
struct Req {
  ExeCtx &e;
  Req(Ctx &c, const char *name, const U4 *a, int n) : e(ectx(c)) {
    if (e.depth == 0) {
      ++e.counters.requests;
      if (e.observer) e.observer(name, a, n);
      emit(c, e, name, a, n);
    }
    ++e.depth;
  }
  ~Req() { --e.depth; }
};

// ---- the exe's import directory (for sink commands) ------------------------------------------------------------------
bool exe_import_name(Ctx &c, U4 iat_rva, std::string &dll, std::string &name) {
  const U4 b = exe_base(c);
  const U4 pe = L32(c, b + 0x3c);
  const U4 imp = L32(c, b + pe + 0x18 + 0x60 + 8);
  if (imp == 0) return false;
  auto cstr = [&](U4 va) {
    std::string s;
    for (char ch; (ch = static_cast<char>(L<uint8_t>(c, va))) != 0 && s.size() < 256; ++va) s += ch;
    return s;
  };
  for (U4 d = b + imp;; d += 20) {
    const U4 oft = L32(c, d), nm = L32(c, d + 12), ft = L32(c, d + 16);
    if (nm == 0 && ft == 0) return false;
    for (U4 i = 0;; ++i) {
      const U4 th = L32(c, b + (oft ? oft : ft) + 4 * i);
      if (th == 0) break;
      if (ft + 4 * i == iat_rva) {
        dll = cstr(b + nm);
        name = (th & 0x80000000u) ? "#" + std::to_string(th & 0xffffu) : cstr(b + th + 2);
        return true;
      }
    }
  }
}

Ret sink_import(Ctx &c, U4 iat_rva, const U4 *args, int n) {
  ExeCtx &e = ectx(c);
  AudioCommand cmd;
  if (!exe_import_name(c, iat_rva, cmd.dll, cmd.name)) throw MissingResource{"golf_clean.exe import slot"};
  if (!e.sink) throw MissingResource{"an AudioSink for the exe's imports"};
  cmd.args.assign(args, args + n);
  const AudioReply r = e.sink->import(c, cmd);
  Ret out;
  out.eax = r.eax;
  out.edx = r.edx;
  return out;
}

// ---- production loader (no host): LoadLibraryA / GetProcAddress / FreeLibrary over the audio core -------------------------
std::string cstr_at(Ctx &c, U4 va) {
  std::string s;
  for (char ch; (ch = static_cast<char>(L<uint8_t>(c, va))) != 0 && s.size() < 260; ++va) s += ch;
  return s;
}
bool ieq(const std::string &a, const char *b) {
  if (a.size() != std::strlen(b)) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  return true;
}
// sound.dll's preferred base from its header (the port maps it there: nothing else occupies it)
U4 sound_preferred_base(Ctx &c) {
  std::vector<uint8_t> h(0x400);
  const int64_t n = c.files().read("sound.dll", 0, h.data(), h.size());
  if (n < 0x200) throw MissingResource{"sound.dll"};
  U4 pe, ib;
  std::memcpy(&pe, h.data() + 0x3c, 4);
  if (pe + 0x38 > h.size()) throw MissingResource{"sound.dll: header"};
  std::memcpy(&ib, h.data() + pe + 0x18 + 0x1c, 4);
  return ib;
}
// LoadLibraryA's name as a GameFiles name: the exe passes ".\\sound.dll" (f_strpool_4e43ac + 0x10), relative to the current
// directory, which is the install root
std::string file_name(std::string s) {
  for (char &ch : s)
    if (ch == '\\') ch = '/';
  while (s.rfind("./", 0) == 0) s = s.substr(2);
  return s;
}
U4 load_library(Ctx &c, U4 name_va) {
  const std::string name = file_name(cstr_at(c, name_va));
  if (!ieq(name, "sound.dll")) return 0;
  if (!audio_attached(c)) {
    if (!c.files().exists("sound.dll")) return 0;
    audio_attach(c, sound_preferred_base(c), true);
    audio_set_sink(c, ectx(c).sink);  // audio_attach starts a fresh core state: the module's sink is the core's again
  }
  return snd_base(c);
}
U4 get_proc_address(Ctx &c, U4 module, U4 name_or_ord) {
  if (!audio_attached(c) || module != snd_base(c) || (name_or_ord >> 16) != 0) return 0;  // ordinals only
  const U4 b = snd_base(c);
  const U4 pe = L32(c, b + 0x3c);
  const U4 ed = L32(c, b + pe + 0x18 + 0x60);
  if (ed == 0) return 0;
  const U4 ord_base = L32(c, b + ed + 0x10), nfun = L32(c, b + ed + 0x14), funcs = L32(c, b + ed + 0x1c);
  const U4 k = name_or_ord - ord_base;
  if (k >= nfun) return 0;
  const U4 rva = L32(c, b + funcs + 4 * k);
  return rva ? b + rva : 0;
}

// ---- negative controls --------------------------------------------------------------------------------------------------
#define NEG(v) int g_neg_##v = 0;
NEG(playSound) NEG(startSoundSlot) NEG(loadSoundTable) NEG(shutdownSoundSystem) NEG(initSound) NEG(shutdownSound)
NEG(soundVersionCheck) NEG(freeSoundLib) NEG(callSoundHook74) NEG(callSoundHook94) NEG(createSound) NEG(releaseSoundObj)
NEG(clearSoundPtrs) NEG(ctorBase) NEG(scalarDtor4841c0) NEG(sndDtor) NEG(setMode) NEG(musicSetFile) NEG(unlinkSound)
NEG(unlinkFromList) NEG(stop484550) NEG(queryDevice) NEG(status) NEG(setField34) NEG(setField38) NEG(applyField484750)
NEG(applyField4847b0) NEG(setPan) NEG(ctorDerived) NEG(musicScalarDtor) NEG(musicDtor) NEG(play) NEG(configure) NEG(busy)
NEG(setName) NEG(streamOpen) NEG(open) NEG(reopen) NEG(applyMode) NEG(start) NEG(stop484f00) NEG(setPitch) NEG(flags)
NEG(setVolume) NEG(ctor485260) NEG(scalarDtor4852b0) NEG(setVtbl) NEG(voiceRxCtor) NEG(openHookA) NEG(openHookB)
NEG(openHookC) NEG(closeHookC) NEG(c2) NEG(isDisabled) NEG(isMuted) NEG(hashRemove) NEG(resourceQuery) NEG(resourceLoad)
NEG(resourceLoadB) NEG(releaseA) NEG(releaseB) NEG(releaseC) NEG(closeHookA) NEG(closeHookB) NEG(showMessageBox)
NEG(gSoundsDtor) NEG(arrayInit) NEG(deviceDtors) NEG(playChild) NEG(voiceRxStart)
#undef NEG
int g_neg_none = 0;
#define NG(v) SG_NEGCTL(g_neg_##v)

// ---- S7.12: the course-sfx directory (U-1499, roadmap decision G15; log/roadmap/S7/12_notes.md) --------------------------
// The exe's sound table names the course effects `sounds\Golf sfx\<file>` (17 strings, one more as `sounds\golf sfx\`). The
// CD's InstallShield header names the directory `Golf Sfx`, which an NTFS install matches case-insensitively, but the extracted
// install (unshield writes spaces in directory names as underscores) holds `sounds\Golf_Sfx`, so on it every mmioOpenA of those
// names fails in the original and those slots never sound (U-1499). Defect switch sound.golfSfxAlias: 1 (default) = the
// original's failed open; 0 = when the exe's name is absent, the same file under the install's `Golf_Sfx` (directory and file
// matched case-insensitively). The simulation never opens a sound file (every segment runs the exe's device-less sound path),
// so the switch reaches only a device sink.
int g_defect_golf_sfx = 1;
int g_neg_golfSfx = 0;  // 1: the directory compared case-sensitively; 2: deviceStep 3's slot check ignores the alias;
                        // 3 (S7.13): the music directory aliases ignored

bool ieq(const std::string &a, const std::string &b, bool fold) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    const auto x = static_cast<unsigned char>(a[i]), y = static_cast<unsigned char>(b[i]);
    if (fold ? std::tolower(x) != std::tolower(y) : x != y) return false;
  }
  return true;
}

// The directories unshield renamed (a space written as `_`) that the exe names with the space. S7.12: the course effects;
// S7.13 (decision G16): the three theme music directories `sounds\music\Desert Music\`, `Links Music\`, `Tropical Music\`
// (six tracks of the device run open nowhere at switch 1: log/roadmap/S7/12_absent_names.log). The seven other absent names are
// wrong on the CD itself and stay absent at 0.
struct DirAlias {
  const char *parent;  // the components before the renamed directory, `/`-joined
  const char *exe;     // the exe's spelling
  const char *inst;    // the extracted install's spelling
};
constexpr DirAlias kDirAliases[] = {
    {"sounds", "Golf sfx", "Golf_Sfx"},
    {"sounds/music", "Desert Music", "Desert_Music"},
    {"sounds/music", "Links Music", "Links_Music"},
    {"sounds/music", "Tropical Music", "Tropical_Music"},
};

// `n` as a GameFiles name (`/` separators). At switch 0 the install's name for an absent name under one of kDirAliases' exe
// directories (directory and file matched case-insensitively), else n.
std::string golf_sfx_name(const GameFiles &f, const std::string &n) {
  if (SG_DEFECT(g_defect_golf_sfx) || n.empty() || f.exists(n)) return n;
  const bool fold = NG(golfSfx) != 1;
  for (const DirAlias &al : kDirAliases) {
    if (NG(golfSfx) == 3 && std::string(al.parent) != "sounds") continue;  // control 3: the music aliases ignored
    const std::string parent = al.parent;
    if (n.size() <= parent.size() + 1 || !ieq(n.substr(0, parent.size()), parent, fold) || n[parent.size()] != '/') continue;
    const size_t a = parent.size(), b = n.find('/', a + 1);
    if (b == std::string::npos || b + 1 >= n.size() || !ieq(n.substr(a + 1, b - a - 1), al.exe, fold)) continue;
    const std::string head = n.substr(0, a + 1), rest = n.substr(b + 1), inst = al.inst;
    if (f.exists(head + inst + "/" + rest)) return head + inst + "/" + rest;
    for (const DirEntry &d : f.list(head + inst))  // the install's own spelling (a case-sensitive host)
      if (d.is_dir)
        for (const DirEntry &e : f.list(head + d.name + "/" + rest))
          if (!e.is_dir) return head + d.name + "/" + e.name;
    return n;
  }
  return n;
}

}  // namespace

// ================================================================================================ public: dispatch
const ExeFn *exe_functions(size_t *count) {
  if (count) *count = kExeFnCount;
  return kExeFns;
}
const char *exe_function_name(uint32_t rva) {
  for (const ExeFn &f : kExeFns)
    if (f.rva == rva) return f.name;
  return nullptr;
}

void audio_exe_set_replay(Ctx &ctx, bool replay) { ectx(ctx).replay = replay; }
bool audio_exe_replay(Ctx &ctx) { return ectx(ctx).replay; }
void audio_exe_claim(Ctx &ctx, uint32_t va) { ectx(ctx).claims.insert(va); }
void audio_exe_reset(Ctx &ctx) {
  std::lock_guard<std::mutex> lk(g_mu);
  table().erase(&ctx);
}
const AudioExeCounters &audio_exe_counters(Ctx &ctx) { return ectx(ctx).counters; }

void exe_map_image(Ctx &ctx) {
  const U4 b = exe_base(ctx);
  if (mapped(ctx, b, 0x40) && mapped(ctx, b + kKVolumeScale, 8)) return;
  std::lock_guard<std::mutex> lk(g_img_mu);
  std::vector<uint8_t> &img = shared_image();
  if (img.empty()) {
    const std::vector<uint8_t> f = ctx.files().read_all("golf_clean.exe");
    auto r32 = [&](size_t o) {
      if (o + 4 > f.size()) throw MissingResource{"golf_clean.exe: truncated"};
      U4 v;
      std::memcpy(&v, f.data() + o, 4);
      return v;
    };
    auto r16 = [&](size_t o) {
      if (o + 2 > f.size()) throw MissingResource{"golf_clean.exe: truncated"};
      uint16_t v;
      std::memcpy(&v, f.data() + o, 2);
      return v;
    };
    const U4 pe = r32(0x3c);
    if (r32(pe) != 0x4550u) throw MissingResource{"golf_clean.exe: not a PE file"};
    const uint16_t nsec = r16(pe + 6), optsz = r16(pe + 0x14);
    const size_t opt = pe + 0x18;
    if (r32(opt + 0x38) != kExeSizeOfImage) throw MissingResource{"golf_clean.exe: SizeOfImage differs from the anchored build"};
    std::vector<uint8_t> out(kExeRdataEnd, 0);
    const U4 hsize = r32(opt + 0x3c);
    std::memcpy(out.data(), f.data(), std::min<size_t>({hsize, f.size(), out.size()}));
    bool data_ok = false;
    for (uint16_t i = 0; i < nsec; ++i) {
      const size_t s = opt + optsz + 40u * i;
      char nm[9] = {};
      std::memcpy(nm, f.data() + s, 8);
      const U4 vsize = r32(s + 8), rva = r32(s + 12), rawsz = r32(s + 16), rawoff = r32(s + 20);
      if (std::strcmp(nm, ".data") == 0) data_ok = rva == kExeDataRva && vsize == sizeof(GameState);
      if (rva >= kExeRdataEnd) continue;
      const U4 n = std::min(rawsz, vsize ? vsize : rawsz);
      if (static_cast<uint64_t>(rawoff) + n > f.size() || static_cast<uint64_t>(rva) + n > kExeRdataEnd)
        throw MissingResource{"golf_clean.exe: section outside the file"};
      std::memcpy(out.data() + rva, f.data() + rawoff, n);
    }
    if (!data_ok) throw MissingResource{"golf_clean.exe: .data differs from the GameState"};
    img = std::move(out);
  }
  if (!ctx.mem.map(b, img.data(), kExeRdataEnd, true)) throw MissingResource{"golf_clean.exe image: the range is taken"};
}

Ret exe_call_va(Ctx &ctx, uint32_t va, SndAbi abi, const uint32_t *args, int nargs, bool want_st0) {
  ExeCtx &e = ectx(ctx);
  const U4 b = exe_base(ctx);
  if (e.claims.count(va)) return snd_call_va(ctx, va, abi, args, nargs, want_st0);  // the core routes it to the host
  if (va - b < kExeImageSize) {
    const char *name = exe_function_name(va - b);
    const FuncDef *f = name ? port_fn(name) : nullptr;
    if (f != nullptr) {
      if (f->arity != nargs) throw MissingResource{name};
      ++e.counters.dispatched;
      Ret r;
      f->fn(ctx, args, r);
      return r;
    }
    if (!e.replay) throw MissingResource{name ? name : "golf_clean.exe code with no port function"};
    ++e.counters.fallbacks;
    e.counters.fallback_names += std::string(name ? name : "?") + " ";
    return snd_call_va(ctx, va, abi, args, nargs, want_st0);  // the host runs the original
  }
  // a cdecl call into sound.dll with more slots than the port function reads (openHook* pass (holder + 0x14, arg) to
  // Dll_Midi_Device::create_device, which reads one): the caller cleans the stack, the callee never sees the rest
  if (abi == SndAbi::Cdecl && audio_attached(ctx) && va - snd_base(ctx) < kSoundImageSize) {
    U4 t = va;
    for (int hop = 0; hop < 8 && L<uint8_t>(ctx, t) == 0xE9; ++hop) t = t + 5 + L32(ctx, t + 1);  // incremental-link thunks
    if (const FuncDef *f = snd_port_function(ctx, t - snd_base(ctx)))
      if (f->arity < nargs) nargs = f->arity;
  }
  return snd_call_va(ctx, va, abi, args, nargs, want_st0);
}
Ret exe_call_va(Ctx &ctx, uint32_t va, SndAbi abi, std::initializer_list<uint32_t> args, bool want_st0) {
  return exe_call_va(ctx, va, abi, args.begin(), static_cast<int>(args.size()), want_st0);
}
Ret exe_call(Ctx &ctx, uint32_t rva, SndAbi abi, std::initializer_list<uint32_t> args) {
  return exe_call_va(ctx, exe_base(ctx) + rva, abi, args.begin(), static_cast<int>(args.size()));
}
Ret exe_vcall(Ctx &ctx, uint32_t obj, uint32_t slot_off, std::initializer_list<uint32_t> args) {
  U4 a[kSndMaxArgs];
  int n = 0;
  a[n++] = obj;
  for (U4 v : args) a[n++] = v;
  const U4 vt = L32(ctx, obj);
  if (vt - exe_base(ctx) < kExeImageSize) exe_map_image(ctx);  // an exe class table: .rdata must be readable
  return exe_call_va(ctx, L32(ctx, vt + slot_off), SndAbi::Thiscall, a, n);
}

Ret exe_import(Ctx &ctx, uint32_t iat_rva, SndAbi abi, std::initializer_list<uint32_t> args, int32_t clock_site) {
  ExeCtx &e = ectx(ctx);
  const U4 *a = args.begin();
  const int n = static_cast<int>(args.size());
  if (e.replay) return exe_call_va(ctx, L32(ctx, exe_base(ctx) + iat_rva), abi, a, n);
  exe_map_image(ctx);
  Ret r;
  switch (iat_rva) {
    case exe_rva::iat_LoadLibraryA: r.eax = load_library(ctx, a[0]); return r;
    case exe_rva::iat_GetProcAddress: r.eax = get_proc_address(ctx, a[0], a[1]); return r;
    case exe_rva::iat_FreeLibrary: r.eax = 1; return r;  // the mapping stays (Memory has no single unmap)
    case exe_rva::iat_timeGetTime:
      if (clock_site != 0)
        if (InputSource *src = input_source(ctx)) {
          U4 t = 0;
          if (src->clock(clock_site, t)) {
            r.eax = t;
            return r;
          }
        }
      break;
    default: break;
  }
  return sink_import(ctx, iat_rva, a, n);
}

uint32_t exe_new(Ctx &ctx, uint32_t n) {
  ExeCtx &e = ectx(ctx);
  const U4 va = exe_base(ctx) + exe_rva::operator_new;
  if (e.replay || e.claims.count(va)) return exe_call_va(ctx, va, SndAbi::Cdecl, {n}).eax;
  if (e.arena.empty() || !mapped(ctx, kArenaBase, 4)) {
    e.arena.assign(kArenaSize, 0);
    e.arena_used = 0;
    if (!ctx.mem.map(kArenaBase, e.arena.data(), kArenaSize, false)) throw MissingResource{"audio_exe heap arena"};
  }
  const U4 need = (std::max<U4>(n, 1) + 15u) & ~15u;
  if (e.arena_used + need > kArenaSize) throw MissingResource{"audio_exe heap arena full"};
  const U4 p = kArenaBase + e.arena_used;
  e.arena_used += need;
  ++e.counters.heap_allocs;
  return p;
}
void exe_delete(Ctx &ctx, uint32_t p) {
  ExeCtx &e = ectx(ctx);
  const U4 va = exe_base(ctx) + exe_rva::operator_delete;
  if (e.replay || e.claims.count(va)) {
    exe_call_va(ctx, va, SndAbi::Cdecl, {p});
    return;
  }
  // the arena never reuses a block: a delete has no effect on the port's state
}

// ================================================================================================ the functions
namespace {

constexpr SndAbi kThis = SndAbi::Thiscall, kC = SndAbi::Cdecl;
inline Ret ec(Ctx &c, U4 rva, SndAbi abi, std::initializer_list<U4> a) { return exe_call(c, rva, abi, a); }
inline Ret vc(Ctx &c, U4 obj, U4 slot, std::initializer_list<U4> a) { return exe_vcall(c, obj, slot, a); }
// a device object's slot (a sound.dll object or a fixture's: thiscall through the audio core)
inline Ret dv(Ctx &c, U4 obj, U4 slot, std::initializer_list<U4> a) { return exe_vcall(c, obj, slot, a); }
U4 ex(Ctx &c, U4 rva) { return exe_base(c) + rva; }

U4 vstrlen(Ctx &c, U4 s) {
  U4 n = 0;
  while (L<uint8_t>(c, s + n) != 0) ++n;
  return n;
}
// VC6's inline copy of n bytes: n / 4 dwords then n % 4 bytes, forward
void movs(Ctx &c, U4 dst, U4 src, U4 n) {
  for (U4 i = 0; i < n / 4; ++i) S32(c, dst + 4 * i, L32(c, src + 4 * i));
  for (U4 i = n & ~3u; i < n; ++i) S<uint8_t>(c, dst + i, L<uint8_t>(c, src + i));
}

// ---- top-level sound API -------------------------------------------------------------------------------------------------
// playSound(idx, volume, pan, pitch, field34): idx -1 returns -1; else the Snd at g_sounds + idx * 0x6c (32-bit wrap) gets
// Snd::setPitch, setVolume, setPan, setField34 (direct calls, playSound+0x1e..+0x42) and Snd::play's eax is returned.
I4 play_sound(Ctx &c, I4 idx, I4 vol, I4 pan, I4 pitch, I4 f34) {
  if (idx == -1) return -1;
  const U4 snd = GSV(c, &GameState::g_sounds) + static_cast<U4>(idx) * 0x6cu;
  ec(c, 0x84f40, kThis, {snd, static_cast<U4>(pitch)});
  ec(c, 0x85140, kThis, {snd, static_cast<U4>(NG(playSound) == 1 ? vol + 1 : vol)});
  ec(c, 0x847f0, kThis, {snd, static_cast<U4>(pan)});
  ec(c, 0x846b0, kThis, {snd, static_cast<U4>(f34)});
  return static_cast<I4>(ec(c, 0x84940, kThis, {snd}).eax);
}
// startSoundSlot(i): Snd::applyField484750(1000) on g_sounds + i * 0x6c (i * 27 * 4)
U4 start_sound_slot(Ctx &c, U4 i) {
  const U4 snd = GSV(c, &GameState::g_sounds) + i * 27u * 4u;
  return ec(c, 0x84750, kThis, {snd, NG(startSoundSlot) == 1 ? 999u : 1000u}).eax;
}
// loadSoundTable(): the 219 groups of its own instructions `push mode` (6a imm8 / 68 imm32), `push path` (68 imm32),
// `mov ecx, slot` (b9 imm32), `call Snd::applyMode484e30` (e8 rel32), up to its `ret`: each group runs
// Snd::applyMode484e30(path, mode) on the slot; eax = the last call's. The table stays in the user's exe (read from the
// mapped .text); every group is checked to call Snd::applyMode484e30.
U4 load_sound_table(Ctx &c) {
  exe_map_image(c);
  const U4 b = exe_base(c);
  U4 p = b + exe_rva::loadSoundTable, r = 0;
  int groups = 0;
  for (;;) {
    const uint8_t op = L<uint8_t>(c, p);
    if (op == 0xc3) break;
    U4 mode;
    if (op == 0x6a) {
      mode = static_cast<U4>(static_cast<I4>(L<int8_t>(c, p + 1)));
      p += 2;
    } else if (op == 0x68) {
      mode = L32(c, p + 1);
      p += 5;
    } else {
      throw MissingResource{"loadSoundTable: unexpected instruction in the table"};
    }
    if (L<uint8_t>(c, p) != 0x68 || L<uint8_t>(c, p + 5) != 0xb9 || L<uint8_t>(c, p + 10) != 0xe8)
      throw MissingResource{"loadSoundTable: unexpected group"};
    const U4 path = L32(c, p + 1), obj = L32(c, p + 6);
    const U4 target = p + 15 + L32(c, p + 11);
    if (target != b + exe_rva::Snd_applyMode) throw MissingResource{"loadSoundTable: a group calls another function"};
    p += 15;
    ++groups;
    if (NG(loadSoundTable) == 1 && groups == 219) break;  // the last group skipped
    r = ec(c, exe_rva::Snd_applyMode, kThis, {obj, path, mode}).eax;
  }
  return r;
}
// shutdownSoundSystem(): Snd::stop484f00 on every record g_sounds .. the end of the array (signed `jl`), then
// shutdownSound's eax.
U4 shutdown_sound_system(Ctx &c) {
  const U4 first = GSV(c, &GameState::g_sounds), end = first + 300u * 0x6cu;
  for (U4 r = first; static_cast<I4>(r) < static_cast<I4>(end); r += (NG(shutdownSoundSystem) == 1 ? 0xd8u : 0x6cu))
    ec(c, 0x84f00, kThis, {r});
  return ec(c, 0x83f10, kC, {}).eax;
}

// ---- start-up and shut-down ----------------------------------------------------------------------------------------------
// initSound(device, flags)
U4 init_sound(Ctx &c, U4 device, U4 flags) {
  U4 r = ec(c, 0x83fc0, kC, {}).eax;
  if (r != 0) return r;
  ec(c, 0x84090, kC, {0, 0});
  if (flags & 2) ec(c, 0x87a70, kThis, {GSV(c, &GameState::g_sfxDevice), device, (flags & 0x100) ? 2u : 0u});
  r = ec(c, 0x87c00, kThis, {music_device(c), device, flags}).eax;
  if (r != 0) {
    ec(c, 0x84060, kC, {});
    return r;
  }
  if (flags & 8) ec(c, 0x87c00, kThis, {GSV(c, &GameState::g_voiceDevice), device, flags});
  S32(c, GSV(c, &GameState::g_soundReady), NG(initSound) == 1 ? 2u : 1u);
  return 0;
}
// shutdownSound()
U4 shutdown_sound(Ctx &c) {
  if (sound_ready(c) == 0) return 1;
  exe_call_va(c, sound_fn(c, 3), kC, {});  // g_soundFns[3] (release_sound), no argument
  const U4 head = GSV(c, &GameState::g_soundHead);
  for (U4 s = L32(c, head); s != 0; s = L32(c, head)) {
    if (L32(c, s + 0x40) != 0) {
      ec(c, 0x84110, kC, {L32(c, s + 0x40)});
      S32(c, s + 0x40, 0);
    }
    if (L32(c, s + 0x50) != 0) {
      exe_delete(c, L32(c, s + 0x50));
      S32(c, s + 0x50, 0);
    }
    const U4 f = L32(c, s + 0x44);
    if ((f & 2) && (f & 0x200)) {
      ec(c, 0x843e0, kC, {s});
      ec(c, 0x84110, kC, {s});
    } else {
      vc(c, s, 0x80, {});
    }
    if (NG(shutdownSound) == 1) break;
  }
  ec(c, 0x87ac0, kThis, {GSV(c, &GameState::g_sfxDevice)});
  ec(c, 0x87460, kThis, {music_device(c)});
  ec(c, 0x87c40, kThis, {GSV(c, &GameState::g_voiceDevice)});
  ec(c, 0x84060, kC, {});
  return 0;
}
// soundVersionCheck()
U4 sound_version_check(Ctx &c) {
  const U4 hdll = GSV(c, &GameState::g_soundDll);
  if (L32(c, hdll) != 0) return 0;
  const U4 h = exe_import(c, exe_rva::iat_LoadLibraryA, SndAbi::Stdcall,
                           {GSV(c, &GameState::f_strpool_4e43ac) + kPoolSoundDll}).eax;
  S32(c, hdll, h);
  if (h == 0) return 1;
  const U4 fns = GSV(c, &GameState::g_soundFns);
  for (U4 i = 0; i < 11; ++i) {
    const U4 fn = exe_import(c, exe_rva::iat_GetProcAddress, SndAbi::Stdcall, {L32(c, hdll), (i + 1) & 0xffffu}).eax;
    S32(c, fns + 4 * i, fn);
    if (fn == 0) {
      ec(c, 0x84060, kC, {});
      S32(c, GSV(c, &GameState::g_soundReady), 0);
      return 1;
    }
  }
  const U4 v = ec(c, 0x840c0, kC, {}).eax;
  if ((v & 0xff00u) != (NG(soundVersionCheck) == 1 ? 0xd00u : 0xc00u))
    ec(c, 0x85590, kC, {ex(c, kMsgText), ex(c, kMsgCaption)});
  return 0;
}
// freeSoundLib()
U4 free_sound_lib(Ctx &c) {
  const U4 hdll = GSV(c, &GameState::g_soundDll);
  const U4 h = L32(c, hdll);
  S32(c, GSV(c, &GameState::g_soundReady), 0);
  if (h != 0) {
    exe_import(c, exe_rva::iat_FreeLibrary, SndAbi::Stdcall, {h});
    if (NG(freeSoundLib) != 1) S32(c, hdll, 0);
  }
  ec(c, 0x84130, kC, {});
  return 0;
}
// callSoundHook74(a, b): no library returns 4; else g_soundFns[2](a, b) (cdecl)
U4 call_sound_hook74(Ctx &c, U4 a, U4 b) {
  if (L32(c, GSV(c, &GameState::g_soundDll)) == 0) return NG(callSoundHook74) == 1 ? 5 : 4;
  return exe_call_va(c, sound_fn(c, 2), kC, {a, b}).eax;
}
// callSoundHook94(): no library returns 0; else a tail jump into g_soundFns[10] (no argument)
U4 call_sound_hook94(Ctx &c) {
  if (L32(c, GSV(c, &GameState::g_soundDll)) == 0) return NG(callSoundHook94) == 1 ? 1 : 0;
  return exe_call_va(c, sound_fn(c, 10), kC, {}).eax;
}
// createSound4840e0(a, b, c): g_soundReady 0 returns 1; else g_soundFns[0](a, b, c) (cdecl)
U4 create_sound(Ctx &c, U4 a, U4 b, U4 d) {
  if (sound_ready(c) == 0) return NG(createSound) == 1 ? 2 : 1;
  return exe_call_va(c, sound_fn(c, 0), kC, {a, b, d}).eax;
}
// releaseSoundObj(p): g_soundReady 0 returns 1; else g_soundFns[1](p)
U4 release_sound_obj(Ctx &c, U4 p) {
  if (sound_ready(c) == 0) return NG(releaseSoundObj) == 1 ? 2 : 1;
  return exe_call_va(c, sound_fn(c, 1), kC, {p}).eax;
}
// clearSoundPtrs(): the 11 entries of g_soundFns = 0 (`rep stosd`, eax 0)
void clear_sound_ptrs(Ctx &c) {
  const U4 fns = GSV(c, &GameState::g_soundFns);
  for (U4 i = 0; i < (NG(clearSoundPtrs) == 1 ? 10u : 11u); ++i) S32(c, fns + 4 * i, 0);
}

// ---- Snd ------------------------------------------------------------------------------------------------------------------
// Snd::ctor485260(): the root table, volume 0x7f, pan 0, +0xc..+0x2c (nine dwords) 0, +0x30 0; returns this
U4 snd_ctor485260(Ctx &c, U4 t) {
  S32(c, t, ex(c, kVtSndRoot));
  S32(c, t + 4, NG(ctor485260) == 1 ? 0x7eu : 0x7fu);
  S32(c, t + 8, 0);
  for (U4 i = 0; i < 9; ++i) S32(c, t + 0xc + 4 * i, 0);
  S32(c, t + 0x30, 0);
  return t;
}
// Snd::ctorBase(): Snd::ctor485260, the base table, +0x48 +0x4c +0x40 +0x50 +0x44 = 0, Snd::setField38(1000) (direct),
// +0x54 = 0; returns this
U4 snd_ctor_base(Ctx &c, U4 t) {
  ec(c, 0x85260, kThis, {t});
  S32(c, t, ex(c, kVtSndBase));
  S32(c, t + 0x48, 0);
  S32(c, t + 0x4c, 0);
  S32(c, t + 0x40, 0);
  S32(c, t + 0x50, 0);
  S32(c, t + 0x44, 0);
  ec(c, 0x846d0, kThis, {t, NG(ctorBase) == 1 ? 999u : 1000u});
  S32(c, t + 0x54, 0);
  return t;
}
// Snd::ctorDerived(): Snd::ctorBase, the Snd table, +0x58 = 0, +0x44 |= 4, Snd::setMode(1) (direct), +0x5c = 0,
// +0x60 = 1.0f, +0x3c = 0x10, +0x64 = +0x68 = 0; returns this
U4 snd_ctor_derived(Ctx &c, U4 t) {
  ec(c, 0x84150, kThis, {t});
  S32(c, t, ex(c, kVtSnd));
  S32(c, t + 0x58, 0);
  S32(c, t + 0x44, L32(c, t + 0x44) | 4u);
  ec(c, 0x84260, kThis, {t, 1});
  S32(c, t + 0x5c, 0);
  S32(c, t + 0x60, 0x3f800000u);
  S32(c, t + 0x3c, NG(ctorDerived) == 1 ? 0xfu : 0x10u);
  S32(c, t + 0x64, 0);
  S32(c, t + 0x68, 0);
  return t;
}
// the deleting destructors: the destructor (direct), operator delete when flags bit 0; returns this
U4 scalar_deleting(Ctx &c, U4 dtor_rva, U4 t, U4 flags, int neg) {
  ec(c, dtor_rva, kThis, {t});
  if ((flags & 1) && neg != 1) exe_delete(c, t);
  return t;
}
// Snd::dtor(): the base table; +0x50 to operator delete and 0; +0x40 to releaseSoundObj and 0; Snd::unlinkFromList and
// Snd::setVtbl4852d0 (direct); eax = unlinkFromList's (setVtbl leaves it)
U4 snd_dtor(Ctx &c, U4 t) {
  S32(c, t, ex(c, kVtSndBase));
  if (L32(c, t + 0x50) != 0) {
    exe_delete(c, L32(c, t + 0x50));
    S32(c, t + 0x50, 0);
  }
  if (L32(c, t + 0x40) != 0) {
    ec(c, 0x84110, kC, {L32(c, t + 0x40)});
    if (NG(sndDtor) != 1) S32(c, t + 0x40, 0);
  }
  const U4 r = ec(c, 0x844e0, kThis, {t}).eax;
  ec(c, 0x852d0, kThis, {t});
  return r;
}
// Snd::setVtbl4852d0(): the root table into +0 (eax untouched: the caller's)
void snd_set_vtbl(Ctx &c, U4 t) { S32(c, t, ex(c, NG(setVtbl) == 1 ? kVtSndBase : kVtSndRoot)); }
// SndMusic::dtor(): the Snd table; group +0x3c below 0x10 -> HashTable::remove(this) on g_musicDevice; +0x50 to operator
// delete; +0x50 = 0; Snd::unlinkFromList; Snd::dtor's eax
U4 music_dtor(Ctx &c, U4 t) {
  S32(c, t, ex(c, kVtSnd));
  if (L32(c, t + 0x3c) < 0x10u) ec(c, 0x876c0, kThis, {music_device(c), t});
  const U4 buf = L32(c, t + 0x50);
  if (buf != 0 && NG(musicDtor) != 1) exe_delete(c, buf);
  S32(c, t + 0x50, 0);
  ec(c, 0x844e0, kThis, {t});
  return ec(c, 0x841e0, kThis, {t}).eax;
}
// Snd::setMode(m): +0x44 |= 4 -> 0x10, 2 -> 8, 1 -> 4, 5 -> 0x28, 6 -> 0x100, 7 -> 0x80; +0x54 = m (eax: the caller's)
void snd_set_mode(Ctx &c, U4 t, U4 m) {
  U4 bit = 0;
  switch (m) {
    case 4: bit = 0x10; break;
    case 2: bit = 8; break;
    case 1: bit = 4; break;
    case 5: bit = 0x28; break;
    case 6: bit = 0x100; break;
    case 7: bit = 0x80; break;
    default: break;
  }
  if (NG(setMode) == 1 && m == 6) bit = 0x80;
  if (bit) S32(c, t + 0x44, L32(c, t + 0x44) | bit);
  S32(c, t + 0x54, m);
}
// SndMusic::setFile(name)
U4 music_set_file(Ctx &c, U4 t, U4 name) {
  if (sound_ready(c) == 0) return 1;
  const U4 slot = t + 0x40;
  if (L32(c, slot) == 0) {
    const U4 r = ec(c, 0x840e0, kC, {slot, name, 1}).eax;
    if (r != 0) return r;
  } else if (dv(c, L32(c, slot), 0x60, {}).eax != 0) {
    return 0xf;
  }
  const U4 r = dv(c, L32(c, slot), 0x10, {name}).eax;
  const U4 fl = L32(c, t + 0x44);
  if (r == 0) {
    if (!(fl & 1)) {
      S32(c, t + 0x44, fl | 1u);
      vc(c, t, 0x7c, {});
      if (L32(c, t + 0x30) != 0) dv(c, L32(c, slot), 0x48, {1});
    }
  } else {
    S32(c, t + 0x44, fl & ~1u);
  }
  const U4 len = vstrlen(c, name) + 1;
  const U4 copy = exe_new(c, len);
  movs(c, copy, name, NG(musicSetFile) == 1 ? len - 1 : len);
  const U4 old = L32(c, t + 0x50);
  if (old != 0) exe_delete(c, old);
  S32(c, t + 0x50, copy);
  return r;
}
// unlinkSound(node) (cdecl): NULL 0xa; flag bit 1 of +0x44 clear 0; else out of the list g_soundHead / g_soundTail
U4 unlink_sound(Ctx &c, U4 node) {
  if (node == 0) return 0xa;
  if ((L<uint8_t>(c, node + 0x44) & 2) == 0) return 0;
  const U4 prev = L32(c, node + 0x48);
  if (prev != 0) S32(c, prev + 0x4c, L32(c, node + 0x4c));
  else S32(c, GSV(c, &GameState::g_soundHead), L32(c, node + 0x4c));
  const U4 next = L32(c, node + 0x4c);
  if (next != 0) S32(c, next + 0x48, L32(c, node + 0x48));
  else S32(c, GSV(c, &GameState::g_soundTail), L32(c, node + 0x48));
  const U4 fl = L32(c, node + 0x44);
  S32(c, node + 0x4c, 0);
  S32(c, node + 0x48, 0);
  S32(c, node + 0x44, fl & (NG(unlinkSound) == 1 ? 0xfffffffdu : 0xfffffdfdu));
  return 0;
}
// Snd::unlinkFromList(): the same list without the NULL test, clearing bit 1 only (`and al, 0xfd`)
U4 unlink_from_list(Ctx &c, U4 t) {
  if ((L<uint8_t>(c, t + 0x44) & 2) == 0) return 0;
  const U4 prev = L32(c, t + 0x48), next = L32(c, t + 0x4c);
  if (prev != 0) S32(c, prev + 0x4c, next);
  else S32(c, GSV(c, &GameState::g_soundHead), next);
  const U4 n2 = L32(c, t + 0x4c);
  if (n2 != 0) S32(c, n2 + 0x48, L32(c, t + 0x48));
  else S32(c, GSV(c, &GameState::g_soundTail), L32(c, t + 0x48));
  const U4 fl = L32(c, t + 0x44);
  S32(c, t + 0x4c, 0);
  S32(c, t + 0x44, (fl & ~0xffu) | (fl & (NG(unlinkFromList) == 1 ? 0xffu : 0xfdu) & 0xffu));
  S32(c, t + 0x48, 0);
  return 0;
}
// Snd::linkIntoList() (C2): already linked (next or prev set) returns 0; else appended at g_soundTail, bit 1 set
U4 link_into_list(Ctx &c, U4 t) {
  if (L32(c, t + 0x4c) != 0 || L32(c, t + 0x48) != 0) return NG(c2) == 1 ? 1 : 0;
  const U4 tail = GSV(c, &GameState::g_soundTail);
  const U4 last = L32(c, tail);
  if (last == 0) {
    S32(c, GSV(c, &GameState::g_soundHead), t);
    S32(c, tail, t);
    const U4 fl = L32(c, t + 0x44);
    S32(c, t + 0x4c, 0);
    S32(c, t + 0x48, 0);
    S32(c, t + 0x44, fl | 2u);
    return 0;
  }
  S32(c, t + 0x48, last);
  S32(c, tail, t);
  S32(c, L32(c, t + 0x48) + 0x4c, t);
  const U4 fl = L32(c, t + 0x44);
  S32(c, t + 0x4c, 0);
  S32(c, t + 0x44, fl | 2u);
  return 0;
}
// Snd::stop484550(): device slot 0x14 (result kept), own slot 0x80, +0x40 = 0, bit 0 of +0x44 cleared
U4 snd_stop484550(Ctx &c, U4 t) {
  U4 r = 0;
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) r = dv(c, dev, 0x14, {}).eax;
  vc(c, t, 0x80, {});
  const U4 fl = L32(c, t + 0x44);
  S32(c, t + 0x40, 0);
  S32(c, t + 0x44, (fl & ~0xffu) | (fl & (NG(stop484550) == 1 ? 0xffu : 0xfeu) & 0xffu));
  return r;
}
// Snd::queryDevice4845d0(): device slot 0x5c (tail jump) or 0
U4 query_device(Ctx &c, U4 t) {
  const U4 dev = L32(c, t + 0x40);
  if (dev == 0) return NG(queryDevice) == 1 ? 1 : 0;
  return dv(c, dev, 0x5c, {}).eax;
}
// Snd::status(): device slot 0x20 (tail jump) or 0x14
U4 snd_status(Ctx &c, U4 t) {
  const U4 dev = L32(c, t + 0x40);
  if (dev == 0) return NG(status) == 1 ? 0x13 : 0x14;
  return dv(c, dev, 0x20, {}).eax;
}
// Snd::setField34(v): +0x34 = v; device slot 0x4c(v)'s eax, else v
U4 set_field34(Ctx &c, U4 t, U4 v) {
  S32(c, t + 0x34, NG(setField34) == 1 ? v + 1 : v);
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) return dv(c, dev, 0x4c, {v}).eax;
  return v;
}
// Snd::setField38(v): v 0 returns 10; +0x38 = v; device slot 4(v); 0
U4 set_field38(Ctx &c, U4 t, U4 v) {
  if (v == 0) return 10;
  S32(c, t + 0x38, NG(setField38) == 1 ? v ^ 1u : v);
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) dv(c, dev, 4, {v});
  return 0;
}
// SndBase::setField38(v) (C2): v 0 returns 10; +0x38 = v; 0
U4 base_set_field38(Ctx &c, U4 t, U4 v) {
  if (v == 0) return 10;
  S32(c, t + 0x38, NG(c2) == 1 ? v + 1 : v);
  return 0;
}
// Snd::setField38Voice54(v) (C2): v 0 returns 10; +0x38 = v; device slot 0x54(v); 0
U4 set_field38_voice54(Ctx &c, U4 t, U4 v) {
  if (v == 0) return 10;
  S32(c, t + 0x38, NG(c2) == 1 ? v + 1 : v);
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) dv(c, dev, 0x54, {v});
  return 0;
}
// Snd::applyField484750(a) / applyField4847b0(a): own slot 4 / 0x54 with a; zero -> own slot 0x28's result
U4 apply_field(Ctx &c, U4 t, U4 slot, U4 a, int neg) {
  U4 r = vc(c, t, slot, {a}).eax;
  if (r == 0 && neg != 1) r = vc(c, t, 0x28, {}).eax;
  return r;
}
// Snd::applyField38Voice28 / 30 (C2): +0x38 and the device set -> device slot 0x28 / 0x30 (tail jump), else 0x13
U4 apply_field38_voice(Ctx &c, U4 t, U4 slot) {
  if (L32(c, t + 0x38) != 0) {
    const U4 dev = L32(c, t + 0x40);
    if (dev != 0) return dv(c, dev, slot, {}).eax;
  }
  return NG(c2) == 1 ? 0x14 : 0x13;
}
// Snd::setPan(p): clamp to [-0x40, 0x3f], +8 = p; device slot 0x44(p)'s eax, else p
U4 set_pan(Ctx &c, U4 t, I4 p) {
  if (p < -0x40) p = -0x40;
  else if (p > (NG(setPan) == 1 ? 0x3e : 0x3f)) p = (NG(setPan) == 1 ? 0x3e : 0x3f);
  S32(c, t + 8, static_cast<U4>(p));
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) return dv(c, dev, 0x44, {static_cast<U4>(p)}).eax;
  return static_cast<U4>(p);
}
// Snd::setPitch(p): clamp to [-0x4b0, 0x4b0], +0x5c = p; device slot 0x9c(p); 0
U4 set_pitch(Ctx &c, U4 t, I4 p) {
  const I4 hi = NG(setPitch) == 1 ? 0x4af : 0x4b0;
  if (p < -0x4b0) p = -0x4b0;
  else if (p > hi) p = hi;
  S32(c, t + 0x5c, static_cast<U4>(p));
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) dv(c, dev, 0x9c, {static_cast<U4>(p)});
  return 0;
}
// Snd::setVolume(v) and Snd::setVolumeBase(v) (the same body): m = v & 0x7f into +4; a group +0x3c below 0x10 makes eax
// __ftol((m * k) * gain) with k the double at f_k_dbl_4bacf0 and gain the group's dword (g_musicDevice + 0x28 + 0x1c *
// group, zero-extended into a qword), on the x87 unit at the caller's control word: fild dword, fmul qword, fild qword,
// fmulp, __ftol (setVolume+0x21..+0x4d); the device's slot 0x40(eax)'s eax, else eax.
U4 set_volume(Ctx &c, U4 t, U4 v, bool base) {
  const U4 m = v & 0x7fu;
  S32(c, t + 4, m);
  U4 eax = m;
  const U4 group = L32(c, t + 0x3c);
  if (group < 0x10u) {
    exe_map_image(c);
    const uint64_t kbits = L<uint64_t>(c, ex(c, kKVolumeScale));
    const U4 gain = L32(c, music_device(c) + 0x28 + group * 0x1c);
    x87::Unit &u = x87::unit(c);
    SG_X87_GUARD(c, {
      u.push(x87::fild(static_cast<int32_t>(m)));
      u.st(0) = x87::mul(u.st(0), x87::from_f64(kbits), u.cw);
      u.push(x87::fild(static_cast<int64_t>(gain)));
      u.st(1) = x87::mul(u.st(1), u.st(0), u.cw);
      u.pop();
      const x87::X80 val = u.pop();
      const uint16_t chop = static_cast<uint16_t>(u.cw | 0x0c00u);
      const int64_t q = x87::fist(val, 64, NG(setVolume) == 1 ? u.cw : chop);
      eax = static_cast<U4>(static_cast<uint64_t>(q));
    });
  }
  if (base) S32(c, t + 4, eax & (NG(c2) == 1 ? 0x3fu : 0x7fu));  // setVolumeBase stores the scaled value back (setVolumeBase+0x52..+0x57)
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) return dv(c, dev, 0x40, {eax}).eax;
  return eax;
}
// Snd::setLoop(v) (C2): +0x30 = v; device slot 0x48(v)'s eax, else v
U4 set_loop(Ctx &c, U4 t, U4 v) {
  S32(c, t + 0x30, NG(c2) == 1 ? v + 1 : v);
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) return dv(c, dev, 0x48, {v}).eax;
  return v;
}
// Snd::setField60(v) (C2): +0x60 = v; device slot 0xec(v)'s eax, else 0x14
U4 set_field60(Ctx &c, U4 t, U4 v) {
  S32(c, t + 0x60, NG(c2) == 1 ? v + 1 : v);
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) return dv(c, dev, 0xec, {v}).eax;
  return 0x14;
}
// Snd::setFlags(f) (C2)
U4 set_flags(Ctx &c, U4 t, U4 f) {
  if (f & 2) S32(c, t + 0x30, 1);
  const U4 o = t + 0x58;
  if (f & 1) S32(c, o, L32(c, o) | 1u);
  const U4 four = f & 4;
  if (four) S32(c, o, L32(c, o) | 2u);
  if (f & 0x40) S32(c, o, L32(c, o) | 8u);
  if (f & 0x80) S32(c, o, L32(c, o) | 0x10u);
  if (!four && (f & 0x10)) S32(c, o, L32(c, o) | 4u);
  if (!four && (f & 0x100)) S32(c, o, L32(c, o) | 0x20u);
  if (f & 0x400) S32(c, o, L32(c, o) | (NG(c2) == 1 ? 0x80u : 0x40u));
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) dv(c, dev, 0x6c, {f});
  return 0;
}
// Snd::flags(): the device's slot 0x70 (or 0) | 2 for a set +0x30 | +0x58's bits mapped
U4 snd_flags(Ctx &c, U4 t) {
  U4 r = 0;
  const U4 dev = L32(c, t + 0x40);
  if (dev != 0) r = dv(c, dev, 0x70, {}).eax;
  if (L32(c, t + 0x30) != 0) r |= 2;
  const uint8_t b = L<uint8_t>(c, t + 0x58);
  if (b & 1) r |= 1;
  if (b & 8) r |= 0x40;
  if (b & 2) r |= 4;
  if (b & 4) r |= 0x10;
  if (b & 0x10) r |= 0x80;
  if (b & 0x20) r |= (NG(flags) == 1 ? 0x200u : 0x100u);
  return r;
}
// Snd::play()
U4 snd_play(Ctx &c, U4 t) {
  U4 result = 0;
  const U4 key = L32(c, t + 0x3c);
  if (key < 0x10u) {
    if ((ec(c, 0x87770, kThis, {music_device(c), key}).eax & 0xff) != 0) return 0x14;
    if ((ec(c, 0x877a0, kThis, {music_device(c), L32(c, t + 0x3c)}).eax & 0xff) != 0) return 0x14;
    if (L<uint8_t>(c, t + 0x58) & 0x10) vc(c, t, 0x40, {L32(c, t + 4)});
  }
  U4 child = L32(c, t + 0x40);
  if (child != 0) {
    result = dv(c, child, 0x1c, {}).eax;
  } else if (L<uint8_t>(c, t + 0x58) & 0x10) {
    ec(c, 0x84d60, kThis, {t});
    child = L32(c, t + 0x40);
    if (child != 0) result = dv(c, child, 0x1c, {}).eax;
  } else {
    result = NG(play) == 1 ? 0x13 : 0x14;
  }
  if (L<uint8_t>(c, t + 0x58) & 0x10) {
    S32(c, t + 0x68, exe_import(c, exe_rva::iat_timeGetTime, SndAbi::Stdcall, {}, kSiteSndPlayClock).eax);
    vc(c, t, 0x80, {});
    S32(c, t + 0x40, 0);
  }
  return result;
}
// Snd::configure(name, flags) (void)
void snd_configure(Ctx &c, U4 t, U4 name, U4 flags) {
  const U4 four = flags & 4;
  if (four != 0 && (flags & 0x80) != 0) return;
  ec(c, 0x84b80, kThis, {t, name});
  const U4 st = t + 0x58;
  S32(c, st, 0);
  if (four != 0) {
    S32(c, st, L32(c, st) | 2u);
    if (L32(c, t + 0x40) == 0) ec(c, 0x84c20, kThis, {t, name});
    const U4 snd = L32(c, t + 0x40);
    if (snd != 0) dv(c, snd, 0x6c, {flags});
  }
  if (flags & 1) S32(c, st, L32(c, st) | 1u);
  if ((flags & 0x10) && four == 0) S32(c, st, L32(c, st) | 4u);
  if (flags & 2) vc(c, t, 0x48, {1});
  if (flags & 0x40) S32(c, st, L32(c, st) | 8u);
  if (flags & 0x80) S32(c, st, L32(c, st) | 0x10u);
  if (flags & 0x100) S32(c, st, L32(c, st) | 0x20u);
  if (flags & 0x400) S32(c, st, L32(c, st) | (NG(configure) == 1 ? 0x80u : 0x40u));
}
// Snd::busy()
U4 snd_busy(Ctx &c, U4 t) {
  if (L32(c, t + 0x40) != 0 || ((L32(c, t + 0x58) >> 4) & 1) == 0) return ec(c, 0x845d0, kThis, {t}).eax;
  if (L32(c, t + 0x68) == 0) return 0;
  const U4 now = exe_import(c, exe_rva::iat_timeGetTime, SndAbi::Stdcall, {}, kSiteSndBusyClock).eax;
  const U4 elapsed = now - L32(c, t + 0x68);
  return NG(busy) == 1 ? (L32(c, t + 0x64) >= elapsed ? 1u : 0u) : (L32(c, t + 0x64) > elapsed ? 1u : 0u);
}
// Snd::setName(name)
U4 snd_set_name(Ctx &c, U4 t, U4 name) {
  if (name == 0) return 0xa;
  const U4 old = L32(c, t + 0x50);
  if (old != 0) exe_delete(c, old);
  const U4 len = vstrlen(c, name) + 1;
  const U4 copy = exe_new(c, len);
  S32(c, t + 0x50, copy);
  movs(c, copy, name, NG(setName) == 1 ? len - 1 : len);
  return 0;
}
// SndStream::open484c20(name)
U4 stream_open(Ctx &c, U4 t, U4 name) {
  const U4 slot = t + 0x40;
  if (L32(c, slot) != 0) return 0xc;
  const U4 r = ec(c, 0x840e0, kC, {slot, name, 1}).eax;
  if (r != 0) return r;
  const U4 snd = L32(c, slot);
  const U4 put = L32(c, L32(c, snd) + 0x6c);  // the new object's table is read before this's slot 0x70 runs
  const U4 v = vc(c, t, 0x70, {}).eax;
  exe_call_va(c, put, kThis, {snd, NG(streamOpen) == 1 ? v + 1 : v});
  vc(c, t, 0x7c, {});
  return 0;
}
// Snd::open(name)
U4 snd_open(Ctx &c, U4 t, U4 name) {
  const U4 slot = t + 0x40;
  U4 mask = 0;
  if (L32(c, slot) == 0) {
    const U4 r = ec(c, 0x840e0, kC, {slot, name, 1}).eax;
    if (r != 0) return r;
  }
  if (L<uint8_t>(c, t + 0x58) & 1) mask = 1;
  if (vc(c, t, 0x58, {}).eax != 0) mask |= 2;
  const U4 f = L32(c, t + 0x58);
  if (f & 8) mask |= 0x40;
  if (f & 0x10) mask |= 0x80;
  if (f & 0x40) mask |= 0x400;
  if (f & 2) mask |= (NG(open) == 1 ? 8u : 4u);
  dv(c, L32(c, slot), 0x6c, {mask});
  if (!(L<uint8_t>(c, t + 0x58) & 2)) {
    const U4 r = ec(c, 0x842f0, kThis, {t, name}).eax;
    if (r != 0) {
      ec(c, 0x84110, kC, {L32(c, slot)});
      S32(c, slot, 0);
      return r;
    }
    S32(c, t + 0x64, dv(c, L32(c, slot), 0xc8, {}).eax);
    dv(c, L32(c, slot), 0x40, {L32(c, t + 4)});
    dv(c, L32(c, slot), 0x9c, {L32(c, t + 0x5c)});
    dv(c, L32(c, slot), 0x44, {L32(c, t + 8)});
  }
  return 0;
}
// Snd::reopen()
U4 snd_reopen(Ctx &c, U4 t) {
  const U4 name = L32(c, t + 0x50);
  if (name == 0) return 8;
  const U4 slot = t + 0x40;
  if (L32(c, slot) == 0) {
    const U4 r = ec(c, 0x840e0, kC, {slot, name, 1}).eax;
    if (r != 0) return r;
  }
  U4 mask = 0;
  if (L<uint8_t>(c, t + 0x58) & 1) mask = 1;
  if (vc(c, t, 0x58, {}).eax != 0) mask |= 2;
  const U4 f = L32(c, t + 0x58);
  if (f & 4) mask |= 0x11;
  if (f & 8) {
    mask |= 0x40;
    if (!(f & 0x20)) mask |= 1;
  }
  if (f & 0x10) mask |= 0x80;
  if (f & 0x20) mask |= (NG(reopen) == 1 ? 0x200u : 0x100u);
  if (f & 0x40) mask |= 0x400;
  dv(c, L32(c, slot), 0x6c, {mask});
  const U4 r = ec(c, 0x842f0, kThis, {t, name}).eax;
  if (r != 0) {
    ec(c, 0x84110, kC, {L32(c, slot)});
    S32(c, slot, 0);
    return r;
  }
  S32(c, t + 0x64, dv(c, L32(c, slot), 0xc8, {}).eax);
  return 0;
}
// Snd::applyMode484e30(a, mode): own slot 0x88(a, mode); mode bit 2 returns 0; else own slot 0x8c's result
U4 apply_mode(Ctx &c, U4 t, U4 a, U4 mode) {
  vc(c, t, 0x88, {a, mode});
  if ((mode & 0xff) & (NG(applyMode) == 1 ? 8u : 4u)) return 0;
  return vc(c, t, 0x8c, {}).eax;
}
// Snd::start()
U4 snd_start(Ctx &c, U4 t) {
  if (L32(c, t + 0x50) == 0) return 8;
  const U4 slot = t + 0x40;
  if (L32(c, slot) == 0) {
    const U4 r = ec(c, 0x840e0, kC, {slot, L32(c, t + 0x50), 1}).eax;
    if (r != 0) return r;
  }
  U4 mask = (L<uint8_t>(c, t + 0x58) & 1) ? 1u : 0u;
  if (vc(c, t, 0x58, {}).eax != 0) mask |= 2;
  dv(c, L32(c, slot), 0x6c, {mask});
  const U4 r = dv(c, L32(c, slot), 0x84, {}).eax;
  if (r != 0) return r;
  const U4 fl = L32(c, t + 0x44);
  if (fl & 1) return 0;
  S32(c, t + 0x44, fl | 1u);
  vc(c, t, 0x7c, {});
  if (L32(c, t + 0x30) != 0) dv(c, L32(c, slot), 0x48, {NG(start) == 1 ? 0u : 1u});
  return 0;
}
// Snd::stop484f00()
U4 snd_stop484f00(Ctx &c, U4 t) {
  U4 r = 0;
  const U4 obj = L32(c, t + 0x40);
  if (obj != 0) r = dv(c, obj, 0x14, {}).eax;
  S32(c, t + 0x40, 0);
  if ((L<uint8_t>(c, t + 0x58) & 2) == 0) vc(c, t, 0x80, {});
  S32(c, t + 0x44, L32(c, t + 0x44) & (NG(stop484f00) == 1 ? 0xfffffffdu : 0xfffffffeu));
  return r;
}

// ---- VoiceRx --------------------------------------------------------------------------------------------------------------
// VoiceRx::ctor(): Snd::ctorBase, the VoiceRx table, its 8-character name (f_strpool_4e43ac + 0x1c, two dwords and the byte
// after them, copied to the frame), Snd::setMode(6), operator new(10) at +0x50 (no NULL test) and the name copied there with
// its NUL (`repne scasb` length, `rep movsd` / `rep movsb`); returns this
U4 voice_rx_ctor(Ctx &c, U4 t) {
  ec(c, 0x84150, kThis, {t});
  S32(c, t, ex(c, kVtVoiceRx));
  const U4 src = GSV(c, &GameState::f_strpool_4e43ac) + kPoolVoiceName;
  uint8_t name[9];
  for (U4 i = 0; i < 9; ++i) name[i] = L<uint8_t>(c, src + i);
  ec(c, 0x84260, kThis, {t, 6});
  const U4 p = exe_new(c, 10);
  S32(c, t + 0x50, p);
  U4 n = 0;
  while (n < 9 && name[n] != 0) ++n;
  n += 1;
  if (NG(voiceRxCtor) == 1) n -= 1;
  for (U4 i = 0; i < n; ++i) S<uint8_t>(c, p + i, name[i]);
  return t;
}
// VoiceRx::start(a, b, d) (C2): a NULL +0x40 runs createSound4840e0(this + 0x40, 0, 6) and a non-zero result returns it;
// with an object its slot 0x84(a, b, d): non-zero releases it (releaseSoundObj, +0x40 = 0); own slot 0x7c; eax = the
// slot's result. A NULL object after a zero create returns the create's 0.
U4 voice_rx_start(Ctx &c, U4 t, U4 a, U4 b, U4 d) {
  const U4 slot = t + 0x40;
  U4 eax = 0;
  if (L32(c, slot) == 0) {
    eax = ec(c, 0x840e0, kC, {slot, 0, 6}).eax;
    if (eax != 0) return eax;
  }
  const U4 obj = L32(c, slot);
  if (obj == 0) return eax;
  const U4 r = dv(c, obj, 0x84, {a, b, d}).eax;
  if (r != 0 && NG(voiceRxStart) != 1) {
    ec(c, 0x84110, kC, {L32(c, slot)});
    S32(c, slot, 0);
  }
  vc(c, t, 0x7c, {});
  return r;
}

// ---- device holders ---------------------------------------------------------------------------------------------------------
// openHookA / B / C(arg): the holder's device (+0x14) set returns 0xc; the create_device entry g_soundFns[4] / [6] / [8]
// NULL returns 0x14; else create(this + 0x14, arg) (cdecl)
U4 open_hook(Ctx &c, U4 t, U4 arg, int fn, int neg) {
  if (L32(c, t + 0x14) != 0) return 0xc;
  const U4 create = sound_fn(c, fn);
  if (create == 0) return neg == 1 ? 0x13 : 0x14;
  return exe_call_va(c, create, kC, {t + 0x14, arg}).eax;
}
// closeHookA / B / C(): a device (+0x14) and the close entry g_soundFns[5] / [7] / [9] -> the entry (no argument), +0x14 =
// 0, 0; else 0x14
U4 close_hook(Ctx &c, U4 t, int fn, int neg) {
  if (L32(c, t + 0x14) != 0) {
    const U4 hook = sound_fn(c, fn);
    if (hook != 0) {
      exe_call_va(c, hook, kC, {});
      if (neg != 1) S32(c, t + 0x14, 0);
      return 0;
    }
  }
  return 0x14;
}
// releaseWithHookA / B(): a device: its slot 0x14, closeHookA / B (direct), +0x14 = 0; 0
// releaseWithHookC(): a device: its slot 0x14, own slot 8, +0x14 = 0; 0
U4 release_with_hook(Ctx &c, U4 t, U4 close_rva, int neg) {
  const U4 dev = L32(c, t + 0x14);
  if (dev != 0) {
    dv(c, dev, 0x14, {});
    if (close_rva) ec(c, close_rva, kThis, {t});
    else vc(c, t, 8, {});
    if (neg != 1) S32(c, t + 0x14, 0);
  }
  return 0;
}
// resourceLoad(device, flags) / resourceLoadB: own slot 4(flags) non-zero is returned; else the device's (+0x14) slot
// 0x10(device, flags): non-zero runs own slot 8 and is returned (B also clears +0x14); zero returns 0
U4 resource_load(Ctx &c, U4 t, U4 device, U4 flags, bool b, int neg) {
  const U4 r = vc(c, t, 4, {flags}).eax;
  if (r != 0) return r;
  const U4 e = dv(c, L32(c, t + 0x14), 0x10, {device, flags}).eax;
  if (e != 0) {
    vc(c, t, 8, {});
    if (b && neg != 1) S32(c, t + 0x14, 0);
    return e;
  }
  return 0;
}
// resourceQuery(): the device's slot 0x74 (tail jump) or 0
U4 resource_query(Ctx &c, U4 t) {
  const U4 dev = L32(c, t + 0x14);
  if (dev == 0) return NG(resourceQuery) == 1 ? 1 : 0;
  return dv(c, dev, 0x74, {}).eax;
}
// SndGroup_isDisabled(g) / isMuted(g): g above 0xf returns al 1 (eax's upper bytes: g's); else sete / setne of the byte
// +0x24 / +0x25 of record g (0x1c each) in al (eax = g before it, so the upper bytes are g's: 0 for g <= 0xf)
U4 group_flag(Ctx &c, U4 t, U4 g, U4 off, bool want_zero, int neg) {
  if (g > 0xf) return (g & ~0xffu) | 1u;
  const uint8_t v = L<uint8_t>(c, t + g * 0x1c + off);
  const bool hit = want_zero ? (v == 0) : (v != 0);
  return (g & ~0xffu) | ((hit != (neg == 1)) ? 1u : 0u);
}
// HashTable::remove(snd): NULL or group >= 0x10 returns 0xa; else the snd's node is taken out of its group's list (head
// +0x30 / tail +0x34 / count +0x3c of record group at +0x2c), deleted, the count decremented; group = 0x10; 0. Not found:
// group = 0x10, eax = the last node's next (0).
U4 hash_remove(Ctx &c, U4 t, U4 snd) {
  if (snd == 0 || L32(c, snd + 0x3c) >= 0x10u) return 0xa;
  const U4 g = L32(c, snd + 0x3c);
  const U4 rec = t + g * 0x1c + 0x2c;
  U4 n = L32(c, t + g * 0x1c + 0x30);
  if (n != 0) {
    while (L32(c, n + 8) != snd) {
      n = L32(c, n + 4);
      if (n == 0) {
        S32(c, snd + 0x3c, 0x10);
        return 0;
      }
    }
    const U4 prev = L32(c, n);
    if (prev != 0) S32(c, prev + 4, L32(c, n + 4));
    else S32(c, rec + 4, L32(c, n + 4));
    const U4 next = L32(c, n + 4);
    if (next != 0) {
      S32(c, next, L32(c, n));
      S32(c, rec + 0xc, next);
    } else {
      S32(c, rec + 0xc, 0);
      S32(c, rec + 8, L32(c, n));
    }
    exe_delete(c, n);
    if (NG(hashRemove) != 1) S32(c, rec + 0x10, L32(c, rec + 0x10) - 1);
  }
  S32(c, snd + 0x3c, 0x10);
  return 0;
}
// NodeList::playChildSuspendingSounds(arg) (C2): no device (+0x14) returns 2. Else walks the sound list from g_soundHead:
// a Snd whose flags (own slot 0x70) have bit 0 set and bit 2 clear is stopped (own slot 0x14) and chained into a private
// list (prev +0x48 / next +0x4c); the device's slot 0x1c(arg) runs; then every chained Snd is reopened through own slot
// 0x10 with its +0x50 after its links are cleared (own slot 0x68 gives the next); 0.
U4 play_child_suspending(Ctx &c, U4 t, U4 arg) {
  if (L32(c, t + 0x14) == 0) return 2;
  U4 kept = 0;
  U4 s = L32(c, GSV(c, &GameState::g_soundHead));
  while (s != 0) {
    if ((vc(c, s, 0x70, {}).eax & 1) && !(vc(c, s, 0x70, {}).eax & 4)) {
      const U4 next = vc(c, s, 0x64, {}).eax;
      vc(c, s, 0x14, {});
      if (kept == 0) {
        S32(c, s + 0x48, 0);
        S32(c, s + 0x4c, 0);
      } else {
        S32(c, kept + 0x4c, s);
        S32(c, s + 0x48, kept);
      }
      kept = s;
      s = next;
    } else {
      s = vc(c, s, 0x64, {}).eax;
    }
  }
  dv(c, L32(c, t + 0x14), 0x1c, {arg});
  while (kept != 0) {
    const U4 cur = kept;
    kept = vc(c, cur, 0x68, {}).eax;
    S32(c, cur + 0x4c, 0);
    S32(c, cur + 0x48, 0);
    vc(c, cur, 0x10, {L32(c, cur + 0x50)});
    if (NG(playChild) == 1) break;
  }
  return 0;
}
// gSoundsDtor_448190() (atexit): the eh vector destructor iterator over g_sounds (300 x 0x6c) with SndMusic::dtor; it
// walks from the last element down (dec count, sub size)
void g_sounds_dtor(Ctx &c) {
  const U4 first = GSV(c, &GameState::g_sounds);
  for (U4 i = (NG(gSoundsDtor) == 1 ? 299u : 300u); i-- > 0;) ec(c, 0x848a0, kThis, {first + i * 0x6cu});

}

// arrayStaticInit_448160() (the CRT initializer dynInit_arrayStaticInit_448160 calls it before gameMain): the eh vector
// constructor iterator over g_sounds (push SndMusic::dtor, Snd::ctorDerived, 0x12c, 0x6c, g_sounds; call
// ehVectorConstructorIterator): Snd::ctorDerived on each of the 300 records of 0x6c, from the first up
void array_static_init(Ctx &c) {
  const U4 first = GSV(c, &GameState::g_sounds);
  for (U4 i = 0; i < (NG(arrayInit) == 1 ? 299u : 300u); ++i) ec(c, 0x84820, kThis, {first + i * 0x6cu});
}

// ---- adapters ---------------------------------------------------------------------------------------------------------------
#define AD(fn) void fn(Ctx &c, [[maybe_unused]] const uint32_t *a, [[maybe_unused]] Ret &r)
#define REQ(name, n) Req rq_(c, name, a, n)

AD(a_play_sound) {
  if (ectx(c).stub_requests) return r.set_i32(0);
  REQ("playSound", 5);
  r.set_i32(play_sound(c, static_cast<I4>(a[0]), static_cast<I4>(a[1]), static_cast<I4>(a[2]), static_cast<I4>(a[3]),
                       static_cast<I4>(a[4])));
}
AD(a_start_sound_slot) {
  if (ectx(c).stub_requests) return r.set_u32(0);
  REQ("startSoundSlot", 1); r.set_u32(start_sound_slot(c, a[0])); }
AD(a_load_sound_table) { REQ("loadSoundTable", 0); r.set_u32(load_sound_table(c)); }
AD(a_shutdown_sound_system) { REQ("shutdownSoundSystem", 0); r.set_u32(shutdown_sound_system(c)); }
AD(a_init_sound) { REQ("initSound", 2); r.set_u32(init_sound(c, a[0], a[1])); }
AD(a_shutdown_sound) { REQ("shutdownSound", 0); r.set_u32(shutdown_sound(c)); }
AD(a_sound_version_check) { REQ("soundVersionCheck", 0); r.set_u32(sound_version_check(c)); }
AD(a_free_sound_lib) { REQ("freeSoundLib", 0); r.set_u32(free_sound_lib(c)); }
AD(a_call_sound_hook74) { REQ("callSoundHook74", 2); r.set_u32(call_sound_hook74(c, a[0], a[1])); }
AD(a_call_sound_hook94) { REQ("callSoundHook94", 0); r.set_u32(call_sound_hook94(c)); }
AD(a_create_sound) { REQ("createSound4840e0", 3); r.set_u32(create_sound(c, a[0], a[1], a[2])); }
AD(a_release_sound_obj) { REQ("releaseSoundObj", 1); r.set_u32(release_sound_obj(c, a[0])); }
AD(a_clear_sound_ptrs) { REQ("clearSoundPtrs", 0); clear_sound_ptrs(c); }
AD(a_ctor_base) { REQ("Snd::ctorBase", 1); r.set_u32(snd_ctor_base(c, a[0])); }
AD(a_scalar_dtor4841c0) {
  REQ("Snd::scalarDtor4841c0", 2);
  r.set_u32(scalar_deleting(c, 0x841e0, a[0], a[1], NG(scalarDtor4841c0)));
}
AD(a_snd_dtor) { REQ("Snd::dtor", 1); r.set_u32(snd_dtor(c, a[0])); }
AD(a_set_mode) { REQ("Snd::setMode", 2); snd_set_mode(c, a[0], a[1]); }
AD(a_music_set_file) { REQ("SndMusic::setFile", 2); r.set_u32(music_set_file(c, a[0], a[1])); }
AD(a_unlink_sound) { REQ("unlinkSound", 1); r.set_u32(unlink_sound(c, a[0])); }
AD(a_is_playing) {
  REQ("Snd::isPlaying", 1);
  r.set_u32((L32(c, a[0] + 0x44) & (NG(c2) == 1 ? 2u : 1u)) ? 0xffffffffu : 0u);  // shl 0x1f; sar 0x1f
}
AD(a_link_into_list) { REQ("Snd::linkIntoList", 1); r.set_u32(link_into_list(c, a[0])); }
AD(a_get_next) { REQ("Snd::getNext", 1); r.set_u32(L32(c, a[0] + (NG(c2) == 1 ? 0x48 : 0x4c))); }
AD(a_get_prev) { REQ("Snd::getPrev", 1); r.set_u32(L32(c, a[0] + (NG(c2) == 1 ? 0x4c : 0x48))); }
AD(a_get_mode) { REQ("Snd::getMode", 1); r.set_u32(L32(c, a[0] + 0x54) + (NG(c2) == 1 ? 1u : 0u)); }
AD(a_get_length) { REQ("Snd::getLengthMs", 1); r.set_u32(L32(c, a[0] + 0x64) + (NG(c2) == 1 ? 1u : 0u)); }
AD(a_unlink_from_list) { REQ("Snd::unlinkFromList", 1); r.set_u32(unlink_from_list(c, a[0])); }
AD(a_ret_b_pop4) { (void)c; (void)a; r.set_u32(NG(c2) == 1 ? 0xcu : 0xbu); }
AD(a_ret_b) { (void)c; (void)a; r.set_u32(NG(c2) == 1 ? 0xcu : 0xbu); }
AD(a_stop484550) { REQ("Snd::stop484550", 1); r.set_u32(snd_stop484550(c, a[0])); }
AD(a_query_device) { REQ("Snd::queryDevice4845d0", 1); r.set_u32(query_device(c, a[0])); }
AD(a_status) {
  if (ectx(c).stub_requests) return r.set_u32(0);
  REQ("Snd::status", 1); r.set_u32(snd_status(c, a[0])); }
AD(a_set_field34) { REQ("Snd::setField34", 2); r.set_u32(set_field34(c, a[0], a[1])); }
AD(a_set_field38) { REQ("Snd::setField38", 2); r.set_u32(set_field38(c, a[0], a[1])); }
AD(a_base_set_field38) { REQ("SndBase::setField38", 2); r.set_u32(base_set_field38(c, a[0], a[1])); }
AD(a_set_field38_voice54) { REQ("Snd::setField38Voice54", 2); r.set_u32(set_field38_voice54(c, a[0], a[1])); }
AD(a_apply_field484750) { REQ("Snd::applyField484750", 2); r.set_u32(apply_field(c, a[0], 4, a[1], NG(applyField484750))); }
AD(a_apply_field4847b0) { REQ("Snd::applyField4847b0", 2); r.set_u32(apply_field(c, a[0], 0x54, a[1], NG(applyField4847b0))); }
AD(a_apply_field38_voice28) { REQ("Snd::applyField38Voice28", 1); r.set_u32(apply_field38_voice(c, a[0], 0x28)); }
AD(a_apply_field38_voice30) { REQ("Snd::applyField38Voice30", 1); r.set_u32(apply_field38_voice(c, a[0], 0x30)); }
AD(a_set_pan) { REQ("Snd::setPan", 2); r.set_u32(set_pan(c, a[0], static_cast<I4>(a[1]))); }
AD(a_ctor_derived) { REQ("Snd::ctorDerived", 1); r.set_u32(snd_ctor_derived(c, a[0])); }
AD(a_music_scalar_dtor) {
  REQ("SndMusic::scalarDtor", 2);
  r.set_u32(scalar_deleting(c, 0x848a0, a[0], a[1], NG(musicScalarDtor)));
}
AD(a_music_dtor) { REQ("SndMusic::dtor", 1); r.set_u32(music_dtor(c, a[0])); }
AD(a_play) { REQ("Snd::play", 1); r.set_u32(snd_play(c, a[0])); }
AD(a_configure) { REQ("Snd::configure", 3); snd_configure(c, a[0], a[1], a[2]); }
AD(a_busy) { REQ("Snd::busy", 1); r.set_u32(snd_busy(c, a[0])); }
AD(a_set_name) { REQ("Snd::setName", 2); r.set_u32(snd_set_name(c, a[0], a[1])); }
AD(a_stream_open) { REQ("SndStream::open484c20", 2); r.set_u32(stream_open(c, a[0], a[1])); }
AD(a_open) { REQ("Snd::open", 2); r.set_u32(snd_open(c, a[0], a[1])); }
AD(a_reopen) { REQ("Snd::reopen", 1); r.set_u32(snd_reopen(c, a[0])); }
AD(a_apply_mode) { REQ("Snd::applyMode484e30", 3); r.set_u32(apply_mode(c, a[0], a[1], a[2])); }
AD(a_start) { REQ("Snd::start", 1); r.set_u32(snd_start(c, a[0])); }
AD(a_stop484f00) { REQ("Snd::stop484f00", 1); r.set_u32(snd_stop484f00(c, a[0])); }
AD(a_set_pitch) { REQ("Snd::setPitch", 2); r.set_u32(set_pitch(c, a[0], static_cast<I4>(a[1]))); }
AD(a_set_flags) { REQ("Snd::setFlags", 2); r.set_u32(set_flags(c, a[0], a[1])); }
AD(a_flags) { REQ("Snd::flags", 1); r.set_u32(snd_flags(c, a[0])); }
AD(a_set_volume) { REQ("Snd::setVolume", 2); r.set_u32(set_volume(c, a[0], a[1], false)); }
AD(a_set_volume_base) { REQ("Snd::setVolumeBase", 2); r.set_u32(set_volume(c, a[0], a[1], true)); }
AD(a_set_loop) { REQ("Snd::setLoop", 2); r.set_u32(set_loop(c, a[0], a[1])); }
AD(a_set_field60) { REQ("Snd::setField60", 2); r.set_u32(set_field60(c, a[0], a[1])); }
AD(a_ctor485260) { REQ("Snd::ctor485260", 1); r.set_u32(snd_ctor485260(c, a[0])); }
AD(a_scalar_dtor4852b0) {
  REQ("Snd::scalarDtor4852b0", 2);
  r.set_u32(scalar_deleting(c, 0x852d0, a[0], a[1], NG(scalarDtor4852b0)));
}
AD(a_set_vtbl) { REQ("Snd::setVtbl4852d0", 1); snd_set_vtbl(c, a[0]); }
AD(a_voice_rx_ctor) { REQ("VoiceRx::ctor", 1); r.set_u32(voice_rx_ctor(c, a[0])); }
AD(a_voice_rx_start) { REQ("VoiceRx::start", 4); r.set_u32(voice_rx_start(c, a[0], a[1], a[2], a[3])); }
AD(a_open_hook_a) { REQ("openHookA", 2); r.set_u32(open_hook(c, a[0], a[1], 4, NG(openHookA))); }
AD(a_open_hook_b) { REQ("openHookB", 2); r.set_u32(open_hook(c, a[0], a[1], 6, NG(openHookB))); }
AD(a_open_hook_c) { REQ("openHookC", 2); r.set_u32(open_hook(c, a[0], a[1], 8, NG(openHookC))); }
AD(a_close_hook_a) { REQ("closeHookA", 1); r.set_u32(close_hook(c, a[0], 5, NG(closeHookA))); }
AD(a_close_hook_b) { REQ("closeHookB", 1); r.set_u32(close_hook(c, a[0], 7, NG(closeHookB))); }
AD(a_close_hook_c) { REQ("closeHookC", 1); r.set_u32(close_hook(c, a[0], 9, NG(closeHookC))); }
AD(a_release_a) { REQ("releaseWithHookA", 1); r.set_u32(release_with_hook(c, a[0], 0x87430, NG(releaseA))); }
AD(a_release_b) { REQ("releaseWithHookB", 1); r.set_u32(release_with_hook(c, a[0], 0x879f0, NG(releaseB))); }
AD(a_release_c) { REQ("releaseWithHookC", 1); r.set_u32(release_with_hook(c, a[0], 0, NG(releaseC))); }
AD(a_resource_load) { REQ("resourceLoad", 3); r.set_u32(resource_load(c, a[0], a[1], a[2], false, NG(resourceLoad))); }
AD(a_resource_load_b) { REQ("resourceLoadB", 3); r.set_u32(resource_load(c, a[0], a[1], a[2], true, NG(resourceLoadB))); }
AD(a_resource_query) { REQ("resourceQuery", 1); r.set_u32(resource_query(c, a[0])); }
AD(a_is_disabled) { REQ("SndGroup_isDisabled", 2); r.set_u32(group_flag(c, a[0], a[1], 0x24, true, NG(isDisabled))); }
AD(a_is_muted) { REQ("SndGroup_isMuted", 2); r.set_u32(group_flag(c, a[0], a[1], 0x25, false, NG(isMuted))); }
AD(a_hash_remove) { REQ("HashTable::remove", 2); r.set_u32(hash_remove(c, a[0], a[1])); }
AD(a_show_message_box) {
  REQ("showMessageBox", 2);
  r.set_u32(exe_import(c, exe_rva::iat_MessageBoxA, SndAbi::Stdcall, {0, a[0], a[1], NG(showMessageBox) == 1 ? 1u : 0u}).eax);
}
AD(a_play_child) { REQ("NodeList::playChildSuspendingSounds", 2); r.set_u32(play_child_suspending(c, a[0], a[1])); }
AD(a_g_sounds_dtor) { REQ("gSoundsDtor_448190", 0); g_sounds_dtor(c); }
AD(a_array_static_init) { REQ("arrayStaticInit_448160", 0); array_static_init(c); }
// gMusicDeviceDtor / gSfxDeviceDtor / gVoiceDeviceDtor: tail jumps into NodeList::dtor / NodeListB::dtor / NodeListC::dtor on
// the holder (other modules' functions: dispatched by name)
AD(a_music_device_dtor) { REQ("gMusicDeviceDtor_483e00", 0); r = exe_call_va(c, ex(c, 0x87390), kThis, {music_device(c) + (NG(deviceDtors) == 1 ? 4u : 0u)}); }
AD(a_sfx_device_dtor) {
  REQ("gSfxDeviceDtor_483e40", 0);
  r = exe_call_va(c, ex(c, 0x87a60), kThis, {GSV(c, &GameState::g_sfxDevice) + (NG(deviceDtors) == 1 ? 4u : 0u)});
}
AD(a_voice_device_dtor) {
  REQ("gVoiceDeviceDtor_483e80", 0);
  r = exe_call_va(c, ex(c, 0x87b80), kThis, {GSV(c, &GameState::g_voiceDevice) + (NG(deviceDtors) == 1 ? 4u : 0u)});
}

// The device forwarders (C2): a NULL device (+0x40) returns `null_eax`; else the device's slot with the stack arguments
// (a tail jump or a call whose eax is left; fwdVoiceF0 returns 0 after its call). `keep` = the null path leaves the caller's
// eax (no write): the port returns 0 there (U-1496).
struct Fwd {
  const char *name;
  U4 slot;
  int nargs;     // stack arguments forwarded
  U4 null_eax;  // eax on the NULL path
  bool keep;     // the NULL path writes no eax
  bool zero_after;  // the result is 0 after the call (fwdVoiceF0)
  bool al_only;     // the NULL path clears only al (fwdDev58)
};
const Fwd kFwds[] = {
    {"Snd::fwdVoice1c", 0x1c, 0, 0x14, false, false, false}, {"Snd::fwdVoice18", 0x18, 1, 0x14, false, false, false},
    {"Snd::fwdVoice38", 0x38, 0, 0x14, false, false, false}, {"Snd::fwdVoice34", 0x34, 3, 0, true, false, false},
    {"Snd::fwdVoice58", 0x58, 0, 0, false, false, false},    {"Snd::fwdVoice74", 0x74, 0, 0, false, false, false},
    {"Snd::fwdVoice60", 0x60, 0, 0, false, false, false},    {"Snd::fwdVoice98", 0x98, 1, 0x14, false, false, false},
    {"Snd::fwdVoice94", 0x94, 1, 0x14, false, false, false}, {"Snd::fwdVoiceD0", 0xd0, 1, 0, true, false, false},
    {"Snd::fwdVoiceD4", 0xd4, 1, 0, true, false, false},     {"Snd::fwdVoiceCC", 0xcc, 0, 0, false, false, false},
    {"Snd::fwdVoiceF0", 0xf0, 2, 0x13, false, true, false},  {"Snd::fwdVoiceB8", 0xb8, 1, 0, false, false, false},
    {"Snd::fwdVoiceBC", 0xbc, 0, 0xffffffffu, false, false, false}, {"Snd::fwdVoice3c", 0x3c, 0, 0, false, false, false},
    {"Snd::fwdDev48", 0xc0, 0, 0, false, false, false},      {"Snd::fwdDev54", 0xd8, 3, 0x14, false, false, false},
    {"Snd::fwdDev55", 0xdc, 1, 0x14, false, false, false},   {"Snd::fwdDev56", 0xe0, 1, 0x14, false, false, false},
    {"Snd::fwdDev57", 0xe4, 1, 0x14, false, false, false},   {"Snd::fwdDev58", 0xe8, 0, 0, false, false, true},
    {"VoiceRx::fwdDev37", 0x94, 0, 0, false, false, false},  {"VoiceRx::callDev34", 0x88, 0, 0, false, true, false},
    {"VoiceRx::callDev8", 0x20, 0, 0, false, true, false},   {"VoiceRx::fwdDev5", 0x14, 0, 0x13, false, false, false},
    {"VoiceRx::fwdDev35", 0x8c, 1, 0x13, false, false, false}, {"VoiceRx::fwdDev36", 0x90, 0, 0, false, false, false},
};
template <int K> void a_fwd(Ctx &c, const uint32_t *a, Ret &r) {
  const Fwd &f = kFwds[K];
  Req rq_(c, f.name, a, 1 + f.nargs);
  const U4 dev = L32(c, a[0] + 0x40);
  if (dev == 0) {
    r.set_u32((f.keep ? 0u : f.null_eax) + (NG(c2) == 1 ? 1u : 0u));
    return;
  }
  U4 args[4] = {dev, 0, 0, 0};
  for (int i = 0; i < f.nargs; ++i) args[1 + i] = a[1 + i];
  const Ret x = exe_call_va(c, L32(c, L32(c, dev) + f.slot), kThis, args, 1 + f.nargs);
  r.set_u32((f.zero_after ? 0u : x.eax) + (NG(c2) == 1 ? 1u : 0u));
  r.edx = x.edx;
}
// Snd::fwdDev49(a, b, d) (C2): a device forwards to its slot 0xc4; with none, a non-zero d stores a NUL at a, eax 1
void a_fwd_dev49(Ctx &c, const uint32_t *a, Ret &r) {
  Req rq_(c, "Snd::fwdDev49", a, 4);
  const U4 dev = L32(c, a[0] + 0x40);
  if (dev != 0) {
    r = exe_call_va(c, L32(c, L32(c, dev) + 0xc4), kThis, {dev, a[1], a[2], a[3]});
    return;
  }
  if (a[3] != 0) S<uint8_t>(c, a[1], 0);
  r.set_u32(NG(c2) == 1 ? 2u : 1u);
}

// ---- the device sink (gate c): a test AudioSink that answers the system as a device-present machine would -----------------
// Every command is logged as RecordingSink logs it (one text line), and answered:
//   KERNEL32 heap (HeapCreate / HeapAlloc / HeapReAlloc / HeapSize / HeapFree / VirtualAlloc ...) from an arena mapped in the
//     context (U-1411: sound.dll's CRT runs as ported down to these imports); critical sections, events, waits, TLS, thread
//     creation (the thread is recorded, never run: the test drives the timer client itself), version and module queries;
//   WINMM: timeGetTime / GetTickCount = a virtual clock (+1 ms per read, +10 ms per driven tick), timeSetEvent records the
//     timer client (the test calls it), timeGetDevCaps, MIDI out opens a handle, wave-in reports no device; the mmio family
//     reads the user's files through GameFiles (U-1494): names as the caller passes them, relative to the install root,
//     plain and buffered reads (mmioGetInfo / mmioAdvance / mmioSetInfo), seeks, RIFF descend / ascend, memory files ('MEM ');
//   DSOUND / ole32: DirectSoundCreate and CoCreateInstance hand out an IDirectSound object, whose CreateSoundBuffer hands out
//     IDirectSoundBuffer objects with storage in the context (Lock gives pointers into it, Unlock logs a digest of the bytes
//     the engine wrote, GetCurrentPosition advances with the clock), QueryInterface for a 3D interface answers
//     E_NOINTERFACE;
//   USER32: a window class and window handle, no messages (PeekMessageA 0), message boxes answered IDOK.
// Anything else answers 0 and is logged as `unanswered`.
class DeviceSink : public DeviceModel {
 public:
  enum Iface : U4 { kDs = 1, kDsBuf = 2 };
  struct Buf {
    U4 storage = 0, size = 0, play = 0, status = 0;
    U4 freq = 22050, vol = 0, pan = 0;
  };
  struct MmFile {
    std::vector<uint8_t> bytes;
    bool mem = false;
    U4 mem_va = 0;
    U4 pos = 0;
    bool buffered = false;
    U4 buf = 0, cch = 0, buf_off = 0, next = 0, end_read = 0, disk = 0;
    U4 open_flags = 0;
    bool own = false;   // the buffer was allocated by mmio (MMIO_ALLOCBUF in dwFlags)
    U4 adw0 = 0;        // a memory file's expansion increment (adwInfo[0])
  };
  U4 used = 0;
  std::map<U4, U4> sizes;
  std::multimap<U4, U4> freed;   // rounded size -> freed block (HeapFree), reused by an allocation of that size
  U4 next_handle = 0x1000;   // clock, timer_cb / _user / _id / _period, unanswered: DeviceModel (audio_exe.h)
  std::map<U4, Buf> bufs;
  std::map<U4, MmFile> mm;
  std::map<U4, U4> tls;
  U4 tls_next = 1;
  U4 ds_obj = 0;
  std::string install_dir;

  // the heap: 16 MB chunks mapped on demand from kBase up, at most kChunks (128 MB, below the exe arena; U-1411); a freed
  // block is reused by the next allocation of its rounded size
  static constexpr U4 kBase = 0xF4000000u, kChunk = 0x01000000u, kChunks = 8;
  std::vector<std::vector<uint8_t>> chunks;

  U4 alloc(Ctx &c, U4 n, bool zero = true) {
    if (!chunks.empty() && !mapped(c, kBase, 4)) {   // a new GameState / address space: start over
      chunks.clear();
      freed.clear();
      sizes.clear();
    }
    const U4 need = (std::max<U4>(n, 1) + 15u) & ~15u;
    if (need > kChunk) throw MissingResource{"device sink block over 16 MB"};
    U4 p;
    if (auto it = freed.find(need); it != freed.end()) {
      p = it->second;
      freed.erase(it);
    } else {
      if (chunks.empty() || used + need > static_cast<U4>(chunks.size()) * kChunk) {   // the block opens a new chunk
        if (chunks.size() >= kChunks) throw MissingResource{"device sink arena full"};
        chunks.emplace_back(static_cast<size_t>(kChunk), uint8_t{0});
        const U4 b = kBase + static_cast<U4>(chunks.size() - 1) * kChunk;
        if (!c.mem.map(b, chunks.back().data(), kChunk, false)) throw MissingResource{"device sink arena"};
        used = static_cast<U4>(chunks.size() - 1) * kChunk;
      }
      p = kBase + used;
      used += need;
    }
    sizes[p] = n;
    if (zero) {
      const U4 off = p - kBase;
      std::memset(chunks[off / kChunk].data() + off % kChunk, 0, need);
    }
    return p;
  }
  U4 obj(Ctx &c, U4 iface, U4 nslots) { return audio_sink_object(c, iface, nslots); }
  DeviceHeapUsage heap_usage() const override {  // S7.11 (audio_exe.h)
    DeviceHeapUsage u;
    u.arena_bytes = chunks.empty() ? 0u : used;
    u.chunks = static_cast<uint32_t>(chunks.size());
    u.live_blocks = static_cast<uint32_t>(sizes.size());
    u.freed_blocks = static_cast<uint32_t>(freed.size());
    for (const auto &kv : sizes) u.live_bytes += kv.second;
    return u;
  }

  AudioReply ok(U4 v) {
    AudioReply r;
    r.eax = v;
    return r;
  }
  std::string cstr(Ctx &c, U4 va) {
    std::string s;
    for (char ch; va && (ch = static_cast<char>(L<uint8_t>(c, va))) != 0 && s.size() < 300; ++va) s += ch;
    return s;
  }
  // a file name as the engine passes it, as a GameFiles name (relative to the install root)
  std::string game_name(const std::string &n) {
    std::string s = n;
    for (char &ch : s)
      if (ch == '\\') ch = '/';
    while (s.rfind("./", 0) == 0) s = s.substr(2);
    return s;
  }

  // ---- mmio ----
  U4 mm_pos(const MmFile &f) const { return f.mem ? f.next : (f.buffered ? f.buf_off + f.next : f.pos); }
  int mm_raw_read(Ctx &c, MmFile &f, U4 at, U4 dst, U4 n) {
    const U4 size = static_cast<U4>(f.bytes.size());
    if (at >= size) return 0;
    const U4 k = std::min(n, size - at);
    for (U4 i = 0; i < k; ++i) S<uint8_t>(c, dst + i, f.bytes[at + i]);
    return static_cast<int>(k);
  }
  void mm_refill(Ctx &c, MmFile &f, U4 at) {
    f.buf_off = at;
    f.next = 0;
    f.end_read = static_cast<U4>(mm_raw_read(c, f, at, f.buf, f.cch));
    f.disk = at + f.end_read;
  }
  int mm_read(Ctx &c, MmFile &f, U4 dst, U4 n) {
    if (f.mem) {   // the buffer is the file: reads stop at pchEndRead
      const U4 k = f.next < f.end_read ? std::min(n, f.end_read - f.next) : 0;
      for (U4 i = 0; i < k; ++i) S<uint8_t>(c, dst + i, L<uint8_t>(c, f.buf + f.next + i));
      f.next += k;
      return static_cast<int>(k);
    }
    if (!f.buffered) {
      const int k = mm_raw_read(c, f, f.pos, dst, n);
      f.pos += static_cast<U4>(k);
      f.disk = f.pos;
      return k;
    }
    U4 done = 0;
    while (done < n) {
      const U4 avail = f.end_read - f.next;
      if (avail) {
        const U4 k = std::min(avail, n - done);
        for (U4 i = 0; i < k; ++i) S<uint8_t>(c, dst + done + i, L<uint8_t>(c, f.buf + f.next + i));
        f.next += k;
        done += k;
        continue;
      }
      const U4 at = f.buf_off + f.next;
      if (n - done > f.cch) {          // larger than the buffer: straight into the caller, the buffer empty there
        const int k = mm_raw_read(c, f, at, dst + done, n - done);
        done += static_cast<U4>(k);
        f.buf_off = at + static_cast<U4>(k);
        f.next = f.end_read = 0;
        f.disk = f.buf_off;
        break;
      }
      mm_refill(c, f, at);
      if (f.end_read == 0) break;
    }
    return static_cast<int>(done);
  }
  bool mm_seek(MmFile &f, U4 to) {
    if (f.mem) {   // a memory file seeks inside [0, pchEndRead - pchBuffer] only
      if (to > f.end_read) return false;
      f.next = to;
      return true;
    }
    if (!f.buffered) {
      f.pos = to;
      return true;
    }
    if (to >= f.buf_off && to <= f.buf_off + f.end_read) {
      f.next = to - f.buf_off;
      return true;
    }
    f.buf_off = to;
    f.next = f.end_read = 0;
    return true;
  }
  void mm_info_out(Ctx &c, U4 h, MmFile &f, U4 info) {
    S32(c, info + 0, f.open_flags | (f.own ? 0x10000u : 0u));
    S32(c, info + 4, f.mem ? 0x204d454du : 0x20534f44u);   // 'MEM ' / 'DOS '
    S32(c, info + 20, f.buffered ? f.cch : 0);
    S32(c, info + 24, f.buffered ? f.buf : 0);
    S32(c, info + 28, f.buffered ? f.buf + f.next : 0);
    S32(c, info + 32, f.buffered ? f.buf + f.end_read : 0);
    S32(c, info + 36, f.buffered ? f.buf + f.cch : 0);
    S32(c, info + 40, f.buffered ? f.buf_off : f.pos);
    S32(c, info + 44, f.disk);
    S32(c, info + 48, f.mem ? f.adw0 : 0);
    S32(c, info + 68, h);
  }
  AudioReply mmio(Ctx &c, const std::string &name, const std::vector<U4> &a) {
    auto file = [&](U4 h) -> MmFile * {
      auto it = mm.find(h);
      return it == mm.end() ? nullptr : &it->second;
    };
    if (name == "mmioOpenA") {
      MmFile f;
      const U4 info = a[1], flags = a[2];
      f.open_flags = flags & 0xffffu;
      if (info && L32(c, info + 4) == 0x204d454du) {
        // 'MEM ' (re/frida/js/mmiostandin.js, measured against winmm): the buffer is the file and always buffered; pchBuffer
        // NULL makes mmio allocate cchBuffer bytes (MMIO_ALLOCBUF added); MMIO_CREATE starts it empty (pchEndRead =
        // pchBuffer), otherwise it holds cchBuffer bytes
        f.mem = f.buffered = true;
        f.cch = L32(c, info + 20);
        f.buf = L32(c, info + 24);
        if (f.buf == 0) {
          f.buf = alloc(c, f.cch);
          f.own = true;
        }
        f.mem_va = f.buf;
        f.end_read = (flags & 0x1000u) ? 0 : f.cch;
        f.adw0 = L32(c, info + 48);
        S32(c, info + 12, 0);
        const U4 h = next_handle++;
        mm[h] = std::move(f);
        return ok(h);
      } else {
        const std::string x = game_name(cstr(c, a[0]));
        const std::string n = golf_sfx_name(c.files(), x);  // S7.12: sound.golfSfxAlias (x itself at switch 1)
        lines.back() += "  ; file " + x + (n != x ? " (alias " + n + ")" : "") + (c.files().exists(n) ? "" : " (absent)");
        ++(c.files().exists(n) ? file_present : file_absent);
        if (n != x) ++file_aliased;
        if (n.empty() || !c.files().exists(n) || (flags & 0x1003u)) {  // a missing file, or write / create (not modelled)
          if (info) S32(c, info + 12, 257);                    // MMIOERR_CANNOTOPEN in wErrorRet
          return ok(0);
        }
        f.bytes = c.files().read_all(n);
      }
      if (flags & 0x10000u) {                                   // MMIO_ALLOCBUF
        f.buffered = f.own = true;
        f.cch = (info && L32(c, info + 20)) ? L32(c, info + 20) : 8192;
        f.buf = alloc(c, f.cch);
      }
      const U4 h = next_handle++;
      mm[h] = std::move(f);
      return ok(h);
    }
    MmFile *f = a.empty() ? nullptr : file(a[0]);
    if (name == "mmioStringToFOURCCA") {
      const std::string s = cstr(c, a[0]);
      U4 v = 0;
      for (size_t i = 0; i < 4; ++i) {
        unsigned char ch = i < s.size() ? static_cast<unsigned char>(s[i]) : ' ';
        if (a[1] & 0x10) ch = static_cast<unsigned char>(std::toupper(ch));
        v |= static_cast<U4>(ch) << (8 * i);
      }
      return ok(v);
    }
    if (f == nullptr) return ok(name == "mmioRead" || name == "mmioSeek" ? 0xffffffffu : 5u);  // MMSYSERR_INVALHANDLE
    if (name == "mmioClose") {
      mm.erase(a[0]);
      return ok(0);
    }
    if (name == "mmioRead") return ok(static_cast<U4>(mm_read(c, *f, a[1], a[2])));
    if (name == "mmioWrite") return ok(0xffffffffu);
    if (name == "mmioSeek") {
      const I4 off = static_cast<I4>(a[1]);
      const U4 size = static_cast<U4>(f->bytes.size());
      if (f->mem) {   // SEEK_END on a memory file is size - lOffset
        const U4 sz = f->end_read;
        const U4 to = a[2] == 1 ? static_cast<U4>(static_cast<I4>(f->next) + off) : (a[2] == 2 ? sz - off : static_cast<U4>(off));
        return ok(mm_seek(*f, to) ? to : 0xffffffffu);
      }
      U4 to = a[2] == 1 ? static_cast<U4>(static_cast<I4>(mm_pos(*f)) + off) : (a[2] == 2 ? size + off : static_cast<U4>(off));
      mm_seek(*f, to);
      return ok(to);
    }
    if (name == "mmioDescend") {
      const U4 ck = a[1], parent = a[2], flags = a[3];
      S32(c, ck + 16, 0);
      if (f->mem) {   // a memory file's readable bytes, for the chunk walk
        f->bytes.resize(f->end_read);
        for (U4 i = 0; i < f->end_read; ++i) f->bytes[i] = L<uint8_t>(c, f->buf + i);
      }
      const U4 want_id = L32(c, ck), want_type = L32(c, ck + 8);
      for (;;) {
        const U4 at = mm_pos(*f);
        if (parent && (at < L32(c, parent + 12) || at >= L32(c, parent + 12) + L32(c, parent + 4))) return ok(265);
        uint8_t hdr[8];
        if (at + 8 > f->bytes.size()) return ok(265);
        std::memcpy(hdr, f->bytes.data() + at, 8);
        U4 id, sz;
        std::memcpy(&id, hdr, 4);
        std::memcpy(&sz, hdr + 4, 4);
        mm_seek(*f, at + 8);
        S32(c, ck, id);
        const U4 data = at + 8;
        const U4 size = static_cast<U4>(f->bytes.size());
        if (data + sz > size) sz = size - data;
        S32(c, ck + 4, sz);
        S32(c, ck + 12, data);
        U4 type = 0;
        if (id == 0x46464952u || id == 0x5453494cu) {           // RIFF / LIST
          if (data + 4 <= size) std::memcpy(&type, f->bytes.data() + data, 4);
          mm_seek(*f, data + 4);
        }
        S32(c, ck + 8, type);
        bool match = true;
        if (flags & 0x10) match = id == want_id;                                      // MMIO_FINDCHUNK
        else if (flags & 0x20) match = id == 0x46464952u && type == want_type;        // MMIO_FINDRIFF
        else if (flags & 0x40) match = id == 0x5453494cu && type == want_type;        // MMIO_FINDLIST
        if (match) return ok(0);
        mm_seek(*f, data + ((sz + 1) & ~1u));
      }
    }
    if (name == "mmioAscend") {
      mm_seek(*f, L32(c, a[1] + 12) + ((L32(c, a[1] + 4) + 1) & ~1u));
      return ok(0);
    }
    if (name == "mmioGetInfo") {
      mm_info_out(c, a[0], *f, a[1]);
      return ok(0);
    }
    if (name == "mmioSetInfo") {
      if (f->buffered) {
        f->next = L32(c, a[1] + 28) - f->buf;
        f->end_read = L32(c, a[1] + 32) - f->buf;
      }
      return ok(0);
    }
    if (name == "mmioAdvance") {
      if (!f->buffered) return ok(266);                         // MMIOERR_UNBUFFERED
      if (f->mem) return ok(a[2] == 0 ? 261u : 262u);           // MMIOERR_CANNOTREAD (a write advance is not modelled)
      const U4 at = L32(c, a[1] + 40) + (L32(c, a[1] + 28) - L32(c, a[1] + 24));
      mm_refill(c, *f, at);
      mm_info_out(c, a[0], *f, a[1]);
      return ok(0);
    }
    if (name == "mmioSetBuffer") {
      if (a[2] == 0) {
        if (f->buffered) f->pos = f->buf_off + f->next;
        f->buffered = false;
      } else {
        const U4 at = mm_pos(*f);
        f->buffered = true;
        f->cch = a[2];
        f->buf = a[1] ? a[1] : alloc(c, a[2]);
        f->buf_off = at;
        f->next = f->end_read = 0;
      }
      return ok(0);
    }
    ++unanswered;
    return ok(0);
  }

  AudioReply import(Ctx &c, const AudioCommand &cmd) override {
    RecordingSink::import(c, cmd);
    const std::string &n = cmd.name;
    const std::vector<U4> &a = cmd.args;
    auto arg = [&](size_t i) { return i < a.size() ? a[i] : 0u; };
    if (cmd.dll == "golf_clean.exe" || cmd.dll == "portsim") return ok(0);   // the stream's own records
    // KERNEL32
    if (n == "HeapCreate" || n == "GetProcessHeap") return ok(0x00150000u);
    if (n == "HeapAlloc") return ok(alloc(c, arg(2)));
    if (n == "HeapReAlloc") {
      const U4 p = alloc(c, arg(3));
      const U4 old = arg(2);
      const U4 k = std::min(sizes.count(old) ? sizes[old] : 0u, arg(3));
      for (U4 i = 0; i < k; ++i) S<uint8_t>(c, p + i, L<uint8_t>(c, old + i));
      return ok(p);
    }
    if (n == "HeapSize") return ok(sizes.count(arg(2)) ? sizes[arg(2)] : 0xffffffffu);
    if (n == "HeapFree" || n == "VirtualFree") {
      const U4 p = n == "HeapFree" ? arg(2) : arg(0);
      if (auto it = sizes.find(p); it != sizes.end()) {
        freed.emplace((std::max<U4>(it->second, 1) + 15u) & ~15u, p);
        sizes.erase(it);
      }
      return ok(1);
    }
    if (n == "HeapDestroy" || n == "VirtualFree" || n == "CloseHandle" || n == "SetEvent" ||
        n == "ResetEvent" || n == "SetThreadPriority" || n == "TlsFree" || n == "FlushFileBuffers")
      return ok(1);
    if (n == "VirtualAlloc") return ok(arg(0) ? arg(0) : alloc(c, (arg(1) + 0xfffu) & ~0xfffu));
    if (n == "InitializeCriticalSection" || n == "EnterCriticalSection" || n == "LeaveCriticalSection" ||
        n == "DeleteCriticalSection" || n == "Sleep" || n == "OutputDebugStringA" || n == "SetLastError" ||
        n == "GetLastError" || n == "ResumeThread" || n == "SetHandleCount")
      return ok(0);
    if (n == "CreateEventA" || n == "GetCurrentThread" || n == "GetCurrentProcess") return ok(next_handle++);
    if (n == "CreateThread") {
      if (arg(5)) S32(c, arg(5), 0x2000u + next_handle);
      return ok(next_handle++);
    }
    if (n == "GetCurrentThreadId") return ok(0x0d44u);
    if (n == "WaitForSingleObject") return ok(0);
    if (n == "TlsAlloc") return ok(tls_next++);
    if (n == "TlsGetValue") return ok(tls.count(arg(0)) ? tls[arg(0)] : 0u);
    if (n == "TlsSetValue") {
      tls[arg(0)] = arg(1);
      return ok(1);
    }
    if (n == "InterlockedIncrement" || n == "InterlockedDecrement") {
      const U4 v = L32(c, arg(0)) + (n == "InterlockedIncrement" ? 1u : 0xffffffffu);
      S32(c, arg(0), v);
      return ok(v);
    }
    if (n == "IsBadReadPtr" || n == "IsBadWritePtr" || n == "IsBadCodePtr") return ok(0);
    if (n == "GetVersion") return ok(0x0a280105u);
    if (n == "GetTickCount") return ok(clock += 1);
    if (n == "GetModuleHandleA" || n == "LoadLibraryA" || n == "GetProcAddress") return ok(0);
    // WINMM
    if (n == "timeGetTime") return ok(clock += 1);
    if (n == "timeBeginPeriod" || n == "timeEndPeriod" || n == "timeKillEvent" || n == "midiOutShortMsg" ||
        n == "midiOutClose")
      return ok(0);
    if (n == "timeGetDevCaps") {
      if (arg(0)) {
        S32(c, arg(0), 1);
        S32(c, arg(0) + 4, 1000000);
      }
      return ok(0);
    }
    if (n == "timeSetEvent") {
      timer_cb = arg(2);
      timer_user = arg(3);
      timer_period = arg(0);
      timer_id = 0x77;
      return ok(timer_id);
    }
    if (n == "midiOutOpen") {
      if (arg(0)) S32(c, arg(0), next_handle++);
      return ok(0);
    }
    if (n == "waveInGetNumDevs") return ok(0);
    if (n.rfind("waveIn", 0) == 0) return ok(2);              // MMSYSERR_BADDEVICEID
    if (n.rfind("mmio", 0) == 0) return mmio(c, n, a);
    // DSOUND (by ordinal: #1 DirectSoundCreate) and ole32
    if (cmd.dll == "DSOUND.dll" && n == "#1") {
      if (arg(1)) S32(c, arg(1), ds(c));
      return ok(0);
    }
    if (cmd.dll == "DSOUND.dll" && n == "#2") {               // DirectSoundEnumerateA(callback, context): one device,
      const U4 desc = alloc(c, 32), mod = alloc(c, 4);         // the primary driver (GUID NULL), as Windows lists it first
      const char *d = "Primary Sound Driver";
      for (U4 i = 0; d[i]; ++i) S<uint8_t>(c, desc + i, static_cast<uint8_t>(d[i]));
      snd_call_va(c, arg(0), SndAbi::Stdcall, {0, desc, mod, arg(1)});
      return ok(0);
    }
    if (cmd.dll == "DSOUND.dll") return ok(0);
    if (n == "CoInitialize" || n == "CoUninitialize") return ok(0);
    if (n == "CoCreateInstance") {
      if (arg(4)) S32(c, arg(4), ds(c));
      return ok(0);
    }
    // USER32
    if (n == "RegisterClassA") return ok(0xc001u);
    if (n == "CreateWindowExA") return ok(0x00020002u);
    if (n == "PeekMessageA" || n == "DefWindowProcA" || n == "CallWindowProcA" || n == "SetWindowLongA") return ok(0);
    if (n == "PostMessageA" || n == "PostThreadMessageA") return ok(1);
    if (n == "MsgWaitForMultipleObjects") return ok(0x102u);  // WAIT_TIMEOUT
    if (n == "MessageBoxA") return ok(1);                     // IDOK
    ++unanswered;
    lines.back() += "  ; unanswered";
    return ok(0);
  }

  U4 ds(Ctx &c) {
    if (!ds_obj) ds_obj = obj(c, kDs, 16);
    return ds_obj;
  }

  AudioReply com(Ctx &c, const AudioCommand &cmd) override {
    RecordingSink::com(c, cmd);
    const std::vector<U4> &a = cmd.args;
    auto arg = [&](size_t i) { return i < a.size() ? a[i] : 0u; };
    if (cmd.iface == kDs) {
      switch (cmd.slot) {
        case 0: return ok(0x80004002u);                      // QueryInterface: E_NOINTERFACE
        case 1: case 2: return ok(1);                        // AddRef / Release
        case 3: {                                             // CreateSoundBuffer(desc, out, unk)
          const U4 desc = arg(1);
          Buf b;
          b.size = desc ? L32(c, desc + 8) : 0;
          if (b.size == 0) b.size = 0x10000;                  // the primary buffer: a size of our own
          b.storage = alloc(c, b.size);
          const U4 o = obj(c, kDsBuf, 24);
          bufs[o] = b;
          if (arg(2)) S32(c, arg(2), o);
          return ok(0);
        }
        case 4:                                               // GetCaps(caps): dwSize kept, the rest 0
          return ok(0);
        default: return ok(0);
      }
    }
    if (cmd.iface == kDsBuf) {
      Buf &b = bufs[arg(0)];
      switch (cmd.slot) {
        case 0: return ok(0x80004002u);
        case 1: case 2: return ok(1);
        case 3:                                               // GetCaps
          if (arg(1)) S32(c, arg(1) + 12, b.size);
          return ok(0);
        case 4:                                               // GetCurrentPosition(play*, write*)
          b.play = (b.play + 0x400u) % std::max<U4>(b.size, 1);
          if (arg(1)) S32(c, arg(1), b.play);
          if (arg(2)) S32(c, arg(2), (b.play + 0x400u) % std::max<U4>(b.size, 1));
          return ok(0);
        case 6: if (arg(1)) S32(c, arg(1), b.vol); return ok(0);
        case 7: if (arg(1)) S32(c, arg(1), b.pan); return ok(0);
        case 8: if (arg(1)) S32(c, arg(1), b.freq); return ok(0);
        case 9: if (arg(1)) S32(c, arg(1), b.status); return ok(0);
        case 11: {                                            // Lock(off, bytes, p1*, n1*, p2*, n2*, flags)
          U4 off = arg(1) % std::max<U4>(b.size, 1), n = arg(2);
          if (arg(7) & 2) { off = 0; n = b.size; }            // DSBLOCK_ENTIREBUFFER
          if (n > b.size) n = b.size;
          const U4 n1 = std::min(n, b.size - off);
          if (arg(3)) S32(c, arg(3), b.storage + off);
          if (arg(4)) S32(c, arg(4), n1);
          if (arg(5)) S32(c, arg(5), n1 < n ? b.storage : 0u);
          if (arg(6)) S32(c, arg(6), n - n1);
          return ok(0);
        }
        case 12: b.status = 1 | (arg(3) & 1 ? 4u : 0u); return ok(0);   // Play
        case 13: b.play = arg(1); return ok(0);
        case 15: b.vol = arg(1); return ok(0);
        case 16: b.pan = arg(1); return ok(0);
        case 17: b.freq = arg(1); return ok(0);
        case 18: b.status = 0; return ok(0);                  // Stop
        case 19: {                                            // Unlock(p1, n1, p2, n2): a digest of the bytes written
          uint64_t h = 1469598103934665603ull;
          U4 nz = 0;
          for (int part = 0; part < 2; ++part) {
            const U4 p = arg(1 + 2 * part), n = arg(2 + 2 * part);
            for (U4 i = 0; p && i < n; ++i) {
              const uint8_t v = L<uint8_t>(c, p + i);
              nz += v != 0 && v != 0x80;
              h = (h ^ v) * 1099511628211ull;
            }
          }
          char t[96];
          std::snprintf(t, sizeof t, "  ; unlock %u+%u bytes, %u not silent, fnv %016llx", arg(2), arg(4), nz,
                        static_cast<unsigned long long>(h));
          lines.back() += t;
          return ok(0);
        }
        default: return ok(0);
      }
    }
    ++unanswered;
    lines.back() += "  ; unanswered";
    return ok(0);
  }
};

// the contexts' DeviceSinks (deviceFixture(1)) and the external sinks' models (audio_exe_device_fixture); S7.10: one lock,
// since the app runs a shadow audio context on its own thread beside the sim's
std::mutex g_dev_mu;
std::map<const Ctx *, std::unique_ptr<DeviceSink>> &device_sinks() {
  static std::map<const Ctx *, std::unique_ptr<DeviceSink>> m;
  return m;
}
std::map<const Ctx *, DeviceModel *> &external_models() {
  static std::map<const Ctx *, DeviceModel *> m;
  return m;
}
// the context's model: its DeviceSink, else the external sink's model, else nullptr
DeviceModel *device_model(const Ctx &c) {
  std::lock_guard<std::mutex> lk(g_dev_mu);
  if (auto it = device_sinks().find(&c); it != device_sinks().end()) return it->second.get();
  if (auto it = external_models().find(&c); it != external_models().end()) return it->second;
  return nullptr;
}

// deviceFixture's preparation of a segment start image (see a_device_fixture)
void device_prepare(Ctx &c) {
  S32(c, GSV(c, &GameState::g_soundDll), 0);
  S32(c, GSV(c, &GameState::g_soundReady), 0);
  for (int i = 0; i < 11; ++i) S32(c, GSV(c, &GameState::g_soundFns) + 4u * static_cast<U4>(i), 0);
  S32(c, GSV(c, &GameState::g_soundHead), 0);
  S32(c, GSV(c, &GameState::g_soundTail), 0);
  for (U4 h : {GSV(c, &GameState::g_sfxDevice), music_device(c), GSV(c, &GameState::g_voiceDevice)}) S32(c, h + 0x14, 0);
  const U4 s0 = GSV(c, &GameState::g_sounds);
  for (U4 i = 0; i < 300; ++i) {
    const U4 s = s0 + i * 0x6cu;
    for (U4 off : {0x40u, 0x48u, 0x4cu, 0x50u, 0x68u}) S32(c, s + off, 0);
    S32(c, s + 0x44, L32(c, s + 0x44) & ~0x203u);
  }
}

// audio_exe.deviceFixture(device): the context's GameState (a segment start image, the exe's device-less state) prepared for a
// device-present start: no sound library, no sound function, empty device holders and sound list, every Snd closed and nameless
// (its +0x50 held a heap pointer of the recorded process); device 1 = a DeviceSink (else a RecordingSink answering 0) as the
// module's and the audio core's sink; production mode (no host).
void a_device_fixture(Ctx &c, const uint32_t *a, Ret &r) {
  audio_exe_reset(c);
  ExeCtx &e = ectx(c);
  device_prepare(c);
  if (a[0]) {
    std::lock_guard<std::mutex> lk(g_dev_mu);
    auto &slot = device_sinks()[&c];
    slot = std::make_unique<DeviceSink>();
    e.sink = slot.get();
    external_models().erase(&c);
  } else {
    e.owned_sink = std::make_unique<RecordingSink>();
    e.sink = e.owned_sink.get();
  }
  audio_set_sink(c, e.sink);
  r.set_u32(a[0]);
}

// S7.10: the CRT's static constructors of the three device holders (written from the instructions, log/roadmap/S7/10_notes.md
// section 3). StrList::ctor(o): [o] = the StrList table (rva 0xbaeac), [o+4] = 0, [o+8] = 0x7f, [o+0xc] / [o+0x18] / [o+0x1c] /
// [o+0x10] / [o+0x14] = 0. staticInit_g83ad80 -> NodeList::ctor(g_musicDevice): StrList::ctor, then the eh vector constructor
// iterator over the 16 buckets of 0x1c at +0x24 (NodeListBucket::ctor: [b+8] = its table rva 0xbafac, [b+0xc..b+0x18] = 0,
// [b+4] = 0x7f, byte [b] = 1), [o] = NodeList's table (rva 0xbaf00), [o+0x20] = 0. staticInit_g83ad58 -> NodeListB::ctor
// (g_sfxDevice): StrList::ctor, [o] = rva 0xbafb0. staticInit_g83af98 -> NodeListC::ctor(g_voiceDevice): StrList::ctor,
// [o] = rva 0xbb014. neg 1: the buckets' default word is 0x7e.
int g_neg_constructHolders = 0;
void construct_holders(Ctx &c) {
  const U4 b = exe_base(c);
  auto strlist = [&](U4 o) {
    S32(c, o, b + 0xbaeacu);
    S32(c, o + 4, 0);
    S32(c, o + 8, 0x7f);
    for (U4 off : {0xcu, 0x18u, 0x1cu, 0x10u, 0x14u}) S32(c, o + off, 0);
  };
  const U4 m = music_device(c);
  strlist(m);
  for (U4 i = 0; i < 16; ++i) {
    const U4 k = m + 0x24 + i * 0x1cu;
    S32(c, k + 8, b + 0xbafacu);
    for (U4 off : {0xcu, 0x10u, 0x14u, 0x18u}) S32(c, k + off, 0);
    S32(c, k + 4, NG(constructHolders) == 1 ? 0x7eu : 0x7fu);
    S<uint8_t>(c, k, 1);
  }
  S32(c, m, b + 0xbaf00u);
  S32(c, m + 0x20, 0);
  const U4 sfx = GSV(c, &GameState::g_sfxDevice), voice = GSV(c, &GameState::g_voiceDevice);
  strlist(sfx);
  S32(c, sfx, b + 0xbafb0u);
  strlist(voice);
  S32(c, voice, b + 0xbb014u);
}
// audio_exe.constructHolders(): construct_holders; returns g_musicDevice's address
void a_construct_holders(Ctx &c, const uint32_t *, Ret &r) {
  construct_holders(c);
  r.set_u32(music_device(c));
}

// audio_exe.deviceStep(step, n): 1 initSound(window 1, 0x8a) as gameMain calls it, 2 loadSoundTable(), 3 playSound for the first
// n slots loadSoundTable fills (volume 100, pan 0, pitch 0), 4 n timer ticks (the client timeSetEvent registered, called as the
// multimedia timer calls it: (id, 0, user, 0, 0) stdcall, the clock +10 ms each), 5 shutdownSoundSystem(). Each step is a call
// into the module from outside it, so the stream records it as a request.
void a_device_step(Ctx &c, const uint32_t *a, Ret &r) {
  auto call = [&](const char *name, std::initializer_list<U4> args) {
    const FuncDef *f = port_fn(name);
    if (!f) throw MissingResource{name};
    U4 v[8] = {0};
    int k = 0;
    for (U4 x : args) v[k++] = x;
    Ret rr;
    f->fn(c, v, rr);
    return rr.eax;
  };
  switch (a[0]) {
    case 1: r.set_u32(call("initSound", {1, 0x8a})); return;
    case 2: r.set_u32(call("loadSoundTable", {})); return;
    case 3: {
      U4 played = 0, last = 0;
      const U4 s0 = GSV(c, &GameState::g_sounds);
      // slots whose file the install holds (the extracted install names a directory `Golf_Sfx` that the exe asks for as
      // `Golf sfx`: U-1499; at sound.golfSfxAlias 0 those count too)
      auto present = [&](U4 s) {
        const U4 np = L32(c, s + 0x50);
        if (!np) return false;
        std::string n;
        for (U4 k = 0; k < 260; ++k) {
          const char ch = static_cast<char>(L<uint8_t>(c, np + k));
          if (!ch) break;
          n += ch == '\\' ? '/' : ch;
        }
        return c.files().exists(NG(golfSfx) == 2 ? n : golf_sfx_name(c.files(), n));  // S7.12: sound.golfSfxAlias
      };
      for (U4 i = 0; i < 300 && played < a[1]; ++i)
        if (present(s0 + i * 0x6cu)) {
          try {
            last = call("playSound", {i, 100, 0, 0, 0});
          } catch (const MissingResource &m) {
            if (DeviceModel *dm = device_model(c)) dm->lines.push_back(std::string("missing ") + (m.what ? m.what : "?"));
            throw;
          }
          ++played;
        }
      r.set_u32(last);
      return;
    }
    case 4: {
      DeviceModel *dm = device_model(c);
      if (dm == nullptr || dm->timer_cb == 0) throw MissingResource{"a registered timer client"};
      DeviceModel &d = *dm;
      for (U4 k = 0; k < a[1]; ++k) {
        d.clock += 10;
        char t[64];
        std::snprintf(t, sizeof t, "tick %u", k);
        d.lines.push_back(t);
        try {
          snd_call_va(c, d.timer_cb, SndAbi::Stdcall, {d.timer_id, 0, d.timer_user, 0, 0});
        } catch (const Fault &f) {
          std::snprintf(t, sizeof t, "fault va 0x%08x len %u", f.va, f.len);
          d.lines.push_back(t);
          throw;
        }
      }
      r.set_u32(a[1]);
      return;
    }
    case 5: r.set_u32(call("shutdownSoundSystem", {})); return;
    default: throw MissingResource{"audio_exe.deviceStep: unknown step"};
  }
}

// ---- sound.dll functions with no hooks.csv row (reached on the device-present path; U-1498) -------------------------------
// Wave_Device::set_rate (Wave_Device's table slot 15, +0x3c of the table rva 0x5b304): `mov eax, [esp + 4]; mov [ecx + 0x1ac], eax;
// ret 4`: the device's +0x1ac = v, eax = v
int g_neg_sndExtra = 0;
void a_snd_e810(Ctx &c, const uint32_t *a, Ret &r) {
  S32(c, a[0] + 0x1ac, NG(sndExtra) == 1 ? a[1] + 1 : a[1]);
  r.set_u32(a[1]);
}
// Wave_Device's other unnamed slots (table rva 0x5b304), from their instructions:
//   slot 1 FUN_10029290 / slot 12 FUN_10029620  `xor eax, eax; ret 4`      slot 2 / 10 / 13 FUN_100292b0 / 295c0 / 29640  `xor eax, eax; ret`
//   slot 11 FUN_100295e0  `ret 4` (eax untouched: the caller's; the port answers 0, U-1496)
//   slot 3 Wave_Device::timer_unhook  [this + 0x1a8] = 0          slot 24 Wave_Device::reset_time_past  [this + 0x1b0] = 0
//   slot 16 Wave_Device::get_rate  eax = [this + 0x1ac]        slot 18 Wave_Device::get_time_past  eax = [this + 0x1b0]
//   slot 27 Wave_Device::is_disabled  eax = ([this + 0x1a4] >> 1) & 1      slot 30 Wave_Device::is_eax  eax = ([this + 0x1a4] >> 3) & 1
//   slot 17 Wave_Device::add_to_time_past(v)  [this + 0x1b4] = v, [this + 0x1b0] += v, eax = v
//   slot 6 Wave_Device::get_ndevices  g_dsDevice (rva 0xb49f0) ? [it + 0x148] : 0
//   slot 34 Wave_Device::is_3d  g_dsDevice ? (byte [it + 0x6c] >> 3) & 1 (al; eax's upper bytes the device's) : al 0 (upper bytes 0)
//   slot 35 Wave_Device::set_listener_position(a, b, d)  g_dsDevice ? Ds3DListener::setPosition(a, b, d) through the thunk rva 0x164a with ecx =
//     g_dsDevice (its eax) : 3
//   slots 31, 36..39 FUN_1000e2f0 / e4d0 / e4f0 / e510 / e530  eax = 0xb (`ret 4`, `ret 0xc` for e4d0)
constexpr U4 kRvaDsDevice = 0xb49f0;
void a_snd_ret0(Ctx &, const uint32_t *, Ret &r) { r.set_u32(NG(sndExtra) == 1 ? 1u : 0u); }
void a_snd_retB(Ctx &, const uint32_t *, Ret &r) { r.set_u32(NG(sndExtra) == 1 ? 0xau : 0xbu); }
void a_snd_e7f0(Ctx &c, const uint32_t *a, Ret &) { S32(c, a[0] + 0x1a8, NG(sndExtra) == 1 ? 1u : 0u); }
void a_snd_e790(Ctx &c, const uint32_t *a, Ret &) { S32(c, a[0] + 0x1b0, NG(sndExtra) == 1 ? 1u : 0u); }
void a_snd_e830(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(L32(c, a[0] + 0x1ac) + (NG(sndExtra) == 1 ? 1u : 0u)); }
void a_snd_e7b0(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(L32(c, a[0] + 0x1b0) + (NG(sndExtra) == 1 ? 1u : 0u)); }
void a_snd_e7d0(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32((L32(c, a[0] + 0x1a4) >> (NG(sndExtra) == 1 ? 2 : 1)) & 1u); }
void a_snd_e850(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32((L32(c, a[0] + 0x1a4) >> (NG(sndExtra) == 1 ? 2 : 3)) & 1u); }
void a_snd_e1d0(Ctx &c, const uint32_t *a, Ret &r) {
  S32(c, a[0] + 0x1b4, a[1]);
  S32(c, a[0] + 0x1b0, L32(c, a[0] + 0x1b0) + (NG(sndExtra) == 1 ? 0u : a[1]));
  r.set_u32(a[1]);
}
void a_snd_e200(Ctx &c, const uint32_t *, Ret &r) {
  const U4 d = L32(c, snd_va(c, kRvaDsDevice));
  r.set_u32(d ? L32(c, d + (NG(sndExtra) == 1 ? 0x14c : 0x148)) : 0u);
}
void a_snd_e5b0(Ctx &c, const uint32_t *, Ret &r) {
  const U4 d = L32(c, snd_va(c, kRvaDsDevice));
  r.set_u32(d ? ((d & ~0xffu) | ((L<uint8_t>(c, d + 0x6c) >> (NG(sndExtra) == 1 ? 2 : 3)) & 1u)) : 0u);
}
void a_snd_e490(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 d = L32(c, snd_va(c, kRvaDsDevice));
  if (!d) {
    r.set_u32(NG(sndExtra) == 1 ? 2u : 3u);
    return;
  }
  r = snd_call(c, 0x164a, SndAbi::Thiscall, {d, a[1], a[2], a[3]});
}
// MultiSound::is_hwbuffer (thiscall, `ret`): eax = (int)([this + 0xc8] << 3) >> 31, i.e. -1 when bit 28 is set, else 0
void a_snd_3a7c0(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32((L32(c, a[0] + 0xc8) & (NG(sndExtra) == 1 ? (1u << 27) : (1u << 28))) ? 0xffffffffu : 0u);
}
// MultiSound's unnamed slots (table rva 0x5c7e8; the same layout as the exe's Snd: device +0x40, flags +0x44, links +0x48 /
// +0x4c) and their small callees, from their instructions (log/roadmap/S4/10_work/ms_dis.txt):
//   FUN_1003a6b0(v)  Channel_recomputeVoiceGains(this + 0x70, v) (thunk 0x1efb); 0
//   Patch::set_fade_in(v)  v 0 -> 0xa; [this + 0x38] = v; 0                       MultiSound::set_delay(v)  [this + 0x29c] = v; eax = v
//   SubChannel::play_1002b1b0(v)  the device (+0x40) slot 0x18(v)'s eax, else 0x14
//   MultiSound::stop()   [this + 0xc8] bit 1 clear -> 0x14; else the queue at rva 0xb4a20, when set, WaveCmdQueue::pushNode(this, 2)
//                    (thunk 0x20b3); 0
//   SubChannel::fade_1002b3a0(a) / SubChannel::fade_in_1002b430(a)  own slot 4 / 0x54 with a; zero -> own slot 0x28's result
//   MultiSound::set_pan(v)  SubChannel::set_pan(v) (thunk 0x15c8) then Channel_setPanGains(this + 0x70, v) (thunk 0x1a7d), its eax
//   SubChannel::set_pan(p)  p clamped to [-0x40, 0x3f] into +8; the device's slot 0x44(p)'s eax, else the clamped p
//   MultiSound::set_loop_state(v)  Channel::set_loop_state(this + 0x70, v) (thunk 0x25f9)
//   Channel::set_loop_state(v)  +0x58 bit 2 = v bit 0, +0x2d0 bit 0 = v bit 0 (a 16-pass loop that sets the same bit); eax = the new +0x2d0
//   MultiSound::get_time(k)  Channel::get_time(this + 0x70, k) (thunk 0x1b09)
//   Channel::get_time(k)  bit k of (+0x58 >> 12) & 0xffff set -> [this + 0x110 k + 0x2f8], else 0
//   MultiSound::is_looping / MultiSound::is_loaded / MultiSound::is_hwbuffer  -(bit 2 / 1 / 28 of [this + 0xc8])
//   MultiSound::is_playing     [this + 0xc8] bit 6 set and bit 4 clear -> 1, else 0
//   SubChannel::get_next / 56e0 / 5700 / 1003a710 / 1003a670  eax = [this + 0x4c / 0x48 / 0x54 / 0x298 / 0x140c]
//   SubChannel::attach     linkIntoList on the list at rva 0xb5070 (head) / 0xb506c (tail), as the exe's Snd::linkIntoList
//   MultiSound::detach     unlinkFromActiveList (thunk 0x2419); 0
//   Wave::load(a, m)  own slot 0x88(a, m); m bit 2 -> 0; else own slot 0x8c's result (the exe's Snd::applyMode484e30)
//   FUN_1002b110 / 2c2b0 (`ret 4`), 2c2d0 (`ret`), 2c2f0 / 2c310 / 2c330 / 2c350 (`ret 0xc`)  eax = 0xb
//   FUN_1002c1c0     the device's slot 0xc0 (tail jump), else 0
//   FUN_1002c1f0(a, b, d)  the device's slot 0xc4(a, b, d), else (d != 0: byte [a] = 0) 1
//   FUN_1002b870(v) / 2b8a0(v)  the device's slot 0xd0 / 0xd4(v); no device: eax the caller's (0 here, U-1496)
constexpr U4 kRvaMsHead = 0xb5070, kRvaMsTail = 0xb506c, kRvaQueue4a20 = 0xb4a20;
U4 dev40(Ctx &c, U4 t) { return L32(c, t + 0x40); }
Ret dslot(Ctx &c, U4 dev, U4 slot, std::initializer_list<U4> args) { return snd_vcall(c, dev, slot, SndAbi::Thiscall, args); }
void a_snd_3a6b0(Ctx &c, const uint32_t *a, Ret &r) {
  snd_call(c, 0x1efb, SndAbi::Thiscall, {a[0] + 0x70, a[1]});
  r.set_u32(NG(sndExtra) == 1 ? 1u : 0u);
}
void a_snd_5670(Ctx &c, const uint32_t *a, Ret &r) {
  if (a[1] == 0) {
    r.set_u32(0xa);
    return;
  }
  S32(c, a[0] + 0x38, NG(sndExtra) == 1 ? a[1] + 1 : a[1]);
  r.set_u32(0);
}
void a_snd_3a1c0(Ctx &c, const uint32_t *a, Ret &r) {
  S32(c, a[0] + 0x29c, NG(sndExtra) == 1 ? a[1] + 1 : a[1]);
  r.set_u32(a[1]);
}
void a_snd_2b1b0(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 d = dev40(c, a[0]);
  if (!d) {
    r.set_u32(NG(sndExtra) == 1 ? 0x13u : 0x14u);
    return;
  }
  r = dslot(c, d, 0x18, {a[1]});
}
void a_snd_3a040(Ctx &c, const uint32_t *a, Ret &r) {
  if (!(L<uint8_t>(c, a[0] + 0xc8) & 2)) {
    r.set_u32(0x14);
    return;
  }
  const U4 q = L32(c, snd_va(c, kRvaQueue4a20));
  if (q) snd_call(c, 0x20b3, SndAbi::Thiscall, {q, a[0], NG(sndExtra) == 1 ? 3u : 2u});
  r.set_u32(0);
}
template <U4 Slot> void a_snd_apply(Ctx &c, const uint32_t *a, Ret &r) {
  U4 v = snd_vcall(c, a[0], Slot, SndAbi::Thiscall, {a[1]}).eax;
  if (v == 0 && NG(sndExtra) != 1) v = snd_vcall(c, a[0], 0x28, SndAbi::Thiscall, {}).eax;
  r.set_u32(v);
}
void a_snd_2b4a0(Ctx &c, const uint32_t *a, Ret &r) {
  I4 p = static_cast<I4>(a[1]);
  if (p < -0x40) p = -0x40;
  else if (p > (NG(sndExtra) == 1 ? 0x3e : 0x3f)) p = NG(sndExtra) == 1 ? 0x3e : 0x3f;
  S32(c, a[0] + 8, static_cast<U4>(p));
  const U4 d = dev40(c, a[0]);
  if (d) {
    r = dslot(c, d, 0x44, {static_cast<U4>(p)});
    return;
  }
  r.set_u32(static_cast<U4>(p));
}
void a_snd_3a1e0(Ctx &c, const uint32_t *a, Ret &r) {
  snd_call(c, 0x15c8, SndAbi::Thiscall, {a[0], a[1]});
  r = snd_call(c, 0x1a7d, SndAbi::Thiscall, {a[0] + 0x70, NG(sndExtra) == 1 ? a[1] + 1 : a[1]});
}
void a_snd_3a690(Ctx &c, const uint32_t *a, Ret &r) { r = snd_call(c, 0x25f9, SndAbi::Thiscall, {a[0] + 0x70, a[1]}); }
void a_snd_378b0(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 bit = (NG(sndExtra) == 1 ? ~a[1] : a[1]) & 1u;
  S32(c, a[0] + 0x58, (L32(c, a[0] + 0x58) & 0xfffffffbu) | (bit << 2));
  const U4 v = (L32(c, a[0] + 0x2d0) & ~1u) | bit;
  S32(c, a[0] + 0x2d0, v);
  r.set_u32(v);
}
void a_snd_3a590(Ctx &c, const uint32_t *a, Ret &r) { r = snd_call(c, 0x1b09, SndAbi::Thiscall, {a[0] + 0x70, a[1]}); }
void a_snd_37860(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 k = a[1];
  const U4 set = (L32(c, a[0] + 0x58) >> 12) & 0xffffu;
  if ((1u << (k & 31)) & set) {
    r.set_u32(L32(c, a[0] + k * 0x110u + (NG(sndExtra) == 1 ? 0x2fcu : 0x2f8u)));
    return;
  }
  r.set_u32(0);
}
template <int Bit> void a_snd_negbit(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32((L32(c, a[0] + 0xc8) >> (NG(sndExtra) == 1 ? (Bit + 1) % 32 : Bit) & 1u) ? 0xffffffffu : 0u);
}
void a_snd_3a790(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 v = L32(c, a[0] + 0xc8);
  r.set_u32(((v & 0x40) && !(v & (NG(sndExtra) == 1 ? 0x20u : 0x10u))) ? 1u : 0u);
}
template <U4 Off> void a_snd_field(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32(L32(c, a[0] + Off) + (NG(sndExtra) == 1 ? 1u : 0u));
}
void a_snd_2b010(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 t = a[0];
  r.set_u32(0);
  if (L32(c, t + 0x4c) != 0 || L32(c, t + 0x48) != 0) return;
  const U4 tail = snd_va(c, kRvaMsTail), head = snd_va(c, kRvaMsHead);
  const U4 last = L32(c, tail);
  if (last == 0) {
    S32(c, head, t);
    S32(c, tail, t);
  } else {
    S32(c, t + 0x48, last);
    S32(c, tail, t);
    S32(c, L32(c, t + 0x48) + 0x4c, t);
  }
  S32(c, t + 0x4c, 0);
  if (last == 0) S32(c, t + 0x48, 0);
  S32(c, t + 0x44, L32(c, t + 0x44) | (NG(sndExtra) == 1 ? 4u : 2u));
}
void a_snd_3a0b0(Ctx &c, const uint32_t *a, Ret &r) {
  snd_call(c, 0x2419, SndAbi::Thiscall, {a[0]});
  r.set_u32(NG(sndExtra) == 1 ? 1u : 0u);
}
void a_snd_2be90(Ctx &c, const uint32_t *a, Ret &r) {
  snd_vcall(c, a[0], 0x88, SndAbi::Thiscall, {a[1], a[2]});
  if (a[2] & (NG(sndExtra) == 1 ? 8u : 4u)) {
    r.set_u32(0);
    return;
  }
  r = snd_vcall(c, a[0], 0x8c, SndAbi::Thiscall, {});
}
void a_snd_2c1c0(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 d = dev40(c, a[0]);
  if (!d) {
    r.set_u32(NG(sndExtra) == 1 ? 1u : 0u);
    return;
  }
  r = dslot(c, d, 0xc0, {});
}
void a_snd_2c1f0(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 d = dev40(c, a[0]);
  if (d) {
    r = dslot(c, d, 0xc4, {a[1], a[2], a[3]});
    return;
  }
  if (a[3] != 0) S<uint8_t>(c, a[1], 0);
  r.set_u32(NG(sndExtra) == 1 ? 2u : 1u);
}
template <U4 Slot> void a_snd_fwd1(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 d = dev40(c, a[0]);
  if (!d) {
    r.set_u32(NG(sndExtra) == 1 ? 1u : 0u);
    return;
  }
  r = dslot(c, d, Slot, {a[1]});
}
// Midi_Device::add_to_time_past(v)  [this + 0x34] += v; eax = v
void a_snd_af20(Ctx &c, const uint32_t *a, Ret &r) {
  S32(c, a[0] + 0x34, L32(c, a[0] + 0x34) + (NG(sndExtra) == 1 ? a[1] + 1 : a[1]));
  r.set_u32(a[1]);
}
// Midi_Device's unnamed slots (table rva 0x5b28c), from their instructions:
//   FUN_10029e30 / 2a0f0 (`ret 4`), 29e50 / 2a0b0 / 2a110 (`ret`)  eax = 0;   FUN_1000a120 / 2a0d0  `ret 4`, eax the caller's
//   FUN_1000a0e0  eax = 0x2f (`ret 0xc`)
//   Midi_Device::timer_unhook / af60  [this + 0x2c] / [this + 0x34] = 0;   Midi_Device::set_rate(v)  [this + 0x30] = v, eax = v
//   Midi_Device::get_rate / af40  eax = [this + 0x30] / [this + 0x34];   Midi_Device::is_disabled  eax = -(bit 0 of [this + 0x24])
//   Midi_Device::suspend  [this + 0x2c] set -> SoundTimer::unregisterClient(this) on g_soundTimer (rva 0xb49ec, thunk 0x1ed3), its
//                 eax; else eax = 0 (the field)
template <U4 Off> void a_snd_zero(Ctx &c, const uint32_t *a, Ret &) { S32(c, a[0] + Off, NG(sndExtra) == 1 ? 1u : 0u); }
template <U4 Off> void a_snd_set(Ctx &c, const uint32_t *a, Ret &r) {
  S32(c, a[0] + Off, NG(sndExtra) == 1 ? a[1] + 1 : a[1]);
  r.set_u32(a[1]);
}
template <U4 Off, int Bit> void a_snd_negbit_at(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32(((L32(c, a[0] + Off) >> (NG(sndExtra) == 1 ? Bit + 1 : Bit)) & 1u) ? 0xffffffffu : 0u);
}
void a_snd_ret2f(Ctx &, const uint32_t *, Ret &r) { r.set_u32(NG(sndExtra) == 1 ? 0x2eu : 0x2fu); }
void a_snd_a140(Ctx &c, const uint32_t *a, Ret &r) {
  const U4 f = L32(c, a[0] + 0x2c);
  if (f == 0 || NG(sndExtra) == 1) {
    r.set_u32(f);
    return;
  }
  r = snd_call(c, 0x1ed3, SndAbi::Thiscall, {L32(c, snd_va(c, 0xb49ec)), a[0]});
}
const ExeFn kSndExtra[] = {
    {0x29e30, "snd.FUN_10029e30"}, {0x2a0f0, "snd.FUN_1002a0f0"}, {0x29e50, "snd.FUN_10029e50"},
    {0x2a0b0, "snd.FUN_1002a0b0"}, {0x2a110, "snd.FUN_1002a110"}, {0xa120, "snd.FUN_1000a120"},
    {0x2a0d0, "snd.FUN_1002a0d0"}, {0xa0e0, "snd.FUN_1000a0e0"}, {0xaf80, "snd.Midi_Device::timer_unhook"},
    {0xaf60, "snd.Midi_Device::reset_time_past"}, {0xafa0, "snd.Midi_Device::set_rate"}, {0xafc0, "snd.Midi_Device::get_rate"},
    {0xaf40, "snd.Midi_Device::get_time_past"}, {0xafe0, "snd.Midi_Device::is_disabled"}, {0xa140, "snd.Midi_Device::suspend"},
    {0xaf20, "snd.Midi_Device::add_to_time_past"},
    {0x3a6b0, "snd.FUN_1003a6b0"}, {0x5670, "snd.Patch::set_fade_in"}, {0x2b110, "snd.FUN_1002b110"},
    {0x2b1b0, "snd.SubChannel::play_1002b1b0"}, {0x3a040, "snd.MultiSound::stop"}, {0x2b3a0, "snd.SubChannel::fade_1002b3a0"},
    {0x2b430, "snd.SubChannel::fade_in_1002b430"}, {0x3a1e0, "snd.MultiSound::set_pan"}, {0x2b4a0, "snd.SubChannel::set_pan"},
    {0x3a690, "snd.MultiSound::set_loop_state"}, {0x378b0, "snd.Channel::set_loop_state"}, {0x3a1c0, "snd.MultiSound::set_delay"},
    {0x3a750, "snd.MultiSound::is_looping"}, {0x3a790, "snd.MultiSound::is_playing"}, {0x3a7e0, "snd.MultiSound::is_loaded"},
    {0x56c0, "snd.SubChannel::get_next"}, {0x56e0, "snd.SubChannel::get_prev"}, {0x3a710, "snd.MultiSound::get_time_1003a710"},
    {0x5700, "snd.SubChannel::get_type"}, {0x2b010, "snd.SubChannel::attach"}, {0x3a0b0, "snd.MultiSound::detach"},
    {0x2be90, "snd.Wave::load"}, {0x2c2b0, "snd.FUN_1002c2b0"}, {0x2c2d0, "snd.FUN_1002c2d0"},
    {0x2c2f0, "snd.FUN_1002c2f0"}, {0x2c310, "snd.FUN_1002c310"}, {0x2c330, "snd.FUN_1002c330"},
    {0x2c350, "snd.FUN_1002c350"}, {0x3a590, "snd.MultiSound::get_time"}, {0x37860, "snd.Channel::get_time"},
    {0x2c1c0, "snd.FUN_1002c1c0"}, {0x2c1f0, "snd.FUN_1002c1f0"}, {0x3a670, "snd.MultiSound::get_ms_length"},
    {0x2b870, "snd.FUN_1002b870"}, {0x2b8a0, "snd.FUN_1002b8a0"},
    {0x3a7c0, "snd.MultiSound::is_hwbuffer"},
    {0xe810, "snd.Wave_Device::set_rate"}, {0x29290, "snd.FUN_10029290"}, {0x29620, "snd.FUN_10029620"},
    {0x292b0, "snd.FUN_100292b0"}, {0x295c0, "snd.FUN_100295c0"}, {0x29640, "snd.FUN_10029640"},
    {0x295e0, "snd.FUN_100295e0"}, {0xe7f0, "snd.Wave_Device::timer_unhook"}, {0xe790, "snd.Wave_Device::reset_time_past"},
    {0xe830, "snd.Wave_Device::get_rate"}, {0xe7b0, "snd.Wave_Device::get_time_past"}, {0xe7d0, "snd.Wave_Device::is_disabled"},
    {0xe850, "snd.Wave_Device::is_eax"}, {0xe1d0, "snd.Wave_Device::add_to_time_past"}, {0xe200, "snd.Wave_Device::get_ndevices"},
    {0xe5b0, "snd.Wave_Device::is_3d"}, {0xe490, "snd.Wave_Device::set_listener_position"}, {0xe2f0, "snd.FUN_1000e2f0"},
    {0xe4d0, "snd.FUN_1000e4d0"}, {0xe4f0, "snd.FUN_1000e4f0"}, {0xe510, "snd.FUN_1000e510"},
    {0xe530, "snd.FUN_1000e530"},
};

// ---- configuration and harness entries ----------------------------------------------------------------------------------
// audio_exe.setup(replay): replay 1 = the oracle / bundle replay (claims from "$audio/claims" as audio_core.setup reads
// them, imports and exe fallbacks to the host); 0 = production. Resets the module's per-context state.
void a_setup(Ctx &c, const uint32_t *a, Ret &r) {
  audio_exe_reset(c);
  ExeCtx &e = ectx(c);
  e.replay = a[0] != 0;
  if (e.replay && c.files().exists("$audio/claims")) {
    const std::vector<uint8_t> b = c.files().read_all("$audio/claims");
    for (size_t i = 0; i + 4 <= b.size(); i += 4) {
      U4 v;
      std::memcpy(&v, b.data() + i, 4);
      e.claims.insert(v);
    }
  }
  r.set_u32(static_cast<U4>(e.claims.size()));
}
// audio_exe.useSink(answer): a RecordingSink for the exe's requests and imports, also the audio core's sink (one stream)
void a_stub_requests(Ctx &c, const uint32_t *a, Ret &) { ectx(c).stub_requests = a[0] != 0; }
void a_use_sink(Ctx &c, const uint32_t *a, Ret &) {
  ExeCtx &e = ectx(c);
  e.owned_sink = std::make_unique<RecordingSink>();
  e.owned_sink->answer = a[0];
  e.sink = e.owned_sink.get();
  e.frame_seen = false;
  audio_set_sink(c, e.sink);
}
// audio_exe.streamLog(): the recorded lines into "$audio_exe/stream" (one per command); returns the line count
void a_stream_log(Ctx &c, const uint32_t *, Ret &r) {
  ExeCtx &e = ectx(c);
  std::string s;
  U4 n = 0;
  const RecordingSink *rs = e.owned_sink.get();
  {
    std::lock_guard<std::mutex> lk(g_dev_mu);
    auto ds = device_sinks().find(&c);
    if (ds != device_sinks().end() && e.sink == ds->second.get()) rs = ds->second.get();
    else if (auto xm = external_models().find(&c); xm != external_models().end()) rs = xm->second;  // S7.10
  }
  if (rs)
    for (const std::string &l : rs->lines) {
      s += l + "\n";
      ++n;
    }
  c.files().write("$audio_exe/stream", std::vector<uint8_t>(s.begin(), s.end()));
  r.set_u32(n);
}
// audio_exe.report(): counters into "$audio_exe/out" (key=value lines)
void a_report(Ctx &c, const uint32_t *, Ret &r) {
  ExeCtx &e = ectx(c);
  const AudioExeCounters &k = e.counters;
  std::string s = "requests=" + std::to_string(k.requests) + "\ndispatched=" + std::to_string(k.dispatched) +
                  "\nfallbacks=" + std::to_string(k.fallbacks) + "\nheap_allocs=" + std::to_string(k.heap_allocs) +
                  "\nfallback_names=" + k.fallback_names + "\nreplay=" + (e.replay ? "1" : "0") + "\n";
  c.files().write("$audio_exe/out", std::vector<uint8_t>(s.begin(), s.end()));
  r.set_u32(k.fallbacks);
}
// audio_exe.golfSfxName() (S7.12, tests): the name in "$audio_exe/sfxname" (`\` or `/`) as the device sink's mmioOpenA opens
// it, into "$audio_exe/sfxname.out"; returns 1 when that file exists
void a_golf_sfx_name(Ctx &c, const uint32_t *, Ret &r) {
  const std::vector<uint8_t> b = c.files().read_all("$audio_exe/sfxname");
  std::string x(b.begin(), b.end());
  for (char &ch : x)
    if (ch == '\\') ch = '/';
  const std::string n = golf_sfx_name(c.files(), x);
  c.files().write("$audio_exe/sfxname.out", std::vector<uint8_t>(n.begin(), n.end()));
  r.set_u32(c.files().exists(n) ? 1u : 0u);
}
// audio_exe.mapImage(): exe_map_image; returns the image base
void a_map_image(Ctx &c, const uint32_t *, Ret &r) {
  exe_map_image(c);
  r.set_u32(exe_base(c));
}

const FuncDef kFuncs[] = {
    // the C3 / C4 rows
    {"playSound", "default", "int", 5, 0x027F, a_play_sound, &g_neg_playSound},
    {"startSoundSlot", "default", "int", 1, 0, a_start_sound_slot, &g_neg_startSoundSlot},
    {"loadSoundTable", "default", "int", 0, 0, a_load_sound_table, &g_neg_loadSoundTable},
    {"shutdownSoundSystem", "default", "int", 0, 0, a_shutdown_sound_system, &g_neg_shutdownSoundSystem},
    {"initSound", "default", "int", 2, 0, a_init_sound, &g_neg_initSound},
    {"shutdownSound", "default", "int", 0, 0, a_shutdown_sound, &g_neg_shutdownSound},
    {"soundVersionCheck", "mscdecl", "int", 0, 0, a_sound_version_check, &g_neg_soundVersionCheck},
    {"freeSoundLib", "mscdecl", "int", 0, 0, a_free_sound_lib, &g_neg_freeSoundLib},
    {"callSoundHook74", "mscdecl", "int", 2, 0, a_call_sound_hook74, &g_neg_callSoundHook74},
    {"callSoundHook94", "mscdecl", "int", 0, 0, a_call_sound_hook94, &g_neg_callSoundHook94},
    {"createSound4840e0", "default", "int", 3, 0, a_create_sound, &g_neg_createSound},
    {"releaseSoundObj", "default", "int", 1, 0, a_release_sound_obj, &g_neg_releaseSoundObj},
    {"clearSoundPtrs", "default", "void", 0, 0, a_clear_sound_ptrs, &g_neg_clearSoundPtrs},
    {"Snd::ctorBase", "thiscall", "pointer", 1, 0, a_ctor_base, &g_neg_ctorBase},
    {"Snd::scalarDtor4841c0", "thiscall", "int", 2, 0, a_scalar_dtor4841c0, &g_neg_scalarDtor4841c0},
    {"Snd::dtor", "thiscall", "int", 1, 0, a_snd_dtor, &g_neg_sndDtor},
    {"Snd::setMode", "thiscall", "void", 2, 0, a_set_mode, &g_neg_setMode},
    {"SndMusic::setFile", "thiscall", "int", 2, 0, a_music_set_file, &g_neg_musicSetFile},
    {"unlinkSound", "default", "uint32", 1, 0, a_unlink_sound, &g_neg_unlinkSound},
    {"Snd::unlinkFromList", "thiscall", "int", 1, 0, a_unlink_from_list, &g_neg_unlinkFromList},
    {"Snd::stop484550", "thiscall", "int", 1, 0, a_stop484550, &g_neg_stop484550},
    {"Snd::queryDevice4845d0", "thiscall", "int", 1, 0, a_query_device, &g_neg_queryDevice},
    {"Snd::status", "thiscall", "int", 1, 0, a_status, &g_neg_status},
    {"Snd::setField34", "thiscall", "int", 2, 0, a_set_field34, &g_neg_setField34},
    {"Snd::setField38", "thiscall", "int", 2, 0, a_set_field38, &g_neg_setField38},
    {"Snd::applyField484750", "thiscall", "int", 2, 0, a_apply_field484750, &g_neg_applyField484750},
    {"Snd::applyField4847b0", "thiscall", "int", 2, 0, a_apply_field4847b0, &g_neg_applyField4847b0},
    {"Snd::setPan", "thiscall", "int", 2, 0, a_set_pan, &g_neg_setPan},
    {"Snd::ctorDerived", "thiscall", "pointer", 1, 0, a_ctor_derived, &g_neg_ctorDerived},
    {"SndMusic::scalarDtor", "thiscall", "int", 2, 0, a_music_scalar_dtor, &g_neg_musicScalarDtor},
    {"SndMusic::dtor", "thiscall", "int", 1, 0, a_music_dtor, &g_neg_musicDtor},
    {"Snd::play", "thiscall", "int", 1, 0, a_play, &g_neg_play},
    {"Snd::configure", "thiscall", "void", 3, 0, a_configure, &g_neg_configure},
    {"Snd::busy", "thiscall", "int", 1, 0, a_busy, &g_neg_busy},
    {"Snd::setName", "thiscall", "int", 2, 0, a_set_name, &g_neg_setName},
    {"SndStream::open484c20", "thiscall", "int", 2, 0, a_stream_open, &g_neg_streamOpen},
    {"Snd::open", "thiscall", "int", 2, 0, a_open, &g_neg_open},
    {"Snd::reopen", "thiscall", "int", 1, 0, a_reopen, &g_neg_reopen},
    {"Snd::applyMode484e30", "thiscall", "int", 3, 0, a_apply_mode, &g_neg_applyMode},
    {"Snd::start", "thiscall", "int", 1, 0, a_start, &g_neg_start},
    {"Snd::stop484f00", "thiscall", "int", 1, 0, a_stop484f00, &g_neg_stop484f00},
    {"Snd::setPitch", "thiscall", "int", 2, 0, a_set_pitch, &g_neg_setPitch},
    {"Snd::flags", "thiscall", "int", 1, 0, a_flags, &g_neg_flags},
    {"Snd::setVolume", "thiscall", "int", 2, 0x027F, a_set_volume, &g_neg_setVolume},
    {"Snd::ctor485260", "thiscall", "pointer", 1, 0, a_ctor485260, &g_neg_ctor485260},
    {"Snd::scalarDtor4852b0", "thiscall", "int", 2, 0, a_scalar_dtor4852b0, &g_neg_scalarDtor4852b0},
    {"Snd::setVtbl4852d0", "thiscall", "void", 1, 0, a_set_vtbl, &g_neg_setVtbl},
    {"VoiceRx::ctor", "thiscall", "pointer", 1, 0, a_voice_rx_ctor, &g_neg_voiceRxCtor},
    {"openHookA", "thiscall", "int", 2, 0, a_open_hook_a, &g_neg_openHookA},
    {"openHookB", "thiscall", "int", 2, 0, a_open_hook_b, &g_neg_openHookB},
    {"openHookC", "thiscall", "int", 2, 0, a_open_hook_c, &g_neg_openHookC},
    {"closeHookC", "thiscall", "int", 1, 0, a_close_hook_c, &g_neg_closeHookC},
    // the C2 rows
    {"gSoundsDtor_448190", "default", "void", 0, 0, a_g_sounds_dtor, &g_neg_gSoundsDtor},
    {"arrayStaticInit_448160", "default", "void", 0, 0, a_array_static_init, &g_neg_arrayInit},
    {"gMusicDeviceDtor_483e00", "default", "int", 0, 0, a_music_device_dtor, &g_neg_deviceDtors},
    {"gSfxDeviceDtor_483e40", "default", "int", 0, 0, a_sfx_device_dtor, &g_neg_deviceDtors},
    {"gVoiceDeviceDtor_483e80", "default", "int", 0, 0, a_voice_device_dtor, &g_neg_deviceDtors},
    {"Snd::isPlaying", "thiscall", "int", 1, 0, a_is_playing, &g_neg_c2},
    {"Snd::linkIntoList", "thiscall", "int", 1, 0, a_link_into_list, &g_neg_c2},
    {"Snd::getNext", "thiscall", "uint32", 1, 0, a_get_next, &g_neg_c2},
    {"Snd::getPrev", "thiscall", "uint32", 1, 0, a_get_prev, &g_neg_c2},
    {"Snd::getMode", "thiscall", "int", 1, 0, a_get_mode, &g_neg_c2},
    {"Snd::getLengthMs", "thiscall", "uint32", 1, 0, a_get_length, &g_neg_c2},
    {"returnB_484540", "thiscall", "int", 2, 0, a_ret_b_pop4, &g_neg_c2},
    {"returnB_485120", "thiscall", "int", 1, 0, a_ret_b, &g_neg_c2},
    {"returnBPop12_485130", "thiscall", "int", 4, 0, a_ret_b, &g_neg_c2},
    {"returnBPop4_485380", "thiscall", "int", 2, 0, a_ret_b_pop4, &g_neg_c2},
    {"Snd::setVolumeBase", "thiscall", "int", 2, 0x027F, a_set_volume_base, &g_neg_c2},
    {"Snd::setLoop", "thiscall", "int", 2, 0, a_set_loop, &g_neg_c2},
    {"Snd::setField38Voice54", "thiscall", "int", 2, 0, a_set_field38_voice54, &g_neg_c2},
    {"Snd::applyField38Voice28", "thiscall", "int", 1, 0, a_apply_field38_voice28, &g_neg_c2},
    {"Snd::applyField38Voice30", "thiscall", "int", 1, 0, a_apply_field38_voice30, &g_neg_c2},
    {"Snd::setField60", "thiscall", "int", 2, 0, a_set_field60, &g_neg_c2},
    {"Snd::setFlags", "thiscall", "int", 2, 0, a_set_flags, &g_neg_c2},
    {"SndBase::setField38", "thiscall", "int", 2, 0, a_base_set_field38, &g_neg_c2},
    {"Snd::fwdDev49", "thiscall", "int", 4, 0, a_fwd_dev49, &g_neg_c2},
    {"VoiceRx::start", "thiscall", "int", 4, 0, a_voice_rx_start, &g_neg_voiceRxStart},
    {"NodeList::playChildSuspendingSounds", "thiscall", "int", 2, 0, a_play_child, &g_neg_playChild},
    {"Snd::fwdVoice1c", "thiscall", "int", 1, 0, a_fwd<0>, &g_neg_c2},
    {"Snd::fwdVoice18", "thiscall", "int", 2, 0, a_fwd<1>, &g_neg_c2},
    {"Snd::fwdVoice38", "thiscall", "int", 1, 0, a_fwd<2>, &g_neg_c2},
    {"Snd::fwdVoice34", "thiscall", "int", 4, 0, a_fwd<3>, &g_neg_c2},
    {"Snd::fwdVoice58", "thiscall", "int", 1, 0, a_fwd<4>, &g_neg_c2},
    {"Snd::fwdVoice74", "thiscall", "int", 1, 0, a_fwd<5>, &g_neg_c2},
    {"Snd::fwdVoice60", "thiscall", "int", 1, 0, a_fwd<6>, &g_neg_c2},
    {"Snd::fwdVoice98", "thiscall", "int", 2, 0, a_fwd<7>, &g_neg_c2},
    {"Snd::fwdVoice94", "thiscall", "int", 2, 0, a_fwd<8>, &g_neg_c2},
    {"Snd::fwdVoiceD0", "thiscall", "int", 2, 0, a_fwd<9>, &g_neg_c2},
    {"Snd::fwdVoiceD4", "thiscall", "int", 2, 0, a_fwd<10>, &g_neg_c2},
    {"Snd::fwdVoiceCC", "thiscall", "int", 1, 0, a_fwd<11>, &g_neg_c2},
    {"Snd::fwdVoiceF0", "thiscall", "int", 3, 0, a_fwd<12>, &g_neg_c2},
    {"Snd::fwdVoiceB8", "thiscall", "int", 2, 0, a_fwd<13>, &g_neg_c2},
    {"Snd::fwdVoiceBC", "thiscall", "int", 1, 0, a_fwd<14>, &g_neg_c2},
    {"Snd::fwdVoice3c", "thiscall", "int", 1, 0, a_fwd<15>, &g_neg_c2},
    {"Snd::fwdDev48", "thiscall", "int", 1, 0, a_fwd<16>, &g_neg_c2},
    {"Snd::fwdDev54", "thiscall", "int", 4, 0, a_fwd<17>, &g_neg_c2},
    {"Snd::fwdDev55", "thiscall", "int", 2, 0, a_fwd<18>, &g_neg_c2},
    {"Snd::fwdDev56", "thiscall", "int", 2, 0, a_fwd<19>, &g_neg_c2},
    {"Snd::fwdDev57", "thiscall", "int", 2, 0, a_fwd<20>, &g_neg_c2},
    {"Snd::fwdDev58", "thiscall", "int", 1, 0, a_fwd<21>, &g_neg_c2},
    {"VoiceRx::fwdDev37", "thiscall", "int", 1, 0, a_fwd<22>, &g_neg_c2},
    {"VoiceRx::callDev34", "thiscall", "int", 1, 0, a_fwd<23>, &g_neg_c2},
    {"VoiceRx::callDev8", "thiscall", "int", 1, 0, a_fwd<24>, &g_neg_c2},
    {"VoiceRx::fwdDev5", "thiscall", "int", 1, 0, a_fwd<25>, &g_neg_c2},
    {"VoiceRx::fwdDev35", "thiscall", "int", 2, 0, a_fwd<26>, &g_neg_c2},
    {"VoiceRx::fwdDev36", "thiscall", "int", 1, 0, a_fwd<27>, &g_neg_c2},
    // the device-holder helpers
    {"SndGroup_isDisabled", "thiscall", "uint8", 2, 0, a_is_disabled, &g_neg_isDisabled},  // al (registry c3r.py: uint8)
    {"SndGroup_isMuted", "thiscall", "uint8", 2, 0, a_is_muted, &g_neg_isMuted},  // al (registry c3r.py: uint8)
    {"HashTable::remove", "thiscall", "int", 2, 0, a_hash_remove, &g_neg_hashRemove},
    {"resourceQuery", "thiscall", "int", 1, 0, a_resource_query, &g_neg_resourceQuery},
    {"resourceLoad", "thiscall", "int", 3, 0, a_resource_load, &g_neg_resourceLoad},
    {"resourceLoadB", "thiscall", "int", 3, 0, a_resource_load_b, &g_neg_resourceLoadB},
    {"releaseWithHookA", "thiscall", "int", 1, 0, a_release_a, &g_neg_releaseA},
    {"releaseWithHookB", "thiscall", "int", 1, 0, a_release_b, &g_neg_releaseB},
    {"releaseWithHookC", "thiscall", "int", 1, 0, a_release_c, &g_neg_releaseC},
    {"closeHookA", "thiscall", "int", 1, 0, a_close_hook_a, &g_neg_closeHookA},
    {"closeHookB", "thiscall", "int", 1, 0, a_close_hook_b, &g_neg_closeHookB},
    {"showMessageBox", "default", "int", 2, 0, a_show_message_box, &g_neg_showMessageBox},
    // configuration
    {"audio_exe.setup", "default", "uint32", 1, 0, a_setup, &g_neg_none},
    {"audio_exe.useSink", "default", "void", 1, 0, a_use_sink, &g_neg_none},
    {"audio_exe.stubRequests", "default", "void", 1, 0, a_stub_requests, &g_neg_none},
    {"audio_exe.streamLog", "default", "uint32", 0, 0, a_stream_log, &g_neg_none},
    {"audio_exe.golfSfxName", "default", "uint32", 0, 0, a_golf_sfx_name, &g_neg_golfSfx},
    {"audio_exe.report", "default", "uint32", 0, 0, a_report, &g_neg_none},
    {"audio_exe.mapImage", "default", "uint32", 0, 0, a_map_image, &g_neg_none},
    {"audio_exe.deviceFixture", "default", "uint32", 1, 0, a_device_fixture, &g_neg_none},
    {"audio_exe.deviceStep", "default", "uint32", 2, 0, a_device_step, &g_neg_none},
    {"audio_exe.constructHolders", "default", "uint32", 0, 0, a_construct_holders, &g_neg_constructHolders},  // S7.10
    // sound.dll functions with no hooks.csv row that the device-present start-up reaches (gate c; the core finds them
    // through audio_exe_sound_name, U-1498)
    {"snd.Wave_Device::set_rate", "thiscall", "uint32", 2, 0, a_snd_e810, &g_neg_sndExtra},
    {"snd.MultiSound::is_hwbuffer", "thiscall", "uint32", 1, 0, a_snd_3a7c0, &g_neg_sndExtra},
    {"snd.FUN_1003a6b0", "thiscall", "uint32", 2, 0, a_snd_3a6b0, &g_neg_sndExtra},
    {"snd.Midi_Device::add_to_time_past", "thiscall", "uint32", 2, 0, a_snd_af20, &g_neg_sndExtra},
    {"snd.FUN_10029e30", "thiscall", "uint32", 2, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_1002a0f0", "thiscall", "uint32", 2, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_10029e50", "thiscall", "uint32", 1, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_1002a0b0", "thiscall", "uint32", 1, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_1002a110", "thiscall", "uint32", 1, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_1000a120", "thiscall", "uint32", 2, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_1002a0d0", "thiscall", "uint32", 2, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_1000a0e0", "thiscall", "uint32", 4, 0, a_snd_ret2f, &g_neg_sndExtra},
    {"snd.Midi_Device::timer_unhook", "thiscall", "void", 1, 0, a_snd_zero<0x2c>, &g_neg_sndExtra},
    {"snd.Midi_Device::reset_time_past", "thiscall", "void", 1, 0, a_snd_zero<0x34>, &g_neg_sndExtra},
    {"snd.Midi_Device::set_rate", "thiscall", "uint32", 2, 0, a_snd_set<0x30>, &g_neg_sndExtra},
    {"snd.Midi_Device::get_rate", "thiscall", "uint32", 1, 0, a_snd_field<0x30>, &g_neg_sndExtra},
    {"snd.Midi_Device::get_time_past", "thiscall", "uint32", 1, 0, a_snd_field<0x34>, &g_neg_sndExtra},
    {"snd.Midi_Device::is_disabled", "thiscall", "uint32", 1, 0, a_snd_negbit_at<0x24, 0>, &g_neg_sndExtra},
    {"snd.Midi_Device::suspend", "thiscall", "uint32", 1, 0, a_snd_a140, &g_neg_sndExtra},
    {"snd.Patch::set_fade_in", "thiscall", "uint32", 2, 0, a_snd_5670, &g_neg_sndExtra},
    {"snd.FUN_1002b110", "thiscall", "uint32", 2, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.SubChannel::play_1002b1b0", "thiscall", "uint32", 2, 0, a_snd_2b1b0, &g_neg_sndExtra},
    {"snd.MultiSound::stop", "thiscall", "uint32", 1, 0, a_snd_3a040, &g_neg_sndExtra},
    {"snd.SubChannel::fade_1002b3a0", "thiscall", "uint32", 2, 0, a_snd_apply<4>, &g_neg_sndExtra},
    {"snd.SubChannel::fade_in_1002b430", "thiscall", "uint32", 2, 0, a_snd_apply<0x54>, &g_neg_sndExtra},
    {"snd.MultiSound::set_pan", "thiscall", "uint32", 2, 0, a_snd_3a1e0, &g_neg_sndExtra},
    {"snd.SubChannel::set_pan", "thiscall", "uint32", 2, 0, a_snd_2b4a0, &g_neg_sndExtra},
    {"snd.MultiSound::set_loop_state", "thiscall", "uint32", 2, 0, a_snd_3a690, &g_neg_sndExtra},
    {"snd.Channel::set_loop_state", "thiscall", "uint32", 2, 0, a_snd_378b0, &g_neg_sndExtra},
    {"snd.MultiSound::set_delay", "thiscall", "uint32", 2, 0, a_snd_3a1c0, &g_neg_sndExtra},
    {"snd.MultiSound::is_looping", "thiscall", "uint32", 1, 0, a_snd_negbit<2>, &g_neg_sndExtra},
    {"snd.MultiSound::is_playing", "thiscall", "uint32", 1, 0, a_snd_3a790, &g_neg_sndExtra},
    {"snd.MultiSound::is_loaded", "thiscall", "uint32", 1, 0, a_snd_negbit<1>, &g_neg_sndExtra},
    {"snd.SubChannel::get_next", "thiscall", "uint32", 1, 0, a_snd_field<0x4c>, &g_neg_sndExtra},
    {"snd.SubChannel::get_prev", "thiscall", "uint32", 1, 0, a_snd_field<0x48>, &g_neg_sndExtra},
    {"snd.MultiSound::get_time_1003a710", "thiscall", "uint32", 1, 0, a_snd_field<0x298>, &g_neg_sndExtra},
    {"snd.SubChannel::get_type", "thiscall", "uint32", 1, 0, a_snd_field<0x54>, &g_neg_sndExtra},
    {"snd.MultiSound::get_ms_length", "thiscall", "uint32", 1, 0, a_snd_field<0x140c>, &g_neg_sndExtra},
    {"snd.SubChannel::attach", "thiscall", "uint32", 1, 0, a_snd_2b010, &g_neg_sndExtra},
    {"snd.MultiSound::detach", "thiscall", "uint32", 1, 0, a_snd_3a0b0, &g_neg_sndExtra},
    {"snd.Wave::load", "thiscall", "uint32", 3, 0, a_snd_2be90, &g_neg_sndExtra},
    {"snd.FUN_1002c2b0", "thiscall", "uint32", 2, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1002c2d0", "thiscall", "uint32", 1, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1002c2f0", "thiscall", "uint32", 4, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1002c310", "thiscall", "uint32", 4, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1002c330", "thiscall", "uint32", 4, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1002c350", "thiscall", "uint32", 4, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.MultiSound::get_time", "thiscall", "uint32", 2, 0, a_snd_3a590, &g_neg_sndExtra},
    {"snd.Channel::get_time", "thiscall", "uint32", 2, 0, a_snd_37860, &g_neg_sndExtra},
    {"snd.FUN_1002c1c0", "thiscall", "uint32", 1, 0, a_snd_2c1c0, &g_neg_sndExtra},
    {"snd.FUN_1002c1f0", "thiscall", "uint32", 4, 0, a_snd_2c1f0, &g_neg_sndExtra},
    {"snd.FUN_1002b870", "thiscall", "uint32", 2, 0, a_snd_fwd1<0xd0>, &g_neg_sndExtra},
    {"snd.FUN_1002b8a0", "thiscall", "uint32", 2, 0, a_snd_fwd1<0xd4>, &g_neg_sndExtra},
    {"snd.FUN_10029290", "thiscall", "uint32", 2, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_10029620", "thiscall", "uint32", 2, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_100292b0", "thiscall", "uint32", 1, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_100295c0", "thiscall", "uint32", 1, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_10029640", "thiscall", "uint32", 1, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.FUN_100295e0", "thiscall", "uint32", 2, 0, a_snd_ret0, &g_neg_sndExtra},
    {"snd.Wave_Device::timer_unhook", "thiscall", "void", 1, 0, a_snd_e7f0, &g_neg_sndExtra},
    {"snd.Wave_Device::reset_time_past", "thiscall", "void", 1, 0, a_snd_e790, &g_neg_sndExtra},
    {"snd.Wave_Device::get_rate", "thiscall", "uint32", 1, 0, a_snd_e830, &g_neg_sndExtra},
    {"snd.Wave_Device::get_time_past", "thiscall", "uint32", 1, 0, a_snd_e7b0, &g_neg_sndExtra},
    {"snd.Wave_Device::is_disabled", "thiscall", "uint32", 1, 0, a_snd_e7d0, &g_neg_sndExtra},
    {"snd.Wave_Device::is_eax", "thiscall", "uint32", 1, 0, a_snd_e850, &g_neg_sndExtra},
    {"snd.Wave_Device::add_to_time_past", "thiscall", "uint32", 2, 0, a_snd_e1d0, &g_neg_sndExtra},
    {"snd.Wave_Device::get_ndevices", "thiscall", "uint32", 1, 0, a_snd_e200, &g_neg_sndExtra},
    {"snd.Wave_Device::is_3d", "thiscall", "uint32", 1, 0, a_snd_e5b0, &g_neg_sndExtra},
    {"snd.Wave_Device::set_listener_position", "thiscall", "uint32", 4, 0, a_snd_e490, &g_neg_sndExtra},
    {"snd.FUN_1000e2f0", "thiscall", "uint32", 2, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1000e4d0", "thiscall", "uint32", 4, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1000e4f0", "thiscall", "uint32", 2, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1000e510", "thiscall", "uint32", 2, 0, a_snd_retB, &g_neg_sndExtra},
    {"snd.FUN_1000e530", "thiscall", "uint32", 2, 0, a_snd_retB, &g_neg_sndExtra},
};

}  // namespace

// sound.dll functions this module ports that have no hooks.csv row (rva -> registered name), for the core's lookup
const char *audio_exe_sound_name(uint32_t rva) {
  for (const ExeFn &f : kSndExtra)
    if (f.rva == rva) return f.name;
  return nullptr;
}

// sg_state_free (through audio_release): the context's ExeCtx and DeviceSink go, so a context allocated later at the same
// address starts from no state (I8: test_buyland's cases inherited a freed context's exe-layer state)
void audio_exe_release(Ctx &ctx) {
  {
    std::lock_guard<std::mutex> lk(g_mu);
    table().erase(&ctx);
  }
  std::lock_guard<std::mutex> lk(g_dev_mu);
  device_sinks().erase(&ctx);
  external_models().erase(&ctx);
}

// ---- S7.10: the application's audio host (audio_exe.h) -------------------------------------------------------------------
void audio_exe_set_observer(Ctx &ctx, AudioRequestObserver obs) { ectx(ctx).observer = std::move(obs); }

std::unique_ptr<DeviceModel> make_device_model() { return std::make_unique<DeviceSink>(); }

void audio_exe_device_fixture(Ctx &ctx, AudioSink *sink, DeviceModel *model, bool prepare) {
  audio_exe_reset(ctx);
  ExeCtx &e = ectx(ctx);
  if (prepare) device_prepare(ctx);
  {
    std::lock_guard<std::mutex> lk(g_dev_mu);
    device_sinks().erase(&ctx);
    if (model) external_models()[&ctx] = model;
    else external_models().erase(&ctx);
  }
  e.sink = sink;
  audio_set_sink(ctx, sink);
}

void audio_exe_construct_holders(Ctx &ctx) { construct_holders(ctx); }

namespace {
const DefectDef kDefects[] = {
    {"sound.golfSfxAlias", "S7.12 (U-1499, G15)",
     "a course effect the exe opens as `sounds\\Golf sfx\\<file>` fails like the original's mmioOpenA on an extracted "
     "install's `Golf_Sfx` directory (the CD names it `Golf Sfx`), and so do the tracks under `sounds\\music\\Desert Music`, "
     "`Links Music`, `Tropical Music` (installed as `Desert_Music` / `Links_Music` / `Tropical_Music`); at 0 the install's "
     "file is opened (S7.13, G16)",
     &g_defect_golf_sfx},
};
}  // namespace

extern const ModuleDef kModule_audio_exe = {"audio_exe", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                            sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
