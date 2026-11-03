#!/bin/bash
# S11.4: the golden-digest candidates (peer-call logs and gfx recorders that hash a renamed name), after the first set
cd /c/Users/maria/Desktop/Proyectos/SimGolf
while ! grep -q SUITE_DONE log/roadmap/S11/4_out/suite.log 2>/dev/null; do sleep 30; done
FILES="test_saveload test_browser test_save_dialogs test_loaders test_menu test_shotstyle test_gfx_info_panel" test-slot bash log/roadmap/S11/4_suite.sh
echo SUITE2_DONE >> log/roadmap/S11/4_out/suite.log
