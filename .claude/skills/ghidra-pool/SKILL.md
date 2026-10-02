---
name: ghidra-pool
description: Manage the Ghidra project pool for SimGolf RE — acquire/release/create slots on demand. Use this skill any time you need to open a Ghidra project for read or write, especially when multiple Claude sessions might run in parallel. Triggers when the user mentions Ghidra, decompilation, RE work, or asks to "open the project", "look at function X", "decompile Y".
---

# Ghidra Pool — SimGolf

Ghidra locks projects with a `.lock` file the moment a session opens them. With multiple Claude sessions or with the headless MCP server running, you need separate clones of the master project. This skill manages that pool **on demand** — slots are created the first time they're needed, not pre-allocated, to save disk (each clone is a full copy of the analyzed `.rep`).

## When to use

- Before any `mcp__ghidra__project_program_open_existing` call.
- When `LockException` appears in MCP output.
- When the user says "open Ghidra", "look at function …", "decompile …", "investigate the binary", or runs `/re`.
- When the user wants to free a slot, refresh slots after edits, or check pool state.

## Architecture (one-time read)

```
C:\Users\maria\Desktop\Proyectos\SimGolf\
├── SimGolf.gpr / SimGolf.rep              # MASTER (GUI-edited; symbols, types, comments live here)
├── SimGolf_headless.gpr / SimGolf_headless.rep   # OPTIONAL second project for headless writes
└── simgolf_pool\                         # CLONES (read-only sessions consume these)
    ├── SimGolf_pool0.gpr / .rep / .lock
    ├── SimGolf_pool1.gpr / .rep / .lock
    └── ...                              # up to MAX_SLOTS=16, created lazily
```

The Ghidra binary lives in TD5RE (`C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC`) — **shared, do not duplicate**. Ghidra MCP is also shared from TD5RE (`ghidra\headless-mcp\`).

## Standard acquire/release flow

```bash
# Step 1: acquire (creates slot 0 first call, reuses unlocked slots after)
SLOT=$(bash "C:/Users/maria/Desktop/Proyectos/SimGolf/scripts/ghidra_pool.sh" acquire)
echo "Got slot: $SLOT"

# Step 2: open the slot through MCP — ALWAYS read_only=true unless writing
mcp__ghidra__project_program_open_existing
  project_location="C:/Users/maria/Desktop/Proyectos/SimGolf/simgolf_pool"
  project_name="$SLOT"
  program_name="golf_clean.exe"
  read_only=true

# Step 3: do work (decompile, look up functions, etc.)

# Step 4: close + release
mcp__ghidra__program_close
bash "C:/Users/maria/Desktop/Proyectos/SimGolf/scripts/ghidra_pool.sh" release ${SLOT#SimGolf_pool}
```

## Commands

| Command | What it does |
|---|---|
| `init` | Verifies master exists; first-run setup |
| `acquire` | Prints `SimGolf_poolN`. Reuses unlocked slot if any; creates new slot otherwise. Hits `MAX_SLOTS=16` limit. |
| `release [N]` | Clears lock files for slot N, or all slots if N omitted |
| `sync` | Refreshes every unlocked slot from master `.rep` (run this after a GUI editing session) |
| `status` | Prints pool state |
| `cleanup` | Wipes all lock files everywhere (use after crashed sessions) |
| `add [N]` | Pre-creates N new slots immediately |
| `remove <N>` | Deletes slot N from disk (must be unlocked) |

## Decision tree for the agent

1. Is there a `SimGolf.gpr` master yet? Check `C:\Users\maria\Desktop\Proyectos\SimGolf\SimGolf.gpr`.
   - **No** → rebuild it with the import recipe in `CLAUDE.md` ("Ghidra project"): headless import of `golf_clean.exe`, `jgld.dll`, `Terrain.dll`, `sound.dll` (~4 min).
   - **Yes** → continue.
2. Run `acquire`. If it errors with "all slots locked":
   1. Run `cleanup`.
   2. Run `acquire` again.
3. Open the slot read-only via MCP.
4. After work: `program_close` + `release N`.

## Writing back to master

Slots are read-only by convention. To persist symbol/type changes:

- Option A: open `SimGolf.gpr` (the master) directly with `read_only=false` from a single session — never run two writers at once.
- Option B: open `SimGolf_headless.gpr` (a second master, dedicated to MCP writes) and merge to `SimGolf.gpr` later via Ghidra's project sync. Recommended only once the workflow stabilizes.

After any master write, run `sync` to push changes to all unlocked pool slots.

## Pre-flight asserts (slot-side)

Hallucinated session control is the dominant failure mode when multiple sessions are live: citing an RVA that doesn't exist in the open program, writing into a slot that's actually read-only, or quietly working from a stale clone after another session synced the master. Before any analysis or citation, run these four guards. The first three are pure file checks (covered by `scripts/ghidra_assert.sh`); the fourth is an MCP call you must make yourself.

```bash
# 1+2+3: file-level guards — slot binding, staleness vs last sync, scribe flag
bash scripts/ghidra_assert.sh preflight "$SLOT"
```

| # | Assert | How | Fails when |
|---|--------|-----|------------|
| 1 | **Slot match** — the program you're about to open belongs to your slot | `ghidra_assert.sh slot-match $SLOT` | `.pool_slot` is missing or names a different slot — you grabbed someone else's binding |
| 2 | **Staleness** — your slot was refreshed after the last master `sync` | `ghidra_assert.sh staleness $SLOT` | `.last_master_sync` is newer than `simgolf_pool/$SLOT.rep` — release + re-acquire |
| 3 | **Scribe** — no other session is mid-master-edit | `ghidra_assert.sh scribe-check` | A `master.WIP-*` flag is in repo root — wait or coordinate before opening master |
| 4 | **Open-program identity** — the MCP-opened program is actually your slot | `mcp__ghidra__program_list_open` then assert path contains `$SLOT` | Path mismatch — you may be operating on the master or a sibling slot |

After the slot is open and before citing any RVA in conversation or in source comments, run a fifth assert per citation:

- **RVA existence** — call `mcp__ghidra__function_at <rva>` or `mcp__ghidra__listing_code_unit_at <rva>`. If the result is null, **halt** and do not cite the address. Do not "round" or "infer" a nearby valid address.

These five together close the path between "I think I'm looking at function X at 0x004a4bb7" and "the open program literally has function X at 0x004a4bb7." Anything skipping them is hallucination-prone.

## MCP availability is required — no documentation fallback

Ghidra MCP is the **only** acceptable source of facts about golf_clean.exe. If the MCP server is unreachable, returns errors, or any required tool call fails, **halt the session and report the failure to the user**. Do not:

- Substitute modding-community notes, wikis, or any other prior-art document for what would have been a Ghidra call.
- Substitute `re/prior_art/` source code for what would have been a Ghidra call.
- Substitute web search, your own training data, or "what this function probably does given its name" for what would have been a Ghidra call.
- Cite an RVA, signature, constant, or call relationship without first verifying it through MCP in the current session.

Prior art and documentation are useful for **framing questions** (what subsystem is this, what Jackal / Alpha Centauri API might this resemble) but never for **answering** them about this specific binary. The binary is the source of truth; MCP is the only access path to it.

Before doing any work, confirm MCP is alive:

```text
mcp__ghidra__health_ping        # must succeed
mcp__ghidra__ghidra_info        # must return the open project info
```

If either fails: stop, surface the error verbatim, and ask the user to fix the MCP connection before continuing. Do not proceed in degraded mode.

## Write-tool gating

Slot programs are read-only by convention; the file system doesn't enforce it. The following MCP tools mutate the open program and **must not be called against a slot** — only against the master, and only by the session holding the `master.WIP-<sessionid>` flag (one writer at a time):

- `decomp_global_rename`, `decomp_global_retype`, `decomp_writeback_locals`, `decomp_writeback_params`, `decomp_override_set`
- `function_create`, `function_delete`, `function_rename`, `function_signature_set`, `function_return_type_set`, `function_calling_convention_set`, `function_flags_set`, `function_thunk_set`, `function_body_set`, `function_batch_run`
- `symbol_create`, `symbol_delete`, `symbol_rename`, `symbol_namespace_move`, `symbol_primary_set`
- `layout_struct_*`, `layout_union_*`, `layout_enum_*` (any *_create / *_add / *_remove / *_rename / *_replace / *_resize / *_clear / *_fill_from_decompiler)
- `type_apply_at`, `type_define_c`, `type_delete`, `type_rename`, `type_parse_c`, `type_category_create`
- `parameter_add`, `parameter_remove`, `parameter_replace`, `parameter_move`
- `stackframe_variable_create`, `stackframe_variable_clear`, `variable_local_create`, `variable_local_remove`, `variable_rename`, `variable_retype`, `variable_comment_set`
- `comment_set`, `bookmark_add`, `bookmark_remove`, `bookmark_clear`, `equate_create`, `equate_delete`, `equate_clear_range`
- `listing_data_create`, `listing_data_clear`, `listing_clear`, `listing_disassemble_*`
- `memory_block_create`, `memory_block_remove`, `memory_write`, `program_image_base_set`, `program_save`, `program_save_as`
- `patch_*`, `reference_create_*`, `reference_clear_*`, `reference_delete`, `reference_association_*`, `reference_primary_set`, `relocation_add`
- `transaction_begin`, `transaction_commit`, `transaction_revert`, `transaction_undo`, `transaction_redo`
- `bookmark_*`, `tag_add`, `tag_remove`, `metadata_store`
- `analysis_update`, `analysis_update_and_wait`, `analysis_options_set`, `analysis_analyzers_set`, `analysis_clear_cache`
- `external_*` (any *_create / *_add / *_remove / *_set_path)
- `source_file_add`, `source_file_remove`, `source_map_add`, `source_map_remove`
- `class_create`, `namespace_create`
- `ghidra_call`, `ghidra_eval`, `ghidra_script` (these can run arbitrary writes)

Read-only tools you can call freely against a slot: `function_at`, `function_by_name`, `function_callers`, `function_callees`, `function_list`, `function_signature_get`, `function_variables`, `function_report`, `decomp_function`, `decomp_ast`, `decomp_tokens`, `decomp_high_function_summary`, `decomp_trace_type_*`, `decomp_override_get`, `pcode_*`, `listing_code_unit_*` (the *_at / *_after / *_before / *_containing / *_list variants), `listing_data_at`, `listing_data_list`, `memory_read`, `memory_blocks_list`, `search_*`, `symbol_by_name`, `symbol_list`, `reference_from`, `reference_to`, `comment_get*`, `comment_list`, `equate_list`, `bookmark_list`, `type_get`, `type_get_by_id`, `type_list`, `type_archives_list`, `type_source_archives_list`, `type_category_list`, `layout_struct_get`, `layout_inspect_components`, `function_calling_conventions_list`, `external_*_list`, `external_location_get`, `relocation_list`, `source_file_list`, `source_map_list`, `stackframe_variables`, `program_list_open`, `program_summary`, `program_report`, `program_mode_get`, `tag_list`, `tag_stats`, `metadata_query`, `analysis_status`, `analysis_options_get`, `analysis_options_list`, `analysis_analyzers_list`, `health_ping`, `ghidra_info`, `mcp_response_format`, `task_status`, `task_result`, `transaction_status`, `graph_*`.

If you are unsure whether a tool mutates state, treat it as a write tool and refuse until you hold the master flag.

## Headless fallback

If the Ghidra MCP is not loaded in this session (it is wired in `.mcp.json`; a new session picks it up), `py -3.12 re\tools\decomp.py 0x<addr> [--program jgld.dll]` decompiles through a pool slot with Ghidra headless. That is still Ghidra on the anchored binary, so it is an acceptable source of facts; documentation is not.

## NO-GUESSING coupling

This skill is the entry point to deterministic RE work. Whenever you load a slot, the conversation should follow the project's NO-GUESSING rule:

- Report only what decompilation literally shows; cite the address for every constant.
- Never invent meaning for unclear values — report raw hex/decimal and mark `[UNCERTAIN]`.
- Never use words like "probably", "likely", "seems to", "appears to", "I think".

See `CLAUDE.md` for the full rule.
