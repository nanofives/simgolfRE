meta:
  id: spritebank
  title: Sprite Bank
  application: Sid Meier's SimGolf
  file-extension: flc
  license: CC0-1.0
  endian: le
doc: |
  An animation bank under `flics\`: an FLC animation whose header's reserved
  tail holds the game's sequence table, so one file holds several sequences
  of the same number of frames (a golfer's swing in each direction, say).
  Read by `FlicAnim::load` and `FlicAnim::confirm_data` (jackal.lib,
  FlicAnim.cpp). A plain FLC, whose `creator` is not the game's marker, is
  taken as one sequence of all its frames.

  File structure:
  1. The 128-byte header
  2. Chunks from `oframe1` to the end of the file: for each sequence, its
     frames in order, each a frame chunk holding the chunks that draw it

seq:
  - id: size
    type: u4
  - id: magic
    contents: [0x12, 0xaf]
  - id: frames
    type: u2
  - id: width
    type: u2
  - id: height
    type: u2
  - id: depth
    type: u2
    doc: Must be 8.
  - id: flags
    type: u2
  - id: speed
    type: u4
  - id: reserved1
    type: u2
  - id: created
    type: u4
  - id: creator
    type: u4
    doc: 0xf1f1f2f2, the game's marker, when the fields after oframe2 are set.
  - id: updated
    type: u4
  - id: updater
    type: u4
  - id: aspect_x
    type: u2
  - id: aspect_y
    type: u2
  - id: ext_flags
    type: u2
  - id: key_frames
    type: u2
  - id: total_frames
    type: u2
  - id: req_memory
    type: u4
  - id: max_regions
    type: u2
  - id: transp_num
    type: u2
  - id: reserved2
    size: 24
  - id: oframe1
    type: u4
    doc: Offset of the first frame chunk.
  - id: oframe2
    type: u4
  - id: unknown_58
    type: u4
  - id: unknown_5c
    type: u4
  - id: sequences
    type: u2
    doc: A sequence number is taken modulo this when there is no sequence mask.
  - id: frames_per_seq
    type: u2
  - id: origin_x
    type: u2
    doc: Where the frame's canvas sits, left.
  - id: origin_y
    type: u2
    doc: Where the frame's canvas sits, top.
  - id: frame_width
    type: u2
  - id: frame_height
    type: u2
  - id: unknown_6c
    type: u4
  - id: seq_mask
    type: u4
    doc: One bit per sequence present, by sequence number; 0 means all.
  - id: reserved3
    size: 12

instances:
  chunks:
    pos: oframe1
    type: chunk
    repeat: eos

types:
  chunk:
    seq:
      - id: size
        type: u4
        doc: To the next chunk.
      - id: type
        type: u2
        enum: chunk_type
      - id: body
        size: size - 6
        type:
          switch-on: type
          cases:
            'chunk_type::frame': frame
  frame:
    seq:
      - id: count
        type: u2
      - id: reserved
        size: 8
      - id: chunks
        type: chunk
        repeat: expr
        repeat-expr: count

enums:
  chunk_type:
    4: color_256
    7: delta_flc
    11: color_64
    12: delta_fli
    13: black
    15: byte_run
    16: copy
    0xf1fa: frame
