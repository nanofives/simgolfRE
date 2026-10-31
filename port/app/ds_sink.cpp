// The app's AudioSink (ds_sink.h). Interface slots: IDirectSound 3 CreateSoundBuffer, 6 SetCooperativeLevel;
// IDirectSoundBuffer 1 AddRef, 2 Release, 4 GetCurrentPosition, 9 GetStatus, 11 Lock, 12 Play, 13 SetCurrentPosition,
// 14 SetFormat, 15 SetVolume, 16 SetPan, 17 SetFrequency, 18 Stop, 19 Unlock (dsound.h's method order). The DSBUFFERDESC the
// engine passes: +0 dwSize, +4 dwFlags, +8 dwBufferBytes, +0x10 lpwfxFormat; WAVEFORMATEX: +0 wFormatTag, +2 nChannels,
// +4 nSamplesPerSec, +0xc nBlockAlign, +0xe wBitsPerSample, +0x10 cbSize.
#include "ds_sink.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <dsound.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace app {

using portsim::AudioCommand;
using portsim::AudioReply;
using portsim::Ctx;

namespace {
constexpr uint32_t kDsIface = 1, kBufIface = 2;  // DeviceSink's interface ids (audio_exe.cpp DeviceSink::Iface)
constexpr uint32_t kFlagPrimary = 1u << 0, kFlagLoc3d = 1u << 4, kFlagLocHw = 1u << 2;
constexpr uint32_t kFlagMute3d = 1u << 17, kFlagLocDefer = 1u << 18;
constexpr uint32_t kPrevWndProc = 1u << 12;  // the game window's "previous window procedure" (a token: never called as code)

uint32_t ld32(Ctx &c, uint32_t va) { return c.mem.load<uint32_t>(va); }
uint16_t ld16(Ctx &c, uint32_t va) { return c.mem.load<uint16_t>(va); }
void st32(Ctx &c, uint32_t va, uint32_t v) { c.mem.store<uint32_t>(va, v); }
uint32_t arg(const AudioCommand &cmd, size_t i) { return i < cmd.args.size() ? cmd.args[i] : 0u; }

// copy n bytes of the context at va into dst (byte by byte across mappings when one block does not hold them)
void read_ctx(Ctx &c, uint32_t va, uint8_t *dst, uint32_t n) {
  try {
    std::memcpy(dst, c.mem.at(va, n), n);
  } catch (const portsim::Fault &) {
    for (uint32_t i = 0; i < n; ++i) dst[i] = c.mem.load<uint8_t>(va + i);
  }
}
double db_gain(int32_t hundredths) {  // DirectSound's attenuation unit: 1/100 dB, -10000 = silence
  if (hundredths <= -10000) return 0.0;
  return std::pow(10.0, static_cast<double>(hundredths) / 2000.0);
}
}  // namespace

struct DsSink::Buf {
  uint32_t obj = 0, storage = 0, size = 0, flags = 0;
  uint16_t tag = 1, channels = 1, bits = 16, block = 2;
  uint32_t rate = 22050, freq = 0;
  int32_t vol = 0, pan = 0;
  bool primary = false, playing = false, looping = false;
  double pos = 0.0;            // capture: the play position in frames
  int refs = 1;
  std::vector<uint8_t> bytes;  // capture: what the engine Unlocked
  IDirectSoundBuffer *real = nullptr;
};

DsSink::DsSink(SinkMode mode, portsim::DeviceModel &model, void *hwnd, uint32_t window_token)
    : mode_(mode), model_(model), hwnd_(hwnd), token_(window_token), t0_(std::chrono::steady_clock::now()) {}

DsSink::~DsSink() {
  for (auto &kv : bufs_) {
    if (kv.second->real) kv.second->real->Release();
    delete kv.second;
  }
  bufs_.clear();
  if (real_ds_) static_cast<IDirectSound *>(real_ds_)->Release();
}

std::vector<PostedMessage> DsSink::take_posted() {
  std::vector<PostedMessage> out;
  out.swap(posted_);
  return out;
}

DsSink::Buf *DsSink::buf_of(uint32_t obj) {
  auto it = bufs_.find(obj);
  return it == bufs_.end() ? nullptr : it->second;
}

uint32_t DsSink::real_ms() const {
  const auto d = std::chrono::steady_clock::now() - t0_;
  return 0x100000u + static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(d).count());
}

// DSound: the real device, created at the engine's first IDirectSound call (DirectSoundCreate / CoCreateInstance)
// S7.11: a failed DirectSoundCreate (or force_no_device) puts the sink in its no-device state before the engine's first
// device probe is answered (ds_sink.h)
void DsSink::real_init() {
  if (real_tried_) return;
  real_tried_ = true;
  if (force_no_device && mode_ != SinkMode::Model) {
    no_device_ = true;
    if (stats.first_failure.empty()) stats.first_failure = "DirectSoundCreate forced DSERR_NODRIVER";
    return;
  }
  if (mode_ != SinkMode::DSound) return;
  IDirectSound *ds = nullptr;
  const HRESULT hr = DirectSoundCreate(nullptr, &ds, nullptr);
  if (FAILED(hr) || ds == nullptr) {
    ++stats.real_failures;
    no_device_ = true;
    if (stats.first_failure.empty()) stats.first_failure = "DirectSoundCreate " + std::to_string(static_cast<long>(hr));
    return;
  }
  real_ds_ = ds;
}

AudioReply DsSink::import(Ctx &c, const AudioCommand &cmd) {
  ++stats.commands;
  AudioReply r = model_.import(c, cmd);
  const std::string &n = cmd.name;
  if (cmd.dll == "USER32.dll") {
    if (n == "PostMessageA") {
      posted_.push_back({arg(cmd, 0), arg(cmd, 1), arg(cmd, 2), arg(cmd, 3)});
      ++stats.posted;
    } else if (n == "SetWindowLongA" && arg(cmd, 0) == token_ && arg(cmd, 1) == 0xfffffffcu) {  // GWL_WNDPROC (-4)
      // The original subclasses the game window, whose SetWindowLongA returns the previous procedure (non-zero), and
      // SoundWindowObj_subclass keeps the window as g_soundWindow only then. The model answers 0 (S4's DeviceSink, gate a);
      // the real backends answer a previous-procedure token, which the engine hands only to CallWindowProcA (the model's)
      // and to the SetWindowLongA that restores it (U-1687).
      const uint32_t installed = subclass_proc_;
      const bool restore = installed != 0 && arg(cmd, 2) == kPrevWndProc;
      subclass_proc_ = restore ? 0u : arg(cmd, 2);
      if (mode_ != SinkMode::Model) r.eax = restore ? installed : kPrevWndProc;
    }
    return r;
  }
  if (mode_ == SinkMode::DSound && (n == "timeGetTime" || n == "GetTickCount")) r.eax = real_ms();
  const bool ds_create = (cmd.dll == "DSOUND.dll" && n == "#1") || n == "CoCreateInstance";
  const bool probe = ds_create || n == "midiOutOpen" || n == "waveOutOpen" || n == "waveOutGetNumDevs" ||
                     n == "midiOutGetNumDevs";
  if (probe) real_init();  // the first device probe decides (S7.11)
  if (ds_create) {
    const uint32_t out = n == "#1" ? arg(cmd, 1) : arg(cmd, 4);
    if (out) ds_obj_ = ld32(c, out);
  }
  if (no_device_ && probe && n != "CoCreateInstance") {  // the measured device-less answers (ds_sink.h); CoCreateInstance S_OK
    ++stats.no_device_answers;
    if (n == "#1") {
      if (arg(cmd, 1)) st32(c, arg(cmd, 1), 0);
      r.eax = static_cast<uint32_t>(DSERR_NODRIVER);
    } else if (n == "midiOutOpen") {
      r.eax = MMSYSERR_ERROR;
    } else if (n == "waveOutOpen") {
      r.eax = MMSYSERR_BADDEVICEID;
    } else {
      r.eax = 0;  // waveOutGetNumDevs / midiOutGetNumDevs
    }
  }
  return r;
}

AudioReply DsSink::com(Ctx &c, const AudioCommand &cmd) {
  ++stats.commands;
  AudioReply r = model_.com(c, cmd);
  if (cmd.iface == kDsIface) {
    real_init();
    if (no_device_ && cmd.slot == 10) {  // Initialize: DSERR_NODRIVER (measured at the call from sound.dll rva 0x11d1d)
      failed_ds_ = arg(cmd, 0);
      ++stats.no_device_answers;
      r.eax = static_cast<uint32_t>(DSERR_NODRIVER);
      return r;
    }
    if (no_device_ && cmd.slot == 2 && arg(cmd, 0) == failed_ds_) {  // its Release: 0 (measured, sound.dll rva 0x11a2a)
      ++stats.no_device_answers;
      r.eax = 0;
      return r;
    }
    if (cmd.slot == 3) {
      create_buffer(c, cmd);
    } else if (cmd.slot == 6 && real_ds_) {
      HWND w = hwnd_ ? static_cast<HWND>(hwnd_) : GetDesktopWindow();
      const HRESULT hr = static_cast<IDirectSound *>(real_ds_)->SetCooperativeLevel(w, arg(cmd, 2));
      if (FAILED(hr) && stats.first_failure.empty()) stats.first_failure = "SetCooperativeLevel " + std::to_string(hr);
    }
    return r;
  }
  if (cmd.iface != kBufIface) return r;
  Buf *b = buf_of(arg(cmd, 0));
  if (b == nullptr) return r;
  IDirectSoundBuffer *rb = b->real;
  switch (cmd.slot) {
    case 1: ++b->refs; break;
    case 2:
      if (--b->refs == 0) {
        if (rb) rb->Release();
        bufs_.erase(b->obj);
        delete b;
      }
      break;
    case 4: position(c, *b, cmd); break;
    case 9:  // GetStatus: the backend's (DSBSTATUS_PLAYING 1, DSBSTATUS_LOOPING 4)
      if (arg(cmd, 1) && mode_ != SinkMode::Model) {
        DWORD s = 0;
        if (rb) rb->GetStatus(&s);
        else if (mode_ == SinkMode::Capture) s = (b->playing ? 1u : 0u) | (b->playing && b->looping ? 4u : 0u);
        if (rb || mode_ == SinkMode::Capture) st32(c, arg(cmd, 1), s);
      }
      break;
    case 11: {  // Lock(off, bytes, *p1, *n1, *p2, *n2, flags): the model's storage address, from its first pointer
      const uint32_t off = (arg(cmd, 7) & 2u) ? 0u : (b->size ? arg(cmd, 1) % b->size : 0u);  // DSBLOCK_ENTIREBUFFER
      if (arg(cmd, 3) && b->storage == 0) b->storage = ld32(c, arg(cmd, 3)) - off;
      break;
    }
    case 12:  // Play(0, 0, flags): DSBPLAY_LOOPING 1
      b->playing = true;
      b->looping = (arg(cmd, 3) & 1u) != 0;
      ++stats.plays;
      if (rb) rb->Play(0, 0, arg(cmd, 3) & 1u);
      break;
    case 13:  // SetCurrentPosition(bytes)
      b->pos = b->block ? static_cast<double>(arg(cmd, 1) / b->block) : 0.0;
      if (rb) rb->SetCurrentPosition(arg(cmd, 1));
      break;
    case 14:  // SetFormat(wfx) (the primary buffer)
      if (arg(cmd, 1)) {
        const uint32_t w = arg(cmd, 1);
        b->channels = ld16(c, w + 2);
        b->rate = ld32(c, w + 4);
        b->block = ld16(c, w + 0xc);
        b->bits = ld16(c, w + 0xe);
        if (b->primary) {
          out_rate_ = b->rate ? b->rate : out_rate_;
          out_ch_ = b->channels == 1 ? 1u : 2u;
        }
        if (rb) {
          WAVEFORMATEX f{};
          read_ctx(c, w, reinterpret_cast<uint8_t *>(&f), 16);
          f.cbSize = 0;
          const HRESULT hr = rb->SetFormat(&f);
          if (FAILED(hr) && stats.first_failure.empty()) stats.first_failure = "SetFormat " + std::to_string(hr);
        }
      }
      break;
    case 15:
      b->vol = static_cast<int32_t>(arg(cmd, 1));
      if (rb) rb->SetVolume(static_cast<LONG>(b->vol));
      break;
    case 16:
      b->pan = static_cast<int32_t>(arg(cmd, 1));
      if (rb) rb->SetPan(static_cast<LONG>(b->pan));
      break;
    case 17:
      b->freq = arg(cmd, 1);
      if (rb) rb->SetFrequency(b->freq);
      break;
    case 18:
      b->playing = false;
      ++stats.stops;
      if (rb) rb->Stop();
      break;
    case 19: unlock(c, *b, cmd); break;
    default: break;
  }
  return r;
}

// CreateSoundBuffer(this, desc, *out, unk): the model wrote its buffer object to *out
void DsSink::create_buffer(Ctx &c, const AudioCommand &cmd) {
  const uint32_t desc = arg(cmd, 1), out = arg(cmd, 2);
  if (!desc || !out) return;
  auto *b = new Buf;
  b->obj = ld32(c, out);
  b->flags = ld32(c, desc + 4);
  b->size = ld32(c, desc + 8);
  b->primary = (b->flags & kFlagPrimary) != 0;
  const uint32_t w = ld32(c, desc + 0x10);
  WAVEFORMATEX f{};
  if (w) {
    read_ctx(c, w, reinterpret_cast<uint8_t *>(&f), 16);
    f.cbSize = 0;
    b->tag = f.wFormatTag;
    b->channels = f.nChannels;
    b->rate = f.nSamplesPerSec;
    b->block = f.nBlockAlign;
    b->bits = f.wBitsPerSample;
  }
  if (!b->primary) b->bytes.assign(mode_ == SinkMode::Capture ? b->size : 0u, 0);
  ++stats.buffers;
  if (real_ds_) {
    DSBUFFERDESC d{};
    d.dwSize = sizeof d;
    d.dwFlags = b->flags & ~(kFlagLoc3d | kFlagLocHw | kFlagMute3d | kFlagLocDefer);  // no 3D (the model refuses it)
    d.dwBufferBytes = b->primary ? 0u : b->size;
    d.lpwfxFormat = (b->primary || !w) ? nullptr : &f;
    IDirectSoundBuffer *rb = nullptr;
    const HRESULT hr = static_cast<IDirectSound *>(real_ds_)->CreateSoundBuffer(&d, &rb, nullptr);
    if (SUCCEEDED(hr) && rb) {
      b->real = rb;
      ++stats.real_buffers;
    } else {
      ++stats.real_failures;
      if (stats.first_failure.empty()) stats.first_failure = "CreateSoundBuffer " + std::to_string(hr);
    }
  }
  if (Buf *old = buf_of(b->obj)) {  // the model reuses a released object's address only through its own heap reuse
    if (old->real) old->real->Release();
    delete old;
  }
  bufs_[b->obj] = b;
}

// Unlock(this, p1, n1, p2, n2): the bytes the engine wrote between Lock and Unlock, at their offsets in the buffer
void DsSink::unlock(Ctx &c, Buf &b, const AudioCommand &cmd) {
  ++stats.unlocks;
  if (defect == SinkDefect::SkipUnlockCopy || b.storage == 0) return;
  std::vector<uint8_t> tmp;
  for (int part = 0; part < 2; ++part) {
    const uint32_t p = arg(cmd, 1 + 2 * part), n = arg(cmd, 2 + 2 * part);
    if (!p || !n || p < b.storage || p - b.storage > b.size) continue;
    const uint32_t off = p - b.storage, k = std::min(n, b.size - off);
    tmp.resize(k);
    read_ctx(c, p, tmp.data(), k);
    stats.bytes_copied += k;
    if (!b.bytes.empty()) std::memcpy(b.bytes.data() + off, tmp.data(), k);
    if (b.real) {
      void *a1 = nullptr, *a2 = nullptr;
      DWORD n1 = 0, n2 = 0;
      HRESULT hr = b.real->Lock(off, k, &a1, &n1, &a2, &n2, 0);
      if (hr == DSERR_BUFFERLOST) {
        b.real->Restore();
        hr = b.real->Lock(off, k, &a1, &n1, &a2, &n2, 0);
      }
      if (SUCCEEDED(hr)) {
        if (a1) std::memcpy(a1, tmp.data(), std::min<DWORD>(n1, k));
        if (a2 && k > n1) std::memcpy(a2, tmp.data() + n1, std::min<DWORD>(n2, k - n1));
        b.real->Unlock(a1, n1, a2, n2);
      } else if (stats.first_failure.empty()) {
        stats.first_failure = "Lock " + std::to_string(hr);
      }
    }
  }
}

// GetCurrentPosition(this, *play, *write): DSound from the real buffer, Capture from the software play position (the write
// cursor 10 ms of frames ahead, U-1688), Model the model's
void DsSink::position(Ctx &c, Buf &b, const AudioCommand &cmd) {
  if (mode_ == SinkMode::Model) return;
  DWORD play = 0, write = 0;
  if (b.real) {
    if (FAILED(b.real->GetCurrentPosition(&play, &write))) return;
  } else if (mode_ == SinkMode::Capture && b.size && b.block) {
    play = (static_cast<uint32_t>(b.pos) * b.block) % b.size;
    const uint32_t rate = b.freq ? b.freq : b.rate;
    write = (play + b.block * ((rate * 10u + 999u) / 1000u)) % b.size;
  } else {
    return;
  }
  if (arg(cmd, 1)) st32(c, arg(cmd, 1), play);
  if (arg(cmd, 2)) st32(c, arg(cmd, 2), write);
}

// Capture: DirectSound's mix of the playing secondary buffers, as the model of U-1689: each buffer's frames read at its
// position (linear interpolation between neighbours), stepped by frequency / output rate, scaled by 10^(volume / 2000) and
// by the pan's attenuation of the other side (pan > 0 lowers the left, < 0 the right, by 10^(-|pan| / 2000)), summed and
// clipped to 16 bits; a buffer that is not looping stops at its end, its position back at 0.
void DsSink::render(uint32_t ms) {
  const size_t frames = static_cast<size_t>(static_cast<uint64_t>(out_rate_) * (rendered_ms_ + ms) / 1000u -
                                            static_cast<uint64_t>(out_rate_) * rendered_ms_ / 1000u);  // no drift
  rendered_ms_ += ms;
  const size_t base = cap_.size();
  cap_.resize(base + frames * out_ch_, 0);
  std::vector<double> acc(frames * 2, 0.0);
  for (auto &kv : bufs_) {
    Buf &b = *kv.second;
    if (b.primary || !b.playing || b.bytes.empty() || b.block == 0 || (b.bits != 8 && b.bits != 16)) continue;
    const uint32_t total = b.size / b.block;
    if (total == 0) continue;
    const uint32_t rate = (b.freq && defect != SinkDefect::IgnoreFrequency) ? b.freq : b.rate;
    const double step = static_cast<double>(rate) / out_rate_;
    const double g = defect == SinkDefect::IgnoreVolume ? 1.0 : db_gain(b.vol);
    const int32_t pan = defect == SinkDefect::IgnorePan ? 0 : b.pan;
    const double gl = g * (pan > 0 ? db_gain(-pan) : 1.0), gr = g * (pan < 0 ? db_gain(pan) : 1.0);
    auto sample = [&](uint32_t f, uint32_t ch) -> double {
      const uint32_t chn = std::min<uint32_t>(ch, b.channels - 1u);
      const size_t at = static_cast<size_t>(f) * b.block + chn * (b.bits / 8u);
      if (b.bits == 8) return (static_cast<double>(b.bytes[at]) - 128.0) * 256.0;
      return static_cast<double>(static_cast<int16_t>(b.bytes[at] | (b.bytes[at + 1] << 8)));
    };
    for (size_t j = 0; j < frames && b.playing; ++j) {
      const uint32_t f0 = static_cast<uint32_t>(b.pos);
      const double t = b.pos - f0;
      uint32_t f1 = f0 + 1;
      if (f1 >= total) f1 = b.looping ? 0u : f0;
      const double l = sample(f0, 0) * (1.0 - t) + sample(f1, 0) * t;
      const double r = sample(f0, 1) * (1.0 - t) + sample(f1, 1) * t;
      acc[2 * j] += l * gl;
      acc[2 * j + 1] += r * gr;
      b.pos += step;
      if (b.pos >= total) {
        if (b.looping) {
          b.pos -= total;
        } else {
          b.playing = false;
          b.pos = 0.0;
        }
      }
    }
  }
  for (size_t j = 0; j < frames; ++j)
    for (uint32_t ch = 0; ch < out_ch_; ++ch) {
      const double v = out_ch_ == 1 ? (acc[2 * j] + acc[2 * j + 1]) / 2.0 : acc[2 * j + ch];
      cap_[base + j * out_ch_ + ch] = static_cast<int16_t>(std::clamp(std::lround(v), -32768L, 32767L));
    }
  stats.frames_rendered += frames;
}

bool DsSink::write_wav(const std::string &path) const { return write_wav_file(path, cap_, out_rate_, out_ch_); }

// S7.11: also used for a capture kept after the audio thread ended (AudioOut::write_capture)
bool write_wav_file(const std::string &path, const std::vector<int16_t> &cap_, uint32_t out_rate_, uint32_t out_ch_) {
  std::FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const uint32_t data = static_cast<uint32_t>(cap_.size() * 2), riff = 36 + data;
  const uint32_t rate = out_rate_, ch = out_ch_, bps = rate * ch * 2, fmt_len = 16;
  const uint16_t pcm = 1, chs = static_cast<uint16_t>(ch), block = static_cast<uint16_t>(ch * 2), bits = 16;
  std::fwrite("RIFF", 1, 4, f);
  std::fwrite(&riff, 4, 1, f);
  std::fwrite("WAVEfmt ", 1, 8, f);
  std::fwrite(&fmt_len, 4, 1, f);
  std::fwrite(&pcm, 2, 1, f);
  std::fwrite(&chs, 2, 1, f);
  std::fwrite(&rate, 4, 1, f);
  std::fwrite(&bps, 4, 1, f);
  std::fwrite(&block, 2, 1, f);
  std::fwrite(&bits, 2, 1, f);
  std::fwrite("data", 1, 4, f);
  std::fwrite(&data, 4, 1, f);
  if (!cap_.empty()) std::fwrite(cap_.data(), 2, cap_.size(), f);
  return std::fclose(f) == 0;
}

std::string DsSink::describe() const {
  std::string s;
  char t[200];
  for (const auto &kv : bufs_) {
    const Buf &b = *kv.second;
    std::snprintf(t, sizeof t, "buf 0x%08x %s fmt %u/%u/%u/%u size %u flags 0x%x playing %d loop %d vol %d pan %d freq %u real %d\n",
                  b.obj, b.primary ? "primary" : "secondary", b.tag, b.channels, b.rate, b.bits, b.size, b.flags,
                  b.playing ? 1 : 0, b.looping ? 1 : 0, b.vol, b.pan, b.freq, b.real ? 1 : 0);
    s += t;
  }
  return s;
}

}  // namespace app
