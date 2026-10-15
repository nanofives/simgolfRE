// FLAGS sound.dll: /Od /ZI /GZ
// LANG c
// m36c D2: sound.dll codec (0x1003e680..0x10041a00), hand-matched from Ghidra's decompilation and the disassembly.
// The code has the shape of f2c output of the LPC-10 2400 bps codec (lpc10 1.5 style, state structs); the routine
// names below are that shape's names, used as a spelling guide: no source copy was compared. Compiled as C: in C++
// mode VC6 /Od routes a forward goto and an early return through jmp stubs the original does not have (invert_).
#include <windows.h>
#include <math.h>
typedef long integer; typedef float real; typedef short shortint; typedef long logical; typedef double doublereal;
#define f2c_abs(x) ((x) >= 0 ? (x) : -(x))
#define f2c_min(a,b) ((a) <= (b) ? (a) : (b))
#define f2c_max(a,b) ((a) >= (b) ? (a) : (b))
#define TRUE_ (1)
#define FALSE_ (0)

/* the 1.0f that onset_ and vparms_ pass to the sign routine 0x1003e380 (the binary has one copy per routine:
   0x10064e3c and 0x10064e40; data addresses are masked by match.py) */
static real c_b2 = 1.f;

// MATCH: sound.dll 0x1003e6d0 _invert_
int invert_(integer *order, real *phi, real *psi, real *rc)
{
    integer phi_dim1, phi_offset, i__1, i__2, i__3;
    real r__1, r__2;

    real save;
    integer i__, j, k;
    real v[100];

    --rc;
    --psi;
    phi_dim1 = *order;
    phi_offset = phi_dim1 + 1;
    phi -= phi_offset;

    i__1 = *order;
    for (j = 1; j <= i__1; ++j) {
	i__2 = *order;
	for (i__ = j; i__ <= i__2; ++i__) {
	    v[i__ + j * 10 - 11] = phi[i__ + j * phi_dim1];
	}
	i__2 = j - 1;
	for (k = 1; k <= i__2; ++k) {
	    save = v[j + k * 10 - 11] * v[k + k * 10 - 11];
	    i__3 = *order;
	    for (i__ = j; i__ <= i__3; ++i__) {
		v[i__ + j * 10 - 11] -= v[i__ + k * 10 - 11] * save;
	    }
	}
	if ((r__1 = v[j + j * 10 - 11], f2c_abs(r__1)) < 1e-10f) {
	    goto L100;
	}
	rc[j] = psi[j];
	i__2 = j - 1;
	for (k = 1; k <= i__2; ++k) {
	    rc[j] -= rc[k] * v[j + k * 10 - 11];
	}
	v[j + j * 10 - 11] = 1.f / v[j + j * 10 - 11];
	rc[j] *= v[j + j * 10 - 11];
	r__2 = rc[j];
	r__1 = f2c_min(r__2,.999f);
	rc[j] = f2c_max(r__1,-.999f);
    }
    return 0;
L100:
    i__1 = *order;
    for (i__ = j; i__ <= i__1; ++i__) {
	rc[i__] = 0.f;
    }
    return 0;
}

// MATCH: sound.dll 0x1003e9f0 _mload_
int mload_(integer *order, integer *awins, integer *awinf,
	real *speech, real *phi, real *psi)
{
    integer phi_dim1, phi_offset, i__1, i__2;

    integer c__, i__, r__, start;

    --psi;
    phi_dim1 = *order;
    phi_offset = phi_dim1 + 1;
    phi -= phi_offset;
    --speech;

    start = *awins + *order;
    i__1 = *order;
    for (r__ = 1; r__ <= i__1; ++r__) {
	phi[r__ + phi_dim1] = 0.f;
	i__2 = *awinf;
	for (i__ = start; i__ <= i__2; ++i__) {
	    phi[r__ + phi_dim1] += speech[i__ - 1] * speech[i__ - r__];
	}
    }
    psi[*order] = 0.f;
    i__1 = *awinf;
    for (i__ = start; i__ <= i__1; ++i__) {
	psi[*order] += speech[i__] * speech[i__ - *order];
    }
    i__1 = *order;
    for (r__ = 2; r__ <= i__1; ++r__) {
	i__2 = r__;
	for (c__ = 2; c__ <= i__2; ++c__) {
	    phi[r__ + c__ * phi_dim1] = phi[r__ - 1 + (c__ - 1) * phi_dim1] -
		    speech[*awinf + 1 - r__] * speech[*awinf + 1 - c__] +
		    speech[start - r__] * speech[start - c__];
	}
    }
    i__1 = *order - 1;
    for (c__ = 1; c__ <= i__1; ++c__) {
	psi[c__] = phi[c__ + 1 + phi_dim1] - speech[start - 1] * speech[start
		- 1 - c__] + speech[*awinf] * speech[*awinf - c__];
    }
    return 0;
}

// MATCH: sound.dll 0x1003ec60 _energy_
int energy_(integer *len, real *speech, real *rms)
{
    integer i__1;
    integer i__;

    --speech;

    *rms = 0.f;
    i__1 = *len;
    for (i__ = 1; i__ <= i__1; ++i__) {
	*rms += speech[i__] * speech[i__];
    }
    *rms = sqrt(*rms / *len);
    return 0;
}

// MATCH: sound.dll 0x1003ed00 _dcbias_
int dcbias_(integer *len, real *speech, real *sigout)
{
    integer i__1;
    real bias;
    integer i__;

    --sigout;
    --speech;

    bias = 0.f;
    i__1 = *len;
    for (i__ = 1; i__ <= i__1; ++i__) {
	bias += speech[i__];
    }
    bias /= *len;
    i__1 = *len;
    for (i__ = 1; i__ <= i__1; ++i__) {
	sigout[i__] = speech[i__] - bias;
    }
    return 0;
}

// MATCH: sound.dll 0x10040000 _tbdm_
int tbdm_(real *speech, integer *lpita, integer *tau,
	integer *ltau, real *amdf, integer *minptr, integer *maxptr, integer *
	mintau)
{
    integer i__1, i__2, i__3, i__4;

    real amdf2[6];
    integer minp2, ltau2, maxp2, i__;
    extern int difmag_(real *, integer *, integer *, integer
	    *, integer *, real *, integer *, integer *);
    integer minamd, ptr, tau2[6];

    --speech;
    --amdf;
    --tau;

    difmag_(&speech[1], lpita, &tau[1], ltau, &tau[*ltau], &amdf[1], minptr,
	    maxptr);
    *mintau = tau[*minptr];
    minamd = amdf[*minptr];
    ltau2 = 0;
    ptr = *minptr - 2;
    i__1 = *mintau - 3;
    i__3 = *mintau + 3, i__4 = tau[*ltau] - 1;
    i__2 = f2c_min(i__3,i__4);
    for (i__ = f2c_max(i__1,41); i__ <= i__2; ++i__) {
	while(tau[ptr] < i__) {
	    ++ptr;
	}
	if (tau[ptr] != i__) {
	    ++ltau2;
	    tau2[ltau2 - 1] = i__;
	}
    }
    if (ltau2 > 0) {
	difmag_(&speech[1], lpita, tau2, &ltau2, &tau[*ltau], amdf2, &minp2, &
		maxp2);
	if (amdf2[minp2 - 1] < (real) minamd) {
	    *mintau = tau2[minp2 - 1];
	    minamd = amdf2[minp2 - 1];
	}
    }
    if (*mintau >= 80) {
	i__ = *mintau / 2;
	if ((i__ & 1) == 0) {
	    ltau2 = 2;
	    tau2[0] = i__ - 1;
	    tau2[1] = i__ + 1;
	} else {
	    ltau2 = 1;
	    tau2[0] = i__;
	}
	difmag_(&speech[1], lpita, tau2, &ltau2, &tau[*ltau], amdf2, &minp2, &
		maxp2);
	if (amdf2[minp2 - 1] < (real) minamd) {
	    *mintau = tau2[minp2 - 1];
	    minamd = amdf2[minp2 - 1];
	    *minptr += -20;
	}
    }
    amdf[*minptr] = (real) minamd;
    i__2 = *minptr - 5;
    *maxptr = f2c_max(i__2,1);
    i__1 = *minptr + 5;
    i__2 = f2c_min(i__1,*ltau);
    for (i__ = *maxptr + 1; i__ <= i__2; ++i__) {
	if (amdf[i__] > amdf[*maxptr]) {
	    *maxptr = i__;
	}
    }
    return 0;
}

// MATCH: sound.dll 0x10040330 _ivfilt_
int ivfilt_(real *lpbuf, real *ivbuf, integer *len, integer *
	nsamp, real *ivrc)
{
    integer i__1;

    integer i__, j, k;
    real r__[3], pc1, pc2;

    --ivbuf;
    --lpbuf;
    --ivrc;

    for (i__ = 1; i__ <= 3; ++i__) {
	r__[i__ - 1] = 0.f;
	k = i__ - 1 << 2;
	i__1 = *len;
	for (j = (i__ << 2) + *len - *nsamp; j <= i__1; j += 2) {
	    r__[i__ - 1] += lpbuf[j] * lpbuf[j - k];
	}
    }
    pc1 = 0.f;
    pc2 = 0.f;
    ivrc[1] = 0.f;
    ivrc[2] = 0.f;
    if (r__[0] > 1e-10f) {
	ivrc[1] = r__[1] / r__[0];
	ivrc[2] = (r__[2] - ivrc[1] * r__[1]) / (r__[0] - ivrc[1] * r__[1]);
	pc1 = ivrc[1] - ivrc[1] * ivrc[2];
	pc2 = ivrc[2];
    }
    i__1 = *len;
    for (i__ = *len + 1 - *nsamp; i__ <= i__1; ++i__) {
	ivbuf[i__] = lpbuf[i__] - pc1 * lpbuf[i__ - 4] - pc2 * lpbuf[i__ - 8];
    }
    return 0;
}

// MATCH: sound.dll 0x100404d0 _lpfilt_
int lpfilt_(real *inbuf, real *lpbuf, integer *len, integer *nsamp)
{
    integer i__1;

    integer j;
    real t;

    --lpbuf;
    --inbuf;

    i__1 = *len;
    for (j = *len + 1 - *nsamp; j <= i__1; ++j) {
	t = (inbuf[j] + inbuf[j - 30]) * -.0097201988f;
	t += (inbuf[j - 1] + inbuf[j - 29]) * -.0105179986f;
	t += (inbuf[j - 2] + inbuf[j - 28]) * -.0083479648f;
	t += (inbuf[j - 3] + inbuf[j - 27]) * 5.860774e-4f;
	t += (inbuf[j - 4] + inbuf[j - 26]) * .0130892089f;
	t += (inbuf[j - 5] + inbuf[j - 25]) * .0217052232f;
	t += (inbuf[j - 6] + inbuf[j - 24]) * .0184161253f;
	t += (inbuf[j - 7] + inbuf[j - 23]) * 3.39723e-4f;
	t += (inbuf[j - 8] + inbuf[j - 22]) * -.0260797087f;
	t += (inbuf[j - 9] + inbuf[j - 21]) * -.0455563702f;
	t += (inbuf[j - 10] + inbuf[j - 20]) * -.040306855f;
	t += (inbuf[j - 11] + inbuf[j - 19]) * 5.029835e-4f;
	t += (inbuf[j - 12] + inbuf[j - 18]) * .0729262903f;
	t += (inbuf[j - 13] + inbuf[j - 17]) * .1572008878f;
	t += (inbuf[j - 14] + inbuf[j - 16]) * .2247288674f;
	t += inbuf[j - 15] * .250535965f;
	lpbuf[j] = t;
    }
    return 0;
}

// MATCH: sound.dll 0x10040da0 _preemp_
int preemp_(real *inbuf, real *pebuf, integer *nsamp, real *coef, real *z__)
{
    integer i__1;
    real temp;
    integer i__;

    --pebuf;
    --inbuf;

    i__1 = *nsamp;
    for (i__ = 1; i__ <= i__1; ++i__) {
	temp = inbuf[i__] - *coef * *z__;
	*z__ = inbuf[i__];
	pebuf[i__] = temp;
    }
    return 0;
}

/* the high-pass filter's four state words sit at offset 0 of the state block it is given */
struct hp100_state { real z11; real z21; real z12; real z22; };

// MATCH: sound.dll 0x10040e30 _hp100_
int hp100_(real *speech, integer *start, integer *end,
	struct hp100_state *st)
{
    real z11;
    real z21;
    real z12;
    real z22;

    integer i__1;

    integer i__;
    real si, err;

    if (speech) {
	--speech;
	}

    z11 = st->z11;
    z21 = st->z21;
    z12 = st->z12;
    z22 = st->z22;

    i__1 = *end;
    for (i__ = *start; i__ <= i__1; ++i__) {
	si = speech[i__];
	err = si + z11 * 1.859076f - z21 * .8648249f;
	si = err - z11 * 2.f + z21;
	z21 = z11;
	z11 = err;
	err = si + z12 * 1.935715f - z22 * .9417004f;
	si = err - z12 * 2.f + z22;
	z22 = z12;
	z12 = err;
	speech[i__] = si * .902428f;
    }

    st->z11 = z11;
    st->z21 = z21;
    st->z12 = z12;
    st->z22 = z22;

    return 0;
}

/* the noise generator's state: j at +0xbd8, k at +0xbdc, the five 16-bit words y at +0xbe0 */
struct random_state { char pad_0[0xbd8]; integer j; integer k; shortint y[5]; };

// MATCH: sound.dll 0x10040f60 _random_
integer random_(struct random_state *st)
{
    integer *j;
    integer *k;
    shortint *y;
    integer ret_val;

    j = &(st->j);
    k = &(st->k);
    y = &(st->y[0]);

    y[*k - 1] += y[*j - 1];
    ret_val = y[*k - 1];
    --(*k);
    if (*k <= 0) {
	*k = 5;
    }
    --(*j);
    if (*j <= 0) {
	*j = 5;
    }
    return ret_val;
}

// MATCH: sound.dll 0x100414f0 _difmag_
int difmag_(real *speech, integer *lpita, integer *tau,
	integer *ltau, integer *maxlag, real *amdf, integer *minptr, integer *
	maxptr)
{
    integer i__1, i__2;
    real r__1;

    integer i__, j, n1, n2;
    real sum;

    --amdf;
    --tau;
    --speech;

    *minptr = 1;
    *maxptr = 1;
    i__1 = *ltau;
    for (i__ = 1; i__ <= i__1; ++i__) {
	n1 = (*maxlag - tau[i__]) / 2 + 1;
	n2 = n1 + *lpita - 1;
	sum = 0.f;
	i__2 = n2;
	for (j = n1; j <= i__2; j += 4) {
	    sum += (r__1 = speech[j] - speech[j + tau[i__]], f2c_abs(r__1));
	}
	amdf[i__] = sum;
	if (amdf[i__] < amdf[*minptr]) {
	    *minptr = i__;
	}
	if (amdf[i__] > amdf[*maxptr]) {
	    *maxptr = i__;
	}
    }
    return 0;
}

// MATCH: sound.dll 0x1003edc0 _placea_
int placea_(integer *ipitch, integer *voibuf, integer *
	obound, integer *af, integer *vwin, integer *awin, integer *ewin,
	integer *lframe, integer *maxwin)
{
    real r__1;

    integer i_nint(real *);

    logical allv, winv;
    integer i__, j, k, l, hrange;
    logical ephase;
    integer lrange;

    ewin -= 3;
    awin -= 3;
    vwin -= 3;
    --voibuf;

    lrange = (*af - 2) * *lframe + 1;
    hrange = *af * *lframe;
    allv = voibuf[((*af - 2) << 1) + 2] == 1;
    allv = allv && voibuf[((*af - 1) << 1) + 1] == 1;
    allv = allv && voibuf[((*af - 1) << 1) + 2] == 1;
    allv = allv && voibuf[(*af << 1) + 1] == 1;
    allv = allv && voibuf[(*af << 1) + 2] == 1;
    winv = voibuf[(*af << 1) + 1] == 1 || voibuf[(*af << 1) + 2] == 1;
    if (allv || (winv && *obound == 0)) {
	i__ = (lrange + *ipitch - 1 - awin[((*af - 1) << 1) + 1]) / *ipitch;
	i__ *= *ipitch;
	i__ += awin[((*af - 1) << 1) + 1];
	l = *maxwin;
	k = (vwin[(*af << 1) + 1] + vwin[(*af << 1) + 2] + 1 - l) / 2;
	r__1 = (real) (k - i__) / *ipitch;
	awin[(*af << 1) + 1] = i__ + i_nint(&r__1) * *ipitch;
	awin[(*af << 1) + 2] = awin[(*af << 1) + 1] + l - 1;
	if (*obound >= 2 && awin[(*af << 1) + 2] > vwin[(*af << 1) + 2]) {
	    awin[(*af << 1) + 1] -= *ipitch;
	    awin[(*af << 1) + 2] -= *ipitch;
	}
	if ((*obound == 1 || *obound == 3) && awin[(*af << 1) + 1] < vwin[(*
		af << 1) + 1]) {
	    awin[(*af << 1) + 1] += *ipitch;
	    awin[(*af << 1) + 2] += *ipitch;
	}
	while(awin[(*af << 1) + 2] > hrange) {
	    awin[(*af << 1) + 1] -= *ipitch;
	    awin[(*af << 1) + 2] -= *ipitch;
	}
	while(awin[(*af << 1) + 1] < lrange) {
	    awin[(*af << 1) + 1] += *ipitch;
	    awin[(*af << 1) + 2] += *ipitch;
	}
	ephase = TRUE_;
    } else {
	awin[(*af << 1) + 1] = vwin[(*af << 1) + 1];
	awin[(*af << 1) + 2] = vwin[(*af << 1) + 2];
	ephase = FALSE_;
    }
    j = (awin[(*af << 1) + 2] - awin[(*af << 1) + 1] + 1) / *ipitch * *ipitch;
    if (j == 0 || ! winv) {
	ewin[(*af << 1) + 1] = vwin[(*af << 1) + 1];
	ewin[(*af << 1) + 2] = vwin[(*af << 1) + 2];
    } else if (! ephase && *obound == 2) {
	ewin[(*af << 1) + 1] = awin[(*af << 1) + 2] - j + 1;
	ewin[(*af << 1) + 2] = awin[(*af << 1) + 2];
    } else {
	ewin[(*af << 1) + 1] = awin[(*af << 1) + 1];
	ewin[(*af << 1) + 2] = awin[(*af << 1) + 1] + j - 1;
    }
    return 0;
}

struct dyptrk_state { char pad_0[0x2268]; real s[60]; integer p[120]; integer ipoint; real alphax; };

// MATCH: sound.dll 0x1003f310 _dyptrk_
int dyptrk_(real *amdf, integer *ltau, integer *
	minptr, integer *voice, integer *pitch, integer *midx,
	struct dyptrk_state *st)
{
    real *s;
    integer *p;
    integer *ipoint;
    real *alphax;

    integer i__1;

    integer pbar;
    real sbar;
    integer path[2], iptr, i__, j;
    real alpha, minsc, maxsc;

    s = &(st->s[0]);
    p = &(st->p[0]);
    ipoint = &(st->ipoint);
    alphax = &(st->alphax);

    if (amdf) {
	--amdf;
	}

    if (*voice == 1) {
	*alphax = *alphax * .75f + amdf[*minptr] / 2.f;
    } else {
	*alphax *= .984375f;
    }
    alpha = *alphax / 16;
    if (*voice == 0 && *alphax < 128.f) {
	alpha = 8.f;
    }
    iptr = *ipoint + 1;
    p[iptr * 60 - 60] = 1;
    i__ = 1;
    pbar = 1;
    sbar = s[0];
    i__1 = *ltau;
    for (i__ = 1; i__ <= i__1; ++i__) {
	sbar += alpha;
	if (sbar < s[i__ - 1]) {
	    s[i__ - 1] = sbar;
	    p[i__ + iptr * 60 - 61] = pbar;
	} else {
	    sbar = s[i__ - 1];
	    p[i__ + iptr * 60 - 61] = i__;
	    pbar = i__;
	}
    }
    i__ = pbar - 1;
    sbar = s[i__];
    while(i__ >= 1) {
	sbar += alpha;
	if (sbar < s[i__ - 1]) {
	    s[i__ - 1] = sbar;
	    p[i__ + iptr * 60 - 61] = pbar;
	} else {
	    pbar = p[i__ + iptr * 60 - 61];
	    i__ = pbar;
	    sbar = s[i__ - 1];
	}
	--i__;
    }
    s[0] += amdf[1] / 2;
    minsc = s[0];
    maxsc = minsc;
    *midx = 1;
    i__1 = *ltau;
    for (i__ = 2; i__ <= i__1; ++i__) {
	s[i__ - 1] += amdf[i__] / 2;
	if (s[i__ - 1] > maxsc) {
	    maxsc = s[i__ - 1];
	}
	if (s[i__ - 1] < minsc) {
	    *midx = i__;
	    minsc = s[i__ - 1];
	}
    }
    i__1 = *ltau;
    for (i__ = 1; i__ <= i__1; ++i__) {
	s[i__ - 1] -= minsc;
    }
    maxsc -= minsc;
    j = 0;
    for (i__ = 20; i__ <= 40; i__ += 10) {
	if (*midx > i__) {
	    if (s[*midx - i__ - 1] < maxsc / 4) {
		j = i__;
	    }
	}
    }
    *midx -= j;
    j = *ipoint;
    *pitch = *midx;
    for (i__ = 1; i__ <= 2; ++i__) {
	j = j % 2 + 1;
	*pitch = p[*pitch + j * 60 - 61];
	path[i__ - 1] = *pitch;
    }
    *ipoint = (*ipoint + 1) % 2;
    return 0;
}

struct voicin_state { char pad_0[0x2224]; real dither; real snr; real maxmin; real voice[6];
    integer lbve; integer lbue; integer fbve; integer fbue; integer ofbue; integer sfbue; integer olbue;
    integer slbue; };

// MATCH: sound.dll 0x1003f720 _voicin_
int voicin_(integer *vwin, real *inbuf, real *
	lpbuf, integer *buflim, integer *half, real *minamd, real *maxamd,
	integer *mintau, real *ivrc, integer *obound, integer *voibuf,
	integer *af, struct voicin_state *st)
{
    real *dither;
    static real vdc[100] = { 0.f,1714.f,-110.f,
	    334.f,-4096.f,-654.f,3752.f,3769.f,0.f,1181.f,0.f,874.f,-97.f,
	    300.f,-4096.f,-1021.f,2451.f,2527.f,0.f,-500.f,0.f,510.f,-70.f,
	    250.f,-4096.f,-1270.f,2194.f,2491.f,0.f,-1500.f,0.f,500.f,-10.f,
	    200.f,-4096.f,-1300.f,2e3f,2e3f,0.f,-2e3f,0.f,500.f,0.f,0.f,
	    -4096.f,-1300.f,2e3f,2e3f,0.f,-2500.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,
	    0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,
	    0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,
	    0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f };
    static integer nvdcl = 5;
    static real vdcl[10] = { 600.f,450.f,300.f,200.f,0.f,0.f,0.f,0.f,0.f,0.f };

    integer inbuf_offset, lpbuf_offset, i__1, i__2;
    real r__1, r__2;

    integer i_nint(real *);

    real ar_b__, ar_f__;
    integer *lbve, *lbue, *fbve, *fbue;
    integer snrl, i__;
    integer *ofbue, *sfbue;
    real *voice;
    integer *olbue, *slbue;
    real value[9];
    integer zc;
    logical ot;
    real qs;
    real *maxmin;
    integer vstate;
    real rc1;
    extern int vparms_(integer *, real *, real *, integer *,
	    integer *, real *, integer *, integer *, integer *, integer *,
	    real *, real *, real *, real *);
    integer fbe, lbe;
    real *snr;
    real snr2;

    dither = (&st->dither);
    snr = (&st->snr);
    maxmin = (&st->maxmin);
    voice = (&st->voice[0]);
    lbve = (&st->lbve);
    lbue = (&st->lbue);
    fbve = (&st->fbve);
    fbue = (&st->fbue);
    ofbue = (&st->ofbue);
    olbue = (&st->olbue);
    sfbue = (&st->sfbue);
    slbue = (&st->slbue);

    if (vwin) {
	--vwin;
	}
    if (buflim) {
	--buflim;
	}
    if (inbuf) {
	inbuf_offset = buflim[1];
	inbuf -= inbuf_offset;
	}
    if (lpbuf) {
	lpbuf_offset = buflim[3];
	lpbuf -= lpbuf_offset;
	}
    if (ivrc) {
	--ivrc;
	}
    if (obound) {
	--obound;
	}
    if (voibuf) {
	--voibuf;
	}

    if (*half == 1) {
	voice[0] = voice[2];
	voice[1] = voice[3];
	voice[2] = voice[4];
	voice[3] = voice[5];
	*maxmin = *maxamd / f2c_max(*minamd,1.f);
    }
    vparms_(&vwin[1], &inbuf[inbuf_offset], &lpbuf[lpbuf_offset], &buflim[1],
	    half, dither, mintau, &zc, &lbe, &fbe, &qs, &rc1, &ar_b__, &
	    ar_f__);
    r__1 = (*snr + *fbve / (real) f2c_max(*fbue,1)) * 63 / 64.f;
    *snr = (real) i_nint(&r__1);
    snr2 = *snr * *fbue / f2c_max(*lbue,1);
    snrl = 1;
    i__1 = nvdcl - 1;
    for (snrl = 1; snrl <= i__1; ++snrl) {
	if (snr2 > vdcl[snrl - 1]) {
	    goto L69;
	}
    }
L69:
    value[0] = *maxmin;
    value[1] = (real) lbe / f2c_max(*lbve,1);
    value[2] = (real) zc;
    value[3] = rc1;
    value[4] = qs;
    value[5] = ivrc[2];
    value[6] = ar_b__;
    value[7] = ar_f__;
    voice[*half + 3] = vdc[snrl * 10 - 1];
    for (i__ = 1; i__ <= 8; ++i__) {
	voice[*half + 3] += vdc[i__ + snrl * 10 - 11] * value[i__ - 1];
    }
    if (voice[*half + 3] > 0.f) {
	voibuf[*half + 6] = 1;
    } else {
	voibuf[*half + 6] = 0;
    }
    vstate = -1;
    if (*half == 1) {
	goto L99;
    }
    ot = ((obound[1] & 2) != 0 || obound[2] == 1) && (obound[3] & 1) == 0;
    vstate = (voibuf[3] << 3) + (voibuf[4] << 2) + (voibuf[5] << 1) + voibuf[
	    6];
    switch (vstate + 1) {
	case 1:  goto L99;
	case 2:  goto L1;
	case 3:  goto L2;
	case 4:  goto L99;
	case 5:  goto L4;
	case 6:  goto L5;
	case 7:  goto L6;
	case 8:  goto L7;
	case 9:  goto L8;
	case 10:  goto L99;
	case 11:  goto L10;
	case 12:  goto L11;
	case 13:  goto L99;
	case 14:  goto L13;
	case 15:  goto L14;
	case 16:  goto L99;
    }
L1:
    if (ot && voibuf[7] == 1) {
	voibuf[5] = 1;
    }
    goto L99;
L2:
    if (voibuf[7] == 0 || voice[2] < -voice[3]) {
	voibuf[5] = 0;
    } else {
	voibuf[6] = 1;
    }
    goto L99;
L4:
    voibuf[4] = 0;
    goto L99;
L5:
    if (voice[1] < -voice[2]) {
	voibuf[4] = 0;
    } else {
	voibuf[5] = 1;
    }
    goto L99;
L6:
    if (voibuf[1] == 1 || voibuf[7] == 1 || voice[3] > voice[0]) {
	voibuf[6] = 1;
    } else {
	voibuf[3] = 1;
    }
    goto L99;
L7:
    if (ot) {
	voibuf[4] = 0;
    }
    goto L99;
L8:
    if (ot) {
	voibuf[4] = 1;
    }
    goto L99;
L10:
    if (voice[2] < -voice[1]) {
	voibuf[5] = 0;
    } else {
	voibuf[4] = 1;
    }
    goto L99;
L11:
    voibuf[4] = 1;
    goto L99;
L13:
    if (voibuf[7] == 0 && voice[3] < -voice[2]) {
	voibuf[6] = 0;
    } else {
	voibuf[5] = 1;
    }
    goto L99;
L14:
    if (ot && voibuf[7] == 0) {
	voibuf[5] = 0;
    }
L99:
    if (voibuf[*half + 6] == 0) {
	i__1 = fbe, i__2 = *ofbue * 3;
	r__1 = (*sfbue * 63 + (f2c_min(i__1,i__2) << 3)) / 64.f;
	*sfbue = i_nint(&r__1);
	*fbue = *sfbue / 8;
	*ofbue = fbe;
	i__1 = lbe, i__2 = *olbue * 3;
	r__1 = (*slbue * 63 + (f2c_min(i__1,i__2) << 3)) / 64.f;
	*slbue = i_nint(&r__1);
	*lbue = *slbue / 8;
	*olbue = lbe;
    } else {
	r__1 = (*lbve * 63 + lbe) / 64.f;
	*lbve = i_nint(&r__1);
	r__1 = (*fbve * 63 + fbe) / 64.f;
	*fbve = i_nint(&r__1);
    }
    r__2 = (real)(sqrt((real) (*lbue * *lbve)) * 64 / 3000);
    r__1 = f2c_max(r__2,1.f);
    *dither = f2c_min(r__1,20.f);
    return 0;
}

// MATCH: sound.dll 0x10040740 _placev_
int placev_(integer *osbuf, integer *osptr, integer *oslen,
	integer *obound, integer *vwin, integer *af, integer *lframe, integer
	*minwin, integer *maxwin, integer *dvwinl, integer *dvwinh)
{
    integer i__1, i__2;

    logical crit;
    integer i__, q, osptr1, hrange, lrange;

    --osbuf;
    vwin -= 3;

    i__1 = vwin[((*af - 1) << 1) + 2] + 1, i__2 = (*af - 2) * *lframe + 1;
    lrange = f2c_max(i__1,i__2);
    hrange = *af * *lframe;
    for (osptr1 = *osptr - 1; osptr1 >= 1; --osptr1) {
	if (osbuf[osptr1] <= hrange) {
	    goto L90;
	}
    }
L90:
    ++osptr1;
    if (osptr1 <= 1 || osbuf[osptr1 - 1] < lrange) {
	i__1 = vwin[((*af - 1) << 1) + 2] + 1;
	vwin[(*af << 1) + 1] = f2c_max(i__1,*dvwinl);
	vwin[(*af << 1) + 2] = vwin[(*af << 1) + 1] + *maxwin - 1;
	*obound = 0;
    } else {
	for (q = osptr1 - 1; q >= 1; --q) {
	    if (osbuf[q] < lrange) {
		goto L100;
	    }
	}
L100:
	++q;
	i__1 = osptr1 - 1;
	for (i__ = q + 1; i__ <= i__1; ++i__) {
	    if (osbuf[i__] - osbuf[q] >= *minwin) {
		crit = TRUE_;
		goto L105;
	    }
	}
	crit = FALSE_;
L105:
	i__1 = (*af - 1) * *lframe, i__2 = lrange + *minwin - 1;
	if (! crit && osbuf[q] > f2c_max(i__1,i__2)) {
	    vwin[(*af << 1) + 2] = osbuf[q] - 1;
	    i__1 = lrange, i__2 = vwin[(*af << 1) + 2] - *maxwin + 1;
	    vwin[(*af << 1) + 1] = f2c_max(i__1,i__2);
	    *obound = 2;
	} else {
	    vwin[(*af << 1) + 1] = osbuf[q];
L110:
	    ++q;
	    if (q >= osptr1) {
		goto L120;
	    }
	    if (osbuf[q] > vwin[(*af << 1) + 1] + *maxwin) {
		goto L120;
	    }
	    if (osbuf[q] < vwin[(*af << 1) + 1] + *minwin) {
		goto L110;
	    }
	    vwin[(*af << 1) + 2] = osbuf[q] - 1;
	    *obound = 3;
	    return 0;
L120:
	    i__1 = vwin[(*af << 1) + 1] + *maxwin - 1;
	    vwin[(*af << 1) + 2] = f2c_min(i__1,hrange);
	    *obound = 1;
	}
    }
    return 0;
}

struct onset_state { char pad_0[0x21c4]; real n; real d__; real fpc; real l2buf[16]; real l2sum1;
    integer l2ptr1; integer l2ptr2; integer lasti; logical hyst; };

// MATCH: sound.dll 0x10040ab0 _onset_
int onset_(real *pebuf, integer *osbuf, integer *
	osptr, integer *oslen, integer *sbufl, integer *sbufh, integer *lframe,
	struct onset_state *st)
{
    real *n;
    real *d__;
    real *l2buf;
    real *l2sum1;
    integer *l2ptr1;
    integer *l2ptr2;
    logical *hyst;

    integer pebuf_offset, i__1;
    real r__1;

    double r_sign(real *, real *);

    integer i__;
    integer *lasti;
    real l2sum2;
    real *fpc;

    n = &(st->n);
    d__ = &(st->d__);
    fpc = &(st->fpc);
    l2buf = &(st->l2buf[0]);
    l2sum1 = &(st->l2sum1);
    l2ptr1 = &(st->l2ptr1);
    l2ptr2 = &(st->l2ptr2);
    lasti = &(st->lasti);
    hyst = &(st->hyst);

    if (osbuf) {
	--osbuf;
	}
    if (pebuf) {
	pebuf_offset = *sbufl;
	pebuf -= pebuf_offset;
	}

    if (*hyst) {
	*lasti -= *lframe;
    }
    i__1 = *sbufh;
    for (i__ = *sbufh - *lframe + 1; i__ <= i__1; ++i__) {
	*n = (pebuf[i__] * pebuf[i__ - 1] + (*n) * 63.f) / 64.f;
	r__1 = pebuf[i__ - 1];
	*d__ = (r__1 * r__1 + (*d__) * 63.f) / 64.f;
	if ((*d__) != 0.f) {
	    if (f2c_abs(*n) > (*d__)) {
		*fpc = (real)r_sign(&c_b2, n);
	    } else {
		*fpc = (*n) / (*d__);
	    }
	}
	l2sum2 = l2buf[*l2ptr1 - 1];
	*l2sum1 = *l2sum1 - l2buf[*l2ptr2 - 1] + *fpc;
	l2buf[*l2ptr2 - 1] = *l2sum1;
	l2buf[*l2ptr1 - 1] = *fpc;
	*l2ptr1 = *l2ptr1 % 16 + 1;
	*l2ptr2 = *l2ptr2 % 16 + 1;
	if ((r__1 = *l2sum1 - l2sum2, f2c_abs(r__1)) > 1.7f) {
	    if (! (*hyst)) {
		if (*osptr <= *oslen) {
		    osbuf[*osptr] = i__ - 9;
		    ++(*osptr);
		}
		*hyst = TRUE_;
	    }
	    *lasti = i__;
	} else if ((*hyst) && i__ - *lasti >= 10) {
	    *hyst = FALSE_;
	}
    }
    return 0;
}

// MATCH: sound.dll 0x10041020 _vparms_
int vparms_(integer *vwin, real *inbuf, real *lpbuf, integer
	*buflim, integer *half, real *dither, integer *mintau, integer *zc,
	integer *lbe, integer *fbe, real *qs, real *rc1, real *ar_b__, real *
	ar_f__)
{
    integer inbuf_offset, lpbuf_offset, i__1;
    real r__1, r__2;

    double r_sign(real *, real *);
    integer i_nint(real *);

    integer vlen, stop, i__;
    real e_pre__;
    integer start;
    real ap_rms__, e_0__, oldsgn, lp_rms__, e_b__, e_f__, r_b__, r_f__, e0ap;

    --vwin;
    --buflim;
    lpbuf_offset = buflim[3];
    lpbuf -= lpbuf_offset;
    inbuf_offset = buflim[1];
    inbuf -= inbuf_offset;

    lp_rms__ = 0.f;
    ap_rms__ = 0.f;
    e_pre__ = 0.f;
    e0ap = 0.f;
    *rc1 = 0.f;
    e_0__ = 0.f;
    e_b__ = 0.f;
    e_f__ = 0.f;
    r_f__ = 0.f;
    r_b__ = 0.f;
    *zc = 0;
    vlen = vwin[2] - vwin[1] + 1;
    start = vwin[1] + (*half - 1) * vlen / 2 + 1;
    stop = start + vlen / 2 - 1;

    r__1 = inbuf[start - 1] - *dither;
    oldsgn = (real)r_sign(&c_b2, &r__1);
    i__1 = stop;
    for (i__ = start; i__ <= i__1; ++i__) {
	lp_rms__ += (r__1 = lpbuf[i__], f2c_abs(r__1));
	ap_rms__ += (r__1 = inbuf[i__], f2c_abs(r__1));
	e_pre__ += (r__1 = inbuf[i__] - inbuf[i__ - 1], f2c_abs(r__1));
	r__1 = inbuf[i__];
	e0ap += r__1 * r__1;
	*rc1 += inbuf[i__] * inbuf[i__ - 1];
	r__1 = lpbuf[i__];
	e_0__ += r__1 * r__1;
	r__1 = lpbuf[i__ - *mintau];
	e_b__ += r__1 * r__1;
	r__1 = lpbuf[i__ + *mintau];
	e_f__ += r__1 * r__1;
	r_f__ += lpbuf[i__] * lpbuf[i__ + *mintau];
	r_b__ += lpbuf[i__] * lpbuf[i__ - *mintau];
	r__1 = inbuf[i__] + *dither;
	if (r_sign(&c_b2, &r__1) != oldsgn) {
	    ++(*zc);
	    oldsgn = -oldsgn;
	}
	*dither = -(*dither);
    }
    *rc1 /= f2c_max(e0ap,1.f);
    r__1 = ap_rms__ * 2.f;
    *qs = e_pre__ / f2c_max(r__1,1.f);
    *ar_b__ = r_b__ / f2c_max(e_b__,1.f) * (r_b__ / f2c_max(e_0__,1.f));
    *ar_f__ = r_f__ / f2c_max(e_f__,1.f) * (r_f__ / f2c_max(e_0__,1.f));
    r__2 = (real) (*zc << 1);
    r__1 = r__2 * (90.f / vlen);
    *zc = i_nint(&r__1);
    r__1 = lp_rms__ / 4 * (90.f / vlen);
    i__1 = i_nint(&r__1);
    *lbe = f2c_min(i__1,32767);
    r__1 = ap_rms__ / 4 * (90.f / vlen);
    i__1 = i_nint(&r__1);
    *fbe = f2c_min(i__1,32767);
    return 0;
}
