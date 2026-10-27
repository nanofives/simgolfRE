// Real-time pacing for the Windows host (roadmap S7.3; the live loop that uses it is S7.6's).
//
// The port's frame step runs as fast as it is called: the original's frame wait, the one place its frame rate comes from,
// is dropped from the simulation (re/analysis/sim/time_model.md section 3). This module is that wait for the host, written
// from the same section, with the clock injected so it can be tested on a virtual clock:
//
//   H1 wait (mainLoop, after H1):    g_frameMs = now() - head                           (one clock read)
//                                    target = head + ((g_flags & (1 << 17)) ? 250 : 175) / 2    (+125 / +87 ms)
//                                    while (now() < target) waitTicks(1, 0)             (unsigned compare: `jae` / `jb`)
//   waitTicks(n, interruptible):     n == 0 returns 0 without reading the clock
//                                    ms = n * 1000 / 100 (32-bit signed, truncated)    (= 10 n)
//                                    start = now(); while (now() - start < ms) pump    (unsigned compare)
//
// Under a clock that advances 1 ms per read this gives the 99 reads per frame time_model.md measures (frame head 1, H1's
// two reads, 8 x 12 in the loop); in real time a frame lasts 87 ms (125 with bit 17) plus at most one waitTicks quantum,
// unless its own work took longer (then the wait is one test).
//
// The host's clock (RealClock) reads timeGetTime and, between two reads of a wait loop, sleeps until the next instant the
// loop needs (MsgWaitForMultipleObjects, so window input wakes it): the original busy-pumps there, the host does not have to.
#ifndef PORTAPP_PACING_H
#define PORTAPP_PACING_H

#include <cstdint>
#include <functional>

namespace portapp {

constexpr uint32_t kFlagSlowFrames = 1u << 17;  // g_flags bit 17: 125 ms frames (no setter in game logic, time_model.md)

// The clock the waits read. now() is timeGetTime's value; idle_until(t) is called inside a wait loop when the next read
// can only end the loop at or after t (a virtual clock does nothing; the real one sleeps).
class PaceClock {
 public:
  virtual ~PaceClock() = default;
  virtual uint32_t now() = 0;
  virtual void idle_until(uint32_t t) { (void)t; }
};

// The pump a wait calls between its clock reads (the original's pumpInput483c30); for an interruptible waitTicks it
// returns true when a key or mouse press arrived (the original returns 1 then).
using PumpFn = std::function<bool()>;

// target of the frame wait for a frame whose head read the clock at `head`
uint32_t frame_target(uint32_t head, uint32_t g_flags);
// waitTicks' duration in ms: n * 1000 / 100 with the original's 32-bit signed arithmetic
int32_t wait_ticks_ms(int32_t n);

struct WaitStats {
  uint32_t reads = 0;        // clock reads the wait made
  uint32_t wait_ticks = 0;   // waitTicks(1, 0) calls (frame wait)
  uint32_t pumps = 0;        // pump calls
  uint32_t frame_ms = 0;     // g_frameMs: now() - head at the wait's first read
  uint32_t end = 0;          // the clock value of the last read
};

// waitTicks(n, interruptible): 1 when an interruptible wait was ended by input, else 0 (as the original).
int wait_ticks(PaceClock &clock, int32_t n, bool interruptible, const PumpFn &pump, WaitStats *stats = nullptr);

// The H1 frame wait of a frame whose head read `head`; returns the statistics.
WaitStats frame_wait(PaceClock &clock, uint32_t head, uint32_t g_flags, const PumpFn &pump);

// A clock that advances 1 ms per read (the corpus' virtual clock, re/frida/js/scenario_det.js), starting at `t0`.
class VirtualClock : public PaceClock {
 public:
  explicit VirtualClock(uint32_t t0 = 0) : t_(t0) {}
  uint32_t now() override { return t_++; }
  uint32_t peek() const { return t_; }

 private:
  uint32_t t_;
};

#ifdef _WIN32
// timeGetTime with a 1 ms timer period (timeBeginPeriod while it lives). idle_until sleeps in MsgWaitForMultipleObjects
// until t or until input arrives in this thread's queue (QS_ALLINPUT), never past t.
class RealClock : public PaceClock {
 public:
  RealClock();
  ~RealClock() override;
  RealClock(const RealClock &) = delete;
  RealClock &operator=(const RealClock &) = delete;
  uint32_t now() override;
  void idle_until(uint32_t t) override;
  uint32_t sleeps() const { return sleeps_; }

 private:
  bool period_ = false;
  uint32_t sleeps_ = 0;
};
#endif

}  // namespace portapp

#endif
