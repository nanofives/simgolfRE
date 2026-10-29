meta:
  id: savegame
  title: Saved Game
  application: Sid Meier's SimGolf
  file-extension: sve
  license: CC0-1.0
  endian: le
  imports:
    - golfer
doc: |
  A saved game, `saved games\<name>.sve` (the autosaves too, and
  `While_Browsing.sve`). Written by `SaveGame` and read by `LoadSavedGame`
  (SpriteAssets.cpp), both through `TransferGameState`, which moves the
  game's tables in one fixed order with no field between them: every block
  below is the whole table the game keeps in memory. A file that
  `LoadSavedGame` reads from `Themes\Championship\` has a second 100-byte line
  after the first.

  File structure:
  1. One 100-byte line naming the save in the game's file list
  2. The game state

seq:
  - id: description
    type: strz
    size: 100
    encoding: windows-1252
    doc: '"-<course name>, <day> <date>", as the file list shows it.'
  - id: state
    type: game_state

types:
  game_state:
    seq:
      - id: course_id
        type: s4
      - id: next_hole
        type: s4
      - id: course_yards
        type: s4
      - id: course_par
        type: s4
      - id: theme
        type: u1
        doc: The number of the course's theme.
      - id: fun_rating
        type: s4
      - id: skill_rating
        type: s4
      - id: length_skill
        type: s4
      - id: accuracy_skill
        type: s4
      - id: imagination
        type: s4
      - id: course_score
        type: s4
      - id: building_count
        type: s4
      - id: win_count
        type: s4
      - id: golfer_story_count
        type: s4
      - id: money
        type: s4
        doc: Club cash in units of $100; multiply by 100 for dollars.
      - id: tour_index
        type: s4
      - id: holes
        size: 0x28a0
        doc: 20 hole records of 520 bytes.
      - id: terrain_cover
        size: 0x9c4
        doc: 50 x 50 tiles, one byte each.
      - id: terrain_variation
        size: 0x9c4
        doc: 50 x 50 tiles, one byte each.
      - id: tile_extra
        size: 0x1388
        doc: 50 x 50 tiles, two bytes each.
      - id: terrain_walk
        size: 0x9c4
        doc: 50 x 50 tiles, one byte each.
      - id: vertex_elevation
        size: 0xa29
        doc: 51 x 51 tile corners, one byte each.
      - id: bilerp_grid
        size: 0x169
        doc: 19 x 19 bytes.
      - id: vip_slots
        size: 0x1000
      - id: new_game_mark
        type: s4
      - id: last_grade
        type: s4
      - id: golfers
        size: 0x9c00
      - id: club_staff
        size: 0x1300
      - id: clock
        type: s4
        doc: The game clock; 1024 ticks a month.
      - id: max_hole_index
        type: s4
      - id: rule_param
        type: s4
      - id: level
        type: s4
      - id: skills_line
        size: 0x3e8
      - id: money_line
        size: 0x3e8
      - id: fun_history
        size: 0x3e8
      - id: green_fee
        type: s4
      - id: pro_match_stakes
        type: s4
      - id: next_golfer
        type: s4
      - id: spawn_counts
        size: 0x80
      - id: seen_actions_low
        type: u4
      - id: events
        size: 0x3e8
      - id: unlocked_landmarks
        type: u4
      - id: hole_stats_done
        type: s4
      - id: current_golfer_index
        type: s4
      - id: yearly_stats
        size: 0x7d0
      - id: available_landmarks
        type: u4
      - id: pending_pro_index
        type: s4
      - id: vip_ladder_level
        type: s4
      - id: landmark_count
        type: s4
      - id: stats_rows
        size: 0x1700
      - id: course_records
        size: 0xf0
      - id: landing_spots
        size: 0x1c8
      - id: last_chat_tick
        type: s4
      - id: litter_particles
        size: 0x2400
      - id: ball_trail
        size: 0x3880
      - id: golfer_record_per_type
        size: 0xe70
      - id: followed_golfer
        type: s4
      - id: skill_alloc
        size: 0x20
      - id: animals
        size: 0x1770
      - id: misc_game_flags
        size: 0x12
      - id: placed
        size: 0x320
      - id: wildlife_generators
        size: 0x40
      - id: wild_animals
        size: 0xa00
      - id: golfer_types
        type: golfer::golfer_type
        repeat: expr
        repeat-expr: 76
      - id: golfer_type_text
        size: 0x19a28
        doc: 84 rows of 25 lines of 50 characters.
      - id: tournament_prize
        type: s4
      - id: good_mood_hits
        size: 0x9c4
      - id: complaint_count
        size: 0x9c4
      - id: landing_count
        size: 0x9c4
      - id: dev_map
        size: 0x9c4
      - id: game_options
        type: u4
      - id: accomplishments
        size: 0x420
      - id: courses
        size: 0x30e
      - id: member_num_history
        size: 0x3e8
      - id: theme_name
        type: strz
        size: 0x64
        encoding: windows-1252
        doc: The directory under `Themes\` the course's golfer types come from.
      - id: seen_actions_high
        type: u4
      - id: text_pool
        size: 0x1002
      - id: text_pool_offset
        size: 0x100
      - id: text_pool_length
        size: 0x100
      - id: seq_counter
        type: s4
      - id: warn_stage
        type: s4
      - id: best_scores
        size: 0x28
      - id: tournament_changes
        type: s4
      - id: terrain_revert_map
        size: 0x9c4
      - id: refund_map
        size: 0x9c4
      - id: preference_flags
        type: u4
      - id: extra_golfer_types
        type: golfer::golfer_type
        repeat: expr
        repeat-expr: 4
        if: (game_options & 0x40000000) != 0
        doc: Golfer types 76 to 79, present when bit 30 of game_options is set.
      - id: types_swap
        size: 0xb7c0
        if: (game_options & 0x200000) != 0
      - id: types_backup
        size: 0x19a28
        if: (game_options & 0x200000) != 0
