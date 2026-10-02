# UNCERTAINTIES

Explicit knowledge holes. Every `[UNCERTAIN U-NNNN]` marker in a note or source must have a row here.
Types: `semantic` / `structural` block C3 for that address; `behavioural` / `tooling` do not.

| ID | addr | type | description | path to resolution | status |
|----|------|------|-------------|--------------------|--------|
| U-0001 | 0x0045baf0 | tooling | `golf_clean.exe` loads `jgld.dll` (Jackal **debug** build, `C:\JackDev\Debug\jgld.pdb`), not the release `jgl.dll` shipped beside it. Why, and whether the release DLL is a drop-in, is not established. | Find the `LoadLibraryA("jgld.dll")` call site in Ghidra, read the selection logic; A/B boot with `jgl.dll` substituted. | open |
| U-0002 | 0x0045baf0 | behavioural | SafeDisc-protected `golf.exe` run through SafeDiscLoader2 exits with code 0 right after reading `jackal.txt` (return path `0x004a51cd` <- `0x004a5115` <- `0x004a6918`); `golf_clean.exe` does not. Cause not established. | Not needed for the RE target; revisit only if the protected exe is ever needed as a reference. | open |
