meta:
  id: golfer
  title: Golfer Profile
  application: Sid Meier's SimGolf
  file-extension: pro
  license: CC0-1.0
  endian: le
doc: |
  A golfer type in a .pro, .chr or .glf file. The customisation screen writes
  `Themes\<theme>\<name>.chr` for a character and .pro for a pro
  (`Themes\Championship\` for championship play, `Themes\Default.pro` for the
  player's own golfer). The shipped .glf files use the same layout. Written
  by `SaveGolferToTheme` and read by `LoadGolferFromTheme` (SetupScreens.cpp).

  File structure:
  1. The golfer type record
  2. The type's text: 25 lines of 50 characters
  3. The golfer's four rating words
  4. Optionally, the tag `*PCXFILE` and a PCX picture holding the type's three
     head cells, 140 x 140 each, one under the other

  Shipped files can end after the ratings, at offset 0x722 (1826 bytes),
  without the tag or picture. Examples include Standard\Joe Pro.glf and
  The Sims\Gwendoline.chr. The writer always appends a portrait, but the
  reader does not require one for a stock head.

seq:
  - id: type
    type: golfer_type
  - id: text
    type: strz
    size: 50
    encoding: windows-1252
    repeat: expr
    repeat-expr: 25
    doc: The lines the type's golfers say.
  - id: ratings
    type: u4
    repeat: expr
    repeat-expr: 4
    doc: |
      Copied into the golfer's ratings (and, for type 0, into the player's own
      skill points) unless the game loads the type without a golfer.
  - id: picture_tag
    contents: '*PCXFILE'
    if: not _io.eof
  - id: picture
    size-eos: true
    if: not _io.eof
    doc: |
      A PCX file, 140 x 420, 8 bits per pixel. The three cells are cut from it
      only when the type's head is not one of the 20 stock heads.

types:
  golfer_type:
    doc: One `GolferType` record (Golfer.h), 0x230 bytes.
    seq:
      - id: title
        type: strz
        size: 0x10
        encoding: windows-1252
        doc: Shown in parentheses under the name ("Golf Pro" for a pro).
      - id: name
        type: strz
        size: 0x10
        encoding: windows-1252
      - id: caps
        type: s1
        doc: Five trait bits; pairing counts how many two golfers' types share.
      - id: age_bits
        type: s1
        doc: The low byte of a new golfer's age flags; bit 7 gives the sex.
      - id: head
        type: s1
        doc: The type's face, one of its sex's 72 heads; 20 and above is a face of its own.
      - id: appearance
        type: s1
        doc: High nibble the body type (and voice), low nibble the fifth palette pick.
      - id: shirt_color
        type: s1
      - id: pants_color
        type: s1
      - id: extra_tone
        type: s1
      - id: skin_tone
        type: s1
      - id: hair_color
        type: s1
      - id: unused
        size: 3
      - id: fixed_kind
        type: s4
        doc: Nonzero, the low three bits give every golfer of this type its kind.
      - id: bio
        type: strz
        size: 0x200
        encoding: windows-1252
        doc: The type's biography.
