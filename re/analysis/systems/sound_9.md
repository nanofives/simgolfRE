# sound_9: sound.dll codec analysis stages (hand-named, 2026-10-08)

Two stages under `codecStageDispatch` 0x1003c640, named because `floatCopySign` 0x1003e380 and `floatFloorToInt`
0x1003e400 were held at C2 with only these unnamed callers (log/c3/c3bd_notes.txt). Both are /Od __cdecl bodies that
end with `call 0x100431ce` (__chkesp).

- `codecMarkCorrelationJumps` 0x10040ab0 (direct stage of codecStageDispatch, 8 stack arguments): per sample it
  smooths the lag-1 product and the energy with weights 63/64 (0x1005ec68 = 63.0, 0x1005ebfc = 64.0) into context
  slots +0x21c4 / +0x21c8, stores their ratio in +0x21cc (replaced by floatCopySign(1.0, r) from 0x10064e3c when
  |r| exceeds the energy), keeps a 16-slot ring with a running sum (+0x21d0, +0x2210; indices +0x2214 / +0x2218
  wrap with `and 0x8000000f`), and when the running sum moves by more than 1.7 (0x1005ecac) appends `sample - 9` to the
  caller's index array (count *param_3 bounded by *param_4) and sets the open-event flag +0x2220, which clears once the
  last jump (+0x221c) is more than 9 samples back.
- `codecWindowFeatures` 0x10041020 (called by the unnamed 0x1003f720, itself a codecStageDispatch stage): over a
  half-overlapping window of two sample arrays it counts sign changes of x plus an alternating dither (*param_6,
  sign flipped per sample, via floatCopySign), and writes seven features: the crossing rate and the two mean
  absolute levels scaled by 90/n (0x1005ec30 = 90.0, levels divided by 4.0 at 0x1005ec20, rounded by floatFloorToInt
  and clamped to 0x7fff), sum|dx| / (2 sum|x|), the normalised lag-1 product, and the squared normalised correlations
  of the second array at -lag and +lag (*param_7). Every divisor below 1.0 (0x1005b8a8) is replaced by 1.0.
- `codecFeatureDecision` 0x1003f720 (direct stage of codecStageDispatch, 13 stack arguments, added 2026-10-08 after
  round 15 because it is codecWindowFeatures' only caller): per half-window it fills a feature vector through
  codecWindowFeatures, picks a row of the threshold table 0x10064e10, scores the features against that row's bias
  (0x10064c7c) and eight weights (0x10064c54, 0x28-byte rows), writes the sign of the score as a 0/1 flag at
  param_11+0x18+half*4, and on the second half adjusts the four flags +0xc/+0x10/+0x14/+0x18 through a switch on
  their combined 4-bit value. The name is from that structure (a per-half binary decision over a weighted feature
  sum); which signal property the flag stands for is not determined by the code.
