# Readers of a loaded .sve / .cse header (S1.1, U-1060, 2026-10-24)

Question (U-1060): saveGame 0x0040b4a0 and saveCourse 0x0040b680 write all 0x64 bytes of g_textBuffer (0x0051a068) as the
header line, so the bytes after the text's NUL are leftover text the port does not reproduce. Does any reader of a loaded
header use the bytes past the NUL?

## Every path from a file into g_textBuffer

`__read` 0x004a583a is called with `push 0x0051a068` at exactly four sites (found by scanning every function range of
golf_clean.exe, `port/test/test_sve_header_readers.py`):

| site | function | read |
|---|---|---|
| 0x0040ba91 | loadGame 0x0040b9b0 | header line 1, 0x64 bytes (0x0040ba7d); the first byte is cleared first (0x0040ba8a) |
| 0x0040baaa | loadGame | header line 2, 0x64 bytes (0x0040baa2), only when the fourth argument (championship file) is set (0x0040ba99) |
| 0x0040b88f | loadCourse 0x0040b840 | header line 1, 0x64 bytes (0x0040b87b); first byte cleared (0x0040b888) |
| 0x0040b92c | loadCourse | header line 2, 0x64 bytes (0x0040b91f) |

The other `__read` callers read elsewhere: loadTopScores 0x0040ad60 (the score table), saveFieldIo 0x0040af70 (body fields,
destination is each global), `__fread_lk` / `__filbuf` (the CRT; story and theme text files, not .sve / .cse). The two
body decoders serializeCourse 0x0040afa0 and deserializeCourse 0x0040bbf0 and saveFieldIo hold no operand inside
0x0051a068..0x0051b067 (static test), so nothing in the body path reads the buffer.

The writers are exactly the two savers (`__write` with the buffer): saveGame and saveCourse.

## Consumers of the loaded text, and whether each stops at the NUL

| consumer | where | stops at the NUL |
|---|---|---|
| loadGameBrowserScreen 0x0043b610, list rows | it calls `loadGame(name, 0, 1, 0)` (header only, call at the browser's first loop) and copies the buffer into a row string with `repne scasb` + `rep movs` sized by the scanned length, then strcat's it | yes: 58 `repne scasb` scans (test pins >= 20), no `mov ecx, 0x19` block copy, no file call |
| drawText4049d0 0x004049d0 -> drawText 0x00477c30 | draws the row and the other g_textBuffer lines | yes: drawText measures with `repne scasb` (0x00477c60, 0x00477c6e) and passes min(requested, strlen) to drawRichText 0x004775b0 |
| loadGame full load (second argument set) | strcpy's the label at 0x004c5304 into the buffer (0x0040bb70..), overwriting the header | writer, not a reader |
| loadCourse mode 0 | strcpy's the label at 0x004c52f4 into the buffer (0x0040b8bd) | writer, not a reader |
| loadGame / loadCourse with a second header (champ files, loadCourse always) | the second 0x64-byte read overwrites the whole buffer prefix, residue of line 1 included | n/a |
| mainLoop `loadGame(0x004c60b0, 1, 0, 0)` (0x0041c102) | full load, label overwrites the header | writer |

Result: no reader of a loaded header looks at a byte after the first NUL. The 0x64 length appears in the browser only as the
100-byte stride of the file-name table (0x0080b130 + n * 100), never as a block copy of the header text.

## Decision

U-1060 resolved for the original: the residue after the NUL is dead data that no code path in golf_clean.exe reads, so the
port may keep the text and the NUL and report the residue (what test_sve_roundtrip.py does). It matters only if something
outside the game reads a file byte-for-byte: re/tools/sve.py reads the header as text up to the NUL (not re-checked here),
and a byte-equality check of whole files would need the ticker's text layout (phase 5). `loadThemes` / `loadThemeFile` /
`.srf` readers use the CRT's fgets over other files and are outside the set.

Not measured here: no recording shows a loaded header being displayed with residue (the list rows are drawn from the
NUL-bounded copy by construction).
