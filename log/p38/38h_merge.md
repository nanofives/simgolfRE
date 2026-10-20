# Stream 38h: lines for the parent to merge

1. `port/src/sim/modules.inc`: append one line (any position; the module registers by hooks.csv name and calls its peers
   through the registry):

   ```
   SG_MODULE(chareditor)
   ```

   Registered names: showCharacterEditor, pickCharEditor, nearestCharControl, charEditorZone, plus test entries
   chareditor.setScript / scriptStats / clearScript / scriptPeer / clearPeers. None of these is registered by another
   module in the working tree (checked against the merged 38a..38g DLL).

2. `port/test/test_tick.py` OPEN_MISSING: delete the entry
   `"showCharacterEditor": "not registered (cov_a_keys3/01 after drawGolferInfoPanel, 38d)",`
   (cov_a_keys3/01 PASS 118/118 with the module).

3. `re/port/commands_split.tsv`: replace row `004385d0` (class gap) by the row in `log/p38/38h_split_rows.tsv`
   (`log/p38/apply_split_rows.py log/p38/38h_split_rows.tsv` as for 38c).

4. `UNCERTAINTIES.md`: the rows of `log/p38/38h_uncertain.tsv` (U-1030..U-1035), and
   - U-0996 (cov_a_keys3/01 stops on showCharacterEditor): resolved by module chareditor. Correction to its text: the
     segment opens the editor with H1's key 0x3f (TAKE at 0x0041c0d7 value 63, mainLoop ML+0xe387), not clickGolferPanel.

5. `CLAUDE.md` (one paragraph, the parent's wording; facts in log/p38/38h_notes.md): module `chareditor` (phase 3.8 stream
   38h) ports showCharacterEditor with pickCharEditor / nearestCharControl / charEditorZone; its oracle runs View_toLocal
   for real over a fake display (cursor slot +0x2c, slot +0x9c, compositor object for composeGolferSprite), treats the
   release-wait pumps (return addresses 0x0043a23a / 0x0043a254) as drains, and types keys as an idle WM_KEYDOWN pump plus
   the WM_CHAR (consecutive key batches let flushRedraw's pending flag erase every other key); the bio EditBox is not ported
   (U-1032); cov_a_keys3/01 PASS.

No shared file was edited by this stream.
