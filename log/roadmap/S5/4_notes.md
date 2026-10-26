# S5.4 notes: selection, whole-gate measurements with FreeType, the report (2026-10-24)

Status: DONE. Report: re/analysis/data/text_freetype.md. Merge: 4_merge.md. Uncertainties 4_uncertain.tsv (U-1530..U-1532).

## Method
Shared harness files are not edited. 4_patch.py writes patched copies (log/roadmap/S5/4_harness/{segments,boot,integrator}.py:
`text_provider(mode, lib)` maps gdi -> gditext, freetype -> fttext, anything else None); 4_site/sitecustomize.py loads them as
port.test.* in every process when S5_4_PATCH=1 (PYTHONPATH=log/roadmap/S5/4_site), 4_run.py runs boot / integrator through them.
Mirror build: `SG_FREETYPE_ROOT=C:/Users/maria/vcpkg/installed ~/bin/py -3.12 log/roadmap/S5/4_build.py mirror --build both`
(port/build/S5_4, S5_4_w32; 4_build.log).

## Numbers
| gate | result | log |
|---|---|---|
| segments x64, SG_TEXT_METRICS=freetype | 205 / 205 PASS, 0 digests differ from log/p5/segs_x64_loc.tsv (66 s) | 4_segs_ft_x64.* , 4_segs_ft_x64_compare.log |
| segments x64, default (patched harness) | 205 / 205, 0 differ | 4_segs_gdi_x64.* |
| segments Win32, freetype | 205 / 205, 0 differ from the x64 freetype rows | 4_segs_ft_w32.log, 4_segs_ft_w32.tsv |
| boot x64, freetype | HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from S0 (extra rows as S5.1: cov_r_bio, cov_r_flic_input FAIL) | 4_boot_gate_ft_x64.*, 4_boot_compare_ft_x64.log |
| boot Win32, freetype | same 58 / 2 / 1, 0 differ (4_boot_w32.py, sequential) | 4_boot_gate_ft_w32.*, 4_boot_compare_ft_w32.log |
| gfx checkpoints, fttext | menu 1,735; refused 15,347; site_hover 13,964; buttons 13,600; tip 13,714; back 1,735 px, all in label boxes; Win32 identical | 4_gfx_ft_x64.*, 4_gfx_ft_w32.* |
| gfx checkpoints, gditext | 0 px on all six | 4_gfx_gdi_x64.* |
| selection control | freetype ppem+1 control: ui_tour/00, /02, championship/00 FAIL; gdi with the same control PASS; none: ui_tour FAIL; controls 1 and 3 PASS on these segments (U-1530) | 4_provider_probe.log |

The boot gate with freetype passes through boot.run's selection; gfx checkpoints reuse 3_gfx_ft.py (which sets the provider itself).

## Reproduce
```
~/bin/py -3.12 log/roadmap/S5/4_patch.py write
export SG_PORTSIM_DLL=port/build/S5_4/Release/portsim_c.dll SG_TEXT_METRICS=freetype
~/bin/py -3.12 log/roadmap/S5/4_run.py integrator segments --backend x64 --dll $SG_PORTSIM_DLL --jobs 8 --out <tsv>
~/bin/py -3.12 log/roadmap/S5/4_run.py boot --all --jobs 3 --out <tsv> && ~/bin/py -3.12 log/roadmap/S5/4_boot_compare.py <tsv>
S5_4_PATCH=1 PYTHONPATH=log/roadmap/S5/4_site ~/bin/py -3.12 log/roadmap/S5/4_boot_w32.py --out <tsv>   # Win32
S5_4_PATCH=1 PYTHONPATH=log/roadmap/S5/4_site ~/bin/py -3.12 log/roadmap/S5/4_provider_probe.py
```
