// FLAGS sound.dll: /Od /ZI /GZ
// LANG c
// m36c D1, hand-matched from Ghidra's decompilation and the instructions: sound.dll's speech codec, written in the
// shape of f2c output (LPC-10 2400 bps codec with its state in a struct per encoder / decoder) and the f2c runtime
// helpers. Field names follow that shape and are not evidence; offsets are (each store cites its offset).
#include <stdlib.h>
#include <math.h>

typedef long int integer;
typedef float real;
typedef long int logical;
typedef short int shortint;
#define TRUE_ (1)
#define FALSE_ (0)

struct lpc10_encoder_state {
    real z11, z21, z12, z22;            /* 0x0000 */
    real inbuf[540], pebuf[540];        /* 0x0010, 0x0880 */
    real lpbuf[696], ivbuf[312];        /* 0x10f0, 0x1bd0 */
    real bias;                          /* 0x20b0 */
    integer osbuf[10];                  /* 0x20b4 */
    integer osptr;                      /* 0x20dc */
    integer obound[3];                  /* 0x20e0 */
    integer vwin[6];                    /* 0x20ec */
    integer awin[6];                    /* 0x2104 */
    integer voibuf[8];                  /* 0x211c */
    real rmsbuf[3];                     /* 0x213c */
    real rcbuf[30];                     /* 0x2148 */
    real zpre;                          /* 0x21c0 */
    real n;                             /* 0x21c4 */
    real d__;                           /* 0x21c8 */
    real fpc;                           /* 0x21cc */
    real l2buf[16];                     /* 0x21d0 */
    real l2sum1;                        /* 0x2210 */
    integer l2ptr1, l2ptr2, lasti;      /* 0x2214 */
    logical hyst;                       /* 0x2220 */
    real dither, snr, maxmin;           /* 0x2224 */
    real voice[6];                      /* 0x2230 */
    integer lbve, lbue, fbve, fbue;     /* 0x2248 */
    integer ofbue, sfbue;               /* 0x2258 */
    integer olbue, slbue;               /* 0x2260 */
    real s[60];                         /* 0x2268 */
    integer p[120];                     /* 0x2358 */
    integer ipoint;                     /* 0x2538 */
    real alphax;                        /* 0x253c */
    integer isync;                      /* 0x2540, size 0x2544 (push 0x2544 at 0x1003aaa8) */
};

struct lpc10_decoder_state {
    integer iptold;                     /* 0x000 */
    logical first;                      /* 0x004 */
    integer ivp2h, iovoic, iavgp, erate;/* 0x008 */
    integer drc[30];                    /* 0x018 */
    integer dpit[3], drms[3];           /* 0x090, 0x09c */
    real buf[360];                      /* 0x0a8 */
    integer buflen;                     /* 0x648 */
    integer ivoico, ipito;              /* 0x64c */
    real rmso;                          /* 0x654 */
    real rco[10];                       /* 0x658 */
    integer jsamp;                      /* 0x680 */
    logical first_pitsyn;               /* 0x684 */
    integer ipo;                        /* 0x688 */
    real exc[166], exc2[166];           /* 0x68c, 0x924 */
    real lpi1, lpi2, lpi3, hpi1, hpi2, hpi3, rmso_bsynz; /* 0xbbc */
    integer j, k;                       /* 0xbd8 */
    shortint y[5];                      /* 0xbe0 */
    real dei1, dei2, deo1, deo2, deo3;  /* 0xbec, size 0xc00 (push 0xc00) */
};

extern integer c__10_10063ef0;          /* the constant 10 read at 0x10063ef0 */
extern integer c__180_10063ef4;
extern integer c__10_10063ef8;

int lpcini_(void);                                                          /* 0x1003aa50 */
void init_lpc10_encoder_state(struct lpc10_encoder_state *st);
void init_lpc10_decoder_state(struct lpc10_decoder_state *st);
int chanrd_(integer *order, integer *ipitv, integer *irms, integer *irc, integer *ibits);   /* 0x1003c1d0 */
int chanwr_(integer *order, integer *ipitv, integer *irms, integer *irc, integer *ibits,
            struct lpc10_encoder_state *st);                                /* 0x1003c180 */
int decode_(integer *ipitv, integer *irms, integer *irc, integer *voice, integer *pitch, real *rms,
            real *rc, struct lpc10_decoder_state *st);                      /* 0x1003b5a0 */
int synths_(integer *voice, integer *pitch, real *rms, real *rc, real *speech, integer *k,
            struct lpc10_decoder_state *st);                                /* 0x1003b230 */
int prepro_(real *speech, integer *length, struct lpc10_encoder_state *st); /* 0x1003d040 */
int analys_(real *speech, integer *voice, integer *pitch, real *rms, real *rc,
            struct lpc10_encoder_state *st);                                /* 0x1003c640 */
int encode_(integer *voice, integer *pitch, real *rms, real *rc, integer *ipitch, integer *irms,
            integer *irc);                                                  /* 0x1003c220 */

// MATCH: sound.dll 0x1003a980 _lpc10_decode
int lpc10_decode(integer *bits, real *speech, struct lpc10_decoder_state *st)
{
    integer irms, voice[2], pitch, ipitv;
    real rc[10];
    integer irc[10], len;
    real rms;

    /* Parameter adjustments */
    if (bits) {
        --bits;
    }
    if (speech) {
        --speech;
    }

    /* Function Body */
    chanrd_(&c__10_10063ef0, &ipitv, &irms, irc, &bits[1]);
    decode_(&ipitv, &irms, irc, voice, &pitch, &rms, rc, st);
    synths_(voice, &pitch, &rms, rc, &speech[1], &len, st);
    return 0;
}

// MATCH: sound.dll 0x1003aa90 _create_lpc10_encoder_state
struct lpc10_encoder_state *create_lpc10_encoder_state(void)
{
    struct lpc10_encoder_state *st;

    st = (struct lpc10_encoder_state *) malloc((unsigned) sizeof (struct lpc10_encoder_state));
    if (st != 0) {
        init_lpc10_encoder_state(st);
    }
    return (st);
}

// MATCH: sound.dll 0x1003aae0 _init_lpc10_encoder_state
void init_lpc10_encoder_state(struct lpc10_encoder_state *st)
{
    int i;

    lpcini_();

    st->z11 = 0.0f;
    st->z21 = 0.0f;
    st->z12 = 0.0f;
    st->z22 = 0.0f;

    for (i = 0; i < 540; i++) {
        st->inbuf[i] = 0.0f;
        st->pebuf[i] = 0.0f;
    }
    for (i = 0; i < 696; i++) {
        st->lpbuf[i] = 0.0f;
    }
    for (i = 0; i < 312; i++) {
        st->ivbuf[i] = 0.0f;
    }
    st->bias = 0.0f;
    st->osptr = 1;
    for (i = 0; i < 3; i++) {
        st->obound[i] = 0;
    }
    st->vwin[4] = 307;
    st->vwin[5] = 462;
    st->awin[4] = 307;
    st->awin[5] = 462;
    for (i = 0; i < 8; i++) {
        st->voibuf[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        st->rmsbuf[i] = 0.0f;
    }
    for (i = 0; i < 30; i++) {
        st->rcbuf[i] = 0.0f;
    }
    st->zpre = 0.0f;

    st->n = 0.0f;
    st->d__ = 1.0f;
    for (i = 0; i < 16; i++) {
        st->l2buf[i] = 0.0f;
    }
    st->l2sum1 = 0.0f;
    st->l2ptr1 = 1;
    st->l2ptr2 = 9;
    st->hyst = FALSE_;

    st->dither = 20.0f;
    st->maxmin = 0.0f;
    for (i = 0; i < 6; i++) {
        st->voice[i] = 0.0f;
    }
    st->lbve = 3000;
    st->fbve = 3000;
    st->fbue = 187;
    st->ofbue = 187;
    st->sfbue = 187;
    st->lbue = 93;
    st->olbue = 93;
    st->slbue = 93;
    st->snr = (real) (st->fbve / st->fbue << 6);

    for (i = 0; i < 60; i++) {
        st->s[i] = 0.0f;
    }
    for (i = 0; i < 120; i++) {
        st->p[i] = 0;
    }
    st->ipoint = 0;
    st->alphax = 0.0f;

    st->isync = 0;
}

// MATCH: sound.dll 0x1003aea0 _create_lpc10_decoder_state
struct lpc10_decoder_state *create_lpc10_decoder_state(void)
{
    struct lpc10_decoder_state *st;

    st = (struct lpc10_decoder_state *) malloc((unsigned) sizeof (struct lpc10_decoder_state));
    if (st != 0) {
        init_lpc10_decoder_state(st);
    }
    return (st);
}

// MATCH: sound.dll 0x1003aef0 _init_lpc10_decoder_state
void init_lpc10_decoder_state(struct lpc10_decoder_state *st)
{
    int i;

    lpcini_();

    st->iptold = 60;
    st->first = TRUE_;
    st->ivp2h = 0;
    st->iovoic = 0;
    st->iavgp = 60;
    st->erate = 0;
    for (i = 0; i < 30; i++) {
        st->drc[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        st->dpit[i] = 0;
        st->drms[i] = 0;
    }

    for (i = 0; i < 360; i++) {
        st->buf[i] = 0.0f;
    }
    st->buflen = 180;

    st->rmso = 1.0f;
    st->first_pitsyn = TRUE_;

    st->ipo = 0;
    for (i = 0; i < 166; i++) {
        st->exc[i] = 0.0f;
        st->exc2[i] = 0.0f;
    }
    st->lpi1 = 0.0f;
    st->lpi2 = 0.0f;
    st->lpi3 = 0.0f;
    st->hpi1 = 0.0f;
    st->hpi2 = 0.0f;
    st->hpi3 = 0.0f;
    st->rmso_bsynz = 0.0f;

    st->j = 2;
    st->k = 5;
    st->y[0] = (shortint) -21161;
    st->y[1] = (shortint) -8478;
    st->y[2] = (shortint) 30892;
    st->y[3] = (shortint) -10216;
    st->y[4] = (shortint) 16950;

    st->dei1 = 0.0f;
    st->dei2 = 0.0f;
    st->deo1 = 0.0f;
    st->deo2 = 0.0f;
    st->deo3 = 0.0f;
}

// MATCH: sound.dll 0x1003b150 _lpc10_encode
int lpc10_encode(real *speech, integer *bits, struct lpc10_encoder_state *st)
{
    integer irms, voice[2], pitch, ipitv;
    real rc[10];
    integer irc[10];
    real rms;

    /* Parameter adjustments */
    if (speech) {
        --speech;
    }
    if (bits) {
        --bits;
    }

    /* Function Body */
    prepro_(&speech[1], &c__180_10063ef4, st);
    analys_(&speech[1], voice, &pitch, &rms, rc, st);
    encode_(voice, &pitch, &rms, rc, &ipitv, &irms, irc);
    chanwr_(&c__10_10063ef8, &ipitv, &irms, irc, &bits[1], st);
    return 0;
}

// MATCH: sound.dll 0x1003e2c0 _pow_ii
integer pow_ii(integer *ap, integer *bp)
{
    integer pow, x, n;
    unsigned long u;

    x = *ap;
    n = *bp;

    if (n <= 0) {
        if (n == 0 || x == 1)
            return 1;
        if (x != -1)
            return x == 0 ? 1/x : 0;
        n = -n;
    }
    u = n;
    for(pow = 1; ; )
    {
        if(u & 01)
            pow *= x;
        if(u >>= 1)
            x *= x;
        else
            break;
    }
    return(pow);
}

// MATCH: sound.dll 0x1003e380 _r_sign
double r_sign(real *a, real *b)
{
    double x;
    x = (*a >= 0 ? *a : - *a);
    return( *b >= 0 ? x : -x);
}

// MATCH: sound.dll 0x1003e400 _i_nint
integer i_nint(real *x)
{
    return (integer)( (*x)>=0 ?
        floor(*x + .5) : -floor(.5 - *x) );
}

// MATCH: sound.dll 0x1003e480 _median_
integer median_(integer *d1, integer *d2, integer *d3)
{
    /* System generated locals */
    integer ret_val;

    ret_val = *d2;
    if (*d2 > *d1 && *d2 > *d3) {
        ret_val = *d1;
        if (*d3 > *d1) {
            ret_val = *d3;
        }
    } else if (*d2 < *d1 && *d2 < *d3) {
        ret_val = *d1;
        if (*d3 < *d1) {
            ret_val = *d3;
        }
    }
    return ret_val;
}

extern integer dactab_10064a80[128];    /* the 128-entry table read at 0x1003e58f */

// MATCH: sound.dll 0x1003e520 _ham84_
int ham84_(integer *input, integer *output, integer *errcnt)
{
    integer i__, j, parity;

    parity = *input & 255;
    parity ^= parity / 16;
    parity ^= parity / 4;
    parity ^= parity / 2;
    parity &= 1;
    i__ = dactab_10064a80[(*input & 127)];
    *output = i__ & 15;
    j = i__ & 16;
    if (j != 0) {
        if (parity != 0) {
            ++(*errcnt);
        }
    } else {
        ++(*errcnt);
        if (parity == 0) {
            ++(*errcnt);
            *output = -1;
        }
    }
    return 0;
}

#define abs(x) ((x) >= 0 ? (x) : -(x))

// MATCH: sound.dll 0x1003e600 _rcchk_
int rcchk_(integer *order, real *rc1f, real *rc2f)
{
    /* System generated locals */
    integer i__1;
    real r__1;

    /* Local variables */
    integer i__;

    /* Parameter adjustments */
    --rc2f;
    --rc1f;

    /* Function Body */
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        if ((r__1 = rc2f[i__], abs(r__1)) > .99f) {
            goto L10;
        }
    }
    return 0;
L10:
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        rc2f[i__] = rc1f[i__];
    }
    return 0;
}

#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))

extern struct {
    integer order, lframe;
    logical corrp;
} contrl_;                              /* 0x100b58a0 (written by lpcini_ 0x1003aa50) */
#define contrl_1 contrl_

extern real c_b2_10063efc;              /* the float 0.7 read at 0x10063efc */

int pitsyn_(integer *order, integer *voice, integer *pitch, real *rms, real *rc, integer *lframe,
            integer *ivuv, integer *ipiti, real *rmsi, real *rci, integer *nout, real *ratio,
            struct lpc10_decoder_state *st);                                /* 0x1003da50 */
int bsynz_(real *coef, integer *ip, integer *iv, real *sout, real *rms, real *ratio, real *g2pass,
           struct lpc10_decoder_state *st);                                 /* 0x1003d1d0 */
int irc2pc_(real *rc, real *pc, integer *order, real *gprime, real *g2pass);
int deemp_(real *x, integer *n, struct lpc10_decoder_state *st);

// MATCH: sound.dll 0x1003b230 _synths_
int synths_(integer *voice, integer *pitch, real *rms, real *rc, real *speech, integer *k,
            struct lpc10_decoder_state *st)
{
    real *buf;
    integer *buflen;

    /* System generated locals */
    integer i__1;
    real r__1, r__2;

    /* Local variables */
    real rmsi[16];
    integer nout, ivuv[16], i__, j;
    real ratio;
    integer ipiti[16];
    real g2pass;
    real pc[10];
    real rci[160];

    /* Parameter adjustments */
    if (voice) {
        --voice;
    }
    if (rc) {
        --rc;
    }
    if (speech) {
        --speech;
    }

    /* Function Body */
    buf = &(st->buf[0]);
    buflen = &(st->buflen);

/* Computing MAX */
    i__1 = min(*pitch,156);
    *pitch = max(i__1,20);
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
/* Computing MAX */
/* Computing MIN */
        r__2 = rc[i__];
        r__1 = min(r__2,.99f);
        rc[i__] = max(r__1,-.99f);
    }
    pitsyn_(&contrl_1.order, &voice[1], pitch, rms, &rc[1], &contrl_1.lframe,
            ivuv, ipiti, rmsi, rci, &nout, &ratio, st);
    if (nout > 0) {
        i__1 = nout;
        for (j = 1; j <= i__1; ++j) {
            irc2pc_(&rci[j * 10 - 10], pc, &contrl_1.order, &c_b2_10063efc, &g2pass);
            bsynz_(pc, &ipiti[j - 1], &ivuv[j - 1], &buf[*buflen], &rmsi[j - 1],
                   &ratio, &g2pass, st);
            deemp_(&buf[*buflen], &ipiti[j - 1], st);
            *buflen += ipiti[j - 1];
        }
        for (i__ = 1; i__ <= 180; ++i__) {
            speech[i__] = buf[i__ - 1] / 4096.f;
        }
        *k = 180;
        *buflen += -180;
        i__1 = *buflen;
        for (i__ = 1; i__ <= i__1; ++i__) {
            buf[i__ - 1] = buf[i__ + 179];
        }
    }
    return 0;
}

integer pow_ii(integer *ap, integer *bp);
extern integer c__2_10064554;           /* the constant 2 passed to pow_ii at 0x1003c51e */
extern integer enctab_10064558[16];
extern integer entau_10064598[60];
extern integer enadd_10064688[8];
extern real enscl_100646a8[8];
extern integer enbits_100646c8[8];
extern integer entab6_100646e8[64];
extern integer rmst_100647e8[64];

// MATCH: sound.dll 0x1003c220 _encode_
int encode_(integer *voice, integer *pitch, real *rms, real *rc, integer *ipitch, integer *irms,
            integer *irc)
{
    /* System generated locals */
    integer i__1, i__2;

    /* Local variables */
    integer idel, nbit, i__, j, i2, i3, mrk;

    /* Parameter adjustments */
    --irc;
    --rc;
    --voice;

    /* Function Body */
    *irms = *rms;
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        irc[i__] = rc[i__] * 32768.f;
    }
    if (voice[1] != 0 && voice[2] != 0) {
        *ipitch = entau_10064598[*pitch - 1];
    } else {
        if (contrl_1.corrp) {
            *ipitch = 0;
            if (voice[1] != voice[2]) {
                *ipitch = 127;
            }
        } else {
            *ipitch = (voice[1] << 1) + voice[2];
        }
    }
    j = 32;
    idel = 16;
    *irms = min(*irms,1023);
    while(idel > 0) {
        if (*irms > rmst_100647e8[j - 1]) {
            j -= idel;
        }
        if (*irms < rmst_100647e8[j - 1]) {
            j += idel;
        }
        idel /= 2;
    }
    if (*irms > rmst_100647e8[j - 1]) {
        --j;
    }
    *irms = 31 - j / 2;
    for (i__ = 1; i__ <= 2; ++i__) {
        i2 = irc[i__];
        mrk = 0;
        if (i2 < 0) {
            i2 = -i2;
            mrk = 1;
        }
        i2 /= 512;
        i2 = min(i2,63);
        i2 = entab6_100646e8[i2];
        if (mrk != 0) {
            i2 = -i2;
        }
        irc[i__] = i2;
    }
    i__1 = contrl_1.order;
    for (i__ = 3; i__ <= i__1; ++i__) {
        i2 = irc[i__] / 2;
        i2 = (integer) ((i2 + enadd_10064688[contrl_1.order + 1 - i__ - 1]) * enscl_100646a8[
                contrl_1.order + 1 - i__ - 1]);
/* Computing MIN */
        i__2 = max(i2,-127);
        i2 = min(i__2,127);
        nbit = enbits_100646c8[contrl_1.order + 1 - i__ - 1];
        i3 = 0;
        if (i2 < 0) {
            i3 = -1;
        }
        i2 /= pow_ii(&c__2_10064554, &nbit);
        if (i3 == -1) {
            --i2;
        }
        irc[i__] = i2;
    }
    if (contrl_1.corrp) {
        if (*ipitch == 0 || *ipitch == 127) {
            irc[5] = enctab_10064558[(irc[1] & 30) / 2];
            irc[6] = enctab_10064558[(irc[2] & 30) / 2];
            irc[7] = enctab_10064558[(irc[3] & 30) / 2];
            irc[8] = enctab_10064558[(*irms & 30) / 2];
            irc[9] = enctab_10064558[(irc[4] & 30) / 2] / 2;
            irc[10] = enctab_10064558[(irc[4] & 30) / 2] & 1;
        }
    }
    return 0;
}

integer random_(struct lpc10_decoder_state *st);                           /* 0x10040f60 */
extern integer kexc_10064a1c[25];       /* read at 0x1003d48e as [i*4 + 0x10064a18] */

// MATCH: sound.dll 0x1003d1d0 _bsynz_
int bsynz_(real *coef, integer *ip, integer *iv, real *sout, real *rms, real *ratio, real *g2pass,
           struct lpc10_decoder_state *st)
{
    integer *ipo;
    real *rmso;
    real *exc;
    real *exc2;
    real *lpi1;
    real *lpi2;
    real *lpi3;
    real *hpi1;
    real *hpi2;
    real *hpi3;

    /* System generated locals */
    integer i__1, i__2;
    real r__1, r__2;

    /* Local variables */
    real gain, xssq;
    integer i__, j, k;
    real noise[166], pulse;
    integer px;
    real sscale;
    real xy, sum, ssq;
    real lpi0, hpi0;

    /* Parameter adjustments */
    if (coef) {
        --coef;
    }
    if (sout) {
        --sout;
    }

    /* Function Body */
    ipo = &(st->ipo);
    exc = &(st->exc[0]);
    exc2 = &(st->exc2[0]);
    lpi1 = &(st->lpi1);
    lpi2 = &(st->lpi2);
    lpi3 = &(st->lpi3);
    hpi1 = &(st->hpi1);
    hpi2 = &(st->hpi2);
    hpi3 = &(st->hpi3);
    rmso = &(st->rmso_bsynz);

/* Computing MIN */
    r__1 = *rmso / (*rms + 1e-6f);
    xy = min(r__1,8.f);
    *rmso = *rms;
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        exc2[i__ - 1] = exc2[*ipo + i__ - 1] * xy;
    }
    *ipo = *ip;
    if (*iv == 0) {
        i__1 = *ip;
        for (i__ = 1; i__ <= i__1; ++i__) {
            exc[contrl_1.order + i__ - 1] = (real) (random_(st) / 64);
        }
        px = (random_(st) + 32768) * (*ip - 1) / 65536 + contrl_1.order + 1;
        r__1 = *ratio / 4 * 1.f;
        pulse = r__1 * 342;
        if (pulse > 2e3f) {
            pulse = 2e3f;
        }
        exc[px - 1] += pulse;
        exc[px] -= pulse;
    } else {
        sscale = sqrt((real) (*ip)) / 6.928f;
        i__1 = *ip;
        for (i__ = 1; i__ <= i__1; ++i__) {
            exc[contrl_1.order + i__ - 1] = 0.f;
            if (i__ <= 25) {
                exc[contrl_1.order + i__ - 1] = sscale * kexc_10064a1c[i__ - 1];
            }
            lpi0 = exc[contrl_1.order + i__ - 1];
            r__2 = exc[contrl_1.order + i__ - 1] * .125f + *lpi1 * .75f;
            r__1 = r__2 + *lpi2 * .125f;
            exc[contrl_1.order + i__ - 1] = r__1 + *lpi3 * 0.f;
            *lpi3 = *lpi2;
            *lpi2 = *lpi1;
            *lpi1 = lpi0;
        }
        i__1 = *ip;
        for (i__ = 1; i__ <= i__1; ++i__) {
            noise[contrl_1.order + i__ - 1] = random_(st) * 1.f / 64;
            hpi0 = noise[contrl_1.order + i__ - 1];
            r__2 = noise[contrl_1.order + i__ - 1] * -.125f + *hpi1 * .25f;
            r__1 = r__2 + *hpi2 * -.125f;
            noise[contrl_1.order + i__ - 1] = r__1 + *hpi3 * 0.f;
            *hpi3 = *hpi2;
            *hpi2 = *hpi1;
            *hpi1 = hpi0;
        }
        i__1 = *ip;
        for (i__ = 1; i__ <= i__1; ++i__) {
            exc[contrl_1.order + i__ - 1] += noise[contrl_1.order + i__ - 1];
        }
    }
    xssq = 0.f;
    i__1 = *ip;
    for (i__ = 1; i__ <= i__1; ++i__) {
        k = contrl_1.order + i__;
        sum = 0.f;
        i__2 = contrl_1.order;
        for (j = 1; j <= i__2; ++j) {
            sum += coef[j] * exc[k - j - 1];
        }
        sum *= *g2pass;
        exc2[k - 1] = sum + exc[k - 1];
    }
    i__1 = *ip;
    for (i__ = 1; i__ <= i__1; ++i__) {
        k = contrl_1.order + i__;
        sum = 0.f;
        i__2 = contrl_1.order;
        for (j = 1; j <= i__2; ++j) {
            sum += coef[j] * exc2[k - j - 1];
        }
        exc2[k - 1] = sum + exc2[k - 1];
        xssq += exc2[k - 1] * exc2[k - 1];
    }
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        exc[i__ - 1] = exc[*ip + i__ - 1];
        exc2[i__ - 1] = exc2[*ip + i__ - 1];
    }
    r__1 = *rms * *rms;
    ssq = r__1 * *ip;
    gain = sqrt(ssq / xssq);
    i__1 = *ip;
    for (i__ = 1; i__ <= i__1; ++i__) {
        sout[i__] = gain * exc2[contrl_1.order + i__ - 1];
    }
    return 0;
}

// MATCH: sound.dll 0x1003da50 _pitsyn_
int pitsyn_(integer *order, integer *voice, integer *pitch, real *rms, real *rc, integer *lframe,
            integer *ivuv, integer *ipiti, real *rmsi, real *rci, integer *nout, real *ratio,
            struct lpc10_decoder_state *st)
{
    real *rmso;
    logical *first;

    /* System generated locals */
    integer rci_dim1, rci_offset, i__1, i__2;
    real r__1;

    /* Local variables */
    real alrn, alro, yarc[10], prop;
    integer i__, j, vflag, jused, lsamp;
    integer *jsamp;
    real slope;
    integer *ipito;
    real uvpit;
    integer ip, nl, ivoice;
    integer *ivoico;
    integer istart;
    real *rco;
    real xxy;

    /* Parameter adjustments */
    if (rc) {
        --rc;
    }
    if (rci) {
        rci_dim1 = *order;
        rci_offset = rci_dim1 + 1;
        rci -= rci_offset;
    }
    if (voice) {
        --voice;
    }
    if (ivuv) {
        --ivuv;
    }
    if (ipiti) {
        --ipiti;
    }
    if (rmsi) {
        --rmsi;
    }

    /* Function Body */
    ivoico = &(st->ivoico);
    ipito = &(st->ipito);
    rmso = &(st->rmso);
    rco = &(st->rco[0]);
    jsamp = &(st->jsamp);
    first = &(st->first_pitsyn);

    if (*rms < 1.f) {
        *rms = 1.f;
    }
    if (*rmso < 1.f) {
        *rmso = 1.f;
    }
    uvpit = 0.f;
    *ratio = *rms / (*rmso + 8.f);
    if (*first) {
        lsamp = 0;
        ivoice = voice[2];
        if (ivoice == 0) {
            *pitch = *lframe / 4;
        }
        *nout = *lframe / *pitch;
        *jsamp = *lframe - *nout * *pitch;
        i__1 = *nout;
        for (i__ = 1; i__ <= i__1; ++i__) {
            i__2 = *order;
            for (j = 1; j <= i__2; ++j) {
                rci[j + i__ * rci_dim1] = rc[j];
            }
            ivuv[i__] = ivoice;
            ipiti[i__] = *pitch;
            rmsi[i__] = *rms;
        }
        *first = FALSE_;
    } else {
        vflag = 0;
        lsamp = *lframe + *jsamp;
        slope = (*pitch - *ipito) / (real) lsamp;
        *nout = 0;
        jused = 0;
        istart = 1;
        if (voice[1] == *ivoico && voice[2] == voice[1]) {
            if (voice[2] == 0) {
                *pitch = *lframe / 4;
                *ipito = *pitch;
                if (*ratio > 8.f) {
                    *rmso = *rms;
                }
            }
            slope = (*pitch - *ipito) / (real) lsamp;
            ivoice = voice[2];
        } else {
            if (*ivoico != 1) {
                if (*ivoico == voice[1]) {
                    nl = lsamp - *lframe / 4;
                } else {
                    nl = lsamp - *lframe * 3 / 4;
                }
                ipiti[1] = nl / 2;
                ipiti[2] = nl - ipiti[1];
                ivuv[1] = 0;
                ivuv[2] = 0;
                rmsi[1] = *rmso;
                rmsi[2] = *rmso;
                i__1 = *order;
                for (i__ = 1; i__ <= i__1; ++i__) {
                    rci[i__ + rci_dim1] = rco[i__ - 1];
                    rci[i__ + (rci_dim1 << 1)] = rco[i__ - 1];
                    rco[i__ - 1] = rc[i__];
                }
                slope = 0.f;
                *nout = 2;
                *ipito = *pitch;
                jused = nl;
                istart = nl + 1;
                ivoice = 1;
            } else {
                if (*ivoico != voice[1]) {
                    lsamp = *lframe / 4 + *jsamp;
                } else {
                    lsamp = *lframe * 3 / 4 + *jsamp;
                }
                i__1 = *order;
                for (i__ = 1; i__ <= i__1; ++i__) {
                    yarc[i__ - 1] = rc[i__];
                    rc[i__] = rco[i__ - 1];
                }
                ivoice = 1;
                slope = 0.f;
                vflag = 1;
            }
        }
        while (TRUE_) {
            i__1 = lsamp;
            for (i__ = istart; i__ <= i__1; ++i__) {
                r__1 = *ipito + slope * i__;
                ip = r__1 + .5f;
                if (uvpit != 0.f) {
                    ip = uvpit;
                }
                if (ip <= i__ - jused) {
                    ++(*nout);
                    ipiti[*nout] = ip;
                    *pitch = ip;
                    ivuv[*nout] = ivoice;
                    jused += ip;
                    prop = (jused - ip / 2) / (real) lsamp;
                    i__2 = *order;
                    for (j = 1; j <= i__2; ++j) {
                        alro = log((rco[j - 1] + 1) / (1 - rco[j - 1]));
                        alrn = log((rc[j] + 1) / (1 - rc[j]));
                        xxy = alro + prop * (alrn - alro);
                        xxy = exp(xxy);
                        rci[j + *nout * rci_dim1] = (xxy - 1) / (xxy + 1);
                    }
                    rmsi[*nout] = log(*rmso) + prop * (log(*rms) - log(*rmso));
                    rmsi[*nout] = exp(rmsi[*nout]);
                }
            }
            if (vflag != 1) {
                goto L200;
            }
            vflag = 0;
            istart = jused + 1;
            lsamp = *lframe + *jsamp;
            slope = 0.f;
            ivoice = 0;
            uvpit = (real) ((lsamp - istart) / 2);
            if (uvpit > 90.f) {
                uvpit /= 2;
            }
            *rmso = *rms;
            i__1 = *order;
            for (i__ = 1; i__ <= i__1; ++i__) {
                rc[i__] = yarc[i__ - 1];
                rco[i__ - 1] = yarc[i__ - 1];
            }
        }
L200:
        *jsamp = lsamp - jused;
    }
    if (*nout != 0) {
        *ivoico = voice[2];
        *ipito = *pitch;
        *rmso = *rms;
        i__1 = *order;
        for (i__ = 1; i__ <= i__1; ++i__) {
            rco[i__ - 1] = rc[i__];
        }
    }
    return 0;
}

extern integer c__2_10063f00;           /* the constant 2 passed to pow_ii at 0x1003be13 */
extern integer ethrs_10063f04, ethrs1_10063f08, ethrs2_10063f0c, ethrs3_10063f10;
extern integer ivtab_10063f14[32];
extern real corth_10063f94[32];
extern integer detau_10064014[128];
extern integer rmst_10064214[64];
extern integer detab7_10064314[32];
extern real descl_10064394[8];
extern integer deadd_100643b4[8];
extern integer qb_100643d4[8];
extern integer nbit_100643f4[10];
extern integer zrc_1006441c[10];
extern integer bit_10064444[5];

// MATCH: sound.dll 0x1003b5a0 _decode_
int decode_(integer *ipitv, integer *irms, integer *irc, integer *voice, integer *pitch, real *rms,
            real *rc, struct lpc10_decoder_state *st)
{
    logical *first;
    integer *ivp2h;
    integer *iovoic;
    integer *iavgp;
    integer *iptold;
    integer *erate;
    integer *drc;
    integer *dpit;
    integer *drms;

    /* System generated locals */
    integer i__1, i__2;

    /* Local variables */
    integer ipit, iout, i__, icorf, index, ivoic, ixcor, i1, i2, i4;
    integer ishift, errcnt, lsb;

    /* Parameter adjustments */
    if (irc) {
        --irc;
    }
    if (voice) {
        --voice;
    }
    if (rc) {
        --rc;
    }

    /* Function Body */
    iptold = &(st->iptold);
    first = &(st->first);
    ivp2h = &(st->ivp2h);
    iovoic = &(st->iovoic);
    iavgp = &(st->iavgp);
    erate = &(st->erate);
    drc = &(st->drc[0]);
    dpit = &(st->dpit[0]);
    drms = &(st->drms[0]);

    i4 = detau_10064014[*ipitv];
    if (! contrl_1.corrp) {
        voice[1] = 1;
        voice[2] = 1;
        if (*ipitv <= 1) {
            voice[1] = 0;
        }
        if (*ipitv == 0 || *ipitv == 2) {
            voice[2] = 0;
        }
        *pitch = i4;
        if (*pitch <= 4) {
            *pitch = *iptold;
        }
        if (voice[1] == 1 && voice[2] == 1) {
            *iptold = *pitch;
        }
        if (voice[1] != voice[2]) {
            *pitch = *iptold;
        }
        goto L900;
    }
    if (i4 > 4) {
        dpit[0] = i4;
        ivoic = 2;
        *iavgp = (*iavgp * 15 + i4 + 8) / 16;
    } else {
        ivoic = i4;
        dpit[0] = *iavgp;
    }
    drms[0] = *irms;
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        drc[i__ * 3 - 3] = irc[i__];
    }
    index = (*ivp2h << 4) + (*iovoic << 2) + ivoic + 1;
    i1 = ivtab_10063f14[index - 1];
    ipit = i1 & 3;
    icorf = i1 / 8;
    if (*erate < ethrs_10063f04) {
        icorf /= 64;
    }
    ixcor = 4;
    if (*erate < ethrs3_10063f10) {
        ixcor = 3;
    }
    if (*erate < ethrs2_10063f0c) {
        ixcor = 2;
    }
    if (*erate < ethrs1_10063f08) {
        ixcor = 1;
    }
    voice[1] = icorf / 2 & 1;
    voice[2] = icorf & 1;
    if (*first) {
        *first = FALSE_;
        *pitch = i4;
        if (*pitch <= 4) {
            *pitch = *iptold;
        }
        goto L500;
    }
    if ((icorf & bit_10064444[3]) != 0) {
        errcnt = 0;
        lsb = drms[1] & 1;
        index = (drc[22] << 4) + drms[1] / 2;
        ham84_(&index, &iout, &errcnt);
        drms[1] = drms[2];
        if (iout >= 0) {
            drms[1] = (iout << 1) + lsb;
        }
        for (i__ = 1; i__ <= 4; ++i__) {
            if (i__ == 1) {
                i1 = ((drc[25] & 7) << 1) + (drc[28] & 1);
            } else {
                i1 = drc[(9 - i__) * 3 - 2] & 15;
            }
            i2 = drc[(5 - i__) * 3 - 2] & 31;
            lsb = i2 & 1;
            index = (i1 << 4) + i2 / 2;
            ham84_(&index, &iout, &errcnt);
            if (iout >= 0) {
                iout = (iout << 1) + lsb;
                if ((iout & 16) == 16) {
                    iout += -32;
                }
            } else {
                iout = drc[(5 - i__) * 3 - 1];
            }
            drc[(5 - i__) * 3 - 2] = iout;
        }
        *erate = *erate * .96875f + errcnt * 102;
    }
    *irms = drms[1];
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        irc[i__] = drc[i__ * 3 - 2];
    }
    if (ipit == 1) {
        dpit[1] = dpit[2];
    }
    if (ipit == 3) {
        dpit[1] = dpit[0];
    }
    *pitch = dpit[1];
    if ((icorf & bit_10064444[1]) != 0) {
        if ((i__1 = drms[1] - drms[0], (real) abs(i__1)) >= corth_10063f94[ixcor + 3] &&
                (i__2 = drms[1] - drms[2], (real) abs(i__2)) >= corth_10063f94[ixcor + 3]) {
            *irms = median_(&drms[2], &drms[1], drms);
        }
        for (i__ = 1; i__ <= 6; ++i__) {
            if ((i__1 = drc[i__ * 3 - 2] - drc[i__ * 3 - 3], (real) abs(i__1)) >=
                    corth_10063f94[ixcor + (i__ + 2 << 2) - 5] &&
                    (i__2 = drc[i__ * 3 - 2] - drc[i__ * 3 - 1], (real) abs(i__2)) >=
                    corth_10063f94[ixcor + (i__ + 2 << 2) - 5]) {
                irc[i__] = median_(&drc[i__ * 3 - 1], &drc[i__ * 3 - 2], &drc[i__ * 3 - 3]);
            }
        }
    }
    if ((icorf & bit_10064444[2]) != 0) {
        if ((i__1 = dpit[1] - dpit[0], (real) abs(i__1)) >= corth_10063f94[ixcor - 1] &&
                (i__2 = dpit[1] - dpit[2], (real) abs(i__2)) >= corth_10063f94[ixcor - 1]) {
            *pitch = median_(&dpit[2], &dpit[1], dpit);
        }
    }
L500:
    if ((icorf & bit_10064444[4]) != 0) {
        i__1 = contrl_1.order;
        for (i__ = 5; i__ <= i__1; ++i__) {
            irc[i__] = zrc_1006441c[i__ - 1];
        }
    }
    *iovoic = ivoic;
    *ivp2h = voice[2];
    dpit[2] = dpit[1];
    dpit[1] = dpit[0];
    drms[2] = drms[1];
    drms[1] = drms[0];
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        drc[i__ * 3 - 1] = drc[i__ * 3 - 2];
        drc[i__ * 3 - 2] = drc[i__ * 3 - 3];
    }
L900:
    *irms = rmst_10064214[(31 - *irms) * 2];
    for (i__ = 1; i__ <= 2; ++i__) {
        i2 = irc[i__];
        i1 = 0;
        if (i2 < 0) {
            i1 = 1;
            i2 = -i2;
            if (i2 > 15) {
                i2 = 0;
            }
        }
        i2 = detab7_10064314[i2 * 2];
        if (i1 == 1) {
            i2 = -i2;
        }
        ishift = 15 - nbit_100643f4[i__ - 1];
        irc[i__] = i2 * pow_ii(&c__2_10063f00, &ishift);
    }
    i__1 = contrl_1.order;
    for (i__ = 3; i__ <= i__1; ++i__) {
        i2 = irc[i__];
        ishift = 15 - nbit_100643f4[i__ - 1];
        i2 *= pow_ii(&c__2_10063f00, &ishift);
        i2 += qb_100643d4[i__ - 3];
        irc[i__] = i2 * descl_10064394[i__ - 3] + deadd_100643b4[i__ - 3];
    }
    *rms = (real) (*irms);
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        rc[i__] = irc[i__] / 16384.f;
    }
    return 0;
}

extern integer c__10_100648e8, c__181_100648ec, c__720_100648f0, c__3_100648f4, c__90_100648f8;
extern integer c__156_100648fc, c__307_10064900, c__462_10064904, c__312_10064908, c__60_1006490c;
extern integer c__1_10064910;
extern integer tau_10064914[60];
extern integer buflim_10064a04[4];
extern real precoef_10064a14;

int preemp_(real *inbuf, real *pebuf, integer *nsamp, real *coef, real *z__);          /* 0x10040da0 */
int onset_(real *pebuf, integer *osbuf, integer *osptr, integer *oslen, integer *sbufl,
           integer *sbufh, integer *lframe, struct lpc10_encoder_state *st);           /* 0x10040ab0 */
int placev_(integer *osbuf, integer *osptr, integer *oslen, integer *obound, integer *vwin,
            integer *af, integer *lframe, integer *minwin, integer *maxwin, integer *dvwinl,
            integer *dvwinh);                                                           /* 0x10040740 */
int lpfilt_(real *inbuf, real *lpbuf, integer *len, integer *nsamp);                   /* 0x100404d0 */
int ivfilt_(real *lpbuf, real *ivbuf, integer *len, integer *nsamp, real *ivrc);       /* 0x10040330 */
int tbdm_(real *speech, integer *lpita, integer *tau, integer *ltau, real *amdf,
          integer *minptr, integer *maxptr, integer *mintau);                          /* 0x10040000 */
int voicin_(integer *vwin, real *inbuf, real *lpbuf, integer *buflim, integer *half,
            real *minamd, real *maxamd, integer *mintau, real *ivrc, integer *obound,
            integer *voibuf, integer *af, struct lpc10_encoder_state *st);             /* 0x1003f720 */
int dyptrk_(real *amdf, integer *ltau, integer *minptr, integer *voice, integer *pitch,
            integer *midx, struct lpc10_encoder_state *st);                            /* 0x1003f310 */
int placea_(integer *ipitch, integer *voibuf, integer *obound, integer *af, integer *vwin,
            integer *awin, integer *ewin, integer *lframe, integer *maxwin);           /* 0x1003edc0 */
int dcbias_(integer *len, real *speech, real *sigout);                                 /* 0x1003ed00 */
int energy_(integer *len, real *speech, real *rms);                                    /* 0x1003ec60 */
int mload_(integer *order, integer *awins, integer *awinf, real *speech, real *phi,
           real *psi);                                                                  /* 0x1003e9f0 */
int invert_(integer *order, real *phi, real *psi, real *rc);                           /* 0x1003e6d0 */

// MATCH: sound.dll 0x1003c640 _analys_
int analys_(real *speech, integer *voice, integer *pitch, real *rms, real *rc,
            struct lpc10_encoder_state *st)
{
    /* System generated locals */
    integer i__1;

    /* Local variables */
    real amdf[60];
    integer half;
    real abuf[156];
    real *bias;
    integer *awin;
    integer midx, ewin[6];
    real ivrc[2], temp;
    real *zpre;
    integer *vwin;
    integer i__, j, lanal;
    real *inbuf, *pebuf;
    real *lpbuf, *ivbuf;
    real *rcbuf;
    integer *osbuf;
    integer *osptr;
    integer ipitch;
    integer *obound;
    integer *voibuf;
    integer mintau;
    real *rmsbuf;
    integer minptr, maxptr;
    real phi[100], psi[10];

    /* Parameter adjustments */
    if (speech) {
        --speech;
    }
    if (voice) {
        --voice;
    }
    if (rc) {
        --rc;
    }

    /* Function Body */
    inbuf = &(st->inbuf[0]);
    pebuf = &(st->pebuf[0]);
    lpbuf = &(st->lpbuf[0]);
    ivbuf = &(st->ivbuf[0]);
    bias = &(st->bias);
    osbuf = &(st->osbuf[0]);
    osptr = &(st->osptr);
    obound = &(st->obound[0]);
    vwin = &(st->vwin[0]);
    awin = &(st->awin[0]);
    voibuf = &(st->voibuf[0]);
    rmsbuf = &(st->rmsbuf[0]);
    rcbuf = &(st->rcbuf[0]);
    zpre = &(st->zpre);

    i__1 = 720 - contrl_1.lframe;
    for (i__ = 181; i__ <= i__1; ++i__) {
        inbuf[i__ - 181] = inbuf[contrl_1.lframe + i__ - 181];
        pebuf[i__ - 181] = pebuf[contrl_1.lframe + i__ - 181];
    }
    i__1 = 540 - contrl_1.lframe;
    for (i__ = 229; i__ <= i__1; ++i__) {
        ivbuf[i__ - 229] = ivbuf[contrl_1.lframe + i__ - 229];
    }
    i__1 = 720 - contrl_1.lframe;
    for (i__ = 25; i__ <= i__1; ++i__) {
        lpbuf[i__ - 25] = lpbuf[contrl_1.lframe + i__ - 25];
    }
    j = 1;
    i__1 = *osptr - 1;
    for (i__ = 1; i__ <= i__1; ++i__) {
        if (osbuf[i__ - 1] > contrl_1.lframe) {
            osbuf[j - 1] = osbuf[i__ - 1] - contrl_1.lframe;
            ++j;
        }
    }
    *osptr = j;
    voibuf[0] = voibuf[2];
    voibuf[1] = voibuf[3];
    for (i__ = 1; i__ <= 2; ++i__) {
        vwin[(i__ << 1) - 2] = vwin[(i__ + 1 << 1) - 2] - contrl_1.lframe;
        vwin[(i__ << 1) - 1] = vwin[(i__ + 1 << 1) - 1] - contrl_1.lframe;
        awin[(i__ << 1) - 2] = awin[(i__ + 1 << 1) - 2] - contrl_1.lframe;
        awin[(i__ << 1) - 1] = awin[(i__ + 1 << 1) - 1] - contrl_1.lframe;
        obound[i__ - 1] = obound[i__];
        voibuf[i__ * 2] = voibuf[(i__ + 1) * 2];
        voibuf[i__ * 2 + 1] = voibuf[(i__ + 1) * 2 + 1];
        rmsbuf[i__ - 1] = rmsbuf[i__];
        i__1 = contrl_1.order;
        for (j = 1; j <= i__1; ++j) {
            rcbuf[j + i__ * 10 - 11] = rcbuf[j + (i__ + 1) * 10 - 11];
        }
    }
    temp = 0.f;
    i__1 = contrl_1.lframe;
    for (i__ = 1; i__ <= i__1; ++i__) {
        inbuf[720 - contrl_1.lframe + i__ - 181] = speech[i__] * 4096.f - *bias;
        temp += inbuf[720 - contrl_1.lframe + i__ - 181];
    }
    if (temp > (real) contrl_1.lframe) {
        *bias += 1;
    }
    if (temp < (real) (-contrl_1.lframe)) {
        *bias += -1;
    }
    i__ = 721 - contrl_1.lframe;
    preemp_(&inbuf[i__ - 181], &pebuf[i__ - 181], &contrl_1.lframe, &precoef_10064a14, zpre);
    onset_(pebuf, osbuf, osptr, &c__10_100648e8, &c__181_100648ec, &c__720_100648f0,
           &contrl_1.lframe, st);
    placev_(osbuf, osptr, &c__10_100648e8, &obound[2], vwin, &c__3_100648f4, &contrl_1.lframe,
            &c__90_100648f8, &c__156_100648fc, &c__307_10064900, &c__462_10064904);
    lpfilt_(&inbuf[228], &lpbuf[384], &c__312_10064908, &contrl_1.lframe);
    ivfilt_(&lpbuf[204], ivbuf, &c__312_10064908, &contrl_1.lframe, ivrc);
    tbdm_(ivbuf, &c__156_100648fc, tau_10064914, &c__60_1006490c, amdf, &minptr, &maxptr, &mintau);
    for (half = 1; half <= 2; ++half) {
        voicin_(&vwin[4], inbuf, lpbuf, buflim_10064a04, &half, &amdf[minptr - 1],
                &amdf[maxptr - 1], &mintau, ivrc, obound, voibuf, &c__3_100648f4, st);
    }
    dyptrk_(amdf, &c__60_1006490c, &minptr, &voibuf[7], pitch, &midx, st);
    ipitch = tau_10064914[midx - 1];
    placea_(&ipitch, voibuf, &obound[2], &c__3_100648f4, vwin, awin, ewin, &contrl_1.lframe,
            &c__156_100648fc);
    lanal = awin[5] + 1 - awin[4];
    dcbias_(&lanal, &pebuf[awin[4] - 181], abuf);
    i__1 = ewin[5] - ewin[4] + 1;
    energy_(&i__1, &abuf[ewin[4] - awin[4]], &rmsbuf[2]);
    mload_(&contrl_1.order, &c__1_10064910, &lanal, abuf, phi, psi);
    invert_(&contrl_1.order, phi, psi, &rcbuf[20]);
    rcchk_(&contrl_1.order, &rcbuf[10], &rcbuf[20]);
    voice[1] = voibuf[2];
    voice[2] = voibuf[3];
    *rms = rmsbuf[0];
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        rc[i__] = rcbuf[i__ - 1];
    }
    return 0;
}

extern integer bit_10064458[10];  /* read at 0x1003c0f4 as [i*4 + 0x10064454] */
extern integer iblist_10064480[53];     /* read at 0x1003bff1 as [i*4 + 0x1006447c] */

// MATCH: sound.dll 0x1003bf30 _chanwr_0_
integer chanwr_0_(int n__, integer *order, integer *ipitv, integer *irms, integer *irc, integer *ibits,
                  struct lpc10_encoder_state *st)
{
    integer *isync;

    /* System generated locals */
    integer i__1;

    /* Local variables */
    integer itab[13], i__;

    /* Parameter adjustments */
    --irc;
    --ibits;

    /* Function Body */
    switch(n__) {
        case 1: goto L_chanrd;
    }

    isync = &(st->isync);

    itab[0] = *ipitv;
    itab[1] = *irms;
    itab[2] = 0;
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        itab[i__ + 2] = irc[*order + 1 - i__] & 32767;
    }
    for (i__ = 1; i__ <= 53; ++i__) {
        ibits[i__] = itab[iblist_10064480[i__ - 1] - 1] & 1;
        itab[iblist_10064480[i__ - 1] - 1] /= 2;
    }
    ibits[54] = *isync & 1;
    *isync = 1 - *isync;
    return 0;

L_chanrd:
    for (i__ = 1; i__ <= 13; ++i__) {
        itab[i__ - 1] = 0;
    }
    for (i__ = 1; i__ <= 53; ++i__) {
        itab[iblist_10064480[54 - i__ - 1] - 1] = (itab[iblist_10064480[54 - i__ - 1] - 1] << 1)
                + ibits[54 - i__];
    }
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        if ((itab[i__ + 2] & bit_10064458[i__ - 1]) != 0) {
            itab[i__ + 2] -= bit_10064458[i__ - 1] << 1;
        }
    }
    *ipitv = itab[0];
    *irms = itab[1];
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        irc[i__] = itab[*order + 4 - i__ - 1];
    }
    return 0;
}

// MATCH: sound.dll 0x1003d0a0 _deemp_
int deemp_(real *x, integer *n, struct lpc10_decoder_state *st)
{
    real *dei1;
    real *dei2;
    real *deo1;
    real *deo2;
    real *deo3;

    /* System generated locals */
    integer i__1;
    real r__1;

    /* Local variables */
    integer k;
    real dei0;

    /* Parameter adjustments */
    if (x) {
        --x;
    }

    dei1 = &(st->dei1);
    dei2 = &(st->dei2);
    deo1 = &(st->deo1);
    deo2 = &(st->deo2);
    deo3 = &(st->deo3);

    /* Function Body */
    i__1 = *n;
    for (k = 1; k <= i__1; ++k) {
        dei0 = x[k];
        r__1 = x[k] - *dei1 * 1.9998f + *dei2;
        x[k] = r__1 + *deo1 * 2.5f - *deo2 * 2.0925f + *deo3 * .585f;
        *dei2 = *dei1;
        *dei1 = dei0;
        *deo3 = *deo2;
        *deo2 = *deo1;
        *deo1 = x[k];
    }
    return 0;
}

// MATCH: sound.dll 0x1003d8d0 _irc2pc_
int irc2pc_(real *rc, real *pc, integer *order, real *gprime, real *g2pass)
{
    /* System generated locals */
    integer i__1, i__2;

    /* Local variables */
    real temp[10];
    integer i__, j;

    /* Parameter adjustments */
    --pc;
    --rc;

    /* Function Body */
    *g2pass = 1.f;
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
        *g2pass *= 1.f - rc[i__] * rc[i__];
    }
    *g2pass = *gprime * sqrt(*g2pass);
    pc[1] = rc[1];
    i__1 = *order;
    for (i__ = 2; i__ <= i__1; ++i__) {
        i__2 = i__ - 1;
        for (j = 1; j <= i__2; ++j) {
            temp[j - 1] = pc[j] - rc[i__] * pc[i__ - j];
        }
        i__2 = i__ - 1;
        for (j = 1; j <= i__2; ++j) {
            pc[j] = temp[j - 1];
        }
        pc[i__] = rc[i__];
    }
    return 0;
}
