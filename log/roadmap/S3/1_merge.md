# S3.1 merge request (terrain draw-list passes)

Everything below was proven in the S3.1 build: `log/roadmap/S3/1_mirror.py` copies port/ of the S3 mirror (port/build/S3_head =
git archive of c6d7d78b) to port/build/S3_1_src/port, puts this step's own files in, applies exactly the edits of section 1 and
builds into port/build/S3_head/port/build/S3_1 (x64) and S3_1_w32 (Win32). The gates ran in the S3 mirror with SG_PORTSIM_DLL
pointing at that build (log/roadmap/S3/1_notes.md). Without section 1 the new entry kinds return SG_E_NO_FUNCTION and
port/test/test_terrain_render.py's draw-list tests fail with `sg_gl_render code -?` (Terrain::render keeps working).

## 1. port/src/portsim_c.cpp (C ABI routing, shared file)

The text below is `OLD_RENDER` / `NEW_RENDER` and `OLD_RASTER` / `NEW_RASTER` of log/roadmap/S3/1_mirror.py, which applies them
with an exact-match check (the file has CRLF line ends).

a. In `sg_gl_render`, replace

```cpp
  portsim::gl::GlStreamSink sink;
  try {
    switch (entry) {
      case 2: {
        float angle;
        std::memcpy(&angle, &a1, 4);
        portsim::terrain_render(ctx->ctx, sink, ctx->gl_globals, terrain, a0, angle);
        break;
      }
      default: return SG_E_NO_FUNCTION;
    }
  }
```

by

```cpp
  portsim::gl::GlStreamSink sink;
  try {
    if (!portsim::terrain_render_entry(ctx->ctx, sink, ctx->gl_globals, entry, terrain, a0, a1, a2)) return SG_E_NO_FUNCTION;
  }
```

b. In `sg_gl_render_to_raster`, replace

```cpp
  try {
    switch (entry) {
      case 2: {
        float angle;
        std::memcpy(&angle, &a1, 4);
        // the vertex and normal arrays the faces index are the renderer's globals; the rasteriser reads them for glArrayElement
        const portsim::TerrainGlobals &g = ctx->gl_globals;
        rast->r.set_arrays(g.vertices.data(), g.vertices.size() / 3, g.normals.data(), g.normals.size() / 3);
        portsim::terrain_render(ctx->ctx, rast->r, ctx->gl_globals, terrain, a0, angle);
        break;
      }
      default: return SG_E_NO_FUNCTION;
    }
  }
```

by

```cpp
  try {
    // the vertex and normal arrays the faces index are the renderer's globals; the rasteriser reads them for glArrayElement
    const portsim::TerrainGlobals &g = ctx->gl_globals;
    rast->r.set_arrays(g.vertices.data(), g.vertices.size() / 3, g.normals.data(), g.normals.size() / 3);
    if (!portsim::terrain_render_entry(ctx->ctx, rast->r, ctx->gl_globals, entry, terrain, a0, a1, a2)) return SG_E_NO_FUNCTION;
  }
```

The `(void)a2;` lines at the top of both functions may stay (a2 is now used) or go.

## 2. port/include/portsim_c.h (documentation of sg_gl_render)

Replace

```
 * and write the GL calls it makes as text (portsim/sim/gl_sink.h): entry 2 = Terrain::render(a0 = tile address,
 * a1 = angle bits). out gets the text
```

by

```
 * and write the GL calls it makes as text (portsim/sim/gl_sink.h): entry 1 = stripRender(a0 = pivot, a1 = direction,
 * a2 = angle bits), 2 = Terrain::render(a0 = tile address, a1 = angle bits), 3 = localRender(a0 = tile, a1 = pivot,
 * a2 = angle bits), 4 = pathUpdateRender(a0 = pivot, a1 = angle bits); SG_E_NO_FUNCTION otherwise. out gets the text
```

No signature changes, no new export; the Win32 runner needs nothing (it has no sg_gl ops).

## 3. UNCERTAINTIES.md

Append the five rows of log/roadmap/S3/1_uncertain.tsv (U-1300 .. U-1304) in the file's table form.

## 4. re/PHASE5_TERRAIN_PLAN.md (suggested section 19, the lead's choice)

```
19. DONE, the draw-list passes (roadmap S3.1, 2026-10-24; log/roadmap/S3/1_notes.md): localRender, stripRender and pathUpdateRender
    with drawTile's sorted draw list and drawTileObjects' wall recursion in terrain_render.cpp (terrain_render_entry for the four
    entry kinds). Evidence: 237 draw-list passes of the original equal line for line (33 localRender, 160 stripRender, 44
    pathUpdateRender: c3 .. c7 plus the new recordings g2 .. g6 of re/tools/gl_record.py --scenario, injected on the game thread
    through an Interceptor.replace of render / localRender, every hooked run SAME as its pinned reference), and every pass rasterised
    from the port's stream meets G2 except 16 of g6's constructed type-7-heavy state, where the original's own stream misses it
    identically (U-1300). Controls 9 .. 14 RED; 12 only on g6 (U-1302).
```

## 5. Local data (gitignored, not part of a commit)

- port/corpus/gl/c3 .. c6: the kind-3 snapshots of log/p5/gl/corpus/c3 .. c6 (`port.test.glcorpus export <src> <label> 3`).
- port/corpus/gl/g2 .. g6: `port.test.glcorpus export log/p5/gl/corpus/g<k> g<k> 1,3,4`.
- log/p5/gl/corpus/g1 .. g6 (raw recordings; g1 is the failed first attempt, kept for its organic passes), instances\s3_1 (pinned
  winmm.dll 381ed2b2 copied from instances\p4q).
