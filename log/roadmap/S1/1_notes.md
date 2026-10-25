# S1.1 notes (2026-10-24)

## (a) U-1067
`trace_format.undelivered_input(tr)`, called by `validate()` (so trace_tool record / validate use it): a first-generation input MSG
(WM_KEYDOWN/UP/SYSKEYDOWN/SYSKEYUP 0x100,0x101,0x104,0x105 and mouse buttons 0x201..0x209) needs an earlier DELIVER post of the
same message; each delivery explains one message.
Measurement over 266 stored traces (port/corpus/traces + segments/*/*/trace.trace, script in the scratchpad):
- first rule tried (every 0x100..0x109, 0x200..0x20e): 176 traces break it, because WM_CHAR (0x102, made by TranslateMessage from a
  KEYDOWN) and WM_MOUSEMOVE (0x200, from cursor deliveries) have no DELIVER of their own. Narrowed to first-generation messages.
- narrowed rule: 1 trace breaks it, cov_l_camera/00 (KEYDOWN VK 38 at frame 0 of a segment slice): the message was in the Windows
  queue at the slice start (segment_corpus.md, INPUT IN FLIGHT, start.json queued_messages). Slices (meta `slice`) are exempt up
  to their second FRAME record. After that: 266/266 validate.
Tests: tests/test_trace_format_s1.py (7: cov_r_quit shape KEYDOWN 174 rejected, one-delivery-one-message, derived messages exempt,
mouse button rejected, slice exemption only for frame 0, every stored trace validates), tests/test_trace_format.py still 15 pass.
trace_tool.py unchanged (it calls TF.validate). Negative control: the synthetic cov_r_quit-shape trace is RED (1 of 1).
## (b) U-1060
re/analysis/data/sve_header_readers.md; pinned by port/test/test_sve_header_readers.py (8 static tests: 4 header read sites, 2 writers,
0x64 push before each read, decoders hold no buffer operand, drawText and the browser use NUL scans). Result: no reader looks past
the NUL; U-1060 resolved for the original (residue is dead data). Reproduce:
`~/bin/py -3.12 -m pytest tests/test_trace_format_s1.py port/test/test_sve_header_readers.py -q`.
Browser decomposition kept in log/roadmap/S1/1_browser_decomp.txt.
## Open
- Controls for (b): the tests are structural, no wrong-body control applies; U-1150 records that.
