// The app's AudioSink (roadmap S7.10): what the shadow audio context's DSOUND / WINMM / KERNEL32 / USER32 commands become.
//
// Every command is first answered by S4's DeviceSink model (portsim::DeviceModel, audio_exe.h): heap, critical sections,
// timeSetEvent, mmio over the game files, the IDirectSound / IDirectSoundBuffer model with its storage in the context. The
// sink then acts on a BACKEND with what the engine handed the device:
//   Model    nothing else (the model's answers and log only: gate a's mode);
//   DSound   real dsound.dll buffers: CreateSoundBuffer with the engine's DSBUFFERDESC / WAVEFORMATEX (3D control dropped:
//            the model refuses the 3D interface), SetFormat of the primary buffer, the bytes of every Unlock copied from the
//            context into the real buffer at the same offset, Play / Stop / SetVolume / SetPan / SetFrequency /
//            SetCurrentPosition, SetCooperativeLevel with the app window; GetCurrentPosition / GetStatus answered from the
//            real buffer and timeGetTime / GetTickCount from the real clock;
//   Capture  a software device: the Unlocked bytes kept per buffer and the playing buffers mixed by position, frequency,
//            volume and pan into a 16-bit PCM capture at the primary buffer's format (render(ms)); GetCurrentPosition from
//            that play position, timeGetTime from the model's virtual clock.
// The sink also keeps the messages the engine posts (USER32 PostMessageA) and the window procedure it installs with
// SetWindowLongA(GWL_WNDPROC) for the audio thread to deliver (audio_out.cpp).
#ifndef PORTAPP_DS_SINK_H
#define PORTAPP_DS_SINK_H

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "portsim/sim/audio_exe.h"

namespace app {

enum class SinkMode { Model, DSound, Capture };

struct PostedMessage {
  uint32_t hwnd = 0, msg = 0, wparam = 0, lparam = 0;
};

struct SinkStats {
  uint32_t commands = 0, buffers = 0, real_buffers = 0, real_failures = 0, unlocks = 0, plays = 0, stops = 0, posted = 0;
  uint64_t bytes_copied = 0, frames_rendered = 0;
  uint32_t no_device_answers = 0;  // S7.11: device probes answered as a PC without an output device answers them
  std::string first_failure;  // the first real-backend call that failed (DSound), empty when none
};

// Test-only wrong bodies (negative controls of gate c): each changes one thing the backend does with the engine's commands.
enum class SinkDefect { None, SkipUnlockCopy, IgnorePan, IgnoreVolume, IgnoreFrequency };

// a 16-bit PCM WAV of interleaved frames (S7.11: DsSink::write_wav and a capture kept after the thread ended)
bool write_wav_file(const std::string &path, const std::vector<int16_t> &frames, uint32_t rate, uint32_t channels);

class DsSink : public portsim::AudioSink {
 public:
  // hwnd: the app window for SetCooperativeLevel (DSound; nullptr = the desktop window). window_token: the value the shadow
  // passes to initSound as its window.
  DsSink(SinkMode mode, portsim::DeviceModel &model, void *hwnd, uint32_t window_token = 1);
  ~DsSink() override;
  DsSink(const DsSink &) = delete;
  DsSink &operator=(const DsSink &) = delete;

  portsim::AudioReply import(portsim::Ctx &ctx, const portsim::AudioCommand &cmd) override;
  portsim::AudioReply com(portsim::Ctx &ctx, const portsim::AudioCommand &cmd) override;

  SinkMode mode() const { return mode_; }
  std::vector<PostedMessage> take_posted();
  uint32_t subclass_proc() const { return subclass_proc_; }   // SetWindowLongA(token, GWL_WNDPROC, proc), 0 = none
  uint32_t window_token() const { return token_; }

  // Capture: mix `ms` milliseconds of the playing buffers into the capture (and advance their positions).
  void render(uint32_t ms);
  const std::vector<int16_t> &capture() const { return cap_; }  // interleaved frames at capture_rate / capture_channels
  uint32_t capture_rate() const { return out_rate_; }
  uint32_t capture_channels() const { return out_ch_; }
  bool write_wav(const std::string &path) const;
  // One text line per backend buffer: id, format, size, flags, playing, volume, pan, frequency (a test's view).
  std::string describe() const;

  // S7.11, NO DEVICE: when the real DirectSoundCreate fails (DSound) or force_no_device is set (any mode but Model; the test
  // switch), the engine's device probes get the answers measured on a PC without an output device (scenario_baseline.md
  // section 1, log/c3/b2_soundprobe.js): IDirectSound::Initialize DSERR_NODRIVER, its Release 0, DirectSoundCreate
  // DSERR_NODRIVER, midiOutOpen MMSYSERR_ERROR, waveOutOpen MMSYSERR_BADDEVICEID, waveOutGetNumDevs 0. The engine then
  // takes the original's own device-less path (initSound's music-device open fails, freeSoundLib, g_soundReady 0).
  bool force_no_device = false;
  bool no_device() const { return no_device_; }
  SinkDefect defect = SinkDefect::None;
  SinkStats stats;

 private:
  struct Buf;
  Buf *buf_of(uint32_t obj);
  void create_buffer(portsim::Ctx &ctx, const portsim::AudioCommand &cmd);
  void unlock(portsim::Ctx &ctx, Buf &b, const portsim::AudioCommand &cmd);
  void position(portsim::Ctx &ctx, Buf &b, const portsim::AudioCommand &cmd);
  void real_init();
  uint32_t real_ms() const;

  SinkMode mode_;
  portsim::DeviceModel &model_;
  void *hwnd_;
  uint32_t token_;
  uint32_t subclass_proc_ = 0;
  uint32_t ds_obj_ = 0;            // the model's IDirectSound object
  void *real_ds_ = nullptr;        // IDirectSound * (DSound)
  bool real_tried_ = false;
  bool no_device_ = false;         // S7.11: the device probe failed (or was forced to)
  uint32_t failed_ds_ = 0;         // S7.11: the IDirectSound object whose Initialize answered DSERR_NODRIVER
  std::map<uint32_t, Buf *> bufs_;  // by the model's buffer object
  std::vector<PostedMessage> posted_;
  std::vector<int16_t> cap_;
  uint32_t out_rate_ = 22050, out_ch_ = 2;
  uint64_t rendered_ms_ = 0;
  std::chrono::steady_clock::time_point t0_;
};

}  // namespace app

#endif  // PORTAPP_DS_SINK_H
