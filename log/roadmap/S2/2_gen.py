"""S2.2: writes re/roadmap/S2_screens.tsv (checkpoint rows are the table CP below, family rows are computed from hooks.csv
callees and the gfx.cpp kBodies list). Run: ~/bin/py -3.12 log/roadmap/S2/2_gen.py"""
import csv, io, re, os
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
hooks = list(csv.DictReader(open(os.path.join(ROOT, 'hooks.csv'), encoding='utf8')))
exe = {r['addr'].lower().replace('0x', '').zfill(8): r for r in hooks if r['module'] == 'golf_clean.exe'}
byname = {r['name']: r for r in exe.values()}
gfx = open(os.path.join(ROOT, 'port/src/sim/gfx.cpp'), encoding='utf8').read()
blk = gfx[gfx.index('const Named kBodies[]'):]
blk = blk[:blk.index('};')]
BODIES = set(re.findall(r'\{"([^"]+)"', blk))
BODIES.add('drawWindow')  # kBodies "drawWindow.frame": frame of a golfer-less dialog only
DRAW = set('''drawText4049d0 drawTextScaled drawTextSurface drawCenteredText drawCenteredText2 drawPanelS2 drawWindowFrame drawFrameR3
drawFrameS2 drawWindow drawPanelBackdrop Surface_ctor Surface_free Surface_blit Surface_blitTakeDirty Surface_drawScaled Surface_drawRegion
Image_draw Surface_stretchBlit Surface_blit2 Surface_drawSprite Image_ctor Image_dtor Image_releaseAll Surface_createFull loadBmpSurface
loadImage Surface_box Surface_fillRegion Surface_blitThunk Widget_setQuad Widget_draw Widget_draw2 drawStringAt drawStringAt2 drawText
drawRight drawStringRightAt drawTextLinesAt drawCenteredTextLines drawRectScaled Text_put Widget_fillRect Win_fillRect fillWindowRect
Window::refreshFull drawMessageBuffer blitPanel pixelRed pixelGreen pixelBlue composeGolferSprite addSprite drawThoughtBubble
drawScrollText'''.split())
# the family table: family, full_screen, port functions (hooks.csv names), port source, pinned checkpoint that shows it
FAM = [
 ('difficulty (selectDifficultyScreen)', 'yes', ['selectDifficultyScreen'], 'commands_screens.inc', 'none'),
 ('theme packs (showThemePackSelect)', 'yes', ['showThemePackSelect'], 'menu.cpp', 'none'),
 ('load browser (loadGameBrowserScreen)', 'yes', ['loadGameBrowserScreen'], 'menu.cpp / saveload.cpp', 'cov_a_keys k_dlg_L; cov_r_reload browser keys'),
 ('pick a pro (pickAProScreen)', 'yes', ['pickAProScreen'], 'menu.cpp', 'none'),
 ('credits (showCreditsScreen)', 'yes', ['showCreditsScreen'], 'commands_screens.inc', 'cov_r_quit credits, quit_00..03'),
 ('hire employee (showHireEmployeeDialog)', 'dialog over course', ['showHireEmployeeDialog'], 'commands_screens.inc', 'none (ui_tour presses the key, no checkpoint on it)'),
 ('reports F1-F10 (financial, comments, best scores, histograph, hole stats)', 'yes', ['showFinancialReport', 'showPlayerCommentsReport', 'showBestScoresScreen', 'showHistographScreen', 'showHoleStatsScreen'], 'commands_screens.inc / holestats.cpp', 'none'),
 ('SGA course report (showCourseReport)', 'dialog over course', ['showCourseReport'], 'reports.cpp', 'cov_a_keys k_dlg_J'),
 ('end of year (showEndOfYearScreen)', 'yes', ['showEndOfYearScreen'], 'reports.cpp', 'none'),
 ('character editor (showCharacterEditor)', 'yes', ['showCharacterEditor'], 'chareditor.cpp', 'cov_a_career career_editor; cov_a_golfer button_0; cov_a_keys3 q_editor'),
 ('character intro popup (showCharacterIntro)', 'dialog over course', ['showCharacterIntro'], 'dialogs.cpp', 'cov_a_career career_popup'),
 ('preferences (showPreferencesMenu)', 'dialog over course', ['showPreferencesMenu'], 'commands_menus.inc', 'none'),
 ('system menu (showSystemMenu)', 'dialog over course', ['showSystemMenu'], 'commands_menus.inc', 'cov_r_quit exit_prompt'),
 ('bulletin board (showBulletinBoard)', 'dialog over course', ['showBulletinBoard'], 'dialogs.cpp', 'ui_tour menus; cov_k1_animals bought; cov_b_inject2 debug_overlay (a SimFoto and message ball; the board reads as the tutorial text ball)'),
 ('popups (showPopup)', 'dialog over course', ['showPopup'], 'commands_popup.inc', 'cov_a_champ n_prompt; cov_a_play n_prompt; cov_a_build2 remove_prompt1; cov_a_keys k_C; cov_a_keys2 C_prompt; cov_k3_career prompt, prompt2; cov_r_quit exit_prompt'),
 ('text input box (textInputBox)', 'dialog over course', ['textInputBox', 'promptAndSaveFile', 'saveGameDialog'], 'commands_popup.inc / saveload.cpp', 'cov_a_keys k_dlg_S; cov_r_quit exit_save'),
 ('scroll text (scrollTextWindow, runScrollText, drawScrollText)', 'yes', ['scrollTextWindow', 'runScrollText', 'drawScrollText'], 'dialogs.cpp', 'none'),
 ('golfer info panel / thought bubble / skill points', 'dialog over course', ['drawGolferInfoPanel', 'drawThoughtBubble', 'skillPointDialog'], 'dialogs.cpp', 'none (cov_a_golfer panel_* are H1WAIT with the panel inside the toolbar)'),
 ('routing map (showRoutingMapScreen)', 'yes', ['showRoutingMapScreen'], 'routing.cpp', 'cov_a_keys k_dlg_R; cov_k1_routing routing'),
 ('shortcuts / membership roster', 'yes', ['showShortcutsScreen', 'showMembershipRoster'], 'routing.cpp', 'none'),
 ('tournament results (showTournamentResults)', 'dialog over course', ['showTournamentResults'], 'tick_panels.inc', 'cov_b_shots results*, results_r*'),
 ('buy land (showBuyLandScreen)', 'yes', ['showBuyLandScreen'], 'tick_frame.inc', 'none'),
 ('course action menu / world map (courseActionMenu)', 'yes', ['courseActionMenu'], 'saveload.cpp / course.cpp', 'cov_b_location (PASS); cov_a_keys k_worldmap (PASS)'),
 ('loading screen (showLoadingScreen)', 'yes', ['showLoadingScreen'], 'commands_screens.inc / menu.cpp', 'none'),
 ('main radial menu (showMainRadialMenu)', 'dialog over course', ['showMainRadialMenu'], 'tick_frame.inc', 'none'),
 ('shot analysis overlay (render dot/slash)', 'dialog over course', ['drawWindow'], 'tick_frame.inc', 'cov_a_keys k_render_dot, k_render_slash; cov_b_decor terrain'),
 ('SimFoto / message ball (drawWindow with a golfer, drawMessageBuffer)', 'dialog over course', ['drawWindow', 'drawMessageBuffer'], 'tick_frame.inc', 'championship tour_0180; cov_k2_simfoto bottom, right; cov_a_golfer button_2'),
]
CP = [  # scenario, checkpoint, family, full_screen, viewed, note
 ('cov_r_quit', 'credits', 'credits', 'yes', 'yes', 'scrolling credits over the pinboard photo; text near the bottom; context MODAL_AFTER_H1 showCreditsScreen'),
 ('cov_r_quit', 'quit_00', 'credits', 'yes', 'no', 'same screen as quit_02 viewed; credits scroll position differs'),
 ('cov_r_quit', 'quit_01', 'credits', 'yes', 'no', 'same screen as quit_02 viewed'),
 ('cov_r_quit', 'quit_02', 'credits', 'yes', 'yes', 'credits, DESIGN / PROGRAMMING block scrolled in; first unsupported is Snd::status (a drop with no gfx body, S2.1 adds it to kNotDrawing)'),
 ('cov_r_quit', 'quit_03', 'credits', 'yes', 'no', 'same screen, later scroll'),
 ('cov_r_quit', 'exit_prompt', 'popups + system menu', 'dialog over course', 'yes', 'retirement popup over the system menu (8 items) over the course; blocked first by Snd::status'),
 ('cov_r_quit', 'exit_save', 'text input box', 'dialog over course', 'yes', 'SAVE GAME text box over the retirement popup over the system menu'),
 ('cov_a_keys', 'k_dlg_S', 'text input box', 'dialog over course', 'yes', 'SAVE GAME text box, plain course behind'),
 ('cov_a_keys', 'k_dlg_L', 'load browser', 'yes', 'yes', 'Load Previous Game list (autosaves, .sve files), left panel empty; not in the census MODAL kind=frame list'),
 ('cov_a_keys', 'k_dlg_R', 'routing map', 'yes', 'yes', 'Routing Map: hole table, isometric mini map drawn from tile colours, round buttons'),
 ('cov_a_keys', 'k_dlg_J', 'SGA course report', 'dialog over course', 'yes', 'REPORT of the SIM GOLF ASSOCIATION, grade table with star rows; panel covers the top 410 px, course and toolbar visible below'),
 ('cov_a_keys', 'k_C', 'popups', 'dialog over course', 'yes', 'overwrite-file popup (2 options)'),
 ('cov_a_keys2', 'C_prompt', 'popups', 'dialog over course', 'yes', 'overwrite-file popup, same as k_C'),
 ('cov_a_keys', 'k_render_dot', 'shot analysis overlay', 'dialog over course', 'yes', 'Shot Analysis window with coloured trajectory ellipses on the course'),
 ('cov_a_keys', 'k_render_slash', 'shot analysis overlay', 'dialog over course', 'no', 'same key family as k_render_dot'),
 ('cov_b_decor', 'terrain', 'shot analysis overlay', 'dialog over course', 'yes', 'Shot Analysis window over a decorated course, build-terrain toolbar'),
 ('cov_a_career', 'career_popup', 'character intro popup', 'dialog over course', 'yes', 'customize-character popup (2 options)'),
 ('cov_a_career', 'career_editor', 'character editor', 'yes', 'yes', 'editor for Gary Golf: skill bars, 20 thought lines, portrait'),
 ('cov_a_golfer', 'button_0', 'character editor', 'yes', 'yes', 'editor opened for visitor Gianni (Decorator), bio text and Visitor stats on the right'),
 ('cov_a_keys3', 'q_editor', 'character editor', 'yes', 'yes', 'editor for visitor Dizzy (Musician)'),
 ('cov_a_golfer', 'button_2', 'SimFoto / message ball', 'dialog over course', 'yes', 'SimFoto photo frame over the course, tool palette below; the checkpoint is MODAL:pumpAndRedraw'),
 ('cov_a_champ', 'n_prompt', 'popups', 'dialog over course', 'yes', 'cancel-tournament popup, golfer toolbar with shot-style buttons'),
 ('cov_a_play', 'n_prompt', 'popups', 'dialog over course', 'yes', 'cancel-match popup'),
 ('cov_a_build2', 'remove_prompt1', 'popups', 'dialog over course', 'yes', 'demolish-building popup, build palette'),
 ('cov_k3_career', 'prompt', 'popups', 'dialog over course', 'yes', 'career-ended popup (sandbox or main menu)'),
 ('cov_k3_career', 'prompt2', 'popups', 'dialog over course', 'yes', 'same popup as prompt'),
 ('cov_k3_career', 'answered', 'title menu', 'yes', 'no', 'NOT_RUN: title screen after the answer, no trace snapshot'),
 ('cov_k3_career', 'end', 'title menu', 'yes', 'no', 'NOT_RUN'),
 ('cov_a_champ', 'n_yes', 'title menu', 'yes', 'no', 'NOT_RUN: title after quitting the tournament'),
 ('cov_a_champ', 'end', 'title menu', 'yes', 'no', 'NOT_RUN'),
 ('championship', 'tour_0180', 'SimFoto / message ball', 'dialog over course', 'yes', 'SimFoto and a golfer message ball (face, text); first unsupported is Image_draw with a SpriteRef without sprite'),
 ('championship', 'tournament_end', 'tournament results', 'dialog over course', 'no', 'NOT_RUN: no snapshot; results screen expected (family reference: cov_b_shots results)'),
 ('ui_tour', 'menus', 'bulletin board + SimFoto', 'dialog over course', 'yes', 'bulletin / message ball with Biggie plus a SimFoto frame'),
 ('cov_k1_animals', 'bought', 'bulletin board + SimFoto', 'dialog over course', 'yes', 'message ball (Billy) plus SimFoto'),
 ('cov_b_inject2', 'debug_overlay', 'bulletin board + SimFoto', 'dialog over course', 'yes', 'message ball plus SimFoto plus the debug tile grid overlay'),
 ('cov_k2_simfoto', 'bottom', 'SimFoto / message ball', 'dialog over course', 'yes', 'SimFoto frame at the bottom of the screen, no text ball'),
 ('cov_k2_simfoto', 'right', 'SimFoto / message ball', 'dialog over course', 'no', 'SimFoto at the right edge, sibling of bottom'),
 ('cov_b_shots', 'results', 'tournament results', 'dialog over course', 'yes', 'TOURNAMENT RESULTS strip over the top 140 px, course below'),
 ('cov_b_shots', 'results_r2', 'tournament results', 'dialog over course', 'no', 'same strip, later round'),
 ('cov_b_shots2', 'results', 'tournament results', 'dialog over course', 'no', 'same strip'),
 ('cov_b_shots2', 'results_r2', 'tournament results', 'dialog over course', 'no', 'same strip'),
 ('cov_b_shots2', 'results_r3', 'tournament results', 'dialog over course', 'yes', 'strip with winner Joseph Mary Ole'),
 ('cov_k1_routing', 'routing', 'routing map', 'yes', 'no', 'routing map at the end of the k1 scenario; family viewed through cov_a_keys k_dlg_R'),
 ('cov_a_flags', 'mood5', 'none (tooltip over course)', 'no', 'yes', 'course view with a Rocks 400 tool tooltip; no 2D screen, HEAD context, first unsupported is Snd::status'),
 ('cov_a_golfer', 'q_done', 'none', 'no', 'yes', 'course with golfers and speech lines; no 2D screen; first unsupported Surface_blit3'),
 ('cov_a_build', 'amen_717', 'none', 'no', 'yes', 'course with an amenity palette; no 2D screen; the cause is Surface_blit3 (HUD / sprites), S3 or HUD'),
 ('cov_a_build', 'bldg_8', 'none', 'no', 'yes', 'course with a building palette; no 2D screen'),
 ('cov_k3_camera2', 'x_edge4', 'none', 'no', 'yes', 'camera at the map edge, HUD only; Snd::status'),
 ('cov_k3_champ', 'shot', 'none', 'no', 'yes', 'professional tournament scoreboard in the toolbar plus a leader board text at the top left; the cause is Image_draw with a SpriteRef'),
 ('ui_tour', 'reports', 'none', 'no', 'yes', 'despite the name this frame is the build palette with a Palm tree tooltip; no report screen'),
]
lines = []
hdr = 'kind scenario checkpoint family full_screen reference_viewed port_functions drawing_callees callees_with_body callees_missing notes'.split()
cpmap = {}
for sc, ck, fam, fs, vw, note in CP:
    lines.append(['checkpoint', sc, ck, fam, fs, vw, '', '', '', '', note])
for fam, fs, fns, src, shows in FAM:
    allc, ports = set(), []
    for fn in fns:
        r = byname.get(fn)
        if not r:
            ports.append(fn + '(no hooks.csv row)'); continue
        ports.append('%s %s %s' % (fn, r['addr'], r['confidence']))
        for a in (r['callees'] or '').split(';'):
            a = a.strip().lower().replace('0x', '').zfill(8)
            n = exe.get(a, {}).get('name')
            if n in DRAW and n != fn:
                allc.add(n)
    have = sorted(c for c in allc if c in BODIES)
    miss = sorted(c for c in allc if c not in BODIES)
    note = 'port source %s; pinned checkpoint: %s' % (src, shows)
    if fam.startswith(('shot analysis', 'SimFoto')):
        note += '; drawn by the exe through drawWindow / its callees (see drawing_callees)'
    lines.append(['family', '', '', fam, fs, '', '; '.join(ports), ' '.join(sorted(allc)), ' '.join(have), ' '.join(miss), note])
out = os.path.join(ROOT, 're/roadmap/S2_screens.tsv')
with open(out, 'w', encoding='utf8', newline='') as f:
    w = csv.writer(f, delimiter='\t', lineterminator='\n')
    w.writerow(hdr)
    w.writerows(lines)
print(len(CP), 'checkpoint rows,', len(FAM), 'family rows ->', out)
