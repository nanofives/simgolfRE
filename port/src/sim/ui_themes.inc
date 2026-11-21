// P40B (log/roadmap/P40/P40_notes.md): the procedural UI's themes, as data. Included by gfx_ui.cpp as the built-in theme text;
// the app also reads ui_themes.ini beside simgolf_port.exe or in the data folder (same format), whose sections replace or add to
// these (a section starts from the built-in theme of its name, or from `game`, or from the theme named by its `base` key).
// Format: [name], then `key = r g b [alpha]` (channels 0..255, alpha 0..1) or `key = number`; `#` starts a comment.
// `game` holds the values measured from the user's install (log/roadmap/P40/holestat_geometry.txt; HoleSTAT.pcx, OkStates.pcx).
R"P40THEMES(
[game]
# the panel: the head cell's most frequent colour; the rim 174..132 over x 161..165, the shade 143..105 over x 633..638
frame = 148 150 198
frame_bevel = 5
frame_light = 0.60
frame_dark = 0.45
# the drop shadow under the panel and the button bay (the art: dark rows 368..372, 96 98 129 over the frame colour)
shadow = 20 24 48 0.30
shadow_d = 2
shadow_blur = 3
# the title plate: cream, 2-pixel outline 42 49 91, a sunken inner top shade
plate = 255 247 231
plate_edge = 42 49 91
# the label pills (147..182 shading into 191 193 251) and the value boxes (196..243 shading into the cream)
label_top = 170 172 226
label_bot = 191 193 251
value_top = 232 225 210
value_bot = 255 247 231
pill_outline = 32 42 75
pill_outline_w = 1.6
pill_bevel = 3
# report cells (gfx_ui.cpp): each region in the art's own median colour (the panel, every box, the lines)
art_colours = 1
# the connectors
line = 32 42 75
line_w = 2
# the comment box and its sunken surround (96 98 129 above, 174..208 below)
box = 255 247 231
box_edge = 176 178 214
# table rows painted white and the grid lines between cells (coursereport, memberRoster, histograph)
row = 255 255 255
grid = 206 200 220
# the close button: the ring, the dark well, the blue ball (122..213), its hover yellow, the gloss and the check
ring = 139 141 186
ring_outline = 40 48 90
well = 45 52 88
ball_top = 140 155 240
ball_bot = 200 228 255
ball_hover_top = 240 180 0
ball_hover_bot = 255 238 40
gloss = 255 255 255 0.9
check = 36 47 84

[slate]
# an alternate palette (not the game's): a dark slate panel, white boxes, teal accents, a deeper shadow
frame = 58 70 96
frame_light = 0.30
frame_dark = 0.55
shadow = 0 0 0 0.6
shadow_d = 5
shadow_blur = 8
plate = 250 251 253
plate_edge = 64 196 214
label_top = 206 228 238
label_bot = 226 242 248
value_top = 240 242 245
value_bot = 255 255 255
pill_outline = 18 26 40
line = 64 196 214
line_w = 2.4
box = 250 251 253
box_edge = 84 98 128
row = 238 244 250
grid = 120 140 170
ring = 84 98 128
ring_outline = 12 18 30
well = 18 24 36
ball_top = 30 160 180
ball_bot = 110 225 235
ball_hover_top = 235 110 40
ball_hover_bot = 255 190 90
check = 12 20 32
art_colours = 0
)P40THEMES"
