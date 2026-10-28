// The drawing-body families of the graphics backend (port/src/sim/gfx_body.h, roadmap S2.1). One line per family file
// port/src/sim/gfx_<family>.cpp, which defines `extern const portsim::GfxFamily kGfxFamily_<family>` and
// `int g_negctl_gfx_<family>`. gfx.cpp expands this list into explicit references (dispatch table and one registry entry
// `gfx.family.<family>` per family, whose negctl switch is the family's). Adding a family: one line here, nothing else.
SG_GFX_FAMILY(selftest)
SG_GFX_FAMILY(terrain)
SG_GFX_FAMILY(scene)
SG_GFX_FAMILY(surface)
SG_GFX_FAMILY(prim_text)
SG_GFX_FAMILY(credits)
SG_GFX_FAMILY(prim_fill)
SG_GFX_FAMILY(prim_text2)
SG_GFX_FAMILY(routing)
SG_GFX_FAMILY(boot_screens)
SG_GFX_FAMILY(reports)
SG_GFX_FAMILY(dialogs)
SG_GFX_FAMILY(hud)
SG_GFX_FAMILY(pixel)
SG_GFX_FAMILY(buyland)
SG_GFX_FAMILY(chareditor)
SG_GFX_FAMILY(results)
SG_GFX_FAMILY(simfoto)
SG_GFX_FAMILY(bulletin)
SG_GFX_FAMILY(text)
