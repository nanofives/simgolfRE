"""Patches of the converter's output for the S4.7 assembler (7_assemble.py): PATCHES = (block, old, new) exact replacements and
(block, pattern, replacement, "re") DOTALL regex replacements; VOID_OFF = for each function that returns nothing, the dword (offset
from `this`) negative-control mode 1 changes after the body.

Frame cells (U-1460): locals the original passes by address live in 16-byte cells of the image's tail padding (fcell in the head of
audio_sound.cpp: cells 0.. at image end - 0x4e0, below audio_seq's band).
"""

PATCHES = []

GLOBAL_RE = [
    (r"fldc<void\*>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<u32>(ctx, \1 + \2)"),
    (r"fldc<unsigned char>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint8_t>(ctx, \1 + \2)"),
    (r"fldc<unsigned short>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint16_t>(ctx, \1 + \2)"),
    (r"fldc<unsigned>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint32_t>(ctx, \1 + \2)"),
    (r"fldc<int>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<int32_t>(ctx, \1 + \2)"),
]

VOID_OFF = {
    "Sound::dtor": 0xa48,
    "SoundTimer::stop": 0x4c,
    "SoundTimer::stopKeepLock": 0x4c,
    "Sound_setName": 0x1c,
    "Sound5b6a8_dtor": 0x214,
    "Sound5c450_dtor": 0x58,
    "MultiSound_dtor": 0x1430,
    "Sound_setSourceType": 0x54,
}
