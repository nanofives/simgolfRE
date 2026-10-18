/* Frame-head hash of the phase 2.2 checkpoint tracer (re/frida/js/scenario_det.js, cfg.frameHash): compiled by Frida's
 * CModule (TinyCC) inside the game. fh_frame(T) is called natively from the timeGetTime stub when the call's return
 * address is mainLoop's frame head; it appends one record [T, frame index, sgh64 of every listed object] to fh_out.
 * sgh64 is re/tools/state_image.py's object hash: the bytes as little-endian dwords (the last zero-padded),
 * s = sum (w_i ^ 0x5bd1e995) * ((2i + 1) * 0x9e3779b97f4a7c15) mod 2^64, then s ^ byte length and the splitmix64
 * finaliser. Control words fh_ctl: [0] objects, [1] record capacity, [2] records held, [3] records dropped (buffer
 * full), [4] frame heads seen. fh_objs: (va, size) pairs. The tables are allocated by the agent (a CModule's own data
 * is read-only in this process). */
typedef unsigned int u32;
typedef unsigned long long u64;

extern u32 fh_ctl[];
extern u32 fh_objs[];
extern u32 fh_out[];

static u64 fh_mix(u64 h)
{
  h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
  h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
  return h ^ (h >> 31);
}

u64 fh_sgh64(const unsigned char *p, u32 n)
{
  u64 s = 0;
  u32 nw = n >> 2;
  u32 i;
  const u32 *w = (const u32 *) p;
  for (i = 0; i < nw; i++)
    s += (u64) (w[i] ^ 0x5BD1E995u) * ((u64) (2u * i + 1u) * 0x9E3779B97F4A7C15ULL);
  if (n & 3) {
    u32 last = 0, k;
    for (k = 0; k < (n & 3); k++)
      last |= (u32) p[nw * 4 + k] << (8 * k);
    s += (u64) (last ^ 0x5BD1E995u) * ((u64) (2u * nw + 1u) * 0x9E3779B97F4A7C15ULL);
  }
  return fh_mix(s ^ (u64) n);
}

void fh_frame(u32 t)
{
  u32 n = fh_ctl[0];
  u32 rw = 2 + 2 * n;
  u32 c = fh_ctl[2];
  u32 i;
  u32 *r;
  fh_ctl[4]++;
  if (c >= fh_ctl[1]) {
    fh_ctl[3]++;
    return;
  }
  r = fh_out + c * rw;
  r[0] = t;
  r[1] = fh_ctl[4] - 1;
  for (i = 0; i < n; i++) {
    u64 h = fh_sgh64((const unsigned char *) fh_objs[2 * i], fh_objs[2 * i + 1]);
    r[2 + 2 * i] = (u32) h;
    r[3 + 2 * i] = (u32) (h >> 32);
  }
  fh_ctl[2] = c + 1;
}
