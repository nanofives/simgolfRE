typedef unsigned char U8;
typedef unsigned long U32;

#define VL(o)  (*(U32 *)((U8 *)pwf + (o)))
#define VB(o)  (*(U8  *)((U8 *)pwf + (o)))

unsigned long add_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long a16216s2s_no_pitch(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);

#include "sound.h"
#include "wavefile.h"

#define BP(p)     ((char *)(p))
#define I32(p, o) (*(int *)(BP(p) + (o)))
#define P32(p, o) (*(void **)(BP(p) + (o)))
#define U32(p, o) (*(unsigned int *)(BP(p) + (o)))
#define U8(p, o)  (*(unsigned char *)(BP(p) + (o)))

unsigned long filter_stub(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long a16216s2s_straight(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
as2s16216mbloop:
        movsx   eax, word ptr [esi]
        shl     eax, 10h
        add     dword ptr [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
do_right:
        movsx   eax, word ptr [esi+2]
        shl     eax, 10h
        add     dword ptr [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
next_sample:
        add     esi, 4
        cmp     esi, pend
        jge     eod_condition
        dec     ecx
        jnz     as2s16216mbloop
        jmp     as2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      as2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     as2s16216mbloop
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     as2s16216mbloop
        jmp     as2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     as2s16216mbloop
as2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long a16216s2s_no_pitch(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = ((WaveBuffer *)pwf)->get_left_volume();
        right_volume = ((WaveBuffer *)pwf)->get_right_volume();
        loop_flag = ((WaveBuffer *)pwf)->is_looping() && !((WaveBuffer *)pwf)->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
as2s16216mbloop:
        movsx   eax, word ptr [esi]
        mov     edx, left_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
do_right:
        movsx   eax, word ptr [esi+2]
        mov     edx, right_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
next_sample:
        add     esi, 4
        cmp     esi, pend
        jge     eod_condition
        dec     ecx
        jnz     as2s16216mbloop
        jmp     as2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      as2s16216_done
        inc     loop_count
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     as2s16216mbloop
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     as2s16216mbloop
        jmp     as2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     as2s16216mbloop
as2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long add_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = ((WaveBuffer *)pwf)->get_left_volume();
        right_volume = ((WaveBuffer *)pwf)->get_right_volume();
        pitch = ((WaveBuffer *)pwf)->get_pitch();
        if (pitch == 0x10000) {
            if (left_volume == 0x10000 && right_volume == 0x10000)
                return a16216s2s_straight(ppdst, ppsrc, nsamples, pwf, flags);
            return a16216s2s_no_pitch(ppdst, ppsrc, nsamples, pwf, flags);
        }
        pshifter = ((WaveBuffer *)pwf)->get_shifter();
        loop_flag = ((WaveBuffer *)pwf)->is_looping() && !((WaveBuffer *)pwf)->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = ((WaveBuffer *)pwf)->get_ploop_start();
            pend = ((WaveBuffer *)pwf)->get_ploop_end();
        } else {
            pstart = ((WaveBuffer *)pwf)->get_pstart();
            pend = ((WaveBuffer *)pwf)->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     ebx, pshifter
as2s16216mbloop:
        push    ecx
        movsx   ecx, word ptr [esi]
        lea     edx, [esi+4]
        cmp     edx, pend
        jge     get_start_sample_l
        movsx   eax, word ptr [edx]
do_interpolate_l:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        mov     edx, left_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
        jmp     do_right
get_start_sample_l:
        mov     eax, 0
        cmp     loop_flag, 0
        jz      do_interpolate_l
        mov     eax, pstart
        movsx   eax, word ptr [eax]
        cmp     last_loop_flag, 0
        jz      do_interpolate_l
        mov     loop_flag, 0
        jmp     do_interpolate_l
do_right:
        movsx   ecx, word ptr [esi+2]
        lea     edx, [esi+4]
        cmp     edx, pend
        jge     get_start_sample_r
        movsx   eax, word ptr [edx+2]
do_interpolate_r:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        mov     edx, right_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
        jmp     next_sample
get_start_sample_r:
        mov     eax, 0
        cmp     loop_flag, 0
        jz      do_interpolate_r
        mov     eax, pstart
        movsx   eax, word ptr [eax+2]
        cmp     last_loop_flag, 0
        jz      do_interpolate_r
        mov     loop_flag, 0
        jmp     do_interpolate_r
next_sample:
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        cmp     esi, pend
        jge     eod_condition
        pop     ecx
        dec     ecx
        jnz     as2s16216mbloop
        jmp     as2s16216_done
eod_condition:
        pop     ecx
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      as2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     as2s16216mbloop
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     as2s16216mbloop
        jmp     as2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     as2s16216mbloop
as2s16216_done:
        mov     samples_left, ecx
        mov     eax, [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl add_16216s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return SNDERR_NO_SUPPORT;
}

unsigned long a16216m2s_straight(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }


    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
am2s16216mbloop:
        movsx   eax, word ptr [esi]
        shl     eax, 10h
        add     dword ptr [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
do_right:
        add     dword ptr [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
        jmp     next_sample
next_sample:
        add     esi, 2
        cmp     esi, pend
        jge     eod_condition
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      am2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      cmp_ecx
        mov     loop_flag, 0
cmp_ecx:
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     am2s16216mbloop
am2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long a16216m2s_no_pitch(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume, vol_dec = 0;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        if (pcur_wb->is_fading()) {
            vol_dec = pcur_wb->get_fade_amount() / nsamples;
            if (vol_dec == 0)
                vol_dec = 1;
        }
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
am2s16216mbloop:
        movsx   eax, word ptr [esi]
        push    eax
        mov     edx, left_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
        jmp     do_right
do_right:
        pop     eax
        mov     edx, right_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
        jmp     next_sample
next_sample:
        add     esi, 2
        cmp     esi, pend
        jge     eod_condition
        mov     edx, right_volume
        sub     edx, vol_dec
        jge     fill_rvol
        mov     edx, 0
fill_rvol:
        mov     right_volume, edx
        mov     edx, left_volume
        sub     edx, vol_dec
        jge     fill_lvol
        mov     edx, 0
fill_lvol:
        mov     left_volume, edx
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      am2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     am2s16216mbloop
am2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long add_16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume, vol_dec = 0;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        if (left_volume == 0x10000 && right_volume == 0x10000 && pitch == 0x10000)
            return a16216m2s_straight(ppdst, ppsrc, nsamples, pwf, flags);
        if (((CWaveFile *)pwf)->get_pitch() == 0x10000)
            return a16216m2s_no_pitch(ppdst, ppsrc, nsamples, pwf, flags);
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        if (pcur_wb->is_fading()) {
            vol_dec = pcur_wb->get_fade_amount() / nsamples;
            if (vol_dec == 0)
                vol_dec = 1;
        }
        pitch = pcur_wb->get_pitch();
        if (left_volume == 0x10000 && right_volume == 0x10000 && pitch == 0x10000 && vol_dec == 0)
            return a16216m2s_straight(ppdst, ppsrc, nsamples, pwf, flags);
        if (pcur_wb->get_pitch() == 0x10000)
            return a16216m2s_no_pitch(ppdst, ppsrc, nsamples, pwf, flags);
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     ebx, pshifter
am2s16216mbloop:
        push    ecx
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample
        movsx   eax, word ptr [edx]
do_interpolate:
        movsx   ecx, word ptr [esi]
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        push    eax
        mov     edx, left_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_l
        add     edi, 4
do_right:
        pop     eax
        mov     edx, right_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_r
        add     edi, 4
next_sample:
        mov     eax, pitch
        mov     edx, [ebx+2]
        add     edx, eax
        jc      add_carry
ns2:
        mov     [ebx+2], edx
        shr     edx, 10h
        mov     eax, ppsrc
        shl     edx, 1
        mov     esi, [eax]
        add     esi, edx
        cmp     esi, pend
        jge     eod_condition
        mov     edx, right_volume
        sub     edx, vol_dec
        jge     fill_rvol
        mov     edx, 0
fill_rvol:
        mov     right_volume, edx
        mov     edx, left_volume
        sub     edx, vol_dec
        jge     fill_lvol
        mov     edx, 0
fill_lvol:
        mov     left_volume, edx
        pop     ecx
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
add_carry:
        adc     word ptr [ebx+6], 0
        jmp     ns2
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
        jmp     do_right
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
        jmp     next_sample
get_start_sample:
        mov     eax, 0
        cmp     loop_flag, 0
        jz      do_interpolate
        mov     eax, pstart
        movsx   eax, word ptr [eax]
        cmp     last_loop_flag, 0
        jz      do_interpolate
        mov     loop_flag, 0
        jmp     do_interpolate
eod_condition:
        pop     ecx
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      am2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     am2s16216mbloop
am2s16216_done:
        mov     samples_left, ecx
        mov     eax, [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long old_add_16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume, vol_dec = 0;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        if (left_volume == 0x10000 && right_volume == 0x10000 && pitch == 0x10000)
            return a16216m2s_straight(ppdst, ppsrc, nsamples, pwf, flags);
        if (((CWaveFile *)pwf)->get_pitch() == 0x10000)
            return a16216m2s_no_pitch(ppdst, ppsrc, nsamples, pwf, flags);
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        if (pcur_wb->is_fading()) {
            vol_dec = pcur_wb->get_fade_amount() / nsamples;
            if (vol_dec == 0)
                vol_dec = 1;
        }
        pitch = pcur_wb->get_pitch();
        if (left_volume == 0x10000 && right_volume == 0x10000 && pitch == 0x10000 && vol_dec == 0)
            return a16216m2s_straight(ppdst, ppsrc, nsamples, pwf, flags);
        if (pcur_wb->get_pitch() == 0x10000)
            return a16216m2s_no_pitch(ppdst, ppsrc, nsamples, pwf, flags);
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     ebx, pshifter
am2s16216mbloop:
        push    ecx
        movsx   ecx, word ptr [esi]
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample
        movsx   eax, word ptr [edx]
do_interpolate:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        push    eax
        mov     edx, left_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
        jmp     do_right
get_start_sample:
        mov     eax, 0
        cmp     loop_flag, 0
        jz      do_interpolate
        mov     eax, pstart
        movsx   eax, word ptr [eax]
        cmp     last_loop_flag, 0
        jz      do_interpolate
        mov     loop_flag, 0
        jmp     do_interpolate
do_right:
        pop     eax
        mov     edx, right_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
        jmp     next_sample
next_sample:
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 1
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        cmp     esi, pend
        jge     eod_condition
        mov     edx, right_volume
        sub     edx, vol_dec
        jge     fill_rvol
        mov     edx, 0
fill_rvol:
        mov     right_volume, edx
        mov     edx, left_volume
        sub     edx, vol_dec
        jge     fill_lvol
        mov     edx, 0
fill_lvol:
        mov     left_volume, edx
        pop     ecx
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
eod_condition:
        pop     ecx
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      am2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     am2s16216mbloop
am2s16216_done:
        mov     samples_left, ecx
        mov     eax, [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl add_16216m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long __cdecl add_828s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long __cdecl add_828s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long __cdecl add_828m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long __cdecl add_828m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long add_8216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = pcur_wb->get_pitch();
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !((WaveBuffer *)pwf)->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     ebx, pshifter
as2s8216mbloop:
        push    ecx
        movzx   ecx, byte ptr [esi]
        shl     ecx, 8
        sub     ecx, 8000h
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample_l
        movzx   eax, byte ptr [edx]
        shl     eax, 8
        sub     eax, 8000h
do_interpolate_l:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        mov     edx, left_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
        jmp     do_right
get_start_sample_l:
        mov     eax, 0
        cmp     loop_flag, 0
        jz      do_interpolate_l
        mov     eax, pstart
        movzx   eax, byte ptr [eax]
        shl     eax, 8
        sub     eax, 8000h
        jmp     do_interpolate_l
do_right:
        movzx   ecx, byte ptr [esi+1]
        shl     ecx, 8
        sub     ecx, 8000h
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample_r
        movzx   eax, byte ptr [edx+1]
        shl     eax, 8
        sub     eax, 8000h
do_interpolate_r:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        mov     edx, right_volume
        imul    edx
        add     dword ptr [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
        jmp     next_sample
get_start_sample_r:
        mov     eax, 0
        cmp     loop_flag, 0
        jz      do_interpolate_r
        mov     eax, pstart
        movzx   eax, byte ptr [eax+1]
        shl     eax, 8
        sub     eax, 8000h
        jmp     do_interpolate_r
next_sample:
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 1
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        cmp     esi, pend
        jge     eod_condition
        pop     ecx
        dec     ecx
        jnz     as2s8216mbloop
        jmp     as2s8216_done
eod_condition:
        pop     ecx
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      as2s8216_done
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     as2s8216mbloop
        jmp     as2s8216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     as2s8216mbloop
        jmp     as2s8216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     as2s8216mbloop
as2s8216_done:
        mov     samples_left, ecx
        mov     eax, [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        pcur_wb->set_current_position(pcur_wb->get_current_position() + sample_index);
        pcur_wb->set_loop_count(pcur_wb->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl add_8216s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long add_8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0, last_loop_flag = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = pcur_wb->get_pitch();
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     ebx, pshifter
am2s8216mbloop:
        push    ecx
        movzx   ecx, byte ptr [esi]
        shl     ecx, 8
        sub     ecx, 8000h
        lea     edx, [esi+1]
        cmp     edx, pend
        jge     get_start_sample
        movzx   eax, byte ptr [edx]
        shl     eax, 8
        sub     eax, 8000h
do_interpolate:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        push    eax
        mov     edx, left_volume
        imul    edx
        add     [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
        jmp     do_right
get_start_sample:
        mov     eax, 0
        cmp     loop_flag, 0
        je      do_interpolate
        mov     eax, pstart
        movzx   eax, byte ptr [eax]
        shl     eax, 8
        sub     eax, 8000h
        cmp     last_loop_flag, 0
        je      do_interpolate
        mov     loop_flag, 0
        jmp     do_interpolate
do_right:
        pop     eax
        mov     edx, right_volume
        imul    edx
        add     [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
        jmp     next_sample
next_sample:
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        cmp     esi, pend
        jge     eod_condition
        pop     ecx
        dec     ecx
        jnz     am2s8216mbloop
        jmp     am2s8216_done
eod_condition:
        pop     ecx
        dec     ecx
        cmp     link_flag, 0
        jne     set_link
        cmp     loop_flag, 0
        je      am2s8216_done
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jne     am2s8216mbloop
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        je      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jne     am2s8216mbloop
        jmp     am2s8216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jne     am2s8216mbloop
am2s8216_done:
        mov     samples_left, ecx
        mov     eax, [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl add_8216m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long m16216s2s_straight(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
ms2s16216mbloop:
        movsx   eax, word ptr [esi]
        shl     eax, 10h
        mov     [edi], eax
        add     edi, 4
        movsx   eax, word ptr [esi+2]
        shl     eax, 10h
        mov     [edi], eax
        add     edi, 4
        add     esi, 4
        cmp     esi, pend
        jge     eod_condition
        dec     ecx
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      ms2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     ms2s16216mbloop
ms2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long m16216s2s_no_pitch(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
ms2s16216mbloop:
        movsx   eax, word ptr [esi]
        mov     edx, left_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        movsx   eax, word ptr [esi+2]
        mov     edx, right_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        add     esi, 4
        cmp     esi, pend
        jge     eod_condition
        dec     ecx
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      ms2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     ms2s16216mbloop
ms2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long mov_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0, last_loop_flag;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = pcur_wb->get_pitch();
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
        pitch = pcur_wb->get_pitch();
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     ebx, pshifter
ms2s16216mbloop:
        push    ecx
        movsx   ecx, word ptr [esi]
        lea     edx, [esi+4]
        cmp     edx, pend
        jge     get_start_sample_l
        movsx   eax, word ptr [edx]
do_interpolate_l:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        mov     edx, left_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        jmp     do_right
get_start_sample_l:
        mov     eax, 0
        cmp     loop_flag, 0
        je      do_interpolate_l
        mov     eax, pstart
        movsx   eax, word ptr [eax]
        cmp     last_loop_flag, 0
        je      do_interpolate_l
        mov     loop_flag, 0
        jmp     do_interpolate_l
do_right:
        movsx   ecx, word ptr [esi+2]
        lea     edx, [esi+4]
        cmp     edx, pend
        jge     get_start_sample_r
        movsx   eax, word ptr [edx+2]
do_interpolate_r:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        mov     edx, right_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        jmp     next_sample
get_start_sample_r:
        mov     eax, 0
        cmp     loop_flag, 0
        je      do_interpolate_r
        mov     eax, pstart
        movsx   eax, word ptr [eax+2]
        cmp     last_loop_flag, 0
        je      do_interpolate_r
        mov     loop_flag, 0
        jmp     do_interpolate_r
next_sample:
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        cmp     esi, pend
        jge     eod_condition
        pop     ecx
        dec     ecx
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
eod_condition:
        pop     ecx
        dec     ecx
        cmp     link_flag, 0
        jne     set_link
        cmp     loop_flag, 0
        je      ms2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jne     ms2s16216mbloop
        jmp     ms2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        je      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jne     ms2s16216mbloop
        jmp     ms2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jne     ms2s16216mbloop
ms2s16216_done:
        mov     samples_left, ecx
        mov     eax, [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl mov_16216s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return SNDERR_NO_SUPPORT;
}

unsigned long m16216m2s_straight(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }


    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
am2s16216mbloop:
        movsx   eax, word ptr [esi]
        shl     eax, 10h
        mov     [edi], eax
        add     edi, 4
        mov     [edi], eax
        add     edi, 4
        add     esi, 2
        cmp     esi, pend
        jge     eod_condition
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      am2s16216_done
        inc     loop_count
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      cmp_ecx
        mov     loop_flag, 0
cmp_ecx:
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
set_link:
        inc     loop_count
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     am2s16216mbloop
am2s16216_done:
        mov     samples_left, ecx
        mov     sample_index, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }


    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long m16216m2s_no_pitch(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    unsigned long vol_dec = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        if (pcur_wb->is_fading()) {
            vol_dec = pcur_wb->get_fade_amount() / nsamples;
            if (vol_dec == 1)
                vol_dec = 1;
        }
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
ms2s16216mbloop:
        movsx   eax, word ptr [esi]
        push    eax
        mov     edx, left_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        pop     eax
        mov     edx, right_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        add     esi, 2
        cmp     esi, pend
        jge     eod_condition
        mov     edx, right_volume
        sub     edx, vol_dec
        jge     fill_rvol
        mov     edx, 0
fill_rvol:
        mov     right_volume, edx
        mov     edx, left_volume
        sub     edx, vol_dec
        jge     fill_lvol
        mov     edx, 0
fill_lvol:
        mov     left_volume, edx
        dec     ecx
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      ms2s16216_done
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
set_link:
        inc     loop_count
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     ms2s16216mbloop
ms2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long mov_16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    unsigned long vol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        if (((CWaveFile *)pwf)->get_left_volume() == 0x10000 && ((CWaveFile *)pwf)->get_right_volume() == 0x10000
            && ((CWaveFile *)pwf)->get_pitch() == 0x10000)
            return m16216m2s_straight(ppdst, ppsrc, nsamples, pwf, flags);
        if (((CWaveFile *)pwf)->get_pitch() == 0x10000)
            return m16216m2s_no_pitch(ppdst, ppsrc, nsamples, pwf, flags);
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = pcur_wb->get_pitch();
        if (pcur_wb->get_left_volume() == 0x10000 && pcur_wb->get_right_volume() == 0x10000
            && pcur_wb->get_pitch() == 0x10000 && !pcur_wb->is_fading())
            return m16216m2s_straight(ppdst, ppsrc, nsamples, pwf, flags);
        if (pcur_wb->get_pitch() == 0x10000)
            return m16216m2s_no_pitch(ppdst, ppsrc, nsamples, pwf, flags);
        if (pcur_wb->is_fading()) {
            vol_dec = pcur_wb->get_fade_amount() / nsamples;
            if (vol_dec == 1)
                vol_dec = 1;
        }
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     ebx, pshifter
ms2s16216mbloop:
        push    ecx
        movsx   ecx, word ptr [esi]
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample
        movsx   eax, word ptr [edx]
do_interpolate:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        push    eax
        mov     edx, left_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        jmp     do_right
get_start_sample:
        mov     eax, 0
        cmp     loop_flag, 0
        jz      do_interpolate
        mov     eax, pstart
        movsx   eax, word ptr [eax]
        cmp     last_loop_flag, 0
        jz      do_interpolate
        mov     loop_flag, 0
        jmp     do_interpolate
do_right:
        pop     eax
        mov     edx, right_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        jmp     next_sample
next_sample:
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 1
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        cmp     esi, pend
        jge     eod_condition
        mov     edx, right_volume
        sub     edx, vol_dec
        jge     fill_rvol
        mov     edx, 0
fill_rvol:
        mov     right_volume, edx
        mov     edx, left_volume
        sub     edx, vol_dec
        jge     fill_lvol
        mov     edx, 0
fill_lvol:
        mov     left_volume, edx
        pop     ecx
        dec     ecx
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
eod_condition:
        pop     ecx
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      ms2s16216_done
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     ms2s16216mbloop
        jmp     ms2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     ms2s16216mbloop
ms2s16216_done:
        mov     samples_left, ecx
        mov     eax, [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl mov_16216m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long __cdecl mov_828s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long __cdecl mov_828s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long __cdecl mov_828m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long __cdecl mov_828m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long mov_8216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = pcur_wb->get_pitch();
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     ebx, pshifter
ms2s8216mbloop:
        push    ecx
        movzx   ecx, byte ptr [esi]
        shl     ecx, 8
        sub     ecx, 8000h
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample_l
        movzx   eax, byte ptr [edx]
        shl     eax, 8
        sub     eax, 8000h
do_interpolate_l:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        mov     edx, left_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        jmp     do_right
get_start_sample_l:
        mov     eax, 0
        cmp     loop_flag, 0
        je      do_interpolate_l
        mov     eax, pstart
        movzx   eax, byte ptr [eax]
        shl     eax, 8
        sub     eax, 8000h
        jmp     do_interpolate_l
do_right:
        movzx   ecx, byte ptr [esi+1]
        shl     ecx, 8
        sub     ecx, 8000h
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample_r
        movzx   eax, byte ptr [edx+1]
        shl     eax, 8
        sub     eax, 8000h
do_interpolate_r:
        sub     eax, ecx
        movzx   edx, word ptr [ebx+2]
        imul    edx
        sar     eax, 10h
        add     eax, ecx
        mov     edx, right_volume
        imul    edx
        mov     [edi], eax
        add     edi, 4
        jmp     next_sample
get_start_sample_r:
        mov     eax, 0
        cmp     loop_flag, 0
        je      do_interpolate_r
        mov     eax, pstart
        movzx   eax, byte ptr [eax+1]
        shl     eax, 8
        sub     eax, 8000h
        jmp     do_interpolate_r
next_sample:
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 1
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        cmp     esi, pend
        jge     eod_condition
        pop     ecx
        dec     ecx
        jnz     ms2s8216mbloop
        jmp     ms2s8216_done
eod_condition:
        pop     ecx
        dec     ecx
        cmp     link_flag, 0
        jne     set_link
        cmp     loop_flag, 0
        je      ms2s8216_done
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jne     ms2s8216mbloop
        jmp     ms2s8216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     dword ptr [ebx+4], 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        je      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jne     ms2s8216mbloop
        jmp     ms2s8216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jne     ms2s8216mbloop
ms2s8216_done:
        mov     samples_left, ecx
        mov     eax, [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl mov_8216s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long mov_8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        if (pend == ((CWaveFile *)pwf)->get_pend_write())
            loop_flag = 1;
        else
            loop_flag = 0;
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = pcur_wb->get_pitch();
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov  ecx, nsamples
        mov  edx, ppsrc
        mov  esi, [edx]
        mov  edx, ppdst
        mov  edi, [edx]
        mov  ebx, pshifter
    mm2s8216mbloop:
        push ecx
        movzx ecx, byte ptr [esi]
        shl  ecx, 8
        sub  ecx, 8000h
        lea  edx, [esi+1]
        cmp  edx, pend
        jge  get_start_sample
        movzx eax, byte ptr [edx]
        shl  eax, 8
        sub  eax, 8000h
    do_interpolate:
        sub  eax, ecx
        movzx edx, word ptr [ebx+2]
        imul edx
        sar  eax, 16
        add  eax, ecx
        push eax
        mov  edx, left_volume
        imul edx
        mov  dword ptr [edi], eax
        add  edi, 4
        jmp  do_right
    get_start_sample:
        mov  eax, 0
        cmp  loop_flag, 0
        jz   do_interpolate
        mov  eax, pstart
        movzx eax, byte ptr [eax]
        shl  eax, 8
        sub  eax, 8000h
        cmp  last_loop_flag, 0
        jz   do_interpolate
        mov  loop_flag, 0
        jmp  do_interpolate
    do_right:
        pop  eax
        mov  edx, right_volume
        imul edx
        mov  dword ptr [edi], eax
        add  edi, 4
        mov  eax, pitch
        add  dword ptr [ebx+2], eax
        adc  word ptr [ebx+6], 0
        mov  edx, [ebx+4]
        mov  eax, ppsrc
        mov  esi, [eax]
        add  esi, edx
        cmp  esi, pend
        jge  eod_condition
        pop  ecx
        dec  ecx
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    eod_condition:
        pop  ecx
        dec  ecx
        cmp  link_flag, 0
        jnz  set_link
        cmp  loop_flag, 0
        jz   mm2s8216_done
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        cmp  ecx, 0
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    set_link:
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        mov  eax, plinked_wb
        mov  edx, [eax+0d4h]
        mov  pend, edx
        mov  edx, [eax+0b4h]
        mov  plinked_wb, edx
        cmp  edx, 0
        jz   last_link
        mov  eax, [edx+0d8h]
        mov  pstart, eax
        cmp  ecx, 0
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    last_link:
        mov  link_flag, 0
        mov  loop_flag, 0
        mov  edx, [eax+0d8h]
        cmp  ecx, 0
        jnz  mm2s8216mbloop
    mm2s8216_done:
        mov  samples_left, ecx
        mov  eax, [ebx+4]
        mov  sample_index, eax
        mov  dword ptr [ebx+4], 0
        mov  edx, ppsrc
        mov  [edx], esi
        mov  edx, ppdst
        mov  [edx], edi
        pop  edx
        pop  ecx
        pop  ebx
        pop  edi
        pop  esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl mov_8216m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    return 0;
}

unsigned long add_11k222k8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = 0x8000;
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov  ecx, nsamples
        mov  edx, ppsrc
        mov  esi, [edx]
        mov  edx, ppdst
        mov  edi, [edx]
        mov  ebx, pshifter
    am2s8216mbloop:
        push ecx
        movzx ecx, byte ptr [esi]
        shl  ecx, 8
        sub  ecx, 8000h
        lea  edx, [esi+1]
        cmp  edx, pend
        jge  get_start_sample
        movzx eax, byte ptr [edx]
        shl  eax, 8
        sub  eax, 8000h
    do_interpolate:
        sub  eax, ecx
        movzx edx, word ptr [ebx+2]
        imul edx
        sar  eax, 16
        add  eax, ecx
        push eax
        mov  edx, left_volume
        imul edx
        add  dword ptr [edi], eax
        jo   clip_l
        add  edi, 4
        jmp  do_right
    clip_l:
        jc   clip_neg_l
        mov  eax, 7fffffffh
        mov  dword ptr [edi], eax
        jmp  clip_l_done
    clip_neg_l:
        mov  eax, 80000000h
        mov  dword ptr [edi], eax
    clip_l_done:
        add  edi, 4
        jmp  do_right
    get_start_sample:
        mov  eax, 0
        cmp  loop_flag, 0
        jz   do_interpolate
        mov  eax, pstart
        movzx eax, byte ptr [eax]
        shl  eax, 8
        sub  eax, 8000h
        jmp  do_interpolate
    do_right:
        pop  eax
        mov  edx, right_volume
        imul edx
        add  dword ptr [edi], eax
        jo   clip_r
        add  edi, 4
        jmp  next_sample
    clip_r:
        jc   clip_neg_r
        mov  eax, 7fffffffh
        mov  dword ptr [edi], eax
        jmp  clip_r_done
    clip_neg_r:
        mov  eax, 80000000h
        mov  dword ptr [edi], eax
    clip_r_done:
        add  edi, 4
        jmp  next_sample
    next_sample:
        mov  eax, pitch
        add  dword ptr [ebx+2], eax
        adc  word ptr [ebx+6], 0
        mov  edx, [ebx+4]
        mov  eax, ppsrc
        mov  esi, [eax]
        add  esi, edx
        cmp  esi, pend
        jge  eod_condition
        pop  ecx
        dec  ecx
        jnz  am2s8216mbloop
        jmp  am2s8216_done
    eod_condition:
        pop  ecx
        dec  ecx
        cmp  link_flag, 0
        jnz  set_link
        cmp  loop_flag, 0
        jz   am2s8216_done
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        cmp  ecx, 0
        jnz  am2s8216mbloop
    set_link:
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        mov  eax, plinked_wb
        mov  edx, [eax+0d4h]
        mov  pend, edx
        mov  edx, [eax+0b4h]
        mov  plinked_wb, edx
        cmp  edx, 0
        jz   last_link
        mov  eax, [edx+0d8h]
        mov  pstart, eax
        cmp  ecx, 0
        jnz  am2s8216mbloop
        jmp  am2s8216_done
    last_link:
        mov  link_flag, 0
        mov  loop_flag, 0
        mov  edx, [eax+0d8h]
        cmp  ecx, 0
        jnz  am2s8216mbloop
    am2s8216_done:
        mov  samples_left, ecx
        mov  eax, [ebx+4]
        mov  sample_index, eax
        mov  dword ptr [ebx+4], 0
        mov  edx, ppsrc
        mov  [edx], esi
        mov  edx, ppdst
        mov  [edx], edi
        pop  edx
        pop  ecx
        pop  ebx
        pop  edi
        pop  esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long mov_11k222k8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = 0x8000;
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov  ecx, nsamples
        mov  edx, ppsrc
        mov  esi, [edx]
        mov  edx, ppdst
        mov  edi, [edx]
        mov  ebx, pshifter
    mm2s8216mbloop:
        push ecx
        movzx ecx, byte ptr [esi]
        shl  ecx, 8
        sub  ecx, 8000h
        lea  edx, [esi+1]
        cmp  edx, pend
        jge  get_start_sample
        movzx eax, byte ptr [edx]
        shl  eax, 8
        sub  eax, 8000h
    do_interpolate:
        sub  eax, ecx
        movzx edx, word ptr [ebx+2]
        imul edx
        sar  eax, 16
        add  eax, ecx
        push eax
        mov  edx, left_volume
        imul edx
        mov  dword ptr [edi], eax
        add  edi, 4
        jmp  do_right
    get_start_sample:
        mov  eax, 0
        cmp  loop_flag, 0
        jz   do_interpolate
        mov  eax, pstart
        movzx eax, byte ptr [eax]
        shl  eax, 8
        sub  eax, 8000h
        jmp  do_interpolate
    do_right:
        pop  eax
        mov  edx, right_volume
        imul edx
        mov  dword ptr [edi], eax
        add  edi, 4
        mov  eax, pitch
        add  dword ptr [ebx+2], eax
        adc  word ptr [ebx+6], 0
        mov  edx, [ebx+4]
        mov  eax, ppsrc
        mov  esi, [eax]
        add  esi, edx
        cmp  esi, pend
        jge  eod_condition
        pop  ecx
        dec  ecx
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    eod_condition:
        pop  ecx
        dec  ecx
        cmp  link_flag, 0
        jnz  set_link
        cmp  loop_flag, 0
        jz   mm2s8216_done
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        cmp  ecx, 0
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    set_link:
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        mov  eax, plinked_wb
        mov  edx, [eax+0d4h]
        mov  pend, edx
        mov  edx, [eax+0b4h]
        mov  plinked_wb, edx
        cmp  edx, 0
        jz   last_link
        mov  eax, [edx+0d8h]
        mov  pstart, eax
        cmp  ecx, 0
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    last_link:
        mov  link_flag, 0
        mov  loop_flag, 0
        mov  edx, [eax+0d8h]
        cmp  ecx, 0
        jnz  mm2s8216mbloop
    mm2s8216_done:
        mov  samples_left, ecx
        mov  eax, [ebx+4]
        mov  sample_index, eax
        mov  dword ptr [ebx+4], 0
        mov  edx, ppsrc
        mov  [edx], esi
        mov  edx, ppdst
        mov  [edx], edi
        pop  edx
        pop  ecx
        pop  ebx
        pop  edi
        pop  esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long a11k16216m2s_straight(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     edx, 0
am2s16216mbloop:
        movsx   eax, word ptr [esi]
        shl     eax, 10h
        add     [edi], eax
        jo      clip_l
        add     edi, 4
        jmp     do_right
clip_l:
        jc      clip_neg_l
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_l_done
clip_neg_l:
        mov     eax, 80000000h
        mov     [edi], eax
clip_l_done:
        add     edi, 4
do_right:
        add     [edi], eax
        jo      clip_r
        add     edi, 4
        jmp     next_sample
clip_r:
        jc      clip_neg_r
        mov     eax, 7fffffffh
        mov     [edi], eax
        jmp     clip_r_done
clip_neg_r:
        mov     eax, 80000000h
        mov     [edi], eax
clip_r_done:
        add     edi, 4
        inc     edx
        cmp     edx, 2
        jge     next_sample
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
next_sample:
        mov     edx, 0
        add     esi, 2
        cmp     esi, pend
        jge     eod_condition
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      am2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     am2s16216mbloop
am2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long m11k16216m2s_straight(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edx, ppdst
        mov     edi, [edx]
        mov     edx, 0
am2s16216mbloop:
        movsx   eax, word ptr [esi]
        shl     eax, 10h
        mov     [edi], eax
        add     edi, 4
        mov     [edi], eax
        add     edi, 4
        inc     edx
        cmp     edx, 2
        jge     next_sample
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
next_sample:
        mov     edx, 0
        add     esi, 2
        cmp     esi, pend
        jge     eod_condition
        dec     ecx
        jnz     am2s16216mbloop
        jmp     am2s16216_done
eod_condition:
        dec     ecx
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      am2s16216_done
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
set_link:
        inc     loop_count
        mov     sample_index, 0
        mov     eax, ppsrc
        mov     esi, pstart
        mov     [eax], esi
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        cmp     ecx, 0
        jnz     am2s16216mbloop
        jmp     am2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     am2s16216mbloop
am2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    sample_index = nsamples - samples_left;

    if (flags == 1) {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    } else {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long add_8k222k16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = 0x5ce1;
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = 0x5ce1;
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov  ecx, nsamples
        mov  edx, ppsrc
        mov  esi, [edx]
        mov  edx, ppdst
        mov  edi, [edx]
        mov  ebx, pshifter
    am2s16216mbloop:
        push ecx
        movsx ecx, word ptr [esi]
        lea  edx, [esi+2]
        cmp  edx, pend
        jge  get_start_sample
        movsx eax, word ptr [edx]
    do_interpolate:
        sub  eax, ecx
        movzx edx, word ptr [ebx+2]
        imul edx
        sar  eax, 16
        add  eax, ecx
        push eax
        mov  edx, left_volume
        imul edx
        add  dword ptr [edi], eax
        jo   clip_l
        add  edi, 4
        jmp  do_right
    clip_l:
        jc   clip_neg_l
        mov  eax, 7fffffffh
        mov  dword ptr [edi], eax
        jmp  clip_l_done
    clip_neg_l:
        mov  eax, 80000000h
        mov  dword ptr [edi], eax
    clip_l_done:
        add  edi, 4
        jmp  do_right
    get_start_sample:
        mov  eax, 0
        cmp  loop_flag, 0
        jz   do_interpolate
        mov  eax, pstart
        movsx eax, word ptr [eax]
        jmp  do_interpolate
    do_right:
        pop  eax
        mov  edx, right_volume
        imul edx
        add  dword ptr [edi], eax
        jo   clip_r
        add  edi, 4
        jmp  next_sample
    clip_r:
        jc   clip_neg_r
        mov  eax, 7fffffffh
        mov  dword ptr [edi], eax
        jmp  clip_r_done
    clip_neg_r:
        mov  eax, 80000000h
        mov  dword ptr [edi], eax
    clip_r_done:
        add  edi, 4
        jmp  next_sample
    next_sample:
        mov  eax, pitch
        add  dword ptr [ebx+2], eax
        adc  word ptr [ebx+6], 0
        mov  edx, [ebx+4]
        shl  edx, 1
        mov  eax, ppsrc
        mov  esi, [eax]
        add  esi, edx
        cmp  esi, pend
        jge  eod_condition
        pop  ecx
        dec  ecx
        jnz  am2s16216mbloop
        jmp  am2s16216_done
    eod_condition:
        pop  ecx
        dec  ecx
        cmp  link_flag, 0
        jnz  set_link
        cmp  loop_flag, 0
        jz   am2s16216_done
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        cmp  ecx, 0
        jnz  am2s16216mbloop
        jmp  am2s16216_done
    set_link:
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        mov  eax, plinked_wb
        mov  edx, [eax+0d4h]
        mov  pend, edx
        mov  edx, [eax+0b4h]
        mov  plinked_wb, edx
        cmp  edx, 0
        jz   last_link
        mov  eax, [edx+0d8h]
        mov  pstart, eax
        cmp  ecx, 0
        jnz  am2s16216mbloop
        jmp  am2s16216_done
    last_link:
        mov  link_flag, 0
        mov  loop_flag, 0
        mov  edx, [eax+0d8h]
        cmp  ecx, 0
        jnz  am2s16216mbloop
    am2s16216_done:
        mov  samples_left, ecx
        mov  eax, [ebx+4]
        mov  sample_index, eax
        mov  dword ptr [ebx+4], 0
        mov  edx, ppsrc
        mov  [edx], esi
        mov  edx, ppdst
        mov  [edx], edi
        pop  edx
        pop  ecx
        pop  ebx
        pop  edi
        pop  esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl fp_mov_8k16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags)
{
    double fpvol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0, fade_flag = 0;
    char *peod = 0;
    char **ppend_read = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    double fp_shifter, fp_pitch, fp_lvol, fp_rvol;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;
    short *cur_sample_address = (short *)*ppsrc;
    CWaveFile *pwf;
    int index;
    short s2wrap;
    short chop_mode;
    int loop_over;

    if (flags == 1) {
        pwf = (CWaveFile *)pcaller;
        fp_lvol = 2.0;
        fp_rvol = 2.0;
        pend = pwf->get_pend();
        pstart = pwf->get_pstart();
        fp_shifter = *pwf->get_fp_shifter();
        fp_pitch = 8000.0 / 22050.0;
        if (pwf->is_fading()) {
            fpvol_dec = pwf->m_fp_fade_amount / nsamples;
            fade_flag = 1;
        }
        if (pwf->m_flags.last_loop)
            loop_flag = 0;
        else
            loop_flag = 1;
        if (pwf->is_eod()) {
            ppend_read = &pwf->m_pmmioinfo->pchEndRead;
            peod = pwf->m_pwrite;
            last_loop_flag = 1;
        }
    } else {
        pcur_wb = (WaveBuffer *)pcaller;
        fp_lvol = pcur_wb->m_fp_left_volume;
        fp_rvol = pcur_wb->m_fp_right_volume;
        fp_pitch = 8000.0 / 22050.0;
        fp_shifter = pcur_wb->m_fp_shifter;
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    chop_mode = 0xf7f;
    loop_over = 0;

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edi, pdst
        fninit
        fldcw   word ptr chop_mode
        fld     qword ptr fp_rvol
        fld     qword ptr fp_lvol
        fld     qword ptr fp_pitch
        fld     qword ptr fp_shifter
ms2s16216mbloop:
        fadd    st, st(1)
        fld     st(0)
        fist    dword ptr index
        mov     eax, index
        shl     eax, 1
        add     esi, eax
        fisub   dword ptr index
        cmp     esi, pend
        jge     check_loop_s1
        fild    word ptr [esi]
get_s2:
        add     esi, 2
        cmp     esi, pend
        jge     check_loop_s2
        fild    word ptr [esi]
interpolate:
        fsub    st, st(1)
        fmul    st, st(2)
        fadd    st, st(1)
        fxch
        fstp    st(0)
        fld     st(0)
        fmul    st, st(5)
        fxch
        fmul    st, st(6)
        fxch
        fstp    qword ptr [edi]
        fstp    qword ptr [edi+8]
        fstp    st(0)
        dec     ecx
        jz      ms2s16216_done
        cmp     fade_flag, 0
        add     edi, 16
        mov     esi, [edx]
        jnz     do_fade
        jmp     ms2s16216mbloop
do_fade:
        fld     qword ptr fpvol_dec
        fsub    st(3), st
        fsubp   st(4), st
        jmp     ms2s16216mbloop
check_loop_s1:
        cmp     link_flag, 0
        jnz     set_link1
        cmp     loop_flag, 0
        jz      ms2s16216_done
        fxch
        fstp    st(0)
        fld     st(0)
        fist    dword ptr index
        mov     eax, pstart
        mov     [edx], eax
        mov     esi, eax
        fild    word ptr [esi]
        cmp     last_loop_flag, 0
        jz      get_s2
        mov     loop_flag, 0
        mov     eax, ppend_read
        mov     ebx, peod
        mov     [eax], ebx
        jmp     get_s2
set_link1:
        inc     loop_count
        fxch
        fstp    st(0)
        fld     st(0)
        fist    dword ptr index
        mov     esi, pstart
        mov     [edx], esi
        fild    word ptr [esi]
        push    edx
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        pop     edx
        jmp     get_s2
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     eax, [eax+0d8h]
        pop     edx
        jnz     get_s2
        fist    dword ptr index
check_loop_s2:
        mov     s2wrap, 0
        cmp     link_flag, 0
        jnz     wrap_it
        cmp     loop_flag, 0
        jz      get_wrap_s2
wrap_it:
        mov     eax, pstart
        mov     bx, word ptr [eax]
        mov     s2wrap, bx
get_wrap_s2:
        fild    s2wrap
        jmp     interpolate
ms2s16216_done:
        cmp     ecx, 0
        jnz     unfilled
        sub     esi, 2
        fisub   dword ptr index
        fst     qword ptr fp_shifter
unfilled:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        pwf->m_fp_shifter = fp_shifter;
        if (pwf->is_eod() && !loop_flag)
            pwf->m_flags.last_loop = 1;
    }
    else
    {
        pcur_wb->m_fp_shifter = fp_shifter;
        pcur_wb->set_current_position(pcur_wb->get_current_position() + sample_index);
        pcur_wb->set_loop_count(pcur_wb->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl fp_add_8k16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags)
{
    double fpvol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0, fade_flag = 0;
    char *peod = 0;
    char **ppend_read = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    double fp_shifter, fp_pitch, fp_lvol, fp_rvol;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;
    short *cur_sample_address = (short *)*ppsrc;
    CWaveFile *pwf;
    int index;
    short s2wrap;
    short chop_mode;
    int loop_over;

    if (flags == 1) {
        pwf = (CWaveFile *)pcaller;
        fp_lvol = pwf->m_fp_left_volume;
        fp_rvol = pwf->m_fp_right_volume;
        pend = pwf->get_pend();
        pstart = pwf->get_pstart();
        fp_shifter = *pwf->get_fp_shifter();
        fp_pitch = 8000.0 / 22050.0;
        if (pwf->is_fading()) {
            fpvol_dec = pwf->m_fp_fade_amount / nsamples;
            fade_flag = 1;
        }
        if (pwf->m_flags.last_loop)
            loop_flag = 0;
        else
            loop_flag = 1;
        if (pwf->is_eod()) {
            ppend_read = &pwf->m_pmmioinfo->pchEndRead;
            peod = pwf->m_pwrite;
            last_loop_flag = 1;
        }
    } else {
        pcur_wb = (WaveBuffer *)pcaller;
        fp_lvol = 2.0;
        fp_rvol = 2.0;
        fp_pitch = 8000.0 / 22050.0;
        fp_shifter = pcur_wb->m_fp_shifter;
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    chop_mode = 0xf7f;
    loop_over = 0;

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edi, pdst
        fninit
        fldcw   word ptr chop_mode
        fld     qword ptr fp_rvol
        fld     qword ptr fp_lvol
        fld     qword ptr fp_pitch
        fld     qword ptr fp_shifter
ms2s16216mbloop:
        fadd    st, st(1)
        fld     st(0)
        fist    dword ptr index
        mov     eax, index
        shl     eax, 1
        add     esi, eax
        fisub   dword ptr index
        cmp     esi, pend
        jge     check_loop_s1
        fild    word ptr [esi]
get_s2:
        add     esi, 2
        cmp     esi, pend
        jge     check_loop_s2
        fild    word ptr [esi]
interpolate:
        fsub    st, st(1)
        fmul    st, st(2)
        fadd    st, st(1)
        fxch
        fstp    st(0)
        fld     st(0)
        fmul    st, st(5)
        fxch
        fmul    st, st(6)
        fxch
        fadd    qword ptr [edi]
        fxch
        fadd    qword ptr [edi+8]
        fxch
        fstp    qword ptr [edi]
        fstp    qword ptr [edi+8]
        fstp    st(0)
        dec     ecx
        jz      ms2s16216_done
        add     edi, 16
        mov     esi, [edx]
        cmp     fade_flag, 0
        jnz     do_fade
        jmp     ms2s16216mbloop
do_fade:
        fld     qword ptr fpvol_dec
        fsub    st(3), st
        fsubp   st(4), st
        jmp     ms2s16216mbloop
check_loop_s1:
        cmp     link_flag, 0
        jnz     set_link1
        cmp     loop_flag, 0
        jz      ms2s16216_done
        fxch
        fstp    st(0)
        fld     st(0)
        fist    dword ptr index
        mov     eax, pstart
        mov     [edx], eax
        mov     esi, eax
        fild    word ptr [esi]
        cmp     last_loop_flag, 0
        jz      get_s2
        mov     loop_flag, 0
        mov     eax, ppend_read
        mov     ebx, peod
        mov     [eax], ebx
        jmp     get_s2
set_link1:
        inc     loop_count
        fxch
        fstp    st(0)
        fld     st(0)
        fist    dword ptr index
        mov     esi, pstart
        mov     [edx], esi
        fild    word ptr [esi]
        push    edx
        mov     eax, plinked_wb
        mov     edx, [eax+0d4h]
        mov     pend, edx
        mov     edx, [eax+0b4h]
        mov     plinked_wb, edx
        cmp     edx, 0
        jz      last_link
        mov     eax, [edx+0d8h]
        mov     pstart, eax
        pop     edx
        jmp     get_s2
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     eax, [eax+0d8h]
        pop     edx
        jmp     get_s2
        fist    dword ptr index
check_loop_s2:
        mov     s2wrap, 0
        cmp     link_flag, 0
        jnz     wrap_it
        cmp     loop_flag, 0
        jz      get_wrap_s2
wrap_it:
        mov     eax, pstart
        mov     bx, word ptr [eax]
        mov     s2wrap, bx
get_wrap_s2:
        fild    s2wrap
        jmp     interpolate
ms2s16216_done:
        cmp     ecx, 0
        jnz     unfilled
        sub     esi, 2
        fisub   dword ptr index
        fst     qword ptr fp_shifter
unfilled:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        pwf->m_fp_shifter = fp_shifter;
        if (pwf->is_eod() && !loop_flag)
            pwf->m_flags.last_loop = 1;
    }
    else
    {
        pcur_wb->m_fp_shifter = fp_shifter;
        pcur_wb->set_current_position(pcur_wb->get_current_position() + sample_index);
        pcur_wb->set_loop_count(pcur_wb->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long mov_8k222k16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = 0x5ce1;
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = 0x5ce1;
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov  ecx, nsamples
        mov  edx, ppsrc
        mov  esi, [edx]
        mov  edx, ppdst
        mov  edi, [edx]
        mov  ebx, pshifter
    ms2s16216mbloop:
        push ecx
        movsx ecx, word ptr [esi]
        lea  edx, [esi+2]
        cmp  edx, pend
        jge  get_start_sample
        movsx eax, word ptr [edx]
    do_interpolate:
        sub  eax, ecx
        movzx edx, word ptr [ebx+2]
        imul edx
        sar  eax, 16
        add  eax, ecx
        push eax
        mov  edx, left_volume
        imul edx
        sar  eax, 16
        mov  word ptr [edi], ax
        add  edi, 2
        jmp  do_right
    get_start_sample:
        mov  eax, 0
        cmp  loop_flag, 0
        jz   do_interpolate
        mov  eax, pstart
        movsx eax, word ptr [eax]
        jmp  do_interpolate
    do_right:
        pop  eax
        mov  edx, right_volume
        imul edx
        sar  eax, 16
        mov  word ptr [edi], ax
        add  edi, 2
        jmp  next_sample
    next_sample:
        mov  eax, pitch
        add  dword ptr [ebx+2], eax
        adc  word ptr [ebx+6], 0
        mov  edx, [ebx+4]
        shl  edx, 1
        mov  eax, ppsrc
        mov  esi, [eax]
        add  esi, edx
        cmp  esi, pend
        jge  eod_condition
        pop  ecx
        dec  ecx
        jnz  ms2s16216mbloop
        jmp  ms2s16216_done
    eod_condition:
        pop  ecx
        dec  ecx
        cmp  link_flag, 0
        jnz  set_link
        cmp  loop_flag, 0
        jz   ms2s16216_done
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        cmp  ecx, 0
        jnz  ms2s16216mbloop
        jmp  ms2s16216_done
    set_link:
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        mov  eax, plinked_wb
        mov  edx, [eax+0d4h]
        mov  pend, edx
        mov  edx, [eax+0b4h]
        mov  plinked_wb, edx
        cmp  edx, 0
        jz   last_link
        mov  eax, [edx+0d8h]
        mov  pstart, eax
        cmp  ecx, 0
        jnz  ms2s16216mbloop
        jmp  ms2s16216_done
    last_link:
        mov  link_flag, 0
        mov  loop_flag, 0
        mov  edx, [eax+0d8h]
        cmp  ecx, 0
        jnz  ms2s16216mbloop
    ms2s16216_done:
        mov  samples_left, ecx
        mov  eax, [ebx+4]
        mov  sample_index, eax
        mov  dword ptr [ebx+4], 0
        mov  edx, ppsrc
        mov  [edx], esi
        mov  edx, ppdst
        mov  [edx], edi
        pop  edx
        pop  ecx
        pop  ebx
        pop  edi
        pop  esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long a8k222k8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = 0x5ce1;
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = 0x5ce1;
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov  ecx, nsamples
        mov  edx, ppsrc
        mov  esi, [edx]
        mov  edx, ppdst
        mov  edi, [edx]
        mov  ebx, pshifter
    am2s8216mbloop:
        push ecx
        movzx ecx, byte ptr [esi]
        shl  ecx, 8
        sub  ecx, 8000h
        lea  edx, [esi+1]
        cmp  edx, pend
        jge  get_start_sample
        movzx eax, byte ptr [edx]
        shl  eax, 8
        sub  eax, 8000h
    do_interpolate:
        sub  eax, ecx
        movzx edx, word ptr [ebx+2]
        imul edx
        sar  eax, 16
        add  eax, ecx
        push eax
        mov  edx, left_volume
        imul edx
        add  dword ptr [edi], eax
        jo   clip_l
        add  edi, 4
        jmp  do_right
    clip_l:
        jc   clip_neg_l
        mov  eax, 7fffffffh
        mov  dword ptr [edi], eax
        jmp  clip_l_done
    clip_neg_l:
        mov  eax, 80000000h
        mov  dword ptr [edi], eax
    clip_l_done:
        add  edi, 4
        jmp  do_right
    get_start_sample:
        mov  eax, 0
        cmp  loop_flag, 0
        jz   do_interpolate
        mov  eax, pstart
        movzx eax, byte ptr [eax]
        shl  eax, 8
        sub  eax, 8000h
        jmp  do_interpolate
    do_right:
        pop  eax
        mov  edx, right_volume
        imul edx
        add  dword ptr [edi], eax
        jo   clip_r
        add  edi, 4
        jmp  next_sample
    clip_r:
        jc   clip_neg_r
        mov  eax, 7fffffffh
        mov  dword ptr [edi], eax
        jmp  clip_r_done
    clip_neg_r:
        mov  eax, 80000000h
        mov  dword ptr [edi], eax
    clip_r_done:
        add  edi, 4
        jmp  next_sample
    next_sample:
        mov  eax, pitch
        add  dword ptr [ebx+2], eax
        adc  word ptr [ebx+6], 0
        mov  edx, [ebx+4]
        mov  eax, ppsrc
        mov  esi, [eax]
        add  esi, edx
        cmp  esi, pend
        jge  eod_condition
        pop  ecx
        dec  ecx
        jnz  am2s8216mbloop
        jmp  am2s8216_done
    eod_condition:
        pop  ecx
        dec  ecx
        cmp  link_flag, 0
        jnz  set_link
        cmp  loop_flag, 0
        jz   am2s8216_done
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        cmp  ecx, 0
        jnz  am2s8216mbloop
    set_link:
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        mov  eax, plinked_wb
        mov  edx, [eax+0d4h]
        mov  pend, edx
        mov  edx, [eax+0b4h]
        mov  plinked_wb, edx
        cmp  edx, 0
        jz   last_link
        mov  eax, [edx+0d8h]
        mov  pstart, eax
        cmp  ecx, 0
        jnz  am2s8216mbloop
        jmp  am2s8216_done
    last_link:
        mov  link_flag, 0
        mov  loop_flag, 0
        mov  edx, [eax+0d8h]
        cmp  ecx, 0
        jnz  am2s8216mbloop
    am2s8216_done:
        mov  samples_left, ecx
        mov  eax, [ebx+4]
        mov  sample_index, eax
        mov  dword ptr [ebx+4], 0
        mov  edx, ppsrc
        mov  [edx], esi
        mov  edx, ppdst
        mov  [edx], edi
        pop  edx
        pop  ecx
        pop  ebx
        pop  edi
        pop  esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long m8k222k8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume, right_volume;
    SHIFTER *pshifter;
    int loop_flag = 0, link_flag = 0;
    unsigned long loop_count = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb = 0;
    WaveBuffer *pcur_wb = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = 0x5ce1;
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pitch = 0x5ce1;
        pshifter = pcur_wb->get_shifter();
        loop_flag = pcur_wb->is_looping() && !pcur_wb->is_released();
        link_flag = pcur_wb->is_linked();
        if (link_flag) {
            plinked_wb = pcur_wb->get_plinked_wb();
            pstart = plinked_wb->get_pstart();
            linked_pend = plinked_wb->get_pend();
            pend = pcur_wb->get_pend();
        } else if (loop_flag) {
            pstart = pcur_wb->get_ploop_start();
            pend = pcur_wb->get_ploop_end();
        } else {
            pstart = pcur_wb->get_pstart();
            pend = pcur_wb->get_pend();
        }
    }

    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov  ecx, nsamples
        mov  edx, ppsrc
        mov  esi, [edx]
        mov  edx, ppdst
        mov  edi, [edx]
        mov  ebx, pshifter
    mm2s8216mbloop:
        push ecx
        movzx ecx, byte ptr [esi]
        shl  ecx, 8
        sub  ecx, 8000h
        lea  edx, [esi+1]
        cmp  edx, pend
        jge  get_start_sample
        movzx eax, byte ptr [edx]
        shl  eax, 8
        sub  eax, 8000h
    do_interpolate:
        sub  eax, ecx
        movzx edx, word ptr [ebx+2]
        imul edx
        sar  eax, 16
        add  eax, ecx
        push eax
        cmp  eax, 7fffh
        jg   clip1
        cmp  eax, 0ffff8000h
        jl   clip2
        mov  word ptr [edi], ax
        add  edi, 2
        jmp  do_right
    clip1:
        mov  byte ptr [edi], 0ffh
        add  edi, 2
        jmp  do_right
    clip2:
        mov  byte ptr [edi], 0
        add  edi, 2
        jmp  do_right
    get_start_sample:
        mov  eax, 0
        cmp  loop_flag, 0
        jz   do_interpolate
        mov  eax, pstart
        movzx eax, byte ptr [eax]
        shl  eax, 8
        sub  eax, 8000h
        jmp  do_interpolate
    do_right:
        pop  eax
        cmp  eax, 7fffh
        jg   clipr1
        cmp  eax, 0ffff8000h
        jl   clipr2
        mov  word ptr [edi], ax
        add  edi, 2
        jmp  next_sample
    clipr1:
        mov  byte ptr [edi], 0
        add  edi, 2
        jmp  next_sample
    clipr2:
        mov  byte ptr [edi], 0
        add  edi, 2
    next_sample:
        mov  eax, pitch
        add  dword ptr [ebx+2], eax
        adc  word ptr [ebx+6], 0
        mov  edx, [ebx+4]
        mov  eax, ppsrc
        mov  esi, [eax]
        add  esi, edx
        cmp  esi, pend
        jge  eod_condition
        pop  ecx
        dec  ecx
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    eod_condition:
        pop  ecx
        dec  ecx
        cmp  link_flag, 0
        jnz  set_link
        cmp  loop_flag, 0
        jz   mm2s8216_done
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        cmp  ecx, 0
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    set_link:
        inc  loop_count
        mov  sample_index, 0
        mov  dword ptr [ebx+4], 0
        mov  eax, ppsrc
        mov  esi, pstart
        mov  [eax], esi
        mov  eax, plinked_wb
        mov  edx, [eax+0d4h]
        mov  pend, edx
        mov  edx, [eax+0b4h]
        mov  plinked_wb, edx
        cmp  edx, 0
        jz   last_link
        mov  eax, [edx+0d8h]
        mov  pstart, eax
        cmp  ecx, 0
        jnz  mm2s8216mbloop
        jmp  mm2s8216_done
    last_link:
        mov  link_flag, 0
        mov  loop_flag, 0
        mov  edx, [eax+0d8h]
        cmp  ecx, 0
        jnz  mm2s8216mbloop
    mm2s8216_done:
        mov  samples_left, ecx
        mov  eax, [ebx+4]
        mov  sample_index, eax
        mov  dword ptr [ebx+4], 0
        mov  edx, ppsrc
        mov  [edx], esi
        mov  edx, ppdst
        mov  [edx], edi
        pop  edx
        pop  ecx
        pop  ebx
        pop  edi
        pop  esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)->set_current_position(((CWaveFile *)pwf)->get_current_position() + sample_index);
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long __cdecl fp_add_16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags)
{
    double fpvol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0, fade_flag = 0;
    char *peod = 0;
    char **ppend_read = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    double fp_shifter, fp_pitch, fp_lvol, fp_rvol;
    short *cur_sample_address = (short *)*ppsrc;
    CWaveFile *pwf;
    WaveBuffer *pwb;
    int index;
    short s2wrap;
    short chop_mode;
    int loop_over;

    if (flags == 1) {
        pwf = (CWaveFile *)pcaller;
        fp_lvol = pwf->m_fp_left_volume;
        fp_rvol = pwf->m_fp_right_volume;
        pend = pwf->get_pend();
        pstart = pwf->get_pstart();
        fp_shifter = *pwf->get_fp_shifter();
        fp_pitch = *pwf->get_fp_pitch();
        if (pwf->is_fading()) {
            fpvol_dec = pwf->m_fp_fade_amount / nsamples;
            fade_flag = 1;
        }
        if (pwf->m_flags.last_loop)
            loop_flag = 0;
        else
            loop_flag = 1;
        if (pwf->is_eod()) {
            ppend_read = &pwf->m_pmmioinfo->pchEndRead;
            if ((char *)*ppsrc > *ppend_read)
                last_loop_flag = 1;
            else
                loop_flag = 0;
            peod = pwf->m_pwrite;
        }
    } else {
        pwb = (WaveBuffer *)pcaller;
        fp_lvol = pwb->m_fp_left_volume;
        fp_rvol = pwb->m_fp_right_volume;
        fp_pitch = pwb->m_fp_pitch;
        fp_shifter = pwb->m_fp_shifter;
        if (pwb->is_fading()) {
            fpvol_dec = pwb->get_fp_fade_amount() / nsamples;
            fade_flag = 1;
        }
        loop_flag = pwb->is_looping() && !pwb->is_released();
        if (loop_flag) {
            pstart = pwb->get_ploop_start();
            pend = pwb->get_ploop_end();
        } else {
            pstart = pwb->get_pstart();
            pend = pwb->get_pend();
        }
    }

    chop_mode = 0xf7f;
    loop_over = 0;

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edi, pdst
        fninit
        fldcw   word ptr chop_mode
        fld     qword ptr fp_rvol
        fld     qword ptr fp_lvol
        fld     qword ptr fp_pitch
        fld     qword ptr fp_shifter
ms2s16216mbloop:
        fadd    st, st(1)
        fld     st(0)
        fist    dword ptr index
        mov     eax, index
        shl     eax, 1
        add     esi, eax
        fisub   dword ptr index
        cmp     esi, pend
        jge     check_loop_s1
        fild    word ptr [esi]
get_s2:
        add     esi, 2
        cmp     esi, pend
        jge     check_loop_s2
        fild    word ptr [esi]
interpolate:
        fsub    st, st(1)
        fmul    st, st(2)
        fadd    st, st(1)
        fxch
        fstp    st(0)
        fld     st(0)
        fmul    st, st(5)
        fxch
        fmul    st, st(6)
        fxch
        fadd    qword ptr [edi]
        fxch
        fadd    qword ptr [edi+8]
        fxch
        fstp    qword ptr [edi]
        fstp    qword ptr [edi+8]
        fstp    st(0)
        dec     ecx
        jz      ms2s16216_done
        cmp     fade_flag, 0
        add     edi, 16
        mov     esi, [edx]
        jnz     do_fade
        jmp     ms2s16216mbloop
do_fade:
        fld     qword ptr fpvol_dec
        fsub    st(3), st
        fsubp   st(4), st
        jmp     ms2s16216mbloop
check_loop_s1:
        cmp     loop_flag, 0
        jz      ms2s16216_done
        fxch
        fstp    st(0)
        fld     st(0)
        fist    dword ptr index
        mov     eax, pstart
        mov     [edx], eax
        mov     esi, eax
        fild    word ptr [esi]
        cmp     last_loop_flag, 0
        jz      get_s2
        mov     loop_flag, 0
        mov     eax, ppend_read
        mov     ebx, peod
        mov     [eax], ebx
        jmp     get_s2
check_loop_s2:
        mov     s2wrap, 0
        cmp     loop_flag, 0
        jz      get_wrap_s2
        mov     eax, pstart
        mov     bx, word ptr [eax]
        mov     s2wrap, bx
get_wrap_s2:
        fild    s2wrap
        jmp     interpolate
ms2s16216_done:
        cmp     ecx, 0
        jnz     unfilled
        sub     esi, 2
        fisub   dword ptr index
        fst     qword ptr fp_shifter
unfilled:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        pwf->m_fp_shifter = fp_shifter;
        if (pwf->is_eod() && !loop_flag)
            pwf->m_flags.last_loop = 1;
    }
    else
    {
        pwb->m_fp_shifter = fp_shifter;
    }
    return nsamples - samples_left;
}

unsigned long __cdecl fp_mov_16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags)
{
    double fpvol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0, fade_flag = 0;
    char *peod = 0;
    char **ppend_read = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    double fp_shifter, fp_pitch, fp_lvol, fp_rvol;
    short *cur_sample_address = (short *)*ppsrc;
    CWaveFile *pwf;
    WaveBuffer *pwb;
    int index;
    short s2wrap;
    double zero = 0;
    short chop_mode;
    int loop_over;

    if (flags == 1) {
        pwf = (CWaveFile *)pcaller;
        fp_lvol = pwf->m_fp_left_volume;
        fp_rvol = pwf->m_fp_right_volume;
        pend = pwf->get_pend();
        pstart = pwf->get_pstart();
        fp_shifter = *pwf->get_fp_shifter();
        fp_pitch = *pwf->get_fp_pitch();
        if (pwf->is_fading()) {
            fpvol_dec = pwf->m_fp_fade_amount / nsamples;
            fade_flag = 1;
        }
        if (pwf->m_flags.last_loop)
            loop_flag = 0;
        else
            loop_flag = 1;
        if (pwf->is_eod()) {
            ppend_read = &pwf->m_pmmioinfo->pchEndRead;
            if ((char *)*ppsrc > *ppend_read)
                last_loop_flag = 1;
            else
                loop_flag = 0;
            peod = pwf->m_pwrite;
        }
    } else {
        pwb = (WaveBuffer *)pcaller;
        fp_lvol = pwb->m_fp_left_volume;
        fp_rvol = pwb->m_fp_right_volume;
        fp_pitch = pwb->m_fp_pitch;
        fp_shifter = pwb->m_fp_shifter;
        if (pwb->is_fading()) {
            fpvol_dec = pwb->get_fp_fade_amount() / nsamples;
            fade_flag = 1;
        }
        loop_flag = pwb->is_looping() && !pwb->is_released();
        if (loop_flag) {
            pstart = pwb->get_ploop_start();
            pend = pwb->get_ploop_end();
        } else {
            pstart = pwb->get_pstart();
            pend = pwb->get_pend();
        }
    }

    chop_mode = 0xf7f;
    loop_over = 0;

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edi, pdst
        fninit
        fldcw   word ptr chop_mode
        fld     qword ptr fp_rvol
        fld     qword ptr fp_lvol
        fld     qword ptr fp_pitch
        fld     qword ptr fp_shifter
ms2s16216mbloop:
        fadd    st, st(1)
        fld     st(0)
        fist    dword ptr index
        mov     eax, index
        shl     eax, 1
        add     esi, eax
        fisub   dword ptr index
        cmp     esi, pend
        jge     check_loop_s1
        fild    word ptr [esi]
get_s2:
        add     esi, 2
        cmp     esi, pend
        jge     check_loop_s2
        fild    word ptr [esi]
interpolate:
        fsub    st, st(1)
        fmul    st, st(2)
        fadd    st, st(1)
        fxch
        fstp    st(0)
        fld     st(0)
        fmul    st, st(5)
        fxch
        fmul    st, st(6)
        fxch
        fstp    qword ptr [edi]
        fstp    qword ptr [edi+8]
        fstp    st(0)
        dec     ecx
        jz      ms2s16216_done
        cmp     fade_flag, 0
        add     edi, 16
        mov     esi, [edx]
        jnz     do_fade
        jmp     ms2s16216mbloop
do_fade:
        fld     qword ptr fpvol_dec
        fsub    st(3), st
        fsubp   st(4), st
        jmp     ms2s16216mbloop
check_loop_s1:
        cmp     loop_flag, 0
        jz      ms2s16216_done
        fxch
        fstp    st(0)
        fld     st(0)
        fist    dword ptr index
        mov     eax, pstart
        mov     [edx], eax
        mov     esi, eax
        fild    word ptr [esi]
        cmp     last_loop_flag, 0
        jz      get_s2
        mov     loop_flag, 0
        mov     eax, ppend_read
        mov     ebx, peod
        mov     [eax], ebx
        jmp     get_s2
check_loop_s2:
        mov     s2wrap, 0
        cmp     loop_flag, 0
        jz      get_wrap_s2
        mov     eax, pstart
        mov     bx, word ptr [eax]
        mov     s2wrap, bx
get_wrap_s2:
        fild    s2wrap
        jmp     interpolate
ms2s16216_done:
        cmp     ecx, 0
        jnz     unfilled
        sub     esi, 2
        fisub   dword ptr index
        fst     qword ptr fp_shifter
unfilled:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        pwf->m_fp_shifter = fp_shifter;
        if (pwf->is_eod() && !loop_flag)
            pwf->m_flags.last_loop = 1;
    }
    else
    {
        pwb->m_fp_shifter = fp_shifter;
    }
    return nsamples - samples_left;
}

unsigned long __cdecl fp_mov_8216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags)
{
    double fpvol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0, fade_flag = 0;
    char *peod = 0;
    char **ppend_read = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    double fp_shifter, fp_pitch, fp_lvol, fp_rvol;
    short *cur_sample_address = (short *)*ppsrc;
    CWaveFile *pwf;
    WaveBuffer *pwb;
    int index;
    short s2wrap;
    double zero = 0;
    unsigned short b2w_temp = 0;
    short chop_mode;
    int loop_over;

    if (flags == 1) {
        pwf = (CWaveFile *)pcaller;
        fp_lvol = pwf->m_fp_left_volume;
        fp_rvol = pwf->m_fp_right_volume;
        pend = pwf->get_pend();
        pstart = pwf->get_pstart();
        fp_shifter = *pwf->get_fp_shifter();
        fp_pitch = *pwf->get_fp_pitch();
        if (pwf->is_fading()) {
            fpvol_dec = pwf->m_fp_fade_amount / nsamples;
            fade_flag = 1;
        }
        if (pwf->m_flags.last_loop)
            loop_flag = 0;
        else
            loop_flag = 1;
        if (pwf->is_eod()) {
            ppend_read = &pwf->m_pmmioinfo->pchEndRead;
            if ((char *)*ppsrc > *ppend_read)
                last_loop_flag = 1;
            else
                loop_flag = 0;
            peod = pwf->m_pwrite;
        }
    } else {
        pwb = (WaveBuffer *)pcaller;
        fp_lvol = pwb->m_fp_left_volume;
        fp_rvol = pwb->m_fp_right_volume;
        fp_pitch = pwb->m_fp_pitch;
        fp_shifter = pwb->m_fp_shifter;
        if (pwb->is_fading()) {
            fpvol_dec = pwb->get_fp_fade_amount() / nsamples;
            fade_flag = 1;
        }
        loop_flag = pwb->is_looping() && !pwb->is_released();
        if (loop_flag) {
            pstart = pwb->get_ploop_start();
            pend = pwb->get_ploop_end();
        } else {
            pstart = pwb->get_pstart();
            pend = pwb->get_pend();
        }
    }

    chop_mode = 0xf7f;
    loop_over = 0;

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edi, pdst
        fninit
        fldcw   word ptr chop_mode
        fld     qword ptr fp_rvol
        fld     qword ptr fp_lvol
        fld     qword ptr fp_pitch
        fld     qword ptr fp_shifter
ms2s16216mbloop:
        fadd    st, st(1)
        fld     st(0)
        fist    dword ptr index
        mov     eax, index
        add     esi, eax
        fisub   dword ptr index
        cmp     esi, pend
        jge     check_loop_s1
        movzx   eax, byte ptr [esi]
        shl     eax, 8
        sub     eax, 8000h
        mov     word ptr b2w_temp, ax
        fild    word ptr b2w_temp
get_s2:
        inc     esi
        cmp     esi, pend
        jge     check_loop_s2
        movzx   eax, byte ptr [esi]
        shl     eax, 8
        sub     eax, 8000h
        mov     word ptr b2w_temp, ax
        mov     word ptr b2w_temp, ax
        fild    word ptr b2w_temp
interpolate:
        fsub    st, st(1)
        fmul    st, st(2)
        fadd    st, st(1)
        fxch
        fstp    st(0)
        fld     st(0)
        fmul    st, st(5)
        fxch
        fmul    st, st(6)
        fxch
        fstp    qword ptr [edi]
        fstp    qword ptr [edi+8]
        fstp    st(0)
        dec     ecx
        jz      ms2s16216_done
        cmp     fade_flag, 0
        add     edi, 16
        mov     esi, [edx]
        jnz     do_fade
        jmp     ms2s16216mbloop
do_fade:
        fld     qword ptr fpvol_dec
        fsub    st(3), st
        fsubp   st(4), st
        jmp     ms2s16216mbloop
check_loop_s1:
        cmp     loop_flag, 0
        jz      ms2s16216_done
        fxch
        fstp    st(0)
        fld     st(0)
        fist    dword ptr index
        mov     eax, pstart
        mov     [edx], eax
        mov     esi, eax
        movzx   eax, byte ptr [esi]
        shl     eax, 8
        sub     eax, 8000h
        mov     word ptr b2w_temp, ax
        fild    word ptr b2w_temp
        cmp     last_loop_flag, 0
        jz      get_s2
        mov     loop_flag, 0
        mov     eax, ppend_read
        mov     ebx, peod
        mov     [eax], ebx
        jmp     get_s2
check_loop_s2:
        mov     s2wrap, 0
        cmp     loop_flag, 0
        jz      get_wrap_s2
        mov     eax, pstart
        movzx   ebx, byte ptr [eax]
        shl     ebx, 8
        sub     ebx, 8000h
        mov     s2wrap, bx
get_wrap_s2:
        fild    s2wrap
        jmp     interpolate
ms2s16216_done:
        cmp     ecx, 0
        jnz     unfilled
        sub     esi, 1
        fisub   dword ptr index
        fst     qword ptr fp_shifter
unfilled:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        pwf->m_fp_shifter = fp_shifter;
        if (pwf->is_eod() && !loop_flag)
            pwf->m_flags.last_loop = 1;
    }
    else
    {
        pwb->m_fp_shifter = fp_shifter;
    }
    return nsamples - samples_left;
}

unsigned long __cdecl fp_add_8216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags)
{
    double fpvol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0, fade_flag = 0;
    char *peod = 0;
    char **ppend_read = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    double fp_shifter, fp_pitch, fp_lvol, fp_rvol;
    short *cur_sample_address = (short *)*ppsrc;
    CWaveFile *pwf;
    WaveBuffer *pwb;
    int index;
    short s2wrap;
    double zero = 0;
    unsigned short b2w_temp = 0;
    short chop_mode;
    int loop_over;

    if (flags == 1) {
        pwf = (CWaveFile *)pcaller;
        fp_lvol = pwf->m_fp_left_volume;
        fp_rvol = pwf->m_fp_right_volume;
        pend = pwf->get_pend();
        pstart = pwf->get_pstart();
        fp_shifter = *pwf->get_fp_shifter();
        fp_pitch = *pwf->get_fp_pitch();
        if (pwf->is_fading()) {
            fpvol_dec = pwf->m_fp_fade_amount / nsamples;
            fade_flag = 1;
        }
        if (pwf->m_flags.last_loop)
            loop_flag = 0;
        else
            loop_flag = 1;
        if (pwf->is_eod()) {
            ppend_read = &pwf->m_pmmioinfo->pchEndRead;
            if ((char *)*ppsrc > *ppend_read)
                last_loop_flag = 1;
            else
                loop_flag = 0;
            peod = pwf->m_pwrite;
        }
    } else {
        pwb = (WaveBuffer *)pcaller;
        fp_lvol = pwb->m_fp_left_volume;
        fp_rvol = pwb->m_fp_right_volume;
        fp_pitch = pwb->m_fp_pitch;
        fp_shifter = pwb->m_fp_shifter;
        if (pwb->is_fading()) {
            fpvol_dec = pwb->get_fp_fade_amount() / nsamples;
            fade_flag = 1;
        }
        loop_flag = pwb->is_looping() && !pwb->is_released();
        if (loop_flag) {
            pstart = pwb->get_ploop_start();
            pend = pwb->get_ploop_end();
        } else {
            pstart = pwb->get_pstart();
            pend = pwb->get_pend();
        }
    }

    chop_mode = 0xf7f;
    loop_over = 0;

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edi, pdst
        fninit
        fldcw   word ptr chop_mode
        fld     qword ptr fp_rvol
        fld     qword ptr fp_lvol
        fld     qword ptr fp_pitch
        fld     qword ptr fp_shifter
ms2s16216mbloop:
        fadd    st, st(1)
        fld     st(0)
        fist    dword ptr index
        mov     eax, index
        add     esi, eax
        fisub   dword ptr index
        cmp     esi, pend
        jge     check_loop_s1
        movzx   eax, byte ptr [esi]
        shl     eax, 8
        sub     eax, 8000h
        mov     word ptr b2w_temp, ax
        fild    word ptr b2w_temp
get_s2:
        inc     esi
        cmp     esi, pend
        jge     check_loop_s2
        movzx   eax, byte ptr [esi]
        shl     eax, 8
        sub     eax, 8000h
        mov     word ptr b2w_temp, ax
        mov     word ptr b2w_temp, ax
        fild    word ptr b2w_temp
interpolate:
        fsub    st, st(1)
        fmul    st, st(2)
        fadd    st, st(1)
        fxch
        fstp    st(0)
        fld     st(0)
        fmul    st, st(5)
        fxch
        fmul    st, st(6)
        fxch
        fadd    qword ptr [edi]
        fxch
        fadd    qword ptr [edi+8]
        fxch
        fstp    qword ptr [edi]
        fstp    qword ptr [edi+8]
        fstp    st(0)
        dec     ecx
        jz      ms2s16216_done
        cmp     fade_flag, 0
        add     edi, 16
        mov     esi, [edx]
        jnz     do_fade
        jmp     ms2s16216mbloop
do_fade:
        fld     qword ptr fpvol_dec
        fsub    st(3), st
        fsubp   st(4), st
        jmp     ms2s16216mbloop
check_loop_s1:
        cmp     loop_flag, 0
        jz      ms2s16216_done
        fxch
        fstp    st(0)
        fld     st(0)
        fist    dword ptr index
        mov     eax, pstart
        mov     [edx], eax
        mov     esi, eax
        movzx   eax, byte ptr [esi]
        shl     eax, 8
        sub     eax, 8000h
        mov     word ptr b2w_temp, ax
        fild    word ptr b2w_temp
        cmp     last_loop_flag, 0
        jz      get_s2
        mov     loop_flag, 0
        mov     eax, ppend_read
        mov     ebx, peod
        mov     [eax], ebx
        jmp     get_s2
check_loop_s2:
        mov     s2wrap, 0
        cmp     loop_flag, 0
        jz      get_wrap_s2
        mov     eax, pstart
        movzx   ebx, byte ptr [eax]
        shl     ebx, 8
        sub     ebx, 8000h
        mov     s2wrap, bx
get_wrap_s2:
        fild    s2wrap
        jmp     interpolate
ms2s16216_done:
        cmp     ecx, 0
        jnz     unfilled
        sub     esi, 1
        fisub   dword ptr index
        fst     qword ptr fp_shifter
unfilled:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        pwf->m_fp_shifter = fp_shifter;
        if (pwf->is_eod() && !loop_flag)
            pwf->m_flags.last_loop = 1;
    }
    else
    {
        pwb->m_fp_shifter = fp_shifter;
    }
    return nsamples - samples_left;
}

unsigned long __cdecl fp_mov_16216s2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags)
{
    double fpvol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0, fade_flag = 0;
    char *peod = 0;
    char **ppend_read = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    double fp_lvol, fp_rvol;
    short *cur_sample_address = (short *)*ppsrc;
    CWaveFile *pwf;
    WaveBuffer *pwb;
    int index = 0;
    double zero = 0;
    short chop_mode;
    int loop_over;

    if (flags == 1) {
        pwf = (CWaveFile *)pcaller;
        fp_lvol = pwf->m_fp_left_volume;
        fp_rvol = pwf->m_fp_right_volume;
        pend = pwf->get_pend();
        pstart = pwf->get_pstart();
        if (pwf->is_fading()) {
            fpvol_dec = pwf->m_fp_fade_amount / nsamples;
            fade_flag = 1;
        }
        if (pwf->m_flags.last_loop)
            loop_flag = 0;
        else
            loop_flag = 1;
        if (pwf->is_eod()) {
            ppend_read = &pwf->m_pmmioinfo->pchEndRead;
            if ((char *)*ppsrc > *ppend_read)
                last_loop_flag = 1;
            else
                loop_flag = 0;
            peod = pwf->m_pwrite;
        }
    } else {
        pwb = (WaveBuffer *)pcaller;
        fp_lvol = pwb->m_fp_left_volume;
        fp_rvol = pwb->m_fp_right_volume;
        if (pwb->is_fading()) {
            fpvol_dec = pwb->get_fp_fade_amount() / nsamples;
            fade_flag = 1;
        }
        loop_flag = pwb->is_looping() && !pwb->is_released();
        if (loop_flag) {
            pstart = pwb->get_ploop_start();
            pend = pwb->get_ploop_end();
        } else {
            pstart = pwb->get_pstart();
            pend = pwb->get_pend();
        }
    }

    chop_mode = 0xf7f;
    loop_over = 0;

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edi, pdst
        fninit
        fldcw   word ptr chop_mode
        fld     qword ptr fp_rvol
        fld     qword ptr fp_lvol
ms2s16216mbloop:
        cmp     esi, pend
        jge     check_loop_s1
        fild    word ptr [esi]
        fild    word ptr [esi+2]
        fxch
        fmul    st, st(2)
        fxch
        fmul    st, st(3)
        fxch
        fstp    qword ptr [edi]
        fstp    qword ptr [edi+8]
        dec     ecx
        jz      ms2s16216_done
        add     edi, 16
        add     esi, 4
        cmp     fade_flag, 0
        jnz     do_fade
        jmp     ms2s16216mbloop
do_fade:
        fld     qword ptr fpvol_dec
        fsub    st(1), st
        fsubp   st(2), st
        jmp     ms2s16216mbloop
check_loop_s1:
        cmp     loop_flag, 0
        jz      ms2s16216_done
        mov     eax, pstart
        mov     [edx], eax
        mov     esi, eax
        cmp     last_loop_flag, 0
        jz      ms2s16216mbloop
        mov     loop_flag, 0
        jmp     ms2s16216mbloop
ms2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        if (pwf->is_eod() && !loop_flag)
            pwf->m_flags.last_loop = 1;
    }
    return nsamples - samples_left;
}

unsigned long __cdecl fp_add_16216s2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags)
{
    double fpvol_dec = 0;
    int loop_flag = 0, link_flag = 0, last_loop_flag = 0, fade_flag = 0;
    char *peod = 0;
    char **ppend_read = 0;
    char *pend = 0, *pstart = 0, *pcur = 0, *linked_pend = 0;
    unsigned long sample_index = 0, samples_left;
    double fp_lvol, fp_rvol;
    short *cur_sample_address = (short *)*ppsrc;
    CWaveFile *pwf;
    WaveBuffer *pwb;
    int index = 0;
    double zero = 0;
    short chop_mode;
    int loop_over;

    if (flags == 1) {
        pwf = (CWaveFile *)pcaller;
        fp_lvol = pwf->m_fp_left_volume;
        fp_rvol = pwf->m_fp_right_volume;
        pend = pwf->get_pend();
        pstart = pwf->get_pstart();
        if (pwf->is_fading()) {
            fpvol_dec = pwf->m_fp_fade_amount / nsamples;
            fade_flag = 1;
        }
        if (pwf->m_flags.last_loop)
            loop_flag = 0;
        else
            loop_flag = 1;
        if (pwf->is_eod()) {
            ppend_read = &pwf->m_pmmioinfo->pchEndRead;
            if ((char *)*ppsrc > *ppend_read)
                last_loop_flag = 1;
            else
                loop_flag = 0;
            peod = pwf->m_pwrite;
        }
    } else {
        pwb = (WaveBuffer *)pcaller;
        fp_lvol = pwb->m_fp_left_volume;
        fp_rvol = pwb->m_fp_right_volume;
        if (pwb->is_fading()) {
            fpvol_dec = pwb->get_fp_fade_amount() / nsamples;
            fade_flag = 1;
        }
        loop_flag = pwb->is_looping() && !pwb->is_released();
        if (loop_flag) {
            pstart = pwb->get_ploop_start();
            pend = pwb->get_ploop_end();
        } else {
            pstart = pwb->get_pstart();
            pend = pwb->get_pend();
        }
    }

    chop_mode = 0xf7f;
    loop_over = 0;

    __asm {
        push    esi
        push    edi
        push    ebx
        push    ecx
        push    edx
        mov     ecx, nsamples
        mov     edx, ppsrc
        mov     esi, [edx]
        mov     edi, pdst
        fninit
        fldcw   word ptr chop_mode
        fld     qword ptr fp_rvol
        fld     qword ptr fp_lvol
ms2s16216mbloop:
        cmp     esi, pend
        jge     check_loop_s1
        fild    word ptr [esi]
        fild    word ptr [esi+2]
        fxch
        fmul    st, st(2)
        fxch
        fmul    st, st(3)
        fxch
        fadd    qword ptr [edi]
        fxch
        fadd    qword ptr [edi+8]
        fxch
        fstp    qword ptr [edi]
        fstp    qword ptr [edi+8]
        dec     ecx
        jz      ms2s16216_done
        add     edi, 16
        add     esi, 4
        cmp     fade_flag, 0
        jnz     do_fade
        jmp     ms2s16216mbloop
do_fade:
        fld     qword ptr fpvol_dec
        fsub    st(1), st
        fsubp   st(2), st
        jmp     ms2s16216mbloop
check_loop_s1:
        cmp     loop_flag, 0
        jz      ms2s16216_done
        mov     eax, pstart
        mov     [edx], eax
        mov     esi, eax
        cmp     last_loop_flag, 0
        jz      ms2s16216mbloop
        mov     loop_flag, 0
        jmp     ms2s16216mbloop
ms2s16216_done:
        mov     samples_left, ecx
        mov     edx, ppsrc
        mov     [edx], esi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        if (pwf->is_eod() && !loop_flag)
            pwf->m_flags.last_loop = 1;
    }
    return nsamples - samples_left;
}

unsigned long add_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long add_16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long add_8216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long add_8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long mov_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long mov_16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long mov_8216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);

const PFNFILTER filter_table[24] = {
    add_16216s2smb, add_16216s2mmb, add_16216m2smb, add_16216m2mmb, add_828s2smb,  add_828s2mmb,
    add_828m2smb,   add_828m2mmb,   add_8216s2smb,  add_8216s2mmb,  add_8216m2smb, add_8216m2mmb,
    mov_16216s2smb, mov_16216s2mmb, mov_16216m2smb, mov_16216m2mmb, mov_828s2smb,  mov_828s2mmb,
    mov_828m2smb,   mov_828m2mmb,   mov_8216s2smb,  mov_8216s2mmb,  mov_8216m2smb, mov_8216m2mmb,
};

extern const int dsb_block_align[24] = {
    4, 2, 4, 2, 2, 1, 2, 1, 4, 2, 4, 2, 4, 2, 4, 2, 2, 1, 2, 1, 4, 2, 4, 2,
};

#include <windows.h>
#include <mmsystem.h>
#include "sound.h"
