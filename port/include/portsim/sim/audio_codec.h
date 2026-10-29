// sound.dll's LPC-10 2400 bps speech codec (step S4.3, log/roadmap/S4/3_notes.md): module audio_codec, 42 functions registered
// under their hooks.csv names (codecEncodeFrame / codecDecodeFrame and the analysis, quantisation, bit packing, parameter
// decoding, interpolation and synthesis stages below them, the context allocators and initialisers). It has no API of its own:
// the functions are reached through the audio core's dispatcher (portsim/sim/audio.h: snd_call by rva) like any sound.dll code.
//
// x87: the bodies are float code with the original's `fstp dword` spills; none loads a control word, so each runs at its
// caller's. The judging keys run at 0x027f, where the double arithmetic of the port equals the original on x64 and Win32 alike
// (port/CORE.md section 7); FuncDef.fpcw is 0x027f for the functions listed here.
#ifndef PORTSIM_SIM_AUDIO_CODEC_H
#define PORTSIM_SIM_AUDIO_CODEC_H

namespace portsim {

// The functions of the module whose result depends on the caller's x87 control word (every one with float arithmetic).
inline constexpr const char *kAudioCodecX87Functions[] = {
    "codecDecodeFrame",       "codecEncodeFrame",      "codecSynthesizeFrame",     "codecDecodeParams",
    "quantizeParams",         "codecAnalyzeFrame",     "codecHighPassFrame",       "codecSynthPostFilter",
    "lpcSynthesize",          "reflectionToLpc",       "interpSubframeParams",     "floatCopySign",
    "floatRoundHalfAway",     "codecRevertUnstableCoeffs", "codecLdlSolveClamped", "codecCovarianceMatrix",
    "codecRmsEnergy",         "codecRemoveMean",       "codecAlignAnalysisWindow", "codecPitchTrackDp",
    "codecVoicingDecision",   "codecRefinePitchLag",   "codecInverseFilter2",      "codecLowPassFir31",
    "codecSelectWindowFromJumps", "codecMarkCorrelationJumps", "codecPreEmphasis", "codecHighPassFilter",
    "codecRing5Add",          "codecWindowFeatures",   "codecAmdfPitch",           "codecInitEncoderContext"};

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_CODEC_H
