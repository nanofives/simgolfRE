# Contributing

## Important Note

While we're thrilled that there is so much interest in reverse engineering SimGolf and are happy to accept contributions from anyone who would like to help progress us further to our goal of a complete codebase, proposed changes to this repository must adhere to a certain degree of engineering quality. While the established contributors here are more than happy to provide code reviews and constructive criticism, it is not their job to teach potential contributors C++ or decompilation fundamentals. As a project that is largely an artifact of the free time of its contributors, the more of that (often scarce) resource that can be dedicated to efficient work, the faster the decompilation will progress. Unfortunately, this results in well-intentioned but poorly constructed contributions actually hurting progress in the long-term. While we are greatly appreciative of the sentiment, if you aren't very confident in your decompilation abilities, it is generally in the project's best interest that you return when you have a better grasp over the process.

Generally, decompilation is a fairly advanced skill. Depending on your current proficiency with C/C++ and x86 assembly, it could take you months or even years to learn the skills necessary to do it adequately. Once again, please make yourself familiar with this process before attempting to contribute code to this project.

## General Guidelines

If you feel fit to contribute, feel free to create a pull request! Someone will review and merge it (or provide feedback) as soon as possible.

Please keep your pull requests small and understandable; you may be able to shoot ahead and make a lot of progress in a short amount of time, but this is a collaborative project, so you must allow others to catch up and follow along. Large pull requests become significantly more unwieldy to review, and as such make it exponentially more likely for a mistake or error to go undetected. They also make it harder to merge other pull requests because the more files you modify, the more likely it is for a merge conflict to occur. A general guideline is to keep submissions limited to one class at a time. Sometimes two or more classes may be too interlinked for this to be feasible, so this is not a hard rule, however if your PR is starting to modify more than 10 or so files, it's probably getting too big.

This repository currently has only one goal: accuracy to the original executables. We are byte/instruction matching as much as possible, which means the priority is making the original compiler (Visual C++ 6.0, and its Service Pack 5 for sound.dll) produce code that matches the original game. As such, modernizations and bug fixes will probably be rejected for the time being.

## Overview

* [`golf`](/golf): Decompilation of `golf.exe`, with the studio's engine library `jackal.lib`, its build of the IJG JPEG library `jpeg.lib`, and `binkw32.lib`, a stand-in for RAD's Bink DLL that is linked only for its import library.
* [`jgl`](/jgl): Decompilation of `jgld.dll`, the 2D graphics library (most files `jgl*`), with the `libpng.lib` and `zlib.lib` it links.
* [`sound`](/sound): Decompilation of `sound.dll`, the sound system, with the `lpc10.lib` it links and the DirectX 8.0 SDK (`DXSDK`) it and golf.exe compile against.
* [`terrain`](/terrain): Decompilation of `Terrain.dll`, the 3D terrain library.
* [`toolchain`](/toolchain): Visual C++ 6.0, without a service pack (`vc6`), with Service Pack 5 (`vc6sp5`) and with Service Pack 2 (`vc6sp2`).
* `tools`: Where a checkout of smsg, the project's own decompilation tooling, goes. smsg is not published.

Code obtained from third parties, not including Firaxis, is checked out as git submodules: the JPEG library, libpng, zlib and LPC-10 inside the projects that build them, the DirectX 8.0 SDK in `sound`, and the toolchains in `toolchain`. Clone with `git clone --recurse-submodules`. As these are unaltered files, our style guide (see below) does not apply; never add, edit or format a file inside a submodule.

## Tooling

The tooling this project was built with (smsg) is not published. [The Build Lock](#the-build-lock) says how to check a build without it.

## Notes on MSVC 6.0

As outlined in the [`README`](/README.md), Microsoft Visual C++ 6.0 is the compiler we use to build the game: golf.exe, Terrain.dll and jgld.dll without a service pack, sound.dll with Service Pack 5.

One important aspect of this compiler is that it keeps hidden bookkeeping state.
A seemingly unrelated declaration, file path, or line position can therefore
change the machine code produced for a function. smsg's verification build
records and checks the required compiler conditions (the three toolchains and
the original teams' directories), allowing the project to reproduce the
reference build from the source as it is reviewed.

This still matters when contributing: a harmless-looking edit can move other
compiled output. Build with `EXACT=1` and check the outputs against
[the lock](#the-build-lock) before you commit.

## The Build Lock

[`smsg/lock.json`](/smsg/lock.json) records the build this tree has reached: each binary's md5 as the project's tooling builds it, a digest of every object file, the submodule commits, and under `pins.records` the sha256 of `build.bat`, `smsg.toml` and the other files under `smsg/`. Objects are compared by their code (each section's contents and relocations, in any order), so renaming or reformatting changes no entry. The tooling's check (`smsg lock --check`, not published) fails on any difference. The tooling writes the lock; a commit that changes it ends its message with a `Lock-Update: <reason>` trailer.

To verify a build from `build.bat` with `EXACT=1` against the lock without smsg: for each entry of `images`, set the byte ranges `raw_build.zeroed` lists (`[offset, length]` pairs) to zero in a copy of the file at `artifact`; its md5 is `raw_build.md5`.

## Code Style

In general, we're not exhaustively strict about coding style, but there are some preferable guidelines to follow that have been adopted from what we know about the original codebase:

### Formatting

We are using [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) with configuration files that aim to replicate the code formatting employed by the original teams: the root `.clang-format` holds what the four binaries share, and each binary's directory adds its own. There are [integrations](https://clang.llvm.org/docs/ClangFormat.html#vim-integration) available for most editors and IDEs. The required version is `23.1.3`, exactly.

`build.bat` and every smsg build check the formatting before they compile anything. A build from the IDE (F7) is not checked: run `build.bat check` before you commit. `build.bat format` fixes what the check reports, bar the files it names, where formatting can move a compiled byte (below): fix those by hand, then check the build against [the lock](#the-build-lock).

Some lines reach the compiled bytes: `__LINE__`, assertions and debug reports, and the line structure golf's engine headers order their inline functions by. `.clang-format-ignore` names the files that are never formatted (the submodules and `tools/`), and [`smsg/format-optouts.txt`](/smsg/format-optouts.txt) records each with its reason. Some functions the compiler emits on its own (destructors and template members) take their place in golf and jgld from the intermediate code ahead of them, and nine pads hold those windows: in golf, `BasePopPad`, `BasePopPadA` and `BasePopPadB` (the `ImageButton`, `Dialogs` and `BasePop` destructors), `WidgetObserversPad` (`AutoSound`'s), `GlobalsUnrecovered` in `Globals.cpp` (`Screen`'s), `NetworkUnrecovered` in `Network.h` (the order of `window.cpp`'s generated functions; it also holds golf's C1 label count, so it is resized only within that count) and `SoundUnrecoveredPad` (what every golf file that includes `sound.h` compiles after it, `AutoSound`'s destructor and `Plot.cpp`'s `ClassList` members among them); in jgld, `ModeArrayPad` (`GraphSys`'s) and `SurfacePad` (`GSBuffer`'s). An edit that moves one is fixed by resizing its pad inside the window, never by a new pad.

### Comments

The source holds no comments in any file outside the submodules, and the project's tooling refuses one. A line a program requires stays, recorded with the proof that its program refuses the file without it: the two header lines of each `.dsp` and the shebang of `docker/entrypoint.sh`. The addresses of the functions in the original binaries are kept in the project's tooling, which is not published.

### Naming conventions

Names follow the evidence: exported names, strings the code prints, the debug information of the Debug libraries and the symbols of JACKAL's sibling builds (the Mac ports of Alpha Centauri and Civilization III) name what they name; every other name is chosen from what the code does, in the style of the team that wrote the binary. A class has one declaration, in the binary that defines it.
