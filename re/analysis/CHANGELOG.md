# CHANGELOG (newest first)

One line per confidence change, written only by `scripts/re_classify.py`. Never edit or truncate history;
add a correcting entry instead.

<!-- ENTRIES -->
2026-10-06  0049bec0  netListRemove  subsystem util->net  re/names/exe_5.tsv
2026-10-06  0049b970  netHeapAlloc  subsystem util->net  re/names/exe_5.tsv
2026-10-06  0049b7b0  netPoll  subsystem util->net  re/names/exe_5.tsv
2026-10-06  0049b690  netQueueMessage  subsystem util->net  re/names/exe_5.tsv
2026-10-06  0049acf0  netHandleControl  subsystem util->net  re/names/exe_5.tsv
2026-10-06  0049ab40  netServiceControl  subsystem util->net  re/names/exe_5.tsv
2026-10-06  0049aa70  netPump  subsystem util->net  re/names/exe_5.tsv
2026-10-06  0049aa30  netNameOf  subsystem util->net  re/names/exe_5.tsv
2026-10-06  00499140  netSendMessages  subsystem util->net  re/names/exe_5.tsv
2026-10-06  00497fc0  netService  subsystem util->net  re/names/exe_5.tsv
2026-10-06  00497d10  netSendShutdown  subsystem util->net  re/names/exe_5.tsv
2026-10-06  00497cc0  netFlush  subsystem util->net  re/names/exe_5.tsv
2026-10-06  00497c70  netTimerReadB  subsystem util->net  re/names/exe_5.tsv
2026-10-06  00497c20  netTimerReadA  subsystem util->net  re/names/exe_5.tsv
2026-10-06  00497b40  netSendList  subsystem util->net  re/names/exe_5.tsv
2026-10-06  0047ae80  setFocusItem  C0->C1  writes DAT_0083ab98; calls item vtable +0xf0
2026-10-06  0047abe0  View_attach  C0->C1  match re/match/golf_hand_r2.cpp
2026-10-06  0047a4c0  layoutViewTree  C0->C1  string 0x004e42f0; string 0x004e4304; callee 0x00479f30 (layoutNode)
2026-10-06  0047a3c0  View_dtor  C0->C1  match re/match/golf_hand_r0.cpp; virtual dtor
2026-10-06  00479f30  layoutNode  C0->C1  callee 0x00479f30 (self); callee 0x0047b080 (contains@Node47b)
2026-10-06  00479c40  View_ctor  C0->C1  installs vtables 0x4ba8b4 and 0x4baa10
2026-10-06  00479bb0  Vec_release  C0->C1  match re/match/golf_classes.cpp; field +0x224 buffer, +0x228 refcount; callee _free
2026-10-06  00479a80  Surface_clip  C0->C1  match re/match/golf_hand_05.cpp; tagRECT
2026-10-06  00479950  drawPixelRun  C0->C1  callee 0x004796a0 (Surface_pixelPtr)
2026-10-06  004798c0  blendGreen  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  00479830  blendRed  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  004796a0  Surface_pixelPtr  C0->C1  match re/match/golf_hand_r0.cpp
2026-10-06  004791b0  drawComposite  C0->C1  callee 0x00475e10/0x00475fb0/0x00476140
2026-10-06  00478f50  openImageAsset  C0->C1  string 0x004e4234; string 0x004e42b0; string 0x004e42b8
2026-10-06  00478ea0  drawLineV  C0->C1  callee 0x004796a0 (Surface_pixelPtr)
2026-10-06  00478df0  drawLineH  C0->C1  callee 0x004796a0 (Surface_pixelPtr)
2026-10-06  00478cd0  loadImageData  C0->C1  match re/match/golf_hand_r3.cpp; string 0x004e4218
2026-10-06  00478af0  Win_fillRect  C0->C1  match re/match/golf_small6.cpp
2026-10-06  00478a90  addDropdownSpot  C0->C1  match re/match/golf_raw_13.cpp; string 0x004e428c
2026-10-06  00478a70  Widget_reset  C0->C1  match re/match/golf_small2.cpp
2026-10-06  00478a20  List_reset  C0->C1  match re/match/golf_small7.cpp
2026-10-06  00478970  Text_flush  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  004787a0  drawTextBlock  C0->C1  callee 0x00476ef0; callee 0x00477c30 (drawText)
2026-10-06  00478610  measureText4  C0->C1  callee 0x00476ef0; callee 0x00477250 (Text_put)
2026-10-06  00478530  measureText3  C0->C1  callee 0x00476ef0; callee 0x00477da0 (drawCentered2)
2026-10-06  00478430  measureText2  C0->C1  callee 0x00476ef0 (measureMarkupRun)
2026-10-06  004782d0  measureText  C0->C1  callee 0x00476ef0 (measureMarkupRun); callee 0x00477c30 (drawText)
2026-10-06  004781f0  drawRight2  C0->C1  match re/match/golf_hand_07_4781f0.cpp
2026-10-06  00478140  drawString  C0->C1  callee 0x00477280 (drawMarkupRun)
2026-10-06  00478080  drawRight  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  00477fc0  drawStringLeft  C0->C1  callee 0x00477280 (drawMarkupRun)
2026-10-06  00477eb0  drawCentered3  C0->C1  match re/match/golf_hand_08_e.cpp
2026-10-06  00477e60  drawBox  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00477da0  drawCentered2  C0->C1  match re/match/golf_hand_r2.cpp
2026-10-06  00477cd0  drawCentered  C0->C1  match re/match/golf_hand_r1.cpp; char* + tagRECT
2026-10-06  00477c30  drawText  C0->C1  callee 0x004775b0 (drawRichText)
2026-10-06  004775b0  drawRichText  C0->C1  string 0x004e4254; 8 drawCentered/drawRight callers
2026-10-06  00477580  Widget_value  C0->C1  match re/match/golf_small2.cpp; 23 callers
2026-10-06  00477560  Widget_get  C0->C1  match re/match/golf_hand_00.cpp
2026-10-06  00477280  drawMarkupRun  C0->C1  string 0x004e423c
2026-10-06  00477250  Text_put  C0->C1  match re/match/golf_small4.cpp
2026-10-06  00476ef0  measureMarkupRun  C0->C1  string 0x004e423c; callee 0x00476d80 (skipToken)
2026-10-06  00476e20  wrapText  C0->C1  match re/match/golf_hand_r0.cpp
2026-10-06  00476dd0  scanToken  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  00476d80  skipToken  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  004767a0  allocTextBuffer  C0->C1  caller 0x004775b0 (drawRichText); callee _malloc
2026-10-06  00476750  drawStringAt2  C0->C1  callee 0x00478140 (drawString)
2026-10-06  00476700  drawStringAt  C0->C1  callee 0x00477fc0 (drawStringLeft)
2026-10-06  004766a0  Widget_draw2  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  00476650  Widget_draw  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  00476310  Widget_setQuad  C0->C1  match re/match/golf_small2.cpp; 40 callers
2026-10-06  004762d0  setDrawTarget  C0->C1  writes this+0x5c/0x60/0x64/0x68
2026-10-06  00476140  blitRight  C0->C1  caller 0x004791b0 (drawComposite); callee 0x00473e60 (Image_draw)
2026-10-06  00475fb0  blitMid  C0->C1  caller 0x004791b0 (drawComposite)
2026-10-06  00475e10  blitLeft  C0->C1  caller 0x004791b0 (drawComposite); callee 0x00475b00
2026-10-06  00475da0  Surface_fill  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  00475d00  Surface_blit3  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  00475c90  Surface_fillRegion  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  00475c60  Surface_blitThunk  C0->C1  callee 0x00475d00 (Surface_blit3)
2026-10-06  00475c20  DC_calla0  C0->C1  match re/match/golf_hand_01.cpp; calls DC vtable +0xa0
2026-10-06  00475be0  DC_calla4  C0->C1  match re/match/golf_hand_01.cpp; calls DC vtable +0xa4
2026-10-06  00475ba0  DC_call90  C0->C1  match re/match/golf_hand_01.cpp; calls DC vtable +0x90
2026-10-06  00475b60  DC_call84  C0->C1  match re/match/golf_hand_01.cpp; calls DC vtable +0x84
2026-10-06  00475b20  Surface_box  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00475840  loadImage  C0->C1  string 0x004e4234; callee 0x00478cd0 (loadImageData); 22 callers
2026-10-06  00475580  saveBmp  C0->C1  caller 0x00407e00; callee _fwrite
2026-10-06  00475410  flipBmpRows  C0->C1  match re/match/golf_hand_r3.cpp; __stdcall; tagBITMAPINFOHEADER
2026-10-06  00475210  loadBmpSurface  C0->C1  caller 0x0046e810 (showBulletinBoard); callee 0x00474ee0 (loadBmp); callee 0x00475410 (flipBmpRows)
2026-10-06  00475050  reloadBmp  C0->C1  caller 0x00475210 (loadBmpSurface)
2026-10-06  00474ee0  loadBmp  C0->C1  match re/match/golf_hand_r2.cpp; __stdcall; tagBITMAPINFO
2026-10-06  00474e70  Surface_paint  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  00474dd0  Surface_createFull  C0->C1  match re/match/golf_hand_04.cpp; 7-arg create
2026-10-06  00474c40  Image_dtor  C0->C1  match re/match/golf_hand_r0.cpp; virtual dtor
2026-10-06  00474ae0  Image_ctor  C0->C1  match re/match/golf_hand_09_a.cpp; __fastcall ctor
2026-10-06  00474860  heapAlloc  C0->C1  match re/match/golf_hand_s0_k.cpp; string 0x004e41b0
2026-10-06  00474820  Buf_alloc  C0->C1  match re/match/golf_small6.cpp
2026-10-06  00474650  Surface_get  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  004745c0  Surface_init2  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  00474550  Surface_create  C0->C1  match re/match/golf_hand_03.cpp; __fastcall
2026-10-06  00474440  Surface_drawSprite  C0->C1  calls backing-surface vtable +0x94
2026-10-06  00474380  Surface_draw  C0->C1  match re/match/golf_hand_06_j.cpp
2026-10-06  00474260  Sprite_drawScaled  C0->C1  match re/match/golf_hand_08_h.cpp
2026-10-06  004741b0  Sprite_draw  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  004740f0  Surface_blit2  C0->C1  match re/match/golf_hand_06_j.cpp
2026-10-06  00474030  Surface_drawScaled2  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  00473fc0  Surface_blitMasked  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  00473f60  Surface_stretchBlit  C0->C1  calls backing-surface vtable +0x54
2026-10-06  00473e60  Image_draw  C0->C1  match re/match/golf_hand_r1.cpp; 40+ callers
2026-10-06  00473df0  Surface_drawRegion  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  00473cb0  Surface_drawScaled  C0->C1  match re/match/golf_hand_09_e.cpp
2026-10-06  00473c60  Surface_blitTakeDirty  C0->C1  callee 0x00473bf0 (Surface_blit)
2026-10-06  00473bf0  Surface_blit  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  00473b50  Surface_open  C0->C1  match re/match/golf_hand_04.cpp; __thiscall(char* name)
2026-10-06  00473ae0  Surface_init  C0->C1  match re/match/golf_raw_11.cpp
2026-10-06  00473ab0  Surface_ctor  C0->C1  match re/match/golf_hand_00.cpp; __fastcall ctor
2026-10-06  00473a70  initGlobalImage  C0->C1  match re/match/golf_raw_02.cpp; callee 0x00474ae0 (Image_ctor); operates on DAT_00839348
2026-10-06  00473470  drawScoreSummary  C0->C1  string 0x004e4148; string 0x004e4170; caller 0x0044cff0 (showEndOfYearScreen)
2026-10-06  004732d0  addScore  C0->C1  match re/match/golf_hand_r1.cpp; string 0x004d6098
2026-10-06  004724c0  listThemes  C0->C1  match re/match/golf_hand_07_4724c0.cpp; string 0x004c3c5c
2026-10-06  00470a60  applyTerrainEdit  C0->C1  callee 0x00402970 (spawnWalker); callee 0x0042f7a0 (r0_rebuild42f7a0); callee 0x00470a10 (clearTile)
2026-10-06  00470a10  clearTile  C0->C1  match re/match/golf_small8.cpp
2026-10-06  0046f550  courseActionMenu  C0->C1  string 0x004c3f5c; string 0x004e3ebc; callee 0x0040b4a0 (s0_saveGame40b4a0); callee 0x0046f1d0 (tierPrice)
2026-10-06  0046f2b0  seedCourseTable  C0->C1  caller 0x0046f550 (courseActionMenu); callee 0x0045c1e0 (range@Random); writes DAT_00571fd8
2026-10-06  0046f1d0  tierPrice  C0->C1  match re/match/golf_small15.cpp
2026-10-06  0046e810  showBulletinBoard  C0->C1  string 0x004e3e58; string 0x004e3e7c; caller 0x0040f5c0
2026-10-06  0046e7b0  postEvent  C0->C1  match re/match/golf_hand_02.cpp; caller 0x00460df0 (announceHoleType)
2026-10-06  0046e260  drawScrollText  C0->C1  caller 0x0046dea0 (runScrollText); callee 0x00477250 (Text_put)
2026-10-06  0046dea0  runScrollText  C0->C1  caller 0x0046de70 (scrollTextWindow); callee 0x0046e260 (drawScrollText); callee 0x0045bf80 (waitTicks)
2026-10-06  0046de70  scrollTextWindow  C0->C1  match re/match/golf_raw_13.cpp; callee 0x0046dea0 (runScrollText)
2026-10-06  0046ddd0  startNewCourse  C0->C1  callee 0x0046c970 (loadGolfPro); writes _g_cash_div100, g_date, g_flags
2026-10-06  0046d6e0  showPopup  C0->C1  callee 0x0040cc00 (S2_panel40cc00); callee 0x0045bf80 (waitTicks); callee 0x0045c030 (pumpMessages); 11 callers
2026-10-06  0046d200  buildTournamentPrep  C0->C1  string 0x004e3d54; string 0x004e3cc0
2026-10-06  0046d0c0  resetGolferState  C0->C1  match re/match/golf_hand_09_d.cpp; string 0x004d6088
2026-10-06  0046d040  initGolferState  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  0046c970  loadGolfPro  C0->C1  string 0x004c593c; string 0x004c5948; caller 0x0046ddd0 (startNewCourse)
2026-10-06  0046c940  typeBit7Clear  C0->C1  match re/match/golf_util.cpp; 15 callers
2026-10-06  00469b00  buildGolferComment  C0->C1  string 0x004d3954; string 0x004e2ae0; callee 0x00469a20 (appendOpinion); callee 0x0045b7c0 (replaceInText)
2026-10-06  00469a20  appendOpinion  C0->C1  match re/match/golf_small27.cpp; string 0x004e28e4
2026-10-06  00467a00  emitGolferThought  C0->C1  string 0x004e1bd4; string 0x004e1c30; callee 0x00467a00 (self); callee 0x0040cb00 (startMessage)
2026-10-06  004676e0  buildGolferName  C0->C1  string 0x004e1b80; string 0x004e1bac
2026-10-06  00467600  pickGolferName  C0->C1  reads DAT_0058587a; reads PTR_s_Chuck_004c148c
2026-10-06  004675d0  thoughtFlag  C0->C1  match re/match/golf_small2.cpp
2026-10-06  00467560  appendEndearment  C0->C1  match re/match/golf_hand_02.cpp; string 0x004e1b5c; string 0x004e1b68
2026-10-06  004674c0  bilinearSample  C0->C1  caller 0x0042dba0 (sample42dba0); reads DAT_00838c1c
2026-10-06  004673e0  genNoise  C0->C1  match re/match/golf_hand_06_p.cpp
2026-10-06  004672d0  angleFixed  C0->C1  returns 16.16 quadrant angles
2026-10-06  004672b0  absDiff  C0->C1  match re/match/golf_small.cpp
2026-10-06  00467270  foldRange  C0->C1  match re/match/golf_small4.cpp
2026-10-06  004671a0  direction8  C0->C1  returns constants 0..7
2026-10-06  00467170  approxDistance  C0->C1  match re/match/golf_small3.cpp; 19 callers
2026-10-06  00467150  sign  C0->C1  match re/match/golf_util.cpp
2026-10-06  00467130  clamp  C0->C1  match re/match/golf_util.cpp; 44 callers
2026-10-06  00467110  seedFromTime  C0->C1  match re/match/golf_small.cpp
2026-10-06  00466b70  loadStoryStrings  C0->C1  string 0x004c6c10; string 0x004c84e8; callee _fgets
2026-10-06  004669f0  computeStoryReply  C0->C1  caller 0x004668f0 (storyText); callee 0x00466b70 (loadStoryStrings); note re/analysis/golfers/004669f0_story_reply.md
2026-10-06  004668f0  storyText  C0->C1  match re/match/golf_story.cpp; callee 0x004669f0 (computeStoryReply); note re/analysis/golfers
2026-10-06  00466370  advanceStory  C0->C1  callee 0x004668f0 (storyText); callee 0x004722c0 (announceLandmark); note CLAUDE.md scenario golfer_stories
2026-10-06  004659a0  loadGolferRoster  C0->C1  string 0x004e1848; string 0x004e188c; caller 0x004658b0 (loadThemes); callee 0x0043d2a0 (listFiles43d2a0)
2026-10-06  004658b0  loadThemes  C0->C1  match re/match/golf_hand_07_4658b0.cpp; string 0x004c84e8; callee 0x004659a0 (loadGolferRoster)
2026-10-06  00463180  buildSceneSprites  C0->C1  caller 0x0040f5c0; callee 0x0043d6f0 (cellAt); callee 0x004628d0 (addSprite)
2026-10-06  00463100  renderSpriteQueue  C0->C1  caller 0x0040f5c0; callee 0x00462be0 (drawQueuedSprite); reads DAT_0082c160, DAT_008361f4
2026-10-06  00462be0  drawQueuedSprite  C0->C1  caller 0x00463100 (renderSpriteQueue); reads DAT_0082815c; callee 0x004740f0 (Surface_blit2)
2026-10-06  00462a30  enqueueSprite  C0->C1  writes DAT_00838200, DAT_00830164, DAT_00831164, DAT_0082415c; reads DAT_004c2844
2026-10-06  004628d0  addSprite  C0->C1  match re/match/golf_hand_r0.cpp; callee 0x00462a30 (enqueueSprite)
2026-10-06  00462800  snapshotTables  C0->C1  match re/match/golf_small22.cpp
2026-10-06  004627d0  resetSpriteQueue  C0->C1  caller 0x0040f5c0; writes DAT_00838200, DAT_0082c160, DAT_008371f4
2026-10-06  00462020  formatGolferTitle  C0->C1  callee 0x0046c940 (typeBit7Clear); reads DAT_0057956e
2026-10-06  00461830  build16BitColorTable  C0->C1  callee 0x00461810 (brightenComponent); reads DAT_0083ad50; writes DAT_00824148
2026-10-06  00461810  brightenComponent  C0->C1  match re/match/golf_small.cpp
2026-10-06  004616f0  spreadAllShade  C0->C1  match re/match/golf_hand_07_4616f0.cpp; caller 0x0045baf0; callee 0x004615f0 (spreadShade)
2026-10-06  004615f0  spreadShade  C0->C1  caller 0x004616f0 (spreadAllShade); callee 0x00467170 (approxDistance); writes DAT_00822da0, DAT_0082376c
2026-10-06  00461110  drawMatchScoreboard  C0->C1  string 0x004d55b0; string 0x004c88a8; caller 0x004362f0; callee 0x00477250 (Text_put)
2026-10-06  00460df0  announceHoleType  C0->C1  string 0x004d5538; caller 0x0042dea0; callee 0x0046e7b0 (post46e7b0)
2026-10-06  004604f0  showTutorialText  C0->C1  string 0x004d49d0; callee 0x0040b4a0 (s0_saveGame40b4a0); callee 0x0040cb00 (startMessage)
2026-10-06  0045fd80  buildScenarioIntro  C0->C1  match re/match/golf_hand_t2.cpp; string 0x004d44d4; string 0x004d4324
2026-10-06  0045f870  showCourseRanking  C0->C1  string 0x004d3ca4; string 0x004d3c54
2026-10-06  0045f0f0  skillPointDialog  C0->C1  string 0x004d3bf4; string 0x004d3c3c
2026-10-06  0045de80  announceVipGolfer  C0->C1  string 0x004d3a14; string 0x004c6fa4; callee 0x00467a00 (emitGolferThought)
2026-10-06  0045de30  swapTableEntry  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00485260  Snd::ctor485260  C0->C1  Ghidra decompilation of 0x00485260 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  00484a40  Snd::configure  C0->C1  Ghidra decompilation of 0x00484a40 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  004840e0  createSound4840e0  C0->C1  Ghidra decompilation of 0x004840e0 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  00482b90  Flic::createStream  C0->C1  Ghidra decompilation of 0x00482b90 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  00482b60  Flic::ctorBase482b60  C0->C1  Ghidra decompilation of 0x00482b60 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  004827d0  decodeSprite4827d0  C0->C1  Ghidra decompilation of 0x004827d0 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  00482570  decodeImageChunks  C0->C1  Ghidra decompilation of 0x00482570 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  00482420  Image::step  C0->C1  Ghidra decompilation of 0x00482420 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  00481ca0  Flic::buildFrameTable  C0->C1  Ghidra decompilation of 0x00481ca0 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  004810d0  drawPanel4810d0  C0->C1  Ghidra decompilation of 0x004810d0 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  00480c20  Window::refreshRect  C0->C1  Ghidra decompilation of 0x00480c20 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  00480b00  fillWindowRect  C0->C1  Ghidra decompilation of 0x00480b00 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  004804a0  View4804a0::ctor  C0->C1  Ghidra decompilation of 0x004804a0 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047fab0  compositeScreen  C0->C1  Ghidra decompilation of 0x0047fab0 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047fa30  redrawWindow  C0->C1  Ghidra decompilation of 0x0047fa30 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047f8e0  redrawDrawList  C0->C1  Ghidra decompilation of 0x0047f8e0 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047f340  hitTestTree  C0->C1  Ghidra decompilation of 0x0047f340 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047f2f0  currentFocusOwner  C0->C1  Ghidra decompilation of 0x0047f2f0 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047e450  rebuildDrawList  C0->C1  Ghidra decompilation of 0x0047e450 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047d610  buildControlStrip47d610  C0->C1  Ghidra decompilation of 0x0047d610 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047d570  Window::layoutScrollbars  C0->C1  Ghidra decompilation of 0x0047d570 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  0047be40  Window::mouseEvent238  C0->C1  Ghidra decompilation of 0x0047be40 (re/tools/decomp.py); callers/callees from re/tools/xref.py
2026-10-06  Terrain.dll:0000f7f0  Tile::drawWalls  C0->C1  match re/match/terrain_walls.cpp
2026-10-06  Terrain.dll:0000ab30  Terrain::tileHit  C0->C1  match re/match/terrain_tilehit.cpp
2026-10-06  Terrain.dll:000153c0  Tile::getFaces  C0->C1  match re/match/terrain_tile6.cpp
2026-10-06  Terrain.dll:0000c3d0  Tile::~Tile  C0->C1  match re/match/terrain_tile6.cpp
2026-10-06  Terrain.dll:0000c210  Tile::Tile  C0->C1  match re/match/terrain_tile6.cpp
2026-10-06  Terrain.dll:0000f750  WallInfo::WallInfo  C0->C1  match re/match/terrain_tile6.cpp
2026-10-06  Terrain.dll:0000f690  PathInfo::PathInfo  C0->C1  match re/match/terrain_tile6.cpp
2026-10-06  Terrain.dll:00001fe0  Faces::~Faces  C0->C1  match re/match/terrain_tile6.cpp
2026-10-06  Terrain.dll:00001fa0  Faces::Faces  C0->C1  match re/match/terrain_tile6.cpp
2026-10-06  Terrain.dll:00037c80  normalize  C0->C1  match re/match/terrain_tile5.cpp
2026-10-06  Terrain.dll:0000a130  Terrain::buildArrays  C0->C1  match re/match/terrain_tile5.cpp
2026-10-06  Terrain.dll:000033e0  Terrain::initGL  C0->C1  match re/match/terrain_tile4.cpp
2026-10-06  Terrain.dll:000380a0  Terrain::reloadTextures  C0->C1  match re/match/terrain_tile4.cpp
2026-10-06  Terrain.dll:00012cf0  Tile::calcNormals  C0->C1  match re/match/terrain_tile4.cpp
2026-10-06  Terrain.dll:0000f7a0  WallInfo::clear  C0->C1  match re/match/terrain_tile4.cpp
2026-10-06  Terrain.dll:0000f6e0  PathInfo::clear  C0->C1  match re/match/terrain_tile4.cpp
2026-10-06  Terrain.dll:0000c2c0  Tile::reset  C0->C1  match re/match/terrain_tile4.cpp
2026-10-06  Terrain.dll:000037e0  Terrain::initLists  C0->C1  match re/match/terrain_tile3.cpp
2026-10-06  Terrain.dll:00003980  Terrain::rebuild  C0->C1  match re/match/terrain_tile3.cpp
2026-10-06  Terrain.dll:0000adc0  Terrain::setViewAngle  C0->C1  match re/match/terrain_tile3.cpp
2026-10-06  Terrain.dll:00013400  Tile::layPath  C0->C1  match re/match/terrain_tile3.cpp
2026-10-06  Terrain.dll:00015500  Tile::maxHeight  C0->C1  match re/match/terrain_tile3.cpp
2026-10-06  Terrain.dll:00003270  Terrain::normalArray  C0->C1  match re/match/terrain_tile2.cpp
2026-10-06  Terrain.dll:000032b0  Terrain::vertexArray  C0->C1  match re/match/terrain_tile2.cpp
2026-10-06  Terrain.dll:00013ff0  Tile::normalArray  C0->C1  match re/match/terrain_tile2.cpp
2026-10-06  Terrain.dll:00013fc0  Tile::vertexArray  C0->C1  match re/match/terrain_tile2.cpp
2026-10-06  Terrain.dll:00014020  Tile::setTypeId  C0->C1  match re/match/terrain_tile2.cpp
2026-10-06  Terrain.dll:00015400  Tile::setWall  C0->C1  match re/match/terrain_tile.cpp
2026-10-06  Terrain.dll:00013320  Tile::hasPath  C0->C1  match re/match/terrain_tile.cpp
2026-10-06  Terrain.dll:00002f80  Tile::setVariation  C0->C1  match re/match/terrain_tile.cpp
2026-10-06  Terrain.dll:00015380  Tile::setRotation  C0->C1  match re/match/terrain_tile.cpp
2026-10-06  Terrain.dll:00015340  Tile::getVariation  C0->C1  match re/match/terrain_tile.cpp
2026-10-06  Terrain.dll:00015460  Tile::isHidden  C0->C1  match re/match/terrain_tile.cpp
2026-10-06  Terrain.dll:00006810  Tile::getY  C0->C1  match re/match/terrain_tile.cpp
2026-10-06  Terrain.dll:00005960  Tile::getX  C0->C1  match re/match/terrain_tile.cpp
2026-10-06  Terrain.dll:000080e0  Terrain::renderTile  C0->C1  match re/match/terrain_tga.cpp
2026-10-06  Terrain.dll:0000be30  LoadTGA  C0->C1  match re/match/terrain_tga.cpp
2026-10-06  Terrain.dll:000076e0  Terrain::relight  C0->C1  match re/match/terrain_textures.cpp
2026-10-06  Terrain.dll:00009ed0  Terrain::resize  C0->C1  match re/match/terrain_system.cpp
2026-10-06  Terrain.dll:00009c80  Terrain::initSystem  C0->C1  match re/match/terrain_system.cpp
2026-10-06  Terrain.dll:00009270  Terrain::stripRender  C0->C1  match re/match/terrain_strip.cpp
2026-10-06  Terrain.dll:00005230  Terrain::drawBezierSpline  C0->C1  match re/match/terrain_splines.cpp
2026-10-06  Terrain.dll:00004c70  Terrain::drawCardinalSpline  C0->C1  match re/match/terrain_splines.cpp
2026-10-06  Terrain.dll:00011ef0  Tile::smoothNormals  C0->C1  match re/match/terrain_smooth.cpp
2026-10-06  Terrain.dll:00002900  Tile::minHeight  C0->C1  match re/match/terrain_small4.cpp
2026-10-06  Terrain.dll:0000c560  initGrid  C0->C1  match re/match/terrain_small4.cpp
2026-10-06  Terrain.dll:00003830  Terrain::light1Off  C0->C1  match re/match/terrain_small3.cpp
2026-10-06  Terrain.dll:00005090  Terrain::hermitePoint  C0->C1  match re/match/terrain_small3.cpp
2026-10-06  Terrain.dll:00001c00  flipVertical  C0->C1  match re/match/terrain_small3.cpp
2026-10-06  Terrain.dll:00011d60  Terrain::faceNormal  C0->C1  match re/match/terrain_small3.cpp
2026-10-06  Terrain.dll:00015650  Tile::edgeKind  C0->C1  match re/match/terrain_small3.cpp
2026-10-06  Terrain.dll:00002750  Tile::setNormal  C0->C1  match re/match/terrain_small3.cpp
2026-10-06  Terrain.dll:00002640  Tile::setHeight  C0->C1  match re/match/terrain_small3.cpp
2026-10-06  Terrain.dll:00003090  Terrain::~Terrain  C0->C1  match re/match/terrain_small3.cpp
2026-10-06  Terrain.dll:00013f00  Terrain::flagCode  C0->C1  match re/match/terrain_small2.cpp
2026-10-06  Terrain.dll:00013dd0  Terrain::idClass  C0->C1  match re/match/terrain_small2.cpp
2026-10-06  Terrain.dll:000155b0  Tile::isOpenType  C0->C1  match re/match/terrain_small2.cpp
2026-10-06  Terrain.dll:00001b60  swapRedBlue  C0->C1  match re/match/terrain_small2.cpp
2026-10-06  Terrain.dll:00009ba0  Terrain::closeSystem  C0->C1  match re/match/terrain_small2.cpp
2026-10-06  Terrain.dll:00005750  Terrain::bernstein  C0->C1  match re/match/terrain_small2.cpp
2026-10-06  Terrain.dll:000058a0  factorial  C0->C1  match re/match/terrain_small2.cpp
2026-10-06  Terrain.dll:00005840  powf2  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:00002010  roundHalf  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:00012e70  setTextureId  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:000154a0  Tile::isSolidType  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:00001880  degToRad  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:0000a4b0  Terrain::updatePath  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:0000a450  Terrain::hasConnectedPath  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:0000c520  Tile::setNeighbour  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:000133a0  Tile::setConnected  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:00013360  Tile::isConnected  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:00001e40  Tile::getCorner  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:00001ed0  Tile::getWall  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:00001f60  Tile::getType  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:0000a840  Terrain::setSplineHeight  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:000042a0  Terrain::drawCircle  C0->C1  match re/match/terrain_small1.cpp
2026-10-06  Terrain.dll:0000ea30  Tile::drawSkirts  C0->C1  match re/match/terrain_skirts.cpp
2026-10-06  Terrain.dll:00005990  Terrain::render  C0->C1  match re/match/terrain_render.cpp
2026-10-06  Terrain.dll:000108f0  Tile::drawPaths  C0->C1  match re/match/terrain_paths.cpp
2026-10-06  Terrain.dll:00006410  Terrain::pathUpdateRender  C0->C1  match re/match/terrain_pathrender.cpp
2026-10-06  Terrain.dll:0000aa10  Terrain::resetTerrain  C0->C1  match re/match/terrain_methods2.cpp
2026-10-06  Terrain.dll:0000a740  Terrain::calcAllNormals  C0->C1  match re/match/terrain_methods2.cpp
2026-10-06  Terrain.dll:0000a880  Terrain::passCollarInfo  C0->C1  match re/match/terrain_methods2.cpp
2026-10-06  Terrain.dll:000032f0  Terrain::setType  C0->C1  match re/match/terrain_methods2.cpp
2026-10-06  Terrain.dll:0000a970  Terrain::initTerrain  C0->C1  match re/match/terrain_methods2.cpp
2026-10-06  Terrain.dll:00001af0  Terrain::loadNewCourseType  C0->C1  match re/match/terrain_methods2.cpp
2026-10-06  Terrain.dll:0000a680  Terrain::lowerEdgeCorner  C0->C1  match re/match/terrain_methods.cpp
2026-10-06  Terrain.dll:0000a3e0  Terrain::layPath  C0->C1  match re/match/terrain_methods.cpp
2026-10-06  Terrain.dll:0000a560  Terrain::setWall  C0->C1  match re/match/terrain_methods.cpp
2026-10-06  Terrain.dll:0000a620  Terrain::lowerCorner  C0->C1  match re/match/terrain_methods.cpp
2026-10-06  Terrain.dll:0000a5c0  Terrain::elevateCorner  C0->C1  match re/match/terrain_methods.cpp
2026-10-06  Terrain.dll:0000a6f0  Terrain::calcNormals  C0->C1  match re/match/terrain_methods.cpp
2026-10-06  Terrain.dll:00003390  Terrain::getVariation  C0->C1  match re/match/terrain_methods.cpp
2026-10-06  Terrain.dll:0000a510  Terrain::hasPath  C0->C1  match re/match/terrain_methods.cpp
2026-10-06  Terrain.dll:0000bba0  std::_Lockit::~_Lockit  C0->C1  match re/match/terrain_lockit.cpp
2026-10-06  Terrain.dll:0000bb70  std::_Lockit::_Lockit  C0->C1  match re/match/terrain_lockit.cpp
2026-10-06  Terrain.dll:000089e0  Terrain::localRender  C0->C1  match re/match/terrain_localrender.cpp
2026-10-06  Terrain.dll:0000bb40  std::_Destroy  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000bae0  std::_Allocate  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000bab0  operator new  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000ba40  std::_Construct  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b9f0  std::allocator<Tile*>::destroy  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b9b0  std::reverse_bidirectional_iterator<std::list<Tile*>::const_iterator>::ctor  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b960  std::allocator<Tile*>::deallocate  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b910  std::allocator<Tile*>::_Charalloc  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b800  std::list<Tile*>::erase_1000b800  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b7a0  std::list<Tile*>::const_iterator::operator++  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b750  std::list<Tile*>::iterator::iterator  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b710  std::list<Tile*>::const_iterator::_Mynode  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b6e0  std::list<Tile*>::const_iterator::const_iterator  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b690  std::allocator<Tile*>::construct  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b660  std::list<Tile*>::_Acc::_Value  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b630  std::list<Tile*>::_Acc::_Prev  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b600  std::list<Tile*>::_Acc::_Next  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b5b0  std::list<Tile*>::_Freenode  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b500  std::list<Tile*>::_Buynode  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b470  std::list<Tile*>::erase  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b430  std::list<Tile*>::size  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b3d0  std::list<Tile*>::const_iterator::operator!=  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b380  std::list<Tile*>::const_iterator::operator==  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b320  std::list<Tile*>::iterator::operator++  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b2d0  std::list<Tile*>::const_iterator::operator*  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b280  std::reverse_bidirectional_iterator<std::list<Tile*>::iterator>::ctor  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b210  std::list<Tile*>::clear  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b100  std::list<Tile*>::insert  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b090  std::list<Tile*>::push_back  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000b040  std::list<Tile*>::empty  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000afe0  std::list<Tile*>::end  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000af70  std::list<Tile*>::begin  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000aed0  std::list<Tile*>::~list  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:0000ae60  std::list<Tile*>::list  C0->C1  match re/match/terrain_list.cpp
2026-10-06  Terrain.dll:00006dd0  Terrain::loadLighting  C0->C1  match re/match/terrain_lighting.cpp
2026-10-06  Terrain.dll:00008fc0  Terrain::setZoomLevel  C0->C1  match re/match/terrain_large1.cpp
2026-10-06  Terrain.dll:00003540  Terrain::changeLighting  C0->C1  match re/match/terrain_large1.cpp
2026-10-06  Terrain.dll:00003a50  rotateAxis  C0->C1  match re/match/terrain_large1.cpp
2026-10-06  Terrain.dll:0000bbd0  textureLoad  C0->C1  match re/match/terrain_large1.cpp
2026-10-06  Terrain.dll:00010590  istream::getline  C0->C1  match re/match/terrain_iostream.cpp
2026-10-06  Terrain.dll:00037930  ios::unlock  C0->C1  match re/match/terrain_iostream.cpp
2026-10-06  Terrain.dll:000046a0  ifstream::{vbase dtor}  C0->C1  match re/match/terrain_iostream.cpp
2026-10-06  Terrain.dll:00003f80  ios::lockptr  C0->C1  match re/match/terrain_iostream.cpp
2026-10-06  Terrain.dll:00002510  ios::fail  C0->C1  match re/match/terrain_iostream.cpp
2026-10-06  Terrain.dll:000018b0  ios::lock  C0->C1  match re/match/terrain_iostream.cpp
2026-10-06  Terrain.dll:000031a0  Terrain::getInstance  C0->C1  match re/match/terrain_instance.cpp
2026-10-06  Terrain.dll:000158b0  iabs  C0->C1  match re/match/terrain_iabs.cpp
2026-10-06  Terrain.dll:00002060  Faces::build  C0->C1  match re/match/terrain_faces.cpp
2026-10-06  Terrain.dll:00012ec0  Tile::setFaceTexture  C0->C1  match re/match/terrain_face.cpp
2026-10-06  Terrain.dll:00038900  Terrain::lowerEdge  C0->C1  match re/match/terrain_edge.cpp
2026-10-06  Terrain.dll:000381a0  Terrain::drawTile  C0->C1  match re/match/terrain_drawtile.cpp
2026-10-06  Terrain.dll:000048a0  Terrain::drawLine  C0->C1  match re/match/terrain_drawline.cpp
2026-10-06  Terrain.dll:00007380  Terrain::drawTileObjects  C0->C1  match re/match/terrain_draw.cpp
2026-10-06  Terrain.dll:0000e6c0  Tile::render  C0->C1  match re/match/terrain_draw.cpp
2026-10-06  Terrain.dll:00006850  Terrain::isCulled  C0->C1  match re/match/terrain_culled.cpp
2026-10-06  Terrain.dll:00002ae0  Terrain::Terrain  C0->C1  match re/match/terrain_ctor.cpp
2026-10-06  Terrain.dll:000116d0  Tile::drawCornerOverlays  C0->C1  match re/match/terrain_corneroverlay.cpp
2026-10-06  Terrain.dll:0000ccc0  Tile::lowerCorner  C0->C1  match re/match/terrain_corner.cpp
2026-10-06  Terrain.dll:0000c7b0  Tile::elevateCorner  C0->C1  match re/match/terrain_corner.cpp
2026-10-06  Terrain.dll:00001900  SaveDIBitmap  C0->C1  match re/match/terrain_bitmap.cpp
2026-10-06  Terrain.dll:000016a0  LoadDIBitmap  C0->C1  match re/match/terrain_bitmap.cpp
2026-10-06  Terrain.dll:00001f10  Terrain::getType  C0->C1  match re/match/terrain.cpp
2026-10-06  Terrain.dll:00001e80  Terrain::getWall  C0->C1  match re/match/terrain.cpp
2026-10-06  Terrain.dll:00001de0  Terrain::getElevation  C0->C1  match re/match/terrain.cpp
2026-10-06  sound.dll:000414f0  formatUnsignedDecimal  C0->C1  string 0x1005eccc
2026-10-06  sound.dll:0004751d  crtLockInit  C0->C1  import InitializeCriticalSection, import EnterCriticalSection
2026-10-06  sound.dll:00047148  crtCommitPages  C0->C1  import VirtualAlloc
2026-10-06  sound.dll:00047771  crtStartupIo  C0->C1  import GetStartupInfoA, import GetStdHandle, import GetFileType, string 0x1005f1b4
2026-10-06  sound.dll:0004a158  crtWideCharConvert  C0->C1  import WideCharToMultiByte, import SetFilePointer
2026-10-06  sound.dll:00049604  crtValidatePointer  C0->C1  import IsBadReadPtr, import IsBadWritePtr
2026-10-06  sound.dll:00045d10  crtHeapInit  C0->C1  import HeapCreate, import HeapDestroy
2026-10-06  sound.dll:00045bc8  crtHeapSelect  C0->C1  import GetEnvironmentVariableA, import GetVersionExA
2026-10-06  sound.dll:00045181  crtExitProcess  C0->C1  import ExitProcess, import TerminateProcess, import GetCurrentProcess
2026-10-06  sound.dll:000459c3  crtExceptionFilter  C0->C1  import UnhandledExceptionFilter
2026-10-06  sound.dll:000455eb  crtMsize  C0->C1  import HeapSize
2026-10-06  sound.dll:00045252  crtRealloc  C0->C1  import HeapAlloc, import HeapReAlloc
2026-10-06  sound.dll:00042cc9  crtFree  C0->C1  import HeapFree
2026-10-06  sound.dll:00042bd1  crtMalloc  C0->C1  import HeapAlloc
2026-10-06  sound.dll:00042a8e  crtEndThread  C0->C1  import ExitThread, import CloseHandle
2026-10-06  sound.dll:00042690  crtBeginThread  C0->C1  import CreateThread, import ResumeThread, import TlsSetValue
2026-10-06  sound.dll:0002fc10  createBlankVoice  C0->C1  string 0x10063e98
2026-10-06  sound.dll:0002f220  buildVoiceTxName  C0->C1  string 0x10063e8c
2026-10-06  sound.dll:0002ed50  buildVoiceRxName  C0->C1  string 0x10063e80
2026-10-06  sound.dll:0002c5e0  destroyMixerVoices  C0->C1  import timeGetTime, vtable 0x1005bcb4
2026-10-06  sound.dll:0002baa0  getPlaybackTime  C0->C1  import timeGetTime
2026-10-06  sound.dll:0002b7c0  startDevicePlayback  C0->C1  import timeGetTime, callee 0x1002bd80
2026-10-06  sound.dll:0002ffd0  updateSoundTimer  C0->C1  import timeGetTime
2026-10-06  sound.dll:0002ffa0  markSoundTime  C0->C1  import timeGetTime
2026-10-06  sound.dll:0002f680  stopEngineVoices  C0->C1  import MessageBoxA
2026-10-06  sound.dll:00037920  flushMmioBuffer  C0->C1  import mmioAdvance, import mmioSeek, import mmioSetBuffer
2026-10-06  sound.dll:00035ca0  closeMmioSound  C0->C1  import mmioClose
2026-10-06  sound.dll:00030ba0  streamWaveSamples  C0->C1  import mmioClose
2026-10-06  sound.dll:000316c0  loadWaveResource  C0->C1  import mmioOpenA, import mmioClose, import mmioGetInfo, import mmioSetInfo
2026-10-06  sound.dll:00030840  openMmioSound  C0->C1  import mmioOpenA, import mmioGetInfo, import mmioSetInfo
2026-10-06  sound.dll:000326a0  parseRiffWave  C0->C1  import mmioOpenA, import mmioDescend, string 0x10063ee0, string 0x10063ed8, string 0x10063ed0, string 0x10063eb8
2026-10-06  sound.dll:00038f60  WaveInDevice_queryCaps  C0->C1  import waveInGetDevCapsA
2026-10-06  sound.dll:00038f10  WaveInDevice_reset  C0->C1  import waveInReset, import waveInStop, import waveInGetNumDevs
2026-10-06  sound.dll:00038da0  WaveInDevice_addBuffer  C0->C1  import waveInAddBuffer
2026-10-06  sound.dll:00038b20  WaveInDevice_start  C0->C1  import waveInStart
2026-10-06  sound.dll:00038a00  WaveInDevice_stop  C0->C1  import waveInStop, import waveInReset
2026-10-06  sound.dll:000393d0  WaveInDevice_requeueBuffer  C0->C1  import waveInPrepareHeader, import waveInAddBuffer, import waveInUnprepareHeader
2026-10-06  sound.dll:00039140  WaveInDevice_close  C0->C1  import waveInClose, import waveInStop, import waveInReset
2026-10-06  sound.dll:00039050  WaveInDevice_open  C0->C1  import waveInOpen
2026-10-06  sound.dll:00038650  WaveInDevice_ctor  C0->C1  import waveInGetNumDevs, import waveInGetDevCapsA, vtable 0x1005c604
2026-10-06  sound.dll:00038fd0  onWaveInCallbackMessage  C0->C1  import PostMessageA, callee 0x1002aa90 (postSoundThreadMessage)
2026-10-06  sound.dll:0002aa90  postSoundThreadMessage  C0->C1  import PostThreadMessageA, import OutputDebugStringA, string 0x10063e68
2026-10-06  sound.dll:0002a970  soundThreadPump  C0->C1  import MsgWaitForMultipleObjects, import PeekMessageA, callee 0x100288a0 (soundWindowProc)
2026-10-06  sound.dll:0002a810  stopSignalThread  C0->C1  import SetEvent, import Sleep, import EnterCriticalSection
2026-10-06  sound.dll:0002a920  startSignalThread  C0->C1  import ResetEvent, import SetThreadPriority
2026-10-06  sound.dll:0002a7c0  initSignalQueue  C0->C1  import InitializeCriticalSection, import CreateEventA
2026-10-06  sound.dll:00028b70  soundWindowSubclassProc  C0->C1  import CallWindowProcA
2026-10-06  sound.dll:000288a0  soundWindowProc  C0->C1  import DefWindowProcA, caller 0x1002a970 (soundThreadPump)
2026-10-06  sound.dll:00028630  destroySoundWindowObj  C0->C1  import SetWindowLongA, vtable 0x1005b8ac
2026-10-06  sound.dll:00028740  createSoundWindow  C0->C1  import RegisterClassA, import CreateWindowExA, string 0x10063e10
2026-10-06  sound.dll:00025190  mixRenderRatedC  C0->C1  __cdecl; reads rate +0x1e8,+0x1f8,+0x220 and ring +0x1358; writes short* output
2026-10-06  sound.dll:00014820  mixRenderRatedB  C0->C1  __cdecl; reads 16.16 rate at +0x1e8,+0x1f8,+0x220 (0x10000=unity); delegates to 0x10014250 at unity
2026-10-06  sound.dll:00013890  mixRenderA  C0->C1  __cdecl(out,in,count,voice,mode); reads voice +0x8,+0x98,+0x9c,+0xd8; mode 1 vs loop branch
2026-10-06  sound.dll:0000eda0  Wave_In_Device::op2  C0->C1  reads g_100b4a04; callee 0x10039470
2026-10-06  sound.dll:0000ed70  Wave_In_Device::op1  C0->C1  reads g_100b4a04; callee 0x10039450
2026-10-06  sound.dll:0001bc50  CtrlObject::ctor  C0->C1  sets vtable 0x1005b19c; tag (string in the binary) (0x6c727463) at +4; size 0x18; guard 0x44444444; caller 0x10005440
2026-10-06  sound.dll:00008840  pairClear  C0->C1  callers 0x1000a330,0x1000a670 and others
2026-10-06  sound.dll:00008810  pairStore  C0->C1  callers 0x100097d0,0x1002fb70
2026-10-06  sound.dll:000087f0  listNode3Init  C0->C1  callers 0x1000a240 (CommandQueue::push),0x1000cd80
2026-10-06  sound.dll:000088c0  CommandQueue::at  C0->C1  caller 0x1000cd80; returns this[(i & 0xfff)*4]
2026-10-06  sound.dll:00008890  CommandQueue::clear  C0->C1  caller 0x10009cb0; zeroes 0x1000 dwords + 2 fields
2026-10-06  sound.dll:00008860  CommandQueue::init  C0->C1  caller 0x10009a90 (Dll_Midi_Device::create_device); zeroes 0x1000 dwords + 2 fields
2026-10-06  sound.dll:0000a240  CommandQueue::push  C0->C1  thiscall; callers 0x100097d0 (delete_sound),0x10020be0; ring head +0x4054 mask 0xfff, array +0x50
2026-10-06  sound.dll:000213d0  Midi_Device::setChannelVolume  C0->C1  thiscall; caller 0x10020870 (Midi_Device::playEvent); tests event byte 0x07 (MIDI volume CC)
2026-10-06  sound.dll:00021bf0  Midi_Device::handleMetaEvent  C0->C1  thiscall; switches on 0x2f (MTrk end-of-track); callee 0x1001e710 (varlen)
2026-10-06  sound.dll:0001f8c0  Voice::update  C0->C1  thiscall; caller 0x1000a050 (Device::serviceVoices); reads +0x58 flags,+0x720 double, virtual +0x134
2026-10-06  sound.dll:0000a050  Device::serviceVoices  C0->C1  caller is the timer client; callees 0x1000a670 (dispatchCommand),0x1001f8c0 (Voice::update)
2026-10-06  sound.dll:0000ea00  Dll_Wave_In_Device::delete_device  C0->C1  export 0x100021fd (Dll_Wave_In_Device::delete_device)
2026-10-06  sound.dll:0000b1a0  Dll_Wave_Device::delete_device  C0->C1  export 0x100011c2 (Dll_Wave_Device::delete_device)
2026-10-06  sound.dll:00009bb0  Dll_Midi_Device::delete_device  C0->C1  export 0x10001a5f (Dll_Midi_Device::delete_device)
2026-10-06  sound.dll:0000cd80  Wave_Device::dispatchCommand  C0->C1  import PostMessageA; caller 0x1000c230; data g_100b5038
2026-10-06  sound.dll:0000a670  Midi_Device::dispatchCommand  C0->C1  import PostMessageA; caller 0x1000a050; data g_100b5038,g_100b4a20
2026-10-06  sound.dll:00020b60  postMidiNotification  C0->C1  import PostMessageA; caller 0x1000a670; data g_100b5038
2026-10-06  sound.dll:0000dbe0  postNotification  C0->C1  import PostMessageA; callers 0x10008470,0x1001ac40; data g_100b5038
2026-10-06  sound.dll:0000b1d0  Wave_Device::ctor  C0->C1  caller 0x1000b050 (Dll_Wave_Device::create_device); writes g_100b49f0
2026-10-06  sound.dll:0000b960  Wave_Device::stop  C0->C1  callee 0x10011290 (DirectSoundDevice::close); reads g_100b49f0
2026-10-06  sound.dll:0000c020  Wave_Device::restart  C0->C1  callees 0x10011c60 (DirectSoundDevice::create),0x10012d10; reads g_100b49f0
2026-10-06  sound.dll:0000baa0  Wave_Device::start  C0->C1  callees 0x10011c60 (DirectSoundDevice::create),0x10012d10,0x10012fe0; reads g_100b49f0
2026-10-06  sound.dll:00012fe0  DirectSoundDevice::createBufferFmt  C0->C1  callers 0x1000baa0,0x10032130; uses g_100b4a34/g_100b4a38
2026-10-06  sound.dll:00012d10  DirectSoundDevice::createBuffer  C0->C1  callers 0x1000baa0,0x1000c020; uses g_100b4a34/g_100b4a38
2026-10-06  sound.dll:000114e0  DirectSoundDevice::enumerate  C0->C1  callee 0x1003a978 (DirectSoundEnumerateA); caller 0x1000baa0
2026-10-06  sound.dll:00011a80  DirectSoundDevice::stopAll  C0->C1  caller 0x1000be00
2026-10-06  sound.dll:00011970  DirectSoundDevice::releaseAll  C0->C1  callers 0x1000baa0,0x1000c020; releases interface via vtbl+8
2026-10-06  sound.dll:00011290  DirectSoundDevice::close  C0->C1  import CoUninitialize; callers 0x1000b960,0x1000baa0
2026-10-06  sound.dll:00011c60  DirectSoundDevice::create  C0->C1  import CoInitialize,CoCreateInstance; data 0x1005d67c (CLSID),0x1005d5dc (IID)
2026-10-06  sound.dll:00007d60  writeWaveHeaderC  C0->C1  import mmioWrite
2026-10-06  sound.dll:00007cc0  writeWaveHeaderB  C0->C1  import mmioWrite
2026-10-06  sound.dll:00007c30  writeWaveHeaderA  C0->C1  import mmioWrite
2026-10-06  sound.dll:00008070  writeWaveChunkB  C0->C1  import mmioWrite; caller 0x100075e0 (writeWaveFileB)
2026-10-06  sound.dll:00007df0  writeWaveChunk  C0->C1  import mmioWrite; caller 0x10007010 (writeWaveFile)
2026-10-06  sound.dll:000075e0  writeWaveFileB  C0->C1  import mmioWrite,timeGetTime; caller 0x1000c230; callees 0x10008070,0x100064e0
2026-10-06  sound.dll:00007010  writeWaveFile  C0->C1  import mmioWrite,timeGetTime; caller 0x1000c230; callees 0x10007df0,0x100064e0
2026-10-06  sound.dll:00008770  closeMmioHandle  C0->C1  import mmioClose
2026-10-06  sound.dll:000086f0  openMmioFile  C0->C1  import mmioOpenA; caller 0x1000b110
2026-10-06  sound.dll:00006330  closeMmioNodeList  C0->C1  import mmioClose
2026-10-06  sound.dll:00006850  Wave_Device::appendBuffer  C0->C1  import mmioClose; validates arg == this+0x6c
2026-10-06  sound.dll:00006160  WaveStream::destroy  C0->C1  import mmioClose; caller 0x10006130; vtable 0x1005b1c0
2026-10-06  sound.dll:000064e0  Wave_Device::freeBufferList  C0->C1  import mmioClose; callers 0x10007010,0x100075e0
2026-10-06  sound.dll:00005440  Wave_Device::readFmtChunk  C0->C1  import mmioRead; caller 0x100041d0 (loadSoundFile); vtable 0x1005b178
2026-10-06  sound.dll:00006420  Wave_Device::openMemoryMmio  C0->C1  import mmioOpenA,mmioGetInfo
2026-10-06  sound.dll:0001e3d0  Midi_Device::closeFile  C0->C1  import mmioClose; callers 0x1001d810,0x1001dce0 (Midi_Device::loadFile/loadFromMemory)
2026-10-06  sound.dll:0001dce0  Midi_Device::loadFromMemory  C0->C1  import mmioRead,mmioGetInfo,mmioStringToFOURCCA; string 0x10063d90 ((string in the binary)),0x10063e08 ((string in the binary))
2026-10-06  sound.dll:0001d810  Midi_Device::loadFile  C0->C1  import mmioOpenA,mmioRead,mmioClose,mmioStringToFOURCCA; string 0x10063d90 ((string in the binary)),0x10063e08 ((string in the binary))
2026-10-06  sound.dll:000041d0  loadSoundFile  C0->C1  import mmioOpenA,mmioRead,mmioSeek,mmioClose,mmioStringToFOURCCA; string 0x10063d90 ((string in the binary))
2026-10-06  sound.dll:000246a0  Midi_Device::emitEvent  C0->C1  import midiOutShortMsg; caller 0x10023950
2026-10-06  sound.dll:00021a90  Midi_Device::sendShortMsg  C0->C1  import midiOutShortMsg; caller 0x10020870 (Midi_Device::playEvent)
2026-10-06  sound.dll:000215c0  Midi_Device::noteOn  C0->C1  import midiOutShortMsg; caller 0x10020870 (Midi_Device::playEvent)
2026-10-06  sound.dll:00020870  Midi_Device::playEvent  C0->C1  callees 0x100215c0,0x10021a90,0x100213d0; caller 0x10020... (sequencer)
2026-10-06  sound.dll:0001ef10  Midi_Device::flushMessages  C0->C1  import midiOutShortMsg; caller 0x10020d80
2026-10-06  sound.dll:00009da0  Midi_Device::openOutput  C0->C1  import midiOutOpen; callee 0x100088e0 (SoundTimer::registerClient)
2026-10-06  sound.dll:00009040  SoundTimer::linkClient  C0->C1  caller 0x100088e0 (SoundTimer::registerClient)
2026-10-06  sound.dll:00008920  SoundTimer::unregisterClient  C0->C1  import EnterCriticalSection,LeaveCriticalSection; callers 0x1000b960,0x1000bef0
2026-10-06  sound.dll:000088e0  SoundTimer::registerClient  C0->C1  import EnterCriticalSection,LeaveCriticalSection; callers 0x10009da0,0x1000a170; callee 0x10009040
2026-10-06  sound.dll:00008ed0  SoundTimer::onTick  C0->C1  import OutputDebugStringA,timeGetTime; string 0x10063d98 ((string in the binary))
2026-10-06  sound.dll:00008d70  SoundTimer::killEvent  C0->C1  import timeKillEvent,timeEndPeriod,EnterCriticalSection
2026-10-06  sound.dll:00008bf0  SoundTimer::stopKeepLock  C0->C1  import timeKillEvent,timeEndPeriod,EnterCriticalSection; caller 0x10009390 (release_sound)
2026-10-06  sound.dll:00008ac0  SoundTimer::stop  C0->C1  import timeKillEvent,timeEndPeriod,DeleteCriticalSection; caller 0x10009390 (release_sound)
2026-10-06  sound.dll:00008ce0  SoundTimer::start  C0->C1  import timeBeginPeriod,timeSetEvent; callers 0x1000bf90,0x1000c020
2026-10-06  sound.dll:000089f0  SoundTimer::init  C0->C1  import InitializeCriticalSection,timeGetDevCaps; caller 0x100092e0 (init_sound_timer)
2026-10-06  sound.dll:0000e910  Dll_Wave_In_Device::create_device  C0->C1  export 0x100020e5 (Dll_Wave_In_Device::create_device); callee 0x1002a240
2026-10-06  sound.dll:0000b050  Dll_Wave_Device::create_device  C0->C1  export 0x10001fcd (Dll_Wave_Device::create_device); callee 0x1000b1d0 (Wave_Device ctor)
2026-10-06  sound.dll:00009a90  Dll_Midi_Device::create_device  C0->C1  export 0x100021cb (Dll_Midi_Device::create_device); callee 0x10029e70
2026-10-06  sound.dll:00009390  release_sound  C0->C1  export 0x1000265d (release_sound); callees 0x10008ac0 (SoundTimer::stop), 0x10008bf0
2026-10-06  sound.dll:00009370  get_sound_version  C0->C1  export 0x10002770 (get_sound_version)
2026-10-06  sound.dll:000097d0  delete_sound  C0->C1  export 0x10001a87 (delete_sound)
2026-10-06  sound.dll:000093e0  create_sound  C0->C1  export 0x1000137a (create_sound); callee 0x10042690 (strstr); string 0x10063db4,0x10063db8,0x10063dbc,0x10063dc0
2026-10-06  sound.dll:000092e0  init_sound_timer  C0->C1  export 0x10001c67 (init_sound_timer); callee 0x100089f0 (SoundTimer::init)
2026-10-06  004b0540  jzero_far  C0->C1  match re/match/golf_jpeg_utils.cpp; IJG libjpeg 6a release source
2026-10-06  004b04f0  jcopy_sample_rows  C0->C1  match re/match/golf_jpeg_utils.cpp; IJG libjpeg 6a release source
2026-10-06  004b04d0  jround_up  C0->C1  match re/match/golf_jpeg_utils.cpp; IJG libjpeg 6a release source
2026-10-06  004b04c0  jdiv_round_up  C0->C1  match re/match/golf_jpeg_utils.cpp; IJG libjpeg 6a release source
2026-10-06  004b04b0  jpeg_mem_init  C0->C1  match re/match/golf_jpeg_memnobs.cpp; IJG libjpeg 6a release source
2026-10-06  004b0490  jpeg_open_backing_store  C0->C1  match re/match/golf_jpeg_memnobs.cpp; IJG libjpeg 6a release source
2026-10-06  004b0480  jpeg_mem_available  C0->C1  match re/match/golf_jpeg_memnobs.cpp; IJG libjpeg 6a release source
2026-10-06  004b0470  jpeg_free_large  C0->C1  match re/match/golf_jpeg_memnobs.cpp; IJG libjpeg 6a release source
2026-10-06  004b0460  jpeg_get_large  C0->C1  match re/match/golf_jpeg_memnobs.cpp; IJG libjpeg 6a release source
2026-10-06  004af920  free_pool  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af880  do_barray_io  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af690  do_sarray_io  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af3b0  realize_virt_arrays  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af340  request_virt_barray  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af2d0  request_virt_sarray  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af220  alloc_barray  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af170  alloc_sarray  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af0c0  alloc_large  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004af0a0  out_of_memory  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004aef70  alloc_small  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004aee30  jinit_memory_mgr  C0->C1  match re/match/golf_jpeg_memmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004aecd0  error_exit  C0->C1  match re/match/golf_jpeg_error.cpp; IJG libjpeg 6a release source
2026-10-06  004aec80  jpeg_std_error  C0->C1  match re/match/golf_jpeg_error.cpp; IJG libjpeg 6a release source
2026-10-06  004ae590  term_destination  C0->C1  match re/match/golf_jpeg_datadst.cpp; IJG libjpeg 6a release source
2026-10-06  004ae4d0  jpeg_stdio_dest  C0->C1  match re/match/golf_jpeg_datadst.cpp; IJG libjpeg 6a release source
2026-10-06  004b43f0  fullsize_downsample  C0->C1  match re/match/golf_jpeg_csample.cpp; IJG libjpeg 6a release source
2026-10-06  004b4390  expand_right_edge  C0->C1  match re/match/golf_jpeg_csample.cpp; IJG libjpeg 6a release source
2026-10-06  004b4040  jinit_downsampler  C0->C1  match re/match/golf_jpeg_csample.cpp; IJG libjpeg 6a release source
2026-10-06  004b3f10  create_context_buffer  C0->C1  match re/match/golf_jpeg_cprepct.cpp; IJG libjpeg 6a release source
2026-10-06  004b3d00  expand_bottom_edge  C0->C1  match re/match/golf_jpeg_cprepct.cpp; IJG libjpeg 6a release source
2026-10-06  004b3a80  jinit_c_prep_controller  C0->C1  match re/match/golf_jpeg_cprepct.cpp; IJG libjpeg 6a release source
2026-10-06  004b27d0  emit_buffered_bits  C0->C1  match re/match/golf_jpeg_cphuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b25d0  emit_eobrun  C0->C1  match re/match/golf_jpeg_cphuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b2510  flush_bits_4b2510  C0->C1  match re/match/golf_jpeg_cphuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b2460  emit_restart_4b2460  C0->C1  match re/match/golf_jpeg_cphuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b2420  dump_buffer_4b2420  C0->C1  match re/match/golf_jpeg_cphuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b1f30  jinit_phuff_encoder  C0->C1  match re/match/golf_jpeg_cphuff.cpp; IJG libjpeg 6a release source
2026-10-06  004ae990  jpeg_set_colorspace  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004ae910  jpeg_default_colorspace  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004ae8b0  add_huff_table  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004ae850  std_huff_tables  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004ae760  jpeg_set_defaults  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004ae740  jpeg_set_quality  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004ae700  jpeg_quality_scaling  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004ae6c0  jpeg_set_linear_quality  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004ae600  jpeg_add_quant_table  C0->C1  match re/match/golf_jpeg_cparam.cpp; IJG libjpeg 6a release source
2026-10-06  004afad0  jpeg_alloc_huff_table  C0->C1  match re/match/golf_jpeg_comapi.cpp; IJG libjpeg 6a release source
2026-10-06  004afab0  jpeg_alloc_quant_table  C0->C1  match re/match/golf_jpeg_comapi.cpp; IJG libjpeg 6a release source
2026-10-06  004afa90  jpeg_destroy  C0->C1  match re/match/golf_jpeg_comapi.cpp; IJG libjpeg 6a release source
2026-10-06  004afa60  jpeg_abort  C0->C1  match re/match/golf_jpeg_comapi.cpp; IJG libjpeg 6a release source
2026-10-06  004b5ca0  pass_startup  C0->C1  match re/match/golf_jpeg_cmaster.cpp; IJG libjpeg 6a release source
2026-10-06  004b5ad0  per_scan_setup  C0->C1  match re/match/golf_jpeg_cmaster.cpp; IJG libjpeg 6a release source
2026-10-06  004b59d0  select_scan_parameters  C0->C1  match re/match/golf_jpeg_cmaster.cpp; IJG libjpeg 6a release source
2026-10-06  004b5810  prepare_for_pass  C0->C1  match re/match/golf_jpeg_cmaster.cpp; IJG libjpeg 6a release source
2026-10-06  004b5470  validate_script  C0->C1  match re/match/golf_jpeg_cmaster.cpp; IJG libjpeg 6a release source
2026-10-06  004b52a0  initial_setup  C0->C1  match re/match/golf_jpeg_cmaster.cpp; IJG libjpeg 6a release source
2026-10-06  004b51e0  jinit_c_master_control  C0->C1  match re/match/golf_jpeg_cmaster.cpp; IJG libjpeg 6a release source
2026-10-06  004b0320  write_tables_only  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004b0220  emit_sos  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004b01f0  emit_dri  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004b0120  emit_dht  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004affa0  emit_sof  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afec0  emit_dqt  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afdc0  write_frame_header  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afd20  emit_adobe_app14  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afc80  emit_jfif_app0  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afc40  write_file_header  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afc10  emit_2bytes  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afbf0  emit_marker  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afbb0  emit_byte  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afb60  write_any_marker  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004afaf0  jinit_marker_writer  C0->C1  match re/match/golf_jpeg_cmarker.cpp; IJG libjpeg 6a release source
2026-10-06  004b0640  process_data_simple_main  C0->C1  match re/match/golf_jpeg_cmainct.cpp; IJG libjpeg 6a release source
2026-10-06  004b0560  jinit_c_main_controller  C0->C1  match re/match/golf_jpeg_cmainct.cpp; IJG libjpeg 6a release source
2026-10-06  004b03a0  jinit_compress_master  C0->C1  match re/match/golf_jpeg_cinit.cpp; IJG libjpeg 6a release source
2026-10-06  004b1d80  htest_one_block  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b1b70  flush_bits  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b1ae0  emit_restart  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b1a10  emit_bits  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b19e0  dump_buffer  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b15c0  encode_one_block  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b12b0  jinit_huff_encoder  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b1080  jpeg_gen_optimal_table  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b0f60  jpeg_make_c_derived_tbl  C0->C1  match re/match/golf_jpeg_chuff.cpp; IJG libjpeg 6a release source
2026-10-06  004b34b0  jinit_forward_dct  C0->C1  match re/match/golf_jpeg_cdctmgr.cpp; IJG libjpeg 6a release source
2026-10-06  004b50d0  grayscale_convert  C0->C1  match re/match/golf_jpeg_ccolor.cpp; IJG libjpeg 6a release source
2026-10-06  004b4eb0  rgb_gray_convert  C0->C1  match re/match/golf_jpeg_ccolor.cpp; IJG libjpeg 6a release source
2026-10-06  004b4a60  jinit_color_converter  C0->C1  match re/match/golf_jpeg_ccolor.cpp; IJG libjpeg 6a release source
2026-10-06  004b01e0  null_method  C0->C1  match re/match/golf_jpeg_ccolor.cpp; IJG libjpeg 6a release source
2026-10-06  004b0d70  compress_output  C0->C1  match re/match/golf_jpeg_ccoefct.cpp; IJG libjpeg 6a release source
2026-10-06  004b0860  start_iMCU_row  C0->C1  match re/match/golf_jpeg_ccoefct.cpp; IJG libjpeg 6a release source
2026-10-06  004b06f0  jinit_c_coef_controller  C0->C1  match re/match/golf_jpeg_ccoefct.cpp; IJG libjpeg 6a release source
2026-10-06  004ae400  jpeg_write_scanlines  C0->C1  match re/match/golf_jpeg_capistd.cpp; IJG libjpeg 6a release source
2026-10-06  004ae380  jpeg_start_compress  C0->C1  match re/match/golf_jpeg_capistd.cpp; IJG libjpeg 6a release source
2026-10-06  004ae270  jpeg_finish_compress  C0->C1  match re/match/golf_jpeg_capimin.cpp; IJG libjpeg 6a release source
2026-10-06  004ae220  jpeg_suppress_tables  C0->C1  match re/match/golf_jpeg_capimin.cpp; IJG libjpeg 6a release source
2026-10-06  004ae210  jpeg_abort_compress  C0->C1  match re/match/golf_jpeg_capimin.cpp; IJG libjpeg 6a release source
2026-10-06  004ae150  jpeg_CreateCompress  C0->C1  match re/match/golf_jpeg_capimin.cpp; IJG libjpeg 6a release source
2026-10-06  jgld.dll:0009e6d0  zcfree  C0->C1  match re/match/jgld_zlib_zutil.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009e680  zcalloc  C0->C1  match re/match/jgld_zlib_zutil.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009e630  z_error  C0->C1  match re/match/jgld_zlib_zutil.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:000a0130  inflate_flush  C0->C1  match re/match/jgld_zlib_infutil.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009f250  inflate_trees_free  C0->C1  match re/match/jgld_zlib_inftrees.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009f210  falloc  C0->C1  match re/match/jgld_zlib_inftrees.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009f030  inflate_trees_fixed  C0->C1  match re/match/jgld_zlib_inftrees.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009ef00  inflate_trees_dynamic  C0->C1  match re/match/jgld_zlib_inftrees.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009e7a0  huft_build  C0->C1  match re/match/jgld_zlib_inftrees.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009e710  inflate_trees_bits  C0->C1  match re/match/jgld_zlib_inftrees.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009c0f0  inflate  C0->C1  match re/match/jgld_zlib_inflate.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009c8c0  inflateSync  C0->C1  match re/match/jgld_zlib_inflate.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009c7e0  inflateSetDictionary  C0->C1  match re/match/jgld_zlib_inflate.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009c0b0  inflateInit_  C0->C1  match re/match/jgld_zlib_inflate.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009bf10  inflateInit2_  C0->C1  match re/match/jgld_zlib_inflate.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009be70  inflateEnd  C0->C1  match re/match/jgld_zlib_inflate.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009bdd0  inflateReset  C0->C1  match re/match/jgld_zlib_inflate.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:000a0340  inflate_fast  C0->C1  match re/match/jgld_zlib_inffast.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009f370  inflate_codes  C0->C1  match re/match/jgld_zlib_infcodes.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:000a00e0  inflate_codes_free  C0->C1  match re/match/jgld_zlib_infcodes.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009f2f0  inflate_codes_new  C0->C1  match re/match/jgld_zlib_infcodes.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009ce80  inflate_blocks  C0->C1  match re/match/jgld_zlib_infblock.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009e310  inflate_set_dictionary  C0->C1  match re/match/jgld_zlib_infblock.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009e290  inflate_blocks_free  C0->C1  match re/match/jgld_zlib_infblock.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009cd90  inflate_blocks_new  C0->C1  match re/match/jgld_zlib_infblock.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009cc70  inflate_blocks_reset  C0->C1  match re/match/jgld_zlib_infblock.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009ca70  crc32  C0->C1  match re/match/jgld_zlib_crc32.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009ca40  get_crc_table  C0->C1  match re/match/jgld_zlib_crc32.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0009e370  adler32  C0->C1  match re/match/jgld_zlib_adler32.cpp; zlib 1.0.2 release source
2026-10-06  jgld.dll:0006e250  png_get_user_transform_ptr  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e210  png_set_user_transform_info  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d8d0  png_set_invert_mono  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d890  png_set_invert_alpha  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d850  png_set_swap_alpha  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d6c0  png_set_packswap  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d670  png_set_packing  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d600  png_set_bgr  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e010  png_do_bgr  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006dac0  png_do_strip_filler  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006da10  png_do_packswap  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d980  png_do_swap  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d900  png_do_invert  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d7a0  png_set_filler  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d750  png_set_interlace_handling  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d700  png_set_shift  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d630  png_set_swap  C0->C1  match re/match/jgld_png_trans.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007e500  png_permit_empty_plte  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007e470  png_set_tRNS  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007e400  png_set_tIME  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007e220  png_set_text  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007e120  png_set_sRGB_gAMA_and_cHRM  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007e0d0  png_set_sRGB  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007e070  png_set_sBIT  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007e020  png_set_PLTE  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007dfc0  png_set_pHYs  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007de10  png_set_pCAL  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007ddb0  png_set_oFFs  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007dc70  png_set_IHDR  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007dc20  png_set_hIST  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007dbd0  png_set_gAMA  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007db30  png_set_cHRM  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007dad0  png_set_bKGD  C0->C1  match re/match/jgld_png_set.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007c1f0  png_check_chunk_name  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000794b0  png_crc_read  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007d6b0  png_read_start_row  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007d280  png_read_finish_row  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007ce80  png_read_filter_row  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007c810  png_do_read_interlace  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007c310  png_combine_row  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007c150  png_handle_unknown  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007bc40  png_handle_zTXt  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007baf0  png_handle_tEXt  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007b9b0  png_handle_tIME  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007b620  png_handle_pCAL  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007b4d0  png_handle_oFFs  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007b380  png_handle_pHYs  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007b1a0  png_handle_hIST  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007ae70  png_handle_bKGD  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007ab40  png_handle_tRNS  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007a7d0  png_handle_sRGB  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0007a0d0  png_handle_cHRM  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079ec0  png_handle_sBIT  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079cc0  png_handle_gAMA  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079c20  png_handle_IEND  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000799e0  png_handle_PLTE  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000796e0  png_handle_IHDR  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079620  png_crc_error  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079510  png_crc_finish  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079470  png_get_uint_16  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079410  png_get_uint_32  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000793b0  png_get_int_32  C0->C1  match re/match/jgld_png_rutil.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00077300  png_do_expand  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00070260  png_set_read_user_transform_fn  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000700c0  png_set_gray_to_rgb  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00070090  png_set_tRNS_to_alpha  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00070060  png_set_palette_to_rgb  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00070030  png_set_gray_1_2_4_to_8  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00070000  png_set_expand  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006f360  png_set_strip_alpha  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006f330  png_set_strip_16  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00077d30  png_build_gamma_table  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00077a90  png_do_dither  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00076ec0  png_do_expand_palette  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00076760  png_do_gamma  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00074750  png_do_background  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00074630  png_build_grayscale_palette  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000736c0  png_do_rgb_to_gray  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00073310  png_do_gray_to_rgb  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00072bd0  png_do_read_filler  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00072860  png_do_read_invert_alpha  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000724f0  png_do_read_swap_alpha  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00072430  png_do_chop  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000720c0  png_do_unshift  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00071e80  png_do_unpack  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000717d0  png_do_read_transformations  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000714c0  png_read_transform_info  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000702a0  png_init_read_transformations  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000700f0  png_set_rgb_to_gray  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006ff80  png_set_gamma  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006f3a0  png_set_dither  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006f230  png_set_background  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006f0d0  png_set_crc_action  C0->C1  match re/match/jgld_png_rtran.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078d40  png_default_read_data  C0->C1  match re/match/jgld_png_rio.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078cb0  png_set_read_fn  C0->C1  match re/match/jgld_png_rio.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078c40  png_read_data  C0->C1  match re/match/jgld_png_rio.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d5d0  png_set_read_status_fn  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d150  png_destroy_read_struct  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006cc50  png_read_image  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006c3a0  png_start_read_image  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006c340  png_read_update_info  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006d250  png_read_destroy  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006cd00  png_read_end  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006cb20  png_read_rows  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006c3f0  png_read_row  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006be20  png_read_info  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006bca0  png_read_init  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006bac0  png_create_read_struct  C0->C1  match re/match/jgld_png_read.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078c20  png_check_version  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078ad0  png_init_io  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078aa0  png_get_io_ptr  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078940  png_info_init  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000788d0  png_destroy_info_struct  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078790  png_reset_crc  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078750  png_zfree  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078be0  png_get_copyright  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078b00  png_convert_to_rfc1123  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078980  png_info_destroy  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078870  png_create_info_struct  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000787d0  png_calculate_crc  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000786b0  png_zalloc  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078670  png_check_sig  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000785f0  png_sig_cmp  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078590  png_set_sig_bytes  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078550  png_get_header_version  C0->C1  match re/match/jgld_png_png.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078e20  png_destroy_struct  C0->C1  match re/match/jgld_png_mem.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078f80  png_memset_check  C0->C1  match re/match/jgld_png_mem.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078f20  png_memcpy_check  C0->C1  match re/match/jgld_png_mem.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078ed0  png_free  C0->C1  match re/match/jgld_png_mem.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078e60  png_malloc  C0->C1  match re/match/jgld_png_mem.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078da0  png_create_struct  C0->C1  match re/match/jgld_png_mem.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006f0a0  png_get_rgb_to_gray_status  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006ef70  png_get_tIME  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006eeb0  png_get_sBIT  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006ee50  png_get_PLTE  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006ecc0  png_get_pCAL  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006ec40  png_get_oFFs  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006ea80  png_get_hIST  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006ea20  png_get_sRGB  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e9c0  png_get_gAMA  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e8d0  png_get_cHRM  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e870  png_get_bKGD  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e830  png_get_signature  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e7f0  png_get_channels  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e480  png_get_compression_type  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e440  png_get_interlace_type  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e400  png_get_filter_type  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e3c0  png_get_color_type  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e380  png_get_bit_depth  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e340  png_get_image_height  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e300  png_get_image_width  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e2c0  png_get_rowbytes  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e280  png_get_valid  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006efd0  png_get_tRNS  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006ef10  png_get_text  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006edb0  png_get_pHYs  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006eae0  png_get_IHDR  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e790  png_get_y_offset_pixels  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e730  png_get_x_offset_pixels  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e6d0  png_get_y_offset_microns  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e670  png_get_x_offset_microns  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e5f0  png_get_pixel_aspect_ratio  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e580  png_get_pixels_per_meter  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e520  png_get_y_pixels_per_meter  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006e4c0  png_get_x_pixels_per_meter  C0->C1  match re/match/jgld_png_get.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079300  png_default_warning  C0->C1  match re/match/jgld_png_error.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079250  png_chunk_warning  C0->C1  match re/match/jgld_png_error.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079100  png_format_buffer  C0->C1  match re/match/jgld_png_error.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:000790a0  png_chunk_error  C0->C1  match re/match/jgld_png_error.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079040  png_warning  C0->C1  match re/match/jgld_png_error.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00078fe0  png_error  C0->C1  match re/match/jgld_png_error.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079380  png_get_error_ptr  C0->C1  match re/match/jgld_png_error.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:00079340  png_set_error_fn  C0->C1  match re/match/jgld_png_error.cpp; libpng 1.0.5 release source
2026-10-06  jgld.dll:0006aa70  createPalette  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:0006a9a0  Palette::setRGB  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:0006a860  Palette::animate  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:0006a600  Palette::setDefault  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:0006a4a0  Palette::apply  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:0006a3d0  Palette::update  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:0006a2c0  Palette::~Palette  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:0006a0e0  onCommand  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:00069d50  onChar  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:00069c70  onKey  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:00069b90  onActivate  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:00069ac0  onPaint  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:000699c0  realizePalette  C0->C1  match re/match/jgld_app.cpp
2026-10-06  jgld.dll:000692b0  windowProc  C0->C1  callee 0x10069ac0 (onPaint), callee 0x10069c70 (onKey), callee 0x1006a0e0 (onCommand)
2026-10-06  jgld.dll:00069220  FontBase::FontBase  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00069010  FontBase::~FontBase  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068fa0  initFontTable  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068f30  initFonts  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068e60  Font::textWidth  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068da0  Font::release  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068ae0  Font::loadFile  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068850  Font::create  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068760  Font::~Font  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068660  Font::Font  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00068400  Array_Rec94::add  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:000681a0  Array_Rec14::add  C0->C1  match re/match/jgld_font.cpp
2026-10-06  jgld.dll:00067f10  Lib::setCommandHandler  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:00067cd0  Factory::destroySprite  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:00067c50  Factory::destroySurface  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:00067bd0  Factory::destroyPalette  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:00067b50  Factory::destroyFont  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:00067a90  Factory::createSprite  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:000679d0  Factory::createSurface  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:00067910  Factory::createPalette  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:00067850  Factory::createFont  C0->C1  match re/match/jgld_factory.cpp
2026-10-06  jgld.dll:00066970  Display::restore  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00066890  Display::setResolution  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00066780  Display::getCursor  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00066470  Display::present  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:000662a0  Display::setModeIndex  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00066200  Display::setDesktopMode  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00066040  Display::pickModeFreq  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00065ea0  Display::pickMode  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00065da0  Display::setMode  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00065c30  Display::findMode  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00065a90  Display::enumModes  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:000657e0  Display::createWindow  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:000654b0  Display::Display  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00065420  Lib::destroyDisplay  C0->C1  match re/match/jgld_display_noeh.cpp
2026-10-06  jgld.dll:00065390  createDisplay  C0->C1  match re/match/jgld_display_noeh.cpp
2026-10-06  jgld.dll:00065300  DllMain  C0->C1  match re/match/jgld_display.cpp
2026-10-06  jgld.dll:00063f60  Sprite::draw8_10063f60  C0->C1  match re/match/jgld_blit8.cpp
2026-10-06  jgld.dll:00062dd0  Sprite::draw_10062dd0  C0->C1  match re/match/jgld_blit8d.cpp
2026-10-06  jgld.dll:00061db0  Sprite::draw8_10061db0  C0->C1  match re/match/jgld_blit8.cpp
2026-10-06  jgld.dll:00060cf0  Sprite::draw8_10060cf0  C0->C1  match re/match/jgld_blit8.cpp
2026-10-06  jgld.dll:0005fd20  Sprite::draw8_1005fd20  C0->C1  match re/match/jgld_blit8b.cpp
2026-10-06  jgld.dll:0005ed80  Sprite::draw_1005ed80  C0->C1  match re/match/jgld_blit8c.cpp
2026-10-06  jgld.dll:0005dde0  Sprite::draw_1005dde0  C0->C1  match re/match/jgld_blit8c.cpp
2026-10-06  jgld.dll:0005d1f0  Sprite::drawRle16  C0->C1  match re/match/jgld_blitY5.cpp
2026-10-06  jgld.dll:0005c600  Sprite::drawRle16b  C0->C1  match re/match/jgld_blitY6.cpp
2026-10-06  jgld.dll:0005bac0  Sprite::drawRle16c  C0->C1  match re/match/jgld_blitY7.cpp
2026-10-06  jgld.dll:0005af00  Sprite::drawRle16t  C0->C1  match re/match/jgld_blitY9.cpp
2026-10-06  jgld.dll:0005a790  Sprite::drawRle16m  C0->C1  match re/match/jgld_blitY11.cpp
2026-10-06  jgld.dll:0005a000  Sprite::drawRle16mk  C0->C1  match re/match/jgld_blitY12.cpp
2026-10-06  jgld.dll:000594d0  Sprite::drawRle16n  C0->C1  match re/match/jgld_blitY8.cpp
2026-10-06  jgld.dll:00058970  Sprite::drawRle16p  C0->C1  match re/match/jgld_blitY10.cpp
2026-10-06  jgld.dll:000576a0  Sprite::draw16_100576a0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00056450  Sprite::draw16_10056450  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00054e20  Sprite::draw16_10054e20  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:000537f0  Sprite::draw16_100537f0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:000521d0  Sprite::draw16_100521d0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00050bb0  Sprite::draw16_10050bb0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0004f5d0  Sprite::draw16_1004f5d0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0004dff0  Sprite::draw16_1004dff0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0004ca10  Sprite::draw16_1004ca10  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0004b430  Sprite::draw16_1004b430  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00049ea0  Sprite::draw16_10049ea0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00048910  Sprite::draw16_10048910  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:000473b0  Sprite::draw16_100473b0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00045e50  Sprite::draw16_10045e50  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00044940  Sprite::draw16_10044940  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00043430  Sprite::draw16_10043430  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00041f80  Sprite::draw16_10041f80  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00040930  Sprite::draw16_10040930  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0003f2e0  Sprite::draw16_1003f2e0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0003dca0  Sprite::draw16_1003dca0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0003c660  Sprite::draw16_1003c660  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0003b060  Sprite::draw16_1003b060  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00039a60  Sprite::draw16_10039a60  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00038460  Sprite::draw16_10038460  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00036e60  Sprite::draw16_10036e60  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:000358b0  Sprite::draw16_100358b0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00034300  Sprite::draw16_10034300  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00032d60  Sprite::draw16_10032d60  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:000317c0  Sprite::draw16_100317c0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00030290  Sprite::draw16_10030290  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0002ed60  Sprite::draw16_1002ed60  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0002d890  Sprite::draw16_1002d890  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0002d390  Sprite::blitMaskScaled  C0->C1  match re/match/jgld_blitY2.cpp
2026-10-06  jgld.dll:0002cec0  Sprite::blitMask  C0->C1  match re/match/jgld_blitY1.cpp
2026-10-06  jgld.dll:0002bcf0  Sprite::draw_1002bcf0  C0->C1  match re/match/jgld_blit16d.cpp
2026-10-06  jgld.dll:0002ab90  Sprite::draw_1002ab90  C0->C1  match re/match/jgld_blit16d.cpp
2026-10-06  jgld.dll:000298b0  Sprite::draw16_100298b0  C0->C1  match re/match/jgld_blit16b.cpp
2026-10-06  jgld.dll:00028630  Sprite::draw16_10028630  C0->C1  match re/match/jgld_blit16b.cpp
2026-10-06  jgld.dll:00027430  Sprite::draw16_10027430  C0->C1  match re/match/jgld_blit16b.cpp
2026-10-06  jgld.dll:00026240  Sprite::draw16_10026240  C0->C1  match re/match/jgld_blit16b.cpp
2026-10-06  jgld.dll:00025dd0  Sprite::blitPair1  C0->C1  match re/match/jgld_blitY4.cpp
2026-10-06  jgld.dll:00025900  Sprite::blitPair  C0->C1  match re/match/jgld_blitY3.cpp
2026-10-06  jgld.dll:00024140  Sprite::draw16_10024140  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00022a10  Sprite::draw16_10022a10  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:00021720  Sprite::draw16_10021720  C0->C1  match re/match/jgld_blit16c.cpp
2026-10-06  jgld.dll:00020430  Sprite::draw16_10020430  C0->C1  match re/match/jgld_blit16c.cpp
2026-10-06  jgld.dll:0001f2d0  Sprite::draw_1001f2d0  C0->C1  match re/match/jgld_blit16e.cpp
2026-10-06  jgld.dll:0001e970  Sprite::draw16m_1001e970  C0->C1  match re/match/jgld_blitY13.cpp
2026-10-06  jgld.dll:0001df80  Sprite::draw16m_1001df80  C0->C1  match re/match/jgld_blitY14.cpp
2026-10-06  jgld.dll:0001cde0  Sprite::draw16_1001cde0  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0001bc40  Sprite::draw16_1001bc40  C0->C1  match re/match/jgld_blit16.cpp
2026-10-06  jgld.dll:0001bae0  Sprite::remap8  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:0001b690  Sprite::create8  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:0001b060  Sprite::draw16t_1001b060  C0->C1  match re/match/jgld_blitY15.cpp
2026-10-06  jgld.dll:0001ab40  Sprite::draw16t_1001ab40  C0->C1  match re/match/jgld_blitY16.cpp
2026-10-06  jgld.dll:0001a250  Sprite::create16  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:0001a0e0  Sprite::copyFrom  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00019b90  Sprite::bltS2  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00019aa0  Sprite::bltS  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000199d0  Sprite::blt16c  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00019900  Sprite::blt16b  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00019830  Sprite::blt16a  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000193d0  Sprite::drawP  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00018920  Sprite::drawAlpha  C0->C1  match re/match/jgld_blitY17.cpp
2026-10-06  jgld.dll:000185b0  Sprite::drawO  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00018510  Sprite::dashTo  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00018490  Sprite::lineTo  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00018120  Sprite::drawN  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00018020  getField2c  C0->C1  match re/match/jgld_small1.cpp
2026-10-06  jgld.dll:00017fe0  getField20  C0->C1  match re/match/jgld_small1.cpp
2026-10-06  jgld.dll:00017f70  getField14  C0->C1  match re/match/jgld_small1.cpp
2026-10-06  jgld.dll:00017f30  getField28  C0->C1  match re/match/jgld_small1.cpp
2026-10-06  jgld.dll:00017b40  Sprite::drawM  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000178a0  Sprite::drawL  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000172b0  Sprite::drawK  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00016fc0  Sprite::drawJ  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00016cb0  Sprite::drawI  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000169a0  Sprite::drawH  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000168e0  SpriteBase::~SpriteBase  C0->C1  match re/match/jgld_sprite_dtor.cpp
2026-10-06  jgld.dll:00003340  Matrix::reset  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x100045f0 (Matrix::loadIdentity)
2026-10-06  jgld.dll:00014c00  Sprite::deletingDtor  C0->C1  match re/match/jgld_raw_03.cpp, callee 0x10014de0 (Sprite::dtor)
2026-10-06  jgld.dll:00008ae0  Surface::selectObject  C0->C1  match re/match/jgld_raw_02.cpp, import SelectObject
2026-10-06  jgld.dll:000087b0  Surface::releaseDC2  C0->C1  match re/match/jgld_raw_02.cpp
2026-10-06  jgld.dll:00008730  Surface::acquireDC2  C0->C1  match re/match/jgld_raw_02.cpp
2026-10-06  jgld.dll:000098f0  Surface::blit16  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:000145a0  loadPng  C0->C1  import png_create_read_struct/png_read_image/png_get_IHDR, import _fopen, string 0x1011d368 (libpng version string)
2026-10-06  jgld.dll:00007e40  allocDrawBuffer  C0->C1  string 0x1011d1d0 (fatal error message), string 0x1011d210 (error title), import _exit
2026-10-06  jgld.dll:000165a0  Sprite::drawG  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00016270  Sprite::drawF  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00015f80  Sprite::drawE  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00015bb0  Sprite::drawD  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000157e0  Sprite::drawC  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00015480  Sprite::drawB  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00015180  Sprite::draw  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000150d0  Sprite::remap  C0->C1  match re/match/jgld_sprite.cpp, string 0x1011d508 (jglsprite.cpp assert)
2026-10-06  jgld.dll:00014ff0  Sprite::create  C0->C1  match re/match/jgld_sprite.cpp, callee 0x1001b690 (create8)/0x1001a250 (create16)
2026-10-06  jgld.dll:00014eb0  Sprite::release  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00014de0  Sprite::dtor  C0->C1  match re/match/jgld_sprite_dtor.cpp, vtable 0x1011d380, import DeleteCriticalSection, callee 0x10014eb0 (Sprite::release)
2026-10-06  jgld.dll:00014cd0  Sprite::ctorN  C0->C1  match re/match/jgld_raw_03.cpp, vtable 0x1011d380, callee 0x10014c70 (SpriteBase::ctor)
2026-10-06  jgld.dll:00014af0  Sprite::ctor  C0->C1  match re/match/jgld_raw_03.cpp, vtable 0x1011d380, callee 0x10014c70 (SpriteBase::ctor)
2026-10-06  jgld.dll:00014c70  SpriteBase::ctor  C0->C1  match re/match/jgld_raw_03.cpp, vtable 0x1011d444, callee 0x10006ab0 (Tracked::ctor)
2026-10-06  jgld.dll:00014a90  jfree  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00014a40  jalloc  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:00014550  trace  C0->C1  match re/match/jgld_sprite.cpp
2026-10-06  jgld.dll:000085f0  equal  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00008590  intersect  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00009160  rectHeight  C0->C1  match re/match/jgld_raw_02.cpp
2026-10-06  jgld.dll:00009120  rectWidth  C0->C1  match re/match/jgld_raw_02.cpp
2026-10-06  jgld.dll:00010390  setRect4  C0->C1  match re/match/jgld_raw_03.cpp
2026-10-06  jgld.dll:00008360  rectFromXYWH  C0->C1  match re/match/jgld_raw_02.cpp
2026-10-06  jgld.dll:00008da0  Surface::textOut  C0->C1  match re/match/jgld_raw_02.cpp, import TextOutA
2026-10-06  jgld.dll:00008d00  Surface::setTextColor16  C0->C1  match re/match/jgld_raw_02.cpp, import SetTextColor
2026-10-06  jgld.dll:00008c40  Surface::setTextColorRGB  C0->C1  match re/match/jgld_raw_02.cpp, import SetTextColor
2026-10-06  jgld.dll:00008b90  Surface::selectSystemFont  C0->C1  match re/match/jgld_raw_02.cpp, import GetStockObject/SelectObject
2026-10-06  jgld.dll:000086b0  Surface::releaseDC  C0->C1  match re/match/jgld_raw_02.cpp
2026-10-06  jgld.dll:00008640  Surface::acquireDC  C0->C1  match re/match/jgld_raw_02.cpp
2026-10-06  jgld.dll:0000af30  fillWords  C0->C1  match re/match/jgld_color.cpp
2026-10-06  jgld.dll:0000e540  blueOf  C0->C1  match re/match/jgld_color.cpp
2026-10-06  jgld.dll:0000e4c0  greenOf  C0->C1  match re/match/jgld_color.cpp
2026-10-06  jgld.dll:0000e440  redOf  C0->C1  match re/match/jgld_color.cpp
2026-10-06  jgld.dll:0000a660  makeColor  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00013800  Surface::blit8from8  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00012900  Surface::blitKey8to16  C0->C1  match re/match/jgld_surface16.cpp
2026-10-06  jgld.dll:00012140  Surface::writePcx8  C0->C1  caller 0x1000a550 (Surface::saveImage), callee 0x10002bd0 (MappedFile::create), string 0x1011d354 (file-extension string)
2026-10-06  jgld.dll:00011f80  Surface::ditherRectC8  C0->C1  match re/match/jgld_surface.cpp, caller 0x10009c50
2026-10-06  jgld.dll:00011dd0  Surface::fillRectC8  C0->C1  match re/match/jgld_surface.cpp, caller 0x10009770
2026-10-06  jgld.dll:00011c40  Surface::clear8  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00011880  Surface::dashedVLine8  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:000114e0  Surface::dashedHLine8  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00010870  Surface::dashedLine8  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00010620  Surface::vline  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:000103f0  Surface::hline  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000fb70  Surface::line8  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000f9e0  Surface::fill2_8  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000f880  Surface::fill8  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000f6a0  Surface::blit16from16  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00013c50  Surface::blitT_8to8  C0->C1  caller 0x100091a0 (Surface::blitT), callee 0x10008360/0x10008590
2026-10-06  jgld.dll:00012db0  Surface::blitT_8to16  C0->C1  caller 0x100091a0 (Surface::blitT), callee 0x10008360/0x10008590
2026-10-06  jgld.dll:0000ed70  Surface::blitT_16to16  C0->C1  caller 0x100091a0 (Surface::blitT), callee 0x10008360/0x10008590
2026-10-06  jgld.dll:0000e8b0  Surface::ditherBlendRect16  C0->C1  match re/match/jgld_surface16.cpp
2026-10-06  jgld.dll:0000e580  Surface::ditherRect16  C0->C1  match re/match/jgld_surface16.cpp
2026-10-06  jgld.dll:0000dfc0  Surface::blendRect16  C0->C1  match re/match/jgld_surface16.cpp
2026-10-06  jgld.dll:0000dcf0  Surface::fillRect16  C0->C1  match re/match/jgld_surface16.cpp
2026-10-06  jgld.dll:0000da70  Surface::clear16  C0->C1  match re/match/jgld_surface16.cpp
2026-10-06  jgld.dll:0000d4f0  Surface::dashedVLine16  C0->C1  caller 0x1000c140 (Surface::dashedLine16)
2026-10-06  jgld.dll:0000cf90  Surface::dashedHLine16  C0->C1  caller 0x1000c140 (Surface::dashedLine16)
2026-10-06  jgld.dll:0000c140  Surface::dashedLine16  C0->C1  caller 0x100095a0 (Surface::drawDashedLine), callee 0x1000cf90/0x1000d4f0
2026-10-06  jgld.dll:0000b140  Surface::line16  C0->C1  caller 0x100094e0 (Surface::drawLine), callee 0x1000ba90/0x1000bde0
2026-10-06  jgld.dll:0000bde0  Surface::vline16  C0->C1  match re/match/jgld_surface16.cpp
2026-10-06  jgld.dll:0000ba90  Surface::hline16  C0->C1  match re/match/jgld_surface16.cpp
2026-10-06  jgld.dll:0000afe0  Surface::fill16  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000a8d0  Surface::callBounds  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000a870  Surface::call3b  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000a810  Surface::call3a  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000a330  Surface::drawToZ  C0->C1  match re/match/jgld_surface_sprite.cpp
2026-10-06  jgld.dll:00009fd0  Surface::drawTo  C0->C1  match re/match/jgld_surface_sprite.cpp
2026-10-06  jgld.dll:0000a550  Surface::saveImage  C0->C1  match re/match/jgld_surface.cpp, callee 0x10012140 (writePcx8)
2026-10-06  jgld.dll:00009e40  Surface::ditherBlendRectDispatch  C0->C1  match re/match/jgld_surface.cpp, callee 0x1000e8b0 (ditherBlendRect16)
2026-10-06  jgld.dll:00009ac0  Surface::blendRectDispatch  C0->C1  match re/match/jgld_surface.cpp, callee 0x1000dfc0 (blendRect16)
2026-10-06  jgld.dll:00009c50  Surface::ditherRectDispatch  C0->C1  match re/match/jgld_surface.cpp, callee 0x10011f80/0x1000e580
2026-10-06  jgld.dll:00009770  Surface::fillRectDispatch  C0->C1  match re/match/jgld_surface.cpp, callee 0x10011dd0/0x1000dcf0
2026-10-06  jgld.dll:000095a0  Surface::drawDashedLine  C0->C1  match re/match/jgld_surface.cpp, callee 0x10010870 (dashedLine8)/0x1000c140 (dashedLine16)
2026-10-06  jgld.dll:000094e0  Surface::drawLine  C0->C1  match re/match/jgld_surface.cpp, callee 0x1000fb70 (line8)/0x1000b140 (line16)
2026-10-06  jgld.dll:00009440  Surface::fill2  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00009320  Surface::fillRect  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:000091a0  Surface::blitT  C0->C1  match re/match/jgld_surface.cpp, callee 0x10013c50/0x10012db0/0x1000ed70
2026-10-06  jgld.dll:00008f70  Surface::stretchTo  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00008e50  Surface::blit  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:000089d0  Surface::setPalette  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00008830  Surface::pixelAddr  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000a9d0  Surface::getBounds  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000a930  Surface::getClip  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:000083c0  Surface::setClip  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:0000a4e0  SurfaceBase::deletingDtor  C0->C1  match re/match/jgld_raw_02.cpp, callee 0x100096f0 (SurfaceBase::dtor)
2026-10-06  jgld.dll:000096f0  SurfaceBase::dtor  C0->C1  match re/match/jgld_raw_02.cpp, vtable 0x1011d220, callee 0x10006b80 (Tracked::dtor)
2026-10-06  jgld.dll:00009690  SurfaceBase::ctor  C0->C1  match re/match/jgld_raw_02.cpp, vtable 0x1011d220, callee 0x10006ab0 (Tracked::ctor)
2026-10-06  jgld.dll:00007cc0  Surface::release  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00007bf0  Surface::dtor  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00007900  Surface::deletingDtor  C0->C1  match re/match/jgld_surface.cpp
2026-10-06  jgld.dll:00007970  Surface::ctorOwner  C0->C1  match re/match/jgld_surface.cpp, vtable 0x1011d0b0
2026-10-06  jgld.dll:00007660  Surface::ctor  C0->C1  match re/match/jgld_surface.cpp, vtable 0x1011d0b0, callee 0x10009690 (SurfaceBase::ctor)
2026-10-06  jgld.dll:00007620  Random::ctor  C0->C1  match re/match/jgld_random.cpp
2026-10-06  jgld.dll:000075b0  Random::range  C0->C1  match re/match/jgld_random.cpp, callee 0x10007530 (Random::next)
2026-10-06  jgld.dll:00007530  Random::next  C0->C1  match re/match/jgld_random.cpp, data 0x1011d0a0 (32768.0f)
2026-10-06  jgld.dll:00006a40  atexitDestroyGlobalList  C0->C1  match re/match/jgld_list.cpp, callee 0x10006cf0 (LinkedList::dtor), data 0x10128428/0x10128444
2026-10-06  jgld.dll:00007450  ListNode::dtor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:000073e0  ListNode::deletingDtor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00007370  ListNode::ctor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:000072b0  LinkedList::find  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00007100  LinkedList::removeCurrent  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006f40  LinkedList::add  C0->C1  match re/match/jgld_list.cpp, string 0x1011d060 (debug warning)
2026-10-06  jgld.dll:00006df0  LinkedList::clear  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006d80  LinkedList::deletingDtor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006d40  LinkedList::count  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006cf0  LinkedList::dtor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006c80  LinkedList::ctor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006bf0  Tracked::deleteAll  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006b80  Tracked::dtor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006b10  Tracked::deletingDtor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006ab0  Tracked::ctor  C0->C1  match re/match/jgld_list.cpp
2026-10-06  jgld.dll:00006840  Transform::assignOp  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00006620  Transform::rotateZ  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00006520  Transform::rotateY  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00006420  Transform::rotateX  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00006340  Transform::rotate  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000062e0  Transform::invert  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x10003df0, callee 0x10004530
2026-10-06  jgld.dll:00006270  Transform::scaleTranslation  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:000060d0  Transform::mulAssignTransform  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005ff0  Transform::scaled  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005dd0  Transform::mulTransform  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005d10  Transform::apply  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005c70  Transform::move  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005bd0  Transform::translateXYZ  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005b30  Transform::untranslate  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005990  Transform::translateVec  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005a30  Transform::subVector3  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005890  Transform::addVector3  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005690  Transform::getMatrix  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005650  Transform::reset  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000055c0  Transform::set  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005530  Transform::setRotation  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000054b0  Transform::setTranslation  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003530  Transform::assign  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003470  Transform::ctorVec  C0->C1  match re/match/jgld_raw_01.cpp, vtable 0x1011d028, callee 0x100054b0 (Transform::setTranslation)
2026-10-06  jgld.dll:00003400  Transform::ctorQuat  C0->C1  match re/match/jgld_raw_01.cpp, vtable 0x1011d028, callee 0x10005530 (Transform::setRotation)
2026-10-06  jgld.dll:00003390  Transform::ctorQuatVec  C0->C1  match re/match/jgld_raw_01.cpp, vtable 0x1011d028, callee 0x100055c0 (Transform::set)
2026-10-06  jgld.dll:00003670  Transform::deletingDtor  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x100034e0 (Transform::dtor)
2026-10-06  jgld.dll:000034e0  Transform::dtor  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000068d0  Transform::copyCtor  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005450  Transform::ctor  C0->C1  match re/match/jgld_math.cpp, vtable 0x1011d028
2026-10-06  jgld.dll:00006790  Matrix::assign  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:000053e0  Matrix::translateXYZ  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005360  Matrix::translateVec  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00005030  Matrix::mulMatrix  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00004f40  Matrix::mulScalar  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00004e40  Matrix::mulPoint  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00004dc0  Matrix::equals  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000049e0  Matrix::scaleDiagonal  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00004d40  Matrix::subEquals  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00004cc0  Matrix::addEquals  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00004a50  Matrix::mulAssign  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000048e0  Matrix::add  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00004750  Matrix::setRotation  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000046f0  Matrix::setTranslation  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00004690  Matrix::setRotationTranslation  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x10004750, callee 0x100046f0
2026-10-06  jgld.dll:00003290  Matrix::ctorVec  C0->C1  match re/match/jgld_raw_01.cpp, vtable 0x1011d024, callee 0x100046f0 (Matrix::setTranslation)
2026-10-06  jgld.dll:00003230  Matrix::ctorQuat  C0->C1  match re/match/jgld_raw_01.cpp, vtable 0x1011d024, callee 0x10004750 (Matrix::setRotation)
2026-10-06  jgld.dll:000031d0  Matrix::ctorQuatVec  C0->C1  match re/match/jgld_raw_01.cpp, vtable 0x1011d024, callee 0x10004690
2026-10-06  jgld.dll:00003600  Matrix::deletingDtor  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x100032f0 (Matrix::dtor)
2026-10-06  jgld.dll:000032f0  Matrix::dtor  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00006720  Matrix::copyCtor  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000045f0  Matrix::loadIdentity  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00004590  Matrix::ctor  C0->C1  match re/match/jgld_math.cpp, callee 0x100045f0 (Matrix::loadIdentity)
2026-10-06  jgld.dll:00004530  Quat::conjugate  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00004410  Quat::mul  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000042f0  Quat::mulAssign  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00004230  Quat::setAxisAngle  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003110  Quat::ctorAxisAngle  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x10004230 (Quat::setAxisAngle)
2026-10-06  jgld.dll:000030b0  Quat::copyFrom  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00003170  Quat::setIdentity  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00003050  Quat::ctor  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x100036e0 (Vector3::set)
2026-10-06  jgld.dll:00004100  Vector3::rotate  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003e50  Vector3::cross  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003cf0  Vector3::normalize  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003c90  Vector3::dot  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003bf0  Vector3::distance2  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003b30  Vector3::distance  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003ab0  Vector3::equals  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003a40  Vector3::mulAssignScalar  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:000039b0  Vector3::mulScalar  C0->C1  match re/match/jgld_math.cpp
2026-10-06  jgld.dll:00003f80  Vector3::midpoint  C0->C1  match re/match/jgld_raw_01.cpp, data 0x1011d04c (0.5f)
2026-10-06  jgld.dll:00003820  Vector3::sub  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00003790  Vector3::add  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00003930  Vector3::subAssignRet  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:000038b0  Vector3::addAssignRet  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00004090  Vector3::addAssign  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00004020  Vector3::addXYZ  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00003f10  Vector3::scale  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00003df0  Vector3::negate  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:000036e0  Vector3::set  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00003730  Vector3::setZero  C0->C1  match re/match/jgld_raw_01.cpp
2026-10-06  jgld.dll:00002ef0  MappedFile::closeTruncate  C0->C1  match re/match/jgld_raw_01.cpp, import SetFilePointer/SetEndOfFile/CloseHandle
2026-10-06  jgld.dll:00002e10  MappedFile::close  C0->C1  match re/match/jgld_raw_01.cpp, import UnmapViewOfFile/CloseHandle
2026-10-06  jgld.dll:00002bd0  MappedFile::create  C0->C1  match re/match/jgld_mappedfile.cpp, import CreateFileA/CreateFileMappingA/MapViewOfFile
2026-10-06  jgld.dll:00002a20  MappedFile::openWrite  C0->C1  match re/match/jgld_mappedfile.cpp, import CreateFileA/CreateFileMappingA/MapViewOfFile
2026-10-06  jgld.dll:00002870  MappedFile::openRead  C0->C1  match re/match/jgld_mappedfile.cpp, import CreateFileA/CreateFileMappingA/MapViewOfFile
2026-10-06  jgld.dll:00002820  MappedFile::dtor  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x10002e10 (MappedFile::close)
2026-10-06  jgld.dll:000027b0  MappedFile::ctorName  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x10002a20 (MappedFile::openWrite)
2026-10-06  jgld.dll:00002730  MappedFile::ctorNameFlag  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x10002a20 (MappedFile::openWrite)
2026-10-06  jgld.dll:000026c0  MappedFile::deletingDtor  C0->C1  match re/match/jgld_raw_01.cpp, callee 0x10002820 (MappedFile::dtor)
2026-10-06  jgld.dll:00002650  MappedFile::ctor  C0->C1  match re/match/jgld_raw_01.cpp, vtable 0x1011d01c
2026-10-06  004abe6b  amsgExitThunk  C0->C1  match re/match/golf_raw_05.cpp; callee __amsg_exit 0x004a6937
2026-10-06  004a9115  doserrnoPtr  C0->C1  match re/match/golf_raw_05.cpp; callee __getptd; returns getptd()+3
2026-10-06  004a910c  errnoPtr  C0->C1  match re/match/golf_raw_05.cpp; callee __getptd; returns getptd()+2
2026-10-06  004a512a  doexit  C0->C1  callers _exit 0x004a5108, __exit 0x004a5119; callee __initterm 0x004a51e1, __lockexit
2026-10-06  004a4fc4  initFpControl  C0->C1  match re/match/golf_hand_01.cpp; caller __fpmath 0x004a4fac; writes FP control globals 0x004e4b00..0x004e4b14
2026-10-06  004a4ed0  listRemoveHead  C0->C1  match re/match/golf_classes.cpp; callers moviePlayFrame 0x0049ccc0, listPush; callee _free
2026-10-06  004a4db0  listPush  C0->C1  match re/match/golf_hand_07_4a4db0.cpp; caller movieReadFrame 0x0049cb90; callee removeHead 0x004a4ed0, _malloc
2026-10-06  004a4d50  listClear4a4d  C0->C1  match re/match/golf_hand_02.cpp; callee _free
2026-10-06  004a4d30  listClearThunk  C0->C1  match re/match/golf_raw_05.cpp; callee 0x004a4d50 (listClear4a4d)
2026-10-06  004a4c70  queuePop  C0->C1  match re/match/golf_hand_06_k.cpp; caller netPoll 0x0049b7b0; callee _free
2026-10-06  004a4b60  listAppend4a4b60  C0->C1  match re/match/golf_hand_08_c.cpp; callers netService/netSendMessages; callee _malloc
2026-10-06  004a4b00  listClear4a4b  C0->C1  match re/match/golf_classes.cpp; caller netSendShutdown 0x00497d10; callee _free
2026-10-06  004a48f0  listGetAt  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  004a4890  listFindById  C0->C1  match re/match/golf_hand_02.cpp; caller panelOnPick 0x004a4350
2026-10-06  004a4680  panelOnKey  C0->C1  match re/match/golf_hand_r2.cpp; callee panelDraw4a3f10 0x004a3f10
2026-10-06  004a4570  hotHitCb6  C0->C1  match re/match/golf_raw_11.cpp; callee hit 0x00492a90 (HotList::hit)
2026-10-06  004a4530  hotHitCb5  C0->C1  match re/match/golf_raw_11.cpp; callee hit 0x00492a90 (HotList::hit)
2026-10-06  004a44f0  hotHitCb4  C0->C1  match re/match/golf_raw_11.cpp; callee hit 0x00492a90 (HotList::hit)
2026-10-06  004a44b0  hotHitCb3  C0->C1  match re/match/golf_raw_11.cpp; callee hit 0x00492a90 (HotList::hit)
2026-10-06  004a4470  hotHitCb2  C0->C1  match re/match/golf_raw_11.cpp; callee hit 0x00492a90 (HotList::hit)
2026-10-06  004a4430  hotHitCb1  C0->C1  match re/match/golf_raw_11.cpp; callee hit 0x00492a90 (HotList::hit)
2026-10-06  004a43f0  hotHitCb0  C0->C1  match re/match/golf_raw_11.cpp; callee hit 0x00492a90 (HotList::hit)
2026-10-06  004a4350  panelOnPick  C0->C1  match re/match/golf_hand_04.cpp; callee hit 0x00492a90 (HotList::hit), find 0x004a4890, panelDraw4a3f10
2026-10-06  004a3f10  panelDraw4a3f10  C0->C1  callers onPick 0x004a4350, onKey 0x004a4680; callee draw 0x00473e60, drawCentered 0x00477cd0, 0x00479560
2026-10-06  004a3be0  panelBuildText4a3be0  C0->C1  callers panelStop 0x004a11c0, panelBuild4a3880; callee setQuad, put 0x00477250, value477580
2026-10-06  004a3880  panelBuild4a3880  C0->C1  caller panelDispatch 0x004a08a0; callee alloc 0x00492920, add 0x004929b0, 0x004a3be0
2026-10-06  004a3860  panelReset4a3860  C0->C1  match re/match/golf_raw_05.cpp; caller viewDispatchReset 0x004a12e0; callee reset 0x004897f0, panelSubReset4a35e0
2026-10-06  004a3790  panelOpenReset4a3790  C0->C1  match re/match/golf_raw_05.cpp; caller viewDispatchOpen 0x004a1250; callee open 0x004896b0, panelSubReset4a35e0
2026-10-06  004a35e0  panelSubReset4a35e0  C0->C1  match re/match/golf_hand_r0.cpp; callee setQuad 0x00476310, 0x00492830; reads label table 0x004e449c
2026-10-06  004a3480  panelSubDtor4a3480  C0->C1  match re/match/golf_hand_r1.cpp; callee 0x004928d0, reset 0x004a35e0 (panelSubReset4a35e0)
2026-10-06  004a33e0  panelSubDtor4a33e0  C0->C1  match re/match/golf_hand_04.cpp; frees table 0x004ba2c0
2026-10-06  004a3370  panelSubDtor4a3370  C0->C1  match re/match/golf_hand_00.cpp; callee 0x004a33e0 (panelSubDtor4a33e0)
2026-10-06  004a3110  panelSubCtor4a3110  C0->C1  match re/match/golf_hand_s0.cpp; caller viewCtor4a0740; callee setQuad, 0x00492850; reads label table 0x004e449c
2026-10-06  004a2fa0  widgetShow  C0->C1  match re/match/golf_hand_02.cpp; sets a visibility flag at this
2026-10-06  004a2d50  panelDrawRow  C0->C1  fastcall base-pointer view; callee 0x00475c60, drawCentered 0x00477cd0; reads global 0x0083ab2c
2026-10-06  004a2a60  panelBuildChildren  C0->C1  callers panelStop 0x004a11c0, panelCreate4a2400; callee setQuad 0x00476310, put 0x00477250, value477580
2026-10-06  004a29c0  setStringField  C0->C1  match re/match/golf_hand_04.cpp; caller 0x0048e1c0; callee _malloc, _free
2026-10-06  004a28a0  listAddEntry  C0->C1  match re/match/golf_hand_07_4a28a0.cpp; caller 0x0048e1c0; callee add 0x00401d10, _malloc
2026-10-06  004a2400  panelCreate4a2400  C0->C1  caller panelDispatch 0x004a08a0; SEH-guarded; callee create 0x004806c0, 0x00486110, setText 0x00486200, 0x004a2a60
2026-10-06  004a23a0  viewFieldReset4a23a0  C0->C1  match re/match/golf_hand_03.cpp; callee reset 0x004894b0, _free
2026-10-06  004a2330  viewFieldDtor4a2330  C0->C1  match re/match/golf_hand_03.cpp; callee reset 0x004a23a0 (viewFieldReset4a23a0)
2026-10-06  004a2250  viewFieldCtor4a2250  C0->C1  match re/match/golf_hand_06_g.cpp; caller viewCtor4a0740; callee 0x004804a0, 0x00489150
2026-10-06  004a14c0  resetMsgObject  C0->C1  match re/match/golf_hand_05.cpp; callers netSendMessages 0x00499140, comboDtor 0x00497a20; callee 0x004886d0
2026-10-06  004a13f0  ctorObj4a13f0  C0->C1  match re/match/golf_hand_06_e.cpp; callers 0x0048ce00/0x0048e900/0x00496030; callee ctor 0x00488500
2026-10-06  004a12e0  viewDispatchReset  C0->C1  match re/match/golf_hand_04.cpp; caller init 0x0048db60; callee reset 0x004897f0, viewFieldCleanup49d690/49ece0, 0x004a3860
2026-10-06  004a1250  viewDispatchOpen  C0->C1  match re/match/golf_hand_04.cpp; caller init 0x0048db60; callee open 0x004896b0, viewFieldCleanup49d690/49ece0, reset 0x004a23a0
2026-10-06  004a11c0  panelStop  C0->C1  match re/match/golf_hand_02.cpp; caller 0x0048fe60; callee panelBuildText 0x0049d770, optionDialogDraw 0x0049f050, 0x004a2a60, 0x004a3be0
2026-10-06  004a09a0  sendCommand  C0->C1  match re/match/golf_hand_02.cpp; callee begin 0x00489890, add 0x0048c640
2026-10-06  004a08a0  panelDispatch  C0->C1  match re/match/golf_hand_06_g.cpp; caller 0x0048e900; callee panelBuildRow 0x0049d6d0, optionDialogBuild 0x0049ed20, 0x004a2400, 0x004a3880
2026-10-06  004a0740  viewCtor4a0740  C0->C1  match re/match/golf_hand_r0.cpp; caller 0x0048ce00; callee viewFieldCtor49d5a0, viewFieldCtor49ebb0, 0x004a2250, 0x004a3110
2026-10-06  004a0600  postMessageLine  C0->C1  match re/match/golf_hand_09_h.cpp; caller say 0x0048df20; callee add 0x00401d10
2026-10-06  004a05d0  bufferFree  C0->C1  match re/match/golf_hand_00.cpp; callee _free
2026-10-06  004a0540  bufferCopyTo  C0->C1  match re/match/golf_hand_04.cpp; callee alloc 0x00474860, _malloc
2026-10-06  004a0350  radioSelect  C0->C1  match re/match/golf_hand_s2.cpp; callee setMode 0x004890e0
2026-10-06  004a0320  logLine  C0->C1  match re/match/golf_raw_05.cpp; callee logWrite 0x004a0280; callers movie/reader functions
2026-10-06  004a0280  logWrite  C0->C1  match re/match/golf_hand_05.cpp; string 0x004e4a50 (string in the binary), 0x004e4a60 (string in the binary); callee _fprintf
2026-10-06  004a01d0  loggerSetPath  C0->C1  match re/match/golf_hand_05.cpp; callee resolveAndOpenFile 0x004a00f0, _fclose, _malloc
2026-10-06  004a01a0  freeLoggerBuf  C0->C1  match re/match/golf_raw_10.cpp; callee _free
2026-10-06  004a0180  loggerCtor  C0->C1  match re/match/golf_raw_05.cpp; callee setPath 0x004a01d0
2026-10-06  004a0160  loggerDeleteDtor  C0->C1  match re/match/golf_raw_10.cpp; callee 0x004a01a0 (freeLoggerBuf), operator delete 0x004a4ffc
2026-10-06  004a0140  regAtexit_4a0140  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  004a0130  initLogFile  C0->C1  match re/match/golf_raw_05.cpp; string 0x004e4a44 (string in the binary); callee 0x004a0180; writes global 0x00840930
2026-10-06  004a00f0  resolveAndOpenFile  C0->C1  match re/match/golf_raw_10.cpp; callee resolvePath 0x00491da0; callers open 0x00487fb0, loggerSetPath, logWrite
2026-10-06  004a00a0  loadGraphsyObject  C0->C1  match re/match/golf_raw_11.cpp; string 0x004e4a2c (string in the binary), 0x004a00ba (string in the binary); caller init 0x004855b0; writes global 0x0084092c
2026-10-06  004a0060  freeGraphsyObject  C0->C1  match re/match/golf_raw_11.cpp; callers 0x00485740, 0x004a00a0; frees global 0x0084092c
2026-10-06  0049ff30  freeFonts  C0->C1  match re/match/golf_raw_06.cpp; callee 0x00473ae0; frees font globals 0x008408c8/0x008408f8
2026-10-06  0049fe50  loadFonts  C0->C1  match re/match/golf_hand_07_49fe50.cpp; string 0x004e49fc (string in the binary); writes font globals 0x008408c8/0x008408f8
2026-10-06  0049fda0  optionDialogOnMouse  C0->C1  match re/match/golf_hand_04.cpp; UAEXHH virtual
2026-10-06  0049fa90  optionDialogOnKey  C0->C1  match re/match/golf_hand_09_b.cpp; callee refresh 0x00480ce0, flagToggle49eec0, bitTest, optionListDraw
2026-10-06  0049f9a0  optionDialogUpdate  C0->C1  callee flagToggle49ee90, flagIsSet, find 0x004899d0, get 0x00489a30, 0x0049f370 (optionListDraw)
2026-10-06  0049f370  optionListDraw  C0->C1  callers 0x0049f9a0, onKey 0x0049fa90; callee draw 0x00473e60, drawRight 0x004781f0, drawCentered 0x00477cd0, _strchr
2026-10-06  0049f050  optionDialogDraw  C0->C1  caller optionDialogBuild 0x0049ed20, stop 0x004a11c0; callee setQuad 0x00476310, put 0x00477250 (Text477::put)
2026-10-06  0049f030  bitTest  C0->C1  match re/match/golf_hand_00.cpp; callers flagToggle49eec0, 0x0049f9a0, onKey 0x0049fa90
2026-10-06  0049eff0  bitSet  C0->C1  match re/match/golf_hand_01.cpp; caller flagToggle49eec0
2026-10-06  0049ef80  flagIsSet  C0->C1  match re/match/golf_hand_03.cpp; callers flagToggle49ee90, 0x0049f9a0
2026-10-06  0049eef0  flagSet  C0->C1  match re/match/golf_hand_04.cpp; caller flagToggle49ee90
2026-10-06  0049eec0  flagToggle49eec0  C0->C1  match re/match/golf_hand_01.cpp; callee setBit 0x0049eff0, test 0x0049f030
2026-10-06  0049ee90  flagToggle49ee90  C0->C1  match re/match/golf_hand_01.cpp; callee setFlag 0x0049eef0, isSet 0x0049ef80
2026-10-06  0049ed20  optionDialogBuild  C0->C1  caller 0x004a08a0 (panelDispatch); callee create 0x004806c0, setQuad 0x00476310, 0x0049f050 (optionDialogDraw)
2026-10-06  0049ece0  viewFieldCleanup49ece0  C0->C1  match re/match/golf_hand_r1.cpp; callee reset 0x004894b0, 0x00480610; callers dispatch 0x004a1250/0x004a12e0
2026-10-06  0049ec80  viewFieldDtor49ec80  C0->C1  match re/match/golf_hand_02.cpp; callee 0x0049ece0 (viewFieldCleanup49ece0)
2026-10-06  0049ebb0  viewFieldCtor49ebb0  C0->C1  match re/match/golf_hand_06_f.cpp; caller 0x004a0740 (viewCtor4a0740); callee 0x004804a0, 0x00489150
2026-10-06  0049eb70  regAtexit_49eb70  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049eb60  initGfxGlobal_8408f8  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x008408f8
2026-10-06  0049eb10  regAtexit_49eb10  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049eb00  initGfxGlobal_8408c8  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x008408c8
2026-10-06  0049ead0  freeCursors  C0->C1  match re/match/golf_raw_06.cpp; callee 0x00473ae0; frees cursor globals 0x00840830/0x00840860/0x00840890
2026-10-06  0049e9d0  loadCursors  C0->C1  match re/match/golf_hand_r2.cpp; string 0x004e49fc (string in the binary); writes cursor globals 0x00840830/0x00840860/0x00840890
2026-10-06  0049e7d0  listPanelReadValue  C0->C1  callee value477580 0x00477580; reads widget value fields
2026-10-06  0049e450  listPanelRefresh  C0->C1  callee 0x0049dab0 (listPanelDraw), refresh 0x00480ce0
2026-10-06  0049e230  listPanelOnScroll  C0->C1  match re/match/golf_hand_s0.cpp; callee 0x0049dab0 (listPanelDraw), refresh 0x00480ce0, getSel 0x00489950
2026-10-06  0049dab0  listPanelDraw  C0->C1  callers 0x0049e230 (listPanelOnScroll), 0x0049e450; callee setQuad 0x00476310, drawCentered 0x00477cd0, drawRight 0x004781f0, draw 0x00473e60
2026-10-06  0049d770  panelBuildText  C0->C1  caller 0x0049d6d0, stop 0x004a11c0; callee put 0x00477250 (Text477::put), get 0x00477560
2026-10-06  0049d6d0  panelBuildRow  C0->C1  caller 0x004a08a0 (panelDispatch); callee create 0x004806c0, 0x0049d770 (panelBuildText)
2026-10-06  0049d690  viewFieldCleanup49d690  C0->C1  match re/match/golf_hand_r2.cpp; callee reset 0x004894b0, 0x00480610; callers dispatch 0x004a1250/0x004a12e0
2026-10-06  0049d5a0  viewFieldCtor49d5a0  C0->C1  match re/match/golf_hand_07_49d5a0.cpp; caller 0x004a0740 (viewCtor4a0740); callee 0x004804a0, 0x00489150
2026-10-06  0049d560  regAtexit_49d560  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049d550  initGfxGlobal_840860  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x00840860
2026-10-06  0049d500  regAtexit_49d500  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049d4f0  initGfxGlobal_840890  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x00840890
2026-10-06  0049d4a0  regAtexit_49d4a0  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049d490  initGfxGlobal_840830  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x00840830
2026-10-06  0049d460  freeFileWinBuf  C0->C1  match re/match/golf_small18.cpp; callee _free; frees global 0x00840820
2026-10-06  0049d3b0  loadFileWinResource  C0->C1  match re/match/golf_small21.cpp; string 0x004e4680 (string in the binary), 0x004e4a08 (string in the binary); callee mgrSet 0x00487e70, _malloc; writes global 0x00840820
2026-10-06  0049d370  regAtexit_49d370  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049d360  initGfxGlobal_8407f0  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x008407f0
2026-10-06  0049d310  regAtexit_49d310  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049d300  initGfxGlobal_840790  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x00840790
2026-10-06  0049d2b0  regAtexit_49d2b0  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049d2a0  initGfxGlobal_8407c0  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x008407c0
2026-10-06  0049d280  freeGfxGlobal_840710  C0->C1  match re/match/golf_raw_06.cpp; callee 0x00473ae0; frees global 0x00840710; caller cleanup 0x00485740
2026-10-06  0049d1b0  loadPcxSurface  C0->C1  match re/match/golf_hand_06_f.cpp; string 0x004e49fc (string in the binary); callee init 0x004745c0, 0x00475840; writes global 0x00840710
2026-10-06  0049d170  regAtexit_49d170  C0->C1  match re/match/golf_raw_10.cpp; callee _atexit
2026-10-06  0049d160  initGfxGlobal_840710  C0->C1  match re/match/golf_raw_05.cpp; callee ctor 0x00473ab0; writes global 0x00840710
2026-10-06  0049d100  strDup  C0->C1  match re/match/golf_small7.cpp; callee alloc 0x00474860
2026-10-06  0049d0e0  strFree  C0->C1  match re/match/golf_raw_05.cpp; callee dtor 0x004747e0; caller strAssign
2026-10-06  0049d090  strAssign  C0->C1  match re/match/golf_small7.cpp; string 0x004e49f4 (string in the binary); callee alloc 0x00474820, dup 0x0049d100
2026-10-06  0049d050  allocPairArray  C0->C1  match re/match/golf_raw_05.cpp; callee 0x004747a0
2026-10-06  0049d020  regAtexit_49d020  C0->C1  match re/match/golf_raw_09.cpp; callee _atexit
2026-10-06  0049d010  initPairArrayGlobal  C0->C1  match re/match/golf_raw_05.cpp; callee 0x0049d050; writes global 0x008406e8
2026-10-06  0049cf50  movieSelectStream  C0->C1  match re/match/golf_hand_05.cpp; caller 0x0049ccc0 (moviePlayFrame)
2026-10-06  0049ccc0  moviePlayFrame  C0->C1  string 0x004e49e4 (string in the binary), 0x004e49d4 (string in the binary); callee movieReadFrame, select 0x0049cf50, removeHead 0x004a4ed0
2026-10-06  0049cb90  movieReadFrame  C0->C1  match re/match/golf_hand_r2.cpp; string 0x004e4974 (string in the binary), 0x004e499c (string in the binary); callee push 0x004a4db0 (listPush)
2026-10-06  0049cb80  movieStepReturn1  C0->C1  match re/match/golf_hand_00.cpp; returns constant 1
2026-10-06  0049cb20  movieReadProcess  C0->C1  match re/match/golf_hand_02.cpp; string 0x004e4960 (string in the binary); callee movieReadFrame 0x0049cb90, moviePlayFrame
2026-10-06  0049cae0  movieDecodeStart  C0->C1  match re/match/golf_classes.cpp; string 0x004e493c (string in the binary); callee 0x0049cb80, 0x0049ccc0, logLine
2026-10-06  0049c940  movieAdvanceFrame  C0->C1  string 0x004e4924 (string in the binary), 0x004e4910 (string in the binary), 0x004e48f4 (string in the binary); callee logLine 0x004a0320
2026-10-06  0049c910  serviceMovieFrame  C0->C1  match re/match/golf_raw_05.cpp; callee 0x0049cae0 (movieDecodeStart), 0x0049cb20 (movieReadProcess), 0x0049ccc0 (moviePlayFrame)
2026-10-06  0049c8e0  serviceMovie  C0->C1  callers showScreen/hideScreen/redrawAll 0x00483bb0..; callee 0x0049c910 (serviceMovieFrame)
2026-10-06  0049c800  globalObjDeleteDtor49c800  C0->C1  match re/match/golf_raw_09.cpp; callee 0x0049c780 (globalObjDtor49c780), operator delete 0x004a4ffc
2026-10-06  0049c780  globalObjDtor49c780  C0->C1  match re/match/golf_hand_04.cpp; callee member dtors 0x004805a0/0x00486ce0, 0x0049c0a0
2026-10-06  0049c020  ctorGlobalObj49c020  C0->C1  match re/match/golf_hand_r1.cpp; callee ctors 0x004804a0, 0x004837f0, 0x00486c90
2026-10-06  0049c000  regAtexit_49c000  C0->C1  match re/match/golf_raw_09.cpp; callee _atexit 0x004a56d2
2026-10-06  0049bff0  initGlobalObj_49c020  C0->C1  match re/match/golf_raw_05.cpp; callee 0x0049c020 (ctorGlobalObj49c020)
2026-10-06  0049bec0  netListRemove  C0->C1  match re/match/golf_hand_05.cpp; caller 0x0049acf0 (netHandleControl)
2026-10-06  0049b970  netHeapAlloc  C0->C1  string 0x004c1434 (string in the binary), 0x004c1458 (string in the binary); callee _malloc, _exit, alloc 0x00474860
2026-10-06  0049b7b0  netPoll  C0->C1  match re/match/golf_hand_r0.cpp; callers 0x00497fc0/0x00499140; callee pop 0x004a4c70 (queuePop), netQueueMessage
2026-10-06  0049b690  netQueueMessage  C0->C1  match re/match/golf_hand_08_h.cpp; caller 0x0049b7b0 (netPoll); callee netSendList, 0x0049b970 (netHeapAlloc)
2026-10-06  0049acf0  netHandleControl  C0->C1  string 0x004e4824 (string in the binary), 0x004e4838 (string in the binary), 0x004e484c (string in the binary); callee netNameOf, remove 0x0049bec0
2026-10-06  0049ab40  netServiceControl  C0->C1  caller gfxFlushB 0x00497b20; callee 0x0049acf0 (netHandleControl), 0x00499140
2026-10-06  0049aa70  netPump  C0->C1  match re/match/golf_hand_06_l.cpp; callee 0x00497fc0 (netService), 0x00499140 (netSendMessages); caller gfxFlushA
2026-10-06  0049aa30  netNameOf  C0->C1  match re/match/golf_hand_01.cpp; callers 0x00497fc0/0x00499140/0x0049acf0
2026-10-06  00499140  netSendMessages  C0->C1  string 0x004e47fc (string in the binary); callers netSendList/netFlush/netPump; callee _malloc/_free, add 0x004a4b60
2026-10-06  00497fc0  netService  C0->C1  string 0x004e4680 (string in the binary), 0x004e47b0 (string in the binary), 0x004e47e8 (string in the binary); caller 0x0049aa70 (netPump)
2026-10-06  00497d10  netSendShutdown  C0->C1  match re/match/golf_hand_s2.cpp; callers 0x00497fc0/0x00499140/0x0049b7b0; callee clear 0x004a4b00, _free
2026-10-06  00497cc0  netFlush  C0->C1  match re/match/golf_hand_01.cpp; callee 0x00499140 (netSendMessages)
2026-10-06  00497c70  netTimerReadB  C0->C1  match re/match/golf_raw_05.cpp; reads global 0x008400b0
2026-10-06  00497c20  netTimerReadA  C0->C1  match re/match/golf_raw_05.cpp; reads global 0x008400b0
2026-10-06  00497b40  netSendList  C0->C1  match re/match/golf_hand_07_497b40.cpp; caller 0x0049b690 (netQueueMessage); callee 0x00499140 (netSendMessages), _malloc/_free
2026-10-06  00497b20  gfxFlushB  C0->C1  match re/match/golf_small17.cpp; callers showScreen/redrawAll; callee 0x0049ab40 (netServiceControl)
2026-10-06  00497b00  gfxFlushA  C0->C1  match re/match/golf_small17.cpp; callers showScreen/hideScreen 0x00483bb0/0x00483c10; callee 0x0049aa70 (netPump)
2026-10-06  00497a20  comboDtor  C0->C1  match re/match/golf_hand_06_e.cpp; callee 0x004961d0 (controlInitBase), reset 0x004a14c0, member dtors
2026-10-06  00497a00  comboDeleteDtor  C0->C1  match re/match/golf_raw_09.cpp; callee 0x00497a20 (comboDtor), operator delete 0x004a4ffc
2026-10-06  004979a0  scrollbarResetState  C0->C1  match re/match/golf_hand_02.cpp; caller 0x004967d0
2026-10-06  004974d0  scrollbarThumbRect  C0->C1  caller 0x00496fc0 (scrollbarDraw); reads this+0x5ac RECT, +0x580/+0x584/+0x58c range, +0x574 flags
2026-10-06  004973c0  scrollbarScrollBy  C0->C1  match re/match/golf_hand_08_f.cpp; reads 0x0083ab2c, 0x0083ff18
2026-10-06  00496fc0  scrollbarDraw  C0->C1  match re/match/golf_hand_r0.cpp; callee 0x004974d0 (scrollbarThumbRect), paint 0x004808c0, draw 0x00473e60
2026-10-06  00496e30  scrollbarOnKey  C0->C1  match re/match/golf_hand_r0.cpp; reads global 0x0083ab2c, 0x0083ff18
2026-10-06  00496d20  scrollbarOnDrag  C0->C1  match re/match/golf_raw_12.cpp; same globals 0x0083ab30/0x0083ab34 as scrollbarOnDown
2026-10-06  00496c30  scrollbarOnDown  C0->C1  match re/match/golf_hand_07_496c30.cpp; reads/writes globals 0x0083ab30/0x0083ab34
2026-10-06  004967f0  setWidgetScalar  C0->C1  match re/match/golf_raw_13.cpp; callers 0x0047b9f0/0x0047ba10; writes field at this via a single int arg
2026-10-06  004967d0  scrollbarResetThumb  C0->C1  match re/match/golf_raw_04.cpp; callee 0x004979a0 (scrollbarResetState); caller 0x00496330
2026-10-06  00496770  orderPair  C0->C1  match re/match/golf_raw_09.cpp; callee 0x00493580 (swap493580); callers 0x0047ba30/0x0047ba50
2026-10-06  00496740  scrollbarCreateB  C0->C1  match re/match/golf_raw_04.cpp; caller 0x0047a4c0; callee 0x004966d0
2026-10-06  00496710  scrollbarCreateA  C0->C1  match re/match/golf_raw_04.cpp; caller 0x0047a4c0; callee 0x00496690
2026-10-06  004966d0  scrollbarCreateB_impl  C0->C1  match re/match/golf_raw_04.cpp; callee 0x00496330 (scrollbarCreate); caller 0x00496740
2026-10-06  00496690  scrollbarCreateA_impl  C0->C1  match re/match/golf_raw_04.cpp; callee 0x00496330 (scrollbarCreate); caller 0x00496710
2026-10-06  00496330  scrollbarCreate  C0->C1  caller 0x00496690/0x004966d0; callee 0x004961d0 (controlInitBase), 0x004967d0, create widgets; sets this+0x574 flags, +0x578 page, +0x580/+0x584 min/max
2026-10-06  004961d0  controlInitBase  C0->C1  match re/match/golf_hand_r3.cpp; caller 0x00496330 (scrollbarCreate), 0x00497a20 (comboDtor); callee 0x00480610
2026-10-06  00496030  buildLabeledControl  C0->C1  match re/match/golf_hand_r2.cpp; callee 0x004804a0, 0x004a13f0 (ctorObj4a13f0); reads the 11-string label table at 0x004e4764
2026-10-06  00495eb0  resetWidgetTable  C0->C1  match re/match/golf_raw_06.cpp; writes the same table g_widgetTable 0x0083fe78; callers 0x00479f30, 0x0047a3c0, 0x0047d7c0
2026-10-06  00495d30  initWidgetTable  C0->C1  match re/match/golf_hand_r2.cpp; caller 0x00479c40; writes the 0x60-entry table at g_widgetTable 0x0083fe78
2026-10-06  00495420  decodeImage16  C0->C1  string 0x004e4748 (string in the binary); callee 0x00493630 (fill16), paint 0x00474e70; caller 0x00478cd0
2026-10-06  00495100  decodeImagePaletted  C0->C1  string 0x004e472c (string in the binary); callee setPalette 0x004830e0, paint 0x00474e70, _strncmp; caller 0x00478cd0 (load@r3_C478cd0)
2026-10-06  00494f00  drawColorRunB  C0->C1  caller 0x00478c70; sibling of 0x00494d30, same palette-lookup-then-write-16bit shape
2026-10-06  00494d30  drawColorRunA  C0->C1  match re/match/golf_hand_03.cpp... (golf_raw); caller 0x00478c10; reads a widget palette via vtbl +0xcc/+0xe4, writes ushort pixels
2026-10-06  00494cb0  setHudTextSlot  C0->C1  match re/match/golf_hand_03.cpp; callers 0x00497fc0/0x00499140/0x0049acf0 (net); writes g_hudTextX 0x0083e8b8, g_hudTextY 0x0083d3c8, g_hudTextBuf 0x0083e8e0
2026-10-06  004942f0  expandTextMarkup  C0->C1  string 0x004e423c (string in the binary), 0x004e4248 (string in the binary), 0x004e4708 (string in the binary), 0x004e4714 (string in the binary), 0x004e471c (string in the binary); callee 0x004935f0, _strstr, _strncmp, __itoa
2026-10-06  004942a0  comboCurrentIndex  C0->C1  match re/match/golf_hand_02.cpp; caller 0x00493a60 (comboRefresh)
2026-10-06  004941e0  listCurrentIndex  C0->C1  match re/match/golf_hand_05.cpp; caller 0x00493a60 (comboRefresh)
2026-10-06  004940e0  comboFindItem  C0->C1  caller 0x004942f0; thiscall, walks the node list at this+0x2d98 base (nodes +4 id, +0xc next)
2026-10-06  00494020  listSetItems  C0->C1  match re/match/golf_hand_r0.cpp; callee moveTo 0x0047b420
2026-10-06  00493f50  comboLayoutItems  C0->C1  match re/match/golf_hand_06_o.cpp; callee draw 0x00473e60, 0x00476140
2026-10-06  00493ef0  comboSetItems  C0->C1  match re/match/golf_hand_03.cpp; callee 0x00493f50 (comboLayoutItems)
2026-10-06  00493b90  comboOpenPopup  C0->C1  caller 0x004936a0 (comboInit); callee toParent 0x0047b170, moveTo 0x0047b420, 0x00493a60 (comboRefresh)
2026-10-06  00493a60  comboRefresh  C0->C1  match re/match/golf_hand_r2.cpp; callee 0x00493f50 (comboLayoutItems), drawBox 0x00477e60, setQuad, 0x004941e0/0x004942a0 (current index)
2026-10-06  00493a30  comboRefreshThunk  C0->C1  match re/match/golf_raw_04.cpp; callee 0x00493a60 (comboRefresh)
2026-10-06  004936a0  comboInit  C0->C1  caller 0x004775b0; callee 0x00493ef0 (comboSetItems), 0x00494020 (listSetItems), 0x00493a60 (comboRefresh), setValue/create widgets
2026-10-06  00493630  fill16  C0->C1  match re/match/golf_hand_01.cpp; caller 0x00495420 (decodeImage16)
2026-10-06  004935f0  findByteInRange  C0->C1  match re/match/golf_hand_01.cpp; caller 0x004942f0 (expandTextMarkup)
2026-10-06  004935b0  reverseFindChar  C0->C1  none
2026-10-06  00493580  swapInts  C0->C1  none
2026-10-06  00493520  fillConvexPoly  C0->C1  callee 0x00493100, callee 0x004932d0
2026-10-06  004932d0  fillConvexPoly16  C0->C1  callee 0x00492ed0, callee 0x00493000
2026-10-06  00493100  fillConvexPoly8  C0->C1  callee 0x00492ed0, callee 0x00493000
2026-10-06  00493080  fillSpanUp  C0->C1  global 0x0083d34c
2026-10-06  00493000  fillSpanDown  C0->C1  global 0x0083d34c (fill colour)
2026-10-06  00492fa0  polyEdgeAdvance  C0->C1  callee 0x00492ed0
2026-10-06  00492ed0  polyEdgeStep  C0->C1  global 0x0083d358 (vertex table)
2026-10-06  00492e80  MappedFile::close  C0->C1  import UnmapViewOfFile, import CloseHandle
2026-10-06  00492dd0  MappedFile::open  C0->C1  import CreateFileA, import CreateFileMappingA, import MapViewOfFile
2026-10-06  00492dc0  MappedFile::dtor  C0->C1  vtable 0x004bba78, callee 0x00492e80
2026-10-06  00492da0  MappedFile::scalarDtor  C0->C1  callee 0x00492dc0
2026-10-06  00492d80  MappedFile::ctor  C0->C1  vtable 0x004bba78
2026-10-06  00492d40  HotList::cancelTip  C0->C1  callee 0x00486ec0
2026-10-06  00492cc0  HotList::onMouseMove  C0->C1  callee 0x00486d20 (Timer::init), callee 0x00486e40 (Timer::startRepeat)
2026-10-06  00492bd0  HotList::showTip  C0->C1  callee 0x00486ec0 (Timer::stop), callee 0x00480220 (draw text)
2026-10-06  00492b10  HotList::hitTestRect  C0->C1  callee 0x00492610
2026-10-06  00492a90  HotList::hitTest  C0->C1  callee 0x00492610 (pointInRect)
2026-10-06  004929b0  HotList::add  C0->C1  import malloc
2026-10-06  00492920  HotList::alloc  C0->C1  import operator new
2026-10-06  004928d0  HotList::dtor  C0->C1  vtable 0x004bba74, callee 0x00492830
2026-10-06  004928b0  HotList::scalarDtor  C0->C1  callee 0x004928d0
2026-10-06  00492850  HotList::ctor  C0->C1  vtable 0x004bba74, callee 0x00486c90 (Timer::ctor)
2026-10-06  00492830  HotList::releaseArray  C0->C1  callee 0x00492800
2026-10-06  00492800  HotList::resetFields  C0->C1  callee 0x00486f10 (Timer::reset)
2026-10-06  00492690  HotList::grow  C0->C1  import operator new
2026-10-06  00492660  HotList::freeEntryTip  C0->C1  import free
2026-10-06  00492610  pointInRect  C0->C1  caller 0x00488cf0
2026-10-06  004925f0  appendNewline  C0->C1  none
2026-10-06  004925d0  stripNewline  C0->C1  import strrchr
2026-10-06  004925b0  trimSpaces  C0->C1  callee 0x00492570, callee 0x004924e0
2026-10-06  00492570  trimTrailingSpace  C0->C1  import isspace
2026-10-06  004924e0  trimLeadingSpace  C0->C1  import isspace
2026-10-06  004924b0  flushBlitSurface  C0->C1  none
2026-10-06  00492470  shutdownBlitter  C0->C1  callee 0x004924b0, caller 0x00485740 (engineShutdown)
2026-10-06  00492460  blitError  C0->C1  none
2026-10-06  00492450  blitErrorPtr  C0->C1  none
2026-10-06  00492000  cropSprite  C0->C1  callee 0x00473ae0, callee app vtable+0x80 (createSurface), import malloc
2026-10-06  00491da0  resolveDataPath  C0->C1  import FindFirstFileA, import FindClose
2026-10-06  00491d80  cosScaled  C0->C1  caller 0x00491c10
2026-10-06  00491c70  sinScaled  C0->C1  caller 0x00491c10
2026-10-06  00491c10  initSinTable  C0->C1  import fsin
2026-10-06  00491710  Dialog::dtor  C0->C1  vtable 0x004bb3f8, callee 0x0048d480, callee 0x004928d0 (HotList::dtor)
2026-10-06  004916f0  Dialog::scalarDtor  C0->C1  callee 0x00491710
2026-10-06  00491680  Dialog::childDtor  C0->C1  vtable 0x004bb744, callee 0x0049d690
2026-10-06  00491500  Dialog::viewDtor  C0->C1  vtable 0x004bba3c, callee 0x0049d690, callee 0x00489de0
2026-10-06  004914d0  Dialog::subDtor  C0->C1  callee 0x00491500, callee 0x00489370
2026-10-06  004914b0  Widget::setFlagBit1  C0->C1  none
2026-10-06  00491490  Widget::setFlagBit0  C0->C1  none
2026-10-06  00491470  LabelButton::scalarDtor  C0->C1  callee 0x00491410
2026-10-06  00491410  LabelButton::dtor  C0->C1  vtable 0x004bb5b4, callee 0x00488650
2026-10-06  00491310  TextView::create  C0->C1  callee 0x0048ce00 (TextView::ctor), import operator new
2026-10-06  0048de90  MsgBox::setLabelB  C0->C1  caller 0x00490bf0 (passes Cancel), import malloc
2026-10-06  0048de00  MsgBox::setLabelA  C0->C1  caller 0x00490bf0, import malloc
2026-10-06  00490d20  MsgBox::setColorB  C0->C1  none
2026-10-06  00490cf0  MsgBox::setButtonB  C0->C1  none
2026-10-06  00490cc0  MsgBox::setColorA  C0->C1  none
2026-10-06  00490c80  MsgBox::setButtonA  C0->C1  none
2026-10-06  00490c30  MsgBox::freeLabels  C0->C1  import free
2026-10-06  00490bf0  MsgBox::initDefaults  C0->C1  string 0x004c8590 (Cancel), callee 0x0048de90, callee 0x0048de00
2026-10-06  00490a40  TextView::updateSelection  C0->C1  callee 0x00489950
2026-10-06  00490960  TextView::select  C0->C1  callee 0x004a1370, callee 0x00489950
2026-10-06  0048fe60  TextView::render  C0->C1  callee 0x00476e20 (wrap), callee 0x00477250 (put text), callee 0x00489f50
2026-10-06  0048e900  TextView::build  C0->C1  string 0x004c8590 (Cancel), callee 0x004806c0 (Widget::create), callee 0x004887c0 (Button::create)
2026-10-06  0048e1c0  TextView::layout  C0->C1  none
2026-10-06  0048e190  TextView::setSize  C0->C1  none
2026-10-06  0048e120  TextView::setFontSize  C0->C1  callee app vtable+0xa8
2026-10-06  0048e0b0  TextView::setLineSpacing  C0->C1  callee app vtable+0xa8 (screen width)
2026-10-06  0048e010  TextView::setName  C0->C1  import malloc, import free
2026-10-06  0048dfc0  TextView::addImage  C0->C1  callee 0x00401d10
2026-10-06  0048df20  TextView::addText  C0->C1  callee 0x00401d10, callee 0x004a0600
2026-10-06  0048db60  TextView::init  C0->C1  callee 0x0048d480, callee 0x00474820, callee 0x0048e190
2026-10-06  0048d480  TextView::reset  C0->C1  callee 0x00489e40, callee 0x0049d690 (cleanup)
2026-10-06  0048ce00  TextView::ctor  C0->C1  vtable 0x004bb3f8, callee 0x004804a0 (Widget::ctor), callee 0x00492850 (HotList::ctor)
2026-10-06  0048cd80  matchDirective  C0->C1  string 0x004e4584 keyword table, callee 0x004925b0 (trim)
2026-10-06  0048caf0  ListBox::scrollBy  C0->C1  callee 0x0047b9f0
2026-10-06  0048cac0  ListBox::clearSelection  C0->C1  none
2026-10-06  0048c8a0  ListBox::onDrag  C0->C1  callee 0x00480ce0
2026-10-06  0048c6e0  ListBox::onMouse  C0->C1  import ReleaseCapture, callee 0x0048a120, callee 0x0048a5d0
2026-10-06  0048c640  ListBox::addItem  C0->C1  callee 0x00476e20 (text measure), callee 0x00401d10
2026-10-06  0048c610  ListBox::forwardClick  C0->C1  none
2026-10-06  0048c560  ListBox::fireSelectChanged  C0->C1  callee 0x00489950
2026-10-06  0048c420  ListBox::onScroll  C0->C1  callee 0x00489f50, callee 0x00480ce0
2026-10-06  0048aea0  ListBox::typeAhead  C0->C1  import tolower, callee 0x00489950, callee 0x00480ce0 (refresh)
2026-10-06  0048aa00  ListBox::onNavKey  C0->C1  callee 0x00489f50 (setSelection)
2026-10-06  0048a5d0  ListBox::build  C0->C1  callee 0x004806c0 (Widget::create), callee 0x0048a120, callee 0x0047ba30 (scroll range)
2026-10-06  0048a120  ListBox::measure  C0->C1  callee 0x00477250 (text width), callee 0x00476310 (setQuad)
2026-10-06  00489f50  ListBox::setSelection  C0->C1  callee 0x00489950, callee 0x00489a30, callee 0x0047b9f0 (scroll)
2026-10-06  00489ed0  ListBox::selectById  C0->C1  callee 0x00489f50
2026-10-06  00489e40  ListBox::resetFields  C0->C1  callee 0x00480610, callee 0x004894b0
2026-10-06  00489de0  ListBox::dtor  C0->C1  vtable 0x004bb3cc, callee 0x00489e40
2026-10-06  00489cb0  ListBox::ctor  C0->C1  vtable 0x004bb3cc, callee 0x004804a0 (Widget::ctor), callee 0x00489150 (ListModel::ctor)
2026-10-06  00489c90  ListBox::subDtor  C0->C1  vtable 0x004ba278
2026-10-06  00489b30  ListModel::destroyNodes  C0->C1  vtable 0x004bb22c
2026-10-06  00489ab0  ListModel::setClickHandler  C0->C1  none
2026-10-06  00489a30  ListModel::idAt  C0->C1  none
2026-10-06  004899d0  ListModel::findId  C0->C1  none
2026-10-06  00489950  ListModel::selectedId  C0->C1  none
2026-10-06  004898d0  ListModel::indexOfId  C0->C1  none
2026-10-06  00489890  ListModel::append  C0->C1  callee 0x00401d10
2026-10-06  004897f0  ListModel::reset  C0->C1  callee 0x004894b0
2026-10-06  004896b0  ListModel::setKey  C0->C1  callee 0x00474820
2026-10-06  004894b0  ListModel::clear  C0->C1  none
2026-10-06  00489370  ListModel::dtor  C0->C1  vtable 0x004bb21c, callee 0x004894b0
2026-10-06  00489350  ListModel::scalarDtor  C0->C1  callee 0x00489370
2026-10-06  00489150  ListModel::ctor  C0->C1  vtable 0x004bb21c, callee 0x004747a0
2026-10-06  004890e0  Button::setMode  C0->C1  none
2026-10-06  00488fd0  Button::onTimer  C0->C1  callee 0x00486ec0, callee 0x0047b170
2026-10-06  00488cf0  Button::onMouseMove  C0->C1  callee 0x00492610 (pointInRect), callee 0x00486ec0 (Timer::stop)
2026-10-06  00488a20  Button::setTooltip  C0->C1  import malloc, import free
2026-10-06  004889f0  setButtonDefaults  C0->C1  caller 0x004855b0 (engineInit)
2026-10-06  004889b0  Button::setColorPressed  C0->C1  callee 0x00476370
2026-10-06  00488970  Button::setColorHover  C0->C1  callee 0x00476340
2026-10-06  00488930  Button::setColorNormal  C0->C1  callee 0x00476310 (Widget::setQuad)
2026-10-06  004887c0  Button::create  C0->C1  callee 0x004806c0 (Widget::create), callee 0x00476310 (setQuad), import malloc
2026-10-06  004886d0  Button::resetFields  C0->C1  import free
2026-10-06  00488650  Button::dtor  C0->C1  vtable 0x004bb0a8, callee 0x004886d0
2026-10-06  00488630  Button::scalarDtor  C0->C1  callee 0x00488650
2026-10-06  00488500  Button::ctor  C0->C1  vtable 0x004bb0a8, callee 0x004804a0 (Widget::ctor), callee 0x00486c90 (Timer::ctor)
2026-10-06  004884e0  cacheCursorMetrics  C0->C1  import GetSystemMetrics
2026-10-06  004884b0  Cursor::destroy  C0->C1  import DestroyCursor
2026-10-06  00488490  Cursor::ctor  C0->C1  vtable 0x004ba7f4
2026-10-06  00488460  Reader::scalarDtor  C0->C1  vtable 0x004bb084, callee 0x00487f30
2026-10-06  00488420  KeyTable::lookup  C0->C1  callee 0x00488310
2026-10-06  00488310  KeyTable::find  C0->C1  string 0x004e4418 (.txt), import strcmpi
2026-10-06  004882f0  KeyTable::dtor  C0->C1  vtable 0x004bb088, callee 0x00474810
2026-10-06  004882d0  KeyTable::scalarDtor  C0->C1  callee 0x004882f0
2026-10-06  004882a0  KeyTable::ctor  C0->C1  vtable 0x004bb088, callee 0x004747a0
2026-10-06  00488230  Reader::readLine  C0->C1  import fgets, callee 0x004925d0 (stripNewline), callee 0x004925b0 (trim)
2026-10-06  00487fb0  Reader::open  C0->C1  string 0x004d3884, string 0x004e442c (seek log), import fgets, import fseek
2026-10-06  00487f80  Reader::closeFile  C0->C1  import fclose
2026-10-06  00487f30  Reader::free  C0->C1  import free, callee 0x00487f80
2026-10-06  00487ee0  Reader::allocBuffers  C0->C1  import malloc
2026-10-06  00487ea0  Reader::ctor  C0->C1  vtable 0x004bb084, callee 0x00487ee0
2026-10-06  00487e90  readLineGlobal  C0->C1  callee 0x00488230 (Reader::readLine)
2026-10-06  00487e70  readSectionGlobal  C0->C1  callee 0x00487fb0 (Reader::open)
2026-10-06  00487e60  destroyGlobalReader  C0->C1  callee 0x00487f80
2026-10-06  00487e30  registerKeyTableAtexit  C0->C1  import atexit
2026-10-06  00487e10  initKeyTableArray  C0->C1  callee 0x004882a0
2026-10-06  00487d90  registerReaderAtexit  C0->C1  import atexit
2026-10-06  00487d80  initGlobalReader  C0->C1  callee 0x00487ea0
2026-10-06  00487ce0  NodeList::clearScalarDtor  C0->C1  vtable 0x004bafac, callee 0x004a4ffc
2026-10-06  00487c40  releaseWithHookC  C0->C1  none
2026-10-06  00487c00  resourceLoad  C0->C1  callee 0x004a4ffc
2026-10-06  00487bd0  closeHookC  C0->C1  global 0x0083af90
2026-10-06  00487b80  NodeListC::dtor  C0->C1  vtable 0x004bb014, callee 0x00487260
2026-10-06  00487b60  NodeListC::scalarDtor  C0->C1  callee 0x00487b80
2026-10-06  00487b40  NodeListC::ctor  C0->C1  vtable 0x004bb014, callee 0x00487210
2026-10-06  00487ac0  releaseWithHookB  C0->C1  callee 0x004879f0
2026-10-06  00487a70  resourceLoadB  C0->C1  callee closeHookB 0x004879f0
2026-10-06  00487a60  NodeListB::dtor  C0->C1  vtable 0x004bafb0, callee 0x00487260
2026-10-06  00487a40  NodeListB::scalarDtor  C0->C1  callee 0x00487a60
2026-10-06  00487a20  NodeListB::ctor  C0->C1  vtable 0x004bafb0, callee 0x00487210
2026-10-06  004879f0  closeHookB  C0->C1  global 0x0083af88
2026-10-06  00487630  resourceQuery  C0->C1  none
2026-10-06  004877a0  HashTable::bucketSet  C0->C1  none
2026-10-06  00487770  HashTable::bucketEmpty  C0->C1  none
2026-10-06  004876c0  HashTable::remove  C0->C1  none
2026-10-06  00487460  releaseWithHookA  C0->C1  callee 0x00487430
2026-10-06  00487430  closeHookA  C0->C1  global 0x0083af80
2026-10-06  00487390  NodeList::dtor  C0->C1  vtable 0x004baf00, callee 0x00487310
2026-10-06  00487310  NodeList::clear  C0->C1  callee 0x004a4ffc
2026-10-06  004872f0  NodeList::scalarDtor  C0->C1  callee 0x00487390
2026-10-06  00487280  NodeList::ctor  C0->C1  vtable 0x004baf00, callee 0x004877d0 (array init)
2026-10-06  00487260  StrList::dtor  C0->C1  vtable 0x004baeac
2026-10-06  00487240  StrList::scalarDtor  C0->C1  callee 0x00487260
2026-10-06  00487210  StrList::ctor  C0->C1  vtable 0x004baeac
2026-10-06  00487180  BinkPlayer::frame  C0->C1  import BinkDoFrame, import BinkCopyToBuffer, import BinkBufferBlit
2026-10-06  00487090  BinkPlayer::play  C0->C1  import BinkOpen, import BinkBufferOpen, import BinkSetSoundSystem, import ShowCursor
2026-10-06  00487060  BinkPlayer::close  C0->C1  import BinkClose, import BinkBufferClose
2026-10-06  00487050  BinkPlayer::setTarget  C0->C1  none
2026-10-06  00487040  BinkPlayer::dtor  C0->C1  vtable 0x004baea8, callee 0x00487060
2026-10-06  00487020  BinkPlayer::scalarDtor  C0->C1  callee 0x00487040
2026-10-06  00487000  BinkPlayer::ctor  C0->C1  vtable 0x004baea8
2026-10-06  00486ff0  Timer::lockDec  C0->C1  caller 0x00485740 (engineShutdown)
2026-10-06  00486fe0  Timer::lockInc  C0->C1  caller 0x004855b0 (engineInit)
2026-10-06  00486f90  Timer::callback  C0->C1  import PostMessageA
2026-10-06  00486f10  Timer::reset  C0->C1  callee 0x00486ec0
2026-10-06  00486ec0  Timer::stop  C0->C1  import timeKillEvent, import KillTimer
2026-10-06  00486e40  Timer::startRepeat  C0->C1  import timeSetEvent, import SetTimer
2026-10-06  00486dc0  Timer::startOnce  C0->C1  import timeSetEvent, import SetTimer
2026-10-06  00486d90  Timer::initStartRepeat  C0->C1  callee 0x00486d20, callee 0x00486e40
2026-10-06  00486d60  Timer::initStartOnce  C0->C1  callee 0x00486d20, callee 0x00486dc0
2026-10-06  00486d20  Timer::init  C0->C1  callee 0x00486f10
2026-10-06  00486cf0  Timer::initA  C0->C1  callee 0x00486f10
2026-10-06  00486ce0  Timer::dtor  C0->C1  vtable 0x004baea0, callee 0x00486f10
2026-10-06  00486cc0  Timer::scalarDtor  C0->C1  callee 0x00486ce0
2026-10-06  00486c90  Timer::ctor  C0->C1  vtable 0x004baea0
2026-10-06  00486b30  EditBox::setCaretActive  C0->C1  callee 0x00486dc0 (Timer::startOnce), callee 0x00486ec0 (Timer::stop)
2026-10-06  004866f0  EditBox::insertChar  C0->C1  import MessageBeep
2026-10-06  004863e0  EditBox::pixelToCharPos  C0->C1  callee 0x004862b0, callee 0x00477280
2026-10-06  00486360  EditBox::charPosToPixel  C0->C1  callee 0x00486330, callee 0x00477280 (text measure)
2026-10-06  00486330  EditBox::lineIndexAt  C0->C1  caller 0x00486360
2026-10-06  004862b0  EditBox::wrapToLines  C0->C1  callee 0x00476e20 (text wrap)
2026-10-06  00486250  EditBox::resizeBuffer  C0->C1  import malloc, import strncpy
2026-10-06  00486200  EditBox::setText  C0->C1  import strncpy
2026-10-06  00486110  EditBox::create  C0->C1  callee 0x004806c0 (Widget::create), callee 0x00486cf0 (Timer::init)
2026-10-06  004860d0  EditBox::dtor  C0->C1  callee 0x00485ff0, callee 0x00480610
2026-10-06  00486070  EditBox::ctor  C0->C1  vtable 0x004ba4a8, callee 0x004804a0 (Widget::ctor), callee 0x00486c90 (Timer::ctor)
2026-10-06  00485ff0  EditBox::resetFields  C0->C1  caller 0x00486070 (EditBox::ctor), caller 0x004860d0
2026-10-06  00485e80  Image::vline  C0->C1  callee surface vtable 0xcc/0x14/0xe0 (stride)
2026-10-06  00485d40  Image::hline  C0->C1  callee surface vtable 0xcc (clipRect)/0x14 (lockBits)/0x24 (unlock)
2026-10-06  00485aa0  Image::savePcx  C0->C1  import fwrite
2026-10-06  00485790  Image::loadPcx  C0->C1  import fread, callee 0x00474e70 (surface resize), callee 0x004789f0
2026-10-06  00485740  engineShutdown  C0->C1  callee 0x00492470 (shutdownBlitter), callee 0x00486ff0 (Timer::lockDec), callee 0x00490c30 (MsgBox::freeLabels)
2026-10-06  004855b0  engineInit  C0->C1  callee 0x004a00a0 (create app), callee 0x00491c10 (initSinTable), callee 0x004884e0 (cacheCursorMetrics), callee 0x00486fe0 (Timer::lockInc)
2026-10-06  00485590  showMessageBox  C0->C1  import MessageBoxA
2026-10-06  004854c0  Screen::close  C0->C1  callee 0x00484110, callee 0x004a00a0
2026-10-06  004853d0  Screen::dtor  C0->C1  vtable 0x004badf8, callee 0x004841e0
2026-10-06  004853b0  Screen::scalarDtor  C0->C1  callee 0x004853d0 (Screen::dtor), callee 0x004a4ffc (operator delete)
2026-10-06  004852e0  VoiceRx::ctor  C0->C1  match re/match/golf_hand_05.cpp;string 0x004e43c8
2026-10-06  004852d0  Snd::setVtbl4852d0  C0->C1  match re/match/golf_raw_04.cpp
2026-10-06  004852b0  Snd::scalarDtor4852b0  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00485140  Snd::setVolume  C0->C1  caller 0x004481b0 (playSound);import __ftol
2026-10-06  00484ff0  Snd::flags  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  00484f40  Snd::setPitch  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00484f00  Snd::stop484f00  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00484e70  Snd::start  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  00484e30  Snd::applyMode484e30  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00484d60  Snd::reopen  C0->C1  match re/match/golf_hand_06_n.cpp
2026-10-06  00484c80  Snd::open  C0->C1  match re/match/golf_hand_06_n.cpp
2026-10-06  00484c20  SndStream::open484c20  C0->C1  match re/match/golf_hand_r0.cpp
2026-10-06  00484b80  Snd::setName  C0->C1  match re/match/golf_small10.cpp
2026-10-06  00484b30  Snd::busy  C0->C1  match re/match/golf_hand_01.cpp;import timeGetTime
2026-10-06  00484940  Snd::play  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  004848a0  SndMusic::dtor  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  00484880  SndMusic::scalarDtor  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00484820  Snd::ctorDerived  C0->C1  match re/match/golf_hand_r3.cpp
2026-10-06  004847f0  Snd::setPan  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004847b0  Snd::applyField4847b0  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00484750  Snd::applyField484750  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004846d0  Snd::setField38  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004846b0  Snd::setField34  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004845e0  Snd::status  C0->C1  match re/match/golf_hand_00.cpp
2026-10-06  004845d0  Snd::queryDevice4845d0  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00484550  Snd::stop484550  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  004844e0  Snd::unlinkFromList  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  004843e0  unlinkSound  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004842f0  SndMusic::setFile  C0->C1  match re/match/golf_hand_07_4842f0.cpp
2026-10-06  00484260  Snd::setMode  C0->C1  match re/match/golf_small10.cpp
2026-10-06  004841e0  Snd::dtor  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  004841c0  Snd::scalarDtor4841c0  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00484150  Snd::ctorBase  C0->C1  match re/match/golf_hand_r3.cpp
2026-10-06  00484130  clearSoundPtrs  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00484110  releaseSoundObj  C0->C1  match re/match/golf_hand_00.cpp
2026-10-06  004840c0  callSoundHook94  C0->C1  match re/match/golf_hand_00.cpp
2026-10-06  00484090  callSoundHook74  C0->C1  match re/match/golf_small18.cpp
2026-10-06  00484060  freeSoundLib  C0->C1  match re/match/golf_raw_11.cpp;import FreeLibrary
2026-10-06  00483fc0  soundVersionCheck  C0->C1  string 0x004bac50;string 0x004e43bc
2026-10-06  00483f10  shutdownSound  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  00483e90  initSound  C0->C1  caller 0x0045baf0
2026-10-06  00483e70  atexit_483e80  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00483e60  staticInit_g83af98  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483e30  atexit_483e40  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00483e20  staticInit_g83ad58  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483df0  atexit_483e00  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00483de0  staticInit_g83ad80  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483d80  leaveModal483d80  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483d60  modalFocusClear483d60  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483d40  modalPush483d40  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483d30  enterModal483d30  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483cf0  pumpInput483cf0  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00483cd0  frameTick483cd0  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483c90  pumpInput483c90  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00483c70  redrawAll  C0->C1  match re/match/golf_small16.cpp
2026-10-06  00483c30  pumpInput483c30  C0->C1  match re/match/golf_small6.cpp
2026-10-06  00483c10  hideScreen  C0->C1  match re/match/golf_small17.cpp
2026-10-06  00483bd0  pumpInput483bd0  C0->C1  match re/match/golf_small5.cpp
2026-10-06  00483bb0  showScreen  C0->C1  match re/match/golf_small17.cpp
2026-10-06  00483ba0  screenFlip  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483b90  Surface::ctorVtbl483b90  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483b10  clearPrimarySurface  C0->C1  match re/match/golf_raw_11.cpp
2026-10-06  00483ac0  setPrimarySurface  C0->C1  match re/match/golf_raw_13.cpp;string 0x004e43ac
2026-10-06  004838f0  Stream4838f0::puts  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  004838b0  Surface::releaseBacking  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483850  Surface::createSized  C0->C1  match re/match/golf_raw_13.cpp
2026-10-06  00483800  Surface::createFromId  C0->C1  match re/match/golf_raw_13.cpp
2026-10-06  004837f0  C4837f0::ctor  C0->C1  match re/match/golf_hand_00.cpp
2026-10-06  004837c0  atexit_4837d0  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  004837b0  screenMgrCtor4837b0  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004835f0  Cache482f60::dtor  C0->C1  match re/match/golf_hand_r1.cpp
2026-10-06  00483420  Palette::nearest  C0->C1  match re/match/golf_hand_s1.cpp
2026-10-06  004833f0  Table483::find  C0->C1  match re/match/golf_small18.cpp
2026-10-06  00483340  clearScreenObjects  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  00483320  resetScreenState  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004832e0  startColorCycle  C0->C1  callee 0x004833f0 (Table483::find);callee 0x00486dc0 (Timer::start)
2026-10-06  00483190  allocColorRange  C0->C1  caller 0x0045baf0;callee 0x004833f0 (Table483::find)
2026-10-06  004830e0  Palette::setEntries  C0->C1  match re/match/golf_hand_05.cpp;import tagBITMAPINFO
2026-10-06  00483070  Cache483070::reset  C0->C1  match re/match/golf_classes.cpp
2026-10-06  00483060  Cache483060::detach  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483030  Cache483030::attach  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00483010  Cache483010::ctorVtbl  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00482fd0  Cache482fd0::ctor  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00482f60  Cache482f60::ctor  C0->C1  match re/match/golf_hand_r1.cpp
2026-10-06  00482f10  atexit_482f20  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00482f00  staticInit_g83ac88  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00482ed0  atexit_482ee0  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00482ec0  staticInit_g83ac30  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00482e90  atexit_482ea0  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00482e80  staticInit_g83acb0  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00482e40  Link482b::send  C0->C1  match re/match/golf_small18.cpp
2026-10-06  00482e20  Link482::resolve  C0->C1  match re/match/golf_small16.cpp
2026-10-06  00482e10  Link482::data  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00482dd0  Stream482dd0::dtor  C0->C1  match re/match/golf_classes.cpp
2026-10-06  00482b20  Decoder482b20::ctor  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00482ae0  Decoder482ae0::ctor  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  00482a80  Surface::copyRaw  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  00482990  Surface::blitRLE  C0->C1  match re/match/golf_hand_07_482990.cpp
2026-10-06  00482940  Surface::clear  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  004826f0  Palette::decodeChunk  C0->C1  match re/match/golf_hand_07_4826f0.cpp
2026-10-06  00482490  Image::seek  C0->C1  match re/match/golf_hand_r1.cpp
2026-10-06  004823c0  setNewHeap  C0->C1  match re/match/golf_hand_r1.cpp;string 0x004c1434;string 0x004c1458
2026-10-06  00481f40  Flic::open  C0->C1  callee 0x00492dd0 (MappedFile::open)
2026-10-06  00481e40  Flic::indexFrames  C0->C1  match re/match/golf_hand_r3.cpp
2026-10-06  00481ba0  Flic::reset  C0->C1  match re/match/golf_raw_11.cpp
2026-10-06  00481b50  Flic::ctor481b50  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00481b20  freeResampleTable  C0->C1  match re/match/golf_raw_11.cpp;import free
2026-10-06  00481870  allocResampleTable  C0->C1  caller 0x0045baf0;import malloc
2026-10-06  00481760  Window::calcSizeFromCorners  C0->C1  match re/match/golf_hand_r1.cpp
2026-10-06  00480d70  Window::doLayout  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  00480ce0  Window::invalidate  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  00480c80  Window::refreshFull  C0->C1  match re/match/golf_raw_13.cpp
2026-10-06  00480a10  Window::paintBackground  C0->C1  caller 0x00496fc0 (draw@R0C496fc0)
2026-10-06  004808c0  Window::paint  C0->C1  match re/match/golf_hand_r2.cpp
2026-10-06  00480870  Window::createFromRect  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004806c0  Window::create  C0->C1  match re/match/golf_hand_r2.cpp
2026-10-06  00480610  Window::ctorInit  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  004805a0  Base4805a0::dtor  C0->C1  match re/match/golf_hand_r2.cpp
2026-10-06  00480580  View4804a0::scalarDtor  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00480410  BinkFrame::scalarDtor  C0->C1  match re/match/golf_hand_04.cpp
2026-10-06  00480390  List480::dtor  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  00480360  Tooltip::hide  C0->C1  match re/match/golf_raw_09.cpp
2026-10-06  00480220  Tooltip::show  C0->C1  match re/match/golf_hand_r0.cpp;callee 0x004a6170 (strchr)
2026-10-06  004801f0  Window::visible  C0->C1  match re/match/golf_small18.cpp
2026-10-06  0047f700  hitTestScreen  C0->C1  match re/match/golf_hand_s1.cpp
2026-10-06  0047f1b0  Window::setFocus  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  0047eee0  matchInputCode  C0->C1  string 0x004e42ec
2026-10-06  0047ed30  Window::hitTestBorder  C0->C1  match re/match/golf_hand_09_f.cpp
2026-10-06  0047e700  ChildArray::bringToFront  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  0047e680  ChildArray::remove  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  0047e5f0  ChildArray::insert  C0->C1  match re/match/golf_raw_13.cpp
2026-10-06  0047e580  zRaise  C0->C1  match re/match/golf_small9.cpp
2026-10-06  0047e520  listRemove  C0->C1  match re/match/golf_hand_02.cpp
2026-10-06  0047e4c0  zPush  C0->C1  match re/match/golf_small9.cpp
2026-10-06  0047e330  collectDrawList  C0->C1  match re/match/golf_hand_r0.cpp
2026-10-06  0047e2d0  clipAllWindows  C0->C1  match re/match/golf_raw_11.cpp
2026-10-06  0047e140  relayoutClip  C0->C1  match re/match/golf_hand_r1.cpp
2026-10-06  0047e120  Panel::notify  C0->C1  match re/match/golf_small17.cpp
2026-10-06  0047d850  drawPanelContents  C0->C1  caller 0x004810d0 (drawPanel4810d0)
2026-10-06  0047d840  setDragTarget  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  0047d810  View::scalarDtor47d810  C0->C1  match re/match/golf_raw_08.cpp
2026-10-06  0047d7d0  Window::isFocusChild  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  0047d7c0  View::ctorVtbl47d7c0  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  0047d510  Window::releaseScrollbars  C0->C1  match re/match/golf_raw_11.cpp
2026-10-06  0047d130  drawCursor  C0->C1  caller 0x0047ab00 (setCursor)
2026-10-06  0047d060  addDirtyRect  C0->C1  match re/match/golf_hand_06_b.cpp
2026-10-06  0047d020  Widget::setValue  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  0047d010  screenTick47d010  C0->C1  match re/match/golf_raw_11.cpp
2026-10-06  0047cfd0  resetViewState47cfd0  C0->C1  caller 0x004855b0 (init4855b0)
2026-10-06  0047cdb0  drawTooltip  C0->C1  caller 0x00480220 (Tooltip::show);callee 0x004a6170 (strchr)
2026-10-06  0047cce0  Window::shrinkRect  C0->C1  caller 0x0047b4e0 (Window::setSize)
2026-10-06  0047cc10  Window::growRect  C0->C1  match re/match/golf_hand_06_b.cpp
2026-10-06  0047cb10  Window::innerSize  C0->C1  match re/match/golf_hand_r1.cpp
2026-10-06  0047ca10  Window::adjustSizeOuter  C0->C1  match re/match/golf_hand_r2.cpp
2026-10-06  0047c970  Window::dispatchCommand  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  0047c6c0  Window::dispatchAccel  C0->C1  import MapVirtualKeyA;callee 0x0047c5d0 (Window::key)
2026-10-06  0047c5d0  Window::key  C0->C1  match re/match/golf_hand_07_47c5d0.cpp
2026-10-06  0047c500  Window::mouseDispatch258  C0->C1  match re/match/golf_hand_06_win.cpp
2026-10-06  0047c430  Window::mouseDispatch254  C0->C1  match re/match/golf_hand_06_win.cpp
2026-10-06  0047c360  Window::mouseDispatch250  C0->C1  match re/match/golf_hand_06_win.cpp
2026-10-06  0047c290  Window::mouseDispatch24c  C0->C1  match re/match/golf_hand_06_win.cpp
2026-10-06  0047c1b0  Window::click  C0->C1  match re/match/golf_hand_07_47c1b0.cpp
2026-10-06  0047c0e0  Window::mouseDispatch244  C0->C1  match re/match/golf_hand_06_win.cpp
2026-10-06  0047c010  Window::mouseDispatch240  C0->C1  match re/match/golf_hand_06_win.cpp
2026-10-06  0047bf40  Window::mouseDispatch23c  C0->C1  match re/match/golf_hand_06_win.cpp
2026-10-06  0047bc60  Window::resized  C0->C1  match re/match/golf_hand_s2.cpp;callee 0x0047d570 (Window::layoutScrollbars)
2026-10-06  0047ba90  Window::setField2705a0  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  0047ba70  Window::setField26c5a0  C0->C1  match re/match/golf_raw_03.cpp
2026-10-06  0047ba50  Window::fwdRange270  C0->C1  match re/match/golf_raw_02.cpp
2026-10-06  0047ba30  Window::fwdRange26c  C0->C1  match re/match/golf_raw_02.cpp
2026-10-06  0047ba10  Window::fwdScroll270  C0->C1  match re/match/golf_raw_02.cpp
2026-10-06  0047b9f0  Window::fwdScroll26c  C0->C1  match re/match/golf_raw_02.cpp
2026-10-06  0047b8f0  Label::setText  C0->C1  match re/match/golf_hand_05.cpp
2026-10-06  0047b820  Window::detach  C0->C1  match re/match/golf_raw_11.cpp;caller 0x0047e520 (listRemove)
2026-10-06  0047b670  Window::show  C0->C1  match re/match/golf_hand_r3.cpp
2026-10-06  0047b4e0  Window::setSize  C0->C1  match re/match/golf_hand_r2.cpp
2026-10-06  0047b420  Window::moveTo  C0->C1  match re/match/golf_small19.cpp
2026-10-06  0047b310  Window::center  C0->C1  match re/match/golf_hand_08_b.cpp
2026-10-06  0047b2d0  View::toGlobal  C0->C1  match re/match/golf_small5.cpp
2026-10-06  0047b290  View::toLocal  C0->C1  match re/match/golf_small5.cpp
2026-10-06  0047b200  View::fromParent  C0->C1  match re/match/golf_small13.cpp
2026-10-06  0047b170  View::toParent  C0->C1  match re/match/golf_small13.cpp
2026-10-06  0047b120  View::offsetRectToLocal  C0->C1  match re/match/golf_small8.cpp
2026-10-06  0047b0d0  View::offsetRectToParent  C0->C1  match re/match/golf_small8.cpp
2026-10-06  0047b080  Node::contains  C0->C1  match re/match/golf_small8.cpp
2026-10-06  0047af60  Container::cycleSelection  C0->C1  match re/match/golf_hand_08_t_sw2.cpp
2026-10-06  00448160  arrayStaticInit_448160  C0->C1  match re/match/golf_arrays.cpp; import ??_L (vector ctor iterator)
2026-10-06  0044ad20  arrayStaticInit_44ad20  C0->C1  match re/match/golf_arrays.cpp; import ??_L (vector ctor iterator)
2026-10-06  0043d460  arrayStaticInit_43d460  C0->C1  match re/match/golf_arrays.cpp; import ??_L (vector ctor iterator)
2026-10-06  0043cc10  arrayStaticInit_43cc10  C0->C1  match re/match/golf_arrays.cpp; import ??_L (vector ctor iterator)
2026-10-06  0045bac0  registerExit_45bac0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0045ba70  registerExit_45ba70  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044b770  registerExit_44b770  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044b670  registerExit_44b670  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044b4d0  registerExit_44b4d0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044b3c0  registerExit_44b3c0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044b190  registerExit_44b190  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044b150  registerExit_44b150  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044b110  registerExit_44b110  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044af90  registerExit_44af90  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044af50  registerExit_44af50  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044af10  registerExit_44af10  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044aed0  registerExit_44aed0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044ade0  registerExit_44ade0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044ad90  registerExit_44ad90  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044ad40  registerExit_44ad40  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0044aca0  registerExit_44aca0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  00449240  registerExit_449240  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  004491d0  registerExit_4491d0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  00448180  registerExit_448180  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0043d650  registerExit_43d650  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0043d4f0  registerExit_43d4f0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0043d490  registerExit_43d490  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0043ccc0  registerExit_43ccc0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0043cc80  registerExit_43cc80  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0043cc30  registerExit_43cc30  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  004385b0  registerExit_4385b0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  004378f0  registerExit_4378f0  C0->C1  match re/match/golf_raw_0*.cpp; import _atexit
2026-10-06  0045bab0  staticCtor_45bab0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0045ba60  staticCtor_45ba60  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044b760  staticCtor_44b760  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044b660  staticCtor_44b660  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044b4c0  staticCtor_44b4c0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044b3b0  staticCtor_44b3b0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044b180  staticCtor_44b180  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044b140  staticCtor_44b140  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044b100  staticCtor_44b100  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044af80  staticCtor_44af80  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044af40  staticCtor_44af40  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044af00  staticCtor_44af00  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044aec0  staticCtor_44aec0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044add0  staticCtor_44add0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0044ad80  staticCtor_44ad80  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  004491c0  staticCtor_4491c0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0043d640  staticCtor_43d640  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0043d4e0  staticCtor_43d4e0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0043ccb0  staticCtor_43ccb0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0043cc70  staticCtor_43cc70  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  004385a0  staticCtor_4385a0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  004378e0  staticCtor_4378e0  C0->C1  match re/match/golf_raw_0*.cpp; callee is an object constructor
2026-10-06  0045c460  substituteStoryNames  C0->C1  match re/match/golf_hand_r0.cpp; string 0x4d3954 (PARTNER); string 0x4d395c (MYNAME); callee 0x45b7c0 (replaceInText)
2026-10-06  0045c420  thoughtBalance  C0->C1  match re/match/golf_small5.cpp; reads g_579540
2026-10-06  0045c200  drawThoughtBubble  C0->C1  callee 0x4675d0 (thoughtFlag); callee 0x473cb0 (drawScaled); callers 0x40d320/0x45c560
2026-10-06  0045c1e0  Random::range  C0->C1  match re/match/golf_util.cpp; callee 0x45c1a0 (Random::next); many callers
2026-10-06  0045c1a0  Random::next  C0->C1  match re/match/golf_util.cpp; reads/writes g_4ba800
2026-10-06  0045c150  pumpLight  C0->C1  match re/match/golf_small3.cpp; callee 0x45c030 (pumpMessages)
2026-10-06  0045c0c0  pumpAndRedraw  C0->C1  callee 0x45c030 (pumpMessages); callee setDirtyFlagA/B; many callers
2026-10-06  0045c030  pumpMessages  C0->C1  match re/match/golf_small12.cpp; callee 0x483cd0
2026-10-06  0045bf80  waitTicks  C0->C1  match re/match/golf_small12.cpp; callee 0x45c030 (pumpMessages)
2026-10-06  0045baf0  gameMain  C0->C1  string 0x4d3910 (Sid Meier's SimGolf); string 0x4d3904 (jgld.dll); caller 0x4a682f (entry)
2026-10-06  0045b9f0  appendString  C0->C1  match re/match/golf_small9.cpp; writes g_56fcb0
2026-10-06  0045b8b0  appendToBuffer45b8b0  C0->C1  match re/match/golf_hand_r2.cpp; writes g_59d81c/g_59d91c
2026-10-06  0045b880  clearBuffers  C0->C1  match re/match/golf_small2.cpp; writes g_56fcb0/g_59d81c; callers 0x4315e0/0x442180
2026-10-06  0045b7c0  replaceInText  C0->C1  match re/match/golf_small25.cpp; import _strstr; callers 0x45c460/0x469b00
2026-10-06  0045b660  lookupThemeText  C0->C1  import _fgets; import __strcmpi; caller 0x433190 (showInfoCursorPanel)
2026-10-06  0045b2c0  textInputBox  C0->C1  callee 0x478af0 (fillRect); callee 0x477250 (put@Text477); import _isprint
2026-10-06  0045b0d0  drawValueWidget  C0->C1  callee 0x477580 (value477580@Widget); callee 0x477250 (put@Text477)
2026-10-06  0045af30  renderTextWidget45af30  C0->C1  callee 0x477250 (put@Text477); caller 0x40d320
2026-10-06  0045af00  drainAndRedraw  C0->C1  match re/match/golf_raw_02.cpp; callee 0x483c30 (drain); callee 0x483c70 (redrawAll)
2026-10-06  0045aed0  waitRedrawReady  C0->C1  match re/match/golf_raw_02.cpp; callee 0x45af00; callee 0x483cf0 (drain)
2026-10-06  0045ae70  flushRedraw  C0->C1  callee 0x483c30 (drain); callee 0x483c70 (redrawAll); many callers
2026-10-06  0045ae50  setDirtyFlagB  C0->C1  callers 0x40f5c0/0x43a8c0/0x45c0c0; writes g_822b9c
2026-10-06  0045ae30  setDirtyFlagA  C0->C1  callers 0x40f5c0/0x45b2c0/0x45c0c0; writes g_822b98
2026-10-06  004315e0  buildGolferBioText  C0->C1  string 0x4c77f4 ( favorite color is ); string 0x4c7820 ( likes ); callee 0x45b880 (clearBuffers)
2026-10-06  0045a090  showTournamentResults  C0->C1  string 0x4d3784 (TOURNAMENT RESULTS); string 0x4d37c4 (LEADER BOARD of)
2026-10-06  00459850  showGolferPairSelect  C0->C1  string 0x4d374c (SELECT THE NEXT PAIR OF GOLFERS); string 0x4d3740 ( years old)
2026-10-06  00459400  showHireEmployeeDialog  C0->C1  string 0x4d372c (HIRE AN EMPLOYEE); string 0x4d36e8 (Thirst quenchers...); caller 0x40aa80
2026-10-06  004587a0  showBuyLandScreen  C0->C1  string 0x4d36cc (TRACTS FOR SALE); string 0x4d3698 (Buy tract #); callee 0x406c30 (canAfford)
2026-10-06  00456be0  showRoutingMapScreen  C0->C1  string 0x4d3644 (ROUTING MAP); string 0x4d3620 (COURSE AURA); string 0x4d3634 (HOME SITE VALUE)
2026-10-06  00456bb0  overlayCharAt  C0->C1  match re/match/golf_raw_11.cpp; caller 0x456be0 (showRoutingMapScreen)
2026-10-06  00456b70  mapToScreen456b70  C0->C1  match re/match/golf_hand_01.cpp; callers 0x456be0/0x4587a0
2026-10-06  00455ed0  showHistographScreen  C0->C1  string 0x4d3354 (HISTOGRAPH); string 0x4d3330 (Happy Ending.)
2026-10-06  00455a30  showBestScoresScreen  C0->C1  string 0x4d3318 ( Hole Scores); string 0x4d3328 (Best ); caller 0x432720 (showSystemMenu)
2026-10-06  00454c50  showMembershipRoster  C0->C1  string 0x4d32f0 (Membership Roster); string 0x4c8658 (Gold Member)
2026-10-06  004546b0  showPlayerCommentsReport  C0->C1  string 0x4d3288 (PLAYER COMMENTS REPORT); string 0x4d327c (Frequency)
2026-10-06  00453330  showHoleStatsScreen  C0->C1  string 0x4d326c (HOLE STATS for ); string 0x4d31f0 (Stroke average)
2026-10-06  004532a0  appendRatingLabel  C0->C1  match re/match/golf_hand_03.cpp; string 0x4d3144 ( (outstanding)); string 0x4d3174 ( (poor))
2026-10-06  00453260  golferScore  C0->C1  match re/match/golf_small6.cpp; callers 0x459850/0x45de80
2026-10-06  0044fb30  showCourseReport  C0->C1  string 0x4d2e4c (COURSE REPORT); string 0x4d3104 (REPORT of the SIM GOLF ASSOCIATION)
2026-10-06  0044faf0  bucketValue  C0->C1  match re/match/golf_small4.cpp; many callers
2026-10-06  0044f6b0  showFinancialReport  C0->C1  string 0x4d2e10 (FINANCIAL REPORT)
2026-10-06  0044e770  showShortcutsScreen  C0->C1  string 0x4d2dac (KEYBOARD SHORTCUTS); string 0x4d2dc0 (shortcuts.pcx)
2026-10-06  0044bde0  loadInfoScreenArt  C0->C1  string 0x4d21e0 (tournament result_alpha.pcx); string 0x4d2534 (memberRoster.pcx)
2026-10-06  0044b9c0  showCreditsScreen  C0->C1  match re/match/golf_hand_s3.cpp; string 0x4d21a4 ($bink); string 0x4d21ac (CREDITS.txt)
2026-10-06  0044cce0  resetAllPanels  C0->C1  match re/match/golf_hand_s2.cpp; destroys g_821020/g_821040/g_821070...; caller 0x45baf0
2026-10-06  0044ac60  initPanelState  C0->C1  match re/match/golf_small4.cpp; callee 0x4837f0 (ctor); writes g_821020/g_821ec8
2026-10-06  0044a6e0  renderTerrainStrip  C0->C1  callee stripRender (Terrain import); caller 0x4498a0
2026-10-06  0044a5b0  rebuildTileData  C0->C1  match re/match/golf_hand_r1.cpp; callee 0x449f00/0x449fa0/0x44a380/0x44a410
2026-10-06  0044a410  togglePath  C0->C1  match re/match/golf_hand_r1.cpp; callee layPath/updatePath/hasPath (imports)
2026-10-06  0044a380  applyTileWalls  C0->C1  callee setWall (import); callee 0x449330 (wallHeight); caller 0x44a5b0
2026-10-06  00449fa0  applyTileElevation  C0->C1  callee elevateCorner/lowerCorner/lowerEdgeCorner (imports); caller 0x44a5b0
2026-10-06  00449f00  applyTileVariation  C0->C1  callee getVariation/setType (imports); callee 0x4492d0 (tileType); caller 0x44a5b0
2026-10-06  004498a0  updateTerrainRender  C0->C1  callee render/localRender/setZoomLevel/calcAllNormals (imports); callee 0x449790
2026-10-06  00449860  freeTerrain  C0->C1  match re/match/golf_hand_01.cpp; callee ~Terrain/closeSystem (imports); caller 0x45baf0
2026-10-06  00449790  initTerrain  C0->C1  match re/match/golf_hand_06_f.cpp; callee initTerrain/initSystem (imports); callee 0x449540
2026-10-06  00449540  rebuildTerrainGrid  C0->C1  match re/match/golf_hand_s0.cpp; callee setType/layPath/setWall/calcNormals/elevateCorner (imports)
2026-10-06  00449520  refreshTerrain  C0->C1  match re/match/golf_small16.cpp; callee resetTerrain (import); callee 0x449540
2026-10-06  00449470  syncTiles  C0->C1  match re/match/golf_hand_05.cpp; callee setType/getVariation (Terrain imports)
2026-10-06  00449400  applyCourseType  C0->C1  callee loadNewCourseType (Terrain import); callee passCollarInfo; reads g_578372
2026-10-06  004493d0  drawMapLine  C0->C1  callee drawLine (jgld import); many callers
2026-10-06  004493b0  tileFlag20  C0->C1  match re/match/golf_small2.cpp; callers 0x449540/0x44a410
2026-10-06  00449330  wallHeight  C0->C1  match re/match/golf_small12.cpp; callers 0x449540/0x44a380
2026-10-06  00449310  tileQuery449310  C0->C1  match re/match/golf_small.cpp; callee 0x40bfe0; callers 0x449540/0x449fa0
2026-10-06  004492f0  tileByte  C0->C1  match re/match/golf_small.cpp; callers 0x449470/0x449540
2026-10-06  004492d0  tileType  C0->C1  match re/match/golf_small.cpp; callers 0x449470/0x449540
2026-10-06  00449210  allocTileBuffer  C0->C1  callee operator_new; writes g_820f18/g_820f1c/g_820f20 read by 0x44a5b0 (rebuildTileData)
2026-10-06  00449150  Terrain::getType  C0->C1  match re/match/terrain.cpp
2026-10-06  00449130  Terrain::getWall  C0->C1  match re/match/terrain.cpp
2026-10-06  00449110  Terrain::getElevation  C0->C1  match re/match/terrain.cpp
2026-10-06  004490b0  shutdownSoundSystem  C0->C1  match re/match/golf_raw_12.cpp; callee 0x483f10 (shutdown); callee 0x484f00 (stop); caller 0x45baf0
2026-10-06  00448220  loadSoundTable  C0->C1  match re/match/golf_raw_06.cpp; string 0x4d0604 (sounds\interface\wrong.wav); caller 0x45baf0 (gameMain)
2026-10-06  00448200  startSoundSlot  C0->C1  match re/match/golf_small17.cpp; callee 0x484750
2026-10-06  004481b0  playSound  C0->C1  match re/match/golf_small8.cpp; many callers
2026-10-06  00442180  loadInterfaceArt  C0->C1  string 0x4c5ee4 (bldg.pcx); string 0x4cd018 (interface\EmployeePanel); callee 0x43dbe0 (loadWorldAssets)
2026-10-06  0043dbe0  loadWorldAssets  C0->C1  string 0x4c8a6c (trees\Links\ScotsPine_Lg); string 0x4c273c (Snack Bar); callee 0x43d740 (loadFlic)
2026-10-06  0043d740  loadFlic  C0->C1  string 0x4c8a3c (Too many flics!); string 0x4c8a50 (.flc); callee 0x43d5d0 (allocBlock)
2026-10-06  0043d6f0  cellAt  C0->C1  match re/match/golf_small7.cpp; many callers
2026-10-06  0043d670  freeTable  C0->C1  match re/match/golf_small11.cpp; callee 0x43d520 (freeBlock)
2026-10-06  0043d5d0  allocBlock  C0->C1  match re/match/golf_small9.cpp; caller 0x43d740 (loadFlic)
2026-10-06  0043d520  freeBlock  C0->C1  match re/match/golf_freeblock.cpp; caller 0x43d670 (freeTable)
2026-10-06  0043d2a0  listMatchingFiles  C0->C1  match re/match/golf_hand_r2.cpp; import _strstr; string 0x4c8a2c (shadow)
2026-10-06  004378a0  fileExists  C0->C1  match re/match/golf_small3.cpp
2026-10-06  0043cce0  playAudioFile  C0->C1  match re/match/golf_hand_04.cpp; callee 0x487090 (play@C487090); callee 0x4378a0 (fileExists)
2026-10-06  0043cd70  showTitleScreen  C0->C1  string 0x4c89ac (interface\TitleMO); string 0x4c8a0c (SMSG_introfinal.bik); caller 0x45baf0 (gameMain)
2026-10-06  0043b610  loadGameBrowserScreen  C0->C1  string 0x4c8930 (Load Previous Game); string 0x4c8960 (Select Championship Course); string 0x4c8874 (Saved Games\)
2026-10-06  0043a8c0  pickAProScreen  C0->C1  string 0x4c8830 (Pick A Pro); string 0x4c8814 (Themes\Championship\*.pro); string 0x4c883c (Title_PickAPro)
2026-10-06  0043a400  selectDifficultyScreen  C0->C1  string 0x4c8748 (Select Difficulty); string 0x4c871c (Impossible); string 0x4c875c (interface\TitleSelDiff)
2026-10-06  004385d0  showCharacterEditor  C0->C1  string 0x4c85f4 (Save character); string 0x4c8598 (Gender); string 0x4c86e4 (interface\CustGlfBckMale)
2026-10-06  00438390  pickCharEditor  C0->C1  match re/match/golf_hand_s0.cpp; callee 0x4382f0 (charEditorZone)
2026-10-06  004382f0  charEditorZone  C0->C1  match re/match/golf_hand_04.cpp; callee 0x467170 (approxDistance)
2026-10-06  00438260  nearestCharControl  C0->C1  match re/match/golf_hand_04.cpp; callee 0x467170 (approxDistance)
2026-10-06  00437910  promptAndSaveFile  C0->C1  string 0x4c3f0c (already exists...); string 0x4c84a4 (Invalid file path); import _fwrite
2026-10-06  00437fa0  loadThemeFile  C0->C1  string 0x4c84f4 (Themes\Standard\); callee 0x473bf0 (surface); import _fread
2026-10-06  00436e50  drawEmployeeDetailPanel  C0->C1  string 0x4c8390 (Fire this employee); string 0x4c83f0 (Satisfied customers:)
2026-10-06  00436c00  handleEmployeeAction  C0->C1  string 0x4c837c (Rename Employee...); callee 0x45b2c0 (textInputBox)
2026-10-06  00436b00  hitEmployee436b00  C0->C1  match re/match/golf_raw_13.cpp; callee 0x467170 (approxDistance); callers 0x436c00/0x436e50
2026-10-06  004362f0  drawGolferActionPanel  C0->C1  string 0x4c8344 (Begin Tournament); string 0x4c82e8 (Low punch shot); string 0x4c836c (Practice Round)
2026-10-06  00436060  clickGolferPanel  C0->C1  match re/match/golf_hand_s1.cpp; callee 0x435f00 (hitGolferPanel); callee 0x4385d0 (showCharacterEditor)
2026-10-06  00435f00  hitGolferPanel435f00  C0->C1  match re/match/golf_hand_r2.cpp; callee 0x467170 (approxDistance)
2026-10-06  00435760  drawEmployeeViewPanel  C0->C1  string 0x4c5dc8 (Golfers); string 0x4c82c8 (Hire Employees)
2026-10-06  00435680  clickEmployeeView  C0->C1  match re/match/golf_hand_06_h.cpp; callee 0x435570 (hitEmployeeSlot); callee 0x436c00
2026-10-06  00435570  hitEmployeeSlot435570  C0->C1  match re/match/golf_hand_08_c.cpp; callee 0x467170 (approxDistance)
2026-10-06  00434cf0  drawAmenitiesPanel  C0->C1  string 0x4c8238 (Landmarks); string 0x4c820c (Scenic Bridge); string 0x4c825c (Pathway)
2026-10-06  00434ac0  clickAmenityPanel  C0->C1  match re/match/golf_hand_s2.cpp; string 0x4c7e7c; callee 0x434980 (pickAmenity)
2026-10-06  00434980  pickAmenity434980  C0->C1  match re/match/golf_hand_09_g.cpp; callee 0x467170 (approxDistance)
2026-10-06  00434350  drawBuildingPanel  C0->C1  string 0x4c69a8 (Upgraded ); string 0x4c8100 (Elevation); string 0x4c816c (Golfers play faster)
2026-10-06  00434140  pickBuilding  C0->C1  match re/match/golf_hand_s1.cpp; string 0x4c8084 (upgraded buildings...); callee 0x4340a0
2026-10-06  004340a0  hitBuildingTool  C0->C1  callee 0x467170 (approxDistance); callers 0x434140/0x434350
2026-10-06  00433e50  drawTerrainToolbar  C0->C1  match re/match/golf_hand_s1.cpp; string 0x4c7fe0 (Analyze Golf Shot); string 0x4c7ff4
2026-10-06  00433d30  clickBuildTool433d30  C0->C1  match re/match/golf_hand_r2.cpp; string 0x4c7e7c; callee 0x433c60 (hitBuildTool)
2026-10-06  00433c60  hitBuildTool433c60  C0->C1  match re/match/golf_hand_06_h.cpp; callee 0x467170 (approxDistance)
2026-10-06  00433190  showInfoCursorPanel  C0->C1  string 0x4c60e0 (Lie: ); string 0x4c7f1c (Improvements); string 0x4c7f80 (interface\tropical.txt)
2026-10-06  00433040  clickSlotPanel  C0->C1  match re/match/golf_small34.cpp; string 0x4c7e7c; callee 0x434ac0 (S2_click)
2026-10-06  00432f90  hitTestSlot432f90  C0->C1  callee 0x467170 (approxDistance); callers 0x433040/0x433190
2026-10-06  00432ba0  showMainRadialMenu  C0->C1  string 0x4c7de0 (Rotate Map); string 0x4c7e6c (Build Course); caller 0x40f5c0 (main loop)
2026-10-06  00432620  drawPanelBackdrop  C0->C1  callee 0x404b70 (text); callee 0x4493d0 (drawMapLine); callers are the build/employee panels
2026-10-06  004326a0  nearestMenuSpot  C0->C1  match re/match/golf_small10.cpp; callee 0x467170 (approxDistance)
2026-10-06  00432560  showPreferencesMenu  C0->C1  match re/match/golf_hand_05.cpp; string 0x4c7c34; caller 0x432720 (showSystemMenu)
2026-10-06  004321d0  capsLockOff  C0->C1  match re/match/golf_small4.cpp
2026-10-06  00432170  flushCourseInfo  C0->C1  match re/match/golf_small3.cpp; callee 0x431ee0; callee 0x431fa0
2026-10-06  00431fa0  writeCourseInfo  C0->C1  match re/match/golf_hand_s1.cpp; string 0x4c7918 ([Course]); import _fprintf
2026-10-06  00431ee0  saveThumbAndFull  C0->C1  match re/match/golf_hand_06_i.cpp; string 0x4c782c; string 0x4c7838; callee 0x431d20
2026-10-06  00431d20  saveCourseJpg  C0->C1  match re/match/golf_hand_r1.cpp; import _CreateJPG
2026-10-06  0042fb90  worldToScreen  C0->C1  callee 0x42f4b0 (cornerRange); reads g_4c2ba0/g_4c2ba4 (camera); caller 0x42f940 (tileToScreen)
2026-10-06  0042fa30  heightAt42fa30  C0->C1  match re/match/golf_hand_r3.cpp; callee 0x42f4b0 (cornerRange)
2026-10-06  0042f940  tileToScreen  C0->C1  match re/match/golf_small27.cpp; callee 0x42fb90 (worldToScreen)
2026-10-06  0042f7a0  rebuildHeightfield  C0->C1  match re/match/golf_hand_r0.cpp; callee 0x42f530/0x42f630/0x42f6e0
2026-10-06  0042f6e0  raiseFromNeighbours  C0->C1  match re/match/golf_small28.cpp; callee 0x40bf60 (tileBlocked)
2026-10-06  0042f630  relaxTile42f630  C0->C1  match re/match/golf_hand_05.cpp; callee 0x40bf60 (tileBlocked)
2026-10-06  0042f530  relaxEdges42f530  C0->C1  match re/match/golf_hand_r0.cpp; callee 0x40bf60 (tileBlocked); callee 0x42f4b0 (cornerRange)
2026-10-06  0042f4b0  cornerRange  C0->C1  match re/match/golf_small12.cpp; callee 0x40c170 (r3_f40c170)
2026-10-06  0042f2c0  markTileKind11  C0->C1  match re/match/golf_small11.cpp; callee 0x42f1c0
2026-10-06  0042f1c0  propagateType11  C0->C1  recursive; callee tileBlocked; reads/writes g_tile_byte, reads g_tile_type
2026-10-06  0042f120  floodMark  C0->C1  recursive; writes tile-flag array 0x0053caf0 (bit 0x40), reads type table 0x00578372
2026-10-06  0042ef40  rateLot  C0->C1  match re/match/golf_hand_s1.cpp; callees distance, clearCost
2026-10-06  0042ee80  clearCost  C0->C1  match re/match/golf_hand_05.cpp; callee tileBlocked
2026-10-06  0042e7e0  pathStep  C0->C1  callees directionOf, tileToScreen, drawCenteredText; reads g_tile_type and tile flags 0x0053caf0, g_date/g_flags
2026-10-06  0042e7a0  directionOf  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  0042dea0  holeMagazineEvent  C0->C1  strings 0x004c76ac/0x004c7728 (magazine ratings); callees appendHoleName, startMessage, playSound
2026-10-06  0042dd50  appendCents  C0->C1  match re/match/golf_small33.cpp; callee __itoa; writes 0x0058a528
2026-10-06  0042dc00  appendNumber  C0->C1  match re/match/golf_small32.cpp; callee 0x004ad425 (__itoa); recursive; writes 0x0058a528
2026-10-06  0042dba0  sampleHeight  C0->C1  match re/match/golf_small9.cpp; callees clamp, 0x004674c0
2026-10-06  004289e0  updateGolfers  C0->C1  strings 0x004c750c (leaving in disgust), resign/tired/thirsty/throw-club strings; callees patronEvent, matchUpdate, simulateShot, resetGolfer, rateTile, startMessage
2026-10-06  00427380  matchUpdate  C0->C1  strings 0x004c7228 (new course record), greens-fee and match-score strings; callees pointsPopup, store, appendString
2026-10-06  004266b0  patronEvent  C0->C1  strings 0x004c6d24 (vacation home), 0x004c6f88 (landmark donation), 0x004c7058 (expansion approval); callees landmarkName, startMessage, 0x00469a20 (appendOpinion)
2026-10-06  00426670  resetGolfer  C0->C1  match re/match/golf_small6.cpp; writes 0x005689e8/0x00568d08
2026-10-06  00424120  simulateShot  C0->C1  callees wobble, shotPower, rateTile, heightBlend, sign, thoughtFlag, distance
2026-10-06  00422530  rateTile  C0->C1  match re/match/golf_hand_r1.cpp; callee typeAtPos; reads 0x00543cc8
2026-10-06  00422430  shotPower  C0->C1  match re/match/golf_small27.cpp; callees decaySum, decaySum2
2026-10-06  004223f0  decaySum2  C0->C1  match re/match/golf_small6.cpp
2026-10-06  004223c0  decaySum  C0->C1  match re/match/golf_small4.cpp; reads 0x005783a1
2026-10-06  00421fa0  scanTileLine  C0->C1  reads g_tile_type and per-type attribute table 0x00578372/0x00578376; callees distance, tileBlocked, clamp, fold
2026-10-06  00421bc0  updateMembership  C0->C1  string 0x004c6cac (membership-declining); callees Random, typeBit7Clear; reads 0x00543cc4
2026-10-06  00421b90  drawScaledSprite421b90  C0->C1  callee 0x00474030 (drawScaled)
2026-10-06  00421b60  drawScaledSprite421b60  C0->C1  match re/match/golf_hand_00.cpp; callee 0x00473cb0 (drawScaled)
2026-10-06  0040f5c0  mainLoop  C0->C1  menu strings 0x004c6c88 (Start New Game), 0x004c6c98 (Continue Saved Game), 0x004c6c54 (Sandbox Mode), 0x004c6c64 (Play a Championship); caller 0x0045baf0; 169 callees
2026-10-06  0040f190  customizeCharacter  C0->C1  strings 0x004c593c (Golf Pro), 0x004c5948 (.pro); callees showCharacterIntro, resetGolfer, Random
2026-10-06  0040e720  announceBuilding  C0->C1  strings 0x004c5824 (You may now build a ), 0x004c591c (Hole #), par/dogleg text; callees logTick, startMessage, appendUpgradeText
2026-10-06  0040e5f0  appendUpgradeText  C0->C1  match re/match/golf_hand_r3.cpp; strings 0x004c53a4/0x004c53ec (upgrade benefits)
2026-10-06  0040e5b0  rankValue  C0->C1  match re/match/golf_hand_01.cpp
2026-10-06  0040e000  stampCourseTiles  C0->C1  writes g_tile_type (values 1/4/0x11/0x15/0x16) and g_tile_byte; callee 0x0042f7a0 (r0_rebuild)
2026-10-06  0040df80  objectAt  C0->C1  reads g_placed_objects 0x0058bcb8 and footprint table 0x004c26c0
2026-10-06  0040ddb0  nearestPlaced  C0->C1  match re/match/golf_small25.cpp; callee distance
2026-10-06  0040daa0  appendCourseTitle  C0->C1  match re/match/golf_hand_07_40daa0.cpp; strings 0x004c5364 ( Golf Club)..; callee appendString
2026-10-06  0040d7b0  appendDate  C0->C1  match re/match/golf_small23.cpp; callee __itoa; writes 0x0058a528
2026-10-06  0040d6a0  tickMessage  C0->C1  match re/match/golf_small24.cpp; callee drawWindow
2026-10-06  0040d320  drawWindow  C0->C1  callees drawWindowFrame, drawFrameR3, drawFrameS2, 0x00473cb0 (drawScaled); many UI callers
2026-10-06  0040d0b0  drawFrameS2  C0->C1  match re/match/golf_hand_s2.cpp; callees 0x00473f60, 0x00480b00
2026-10-06  0040cef0  drawFrameR3  C0->C1  match re/match/golf_hand_r3.cpp; callees blitPanel, 0x00473f60
2026-10-06  0040cdd0  drawWindowFrame  C0->C1  match re/match/golf_raw_11.cpp; callees blitPanel, 0x00473f60
2026-10-06  0040cc00  drawPanelS2  C0->C1  match re/match/golf_hand_s2.cpp; callees 0x00473f60, 0x00480b00
2026-10-06  0040cb00  startMessage  C0->C1  match re/match/golf_small30.cpp; callee 0x0045c1e0 (Random::range); widely called
2026-10-06  0040ca10  blitPanel  C0->C1  callee 0x004740f0 (Surf474::blit); reads 0x005a5554
2026-10-06  0040c910  drawMoneyLabels  C0->C1  match re/match/golf_small27.cpp; callees drawCenteredText2, appendNumber
2026-10-06  0040c890  pointsPopup  C0->C1  match re/match/golf_small10.cpp; reads 0x0059abb0
2026-10-06  0040c860  clearMatching  C0->C1  match re/match/golf_small4.cpp
2026-10-06  0040c7a0  drawLabels  C0->C1  match re/match/golf_small26.cpp; callees drawCenteredText2, 0x0042fb90 (world->screen); reads 0x0056c570
2026-10-06  0040c720  queueMessage  C0->C1  match re/match/golf_small11.cpp; writes 0x0056c570/0x0053bba8
2026-10-06  0040c6f0  logTick  C0->C1  match re/match/golf_small4.cpp; writes 0x00834170
2026-10-06  0040c500  playSoundAt  C0->C1  callees 0x0042fb90 (world->screen), 0x004481b0 (playSound), clamp, Random; reads 0x005a9cbc/0x005a9cd8
2026-10-06  0040c4b0  tileDistance  C0->C1  match re/match/golf_util.cpp; callee distance
2026-10-06  0040c450  slopeMix  C0->C1  match re/match/golf_small9.cpp; callees slopeX, slopeY, clamp
2026-10-06  0040c3a0  slopeX  C0->C1  match re/match/golf_small20.cpp; callee cornerHeights
2026-10-06  0040c2f0  slopeY  C0->C1  match re/match/golf_small20.cpp; callee cornerHeights
2026-10-06  0040c170  heightBlend  C0->C1  match re/match/golf_hand_r3.cpp; callees sampleHeight, clamp, tileBlocked
2026-10-06  0040bfe0  cornerHeights  C0->C1  callees heightBlend, 0x0042f4b0 (cornerRange)
2026-10-06  0040bfa0  typeAtPos  C0->C1  match re/match/golf_small5.cpp; callee tileBlocked
2026-10-06  0040bf60  tileBlocked  C0->C1  match re/match/golf_small5.cpp; 29 callers
2026-10-06  0040bf20  lineTo  C0->C1  match re/match/golf_small6.cpp; callee 0x004493d0
2026-10-06  0040bf00  setLinePoint  C0->C1  match re/match/golf_small.cpp; writes 0x005a412c/0x005a4130
2026-10-06  0040bbf0  deserializeCourse  C0->C1  match re/match/golf_raw_11.cpp; string 0x004d6088 (Golf Pro); callee saveFieldIo
2026-10-06  0040b9b0  loadGame  C0->C1  strings 0x004c5304 (Loading Game), 0x004c3f4c (saved games\); callees serializeCourse-area, r0_rebuild, __open/__read/__close
2026-10-06  0040b840  loadCourse  C0->C1  match re/match/golf_hand_r0.cpp; string 0x004c52f4 (Loading Course); callees deserializeCourse, r0_rebuild, __open/__read/__close
2026-10-06  0040b4a0  saveGame  C0->C1  match re/match/golf_hand_s0.cpp; strings 0x004c3f44 (.sve), 0x004c3f4c (saved games\); callees serializeCourse, __open/__write/__close
2026-10-06  0040afa0  serializeCourse  C0->C1  match re/match/golf_hand_t3.cpp; string 0x004c1578 (1st Challenge hole); callee saveFieldIo
2026-10-06  0040af70  saveFieldIo  C0->C1  callees 0x004a583a (__read), 0x004a5b58 (__write); reads 0x0053e638/0x00568d08
2026-10-06  0040ad60  loadTopScores  C0->C1  string 0x004c52ac (top10.sve); callees Random, __open/__read/__close
2026-10-06  0040acd0  distance  C0->C1  match re/match/golf_util.cpp; callee 0x004a6030 (__ftol)
2026-10-06  0040aa80  hireEmployee  C0->C1  string 0x004c5228 (Daily-Fee-course requirement); callees spawnWalker, canAfford, 0x0044faf0 (bucket)
2026-10-06  0040a9a0  appendClubName  C0->C1  match re/match/golf_small15.cpp; strings 0x004c51b0 (Putter)..0x004c5220 (Driver)
2026-10-06  0040a4e0  demolishObject  C0->C1  strings 0x004c516c (Yes/No prompt), 0x004c5188 (Do you really want to demolish); callees placeHabitat-area, markKind11, r0_rebuild, playSound
2026-10-06  0040a160  buildToolLabel  C0->C1  strings 0x004c50d8 (Remove ), 0x004c5144 (Demolish ), 0x004c515c (Out of bounds); callees tileBlocked, objectAt
2026-10-06  0040a130  setGrids  C0->C1  match re/match/golf_small2.cpp
2026-10-06  00409cf0  editTerrainHeight  C0->C1  callees 0x0042f2c0 (markKind11), 0x0042f7a0 (r0_rebuild), playSound, sign; reads camera 0x004c2ba0/0x004c2ba4
2026-10-06  00409a90  scanNearbyGolfers  C0->C1  callees 0x00407000 (holeQuadrants), distance, Random, 0x00467a00; caller updateGolfers
2026-10-06  00409620  scanShotArea  C0->C1  callees narrateShot, distance, typeAtPos, tileDistance, Random, clamp; caller updateGolfers
2026-10-06  00407e00  narrateShot  C0->C1  strings 0x004c4e58 (SimFoto), 0x004c4fe8 (into the hole), skill-change strings; callees appendHoleName, appendClubName, startMessage, playSound
2026-10-06  00407c60  drawCircle  C0->C1  match re/match/golf_small23.cpp; callees setLinePoint, lineTo, 0x0042fb90 (world->screen)
2026-10-06  00407b60  sceneryLabel  C0->C1  match re/match/golf_hand_r2.cpp; string 0x004c4e38 (Scenic); callee decorationName
2026-10-06  00407700  decorationName  C0->C1  strings 0x004c4c54 (home site)..(flowerbeds/scenery); callee landmarkName
2026-10-06  004074a0  landmarkName  C0->C1  match re/match/golf_hand_s0.cpp; strings 0x004c4980 (landmark)..(oil pump/windmill/etc)
2026-10-06  00407400  canStep  C0->C1  match re/match/golf_hand_04.cpp; callee tileBlocked
2026-10-06  00407340  nearestHole  C0->C1  match re/match/golf_hand_06_k.cpp; callee distance
2026-10-06  00407280  appendHoleName  C0->C1  match re/match/golf_small24.cpp; string 0x004c4978 (Hole ); callee 0x0045b9f0 (appendString)
2026-10-06  004070b0  inRange  C0->C1  match re/match/golf_hand_r2.cpp; callee 0x0045c1e0 (Random::range)
2026-10-06  00407000  holeQuadrants  C0->C1  callee tileDistance; scans object table 0x0058bcb8; writes 0x00541318
2026-10-06  00406d50  buildStatusText  C0->C1  match re/match/golf_hand_s0.cpp; callee __itoa
2026-10-06  00406c30  canAfford  C0->C1  match re/match/golf_small33.cpp; reads cash 0x00571fd4; callee appendNumber
2026-10-06  00406670  membershipReport  C0->C1  strings 0x004c4784 (members: ), 0x004c47a4 (Current membership...); callees startMessage, playSound
2026-10-06  004065c0  showCharacterIntro  C0->C1  match re/match/golf_hand_05.cpp; strings 0x004c4714/0x004c46bc (customize-character help); callees drawWindow, 0x0045f0f0
2026-10-06  00406250  showLoadingScreen  C0->C1  strings 0x004c3fa8 (Loading...), golf-quote strings, 0x004c44ac (interface\Loading_Screens\...); callees surface ctors
2026-10-06  00406200  atexitRegister_00406200  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004061f0  staticInit_004061f0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x00474ae0 FUN_00474ae0 [??0C474ae0@@QAE@XZ]
2026-10-06  004061c0  atexitRegister_004061c0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004061b0  staticInit_004061b0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x00474ae0 FUN_00474ae0 [??0C474ae0@@QAE@XZ]
2026-10-06  00405e30  placeHabitat  C0->C1  string 0x004c3f9c ( habitat); callees addHabitatSlot, playSoundAt, queueMessage, tileBlocked
2026-10-06  00405b10  saveGameDialog  C0->C1  strings 0x004c3eec (Invalid File Name), 0x004c3f00 (Game Saved.), 0x004c3f74 (SAVE GAME prompt); callees sanitizeFileName, saveGame
2026-10-06  00405ac0  sanitizeFileName  C0->C1  match re/match/golf_hand_01.cpp; string 0x004c3ee0 (illegal filename chars); callee 0x004a5800 (_strpbrk)
2026-10-06  00405970  addHabitatSlot  C0->C1  match re/match/golf_hand_04.cpp; callee Random::range
2026-10-06  00405920  wobble  C0->C1  match re/match/golf_small8.cpp; callees Random::range, sign
2026-10-06  00405900  atexitRegister_00405900  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004058f0  staticInit_004058f0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x00474ae0 FUN_00474ae0 [??0C474ae0@@QAE@XZ]
2026-10-06  00405830  atexitRegister_00405830  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00405760  initGlobalArray_00405760  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  004056c0  atexitRegister_004056c0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00405620  initGlobalArray_00405620  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00405580  atexitRegister_00405580  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004054e0  initGlobalArray_004054e0  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00405440  atexitRegister_00405440  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004053a0  initGlobalArray_004053a0  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  004052d0  atexitRegister_004052d0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004051f0  initGlobalArray_004051f0  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00405150  atexitRegister_00405150  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004050b0  initGlobalArray_004050b0  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00405000  atexitRegister_00405000  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404f50  initGlobalArray_00404f50  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00404e90  atexitRegister_00404e90  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404e00  initGlobalArray_00404e00  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00404cf0  atexitRegister_00404cf0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404c20  initGlobalArray_00404c20  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00404bc0  drawCenteredText2  C0->C1  match re/match/golf_small7.cpp; callees Widget::setQuad, 0x00477da0 (drawCentered)
2026-10-06  00404b70  drawCenteredText  C0->C1  match re/match/golf_small7.cpp; callees Widget::setQuad, 0x00477da0 (drawCentered)
2026-10-06  00404ad0  drawTextSurface  C0->C1  match re/match/golf_hand_05.cpp; callees Widget::setQuad, Widget::value, 0x00477c30
2026-10-06  00404a20  drawTextScaled  C0->C1  match re/match/golf_hand_05.cpp; callees scaleX, Widget::setQuad, 0x00477580 (Widget::value)
2026-10-06  004049d0  drawText4049d0  C0->C1  match re/match/golf_small7.cpp; callees 0x00476310 (Widget::setQuad), 0x00477c30
2026-10-06  00404970  scaleX  C0->C1  match re/match/golf_small2.cpp; reads 0x00822c8c
2026-10-06  00404850  atexitRegister_00404850  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404840  staticInit_00404840  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  00404800  atexitRegister_00404800  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004047f0  staticInit_004047f0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  004047b0  atexitRegister_004047b0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004047a0  staticInit_004047a0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  00404760  atexitRegister_00404760  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404750  staticInit_00404750  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  00404710  atexitRegister_00404710  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404700  staticInit_00404700  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  004046d0  atexitRegister_004046d0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004046c0  staticInit_004046c0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x00482fd0 FUN_00482fd0 [??0C482fd0@@QAE@XZ]
2026-10-06  00404680  atexitRegister_00404680  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404670  staticInit_00404670  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  00404630  atexitRegister_00404630  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404620  staticInit_00404620  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  004045e0  atexitRegister_004045e0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004045d0  staticInit_004045d0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  00404590  atexitRegister_00404590  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404580  staticInit_00404580  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  00404540  atexitRegister_00404540  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404530  staticInit_00404530  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  004044f0  atexitRegister_004044f0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004044e0  staticInit_004044e0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  004044a0  atexitRegister_004044a0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404490  staticInit_00404490  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  00404450  atexitRegister_00404450  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404440  staticInit_00404440  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  00404400  atexitRegister_00404400  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004043f0  staticInit_004043f0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004837f0 FUN_004837f0 [??0C4837f0@@QAE@XZ]
2026-10-06  004043c0  atexitRegister_004043c0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  004043b0  staticInit_004043b0  C0->C1  match re/match/golf_raw_01.cpp; callee 0x00474ae0 FUN_00474ae0 [??0C474ae0@@QAE@XZ]
2026-10-06  00404380  deletingDtor_404380  C0->C1  match re/match/golf_raw_07.cpp; callees 0x004805a0 (dtor), 0x004a4ffc
2026-10-06  00404360  staticInit_404360  C0->C1  match re/match/golf_raw_01.cpp; callee 0x004804a0 (ctor)
2026-10-06  00404330  atexitRegister_00404330  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404320  staticInit_00404320  C0->C1  match re/match/golf_raw_01.cpp; callee 0x00404360 FUN_00404360 [?FUN_00404360@f_00404360@@YIPAIPAI@Z]
2026-10-06  00404280  R2C404280_dtor  C0->C1  match re/match/golf_hand_r2.cpp; callees 0x00473ae0, 0x00481ba0, 0x00482dd0
2026-10-06  00404260  deletingDtor_404260  C0->C1  match re/match/golf_raw_07.cpp; callees C404200_dtor, 0x004a4ffc
2026-10-06  00404200  C404200_dtor  C0->C1  match re/match/golf_hand_02.cpp; callees 0x00473ae0, 0x00482dd0 (member dtor)
2026-10-06  004041f0  unwindFree_4041f0  C0->C1  match re/match/golf_raw_01.cpp; called from ~110 Unwind thunks; callee 0x00473ae0
2026-10-06  004041c0  resetPair  C0->C1  match re/match/golf_small4.cpp; callees 0x00481ba0, 0x00482dd0 (member dtor)
2026-10-06  00404060  atexitRegister_00404060  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00404040  initGlobalArray_00404040  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00404000  atexitRegister_00404000  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00403fe0  initGlobalArray_00403fe0  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00403fa0  atexitRegister_00403fa0  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00403f80  initGlobalArray_00403f80  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  00403f40  atexitRegister_00403f40  C0->C1  match re/match/golf_raw_07.cpp; callee 0x004a56d2 (_atexit)
2026-10-06  00403f20  initGlobalArray_00403f20  C0->C1  match re/match/golf_arrays.cpp; callee 0x004a55d2 (??_L vector ctor iterator)
2026-10-06  004038f0  drawFlyingBall  C0->C1  strings 0x004c1564 (Im flying!), 0x004d6098 (Gary Golf); callees drawCircle, drawCenteredText, cellAt, clamp, typeBit7Clear
2026-10-06  00402a40  updateWalkers  C0->C1  callees freeAtTile, tileBlocked, typeAtPos, playSoundAt, thoughtFlag, Random; reads walker table 0x00585850 (0x4c stride) and golfer table 0x00579560
2026-10-06  00402970  spawnWalker  C0->C1  match re/match/golf_small25.cpp; callee 0x0045c1e0 (Random::range); writes walker arrays 0x00575cb9/0x00585862
2026-10-06  00402930  freeAtTile  C0->C1  match re/match/golf_small5.cpp; reads 0x005736b8/0x00575ab8
2026-10-06  004026a0  List402_clear  C0->C1  match re/match/golf_hand_03.cpp
2026-10-06  00402280  S3List_add2  C0->C1  match re/match/golf_hand_s3.cpp; string 0x004c1434; callee 0x00474860 (heap alloc)
2026-10-06  004021e0  C4021e0_dtor  C0->C1  match re/match/golf_hand_04.cpp; called from Unwind 0x004b7dc0
2026-10-06  004021b0  unwindFree_4021b0  C0->C1  match re/match/golf_raw_07.cpp; called from Unwind thunks; callee 0x004a5007 (_free)
2026-10-06  004021a0  unwindCleanup_4021a0  C0->C1  match re/match/golf_raw_01.cpp; called from Unwind 0x004b90d4..
2026-10-06  00401d10  S3List_add  C0->C1  match re/match/golf_hand_s3.cpp; string 0x004c1434 (out-of-heap message); callee 0x00474860 (heap alloc)
2026-10-06  00401c70  List401c70_dtor  C0->C1  match re/match/golf_hand_04.cpp; called from Unwind thunks 0x004b7f69..
2026-10-06  00401c30  deletingDtor_401c30  C0->C1  match re/match/golf_raw_14.cpp; callee 0x004a5007 (_free)
2026-10-06  00401c00  tickAnimals  C0->C1  match re/match/golf_small2.cpp; callee placeAnimal
2026-10-06  004017d0  placeAnimal  C0->C1  match re/match/golf_hand_s3.cpp; callees 0x0043d6f0 (cellAt), 0x0045c1e0 (Random::range), 0x0042fb90 (world->screen)
2026-10-06  00401750  initRecords  C0->C1  match re/match/golf_small10.cpp; callees 0x00401000, placeRecord
2026-10-06  004012d0  drawRecords  C0->C1  callees 0x0042fb90 (world->screen), 0x0043d6f0 (cellAt), 0x004628d0/0x00462a30 (sprite draw); reads record banks 0x004e6d20..70 and camera rotation 0x005685f4
2026-10-06  004011e0  spawnObject  C0->C1  match re/match/golf_hand_07_4011e0.cpp; callee 0x0045c1e0 (Random::range)
2026-10-06  00401040  placeRecord  C0->C1  reads record banks 0x004e6d20/d40/d50/d70 and footprint-def table 0x004c11e0
2026-10-02  00001d50  Terrain::tileAt (Terrain.dll)  C3->C4  
2026-10-02  0045c560  GolferPanelEject  C0->C1  string 'Eject Golfer' -> 'Move/Eject Golfer' in v1.03; patch note 1.02 move/eject golfer (re/versions/PATCH_MAP.md)
2026-10-02  00432720  OptionsMenuBuild  C0->C1  refs menu strings 'Save the Current Game', 'Cancel match/tournament' (new in v1.03); patch note 1.02 cancel match (re/versions/PATCH_MAP.md)
2026-10-02  004722c0  LandmarkAvailableNotice  C0->C1  v1.03 adds string ' is now available in your landmarks menu.'; patch note 1.02 free landmarks (re/versions/PATCH_MAP.md)
2026-10-02  0044cff0  CareerRetirementFlow  C0->C1  v1.00 0x0044c870 refs 'After a long and varied career, your retirement date has arrived.'; v1.03 drops it and const 0x32; patch note 1.02 'Removed 50 year limit for required retirement' (re/versions/PATCH_MAP.md)
2026-10-02  00001d50  Terrain::tileAt (Terrain.dll)  C2->C3  
2026-10-02  00001d50  Terrain::tileAt (Terrain.dll)  C1->C2  re/analysis/terrain/00001d50_TerrainDll_tileAt.md
2026-10-02  00001d50  Terrain::tileAt (Terrain.dll)  C0->C1  export ?tileAt@Terrain@@QAEPAVTile@@HH@Z via ILT thunk 0x1000108c
2026-10-02  004490d0  Terrain::tileAt  C1->C2  re/analysis/terrain/004490d0_tileAt.md
2026-10-02  004490d0  Terrain::tileAt  C0->C1  export ?tileAt@Terrain@@QAEPAVTile@@HH@Z ordinal 4 (golf_clean.exe export table)
