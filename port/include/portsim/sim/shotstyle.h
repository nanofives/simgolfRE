// shotStyleButtonCode (phase 3.8, stream 38b; port/src/sim/shotstyle.cpp).
#ifndef PORTSIM_SIM_SHOTSTYLE_H
#define PORTSIM_SIM_SHOTSTYLE_H

#include <cstdint>

namespace portsim {

// Maps a shot-style button number to the 32-bit code mainLoop's H1 stores for it: 3 -> 0x800043f0, 4 -> 0,
// 5 -> 0x8000421f, 6 -> 0x80007ff0, 7 -> 0x80007fff, every other number (the test is unsigned) -> 0.
uint32_t shot_style_button_code(int32_t button);

}  // namespace portsim

#endif  // PORTSIM_SIM_SHOTSTYLE_H
