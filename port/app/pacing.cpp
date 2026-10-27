// Real-time pacing (pacing.h): the original's H1 frame wait and waitTicks over an injected clock.
#include "pacing.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace portapp {

// negative controls of the selftest (main.cpp --negctl pacing-*): 1 bit 17 ignored, 2 the frame-wait test signed,
// 3 waitTicks' quantum 1 ms; 0 = the original's behaviour
int g_pacing_defect = 0;

uint32_t frame_target(uint32_t head, uint32_t g_flags) {
  // ((g_flags & bit 17) ? 0xfa : 0xaf) halved with the signed idiom `cdq; sub eax, edx; sar eax, 1` (both positive)
  const int32_t span = (g_flags & kFlagSlowFrames) && g_pacing_defect != 1 ? 250 : 175;
  return head + static_cast<uint32_t>(span / 2);
}

int32_t wait_ticks_ms(int32_t n) {
  // n * 1000 as a wrapping 32-bit product, then the signed division by 100 (truncation toward zero)
  const int32_t prod = static_cast<int32_t>(static_cast<uint32_t>(n) * 1000u);
  return g_pacing_defect == 3 ? prod / 1000 : prod / 100;
}

int wait_ticks(PaceClock &clock, int32_t n, bool interruptible, const PumpFn &pump, WaitStats *stats) {
  if (n == 0) return 0;
  const uint32_t ms = static_cast<uint32_t>(wait_ticks_ms(n));
  const uint32_t start = clock.now();
  uint32_t t = clock.now();
  if (stats) stats->reads += 2;
  // the first test before the loop and the loop test are both unsigned (`jae`, `jb`)
  while (t - start < ms) {
    if (stats) ++stats->pumps;
    const bool input = pump ? pump() : false;
    if (interruptible && input) {
      if (stats) stats->end = t;
      return 1;
    }
    clock.idle_until(start + ms);
    t = clock.now();
    if (stats) ++stats->reads;
  }
  if (stats) stats->end = t;
  return 0;
}

WaitStats frame_wait(PaceClock &clock, uint32_t head, uint32_t g_flags, const PumpFn &pump) {
  WaitStats s;
  s.frame_ms = clock.now() - head;
  s.reads = 1;
  const uint32_t target = frame_target(head, g_flags);
  uint32_t t = clock.now();
  ++s.reads;
  auto before = [&](uint32_t now) {  // unsigned, as the original's `jae` / `jb`
    return g_pacing_defect == 2 ? static_cast<int32_t>(now) < static_cast<int32_t>(target) : now < target;
  };
  while (before(t)) {
    ++s.wait_ticks;
    WaitStats inner;
    wait_ticks(clock, 1, false, pump, &inner);
    s.reads += inner.reads;
    s.pumps += inner.pumps;
    t = clock.now();
    ++s.reads;
  }
  s.end = t;
  return s;
}

#ifdef _WIN32
RealClock::RealClock() { period_ = timeBeginPeriod(1) == TIMERR_NOERROR; }

RealClock::~RealClock() {
  if (period_) timeEndPeriod(1);
}

uint32_t RealClock::now() { return timeGetTime(); }

void RealClock::idle_until(uint32_t t) {
  const uint32_t n = timeGetTime();
  const int32_t left = static_cast<int32_t>(t - n);
  if (left <= 0) return;
  ++sleeps_;
  MsgWaitForMultipleObjects(0, nullptr, FALSE, static_cast<DWORD>(left), QS_ALLINPUT);
}
#endif

}  // namespace portapp
