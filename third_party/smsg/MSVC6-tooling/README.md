# MSVC6-tooling

Research tools for understanding Visual C++ 6.0 and investigating the compiler and linker decisions that matter when reconstructing a game's original machine code. The tools grew out of [SMSG-decomp](https://github.com/SMSG-decomp/SMSG-decomp) and the companion [MSVC6-decomp](https://github.com/SMSG-decomp/MSVC6-decomp) compiler research.

MSVC6-decomp is a non-building decompilation for reading compiler internals. This repository is a separate Rust workspace containing analysis tools, models, and experiments. Its models describe particular observed behaviors; they are not a complete implementation of VC6 or proof that a game rebuild is byte-exact.

## What is here

| Area | Where to start |
| --- | --- |
| Compiler experiments with expected output | [probes](probes/): small C/C++ programs paired with `.expect` files. |
| Front-end intermediate language and inlining | [msvc6-il](crates/msvc6-il/src/) |
| Compiler behavior models | [msvc6-model](crates/msvc6-model/src/): frame ordering, emitted data, translation-unit effects, and related investigations. |
| LINK behavior | [msvc6-link](crates/msvc6-link/src/): object and member ordering, COMDAT folding, layout, imports, and sorting. |
| Class layout and exception handling | [msvc6-class](crates/msvc6-class/src/) |
| PE, COFF objects, and libraries | [msvc6-image](crates/msvc6-image/src/) |
| CodeView and PDB information | [msvc6-codeview](crates/msvc6-codeview/src/) and [msvc6-pdb](crates/msvc6-pdb/src/) |
| Command-line interface | [msvc6t](crates/msvc6t/src/) |
| Project configuration example | [examples/simgolf/msvc6t.toml](examples/simgolf/msvc6t.toml) |

## Start with a probe

A small experiment is often easier to reason about than a whole game function. For example, compare [the default C calling-convention probe](probes/c1-gd-decorates-cdecl-names.c) and [its expected output](probes/c1-gd-decorates-cdecl-names.expect) with [the `/Gz` probe](probes/c1-gz-decorates-stdcall-names.c) and [its expectations](probes/c1-gz-decorates-stdcall-names.expect).

The `.expect` file states the rule being investigated, the compiler flags, and the output to check. Follow the same approach for a mismatch in a game: reduce it to a probe, inspect the original compiler's output, and use the decompiled compiler routines to help explain it. Check the exact compiler version and settings before generalizing a result.

## Building the tools

The workspace pins Rust **1.98.0**, including `rustfmt` and Clippy, in [rust-toolchain.toml](rust-toolchain.toml). With Rust installed through rustup, build the CLI from this directory:

```sh
cargo build --locked -p msvc6t
./target/debug/msvc6t --help
./target/debug/msvc6t probe --help
./target/debug/msvc6t probe --check probes
./target/debug/msvc6t pdb --help
```

`probe --check` validates probe sources and expectations without running a compiler. Build scripts enforce the workspace's source rules and formatting. Compiler-running commands also check lint provenance. Their diagnostics explain refusals; see `msvc6t lint --help` for the checks.

Commands that run VC6 need your own compiler installation and a configured native, Wine, or container runner. The repository contains no Microsoft compiler binaries or game binaries. The SimGolf example references external compiler trees, game files, and optional research inputs through environment variables; it needs those paths configured for your installation.

File-inspection commands do not need to run VC6. Use `msvc6t image --help`, `msvc6t obj --help`, `msvc6t lib --help`, or `msvc6t pdb --help` to choose an operation. Detailed command help is built into the CLI.

## Research status

Models, recovered structures, and names can be incomplete or wrong. A passing probe establishes its stated check on the toolchain that ran it. Confirm conclusions against the original compiler and linker, their disassembly, and controlled experiments before relying on them for a game reconstruction.

## Credits and license

Visual C++ 6.0 is the work of Microsoft Corporation and its compiler team. [SMSG-decomp](https://github.com/SMSG-decomp/SMSG-decomp) and [MSVC6-decomp](https://github.com/SMSG-decomp/MSVC6-decomp) supplied the research context for these tools.

- [isledecomp/reccmp](https://github.com/isledecomp/reccmp): comparison formats and research into VC6 LINK's sorting behavior.
- [homm3-decomp](https://github.com/sushi-shi/homm3-decomp): prior research into C2.
- [microsoft/microsoft-pdb](https://github.com/microsoft/microsoft-pdb): CodeView record names; see [THIRD-PARTY-NOTICES](THIRD-PARTY-NOTICES).
- [msvc-demangler](https://github.com/mstange/msvc-demangler-rust): decoding decorated names.

See [LICENSE](LICENSE), [COPYING](COPYING), and [THIRD-PARTY-NOTICES](THIRD-PARTY-NOTICES) for license terms and attribution.
