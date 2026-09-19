/* simulation: driving ISR 0x3B1F..0x4791 (port/spec/simulation.md §4.2–§4.12, platform.md §3.3).
 *
 * The original is one hand-written interrupt handler. It is ported at register level: the labels are
 * static functions that take the register set (Reg) and return the label the asm jumps to next; the
 * dispatcher in sim_run_tick() follows them until the `iret` at 0x4673. Registers are modelled so that
 * values that cross labels (stale CH/BH, DX from sim_update_rpm, BX of the look-ahead) behave as in the
 * original. All game state is in mem[]. */
#include "game.h"
#include "../platform/timer.h"
#include "../platform/input.h"
#include "../platform/res.h"

/* Names for offsets that have no dedicated symbol */
#define DS_speed              DS_g_carSpeedFixed          /* u16 8.8, mph in the high byte */
#define DS_speed_hi           (DS_g_carSpeedFixed + 1)
#define DS_run_state          DS_g_stageEvent
#define DS_clock_ticks        DS_g_stageTime
#define DS_cars_left          DS_g_lives
#define DS_knob_target_y      (DS_knob_target_x + 2)      /* 0A83 */
#define DS_car_knob_y0        (DS_car_knob_xy + 2)        /* 26B1 neutral row */
#define DS_sinq_jt_lateral    EGA_CGA(0x205F, 0x2032)     /* jump table: 0x41C9 0x416F 0x417A 0x4185 0x4190 0x41BC */
#define DS_sinq_jt_grip       EGA_CGA(0x206B, 0x203E)     /* jump table: 0x40BD 0x4044 0x404C 0x4057 0x4062 0x40B0 */
#define DS_road_record        EGA_CGA(0x2B70, 0x2B40)     /* [flag, curve, pitch, object] */
/* Code addresses stored in the DGROUP jump tables: the TDCGA ISR is the TDEGA one moved down by 0xC8. */
#define SIM_CS(a)             ((u16)((a) - EGA_CGA(0, 0xC8)))

typedef struct { u16 ax, bx, cx, dx, si, di, bp; } Reg;

#define LO(w) ((u8)(w))
#define HI(w) ((u8)((w) >> 8))
static inline u16 setlo(u16 w, u8 v) { return (u16)((w & 0xFF00) | v); }
static inline u16 sethi(u16 w, u8 v) { return (u16)((w & 0x00FF) | (u16)(v << 8)); }

typedef enum {
    L_IRET,          /* 0x4673 */
    L_TICK,          /* 0x3BEA sim_tick */
    L_CHARKEY,       /* 0x3BB4 sim_input_charkey */
    L_CONTROLS,      /* 0x3C10 */
    L_DIRECTION,     /* 0x3CB8 sim_input_direction */
    L_STEERING,      /* 0x3D7D sim_steering */
    L_KNOB,          /* 0x3DDB sim_shift_knob_anim */
    L_ENGINE,        /* 0x3EAA sim_engine */
    L_BRAKE,         /* 0x3F19 */
    L_DECEL,         /* 0x3F1C */
    L_COAST,         /* 0x3F63 */
    L_APPLY,         /* 0x3F74 */
    L_MOTION,        /* 0x3FE8 sim_motion */
    L_GRIP_DONE,     /* 0x40DB */
    L_ADVANCE,       /* 0x40FE sim_advance_unit */
    L_UNIT_NEXT,     /* 0x425B */
    L_SPAWN,         /* 0x467D sim_spawn_object */
    L_TRAFFIC,       /* 0x427F sim_traffic_update */
    L_COP,           /* 0x438A sim_cop_fsm */
    L_CLEANUP        /* 0x455F sim_object_cleanup */
} Label;

/* ---------------------------------------------------------------------------------------------- */
/* 0x3FAD sim_update_rpm — result in DX (unchanged in neutral / clutch in: quirk Q3) */
static void sim_update_rpm(Reg *r)
{
    DSB(DS_gauges_dirty) = 1;
    if (DSB(DS_gear) == 0) return;
    if (DSB(DS_fire_held) == 1) return;
    u32 p = (u32)DSW(DS_cur_ratio) * DSW(DS_speed);        /* mul dx */
    r->ax = (u16)p;
    r->dx = (u16)(p >> 16);
    if (r->dx < 800) {                                     /* jb */
        r->dx = 800;
        DSW(DS_rpm) = 800;
        return;
    }
    DSW(DS_rpm) = r->dx;
    if (r->dx > DSW(DS_car_rpm_limit))                     /* ja */
        DSB(DS_run_state) = 3;
}

/* Interpolated SINQ lookup through the jump tables DS:206B (grip, fraction in CL) and DS:205F
 * (lateral, fraction in DL). Entry: BL = index (BH = 0). Exit: AL = value, AH = difference byte. */
static void sinq_interp(Reg *r, u16 jtab, u16 t0, u16 t1, u16 t2, u16 t3, u16 t4, u16 t5, u8 *frac)
{
    u8 i  = LO(r->bx);
    u8 al = DSB(DS_sinq_table + i);
    u8 ah = (u8)(DSB(DS_sinq_table + i + 1) - al);         /* sub ah, al */
    u16 s;
    r->bx = (u16)(ah << 1);                                /* mov bl,ah; shl bx,1 (bh = 0) */
    u16 target = DSW(jtab + r->bx);
    u8 f = *frac;

    if (target == t0) {
        /* no fractional part */
    } else if (target == t1) {                             /* shl f,1; adc ax,0 */
        u16 c = f >> 7;
        *frac = (u8)(f << 1);
        r->ax = (u16)((((u16)ah << 8) | al) + c);
        return;
    } else if (target == t2) {                             /* 3f >> 8 */
        r->bx = (u16)(setlo(r->bx, f) << 1);
        s = (u16)(LO(r->bx) + f);
        r->bx = setlo(r->bx, (u8)s);
        al = (u8)(al + HI(r->bx) + (s >> 8));
    } else if (target == t3) {                             /* 4f >> 8 */
        r->bx = (u16)(setlo(r->bx, f) << 2);
        al = (u8)(al + HI(r->bx));
    } else if (target == t4) {                             /* 5f >> 8 */
        r->bx = (u16)(setlo(r->bx, f) << 2);
        s = (u16)(LO(r->bx) + f);
        r->bx = setlo(r->bx, (u8)s);
        al = (u8)(al + HI(r->bx) + (s >> 8));
    } else if (target == t5) {                             /* 6f >> 8 */
        r->bx = (u16)(setlo(r->bx, f) << 1);
        s = (u16)(LO(r->bx) + f);
        r->bx = setlo(r->bx, (u8)s);
        r->bx = sethi(r->bx, (u8)(HI(r->bx) + (s >> 8)));
        r->bx = (u16)(r->bx << 1);
        al = (u8)(al + HI(r->bx));
    } else {
        /* TODO(verify): difference > 5 jumps through the following table/data; not reachable with the
         * shipped SINQ table (max step 5, index <= 0x4B). Treated as no fractional part. */
    }
    r->ax = (u16)(((u16)ah << 8) | al);
}

/* ---------------------------------------------------------------------------------------------- */
/* 0x3BEA sim_tick — input fetch */
static Label sim_tick(Reg *r)
{
    r->cx = 0;
    DSW(DS_accel_mag) = r->cx;
    DSB(DS_throttle_in) = LO(r->cx);
    DSB(DS_steer_in) = LO(r->cx);
    r->ax = input_poll_drive();
    /* Q3: the original leaves the raw input word (joystick bits / BIOS key word) in DX, which the
     * fire-release clash test reads when sim_update_rpm returns early in neutral.
     * TODO(verify): modelled as the returned word; the joystick path of 0x5C24 also clobbers CX
     * (0xA12F measuring loop), modelled as CX = 0. */
    r->dx = r->ax;
    if ((r->ax != 0 && DSW(DS_demo_mode) != 0) || LO(r->ax) == 0xFF) {
        DSB(DS_run_state) = 1;                             /* 0x3BA5 */
        return L_MOTION;
    }
    if (HI(r->ax) == 0xFF) return L_CHARKEY;
    return L_CONTROLS;
}

/* 0x3BB4 sim_input_charkey — D and O keys */
static Label sim_input_charkey(Reg *r)
{
    u8 al = LO(r->ax);
    if (al == 0x64 || al == 0x44) {
        DSB(DS_gearbox_display_toggle) ^= 1;
    } else if (al == 0x6F || al == 0x4F) {
        DSB(DS_gate_shift_mode) ^= 1;
        if (DSB(DS_gate_shift_mode) != 0) {
            r->ax = setlo(r->ax, DSB(DS_gear));
            if (LO(r->ax) == 0) {
                DSB(DS_gate_node) = 1;                     /* 0x3B98 */
            } else {
                r->cx = 0x10;                              /* repne scasb over DS:279B */
                r->di = DS_car_node_gear;
                while (r->cx != 0) {
                    r->cx--;
                    u8 b = DSB(r->di);
                    r->di++;
                    if (b == LO(r->ax)) break;
                }
                r->cx ^= 0xF;
                DSB(DS_gate_node) = LO(r->cx);
            }
        }
    }
    r->ax = setlo(r->ax, 0);                               /* 0x3B9D */
    return L_CONTROLS;
}

/* 0x3C10 autopilot overrides, 0x3C1E fire-release clash check */
static Label sim_controls(Reg *r)
{
    if (DSB(DS_run_state) == 2) {                          /* 0x3C4D stage-end autopilot */
        r->ax = setlo(r->ax, 5);
        DSW(DS_clock_ticks)--;
        if (!((s16)DSW(DS_rpm) > 0xBB8) && DSB(DS_knob_anim) != 1 && !((s8)DSB(DS_gear) <= 1))
            r->ax |= 0x10;
    } else if (DSW(DS_demo_mode) != 0) {                   /* 0x3C99 demo autopilot */
        DSB(DS_dash_timer) = 10;
        r->ax = (u16)(DSW(DS_rpm) + 0x3E8);
        bool less = (s16)r->ax < (s16)DSW(DS_car_rpm_limit);
        r->ax = setlo(r->ax, 1);
        if (!less && DSB(DS_knob_anim) != 1)
            r->ax |= 0x10;
    }

    /* 0x3C1E */
    if ((r->ax & 0x10) || DSB(DS_fire_held) != 1 || DSB(DS_knob_anim) == 1)
        return L_DIRECTION;
    DSB(DS_fire_held) = 0;
    r->cx = DSW(DS_rpm);
    sim_update_rpm(r);
    r->cx = (u16)(r->cx - r->dx);
    if ((s16)r->cx < -2500) {                              /* 0x3C70 */
        DSB(DS_grind_timer) = 3;
        r->ax = (u16)(DSW(DS_speed) - 0x500);
        DSW(DS_speed) = r->ax;
    } else if ((s16)r->cx > 3500) {                        /* 0x3C81 */
        if (DSB(DS_gear) == 1) {
            DSB(DS_grind_timer) = 0x12;
            r->ax = (u16)(DSW(DS_speed) + 0x1E00);
            DSW(DS_speed) = r->ax;
        }
    }
    return L_STEERING;
}

/* 0x3CB8 sim_input_direction — shift gate / gear delta / steer and throttle */
static Label sim_input_direction(Reg *r)
{
    r->bx = r->ax & 0xF;
    if (!(r->ax & 0x10)) {                                 /* 0x3D5D */
        DSB(DS_shift_latch) = 0;
        if (DSB(DS_knob_anim) != 0) return L_STEERING;
        DSB(DS_fire_held) = 0;
        r->ax = setlo(r->ax, DSB(DS_steer_dir_table + r->bx));
        DSB(DS_steer_in) = LO(r->ax);
        r->dx = setlo(r->dx, DSB(DS_throttle_dir_table + r->bx));
        DSB(DS_throttle_in) = LO(r->dx);
        return L_STEERING;
    }

    DSB(DS_fire_held) = 1;
    DSB(DS_gearbox_dirty) = 1;
    DSB(DS_dash_timer) = 0xD;
    if (DSB(DS_gate_shift_mode) != 0 && DSB(DS_run_state) == 0 && DSB(DS_joy_enabled) == 1) {
        r->dx = setlo(r->dx, 0);
        if (LO(r->bx) == 0) goto store_latch;
        r->dx = setlo(r->dx, 1);
        r->bx = (u16)(r->bx << 4);
        r->bx = setlo(r->bx, (u8)(LO(r->bx) + DSB(DS_gate_node)));   /* byte add, carry dropped */
        r->bx = setlo(r->bx, DSB(DS_car_gate_next + r->bx));
        DSB(DS_gate_node) = LO(r->bx);
        r->ax = setlo(r->ax, DSB(DS_car_node_gear + r->bx));
        DSB(DS_gear) = LO(r->ax);
        r->bx = setlo(r->bx, (u8)(LO(r->bx) + 7));          /* knob xy of gate node */
        goto knob_target;
    }

    r->dx = setlo(r->dx, 0);                               /* 0x3D12 */
    r->cx = setlo(r->cx, DSB(DS_gear_delta_table + r->bx));
    if (LO(r->cx) == LO(r->dx)) goto store_latch;
    r->ax = setlo(r->ax, (u8)(DSB(DS_gear) + LO(r->cx)));
    if (LO(r->ax) > DSB(DS_car_num_gears)) goto store_latch;   /* ja: gear -1 fails */
    r->dx = setlo(r->dx, 1);
    if (DSB(DS_shift_latch) == 1) goto store_latch;
    if (DSB(DS_knob_anim) == 1) goto store_latch;
    r->ax &= 0x00FF;
    DSB(DS_gear) = LO(r->ax);
    r->bx = r->ax;

knob_target:                                               /* 0x3D3E */
    r->bx = (u16)(r->bx << 2);
    r->ax = DSW(DS_car_knob_xy + r->bx);
    r->cx = DSW(DS_car_knob_xy + 2 + r->bx);
    DSW(DS_knob_target_x) = r->ax;
    DSW(DS_knob_target_y) = r->cx;
    DSB(DS_knob_anim) = 1;
store_latch:                                               /* 0x3D56 */
    DSB(DS_shift_latch) = LO(r->dx);
    return L_STEERING;
}

/* 0x3D7D sim_steering — no self-centring */
static Label sim_steering(Reg *r)
{
    r->bx = DSW(DS_steer_angle);
    r->cx = setlo(r->cx, DSB(DS_steer_in));
    r->dx = (u16)-DSW(DS_road_curve);
    r->ax = 0xE6;
    if ((s8)LO(r->cx) == 0) return L_KNOB;
    if ((s8)LO(r->cx) > 0) {                               /* 0x3DAA */
        if ((s16)r->bx < (s16)r->dx) {                     /* 0x3DC1 */
            r->ax = (u16)(r->ax + r->bx);
            if ((s16)r->ax < (s16)r->dx) goto store;
            goto store_dx;
        }
    } else {
        r->ax = (u16)-r->ax;
        if ((s16)r->bx > (s16)r->dx) {                     /* 0x3DA1 */
            r->ax = (u16)(r->ax + r->bx);
            if ((s16)r->ax > (s16)r->dx) goto store;
            goto store_dx;
        }
    }
    r->ax = (u16)((s16)r->ax >> 1);                        /* +-115 away from the road angle */
    r->ax = (u16)(r->ax + r->bx);
    if ((s16)r->ax > 0x0EC0) r->ax = 0x0EC0;              /* 0x3DCE clamp */
    else if ((s16)r->ax < -0x0EC0) r->ax = 0xF140;
store:
    DSW(DS_steer_angle) = r->ax;
    return L_KNOB;
store_dx:
    DSW(DS_steer_angle) = r->dx;
    return L_KNOB;
}

/* 0x3DDB sim_shift_knob_anim — knob movement and shift completion */
static Label sim_shift_knob_anim(Reg *r)
{
    if (DSB(DS_knob_anim) != 1) return L_ENGINE;
    DSB(DS_gearbox_dirty) = 1;
    DSB(DS_clock_running) = 1;
    r->ax = DSW(DS_knob_x);
    r->bx = DSW(DS_knob_y);
    if (r->ax == DSW(DS_knob_target_x)) {
        if ((s16)r->bx < (s16)DSW(DS_knob_target_y)) r->bx = (u16)(r->bx + 6);
        else                                          r->bx = (u16)(r->bx - 6);
    } else if (r->bx == DSW(DS_car_knob_y0)) {
        if ((s16)r->ax < (s16)DSW(DS_knob_target_x)) r->ax = (u16)(r->ax + 6);
        else                                          r->ax = (u16)(r->ax - 6);
    } else if ((s16)r->bx > (s16)DSW(DS_car_knob_y0)) {
        r->bx = (u16)(r->bx - 6);
    } else {
        r->bx = (u16)(r->bx + 6);
    }
    DSW(DS_knob_x) = r->ax;
    DSW(DS_knob_y) = r->bx;
    if (r->ax != DSW(DS_knob_target_x)) return L_ENGINE;
    if (r->bx != DSW(DS_knob_target_y)) return L_ENGINE;

    DSB(DS_knob_anim) = HI(r->bx);                         /* mov [..], bh (0 for screen rows) */
    DSB(DS_fire_held) = HI(r->bx);
    r->bx = setlo(r->bx, DSB(DS_gear));
    if (LO(r->bx) == HI(r->bx)) return L_ENGINE;
    r->bx = (u16)(r->bx << 1);
    r->ax = DSW(DS_car_gear_ratio + r->bx);
    DSW(DS_cur_ratio) = r->ax;
    r->cx = DSW(DS_rpm);
    sim_update_rpm(r);
    r->cx = (u16)(r->cx - DSW(DS_rpm));
    if ((s16)r->cx < -2500) {                              /* 0x3E6B */
        DSB(DS_grind_timer) = 3;
        r->ax = sethi(r->ax, (u8)(DSB(DS_speed_hi) - 5));
        DSB(DS_speed_hi) = HI(r->ax);
        return L_BRAKE;
    }
    if ((s16)r->cx > 3500) {                               /* 0x3E7E */
        if (DSB(DS_gear) == 1) {
            DSB(DS_grind_timer) = 0x12;
            r->ax = sethi(r->ax, (u8)(DSB(DS_speed_hi) + 0x1E));
            DSB(DS_speed_hi) = HI(r->ax);
        }
        return L_BRAKE;
    }
    return L_MOTION;                                       /* engine skipped this tick */
}

/* 0x3EAA sim_engine — idle decay, skid, brake, free rev, torque */
static Label sim_engine(Reg *r)
{
    if (DSB(DS_fire_held) == 1 || DSB(DS_gear) == 0) {
        r->ax = (u16)(DSW(DS_rpm) - 0x12C);
        if ((s16)r->ax <= 0x320) {                         /* 0x3E98 */
            r->ax = 0x320;
            if (r->ax != DSW(DS_rpm)) {
                DSW(DS_rpm) = r->ax;
                DSB(DS_gauges_dirty) = 1;
            }
        } else {
            DSW(DS_rpm) = r->ax;
            DSB(DS_gauges_dirty) = 1;
        }
    }
    if (DSB(DS_skidding) == 1) {                           /* 0x3EA4 no throttle while skidding */
        r->dx = 0x30;
        return L_DECEL;
    }
    r->dx = DSB(DS_throttle_in);                           /* xor dh,dh; mov dl,[0A79] */
    if ((s8)LO(r->dx) == 0) return L_COAST;
    if ((s8)LO(r->dx) < 0) return L_BRAKE;
    if (DSB(DS_cop_state) != 7 && (s8)DSB(DS_cop_state) >= 3)
        return L_MOTION;                                   /* being pulled over */
    if (DSB(DS_gear) != 0) {                               /* 0x3F36 torque */
        r->bx = (u16)(DSW(DS_rpm) << 1);
        r->bx = HI(r->bx);                                 /* mov bl,bh; xor bh,bh */
        if ((s16)r->bx > 0x50) r->bx = 0x50;
        r->ax = setlo(r->ax, DSB(DS_car_torque + r->bx));
        r->dx = DSW(DS_cur_ratio);
        u8 c = LO(r->dx) >> 7;                             /* shl dl,1; adc dh,bh */
        r->dx = setlo(r->dx, (u8)(LO(r->dx) << 1));
        r->dx = sethi(r->dx, (u8)(HI(r->dx) + HI(r->bx) + c));
        r->ax = (u16)(LO(r->ax) * HI(r->dx));              /* mul dh */
        r->dx = r->ax;
        if (DSB(DS_gear) == 1)
            r->dx = (u16)((r->dx >> 1) + r->ax);           /* x1.5 */
        return L_APPLY;
    }
    r->ax = (u16)(DSW(DS_rpm) + 0x320);                    /* free revving */
    DSW(DS_rpm) = r->ax;
    if ((s16)r->ax > (s16)DSW(DS_car_rpm_limit))
        DSB(DS_run_state) = 3;
    return L_COAST;
}

/* 0x3F19 brake / 0x3F1C decel (DX = deceleration) */
static Label sim_decel(Reg *r)
{
    DSW(DS_accel_mag) = r->dx;
    u16 spd = DSW(DS_speed);
    r->ax = (u16)(spd - r->dx);
    if (spd < r->dx) r->ax = 0;                            /* jb 0x3F14 */
    DSW(DS_speed) = r->ax;
    sim_update_rpm(r);
    return L_MOTION;
}

/* 0x3F63 coast: in gear without gas speed is held (Q5) */
static Label sim_coast(Reg *r)
{
    if (DSB(DS_fire_held) == 1) return L_APPLY;
    if (DSB(DS_gear) == 0) return L_APPLY;
    return L_MOTION;
}

/* 0x3F74 apply force DX minus drag */
static Label sim_apply(Reg *r)
{
    r->ax = DSW(DS_speed);
    r->bx = (u16)(HI(r->ax) >> 2);
    r->cx = (u16)((u16)(DSB(DS_drag_table + r->bx) << 8) >> 2);
    r->dx = (u16)(r->dx - r->cx);
    r->dx = (u16)((s16)r->dx >> 6);
    r->ax = (u16)(r->ax + r->dx);
    DSW(DS_speed) = r->ax;
    if (!((s16)r->dx > 0)) r->dx = (u16)-r->dx;
    DSW(DS_accel_mag) = r->dx;
    sim_update_rpm(r);
    return L_MOTION;
}

/* ---------------------------------------------------------------------------------------------- */
/* 0x3FE8 sim_motion — sub-unit step and grip check */
static Label sim_motion(Reg *r)
{
    r->ax = DSW(DS_road_pos);
    DSW(DS_road_pos_prev) = r->ax;
    r->ax = DSW(DS_speed);
    u8 step = (u8)(HI(r->ax) + (LO(r->ax) >> 7));          /* shl al,1; adc ah,0 */
    r->ax = (u16)-(u16)step;
    int sum = (s16)r->ax + (s16)DSW(DS_sub_unit);
    r->ax = (u16)sum;
    if (sum >= 0) {                                        /* jl 0x400D not taken */
        DSW(DS_sub_unit) = r->ax;
        DSB(DS_unit_advanced) = 0;
        return L_TRAFFIC;
    }
    DSB(DS_unit_advanced) = 1;
    DSW(DS_sub_unit) = r->ax;
    r->bx &= 0x00FF;
    DSW(DS_note_div_skid) = 0xFFFF;
    DSB(DS_skidding) = HI(r->bx);
    r->cx = DSW(DS_steer_angle);
    if (HI(r->cx) == 0) return L_GRIP_DONE;                /* small positive angles never skid (Q4) */
    if (!((s8)HI(r->cx) > 0)) r->cx = (u16)-r->cx;
    r->bx = setlo(r->bx, HI(r->cx));
    u8 f = LO(r->cx);
    sinq_interp(r, DS_sinq_jt_grip, SIM_CS(0x40BD), SIM_CS(0x4044), SIM_CS(0x404C), SIM_CS(0x4057),
                SIM_CS(0x4062), SIM_CS(0x40B0), &f);
    r->cx = setlo(r->cx, f);
    /* 0x40BD */
    r->ax = sethi(r->ax, DSB(DS_speed_hi));
    r->ax = (u16)(LO(r->ax) * HI(r->ax));
    r->ax = (u16)(r->ax << 1);
    r->ax = (u16)(r->ax + DSW(DS_accel_mag));
    r->ax = (u16)(r->ax + DSW(DS_accel_mag));
    r->dx = (u16)(r->ax >> 2);
    r->ax = (u16)(r->ax + r->dx);
    if ((s16)r->ax >= (s16)DSW(DS_car_grip_limit)) {       /* 0x40A2 */
        DSB(DS_skidding) = 1;
        DSW(DS_note_div_skid) = 0x08E8;
    }
    return L_GRIP_DONE;
}

/* 0x40DB demo hold / stage-end and pull-over steering */
static Label sim_grip_done(Reg *r)
{
    r->ax = DSW(DS_sub_unit);
    r->dx = DSW(DS_yaw_lateral);
    if (DSW(DS_demo_mode) == 1) goto hold;
    if (DSB(DS_run_state) == 2) goto pullover;
    if (DSB(DS_cop_state) == 7) return L_ADVANCE;
    if ((s8)DSB(DS_cop_state) >= 3) goto pullover;
    return L_ADVANCE;

pullover:                                                  /* 0x4094 */
    r->bx = DSW(DS_car_x);
    if ((s16)r->bx >= -100) {
        r->bx = (u16)(r->bx - 0x28);
        DSW(DS_car_x) = r->bx;
    } else if (!((s16)r->bx > -150)) {                     /* 0x406F */
        r->bx = (u16)(r->bx + 0x28);
        DSW(DS_car_x) = r->bx;
    }
hold:                                                      /* 0x407C */
    DSB(DS_skidding) = 0;
    DSW(DS_yaw_lateral) = 0;
    r->bx = (u16)-DSW(DS_road_curve);
    DSW(DS_steer_angle) = r->bx;
    return L_ADVANCE;
}

/* 0x40FE sim_advance_unit — lateral drift, road edge, road read, speed-limit signs, 0x4241 look-ahead */
static Label sim_advance_unit(Reg *r)
{
    r->ax = (u16)(r->ax + 0x5A);
    DSW(DS_sub_unit) = r->ax;
    r->cx = DSW(DS_steer_angle);
    if (DSB(DS_skidding) != 1) {
        r->dx = (u16)(r->cx + DSW(DS_road_curve));
        DSW(DS_yaw_lateral) = r->dx;
        DSW(DS_view_heading) = r->dx;
    } else {                                               /* 0x4120 */
        r->dx = (u16)(r->cx + DSW(DS_road_curve));
        r->ax = (u16)(DSB(DS_car_skid_drift) << 8);
        if ((s16)r->cx < 0) r->ax = (u16)-r->ax;
        r->ax = (u16)((s16)r->ax >> 3);
        r->ax = (u16)(r->ax + r->dx);
        DSW(DS_view_heading) = r->ax;
        r->dx = (u16)(DSW(DS_road_curve) << 1);
        r->dx = (u16)(r->dx + DSW(DS_steer_angle));
        r->dx = (u16)((s16)r->dx >> 1);
        DSW(DS_yaw_lateral) = r->dx;
    }

    /* 0x414E lateral step */
    r->bx = setlo(r->bx, HI(r->dx));
    if (LO(r->bx) != 0) {
        if (!((s8)LO(r->bx) > 0)) {                        /* Q4: byte-wise negation */
            r->bx = setlo(r->bx, (u8)-LO(r->bx));
            r->dx = setlo(r->dx, (u8)-LO(r->dx));
        }
        r->bx &= 0x00FF;
        u8 f = LO(r->dx);
        sinq_interp(r, DS_sinq_jt_lateral, SIM_CS(0x41C9), SIM_CS(0x416F), SIM_CS(0x417A), SIM_CS(0x4185),
                    SIM_CS(0x4190), SIM_CS(0x41BC), &f);
        r->dx = setlo(r->dx, f);
        /* 0x41C9 */
        r->ax &= 0x00FF;
        if ((s8)HI(r->dx) < 0) r->ax = (u16)-r->ax;
        r->ax = (u16)(r->ax << 1);
        r->ax = (u16)(r->ax + DSW(DS_car_x));
        DSW(DS_car_x) = r->ax;
        if ((s16)r->ax > 0x264 || (s16)r->ax < -0x236)
            DSB(DS_run_state) = 3;                         /* 0x419D off the road */
    }

    /* 0x41E5 */
    r->ax = DSW(DS_yaw_lateral);
    if ((s16)r->ax < -0x4B00)     r->ax = 0xB500;
    else if ((s16)r->ax > 0x4B00) r->ax = 0x4B00;
    DSW(DS_yaw_lateral) = r->ax;
    r->dx = r->ax;
    r->bx = (u16)(DSW(DS_road_pos) + 1);
    DSW(DS_road_pos) = r->bx;
    r->ax = setlo(r->ax, DSB(r->bx));
    if (DSB(DS_run_state) == 2) {                          /* 0x41B1 */
        if (!((s16)r->bx < (s16)DSW(DS_stage_end_pos)))
            r->ax = setlo(r->ax, 0);
    }
    /* 0x4209 */
    r->ax = (u16)(s16)(s8)LO(r->ax);                       /* cbw (Q10) */
    r->ax = (u16)(r->ax << 2);
    r->si = (u16)(DS_road_record_curve + r->ax);
    r->ax = (u16)(DSB(r->si) << 8);
    r->ax = (u16)((s16)r->ax >> 2);
    DSW(DS_road_curve) = r->ax;
    r->cx = setlo(r->cx, DSB(r->si + 2));
    if ((s8)LO(r->cx) < 0x10) {
        int t = (s8)LO(r->cx) - 5;
        r->cx = setlo(r->cx, (u8)t);
        if (t >= 0 && (s8)LO(r->cx) < 3) {
            r->cx = setlo(r->cx, (u8)(LO(r->cx) << 1));
            DSB(DS_speed_limit_idx) = LO(r->cx);           /* low byte only */
        }
    }
    DSB(DS_road_anim_counter)++;
    if (DSB(DS_run_state) == 2) return L_UNIT_NEXT;

    /* 0x4241 sim_lookahead */
    r->bx = (u16)(r->bx + 0x28);
    r->ax = setlo(r->ax, DSB(r->bx));
    if (LO(r->ax) == 0xFF) {                               /* 0x4276 stage end */
        DSB(DS_run_state) = 2;
        DSW(DS_stage_end_pos) = r->bx;
        return L_TRAFFIC;
    }
    r->ax = (u16)(s16)(s8)LO(r->ax);
    r->ax = (u16)(r->ax << 2);
    r->si = (u16)(DS_road_record_curve + r->ax);
    r->ax = setlo(r->ax, DSB(r->si + 2));
    if (LO(r->ax) != 0) return L_SPAWN;
    return L_UNIT_NEXT;
}

/* 0x425B loop while sub_unit < 0 (grip check and pull-over are not repeated) */
static Label sim_unit_next(Reg *r)
{
    r->ax = DSW(DS_sub_unit);
    if ((s16)r->ax >= 0) return L_TRAFFIC;
    return L_ADVANCE;
}

/* 0x467D sim_spawn_object — AL = object byte, BX = look-ahead position */
static Label sim_spawn_object(Reg *r)
{
    u8 ob = LO(r->ax);
    r->cx = setlo(r->cx, ob & 0x3F);

    if (LO(r->cx) == 1) {                                  /* radar trap */
        if (DSB(DS_cop_state) == 0) {
            r->cx = setlo(r->cx, ob & 0xC0);
            r->ax = rand8();
            if (!(LO(r->ax) < LO(r->cx))) {
                r->bx = (u16)(r->bx + 0x28);
                DSW(DS_radar_trap) = r->bx;
            }
        }
        return L_UNIT_NEXT;
    }

    if ((s8)LO(r->cx) < 0x18) {                            /* oncoming 0x10..0x14 */
        int t = (s8)LO(r->cx) - 0x10;
        r->cx = setlo(r->cx, (u8)t);
        if (t < 0) return L_UNIT_NEXT;
        if ((s8)LO(r->cx) >= 5) return L_UNIT_NEXT;
        if (DSB(DS_oncoming_count) == 2) return L_UNIT_NEXT;
        if (DSB(DS_cop_state) != 0) return L_UNIT_NEXT;
        r->dx = setlo(r->dx, ob & 0xC0);
        r->ax = rand8();
        if (LO(r->ax) < LO(r->dx)) return L_UNIT_NEXT;
        r->dx = r->cx;
        /* std; rep movsw: 16 words DS:0945.. -> DS:094D.. (backwards), 5th entry dropped */
        for (r->si = EGA_CGA(0x0963, 0x094B), r->di = EGA_CGA(0x096B, 0x0953), r->cx = 0x10; r->cx != 0; r->cx--) {
            DSW(r->di) = DSW(r->si);
            r->si = (u16)(r->si - 2);
            r->di = (u16)(r->di - 2);
        }
        r->dx &= 0x00FF;
        r->ax = r->dx;
        r->dx = (u16)(((r->dx << 2) + r->ax) << 3);        /* type * 40 */
        r->bx = (u16)(r->bx + 0xF);
        DSW(DS_oncoming_list) = r->bx;
        DSW(DS_oncoming_list + 2) = 0;
        DSW(DS_oncoming_list + 6) = r->dx;
        DSB(DS_oncoming_count)++;
        return L_UNIT_NEXT;
    }

    if ((s8)LO(r->cx) < 0x20) {                            /* same direction 0x18..0x1C */
        r->cx = setlo(r->cx, (u8)(LO(r->cx) - 0x13));
        if ((s8)LO(r->cx) >= 0xA) return L_UNIT_NEXT;
        if (DSB(DS_samedir_count) == 5) return L_UNIT_NEXT;
        if ((s8)DSB(DS_cop_state) >= 3) return L_UNIT_NEXT;
        if (DSW(DS_demo_mode) != 0) return L_UNIT_NEXT;
        r->dx = setlo(r->dx, ob & 0xC0);
        r->ax = rand8();
        if (LO(r->ax) < LO(r->dx)) return L_UNIT_NEXT;
        r->dx = r->cx;
        for (r->si = EGA_CGA(0x098B, 0x0973), r->di = EGA_CGA(0x0993, 0x097B), r->cx = 0x10; r->cx != 0; r->cx--) {
            DSW(r->di) = DSW(r->si);
            r->si = (u16)(r->si - 2);
            r->di = (u16)(r->di - 2);
        }
        r->dx &= 0x00FF;
        r->ax = r->dx;
        r->dx = (u16)(((r->dx << 2) + r->ax) << 3);
        r->bx = (u16)(r->bx + 0xF);
        DSW(DS_samedir_list) = r->bx;
        DSW(DS_samedir_list + 2) = 0;
        DSW(DS_samedir_list + 6) = r->dx;
        DSB(DS_samedir_count)++;
        DSB(DS_cop_block_idx)++;
        return L_UNIT_NEXT;
    }

    /* 0x476D hazard 0x20..0x23 */
    r->cx = setlo(r->cx, (u8)(LO(r->cx) - 0x20));
    if ((s8)LO(r->cx) >= 4) return L_UNIT_NEXT;
    r->cx = setlo(r->cx, (u8)(LO(r->cx) << 4));
    r->cx &= 0x00FF;
    r->cx = sethi(r->cx, (u8)(ob >> 6));                   /* shl al / rcl ch, twice */
    r->ax = setlo(r->ax, (u8)(ob << 2));
    DSW(DS_hazard) = r->bx;
    DSW(DS_hazard + 6) = r->cx;
    return L_UNIT_NEXT;
}

/* ---------------------------------------------------------------------------------------------- */
/* 0x427F sim_traffic_update — grind tone, oncoming and same-direction lists */
static Label sim_traffic_update(Reg *r)
{
    if (DSB(DS_run_state) != 2 && DSB(DS_grind_timer) != 0) {   /* 0x4269 */
        DSB(DS_grind_timer)--;
        DSW(DS_note_div_skid) = 0x0474;
    }

    /* 0x428D oncoming: drive toward the player */
    r->cx = setlo(r->cx, DSB(DS_oncoming_count));
    if (LO(r->cx) != 0) {
        r->cx &= 0x00FF;
        r->si = DS_oncoming_list;
        do {
            r->bp = (u16)(DSW(r->si) - DSW(DS_road_pos_prev));
            r->bx = DSW(DS_speed_limit_idx);
            r->ax = DSW(DS_traffic_speed_table + r->bx);
            r->bx = DSW(r->si);
            int t = (s16)DSW(r->si + 2) - (s16)r->ax;
            r->dx = (u16)t;
            if (t < 0) {
                r->bx--;
                r->dx = (u16)(r->dx + 0x5A);
            }
            r->ax = (u16)(DSW(DS_road_pos) - r->bx);
            bool check = true;
            if (r->ax != 0) {
                r->ax ^= r->bp;
                if ((s16)r->ax < 0) check = false;
            }
            if (check && !((s16)DSW(DS_car_x) < 0x25)) {   /* 0x42C1 head-on collision */
                r->bx = DSW(DS_road_pos);
                r->dx = DSW(DS_sub_unit);
                r->bx++;
                DSB(DS_run_state) = 3;
            }
            if (LO(r->cx) != 1) {                          /* 0x42D6 keep 8 units ahead of the next one */
                r->ax = (u16)(DSW(r->si + 8) + 8);
                if (!((s16)r->bx > (s16)r->ax)) r->bx = r->ax;
            }
            DSW(r->si + 2) = r->dx;
            DSW(r->si) = r->bx;
            r->si = (u16)(r->si + 8);
        } while (--r->cx != 0);                            /* loop */
    }

    /* 0x42F1 same direction */
    r->cx = setlo(r->cx, DSB(DS_samedir_count));
    if (LO(r->cx) == 0) return L_COP;
    r->cx &= 0x00FF;
    r->si = DS_samedir_list;
    do {
        r->bp = (u16)(DSW(r->si) - DSW(DS_road_pos_prev));
        if (DSB(DS_cop_state) != 7 && !((s8)DSB(DS_cop_state) < 3)) {
            r->ax = 0x78;
        } else {
            r->bx = DSW(DS_speed_limit_idx);
            r->ax = DSW(DS_traffic_speed_table + r->bx);
        }
        r->bx = DSW(r->si);
        int t = (s16)DSW(r->si + 2) - (s16)r->ax;
        r->dx = (u16)t;
        if (t < 0) {
            r->bx++;
            t = (s16)r->dx + 0x5A;
            r->dx = (u16)t;
            if (t < 0) {
                r->bx++;
                r->dx = (u16)(r->dx + 0x5A);
            }
        }
        if (LO(r->cx) != DSB(DS_samedir_count)) {          /* stay 6 units behind the car ahead */
            r->ax = (u16)(DSW(r->si - 8) - 6);
            if (!((s16)r->ax > (s16)r->bx)) r->bx = r->ax;
        }
        r->ax = (u16)(DSW(DS_road_pos) - r->bx);           /* 0x4349 */
        if (!((s16)r->bp > 0)) {
            if (!((s16)r->ax > 6))                         /* cars behind never hit you */
                r->bx = (u16)(DSW(DS_road_pos) - 6);
        } else if (!((s16)r->ax < -1) && !((s16)DSW(DS_car_x) > 0x6C)) {
            r->bx = DSW(DS_road_pos);                      /* 0x436E rear-end collision */
            r->dx = DSW(DS_sub_unit);
            r->bx++;
            DSB(DS_run_state) = 3;
        }
        DSW(r->si + 2) = r->dx;                            /* 0x437C */
        DSW(r->si) = r->bx;
        r->si = (u16)(r->si + 8);
    } while (--r->cx != 0);
    return L_COP;
}

/* adv(n) with AX = n: sub dx,ax; up to two unit carries */
static void cop_adv(Reg *r)
{
    int t = (s16)r->dx - (s16)r->ax;
    r->dx = (u16)t;
    if (t < 0) {
        r->bx++;
        t = (s16)r->dx + 0x5A;
        r->dx = (u16)t;
        if (t < 0) {
            r->bx++;
            r->dx = (u16)(r->dx + 0x5A);
        }
    }
}

/* 0x438A sim_cop_fsm — dispatch through DS:0A36 */
static Label sim_cop_fsm(Reg *r)
{
    r->bx = DSW(DS_cop_pos);
    r->dx = DSW(DS_cop_sub);
    r->ax = (u16)(DSB(DS_cop_state) << 1);
    if (r->ax == 0) return L_CLEANUP;
    r->di = r->ax;

    switch (DSW(DS_cop_state_jump_table + r->di)) {
    case SIM_CS(0x43A4):                                           /* 1 chase */
        r->ax = DSW(DS_cop_speed);
        cop_adv(r);
        r->ax = (u16)(r->ax + 2);
        if (!((s16)r->ax < 0x78)) r->ax = (u16)(r->ax - 2);
        DSW(DS_cop_speed) = r->ax;
        r->cx = setlo(r->cx, DSB(DS_cop_block_idx));
        if (LO(r->cx) != 0) {                              /* Q9: index may name the wrong car */
            r->cx &= 0x00FF;
            r->si = (u16)((r->cx - 1) << 3);
            r->ax = (u16)(DSW(DS_samedir_list + r->si) - 3);
            if (!((s16)r->ax > (s16)r->bx)) {
                r->bx = r->ax;
                DSB(DS_cop_state) = 2;
            }
        }
        r->ax = (u16)(DSW(DS_road_pos) - 3);               /* 0x43E7 */
        if ((s16)r->ax > (s16)r->bx) {
            DSB(DS_cop_tail_timer) = 0x78;                 /* 0x440C */
        } else {
            r->bx = r->ax;
            r->dx = DSW(DS_sub_unit);
            if (DSW(DS_speed) == 0 || --DSB(DS_cop_tail_timer) == 0)
                DSB(DS_cop_state) = 3;
        }
        break;

    case SIM_CS(0x4414):                                           /* 2 pass the blocking car */
        r->ax = setlo(r->ax, DSB(DS_cop_timer));
        if (DSB(DS_oncoming_count) != 0) break;
        r->ax = setlo(r->ax, (u8)(LO(r->ax) + 2));
        DSB(DS_cop_timer) = LO(r->ax);
        r->ax = (u16)(s16)(s8)LO(r->ax);                   /* cbw */
        r->bx = r->ax;
        r->dx = DSB(DS_cop_lane_curve + r->bx);            /* xor dh,dh; mov dl,[bx+0A46] */
        DSW(DS_cop_lateral) = r->dx;
        r->bx = (u16)((r->bx >> 3) - 3);
        r->cx = setlo(r->cx, (u8)(DSB(DS_cop_block_idx) - 1));   /* CH not cleared */
        r->cx = (u16)((r->cx << 3) + DS_samedir_list);
        r->si = r->cx;
        r->bx = (u16)(r->bx + DSW(r->si));
        r->dx = DSW(r->si + 2);
        if (LO(r->ax) == 0x30) {
            DSB(DS_cop_state) = 1;
            DSB(DS_cop_timer) = 0;
            DSB(DS_cop_block_idx)--;
        }
        break;

    case SIM_CS(0x4467):                                           /* 3 pass the player */
        r->ax = setlo(r->ax, (u8)(DSB(DS_cop_timer) + 1));
        DSB(DS_cop_timer) = LO(r->ax);
        r->ax = (u16)(s16)(s8)LO(r->ax);
        r->bx = r->ax;
        r->dx = DSB(DS_cop_lane_curve + r->bx);
        DSW(DS_cop_lateral) = r->dx;
        r->bx = (u16)((r->bx >> 3) - 3);
        r->bx = (u16)(r->bx + DSW(DS_road_pos));
        r->dx = DSW(DS_sub_unit);
        if (LO(r->ax) == 0x30) {
            DSB(DS_cop_state) = 5;
            DSB(DS_cop_timer) = 0x3C;
            r->ax = setlo(r->ax, (u8)(DSB(DS_speed_hi) + 5));
            r->ax &= 0x00FF;
            DSW(DS_cop_speed) = r->ax;
        }
        break;

    case SIM_CS(0x44A8):                                           /* 4, 5 lead and stop (Q8) */
        if (DSB(DS_cop_timer) != 0) {
            DSB(DS_cop_timer)--;
        } else {
            int t = (s16)DSW(DS_cop_speed) - 1;
            DSW(DS_cop_speed) = (u16)t;
            if (!(t > 0)) {
                DSB(DS_cop_state) = 6;
                DSB(DS_cop_timer) = 0x3C;
                DSW(DS_speed) = 0;
                DSW(DS_cop_speed) = 0;
            }
        }
        r->ax = DSW(DS_cop_speed);                         /* 0x44D2 */
        cop_adv(r);
        r->ax = DSW(DS_road_pos);
        if (!((s16)r->ax < (s16)r->bx)) {                  /* hit the cop: game over */
            DSB(DS_run_state) = 3;
            DSW(DS_cars_left) = 1;
            r->dx = DSW(DS_sub_unit);
            r->bx = r->ax;
            r->bx++;
            break;
        }
        r->ax = (u16)(r->ax + 0xD);                        /* 0x44FF */
        if (!((s16)r->ax > (s16)r->bx)) {
            r->bx = r->ax;
            r->ax = setlo(r->ax, DSB(DS_speed_hi));
            if (!((s8)LO(r->ax) > (s8)DSB(DS_cop_speed)))
                DSB(DS_cop_speed) = LO(r->ax);
            r->dx = DSW(DS_sub_unit);
        }
        break;

    case SIM_CS(0x4526):                                           /* 6 stopped, ticket */
        if (DSB(DS_cop_timer) != 0) {
            DSB(DS_cop_timer)--;
            break;
        }
        r->ax = (u16)(DSW(DS_cop_speed) + 1);
        DSW(DS_cop_speed) = r->ax;
        cop_adv(r);
        break;

    case SIM_CS(0x451B):                                           /* 7 trap armed: nothing */
        return L_CLEANUP;

    default:
        /* TODO(verify): cop_state > 7 jumps through the lane table bytes; never assigned. */
        return L_CLEANUP;
    }

    /* 0x4549 store and range check */
    DSW(DS_cop_pos) = r->bx;
    DSW(DS_cop_sub) = r->dx;
    r->bx = (u16)(r->bx - DSW(DS_road_pos));
    if ((s16)r->bx < -0x24 || (s16)r->bx > 0x3C)
        DSB(DS_cop_state) = 0;                             /* 0x451E */
    return L_CLEANUP;
}

/* 0x455F sim_object_cleanup — removal, radar trap, hazard expiry; then iret */
static Label sim_object_cleanup(Reg *r)
{
    r->ax = DSW(DS_road_pos);

    /* tail of the oncoming list 40 units behind */
    r->bx = setlo(r->bx, DSB(DS_oncoming_count));
    if (LO(r->bx) != 0) {
        r->bx &= 0x00FF;
        r->bx--;
        r->cx = setlo(r->cx, LO(r->bx));
        r->bx = (u16)((r->bx << 3) + DS_oncoming_list);
        r->dx = (u16)(r->ax - DSW(r->bx));
        if ((s16)r->dx > 0x28) {
            DSW(r->bx) = 0;
            DSB(DS_oncoming_count) = LO(r->cx);
        }
    }

    /* tail of the same-direction list */
    r->bx = setlo(r->bx, DSB(DS_samedir_count));
    if (LO(r->bx) != 0) {
        r->bx &= 0x00FF;
        r->bx--;
        r->cx = setlo(r->cx, LO(r->bx));
        r->bx = (u16)((r->bx << 3) + DS_samedir_list);
        r->dx = (u16)(r->ax - DSW(r->bx));
        if ((s16)r->dx > 0x28) {
            DSW(r->bx) = 0;
            DSB(DS_samedir_count) = LO(r->cx);
        }
    }

    /* 0x45B4 head of the same-direction list 60 units ahead */
    r->cx = setlo(r->cx, DSB(DS_samedir_count));
    if (LO(r->cx) != 0) {
        r->di = DS_samedir_list;
        r->dx = (u16)(r->ax - DSW(r->di));
        if ((s16)r->dx < -0x3C) {
            r->cx = setlo(r->cx, (u8)(LO(r->cx) - 1));
            DSB(DS_samedir_count) = LO(r->cx);
            if (LO(r->cx) != 0) {
                /* Q1: rep movsw with CX = n*8 words (n*16 bytes) across DS:096D..0A14 as one byte array */
                r->si = EGA_CGA(0x0975, 0x095D);
                r->cx &= 0x00FF;
                r->cx = (u16)(r->cx << 3);
                for (; r->cx != 0; r->cx--) {
                    DSW(r->di) = DSW(r->si);
                    r->si = (u16)(r->si + 2);
                    r->di = (u16)(r->di + 2);
                }
            }
            DSW(r->di) = 0;                                /* 0x45DE */
        }
    }

    /* 0x45E2 radar trap */
    r->bx = DS_radar_trap;
    r->dx = DSW(r->bx);
    if (r->dx != 0) {
        bool passed = (s16)r->dx < (s16)r->ax;
        r->dx = (u16)(r->dx - r->ax);
        if (passed) {                                      /* 0x460B */
            DSW(r->bx) = 0;
            if (DSB(DS_cop_state) != 0) {                  /* was 7: start the chase */
                r->dx = (u16)(r->ax - 0xA);
                r->si = DS_samedir_list;
                r->cx = setlo(r->cx, 0);
                if (LO(r->cx) != DSB(DS_samedir_count)) {
                    r->cx &= 0x00FF;
                    do {
                        if ((s16)r->dx > (s16)DSW(r->si)) break;
                        r->si = (u16)(r->si + 8);
                        r->cx = setlo(r->cx, (u8)(LO(r->cx) + 1));
                    } while (LO(r->cx) != DSB(DS_samedir_count));
                }
                DSW(DS_cop_pos) = r->dx;                   /* 0x4637 */
                DSB(DS_cop_state) = 1;
                DSW(DS_cop_speed) = 0x3C;
                DSB(DS_cop_block_idx) = LO(r->cx);
                DSW(DS_cop_lateral) = 0;
                DSW(DS_cop_sub) = 0;
                DSB(DS_cop_timer) = 0;
                DSW(DS_cop_light_timer) = 0xA;
            }
        } else if (!((s16)r->dx > 8)) {
            r->si = DSW(DS_speed_limit_idx);
            r->cx = DSW(DS_trap_speed_table + r->si);
            if (!(r->cx > DSW(DS_speed)))                  /* ja */
                DSB(DS_cop_state) = 7;
        }
    }

    /* 0x4661 hazard expiry */
    r->bx = (u16)(r->bx + 8);
    r->dx = DSW(r->bx);
    if (r->dx != 0 && (s16)r->dx < (s16)r->ax)
        DSW(r->bx) = 0;
    return L_IRET;
}

/* ---------------------------------------------------------------------------------------------- */
static void sim_run_tick(Reg *r)
{
    Label l = L_TICK;
    while (l != L_IRET) {
        switch (l) {
        case L_TICK:      l = sim_tick(r); break;
        case L_CHARKEY:   l = sim_input_charkey(r); break;
        case L_CONTROLS:  l = sim_controls(r); break;
        case L_DIRECTION: l = sim_input_direction(r); break;
        case L_STEERING:  l = sim_steering(r); break;
        case L_KNOB:      l = sim_shift_knob_anim(r); break;
        case L_ENGINE:    l = sim_engine(r); break;
        case L_BRAKE:     r->dx = 0x304; l = L_DECEL; break;   /* 0x3F19 */
        case L_DECEL:     l = sim_decel(r); break;
        case L_COAST:     l = sim_coast(r); break;
        case L_APPLY:     l = sim_apply(r); break;
        case L_MOTION:    l = sim_motion(r); break;
        case L_GRIP_DONE: l = sim_grip_done(r); break;
        case L_ADVANCE:   l = sim_advance_unit(r); break;
        case L_UNIT_NEXT: l = sim_unit_next(r); break;
        case L_SPAWN:     l = sim_spawn_object(r); break;
        case L_TRAFFIC:   l = sim_traffic_update(r); break;
        case L_COP:       l = sim_cop_fsm(r); break;
        case L_CLEANUP:   l = sim_object_cleanup(r); break;
        default:          l = L_IRET; break;
        }
    }
}

/* 0x3B1F sim_timer_isr — 100 Hz: chain, rpm slew, engine divisor, 1/8 gate, clock */
void sim_timer_isr(void)
{
    Reg r = { 0, 0, 0, 0, 0, 0, 0 };

    timer_isr();                                           /* pushf; lcall cs:[3B18] */
    r.dx = DGROUP - LOAD_SEG;                              /* mov dx, 0C9Ah (relocated DGROUP) */
    if (DSB(DS_modal_pause) != 0) return;
    if (DSB(DS_run_state) == 3) return;                    /* frozen after a crash */

    r.bx = DSW(DS_rpm_smooth);
    r.cx = DSW(DS_rpm);
    if (r.bx != r.cx) {
        if ((s16)r.bx > (s16)r.cx) {
            r.bx = (u16)(r.bx - 0x40);
            if (!((s16)r.bx > (s16)r.cx)) r.bx = r.cx;
        } else {
            r.bx = (u16)(r.bx + 0x40);
            if (!((s16)r.bx < (s16)r.cx)) r.bx = r.cx;
        }
    }
    DSW(DS_rpm_smooth) = r.bx;
    r.bx = (u16)((r.bx >> 5) & 0xFFFE);
    r.bx = DSW(DS_engine_note_div_table + r.bx);
    DSW(DS_note_div_engine) = r.bx;

    DSW(DS_pit_tick_count)++;
    r.ax = DSW(DS_pit_tick_count);
    if ((LO(r.ax) & 7) != 0) return;
    r.ax = setlo(r.ax, 0);
    DSW(DS_clock_ticks)++;
    if (DSB(DS_clock_running) == 0) DSW(DS_clock_ticks)--;  /* 0x3BAD */

    sim_run_tick(&r);                                      /* 0x3BEA .. iret 0x4673 */
}
