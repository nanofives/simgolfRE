SimGolf port (development build)
================================

This is the Windows port of Sid Meier's SimGolf rebuilt from the reverse-engineering project. It plays from the
game files in game\ and loads none of the original binaries.

Start:   double-click "SimGolf (port)" (made by the updater) or "Play SimGolf.bat".
Update:  close the game, then double-click update.bat. Only changed files are downloaded.
Saves:   %LOCALAPPDATA%\SimGolfPort\files (saved games, top10.sve, character files).
         %LOCALAPPDATA%\SimGolfPort\recordings holds one folder per session: send it if something goes wrong.

Not in this build yet: sound (the audio engine merge is pending) and fast zooming (a zoom frame takes about
0.2 s until the faster rasteriser merges). Network play is not planned for this round.
