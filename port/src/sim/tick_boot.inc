// Phase 4, the boot path (re/PHASE4_BOOT_PLAN.md): what gameMain and mainLoop's Setup do to the game's state before the
// title menu, for a run that starts from the user's exe image (sg_exe_fill_state 1) instead of an original frame-head image.
// Included by tick.cpp inside struct Frame, after tick_menu.inc. Written from the C3 transcriptions: gameMain from
// shim/src/re/c3fv.cpp GameMain_re, Setup from shim/src/re/c3fy.cpp Setup (converted with log/p3/p37a_conv.py, then edited:
// drawing calls go to their drop names, the autosave probe is dropped). Citations: gameMain+0x<off>, ML+0x<off>.

// gameMain up to mainLoop (gameMain+0x6 .. gameMain+0x458): the screen size words; the engine, display, window, icons, fonts,
// message-box and widget defaults, the resample and colour tables, the title screen (the intro movie and the button
// surfaces: showTitleScreen), the sound table (loadSoundTable: sound slots only) and the colour cycle are the renderer's /
// the audio backend's. initSound is the audio backend's. seedFromTime is the RNG seed (gameMain+0x28d).
void BootGameMain() {
    G32(GS(g_screenWidth)) = 0x320;                                    // gameMain+0x6
    G32(GS(g_screenHeight)) = 0x258;                                   // gameMain+0x10
    // engineInit (gameMain+0x2a) creates the display and stores its object in g_graphicsDevice (pointer class, a heap
    // address). The port tests it only against 0 (View_toLocal's cursor read, chareditor, holestats) and never dereferences
    // it, so the boot stores a non-zero token: the display exists. Without it no cursor is read and the title menu's
    // click lands on the wrong hotspot.
    G32(GS(g_graphicsDevice)) = 1;
    // engineInit runs with every flag bit (-1); its step 0x4000 is initSinTable (c3fa.cpp engineInit, util.cpp): g_sinTable,
    // which sinScaled reads (golfers, shots). Its other steps build the display, fonts, cursors and the UI defaults.
    cc("initSinTable");
    // engineInit's step 0x80 setPrimarySurface creates the primary font (g_font83ad20), gameMain's nine font calls the UI
    // fonts (gameMain+0x29f..gameMain+0x35f): their FontRefs get what text measuring reads (gditext.createFont), when the
    // context has the text provider
    if (text_metrics(ctx)) {
        const u32 fonts[] = {GS(g_font83ad20), GS(g_font51b320), GS(g_fontRef_519fd8), GS(g_font51b360), GS(g_font519928),
                             GS(g_font51a028), GS(g_fontRef_519a40), GS(g_font519948), GS(g_font51b340), GS(g_font519968)};
        for (u32 f : fonts) cc("gditext.createFont", f);
    }
    // the message-box and widget defaults (gameMain+0x158 .. gameMain+0x274; c3fv.cpp GameMain_re), between the window's
    // creation and initSound
    G32(GS(g_msgBoxButtonB) + 44) = GS(g_rootWindow);                 // gameMain+0x158
    G32(GS(f_cfg_TextView_4e44e4) + 28) = 7;                           // gameMain+0x17e
    G8(GS(f_cfg_TextView_4e44e4) + 144) = (char)0xF8;                  // gameMain+0x19d
    G32(GS(f_cfg_TextView_4e44e4) + 156) = 2;    G32(GS(f_cfg_Button_4e443c) + 12) = 2;
    G32(GS(f_cfg_TextView_4e44e4) + 148) = 7;    G32(GS(f_cfg_TextView_4e44e4) + 152) = 0xF8;
    G32(GS(g_msgBoxButtonB) + 12) = 0;           G32(GS(f_cfg_View_4e42c8)) = 1;
    G32(GS(f_cfg_View_4e42c8) + 28) = 0xF8;      G32(GS(g_viewDefaults) + 12) = 0;
    G32(GS(f_cfg_View_4e42c8) + 20) = 8;         G32(GS(g_viewDefaults) + 8) = 0xFF;
    G32(GS(f_cfg_View_4e42c8) + 8) = 0;          G32(GS(f_cfg_View_4e42c8) + 12) = 1;
    G32(GS(f_cfg_View_4e42c8) + 16) = 1;         G32(GS(f_cfg_ListBox_4e44d0) + 4) = 0;
    G32(GS(f_cfg_ListBox_4e44d0) + 8) = 0xFF;    G32(GS(f_cfg_ListModel_4e4488) + 16) = 7;
    G32(GS(f_cfg_EditBox_4e43f0) + 4) = 1;       G32(GS(g_editBoxDefaults)) = 0;
    G32(GS(g_editBoxDefaults) + 4) = 0;          G32(GS(g_editBoxDefaults) + 8) = 0;
    G32(GS(f_cfg_EditBox_4e43f0) + 12) = -1;     G32(GS(f_cfg_EditBox_4e43f0) + 16) = 1;
    G32(GS(f_cfg_EditBox_4e43f0) + 20) = 1;      G32(GS(f_cfg_EditBox_4e43f0)) = 7;
    G32(GS(f_cfg_Button_4e443c)) = 0xF8;         G32(GS(f_cfg_Button_4e443c) + 4) = 0xFF;
    G32(GS(f_cfg_Button_4e443c) + 8) = 0xF8;     G32(GS(g_buttonDefaults)) = 0;
    G32(GS(g_buttonDefaults) + 4) = 0;           // gameMain+0x274
    cc("seedFromTime");                                                // gameMain+0x28d
    G32(GS(g_primarySurface)) = GS(g_font519928);                      // gameMain+0x36d
    G32(GS(g_ui83abdc)) = 0;                                           // gameMain+0x377
    // gameMain+0x41f showTitleScreen: the intro movie and the title button / window-frame sprites (a drop; the phase 5 graphics
    // backend draws its sprite part when one is attached). loadSoundTable (gameMain+0x42e) and the colour cycle are not the boot's.
    // gameMain+0x41f showTitleScreen plays the intro first (its call at showTitleScreen+0x27, immediate 0): the video
    // module (S8.3), whose default host takes the original's own skip path (BinkOpen NULL)
    cc("playAudioFile", (GS(f_strpool_4c89ac) + 96), 0);           // ./flics/SMSG_introfinal.bik
    cc("showTitleScreen");
    G32(GS(g_ui83abdc)) = 0;                                           // gameMain+0x452
}

// mainLoop's Setup ML+0x22 .. ML+0x437 (one time, then the title menu ML+0x11875).
void BootSetup(u32 F) {
    // ML+0x2f playSound(0x80, 0x64, 0, 0, 0): the audio backend's
    for (int i = 0; i < 0x114; i++) G32(GS(g_tileTypeAttrs) + 4 * i) = G32(GS(f_tab_tileTypeAttrInit_4c1a40) + 4 * i);  // ML+0x4e
    // ML+0x5f / ML+0x7b / ML+0x97 Surface_createFull of the three title Surfaces, ML+0xa3 PaletteCache::ctor: the renderer's
    FD(0x124C) = 0;                                                    // ML+0xb2 SEH scope state
    MenuSeedSlots(F);                                                  // ML+0xbd..ML+0x1f6 the button geometry and labels
    tc("setDrawTarget", Root(), GS(g_font51a028), 0, 0, 0);            // ML+0x201
    tc("loadImage", GS(g_image568d10), (GS(f_strpool_4c6ba4) + 152), GS(g_palette5199c8), 0, 0x100, 0);   // ML+0x21e
    // ML+0x24e Surface_blit3 and the seven menu items and five labels ML+0x253..ML+0x2c0: drawing, issued with an attached
    // graphics backend (roadmap S2.7: the title art the boot's loading screens are drawn over; negative control 10 of
    // gfx.family.boot_screens leaves it out). The same calls as the title menu's first draw (tick_menu.inc) without the theme
    // line under Start New Game.
    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {
#if PORTSIM_NEGCTL
        static const int fam = registry_find("gfx.family.boot_screens");
        const FuncDef *fd = fam >= 0 ? registry_at(fam, nullptr) : nullptr;
        const bool art = !(fd && fd->negctl && *fd->negctl == 10);
#else
        const bool art = true;
#endif
        if (art) {
            tc("Surface_blit3", GS(g_image568d10), Root(), 0, 0, 0x320, 0x258, 0, 0,
               G32(GS(g_screenWidth)), G32(GS(g_screenHeight)));                              // ML+0x24e
            for (int esi = 0; esi < 7; esi++) {                                               // ML+0x253..ML+0x2c0
                const int k = ((esi == 6) ? 1 : 0) + 2 * esi;
                tc("Image_draw", k * 44 + GS(g_spriteRef_53b7e0), Root(), (int)FW(0x150 + esi * 4),
                   (int)FW(0x152 + esi * 4), 0);                                              // ML+0x288
                if (esi < 5)
                    cc("drawCenteredText", FD(0x1F4 + esi * 4), (int)FW(0xF4 + esi * 4), (int)FW(0xF6 + esi * 4) + 8,
                       (int)0x80006318);                                                      // ML+0x2b4
            }
        }
    }
    tc("setDrawTarget", Root(), GS(g_font51a028), 0, 0, 0);            // ML+0x2d3
    G32(GS(g_preferences)) = -1;                                       // ML+0x2e5
    G32(GS(g_difficulty)) = -1;                                        // ML+0x2eb session mode = none
    G32(GS(g_courseId)) = Rnd(0xFFFF) & 0xFFFF;                        // ML+0x2f1
    cc("loadTopScores");                                               // ML+0x300
    if (cc("fileExists", (GS(f_strpool_4c6ba4) + 132)))                // ML+0x30a Themes\Default.pro
        cc("loadThemeFile", (GS(f_strpool_4c6ba4) + 120), 0, 0);       // ML+0x31f
    G8(GS(g_courseType)) = 0;                                          // ML+0x330
    StrCpy(GS(g_theme), (GS(f_strpool_4c6ba4) + 108));                 // ML+0x350 "Standard"
    // ML+0x352 loadInfoScreenArt: the report art is the renderer's; its five fonts are measured (gditext.createFont)
    if (text_metrics(ctx)) {
        const u32 fonts[] = {GS(g_fontRef_821020), GS(g_fontRef_821f08), GS(g_fontRef_821ec8), GS(g_fontRef_821f28),
                             GS(g_fontRef_821ee8)};
        for (u32 f : fonts) cc("gditext.createFont", f);
    }
    // its image half: the report screens' art cut into their Surfaces (roadmap S2.4, gfx_routing.cpp)
    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, "loadInfoScreenArt.images", nullptr, 0);
    cc("showLoadingScreen", 0);                                        // ML+0x359
    tc("Window::refreshFull", GS(g_rootWindow), 0);                    // ML+0x368
    cc("loadInterfaceArt", 1);                                         // ML+0x36f
    G32(GS(unk_0058dd34)) = -1;                                        // ML+0x379
    G32((GS(unk_0058dd34) + 4)) = -1;                                  // ML+0x381
    G32((GS(unk_0058dd34) + 8)) = -1;                                  // ML+0x387
    G32((GS(unk_0058dd34) + 12)) = -1;                                 // ML+0x38d
    G32((GS(unk_0058dd34) + 16)) = -1;                                 // ML+0x393
    G16((GS(unk_0058dd34) + 20)) = -1;                                 // ML+0x399
    G8((GS(unk_0058dd34) + 22)) = -1;                                  // ML+0x3a0
    for (int i = 0; i < 0x10; i++)                                     // ML+0x3a6..ML+0x3b7 scatter 0..15 by the order table
        GU8(GS(unk_0058dd34) + G8(GS(f_tab_bytes_4c2d28) + i)) = (u8)i;
    StrCpy((GS(g_ui_5694a0) + 8), (GS(f_strpool_4c6ba4) + 92));        // ML+0x3c3
    StrCpy(GS(g_unreadText569528), (GS(f_strpool_4c6ba4) + 76));       // ML+0x3e7
    StrCpy(GS(g_statusText), GS(g_str4e9a84));                         // ML+0x409 (empty default string)
    // ML+0x431 FindFirstFileA(saved games\&AutoSave1.sve, F+0x1104): the find data is read by nothing the port runs; the
    // handle is never closed. Then ML+0x437 jmp ML+0x11875, the title menu.
}

// The C++ static initialization the CRT runs before gameMain (__cinit over the initializer table): of its constructors, these
// write state the port runs on: staticInit_820f04 (the record bank list header, course.cpp), staticInitGTerrain (gTerrain =
// Terrain::getInstance(), the Terrain.dll import, through the context's TerrainApi) and allocTileBuffer. The others construct
// render / UI / audio objects (the boot gate's measure: with these three no simulation object differs at frame 1).
void BootCrt() {
    cc("staticInit_820f04");
    G32(GS(gTerrain)) = terrain_api(ctx).get_instance();
    // allocTileBuffer (c3cz.cpp, the CRT initializer dynInit_initList820f18): the render tile list gTileBuffer: a 0xc-byte head
    // node from operator new linked to itself, the size 0. Its allocator byte +0 takes the high byte of ecx at the call (0
    // in the captured image of cov_l_point; pointer class, never compared).
    const u32 head = terrain_api(ctx).operator_new(0xc);
    G32(head) = head;
    G32(head + 4) = head;
    G32(GS(gTileBuffer) + 4) = head;
    G32(GS(gTileBuffer) + 8) = 0;
}

// The boot step: the CRT's static construction, gameMain's part, Setup, then the title menu and Session as after a frame's
// exit to the menu.
FrameExit boot(u32 F) {
    BootCrt();
    BootGameMain();
    BootSetup(F);
    return menu_session(F);
}
