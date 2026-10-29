# SimGolf File Format Documentation

This folder contains documentation for SimGolf's custom binary file formats using [Kaitai Struct](https://kaitai.io/), a declarative language for describing binary data structures.

## What is Kaitai Struct?

Kaitai Struct allows you to define binary formats in a YAML-based `.ksy` file, which can then be:
- Compiled into parser libraries for [many programming languages](https://kaitai.io/#quick-start) (C++, Python, JavaScript, etc.)
- Visualized interactively using the [Kaitai Struct Visualizer](https://github.com/kaitai-io/kaitai_struct_visualizer)
- Dumped to human-readable formats using `ksdump`

## Documented Formats

| File | Extension | Description |
|------|-----------|-------------|
| [`savegame.ksy`](savegame.ksy) | `.sve` | Saved game (the course, its golfers, the club's staff, money and history) |
| [`course.ksy`](course.ksy) | `.cse` | Course saved for championship play (the course, its record and its golfer types) |
| [`golfer.ksy`](golfer.ksy) | `.pro`, `.chr`, `.glf` | Golfer profile (looks, lines, ratings and optional face picture) |
| [`highscores.ksy`](highscores.ksy) | `.sve` | High score table (`top10.sve`) |
| [`spritebank.ksy`](spritebank.ksy) | `.flc` | Sprite bank (an FLC animation holding several sequences) |

## Using the Tools

### Installation

See the [Kaitai Struct Visualizer installation instructions](https://github.com/kaitai-io/kaitai_struct_visualizer?tab=readme-ov-file#downloading-and-installing) for setup details.

### Kaitai Struct Visualizer (ksv)

The [Kaitai Struct Visualizer](https://github.com/kaitai-io/kaitai_struct_visualizer) (`ksv`) provides an interactive terminal UI for exploring binary files.
Run these examples from the `docs` directory:

```bash
cd docs

# View a saved game
ksv "/path/to/simgolf/saved games/<name>.sve" savegame.ksy

# View a championship course
ksv /path/to/simgolf/Themes/Championship/<course>.cse course.ksy

# View a golfer profile
ksv /path/to/simgolf/Themes/Default.pro golfer.ksy

# View the high score table
ksv /path/to/simgolf/top10.sve highscores.ksy

# View a sprite bank
ksv /path/to/simgolf/flics/<directory>/<name>.flc spritebank.ksy
```

### Kaitai Struct Dump (ksdump)

`ksdump` outputs the parsed structure as JSON or YAML for scripting and inspection.
The examples below also assume that your terminal is in the `docs` directory.

```bash
# Dump a saved game to JSON
ksdump --format json "/path/to/simgolf/saved games/<name>.sve" savegame.ksy

# Dump a championship course to YAML
ksdump --format yaml /path/to/simgolf/Themes/Championship/<course>.cse course.ksy

# Dump a golfer profile to JSON
ksdump --format json /path/to/simgolf/Themes/Default.pro golfer.ksy

# Dump the high score table to YAML
ksdump --format yaml /path/to/simgolf/top10.sve highscores.ksy

# Dump a sprite bank to JSON
ksdump --format json /path/to/simgolf/flics/<directory>/<name>.flc spritebank.ksy
```

## Sample Files

Note: every file above can be found in your SimGolf installation, in the directory named beside it.

The golfer schema also accepts shipped files with a stock head and no embedded
portrait, such as `Themes\Standard\Joe Pro.glf` and
`Themes\The Sims\Gwendoline.chr`. These end after the ratings at 0x722 bytes.
