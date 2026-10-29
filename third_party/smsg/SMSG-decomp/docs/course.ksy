meta:
  id: course
  title: Course File
  application: Sid Meier's SimGolf
  file-extension: cse
  license: CC0-1.0
  endian: le
  imports:
    - golfer
doc: |
  A course saved for championship play, `Themes\Championship\<course>.cse`.
  Written by `SaveCourseToFile` and read by `LoadCourseFromFile`
  (SpriteAssets.cpp), both through `TransferCourseState`, which moves the
  course's tables in one fixed order with no field between them. The course
  list reads only the two lines.

  File structure:
  1. Two 100-byte lines: the course and its designer, the course record
  2. The course state
  3. The tag `*PCXFILE`

seq:
  - id: title
    type: strz
    size: 100
    encoding: windows-1252
    doc: '"<course name> designed by <designer>"'
  - id: record
    type: strz
    size: 100
    encoding: windows-1252
    doc: '"Course Record: <score> by <holder>"'
  - id: state
    type: course_state
  - id: picture_tag
    contents: '*PCXFILE'

types:
  course_state:
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
      - id: best_scores
        size: 0x28
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
      - id: clock
        type: s4
        doc: The game clock; 1024 ticks a month.
      - id: max_hole_index
        type: s4
      - id: rule_param
        type: s4
      - id: level
        type: s4
      - id: pro_match_stakes
        type: s4
      - id: next_golfer
        type: s4
      - id: spawn_counts
        size: 0x80
      - id: seen_actions_low
        type: u4
      - id: unlocked_landmarks
        type: u4
      - id: hole_stats_done
        type: s4
      - id: current_golfer_index
        type: s4
      - id: available_landmarks
        type: u4
      - id: vip_ladder_level
        type: s4
      - id: landmark_count
        type: s4
      - id: landing_spots
        size: 0x1c8
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
      - id: dev_map
        size: 0x9c4
      - id: game_options
        type: u4
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
      - id: club_staff
        size: 0x1300
      - id: courses
        size: 0x30e
