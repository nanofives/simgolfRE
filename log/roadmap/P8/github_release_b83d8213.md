Windows build of the SimGolf port at commit b83d8213 (same build as LAN release b83d8213). Bring your own SimGolf v1.03 install: no game files are included.

Changes since the earlier play-test builds:
- Widescreen on by default: the course fills the window and follows resizes, the world renders at the window's native resolution, and the HUD panels sit at the window edges scaled to its height (`--wide off` gives the 800 x 600 picture).
- The world-load indicator is drawn in the game's own banner style.
- Closing the window leaves through the game's own exit (save prompt in a session).
- `play` profile fixes: walkers, golfers and the camera stay on the map; hover tips show after about 0.4 s; sounds and stories follow the wide view.
- Earlier: resizable window with smooth scaling, + / - zoom keys, no edge scroll while unfocused, course report and top-10 screens fixed.

Run `simgolf_port.exe --install <your SimGolf folder>` or set SIMGOLF_INSTALL and use `Play SimGolf.bat`.
