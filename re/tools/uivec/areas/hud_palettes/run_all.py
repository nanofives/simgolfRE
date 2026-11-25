"""Rebuild every element of the hud_palettes area in dependency order (the trays and the mode fan compose other elements'
scenes, so they run last).  py -3.12 run_all.py"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ORDER = ["els_hud.py", "els_tabs.py", "els_bulldozers.py", "els_rotate.py", "els_amen_tabs.py", "els_modes.py", "els_course.py",
         "els_badge.py", "els_gb.py", "els_mood.py", "els_slots.py", "els_ped.py", "els_arrows.py", "els_bracket.py", "els_strip.py",
         "els_trays.py", "els_modebase.py"]
for k, m in enumerate(ORDER):
    r = subprocess.run([sys.executable, os.path.join(HERE, m)], cwd=HERE, capture_output=True, text=True)
    print(f"[{k + 1}/{len(ORDER)}] {m} {'PASS' if r.returncode == 0 else 'FAIL'}", flush=True)
    sys.stdout.write(r.stdout)
    if r.returncode:
        sys.stdout.write(r.stderr)
