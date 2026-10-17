# sound_18: the speech codec of sound.dll (0x1003a980..0x10041650), names re-examined (batch b6, 2026-10-17)

Scope: every function of `re/functions_ghidra_sound.dll.tsv` between RVA 0x3a900 and 0x41a00 (VA 0x1003a900..0x10041a00), 42 codec functions plus the
DSOUND import thunk 0x1003a978 and the iostream code 0x1004164f..0x100419da that the range also covers. Round 36 (m36c D1 / D2) matched the
codec at 100% as C compiled with `/Od /ZI /GZ`; the source files `re/match/m36c_D1_01.cpp` and `m36c_D2_01.cpp` are the transcription this
batch read, together with the disassembly (dumped per function, addresses cited in `re/names/sound_18.tsv`).

## Method

1. For each function the role was taken from the instructions and the call graph: loop bounds, constants (value read from the file at the cited
   address), tables, argument counts, the shared state (g_codecOrder 0x100b58a0, g_codecFrameLen 0x100b58a4, g_codecErrProtect 0x100b58a8) and
   who calls whom.
2. Where the code has the layout of a public-domain LPC-10 2400 bps routine (f2c style, state in a struct), the layout was compared with it from
   the m36c agents' notes. No reference source was opened in this batch and no reference text or comment was copied. A name is never based on
   the match alone: each RENAME row cites constants, loop bounds and callees from the binary.
3. Numerical claims about filters (low-pass, high-pass) come from evaluating the coefficients read at the cited addresses
   (cycles per sample; the sample rate is not visible in sound.dll, see [UNCERTAIN U-0019]).

## Pipeline (what the codec does)

Encode, `codecEncodeFrame` 0x1003b150 (180 float samples in, 54 bits out):

```
codecHighPassFrame 0x1003d040 -> codecHighPassFilter 0x10040e30        in place, state = encoder context +0..+0xc
codecAnalyzeFrame 0x1003c640                                           samples * 4096.0 - bias (0x1003cb73)
   codecPreEmphasis 0x10040da0              coef 0.9375 (0x10064a14)
   codecMarkCorrelationJumps 0x10040ab0     onset list osbuf
   codecSelectWindowFromJumps 0x10040740    voicing window bounds from the list
   codecLowPassFir31 0x100404d0 -> codecInverseFilter2 0x10040330 -> codecRefinePitchLag 0x10040000 -> codecAmdfPitch 0x100414f0 (3 calls)
   codecVoicingDecision 0x1003f720 (once per half, features from codecWindowFeatures 0x10041020)
   codecPitchTrackDp 0x1003f310             60 candidate lags
   codecAlignAnalysisWindow 0x1003edc0 -> codecRemoveMean 0x1003ed00 -> codecRmsEnergy 0x1003ec60
   codecCovarianceMatrix 0x1003e9f0 -> codecLdlSolveClamped 0x1003e6d0 -> codecRevertUnstableCoeffs 0x1003e600
quantizeParams 0x1003c220                                              codes for pitch, rms, 10 coefficients
bitPack 0x1003c180 -> bitPermute 0x1003bf30                            53 data bits + 1 alternating sync bit
```

Decode, `codecDecodeFrame` 0x1003a980 (54 bits in, 180 float samples out):

```
bitUnpack 0x1003c1d0 -> bitPermute 0x1003bf30
codecDecodeParams 0x1003b5a0     tables, optional error correction (g_codecErrProtect), median of three frames, codes / 16384.0
codecSynthesizeFrame 0x1003b230  pitch clamp 20..156, coefficient clamp +-0.99, output / 4096.0
   interpSubframeParams 0x1003da50   pitch-period split with log-domain interpolation of the coefficients and the rms
   per period: reflectionToLpc 0x1003d8d0 -> lpcSynthesize 0x1003d1d0 (noise: codecRing5Add 0x10040f60) -> codecSynthPostFilter 0x1003d0a0
```

Contexts: `codecAllocEncoderContext` 0x1003aa90 (0x2544 bytes) and `codecAllocDecoderContext` 0x1003aea0 (0xc00 bytes), each followed by its init function;
both init functions call `codecSetParams` 0x1003aa50 (order 10, frame 180, protection flag 1).

## Decisions (every function in the range)

| Address | Old name | Decision | Name now | Main evidence (details in sound_18.tsv) |
|---|---|---|---|---|
| 0x1003a978 | DSOUND.DLL::DirectSoundEnumerateA | CONFIRM | same | import thunk, not codec |
| 0x1003a980 | codecProcessBlock | RENAME | codecDecodeFrame | calls bitUnpack, codecDecodeParams, codecSynthesizeFrame in order (0x1003a9d4, 0x1003a9fc, 0x1003aa23) |
| 0x1003aa50 | codecSetParams | CONFIRM | same | stores 10, 0xb4, 1 at 0x1003aa68, 0x1003aa72, 0x1003aa7c |
| 0x1003aa90 | codecAllocContextA | RENAME | codecAllocEncoderContext | `push 0x2544`; the block is the context argument of codecEncodeFrame |
| 0x1003aae0 | codecInitContextA | RENAME | codecInitEncoderContext | buffers and trackers read by the analysis stages only |
| 0x1003aea0 | codecAllocContextB | RENAME | codecAllocDecoderContext | `push 0xc00`; context argument of codecDecodeFrame |
| 0x1003aef0 | codecInitContextB | RENAME | codecInitDecoderContext | pitch 60 at +0 / +0x10, noise ring at +0xbd8..+0xbe8, post-filter state +0xbec |
| 0x1003b150 | codecProcessResource | RENAME | codecEncodeFrame | 4 calls 0x1003b19c, 0x1003b1bf, 0x1003b1e3, 0x1003b207 |
| 0x1003b230 | codecSynthStage | RENAME | codecSynthesizeFrame | 180-sample output / 4096.0 (0x1003b4f6, 0x1003b50f) |
| 0x1003b5a0 | codecTransformStage | RENAME | codecDecodeParams | dequantization tables, codecCorrectNibble, median, codes / 16384.0 (0x1003befc) |
| 0x1003bf30 | bitPermute | CONFIRM, text corrected | same | 53 slots at 0x10064480 (not 54 at 0x1006447c), sync bit |
| 0x1003c180 | bitPack | CONFIRM | same | calls bitPermute with mode 0 |
| 0x1003c1d0 | bitUnpack | CONFIRM | same | calls bitPermute with mode 1 |
| 0x1003c220 | quantizeParams | CONFIRM | same | x 32768.0 (0x1003c28e), caps 0x3ff / 0x3f / +-0x7f, binary search table 0x100647e4 (0x1003c348) |
| 0x1003c640 | codecStageDispatch | RENAME | codecAnalyzeFrame | fixed 14-stage pipeline with history shifts, not a dispatcher |
| 0x1003d040 | codecApplyFixedCoeffs | RENAME | codecHighPassFrame | forwards to the high-pass filter with start index 1 (0x10064a18 is the constant 1, not a table) |
| 0x1003d0a0 | applyIir2 | RENAME | codecSynthPostFilter | three feedback taps (0x1003d155, 0x1003d163, 0x1003d170), third order |
| 0x1003d1d0 | lpcSynthesize | CONFIRM | same | excitation (pulse table 0x1003d48e or noise 0x10040f60), two all-pole passes, gain from sqrt |
| 0x1003d8d0 | reflectionToLpc | CONFIRM | same | step-up recursion, 1.0 at 0x1003d93a, sqrt of the product of (1 - k^2) |
| 0x1003da50 | interpSubframeParams | CONFIRM | same | log / exp interpolation per pitch period, 90.0 (0x1003e1c1) |
| 0x1003e2c0 | intPow | CONFIRM | same | square-and-multiply |
| 0x1003e380 | floatCopySign | CONFIRM | same | magnitude of the first, sign of the second (0.0 at 0x1003e39d) |
| 0x1003e400 | floatFloorToInt | RENAME | floatRoundHalfAway | floor(x + 0.5) / -floor(0.5 - x) around 0.0 at 0x1003e41d |
| 0x1003e480 | medianOfThree | CONFIRM | same | median of three ints |
| 0x1003e520 | decodeTableSymbol | RENAME | codecCorrectNibble | parity folds + 128-entry table + error counts (0x1003e54f..0x1003e5eb) |
| 0x1003e600 | substituteIfBelowThreshold | RENAME | codecRevertUnstableCoeffs | copies the previous vector when any |x| > 0.99 (condition was inverted in the old text) |
| 0x1003e6d0 | gaussSolveClamped | RENAME | codecLdlSolveClamped | factorization + one forward pass, pivot 1e-10, clamp 0.999; no back-substitution |
| 0x1003e9f0 | codecAutocorrelation | RENAME | codecCovarianceMatrix | shift recurrence with edge terms (0x1003eba0..0x1003ebd2) |
| 0x1003ec60 | codecRmsEnergy | CONFIRM | same | sum of squares / N, sqrt |
| 0x1003ed00 | codecRemoveMean | CONFIRM | same | mean subtraction |
| 0x1003edc0 | codecAlignAnalysisWindow | CONFIRM | same | existing evidence (nine-argument window placement with floatRoundHalfAway) matches the placement loop |
| 0x1003f310 | codecGainSmoother | RENAME | codecPitchTrackDp | 60 scores, two back-pointer rows of 60 (0x1003f3df), 0.75 / 0.984375 / 16.0 / 128.0 |
| 0x1003f720 | codecFeatureDecision | RENAME | codecVoicingDecision | produces voibuf flags that select noise or pulse excitation |
| 0x10040000 | codecPitchAnalysis | RENAME | codecRefinePitchLag | three codecAmdfPitch calls, 41 / 80 / 20 constants |
| 0x10040330 | codecCorrelationLags | RENAME | codecInverseFilter2 | three correlations then a two-tap residual filter at lags 4 and 8 |
| 0x100404d0 | codecSymmetricFir | RENAME | codecLowPassFir31 | 31 symmetric taps, response 0.989 at DC, below 0.01 above 0.2 cycles/sample |
| 0x10040740 | codecSelectWindowFromJumps | CONFIRM | same | existing evidence is address-level and matches |
| 0x10040ab0 | codecMarkCorrelationJumps | CONFIRM | same | 63.0 / 64.0 smoothing (0x10040bbd, 0x10040bc5), 16-slot ring, threshold 1.7 (0x10040d12) |
| 0x10040da0 | codecFirstOrderFilter | RENAME | codecPreEmphasis | caller passes 0.9375 |
| 0x10040e30 | codecBiquadFilter | RENAME | codecHighPassFilter | numerator (1 - z^-1)^2 per section: 0.0 at DC, 0.707 at 0.0125, 1.0 above 0.05 cycles/sample |
| 0x10040f60 | codecRing5Add | CONFIRM | same | renamed in b3 from codecCombFilter; 5-slot additive ring, caller lpcSynthesize |
| 0x10041020 | codecWindowFeatures | CONFIRM | same | eight window features: zero crossings (dither flip), band energies, correlations; 90.0 / 4.0 |
| 0x100414f0 | codecAmdfPitch | CONFIRM, text corrected | same | outputs are table positions, not lags |
| 0x1004164f..0x100419da | operator<<, opfx, osfx, flush, ostream, FUN_100419af | CONFIRM | same | C++ iostream code inside the range, not codec (FID names) |

DEFER: none of the 42 functions. What stays open is listed at the end.

## Structural comparison for the routine-shaped names

Each of these functions has the loop bounds, constants and call structure of the matching public LPC-10 routine as listed in the m36c notes; the
names above are behavioural. Differences the m36c agents recorded (from their memory of the reference, `log/c3/m36c_D2_notes.txt`, not a
source comparison) and differences found here:

| Function | Constants and bounds that match | Differences |
|---|---|---|
| codecHighPassFilter | two sections, gain 0.902428, coefficient pairs at 0x1005ecc0..0x1005ecb4 | state held in the caller's struct (first four words); no static state |
| codecLowPassFir31 | 31 taps, loop `j = len + 1 - nsamp .. len`, centre tap 0.250536 | none recorded |
| codecInverseFilter2 | lags 0, 4, 8; stride 2; threshold 1e-10 | none recorded |
| codecRefinePitchLag | candidates 41, 80, 20, +-3, +-5, 60-entry table | last block assigns `i__2 = *minptr - 5` (m36c D2 note: not `i__1`) |
| codecVoicingDecision | 5 rows x 10 words, thresholds 600 450 300 200, 63 / 64, 3000, 20.0, 16-case state switch | updates voice[] and maxmin only for half 1; value[1] = lbe / max(lbve, 1); indexes voibuf[half + 6]; L2 test reads voibuf[7] == 0 (m36c D2) |
| codecWindowFeatures | window [start, stop] halves, 90.0 / vlen, 4.0, 32767 cap | none recorded |
| codecPitchTrackDp | 60 lags, 2 x 60 back pointers, 0.75, 0.984375, 128, 16, quarter-range test for 20 / 30 / 40 | none recorded |
| codecAlignAnalysisWindow | lrange = (af - 2) * lframe + 1, hrange = af * lframe | none recorded |
| codecSelectWindowFromJumps | minimum separation 90, window 156, lowest start 307 | case 1 reads the previous window's end; `crit = FALSE` is the loop's fall-through (m36c D2) |
| codecMarkCorrelationJumps | 63 / 64 smoothing, 16-slot ring, 1.7, hysteresis 10 | none recorded |
| codecCovarianceMatrix / codecLdlSolveClamped | order 10 matrices, clamp 0.999, pivot 1e-10 | none recorded |
| codecInterp (interpSubframeParams) | loop `while (1)` with a vflag test | goto versus break not determined (m36c D1) |
| bitPermute | 53 slot indices, 13 slots, sync bit flipped per frame | compiled non-static (m36c D1) |
| codecRing5Add | 5-slot ring, indices 2 and 5 at start, seeds at +0xbe0 | index decrement wraps to 5 at 0 |

## Context layouts (offsets from the init stores)

Encoder context (0x2544 bytes, codecInitEncoderContext 0x1003aae0): high-pass words +0x0..+0xc; input buffer +0x10 (540 floats); pre-emphasis buffer
+0x880 (540); low-pass buffer +0x10f0 (696); inverse-filter buffer +0x1bd0 (312); DC bias +0x20b0; onset list +0x20b4 (10) with count +0x20dc; boundary codes
+0x20e0 (3); voicing windows +0x20ec (6) and analysis windows +0x2104 (6); voicing flags +0x211c (8); rms history +0x213c (3); coefficient history +0x2148
(30); pre-emphasis state +0x21c0; correlation-jump tracker +0x21c4..+0x2220; voicing tracker +0x2224..+0x2264; pitch scores +0x2268 (60); back pointers +0x2358
(120); row index +0x2538; penalty +0x253c; sync flag +0x2540.

Decoder context (0xc00 bytes, 0x1003aef0): previous pitch +0x0; first-frame flag +0x4; voicing history +0x8/+0xc; average pitch +0x10; error rate +0x14; coefficient
history +0x18 (30); pitch history +0x90 (3); rms history +0x9c (3); output carry buffer +0xa8 (360 floats) with count +0x648; previous voicing and pitch
+0x64c; previous rms +0x654; previous coefficients +0x658 (10); residual sample count +0x680; first-synthesis flag +0x684; period carry +0x688; excitation buffers
+0x68c / +0x924 (166 floats each); excitation filter words +0xbbc..+0xbd4; noise ring indices +0xbd8 / +0xbdc and words +0xbe0..+0xbe8; post-filter words +0xbec..+0xbfc.

## Tables and globals changed (sound_18_globals.tsv)

- g_codecPermTable 0x1006447c was one dword before the table it named: the code reads `[i*4 + 0x1006447c]` with i from 1, so the table is at 0x10064480
  (53 entries, last 0x10064550); 0x1006447c is the last entry of the 10-entry sign-mask table. Rows: g_codecFrameBitSlots 0x10064480 int[53],
  g_codecCoeffSignMask 0x10064458 int[10]. g_codecBitMasks 0x10064454 (same 1-based base) was dropped; 0x10064444..0x10064454 holds the five flag
  values 2 4 8 16 32 that codecDecodeParams tests (`[0x10064448..0x10064454]`), still unnamed.
- g_codecMode 0x100b58a8 -> g_codecErrProtect (it selects error correction in codecDecodeParams and protection words in quantizeParams).
- g_codecFrameLen 0x100b58a4 added.
- g_codecSymbolTable text updated for codecCorrectNibble.

## MATCH lines that no longer share a name with the names table

The MATCH decorated names in `re/match/m36c_D1_01.cpp` and `m36c_D2_01.cpp` are the C symbols of the transcription (`_lpc10_decode`,
`_analys_`, ...). They never carried the `re/names` spelling, so nothing breaks, but these 22 of the 38 lines now sit next to a different behavioural name:
_lpc10_decode 0x1003a980, _create_lpc10_encoder_state 0x1003aa90, _init_lpc10_encoder_state 0x1003aae0, _create_lpc10_decoder_state 0x1003aea0,
_init_lpc10_decoder_state 0x1003aef0, _lpc10_encode 0x1003b150, _synths_ 0x1003b230, _decode_ 0x1003b5a0, _analys_ 0x1003c640, _deemp_ 0x1003d0a0, _i_nint 0x1003e400, _ham84_ 0x1003e520, _rcchk_ 0x1003e600, _invert_ 0x1003e6d0, _mload_ 0x1003e9f0,
_dyptrk_ 0x1003f310, _voicin_ 0x1003f720, _tbdm_ 0x10040000, _ivfilt_ 0x10040330, _lpfilt_ 0x100404d0, _preemp_ 0x10040da0, _hp100_ 0x10040e30.
The dbg_raw wrappers (`?FUN_1003d040@f_1003d040@@...`) carry the address only. The other 16 agree with a kept name or have no behavioural counterpart in the table:
_bsynz_ (lpcSynthesize), _pitsyn_ (interpSubframeParams), _irc2pc_, _encode_ (quantizeParams), _chanwr_0_ (bitPermute), _pow_ii, _r_sign, _median_, _energy_, _dcbias_, _placea_, _placev_,
_onset_, _vparms_, _difmag_, _random_.

## Open items

- Sample rate of the codec input: the frame is 180 samples; sound.dll holds no sample rate for it [UNCERTAIN U-0019]. Filter cut-offs are therefore given in cycles per sample.
- The meaning of the sixteen flag cases in codecVoicingDecision (which isolated flags each case flips) is described in the binary's terms only.
- The flag table at 0x10064444 (2 4 8 16 32) that codecDecodeParams tests with `icorf & bit` has no name.
- Text in unchanged rows that quoted an old name was refreshed in place (21 fields in sound_5..sound_9 and their globals files).
