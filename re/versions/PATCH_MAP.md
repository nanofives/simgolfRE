# Patch map: v1.00 -> v1.03 function changes linked to the official patch notes

Source data: `re/versions/v100_v103.{tsv,md}` from `re/tools/version_diff.py` over Ghidra fingerprints
(`re/tools/fingerprints.py`, `ghidra/scripts/ExportFingerprints.java`). Binaries: `golf_v100_clean.exe`
(SafeDisc 2.51.021 unwrapped) and `golf_clean.exe` (v1.03, SafeDisc 2.60.52 unwrapped), both anchored.

Match result (2026-10-02): 2211 -> 2207 functions; 2157 matched (exact 1342, exact-ordered 745,
strings 33, callgraph 29, similar 7, shape 1); 2098 identical, 48 changed, 11 operand-only,
54 removed, 50 added.

A link below is listed only when a **string** that the function references appears or disappears
between versions and that string matches a patch-note item. Constants are listed as supporting data.

| Patch note (v1.01-1.03 notes in `original/readme v103.txt` / `SimGolf v103 Patch Notes.txt`) | v1.00 | v1.03 | Evidence | Reach in sandbox_basic |
|---|---|---|---|---|
| "Removed 50 year limit for required retirement" | `0x0044c870` | `0x0044cff0` | removed strings "After a long and varied career, your retirement date has arrived." and "After a long career, you plan to retire af..."; removed consts `0x2d`, `0x32` (50), `0x40` | not reached |
| "A better system for placing landmarks ... Free landmarks now appear in your landmark menu" | `0x00425b50` | `0x004266b0` | added " Go to 'Landmarks' under the Improvements menu to place your landmark."; removed " For a nominal installation fee you may place it anywhere on your course." | not reached |
| same | `0x00471a70` | `0x004722c0` | added " is now available in your landmarks menu." | not reached |
| "New ability to cancel a match or tournament (wrench icon)" | `0x00431db0` | `0x00432720` | menu string list now contains "Cancel match/tournament"; consts `0x7`, `0x6e`, `0x154` added | sandbox 1/3 runs |
| "You can pick up a golfer and move him/her ... eject-golfer icon" | `0x0045be30`* | `0x0045c560` | "Eject Golfer" -> "Move/Eject Golfer" (code identical; string data changed) | not reached |
| "golfers sometimes feel their partner's pain" (difficulty) | `0x00469330` | `0x00469b00` | added 5 "..., PARTNER" lines ("PARTNER, your attitude stinks.", ...) | not reached |

\* instruction-identical (matched `exact`; v1.00 `0x0045be30`); only the referenced string changed.

Other changed functions with clear string anchors but no single patch-note line yet:
`0x00434cf0` ("000-14,000" -> "000-20,000", a price range), `0x0043b610` (drops
"WARNING: Missing .chr files:"; theme loading, candidate for "themes should now load automatically"),
`0x00453330` (v1.00 `0x00452c60`; typo fix "Avgerage" -> "Average", data only), `0x0040a160` (adds "Permanent";
demolish/pathway UI, candidate for "Tees and Holes can no longer be removed during a tournament").
Added in v1.03: `0x0046d200` (" Change hole ", TV towers, green rolling: hole-improvement menu) and
`0x004038f0` ("I'm flying!").

## What this tells the test strategy

Five of the six patch-linked functions never ran in the 40 s sandbox scenario. The canonical
scenarios have to reach game time passing (career/retirement), landmark donation, tournaments
and partner play before these systems can be verified. That is the next scenario work, and the
coverage census will say when it is enough.

Seeded at C1 from this evidence (2026-10-02): `0x0044cff0` CareerRetirementFlow, `0x004722c0`
LandmarkAvailableNotice, `0x00432720` OptionsMenuBuild, `0x0045c560` GolferPanelEject.
