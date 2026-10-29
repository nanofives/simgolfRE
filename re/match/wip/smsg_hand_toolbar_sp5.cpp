// S10.12: SMSG-decomp Toolbar.cpp (unmodified source) compiled with the SP5 toolchain (see the TOOLCHAIN line below). Only the two functions
// the default toolchain misses are annotated (RenderBuildingToolbar 99.9%, RenderEmployeePanel 99.7% with 8168).
// CAUTION U-1910: golf_clean.exe's Rich header lists no build-8966 object (all 8168), so this 100% is a match under a compiler
// the original was not built with: see log/roadmap/S10/12_notes.md section 3 before using it as C4 evidence.
// TOOLCHAIN sp5
// FLAGS golf_clean.exe: /MT /O2 /DWIN32 /DNDEBUG /D_WINDOWS /GR- /Zl
// INCLUDE third_party/smsg/SMSG-decomp/golf/golf.exe/include
// INCLUDE third_party/smsg/SMSG-decomp/golf/jackal.lib/include
// INCLUDE tools/dxsdk8/include
// INCLUDE $VC
// INCLUDE third_party/smsg/SMSG-decomp/golf/jpeg.lib/jpeg-6a
// INCLUDE third_party/smsg/SMSG-decomp/terrain/Terrain.dll/include
// INCLUDE third_party/smsg/SMSG-decomp/sound/sound.dll/include
// INCLUDE third_party/smsg/SMSG-decomp/sound/lpc10.lib/lpc10/lpc10-1.5/lpc55-C
// INCLUDE third_party/smsg/SMSG-decomp/jgl/jgld.dll/include
// INCLUDE third_party/smsg/SMSG-decomp/golf/binkw32.lib
// COMPILE third_party/smsg/SMSG-decomp/golf/golf.exe/src/Toolbar.cpp

// MATCH: golf_clean.exe 0x00434cf0 ?RenderBuildingToolbar@@YAXHH@Z
// MATCH: golf_clean.exe 0x00436e50 ?RenderEmployeePanel@@YAXHH@Z
