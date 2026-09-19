/* Timer and PC-speaker sound — port of TDEGA 0x694A..0x6C3B, 0x7864..0x7894, 0x89F0..0x8A3E, 0x9A01/0x9A05
 * (port/spec/platform.md §4.6, §6). All song/timer state lives in DGROUP at its original address. */
#include "timer.h"

#include "../host.h"
#include "../symbols.h"

/* ---- host-side hardware shadows (not game state: they model PIT channel 2 and port 61h) */
static u16 spk_divisor;           /* last value latched into PIT channel 2 */
static u8  spk_port61;            /* bits 0-1 of port 61h (gate + speaker data) */
static bool timer_hooked;         /* INT 8 vector == CS:6A1F (checked by 0x694A) */
static void (*driving_isr)(void);

/* DS layout of the song pointers: segment word first, offset word second. */
#define SND_PTR_SEG       EGA_CGA(0x642C, 0x63EA)
#define SND_PTR_OFF       EGA_CGA(0x642E, 0x63EC)
#define SND_LOOP_SEG      EGA_CGA(0x6430, 0x63EE)
#define SND_LOOP_OFF      EGA_CGA(0x6432, 0x63F0)

static void spk_off(void)                       /* in al,61h ; and al,0FCh ; out 61h,al */
{
    spk_port61 &= (u8)~3;
    host_speaker(spk_divisor, false);
}

static void spk_note(u16 divisor)               /* out 42h lo/hi ; port 61h |= 3 */
{
    spk_divisor = divisor;
    spk_port61 |= 3;
    host_speaker(spk_divisor, true);
}

void timer_init(void)
{
    host_set_tick_handler(timer_host_tick);
}

void timer_host_tick(void)
{
    if (driving_isr) driving_isr();
    else timer_isr();
}

void timer_set_driving_isr(void (*isr)(void))
{
    driving_isr = isr;
}

/* 0x69CF timer_install_common */
static void timer_install_common(void)
{
    /* PORT: PIT ch0 divisor 0x2E97, 43h=B6h, PIC masks and the INT 8 vector write are dropped; the host
     * tick runs at 100.0404 Hz unconditionally. */
    spk_off();
    DSB(DS_snd_playing) = 0;
    timer_hooked = true;
}

/* 0x699E timer_install_menu */
void timer_install_menu(void)
{
    DSW(DS_chain_reload) = 5;
    DSW(DS_chain_count) = 5;
    DSB(DS_chain_enable) = 1;
    timer_install_common();
}

/* 0x698C timer_install_drive (chain_count untouched) */
void timer_install_drive(void)
{
    DSW(DS_chain_reload) = 0x7D00;
    DSB(DS_chain_enable) = 0;
    timer_install_common();
}

/* 0x694A timer_restore */
void timer_restore(void)
{
    if (!timer_hooked) return;                  /* vector is not CS:6A1F -> ret */
    /* PORT: vector restore and PIT ch0 = 65536 dropped. */
    timer_hooked = false;
    spk_off();
}

/* 0x6AA8 snd_fetch — song bytecode interpreter */
static void snd_fetch(void)
{
    for (;;) {
        u8 f = DSB(DS_snd_flags);
        if (!(f & 4)) goto stop;                                   /* sound disabled */
        if (!(f & DSB(DS_snd_playing))) goto end_of_stream;        /* stream cancelled/ended */

        u16 seg = DSW(SND_PTR_SEG), si = DSW(SND_PTR_OFF);
        u8 op = rd8(seg, si);
        if ((s8)op >= 0) {                                         /* note / rest event */
            u16 ax = rd16(seg, (u16)(si + 1));
            DSW(DS_note_left) = ax;
            DSW(SND_PTR_OFF) = (u16)(DSW(SND_PTR_OFF) + 3);
            if (op == 0) {
                DSW(DS_note_cut) = 0;
                spk_off();
                return;
            }
            u8 cl = DSB(DS_snd_shift);
            /* shr ax,cl — 8086 semantics (count not masked). TODO(verify): 286+ masks cl to 5 bits;
             * shipped data only uses shifts 0 and 3. */
            ax = (cl == 0) ? 0 : (cl >= 16 ? 0 : (u16)(ax >> cl));
            DSW(DS_note_cut) = ax;
            spk_note(DSW((u16)(DS_note_div + op * 2)));          /* divisor read at fetch time */
            return;
        }

        switch ((u8)-op) {                                         /* CS:6B42[-op] */
        case 1:                                                    /* FF: end of stream */
            goto end_of_stream;
        case 2:                                                    /* FE s: articulation shift */
            DSB(DS_snd_shift) = rd8(seg, (u16)(si + 1));
            DSW(SND_PTR_OFF) = (u16)(DSW(SND_PTR_OFF) + 2);
            break;
        case 3: case 4: case 5: {                                  /* FD/FC/FB n16: loop start */
            u16 k = (u16)((u8)-op - 3);
            DSW((u16)(DS_loop_cnt + k * 2)) = rd16(seg, (u16)(si + 1));
            DSW(SND_PTR_OFF) = (u16)(DSW(SND_PTR_OFF) + 3);
            DSW((u16)(DS_loop_start + k * 2)) = DSW(SND_PTR_OFF);
            break;
        }
        case 6: case 7: case 8: {                                  /* FA/F9/F8: loop end */
            u16 k = (u16)((u8)-op - 6);
            u16 cnt = (u16)(DSW((u16)(DS_loop_cnt + k * 2)) - 1);
            DSW((u16)(DS_loop_cnt + k * 2)) = cnt;
            if ((s16)cnt < 0) {                                    /* js */
                DSW(SND_PTR_OFF) = (u16)(DSW(SND_PTR_OFF) + 1);
            } else {
                DSW((u16)(DS_loop_end + k * 2)) = si;              /* offset of this FA/F9/F8 byte */
                DSW(SND_PTR_OFF) = DSW((u16)(DS_loop_start + k * 2));
            }
            break;
        }
        case 9: case 10: case 11: {                                /* F7/F6/F5: skip on last pass */
            u16 k = (u16)((u8)-op - 9);
            if (DSW((u16)(DS_loop_cnt + k * 2)) == 0)
                DSW(SND_PTR_OFF) = DSW((u16)(DS_loop_end + k * 2));  /* possibly stale (original bug) */
            else
                DSW(SND_PTR_OFF) = (u16)(DSW(SND_PTR_OFF) + 1);
            break;
        }
        default:
            /* PORT: opcodes 0x80..0xF4 jump through garbage past CS:6B42 in the original (never in the
             * shipped data); stop the stream instead. */
            goto stop;
        }
        continue;

    end_of_stream:                                                 /* 0x6B00 */
        if (DSB(DS_snd_flags) & 2) {
            DSB(DS_snd_flags) &= 6;
            DSB(DS_snd_playing) = 2;
            DSW(SND_PTR_OFF) = DSW(SND_LOOP_OFF);
            DSW(SND_PTR_SEG) = DSW(SND_LOOP_SEG);
            continue;
        }
    stop:                                                          /* 0x6B1F */
        DSB(DS_snd_playing) = 0;
        DSB(DS_snd_flags) &= 6;
        DSW(DS_note_left) = 0;
        spk_off();
        return;
    }
}

/* 0x6A1F timer_isr body */
void timer_isr(void)
{
    DSW(DS_tick_count)++;
    DSW(DS_chain_count)--;
    if ((s16)DSW(DS_chain_count) <= 0) {
        DSW(DS_chain_count) = DSW(DS_chain_reload);
        /* PORT: BIOS INT 8 chaining (chain_enable) dropped. */
    }
    if (DSB(DS_snd_playing) != 0) {
        if (DSB(DS_modal_pause) != 0) {
            spk_off();
        } else if (DSW(DS_note_left) == 0) {
            snd_fetch();
        } else {
            u16 old = DSW(DS_note_left);
            DSW(DS_note_left) = (u16)(old - 1);
            if (old == DSW(DS_note_cut)) spk_off();
        }
    }
    /* PORT: EOI dropped. */
}

/* ---- tick helpers */

/* 0x9A01 */
u16 ticks_now(void)
{
    return DSW(DS_tick_count);
}

/* 0x9A05 |now - start| (unsigned absolute difference; a span across the 16-bit wrap ends early) */
u16 ticks_elapsed(u16 start)
{
    u16 now = DSW(DS_tick_count);
    return (start > now) ? (u16)(start - now) : (u16)(now - start);
}

/* 0x6C3B */
void delay_ticks(u16 n)
{
    u16 start = ticks_now();
    while (ticks_elapsed(start) < n) host_pump();
}

/* 0x7894 */
void set_deadline(u16 ticks)
{
    DSW(DS_deadline_start) = ticks_now();
    DSW(DS_deadline_len) = ticks;
}

/* 0x7881 */
void wait_deadline(void)
{
    while (ticks_elapsed(DSW(DS_deadline_start)) < DSW(DS_deadline_len)) host_pump();
}

/* ---- sound API */

/* 0x89F0 */
void snd_stop_oneshot(void)
{
    DSB(DS_snd_flags) &= 6;
}

/* 0x8A08 */
void snd_stop_all(void)
{
    DSB(DS_snd_flags) &= 4;
}

/* 0x8A0E: set the background loop stream; start it only when idle */
void snd_set_loop(FarPtr stream)
{
    DSW(SND_LOOP_SEG) = stream.seg;
    DSW(SND_LOOP_OFF) = stream.off;
    if (DSB(DS_snd_playing) == 0) {
        DSB(DS_snd_playing) = 2;
        DSW(SND_PTR_SEG) = stream.seg;
        DSW(SND_PTR_OFF) = stream.off;
    }
    DSB(DS_snd_flags) |= 2;
}

/* 0x8A3E: start a stream now; note_left is not reset, so the sounding note finishes its count */
void snd_play_oneshot(FarPtr stream)
{
    DSW(SND_PTR_SEG) = stream.seg;
    DSW(SND_PTR_OFF) = stream.off;
    DSB(DS_snd_playing) = 1;
    DSB(DS_snd_flags) |= 1;
}
