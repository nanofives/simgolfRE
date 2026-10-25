// The video module (roadmap S8.3; port/src/sim/video.cpp, re/analysis/video/skip_path.md): playAudioFile and the process-wide
// BinkPlayer g_bink, the path both Bink movies of the game take (the intro from showTitleScreen, the closing movie from
// mainLoop's QUIT block). Decision D2: no Bink decoder in the port. The movie goes through a MovieHost; with none attached
// (the default) the movie is unavailable and the module takes the original's own skip path, the one BinkPlayer::play takes
// when _BinkOpen answers NULL (the path of every pinned recording, whose shim answers NULL for the intro).
#ifndef PORTSIM_SIM_VIDEO_H
#define PORTSIM_SIM_VIDEO_H

#include <cstdint>
#include <string>

#include "portsim/registry.h"

namespace portsim {

// What _BinkOpen reports of an open movie (the BINK header words the original reads: width and height go to
// _BinkBufferOpen).
struct MovieInfo {
  uint32_t width = 0, height = 0, frames = 0;
};

// A movie decoder and presenter: what binkw32 does for BinkPlayer::play / BinkPlayer::frame / BinkPlayer::close. Each method
// names the binkw32 call it stands for. A host that plays a movie must reproduce the state effects listed in skip_path.md
// section 4 (the module makes them; the host only answers).
class MovieHost {
 public:
  virtual ~MovieHost() = default;
  // the display's window (display vtable slot +0x20, which playAudioFile hands to BinkPlayer::setTarget); 0 = no window:
  // BinkPlayer::play answers 7 and plays nothing. The port's display exists (the boot's token), so the default is 1.
  virtual uint32_t window(Ctx &) { return 1; }
  // _BinkOpen(path, 0x1000000): true = the movie is open (info filled); false = a NULL handle or a handle whose error word
  // (+0x1c) is non-zero: BinkPlayer::play answers 1 before any buffer or cursor change (the skip path)
  virtual bool open(Ctx &ctx, const std::string &path, MovieInfo &info) = 0;
  // _BinkBufferOpen(window, width, height, 0): false = a NULL buffer: play answers 1 (the handle stays open until close)
  virtual bool open_buffer(Ctx &, uint32_t /*window*/, const MovieInfo &) { return true; }
  // ShowCursor(0) before the frame loop, ShowCursor(1) after it
  virtual void show_cursor(Ctx &, bool /*show*/) {}
  // _BinkWait: true = the next frame is not due yet (the loop runs its two input pumps and asks again)
  virtual bool wait(Ctx &ctx) = 0;
  // BinkPlayer::frame (DoFrame, the buffer copy and blit, NextFrame): false = the clip has ended (the frame counters equal)
  virtual bool frame(Ctx &ctx) = 0;
  // _BinkClose of an open movie (BinkPlayer::close)
  virtual void close_movie(Ctx &) {}
  // _BinkBufferClose of an open buffer; its result is what BinkPlayer::close and playAudioFile return (1 in S8.2's runs)
  virtual int32_t close_buffer(Ctx &) { return 1; }
};

// The default host: every movie is unavailable (open answers false). The skip path.
class SkipMovieHost : public MovieHost {
 public:
  bool open(Ctx &, const std::string &, MovieInfo &) override { return false; }
  bool wait(Ctx &) override { return false; }
  bool frame(Ctx &) override { return false; }
};

void set_movie_host(Ctx &ctx, MovieHost *host);  // not owned; nullptr = the default skip host
MovieHost &movie_host(Ctx &ctx);                 // the attached host or the default SkipMovieHost

// playAudioFile(path, immediate): a path the files do not hold returns 0 at once. Otherwise the deadline word
// g_audio5aa774 = the clock + 7000 for immediate 0 (the intro: the timeGetTime read playAudioFile makes, taken from the
// context's InputSource), 0 for any other immediate (the closing movie); g_bink.target = the host's window; one input pump
// (pumpInput483bd0: g_inputPhase 0x3f, a Drain dispatch, g_inputPhase 0); the root repaint (drawing, not done);
// BinkPlayer::play(path, 0, 0, sound); BinkPlayer::close; the deadline word 0. Returns BinkPlayer::close's result.
int32_t play_audio_file(Ctx &ctx, uint32_t path, int32_t immediate);
// BinkPlayer::play (this, path, x, y, sound): 7 without a target, 1 when the host cannot open the movie or its buffer, 0 after
// the frame loop (g_ui83ab9c = 1 before it; the loop ends when the clip ends or a root-window handler cleared g_ui83ab9c).
int32_t bink_player_play(Ctx &ctx, uint32_t self, uint32_t path, int32_t x, int32_t y, uint32_t sound);
// BinkPlayer::setTarget (this, target): this+0xc = target
void bink_player_set_target(Ctx &ctx, uint32_t self, uint32_t target);
// BinkPlayer::close (this): the host closes an open movie (+4) and buffer (+8); both words 0; the buffer close's result, else 0
int32_t bink_player_close(Ctx &ctx, uint32_t self);

}  // namespace portsim

#endif  // PORTSIM_SIM_VIDEO_H
