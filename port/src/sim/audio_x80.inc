// The software x87 path (step S4.4; included by audio_core.cpp inside namespace portsim::x87 since S4.2b). No third-party code.
//
// A finite non-zero value is held unpacked as (sign, e, m): m a 64-bit significand with bit 63 set and the value
// m * 2^(e - 16383 - 63), e an int (an extended denormal operand is normalised below 1). An exact intermediate result is a
// 128-bit significand with bit 127 set, value sig * 2^(e - 16383 - 127), plus a sticky bit for anything below bit 0;
// round_pack rounds it ONCE to the precision control's 24 / 53 / 64 bits by the rounding control, as the x87 rounds each
// arithmetic result. Loads (fld / fild) are exact; stores (fst qword, fist) round by the rounding control only.
// Special operands follow the Intel SDM vol. 1 tables 4-7 (NaN propagation: an SNaN is returned quieted, a QNaN source
// wins over an SNaN, of two NaNs of one kind the one with the larger significand, the positive one on a tie) and
// 8-10..8-13 (inf - inf, 0 * inf, 0 / 0, inf / inf give the indefinite; x / 0 gives an infinity with the divide-by-zero
// masked). Unnormals and pseudo-NaN / pseudo-infinity encodings (invalid operands on the 387 and later) give the indefinite.

namespace {

struct U128 {
  uint64_t hi = 0, lo = 0;
};

int clz64(uint64_t x) {
  int n = 0;
  if (x == 0) return 64;
  while (!(x & (1ull << 63))) {
    x <<= 1;
    ++n;
  }
  return n;
}

U128 mul64(uint64_t a, uint64_t b) {
  const uint64_t a0 = a & 0xffffffffu, a1 = a >> 32, b0 = b & 0xffffffffu, b1 = b >> 32;
  const uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  const uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
  U128 r;
  r.lo = (mid << 32) | (p00 & 0xffffffffu);
  r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
  return r;
}

// s >> n (n >= 0) with the bits shifted out ORed into sticky
U128 shr128(U128 s, int n, bool &sticky) {
  if (n <= 0) return s;
  if (n >= 128) {
    sticky = sticky || s.hi || s.lo;
    return U128{};
  }
  U128 r;
  if (n >= 64) {
    const int k = n - 64;
    sticky = sticky || s.lo || (k ? (s.hi & ((1ull << k) - 1)) : 0);
    r.lo = k ? (s.hi >> k) : s.hi;
    r.hi = 0;
    return r;
  }
  sticky = sticky || (s.lo & ((1ull << n) - 1));
  r.lo = (s.lo >> n) | (s.hi << (64 - n));
  r.hi = s.hi >> n;
  return r;
}

U128 shl128(U128 s, int n) {
  if (n <= 0) return s;
  if (n >= 128) return U128{};
  U128 r;
  if (n >= 64) {
    r.hi = s.lo << (n - 64);
    r.lo = 0;
    return r;
  }
  r.hi = (s.hi << n) | (s.lo >> (64 - n));
  r.lo = s.lo << n;
  return r;
}

int clz128(U128 s) { return s.hi ? clz64(s.hi) : 64 + clz64(s.lo); }

bool lt128(U128 a, U128 b) { return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo); }

U128 sub128(U128 a, U128 b) {
  U128 r;
  r.lo = a.lo - b.lo;
  r.hi = a.hi - b.hi - (a.lo < b.lo ? 1u : 0u);
  return r;
}

enum Cls { kZero, kFin, kInf, kQnan, kSnan, kBad };

struct Up {
  Cls cls = kZero;
  bool sign = false;
  int32_t e = 0;
  uint64_t m = 0;  // finite: normalised; NaN: the raw significand
};

Up unpack(X80 x) {
  Up u;
  u.sign = (x.se >> 15) != 0;
  const int ex = x.se & 0x7fff;
  u.m = x.m;
  if (ex == 0x7fff) {
    if (!(x.m >> 63)) u.cls = kBad;                       // pseudo-infinity / pseudo-NaN
    else if ((x.m << 1) == 0) u.cls = kInf;
    else u.cls = (x.m & (1ull << 62)) ? kQnan : kSnan;
    return u;
  }
  if (ex == 0) {
    if (x.m == 0) {
      u.cls = kZero;
      return u;
    }
    const int c = clz64(x.m);                             // denormal / pseudo-denormal: weight of bit 63 is 2^(1-16383)
    u.cls = kFin;
    u.m = x.m << c;
    u.e = 1 - c;
    return u;
  }
  if (!(x.m >> 63)) {
    u.cls = kBad;                                          // unnormal
    return u;
  }
  u.cls = kFin;
  u.e = ex;
  return u;
}

X80 make(bool sign, int ex, uint64_t m) {
  X80 r;
  r.m = m;
  r.se = static_cast<uint16_t>((sign ? 0x8000u : 0u) | (static_cast<unsigned>(ex) & 0x7fffu));
  return r;
}
X80 zero(bool sign) { return make(sign, 0, 0); }
X80 inf(bool sign) { return make(sign, 0x7fff, 1ull << 63); }
X80 quiet(X80 x) {
  x.m |= 1ull << 62;
  return x;
}

int prec_bits(uint16_t cw) {
  switch ((cw >> 8) & 3) {
    case 0: return 24;
    case 2: return 53;
    default: return 64;  // 11 = 64 bits; 01 (reserved) treated as 64, as the emulator does
  }
}
int rc_of(uint16_t cw) { return (cw >> 10) & 3; }  // 0 nearest, 1 down, 2 up, 3 chop

bool round_up(int rc, bool sign, bool lsb, bool rbit, bool rest) {
  switch (rc) {
    case 0: return rbit && (rest || lsb);
    case 1: return (rbit || rest) && sign;
    case 2: return (rbit || rest) && !sign;
    default: return false;
  }
}

// sig has bit 127 set (or is zero with sticky clear); value sig * 2^(e - 16383 - 127)
X80 round_pack(bool sign, int32_t e, U128 sig, bool sticky, int p, int rc) {
  uint64_t q;
  bool rbit, rest;
  if (p == 64) {
    q = sig.hi;
    rbit = (sig.lo >> 63) != 0;
    rest = (sig.lo << 1) != 0 || sticky;
  } else {
    const int drop = 64 - p;
    q = sig.hi >> drop;
    rbit = ((sig.hi >> (drop - 1)) & 1) != 0;
    rest = (sig.hi & ((1ull << (drop - 1)) - 1)) != 0 || sig.lo != 0 || sticky;
  }
  if (round_up(rc, sign, (q & 1) != 0, rbit, rest)) {
    ++q;
    if (p == 64 ? q == 0 : q == (1ull << p)) {
      q = 1ull << (p - 1);
      ++e;
    }
  }
  if (e < 1 || e > 0x7ffe)
    throw X87Unmodelled{"x87: a result outside the extended exponent range (e " + std::to_string(e) + ")"};
  return make(sign, e, p == 64 ? q : q << (64 - p));
}

X80 norm_round(bool sign, int32_t e, U128 sig, bool sticky, uint16_t cw) {
  const int c = clz128(sig);
  sig = shl128(sig, c);
  return round_pack(sign, e - c, sig, sticky, prec_bits(cw), rc_of(cw));
}

// NaN propagation (at least one operand a NaN)
X80 pick_nan(const Up &a, X80 xa, const Up &b, X80 xb) {
  const bool an = a.cls == kQnan || a.cls == kSnan, bn = b.cls == kQnan || b.cls == kSnan;
  if (an && bn) {
    if (a.cls == kQnan && b.cls == kSnan) return quiet(xa);
    if (a.cls == kSnan && b.cls == kQnan) return quiet(xb);
    const uint64_t ma = xa.m & ~(1ull << 62), mb = xb.m & ~(1ull << 62);
    if (ma != mb) return quiet(mb > ma ? xb : xa);
    // equal significands: the positive one (measured on the Win32 instructions and in Unicorn: -QNaN / +QNaN gives +QNaN)
    return quiet(a.sign && !b.sign ? xb : xa);
  }
  return quiet(an ? xa : xb);
}

bool any_nan(const Up &a, const Up &b) { return a.cls == kQnan || a.cls == kSnan || b.cls == kQnan || b.cls == kSnan; }

X80 addsub(X80 xa, X80 xb, bool neg_b, uint16_t cw) {
  Up a = unpack(xa), b = unpack(xb);
  if (any_nan(a, b)) return pick_nan(a, xa, b, xb);
  if (a.cls == kBad || b.cls == kBad) return indefinite();
  if (neg_b) b.sign = !b.sign;
  const int rc = rc_of(cw);
  if (a.cls == kInf || b.cls == kInf) {
    if (a.cls == kInf && b.cls == kInf) return a.sign == b.sign ? inf(a.sign) : indefinite();
    return inf(a.cls == kInf ? a.sign : b.sign);
  }
  if (a.cls == kZero && b.cls == kZero) return zero(a.sign == b.sign ? a.sign : rc == 1);
  if (a.cls == kZero) return norm_round(b.sign, b.e, U128{b.m, 0}, false, cw);
  if (b.cls == kZero) return norm_round(a.sign, a.e, U128{a.m, 0}, false, cw);
  if (b.e > a.e || (b.e == a.e && b.m > a.m)) std::swap(a, b);
  bool sticky = false;
  const U128 A{a.m, 0};
  const U128 B = shr128(U128{b.m, 0}, a.e - b.e, sticky);
  if (a.sign == b.sign) {
    U128 s;
    s.lo = A.lo + B.lo;
    s.hi = A.hi + B.hi + (s.lo < A.lo ? 1u : 0u);
    int32_t e = a.e;
    if (s.hi < A.hi) {  // carry out of bit 127
      if (s.lo & 1) sticky = true;
      s.lo = (s.lo >> 1) | (s.hi << 63);
      s.hi = (s.hi >> 1) | (1ull << 63);
      ++e;
    }
    return round_pack(a.sign, e, s, sticky, prec_bits(cw), rc);
  }
  U128 s = sub128(A, B);
  if (sticky) s = sub128(s, U128{0, 1});  // the true subtrahend lies between B and B + 1 unit
  if (s.hi == 0 && s.lo == 0 && !sticky) return zero(rc == 1);
  return norm_round(a.sign, a.e, s, sticky, cw);
}

}  // namespace

namespace sw {

X80 add(X80 a, X80 b, uint16_t cw) { return addsub(a, b, false, cw); }
X80 sub(X80 a, X80 b, uint16_t cw) { return addsub(a, b, true, cw); }

X80 mul(X80 xa, X80 xb, uint16_t cw) {
  const Up a = unpack(xa), b = unpack(xb);
  if (any_nan(a, b)) return pick_nan(a, xa, b, xb);
  if (a.cls == kBad || b.cls == kBad) return indefinite();
  const bool sign = a.sign != b.sign;
  if ((a.cls == kInf && b.cls == kZero) || (a.cls == kZero && b.cls == kInf)) return indefinite();
  if (a.cls == kInf || b.cls == kInf) return inf(sign);
  if (a.cls == kZero || b.cls == kZero) return zero(sign);
  const U128 p = mul64(a.m, b.m);
  return norm_round(sign, a.e + b.e - 16383 + 1, p, false, cw);
}

X80 div(X80 xa, X80 xb, uint16_t cw) {
  const Up a = unpack(xa), b = unpack(xb);
  if (any_nan(a, b)) return pick_nan(a, xa, b, xb);
  if (a.cls == kBad || b.cls == kBad) return indefinite();
  const bool sign = a.sign != b.sign;
  if ((a.cls == kInf && b.cls == kInf) || (a.cls == kZero && b.cls == kZero)) return indefinite();
  if (a.cls == kInf) return inf(sign);
  if (b.cls == kInf) return zero(sign);
  if (b.cls == kZero) return inf(sign);
  if (a.cls == kZero) return zero(sign);
  // Q = floor(ma * 2^127 / mb): ma / mb lies in (1/2, 2), so Q has its leading bit at 127 or 126
  U128 q;
  uint64_t r = a.m;
  if (r >= b.m) {
    q.hi = 1ull << 63;
    r -= b.m;
  }
  for (int i = 126; i >= 0; --i) {
    const bool carry = (r >> 63) != 0;
    r <<= 1;
    if (carry || r >= b.m) {
      r -= b.m;
      if (i >= 64) q.hi |= 1ull << (i - 64);
      else q.lo |= 1ull << i;
    }
  }
  return norm_round(sign, a.e - b.e + 16383, q, r != 0, cw);
}

uint64_t fst64(X80 x, uint16_t cw) {
  const Up a = unpack(x);
  const uint64_t s = a.sign ? (1ull << 63) : 0;
  switch (a.cls) {
    case kZero: return s;
    case kInf: return s | (0x7ffull << 52);
    case kQnan:
    case kSnan: return s | (0x7ffull << 52) | (1ull << 51) | ((x.m >> 11) & ((1ull << 52) - 1));
    case kBad: return 0xfff8000000000000ull;
    case kFin: break;
  }
  const int rc = rc_of(cw);
  int32_t ue = a.e - 16383;  // value = m * 2^(ue - 63)
  if (ue >= -1022) {
    // normal: 53 significant bits
    uint64_t q = a.m >> 11;
    const bool rbit = ((a.m >> 10) & 1) != 0, rest = (a.m & 0x3ff) != 0;
    if (round_up(rc, a.sign, (q & 1) != 0, rbit, rest)) {
      ++q;
      if (q == (1ull << 53)) {
        q >>= 1;
        ++ue;
      }
    }
    if (ue > 1023) {
      const bool to_inf = rc == 0 || (rc == 2 && !a.sign) || (rc == 1 && a.sign);
      return s | (to_inf ? (0x7ffull << 52) : 0x7fefffffffffffffull);
    }
    return s | (static_cast<uint64_t>(ue + 1023) << 52) | (q & ((1ull << 52) - 1));
  }
  // denormal: N = round(m * 2^(ue - 63 + 1074)); the shift is at least 12
  const int sh = 63 - 1074 - ue;
  uint64_t n;
  bool rbit, rest;
  if (sh >= 66) {
    n = 0;
    rbit = false;
    rest = true;
  } else if (sh >= 64) {
    n = 0;
    rbit = sh == 64 ? (a.m >> 63) != 0 : false;
    rest = sh == 64 ? (a.m << 1) != 0 : true;
  } else {
    n = a.m >> sh;
    rbit = ((a.m >> (sh - 1)) & 1) != 0;
    rest = sh >= 2 ? (a.m & ((1ull << (sh - 1)) - 1)) != 0 : false;
  }
  if (round_up(rc, a.sign, (n & 1) != 0, rbit, rest)) ++n;  // n == 2^52 is the smallest normal, encoded alike
  return s | n;
}

int64_t fist(X80 x, int bits, uint16_t cw) {
  const int64_t indef = bits == 64 ? INT64_MIN : (bits == 32 ? INT32_MIN : INT16_MIN);
  const Up a = unpack(x);
  if (a.cls == kZero) return 0;
  if (a.cls != kFin) return indef;
  const int32_t sh = a.e - 16383 - 63;  // value = m * 2^sh
  uint64_t n;
  bool rbit = false, rest = false;
  if (sh > 0) return indef;
  if (sh == 0) {
    n = a.m;
  } else {
    const int s = -sh;
    if (s > 64) {
      n = 0;
      rest = true;
    } else if (s == 64) {
      n = 0;
      rbit = (a.m >> 63) != 0;
      rest = (a.m << 1) != 0;
    } else {
      n = a.m >> s;
      rbit = ((a.m >> (s - 1)) & 1) != 0;
      rest = s >= 2 ? (a.m & ((1ull << (s - 1)) - 1)) != 0 : false;
    }
  }
  if (round_up(rc_of(cw), a.sign, (n & 1) != 0, rbit, rest)) {
    if (n == UINT64_MAX) return indef;
    ++n;
  }
  const uint64_t lim = bits == 64 ? (1ull << 63) : (bits == 32 ? (1ull << 31) : (1ull << 15));
  if (a.sign ? n > lim : n >= lim) return indef;
  return a.sign ? static_cast<int64_t>(0 - n) : static_cast<int64_t>(n);
}

uint16_t fcom(X80 xa, X80 xb) {
  const Up a = unpack(xa), b = unpack(xb);
  if (any_nan(a, b) || a.cls == kBad || b.cls == kBad) return kC3 | kC2 | kC0;
  auto key = [](const Up &u) -> int {  // -1 / 0 / +1 by sign for zero handling
    return u.cls == kZero ? 0 : (u.sign ? -1 : 1);
  };
  const int ka = key(a), kb = key(b);
  if (ka != kb || ka == 0) {
    if (ka == kb) return kC3;
    return ka < kb ? kC0 : 0;
  }
  // same sign, both non-zero: compare magnitudes (infinity largest)
  int c;
  if (a.cls == kInf || b.cls == kInf) c = (a.cls == kInf) - (b.cls == kInf);
  else if (a.e != b.e) c = a.e < b.e ? -1 : 1;
  else c = a.m < b.m ? -1 : (a.m > b.m ? 1 : 0);
  if (ka < 0) c = -c;
  return c == 0 ? kC3 : (c < 0 ? kC0 : 0);
}

}  // namespace sw

X80 from_f64(uint64_t b) {
  const bool sign = (b >> 63) != 0;
  const int ex = static_cast<int>((b >> 52) & 0x7ff);
  const uint64_t frac = b & ((1ull << 52) - 1);
  if (ex == 0x7ff) return frac == 0 ? inf(sign) : make(sign, 0x7fff, (1ull << 63) | (frac << 11));
  if (ex == 0) {
    if (frac == 0) return zero(sign);
    const int c = clz64(frac);
    return make(sign, 16383 + 63 - 1074 - c, frac << c);
  }
  return make(sign, ex - 1023 + 16383, (1ull << 63) | (frac << 11));
}

X80 fld64(uint64_t b) {
  X80 r = from_f64(b);
  if ((r.se & 0x7fff) == 0x7fff && (r.m << 1) != 0) r = quiet(r);
  return r;
}

X80 fild(int64_t v) {
  if (v == 0) return zero(false);
  const bool sign = v < 0;
  const uint64_t mag = sign ? 0 - static_cast<uint64_t>(v) : static_cast<uint64_t>(v);
  const int c = clz64(mag);
  return make(sign, 16383 + 63 - c, mag << c);
}

X80 fabs_(X80 a) {
  a.se &= 0x7fff;
  return a;
}
X80 fchs(X80 a) {
  a.se ^= 0x8000;
  return a;
}
X80 indefinite() { return make(true, 0x7fff, 0xc000000000000000ull); }
