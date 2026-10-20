// Phase 3.8 integrator (stream 38f, log/p38/38f_notes.md): replacements of a trace run's recorded dispatches, for the
// gate that drives a recorded scenario through the command API instead of raw root-handler calls (re/PHASE38_PLAN.md
// section 3, gate d). Module `integrator` (port/src/sim/integrator.cpp), driven from port/test/semantic.py.
//
// A trace run (sg_run_frames) gives each pump call of the frame step the recorded dispatch of the matching pump call of
// the original (tick.cpp TraceInput): the RootHandler commands of the trace's HANDLER records, one batch per dispatch.
// An OVERRIDE replaces the batch whose first HANDLER record sits at clock ordinal `pos` of the slice with other commands
// (semantic kinds of portsim/sim/commands.h, or nothing): the pump call that took the recorded batch applies them
// instead, so a semantic command takes effect at the dispatch point of the input it stands for.
#ifndef PORTSIM_SIM_INTEGRATOR_H
#define PORTSIM_SIM_INTEGRATOR_H

#include <cstdint>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/commands.h"

namespace portsim {

// The replacement of the recorded batch at clock ordinal `pos`, or nullptr when it has none (an empty vector drops the
// batch). Counts the replacements returned (integrator.appliedCount).
const std::vector<Command> *batch_override(Ctx &ctx, uint32_t pos);

}  // namespace portsim

#endif  // PORTSIM_SIM_INTEGRATOR_H
