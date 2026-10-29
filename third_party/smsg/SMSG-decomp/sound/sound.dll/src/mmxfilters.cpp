#include "MidiSequence.h"

unsigned long mmx_add_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    int last_loop_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;
    __int64 d1;
    __int64 d2;
    __int64 d3;
    void *pdst;
    void *psrc;
    unsigned long edi_add;
    unsigned long rl_vol;

    link_flag = 0;
    last_loop_flag = 0;
    loop_count = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;
    pdst = *ppdst;
    psrc = *ppsrc;
    edi_add = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        loop_flag = (pend == ((CWaveFile *)pwf)->get_pend_write());
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pshifter = pcur_wb->get_shifter();
        if (!pcur_wb->is_looping() || pcur_wb->is_released())
            loop_flag = 0;
        else
            loop_flag = 1;
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
    if (right_volume == 0x10000)
        right_volume = 0xffff;
    if (left_volume == 0x10000)
        left_volume = 0xffff;
    rl_vol = ((right_volume >> 1) << 16) | ((left_volume >> 1) & 0xffff);

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
        mov     eax, rl_vol
        mov     dword ptr d1[4], eax
        mov     dword ptr d1, eax
        movq    mm3, qword ptr d1
ms2s16216mbloop:
        mov     eax, [esi]
        mov     dword ptr d1, eax
        lea     edx, [esi+4]
        cmp     edx, pend
        jge     get_start_sample_1
        mov     eax, [edx]
        mov     dword ptr d2, eax
setup_fraction:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3, edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
        jle     do_the_deed
        cmp     esi, pend
        jge     get_start_sample_2
        mov     eax, [esi]
        mov     dword ptr d1[4], eax
        lea     eax, [esi+4]
        cmp     eax, pend
        jge     get_start_sample_3
        mov     eax, [esi+4]
        mov     dword ptr d2[4], eax
setup_fraction2:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3[4], edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
do_the_deed:
        movq    mm0, qword ptr d1
        movq    mm1, qword ptr d2
        movq    mm2, qword ptr d3
        psubsw  mm1, mm0
        pmulhw  mm1, mm2
        paddsw  mm0, mm1
        pmulhw  mm0, mm3
        psllw   mm0, 1
        paddsw  mm0, qword ptr [edi]
        movq    qword ptr [edi], mm0
        mov     eax, edi_add
        add     edi, eax
        mov     edi_add, 0
        cmp     ecx, 0
        jle     ms2s16216_done
        cmp     esi, pend
        jge     eod_condition
        jmp     ms2s16216mbloop
get_start_sample_1:
        cmp     loop_flag, 1
        je      gss1
        mov     eax, 0
        mov     dword ptr d2, eax
        jmp     setup_fraction
gss1:
        sub     esi, pend
        jge     gss12
        mov     esi, 0
gss12:
        mov     [ebx+4], esi
        add     esi, pstart
        mov     eax, [esi]
        mov     dword ptr d2, eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        je      setup_fraction
        mov     loop_flag, 0
        jmp     setup_fraction
get_start_sample_2:
        cmp     loop_flag, 1
        je      gss2
        mov     eax, 0
        mov     dword ptr d1[4], eax
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss2:
        sub     esi, pend
        jge     gss22
        mov     esi, 0
gss22:
        mov     [ebx+4], esi
        add     esi, pstart
        mov     eax, [esi]
        mov     dword ptr d1[4], eax
        mov     eax, [esi+4]
        mov     dword ptr d2[4], eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        je      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
get_start_sample_3:
        cmp     loop_flag, 1
        je      gss3
        mov     eax, 0
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss3:
        lea     eax, [esi+4]
        sub     eax, pend
        jge     gss32
        mov     eax, 0
gss32:
        mov     [ebx+4], eax
        add     eax, pstart
        mov     edx, [eax]
        mov     dword ptr d2[4], edx
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        je      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
eod_condition:
        cmp     link_flag, 0
        jne     set_link
        cmp     loop_flag, 0
        je      ms2s16216_done
        inc     loop_count
        mov     sample_index, 0
        sub     esi, pend
        mov     [ebx+4], esi
        mov     eax, ppsrc
        add     esi, pstart
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
        emms
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)
            ->set_current_position(((CWaveFile *)pwf)->get_current_position() + (nsamples - samples_left));
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long mmx_add_16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    unsigned long vol_dec;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    int last_loop_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;
    __int64 d1;
    __int64 d2;
    __int64 d3;
    void *pdst;
    void *psrc;
    unsigned long edi_add;
    unsigned long rl_vol;

    vol_dec = 0;
    link_flag = 0;
    last_loop_flag = 0;
    loop_count = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;
    pdst = *ppdst;
    psrc = *ppsrc;
    edi_add = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        loop_flag = (pend == ((CWaveFile *)pwf)->get_pend_write());
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        if (pcur_wb->is_fading())
            vol_dec = pcur_wb->get_fade_amount() / nsamples + 1;
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
    if (right_volume == 0x10000)
        right_volume = 0xffff;
    if (left_volume == 0x10000)
        left_volume = 0xffff;
    rl_vol = ((right_volume >> 1) << 16) | ((left_volume >> 1) & 0xffff);

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
        mov     eax, rl_vol
        mov     dword ptr d1[4], eax
        mov     dword ptr d1, eax
        movq    mm3, d1
        mov     eax, vol_dec
        mov     dword ptr d1[4], eax
        mov     dword ptr d1, eax
        movq    mm4, d1
        packssdw mm4, d1
am2s16216mbloop:
        mov     ax, word ptr [esi]
        shl     eax, 10h
        mov     ax, word ptr [esi]
        mov     dword ptr d1, eax
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample_1
        mov     ax, word ptr [edx]
        shl     eax, 10h
        mov     ax, word ptr [edx]
        mov     dword ptr d2, eax
setup_fraction:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3, edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        shl     edx, 1
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
        jle     do_the_deed
        cmp     esi, pend
        jge     get_start_sample_2
        mov     ax, word ptr [esi]
        shl     eax, 10h
        mov     ax, word ptr [esi]
        mov     dword ptr d1[4], eax
        lea     eax, [esi+2]
        cmp     eax, pend
        jge     get_start_sample_3
        mov     ax, word ptr [esi+2]
        shl     eax, 10h
        mov     ax, word ptr [esi+2]
        mov     dword ptr d2[4], eax
setup_fraction2:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3[4], edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        shl     edx, 1
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
do_the_deed:
        movq    mm0, d1
        movq    mm1, d2
        movq    mm2, d3
        psubsw  mm1, mm0
        pmulhw  mm1, mm2
        paddsw  mm0, mm1
        pmulhw  mm0, mm3
        psllw   mm0, 1
        paddsw  mm0, [edi]
        movq    [edi], mm0
        psubusw mm3, mm4
        mov     eax, edi_add
        add     edi, eax
        mov     edi_add, 0
        cmp     ecx, 0
        jle     am2s16216_done
        cmp     esi, pend
        jge     eod_condition
        jmp     am2s16216mbloop
get_start_sample_1:
        cmp     loop_flag, 1
        jz      gss1
        mov     eax, 0
        mov     dword ptr d2, eax
        jmp     setup_fraction
gss1:
        sub     esi, pend
        jge     gss12
        mov     esi, 0
gss12:
        mov     dword ptr [ebx+4], esi
        add     esi, pstart
        mov     ax, word ptr [esi]
        shl     eax, 10h
        mov     ax, word ptr [esi]
        mov     dword ptr d2, eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction
        mov     loop_flag, 0
        jmp     setup_fraction
get_start_sample_2:
        cmp     loop_flag, 1
        jz      gss2
        mov     eax, 0
        mov     dword ptr d1[4], eax
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss2:
        sub     esi, pend
        jge     gss22
        mov     esi, 0
gss22:
        mov     dword ptr [ebx+4], esi
        add     esi, pstart
        mov     eax, dword ptr [esi]
        mov     dword ptr d1[4], eax
        mov     ax, word ptr [esi+2]
        shl     eax, 10h
        mov     ax, word ptr [esi+2]
        mov     dword ptr d2[4], eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
get_start_sample_3:
        cmp     loop_flag, 1
        jz      gss3
        mov     eax, 0
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss3:
        lea     eax, [esi+4]
        sub     eax, pend
        jge     gss32
        mov     eax, 0
gss32:
        mov     dword ptr [ebx+4], eax
        add     eax, pstart
        mov     edx, dword ptr [eax]
        mov     dword ptr d2[4], edx
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
eod_condition:
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      am2s16216_done
        inc     loop_count
        mov     sample_index, 0
        sub     esi, pend
        mov     dword ptr [ebx+4], esi
        mov     eax, ppsrc
        add     esi, pstart
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
        mov     eax, dword ptr [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        emms
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

unsigned long mmx_add_8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    int last_loop_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;
    __int64 d1;
    __int64 d2;
    __int64 d3;
    void *pdst;
    void *psrc;
    unsigned long edi_add;
    unsigned long rl_vol;

    link_flag = 0;
    last_loop_flag = 0;
    loop_count = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;
    pdst = *ppdst;
    psrc = *ppsrc;
    edi_add = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        loop_flag = (pend == ((CWaveFile *)pwf)->get_pend_write());
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
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
    if (right_volume == 0x10000)
        right_volume = 0xffff;
    if (left_volume == 0x10000)
        left_volume = 0xffff;
    rl_vol = ((right_volume >> 1) << 16) | ((left_volume >> 1) & 0xffff);

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
        mov     eax, rl_vol
        mov     dword ptr d1[4], eax
        mov     dword ptr d1, eax
        movq    mm3, d1
am2s8216mbloop:
        movzx   edx, byte ptr [esi]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d1, eax
        lea     edx, [esi+1]
        cmp     edx, pend
        jge     get_start_sample_1
        movzx   edx, byte ptr [esi+1]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2, eax
setup_fraction:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3, edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
        jle     do_the_deed
        cmp     esi, pend
        jge     get_start_sample_2
        movzx   edx, byte ptr [esi]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d1[4], eax
        lea     eax, [esi+1]
        cmp     eax, pend
        jge     get_start_sample_3
        movzx   edx, byte ptr [esi+1]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2[4], eax
setup_fraction2:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3[4], edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
do_the_deed:
        movq    mm0, d1
        movq    mm1, d2
        movq    mm2, d3
        psubsw  mm1, mm0
        pmulhw  mm1, mm2
        paddsw  mm0, mm1
        pmulhw  mm0, mm3
        psllw   mm0, 1
        paddsw  mm0, [edi]
        movq    [edi], mm0
        mov     eax, edi_add
        add     edi, eax
        mov     edi_add, 0
        cmp     ecx, 0
        jle     mm2s8216_done
        cmp     esi, pend
        jge     eod_condition
        jmp     am2s8216mbloop
get_start_sample_1:
        cmp     loop_flag, 1
        jz      gss1
        mov     eax, 0
        mov     dword ptr d2, eax
        jmp     setup_fraction
gss1:
        sub     esi, pend
        jge     gss12
        mov     esi, 0
gss12:
        mov     dword ptr [ebx+4], esi
        add     esi, pstart
        movzx   edx, byte ptr [esi]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2, eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction
        mov     loop_flag, 0
        jmp     setup_fraction
get_start_sample_2:
        cmp     loop_flag, 1
        jz      gss2
        mov     eax, 0
        mov     dword ptr d1[4], eax
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss2:
        sub     esi, pend
        jge     gss22
        mov     esi, 0
gss22:
        mov     dword ptr [ebx+4], esi
        add     esi, pstart
        movzx   edx, byte ptr [esi]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d1[4], eax
        movzx   edx, byte ptr [esi+1]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2[4], eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
get_start_sample_3:
        cmp     loop_flag, 1
        jz      gss3
        mov     eax, 0
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss3:
        lea     eax, [esi+1]
        sub     eax, pend
        jge     gss32
        mov     eax, 0
gss32:
        mov     dword ptr [ebx+4], eax
        add     eax, pstart
        movzx   edx, byte ptr [eax]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2[4], eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
eod_condition:
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      mm2s8216_done
        inc     loop_count
        mov     sample_index, 0
        sub     esi, pend
        mov     dword ptr [ebx+4], esi
        mov     eax, ppsrc
        add     esi, pstart
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     am2s8216mbloop
        jmp     mm2s8216_done
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
        jnz     am2s8216mbloop
        jmp     mm2s8216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     am2s8216mbloop
mm2s8216_done:
        mov     samples_left, ecx
        mov     eax, dword ptr [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        emms
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

unsigned long mmx_mov_16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    unsigned long vol_dec;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    int last_loop_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;
    __int64 d1;
    __int64 d2;
    __int64 d3;
    void *pdst;
    void *psrc;
    unsigned long edi_add;
    unsigned long rl_vol;

    vol_dec = 0;
    link_flag = 0;
    last_loop_flag = 0;
    loop_count = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;
    pdst = *ppdst;
    psrc = *ppsrc;
    edi_add = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        loop_flag = (pend == ((CWaveFile *)pwf)->get_pend_write());
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        if (pcur_wb->is_fading())
            vol_dec = pcur_wb->get_fade_amount() / nsamples;
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
    if (right_volume == 0x10000)
        right_volume = 0xffff;
    if (left_volume == 0x10000)
        left_volume = 0xffff;
    rl_vol = ((right_volume >> 1) << 16) | ((left_volume >> 1) & 0xffff);

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
        mov     eax, rl_vol
        mov     dword ptr d1[4], eax
        mov     dword ptr d1, eax
        movq    mm3, d1
        mov     eax, vol_dec
        mov     dword ptr d1[4], eax
        mov     dword ptr d1, eax
        movq    mm4, d1
        packssdw mm4, d1
mm2s16216mbloop:
        mov     ax, word ptr [esi]
        shl     eax, 10h
        mov     ax, word ptr [esi]
        mov     dword ptr d1, eax
        lea     edx, [esi+2]
        cmp     edx, pend
        jge     get_start_sample_1
        mov     ax, word ptr [edx]
        shl     eax, 10h
        mov     ax, word ptr [edx]
        mov     dword ptr d2, eax
setup_fraction:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3, edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        shl     edx, 1
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
        jle     do_the_deed
        cmp     esi, pend
        jge     get_start_sample_2
        mov     ax, word ptr [esi]
        shl     eax, 10h
        mov     ax, word ptr [esi]
        mov     dword ptr d1[4], eax
        lea     eax, [esi+2]
        cmp     eax, pend
        jge     get_start_sample_3
        mov     ax, word ptr [esi+2]
        shl     eax, 10h
        mov     ax, word ptr [esi+2]
        mov     dword ptr d2[4], eax
setup_fraction2:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3[4], edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        shl     edx, 1
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
do_the_deed:
        movq    mm0, d1
        movq    mm1, d2
        movq    mm2, d3
        psubsw  mm1, mm0
        pmulhw  mm1, mm2
        paddsw  mm0, mm1
        pmulhw  mm0, mm3
        psllw   mm0, 1
        movq    [edi], mm0
        psubusw mm3, mm4
        mov     eax, edi_add
        add     edi, eax
        mov     edi_add, 0
        cmp     ecx, 0
        jle     mm2s16216_done
        cmp     esi, pend
        jge     eod_condition
        jmp     mm2s16216mbloop
get_start_sample_1:
        cmp     loop_flag, 1
        jz      gss1
        mov     eax, 0
        mov     dword ptr d2, eax
        jmp     setup_fraction
gss1:
        sub     esi, pend
        jge     gss12
        mov     esi, 0
gss12:
        mov     dword ptr [ebx+4], esi
        add     esi, pstart
        mov     ax, word ptr [esi]
        shl     eax, 10h
        mov     ax, word ptr [esi]
        mov     dword ptr d2, eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction
        mov     loop_flag, 0
        jmp     setup_fraction
get_start_sample_2:
        cmp     loop_flag, 1
        jz      gss2
        mov     eax, 0
        mov     dword ptr d1[4], eax
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss2:
        sub     esi, pend
        jge     gss22
        mov     esi, 0
gss22:
        mov     dword ptr [ebx+4], esi
        add     esi, pstart
        mov     eax, dword ptr [esi]
        mov     dword ptr d1[4], eax
        mov     ax, word ptr [esi+2]
        shl     eax, 10h
        mov     ax, word ptr [esi+2]
        mov     dword ptr d2[4], eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
get_start_sample_3:
        cmp     loop_flag, 1
        jz      gss3
        mov     eax, 0
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss3:
        lea     eax, [esi+4]
        sub     eax, pend
        jge     gss32
        mov     eax, 0
gss32:
        mov     dword ptr [ebx+4], eax
        add     eax, pstart
        mov     edx, dword ptr [eax]
        mov     dword ptr d2[4], edx
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
eod_condition:
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      mm2s16216_done
        inc     loop_count
        mov     sample_index, 0
        sub     esi, pend
        mov     dword ptr [ebx+4], esi
        mov     eax, ppsrc
        add     esi, pstart
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     mm2s16216mbloop
        jmp     mm2s16216_done
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
        jnz     mm2s16216mbloop
        jmp     mm2s16216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     mm2s16216mbloop
mm2s16216_done:
        mov     samples_left, ecx
        mov     eax, dword ptr [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        emms
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

unsigned long mmx_mov_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    int last_loop_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;
    __int64 d1;
    __int64 d2;
    __int64 d3;
    void *pdst;
    void *psrc;
    unsigned long edi_add;
    unsigned long rl_vol;

    link_flag = 0;
    last_loop_flag = 0;
    loop_count = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;
    pdst = *ppdst;
    psrc = *ppsrc;
    edi_add = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        loop_flag = (pend == ((CWaveFile *)pwf)->get_pend_write());
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
        pshifter = pcur_wb->get_shifter();
        if (!pcur_wb->is_looping() || pcur_wb->is_released())
            loop_flag = 0;
        else
            loop_flag = 1;
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
    if (right_volume == 0x10000)
        right_volume = 0xffff;
    if (left_volume == 0x10000)
        left_volume = 0xffff;
    rl_vol = ((right_volume >> 1) << 16) | ((left_volume >> 1) & 0xffff);

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
        mov     eax, rl_vol
        mov     dword ptr d1[4], eax
        mov     dword ptr d1, eax
        movq    mm3, qword ptr d1
ms2s16216mbloop:
        mov     eax, [esi]
        mov     dword ptr d1, eax
        lea     edx, [esi+4]
        cmp     edx, pend
        jge     get_start_sample_1
        mov     eax, [edx]
        mov     dword ptr d2, eax
setup_fraction:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3, edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
        jle     do_the_deed
        cmp     esi, pend
        jge     get_start_sample_2
        mov     eax, [esi]
        mov     dword ptr d1[4], eax
        lea     eax, [esi+4]
        cmp     eax, pend
        jge     get_start_sample_3
        mov     eax, [esi+4]
        mov     dword ptr d2[4], eax
setup_fraction2:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3[4], edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
do_the_deed:
        movq    mm0, qword ptr d1
        movq    mm1, qword ptr d2
        movq    mm2, qword ptr d3
        psubsw  mm1, mm0
        pmulhw  mm1, mm2
        paddsw  mm0, mm1
        pmulhw  mm0, mm3
        psllw   mm0, 1
        movq    qword ptr [edi], mm0
        mov     eax, edi_add
        add     edi, eax
        mov     edi_add, 0
        cmp     ecx, 0
        jle     ms2s16216_done
        cmp     esi, pend
        jge     eod_condition
        jmp     ms2s16216mbloop
get_start_sample_1:
        cmp     loop_flag, 1
        je      gss1
        mov     eax, 0
        mov     dword ptr d2, eax
        jmp     setup_fraction
gss1:
        sub     esi, pend
        jge     gss12
        mov     esi, 0
gss12:
        mov     [ebx+4], esi
        add     esi, pstart
        mov     eax, [esi]
        mov     dword ptr d2, eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        je      setup_fraction
        mov     loop_flag, 0
        jmp     setup_fraction
get_start_sample_2:
        cmp     loop_flag, 1
        je      gss2
        mov     eax, 0
        mov     dword ptr d1[4], eax
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss2:
        sub     esi, pend
        jge     gss22
        mov     esi, 0
gss22:
        mov     [ebx+4], esi
        add     esi, pstart
        mov     eax, [esi]
        mov     dword ptr d1[4], eax
        mov     eax, [esi+4]
        mov     dword ptr d2[4], eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        je      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
get_start_sample_3:
        cmp     loop_flag, 1
        je      gss3
        mov     eax, 0
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss3:
        lea     eax, [esi+4]
        sub     eax, pend
        jge     gss32
        mov     eax, 0
gss32:
        mov     [ebx+4], eax
        add     eax, pstart
        mov     edx, [eax]
        mov     dword ptr d2[4], edx
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        je      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
eod_condition:
        cmp     link_flag, 0
        jne     set_link
        cmp     loop_flag, 0
        je      ms2s16216_done
        inc     loop_count
        mov     sample_index, 0
        sub     esi, pend
        mov     [ebx+4], esi
        mov     eax, ppsrc
        add     esi, pstart
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
        emms
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
    }

    if (flags == 1)
    {
        ((CWaveFile *)pwf)
            ->set_current_position(((CWaveFile *)pwf)->get_current_position() + (nsamples - samples_left));
        ((CWaveFile *)pwf)->set_loop_count(((CWaveFile *)pwf)->get_loop_count() + loop_count);
    }
    else
    {
        ((WaveBuffer *)pwf)->set_current_position(((WaveBuffer *)pwf)->get_current_position() + sample_index);
        ((WaveBuffer *)pwf)->set_loop_count(((WaveBuffer *)pwf)->get_loop_count() + loop_count);
    }
    return nsamples - samples_left;
}

unsigned long mmx_mov_8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    int last_loop_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;
    __int64 d1;
    __int64 d2;
    __int64 d3;
    void *pdst;
    void *psrc;
    unsigned long edi_add;
    unsigned long rl_vol;

    link_flag = 0;
    last_loop_flag = 0;
    loop_count = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;
    pdst = *ppdst;
    psrc = *ppsrc;
    edi_add = 0;

    if (flags == 1) {
        left_volume = ((CWaveFile *)pwf)->get_left_volume();
        right_volume = ((CWaveFile *)pwf)->get_right_volume();
        pitch = ((CWaveFile *)pwf)->get_pitch();
        pshifter = ((CWaveFile *)pwf)->get_shifter();
        pend = ((CWaveFile *)pwf)->get_pend();
        pstart = ((CWaveFile *)pwf)->get_pstart();
        loop_flag = (pend == ((CWaveFile *)pwf)->get_pend_write());
        if (((CWaveFile *)pwf)->is_eod() && loop_flag)
            last_loop_flag = 1;
    } else {
        pcur_wb = (WaveBuffer *)pwf;
        left_volume = pcur_wb->get_left_volume();
        right_volume = pcur_wb->get_right_volume();
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
    if (right_volume == 0x10000)
        right_volume = 0xffff;
    if (left_volume == 0x10000)
        left_volume = 0xffff;
    rl_vol = ((right_volume >> 1) << 16) | ((left_volume >> 1) & 0xffff);

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
        mov     eax, rl_vol
        mov     dword ptr d1[4], eax
        mov     dword ptr d1, eax
        movq    mm3, d1
mm2s8216mbloop:
        movzx   edx, byte ptr [esi]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d1, eax
        lea     edx, [esi+1]
        cmp     edx, pend
        jge     get_start_sample_1
        movzx   edx, byte ptr [esi+1]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2, eax
setup_fraction:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3, edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
        jle     do_the_deed
        cmp     esi, pend
        jge     get_start_sample_2
        movzx   edx, byte ptr [esi]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d1[4], eax
        lea     eax, [esi+1]
        cmp     eax, pend
        jge     get_start_sample_3
        movzx   edx, byte ptr [esi+1]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2[4], eax
setup_fraction2:
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3[4], edx
        mov     eax, pitch
        add     dword ptr [ebx+2], eax
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
do_the_deed:
        movq    mm0, d1
        movq    mm1, d2
        movq    mm2, d3
        psubsw  mm1, mm0
        pmulhw  mm1, mm2
        paddsw  mm0, mm1
        pmulhw  mm0, mm3
        psllw   mm0, 1
        movq    [edi], mm0
        mov     eax, edi_add
        add     edi, eax
        mov     edi_add, 0
        cmp     ecx, 0
        jle     mm2s8216_done
        cmp     esi, pend
        jge     eod_condition
        jmp     mm2s8216mbloop
get_start_sample_1:
        cmp     loop_flag, 1
        jz      gss1
        mov     eax, 0
        mov     dword ptr d2, eax
        jmp     setup_fraction
gss1:
        sub     esi, pend
        jge     gss12
        mov     esi, 0
gss12:
        mov     dword ptr [ebx+4], esi
        add     esi, pstart
        movzx   edx, byte ptr [esi]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2, eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction
        mov     loop_flag, 0
        jmp     setup_fraction
get_start_sample_2:
        cmp     loop_flag, 1
        jz      gss2
        mov     eax, 0
        mov     dword ptr d1[4], eax
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss2:
        sub     esi, pend
        jge     gss22
        mov     esi, 0
gss22:
        mov     dword ptr [ebx+4], esi
        add     esi, pstart
        movzx   edx, byte ptr [esi]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d1[4], eax
        movzx   edx, byte ptr [esi+1]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2[4], eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
get_start_sample_3:
        cmp     loop_flag, 1
        jz      gss3
        mov     eax, 0
        mov     dword ptr d2[4], eax
        jmp     setup_fraction2
gss3:
        lea     eax, [esi+1]
        sub     eax, pend
        jge     gss32
        mov     eax, 0
gss32:
        mov     dword ptr [ebx+4], eax
        add     eax, pstart
        movzx   edx, byte ptr [eax]
        shl     edx, 8
        sub     edx, 8000h
        mov     eax, edx
        shl     eax, 10h
        mov     ax, dx
        mov     dword ptr d2[4], eax
        mov     eax, ppsrc
        mov     [eax], esi
        cmp     last_loop_flag, 0
        jz      setup_fraction2
        mov     loop_flag, 0
        jmp     setup_fraction2
eod_condition:
        cmp     link_flag, 0
        jnz     set_link
        cmp     loop_flag, 0
        jz      mm2s8216_done
        inc     loop_count
        mov     sample_index, 0
        sub     esi, pend
        mov     dword ptr [ebx+4], esi
        mov     eax, ppsrc
        add     esi, pstart
        mov     esi, pstart
        mov     [eax], esi
        cmp     ecx, 0
        jnz     mm2s8216mbloop
        jmp     mm2s8216_done
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
        jnz     mm2s8216mbloop
        jmp     mm2s8216_done
last_link:
        mov     link_flag, 0
        mov     loop_flag, 0
        mov     edx, [eax+0d8h]
        cmp     ecx, 0
        jnz     mm2s8216mbloop
mm2s8216_done:
        mov     samples_left, ecx
        mov     eax, dword ptr [ebx+4]
        mov     sample_index, eax
        mov     dword ptr [ebx+4], 0
        mov     edx, ppsrc
        mov     [edx], esi
        mov     edx, ppdst
        mov     [edx], edi
        emms
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

unsigned long test3_mmx_mov_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;
    __int64 d1;
    void *pdst;
    void *psrc;
    unsigned long rl_vol;

    loop_flag = 0;
    link_flag = 0;
    loop_count = 0;
    pend = 0;
    pstart = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;
    pdst = *ppdst;
    psrc = *ppsrc;

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
        pitch = pcur_wb->get_pitch();
    }
    right_volume >>= 1;
    left_volume >>= 1;
    rl_vol = (right_volume << 16) | (left_volume & 0xffff);

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
        mov     edx, dword ptr [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
get_next_samples:
        movd    mm0, dword ptr [esi]
        movd    mm1, dword ptr [esi+4]
        psubsw  mm1, mm0
        movd    dword ptr d1, mm1
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        movd    mm3, edx
        pmulhw  mm1, mm3
        movd    dword ptr d1, mm1
        paddsw  mm0, mm1
        movd    dword ptr d1, mm0
        movd    dword ptr [edi], mm0
        add     edi, 4
        mov     edx, pitch
        add     dword ptr [ebx+2], edx
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        dec     ecx
        jz      all_done
        cmp     esi, pend
        jl      get_next_samples
all_done:
        mov     samples_left, ecx
        mov     eax, dword ptr [ebx+4]
        mov     sample_index, eax
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
        emms
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

unsigned long test4_mmx_mov_16216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;
    __int64 d1;
    __int64 d2;
    __int64 d3;
    void *pdst;
    void *psrc;
    unsigned long edi_add;
    unsigned long rl_vol;

    loop_flag = 0;
    link_flag = 0;
    loop_count = 0;
    pend = 0;
    pstart = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;
    pdst = *ppdst;
    psrc = *ppsrc;
    edi_add = 0;

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
        pitch = pcur_wb->get_pitch();
    }
    right_volume >>= 1;
    left_volume >>= 1;
    rl_vol = (right_volume << 16) | (left_volume & 0xffff);

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
        mov     edx, dword ptr [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
get_next_samples:
        mov     eax, dword ptr [esi]
        mov     dword ptr d1, eax
        mov     eax, dword ptr [esi+4]
        mov     dword ptr d2, eax
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3, edx
        mov     edx, pitch
        add     dword ptr [ebx+2], edx
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
        jz      do_the_deed
        cmp     esi, pend
        mov     eax, dword ptr [esi]
        mov     dword ptr d1[4], eax
        mov     eax, dword ptr [esi+4]
        mov     dword ptr d2[4], eax
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        shl     edx, 10h
        mov     dx, word ptr [ebx+2]
        sub     dx, 8000h
        mov     dword ptr d3[4], edx
        mov     edx, pitch
        add     dword ptr [ebx+2], edx
        adc     word ptr [ebx+6], 0
        mov     edx, dword ptr [ebx+4]
        shl     edx, 2
        mov     eax, ppsrc
        mov     esi, [eax]
        add     esi, edx
        add     edi_add, 4
        dec     ecx
do_the_deed:
        movq    mm0, d1
        movq    mm1, d2
        psubsw  mm1, mm0
        movq    mm3, d3
        pmulhw  mm1, mm3
        paddsw  mm0, mm1
        movq    qword ptr [edi], mm0
        mov     edx, edi_add
        add     edi, edx
        mov     edi_add, 0
        cmp     esi, pend
        jge     all_done
        cmp     ecx, 0
        jg      get_next_samples
all_done:
        mov     samples_left, ecx
        mov     eax, dword ptr [ebx+4]
        mov     sample_index, eax
        mov     edx, ppdst
        mov     [edx], edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     edi
        pop     esi
        emms
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

unsigned long mmx_add_8k222k16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;

    loop_flag = 0;
    link_flag = 0;
    loop_count = 0;
    pend = 0;
    pstart = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;

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
        sar  eax, 16
        add  word ptr [edi], ax
        jo   clip_l
        add  edi, 2
        jmp  do_right
    clip_l:
        jc   clip_neg_l
        mov  ax, 7fffh
        mov  word ptr [edi], ax
        jmp  clip_l_done
    clip_neg_l:
        mov  ax, 8000h
        mov  word ptr [edi], ax
    clip_l_done:
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
        add  word ptr [edi], ax
        jo   clip_r
        add  edi, 2
        jmp  next_sample
    clip_r:
        jc   clip_neg_r
        mov  eax, 7fffh
        mov  word ptr [edi], ax
        jmp  clip_r_done
    clip_neg_r:
        mov  eax, 8000h
        mov  word ptr [edi], ax
    clip_r_done:
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

unsigned long mmx_mov_8k222k16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;

    loop_flag = 0;
    link_flag = 0;
    loop_count = 0;
    pend = 0;
    pstart = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;

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

unsigned long mmx_a8k222k8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;

    loop_flag = 0;
    link_flag = 0;
    loop_count = 0;
    pend = 0;
    pstart = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;

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
        sar  eax, 16
        add  word ptr [edi], ax
        jo   clip_l
        add  edi, 2
        jmp  do_right
    clip_l:
        jc   clip_neg_l
        mov  eax, 7fffh
        mov  word ptr [edi], ax
        jmp  clip_l_done
    clip_neg_l:
        mov  eax, 8000h
        mov  word ptr [edi], ax
    clip_l_done:
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
        mov  edx, right_volume
        imul edx
        sar  eax, 16
        add  word ptr [edi], ax
        jo   clip_r
        add  edi, 2
        jmp  next_sample
    clip_r:
        jc   clip_neg_r
        mov  eax, 7fffh
        mov  word ptr [edi], ax
        jmp  clip_r_done
    clip_neg_r:
        mov  eax, 8000h
        mov  word ptr [edi], ax
    clip_r_done:
        add  edi, 2
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

unsigned long mmx_m8k222k8216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags)
{
    unsigned long left_volume;
    unsigned long right_volume;
    SHIFTER *pshifter;
    int loop_flag;
    int link_flag;
    unsigned long loop_count;
    char *pend;
    char *pstart;
    char *pcur;
    char *linked_pend;
    unsigned long sample_index;
    unsigned long samples_left;
    unsigned long pitch;
    WaveBuffer *plinked_wb;
    WaveBuffer *pcur_wb;

    loop_flag = 0;
    link_flag = 0;
    loop_count = 0;
    pend = 0;
    pstart = 0;
    pcur = 0;
    linked_pend = 0;
    sample_index = 0;
    plinked_wb = 0;
    pcur_wb = 0;

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
        movzx eax, byte ptr [eax]
        shl  eax, 8
        sub  eax, 8000h
        jmp  do_interpolate
    do_right:
        pop  eax
        mov  edx, right_volume
        imul edx
        sar  eax, 16
        mov  word ptr [edi], ax
        add  edi, 2
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

unsigned long add_8216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long mov_8216s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);

extern const PFNFILTER mmx_filter_table[24] = {
    mmx_add_16216s2smb, add_16216s2mmb, mmx_add_16216m2smb, add_16216m2mmb, add_828s2smb,      add_828s2mmb,
    add_828m2smb,       add_828m2mmb,   add_8216s2smb,      add_8216s2mmb,  mmx_add_8216m2smb, add_8216m2mmb,
    mmx_mov_16216s2smb, mov_16216s2mmb, mmx_mov_16216m2smb, mov_16216m2mmb, mov_828s2smb,      mov_828s2mmb,
    mov_828m2smb,       mov_828m2mmb,   mov_8216s2smb,      mov_8216s2mmb,  mmx_mov_8216m2smb, mov_8216m2mmb,
};
