# Integrator prompt template (used from I1, 2026-10-24)

Replace <m>, <n> and the section-specific sentence in brackets.

You are integrator I<m> for section S<n> of the SimGolf port roadmap, in C:\Users\maria\Desktop\Proyectos\SimGolf. Read
log/roadmap/WORKER_BRIEF.md (rules) and the section's merge request log/roadmap/S<n>/MERGE.md. Apply its shared-file edits exactly
[section-specific notes]; touch registry.cpp after any modules.inc change. IMPORTANT, how to test: the shared working tree also holds other
sections' unfinished files, so never judge the gates on it. Build a MIRROR: git archive of HEAD into port/build/I<m>_S<n> plus the junctions
of log/roadmap/S0/export.md (the recipe S0 used for re/roadmap/BASELINE.md, including its shim/build and re/replay junctions), copy every
path of log/roadmap/S<n>/files.txt from the shared tree into the mirror, apply the MERGE.md edits in the mirror, build its port/build/x64 and
port/build/Win32, and run the regression gates from re/roadmap/BASELINE.md there (static, port suite one file per process with
log/p38/suite_by_file.sh, segments x64 and Win32 with digests unchanged, boot gate counted over HEAD's scenarios plus the section's own
(U-1102), the Win32 suite against BASELINE.md's known failures, gfx and terrain tests, and the section's own gate from MERGE.md). Heavy runs
strictly one at a time (other sections run heavy jobs too: before each heavy run check the free commit charge and wait if it is under 16 GB).
Tee every run to log/roadmap/merges/S<n>-<m>/. When every gate holds, apply the same shared-file edits in the shared tree and check that every
files.txt path and every edited shared file is byte-identical between the shared tree and the mirror. Write log/roadmap/merges/S<n>-<m>.md:
verdict line `MERGED` or `REFUSED <gate>`, the numbers against BASELINE.md, and the union of the section's files list plus every shared file
you edited as `files:` (one path per line). On MERGED update re/roadmap/BASELINE.md with the new numbers (state the commit it now describes as
`HEAD + S<n>`). Do not commit or stage. Report in at most 8 lines.
