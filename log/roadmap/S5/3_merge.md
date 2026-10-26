# S5.3 merge lines

## 1. port/src/sim/modules.inc
Append one line at the end (after S5.1's `SG_MODULE(fttext)` and S5.2's `SG_MODULE(textlab)`):

```
SG_MODULE(fttext_raster)
```

Then touch port/src/registry.cpp (MSBuild does not rebuild it when only modules.inc changes).

Why: port/test/test_static.py::test_every_module_file_is_listed wants every port/src/sim/*.cpp listed with its kModule_<stem>;
fttext_raster.cpp defines kModule_fttext_raster, whose one entry `fttext_raster.blend5` is the raster's test entry
(port/test/test_fttext_raster.py). The raster itself runs through module fttext's FtTextMetrics::draw_glyphs.

## 2. Nothing else
- fttext_raster.cpp is picked up by CMake's src/sim/*.cpp glob and compiles with the PORTSIM_HAVE_FREETYPE definition and the
  Freetype::Freetype link S5.1's block gives the target (1_merge.md item 1); without FreeType it compiles to its stub branch
  (port/build/S5_3_noft builds).
- The step needs the lines S5.1 and S5.2 asked for: `SG_MODULE(fttext)` (1_merge.md item 2), `SG_MODULE(textlab)` (2_merge.md),
  1_cmake_block.txt appended to port/CMakeLists.txt. log/roadmap/S5/3_build.py applies exactly those plus item 1 to its mirror.

The step's own files are in the shared tree (3_files.txt): fttext_raster.{cpp,h}, the draw_glyphs wiring in fttext.cpp, the
fttext.h comment, port/test/test_fttext_raster.py (marker `port`; skips without fttext / fttext_raster / textlab / gditext,
FreeType, the local label corpus, or a ClearType host at contrast 1200).
