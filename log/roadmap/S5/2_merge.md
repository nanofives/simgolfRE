# S5.2 merge lines

## port/src/sim/modules.inc
Append one line at the end:

```
SG_MODULE(textlab)
```

Then touch port/src/registry.cpp (MSBuild does not rebuild it when only modules.inc changes).

## Dependency on S5.1's gditext refactor
port/src/sim/textlab.cpp includes portsim/sim/gditext.h and defines `class GdiRecorder : public GdiTextMetrics` (default
constructor, `draw_glyphs` overridden and forwarded to `GdiTextMetrics::draw_glyphs`). It needs, after S5.1's refactor:
- `portsim::GdiTextMetrics` still a public, default-constructible class whose `draw_glyphs` is virtual and not `final`;
- gditext's registered entries (Text_put, drawMarkupRun, measureTextWidth) still find the provider with
  `dynamic_cast<GdiTextMetrics *>` (or by any rule a GdiTextMetrics subclass satisfies).
If S5.1 moves the shared chain into a base class that both providers derive from, change GdiRecorder's base to that class
(one line) or let the recorder wrap it; textlab.recordStart picks the recorder by dynamic_cast (textlab.cpp
a_record_start). No other gditext symbol is used.

No CMake change (src/sim/*.cpp is globbed). No shared Python file changed.
