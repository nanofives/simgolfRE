# exe_12: the image pipeline (2026-10-12)

Names from round 29's batch c3ee (log/c3/c3ee_notes.txt), which took the image loaders and decoders of golf_clean.exe to
C3 through the shared file stand-in `re/frida/js/filestandin.js`. Renamed rows live in their existing files; the four
globals are in `re/names/exe_12_globals.tsv`.

## Renames
- `Image_recreateSurface` 0x00474e70 (was Surface_paint): it paints nothing. It (re)creates the object's surface at
  (width, height, depth), through the slot +0x10 sub-object's slot +0xc when that returns non-NULL, else through
  Surface_createFull 0x00474dd0, with `g_surfaceRecreating` 0x00839ab0 set to 1 for the duration (0x00474e73, cleared at
  0x00474ea5 / 0x00474ed1).
- `decodePcx8` 0x00495100 (was decodeImagePaletted) and `decodePcx16` 0x00495420 (was decodeImage16): the 8-bit and
  16-bit RLE PCX decoders (header byte 0 = 0x0a at 0x0049519f, byte 2 = 1 at 0x004951a7). decodePcx16 reads each colour
  from the +4 table of a local Cache482fd0 that its constructor leaves NULL (0x00482ffe), so any 16-bit image with pixels
  faults at 0x0049566c; loadImageData 0x00478cd0 takes that path only when the object's slot +0x10 is non-NULL and
  g_blitError 0x0083c310 is not 8 (0x00478d8c), which the exe's Image class (slot +0x10 = 0x00479820, returns 0) never is.
- `decodeChunk7WordDelta` 0x004827d0 (was decodeSprite4827d0): the chunk-type-7 handler of decodeImageChunks 0x00482570
  (jump table 0x004826cc slot 3), a word-oriented delta (control words 0xc000 at 0x00482825, 0x8000 at 0x00482856).
- Purpose corrections, names kept: reloadBmp 0x00475050 converts an 8-bit BMP through its colour table into a 16-bit
  buffer (5-6-5 when display slot +0xb4 returns 1, else 5-5-5); loadImage 0x00475840 maps a file with MappedFile (the
  extension at 0x004e4234 appended when the name has no dot) and never reads BMP files.

## Globals
`g_imageAssetExtMode` 0x0083fe5c picks openImageAsset's extension (0x00479016). `g_listPalette` 0x00839a98 is the palette
cache loadImage takes from the list 0x0083ac88 (0x00475987). `g_loadImage` 0x00839348 is the Image that Image_draw
0x00473e60 and Surface_drawScaled 0x00473cb0 load into when the surface carries a file name (this+8).
