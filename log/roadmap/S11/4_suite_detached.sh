#!/bin/bash
# S11.4: a representative set of port test files, detached (Start-Process), one w8_lock hold per file
cd /c/Users/maria/Desktop/Proyectos/SimGolf
FILES="test_golfers test_course_menu test_audio_sound test_events test_vectors test_defects_golfers" test-slot bash log/roadmap/S11/4_suite.sh
echo SUITE_DONE >> log/roadmap/S11/4_out/suite.log
