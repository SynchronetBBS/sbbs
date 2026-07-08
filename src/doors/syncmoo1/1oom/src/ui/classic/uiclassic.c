#include "config.h"

#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "bits.h"
#include "cfg.h"
#include "comp.h"
#include "font8x8_draw.h"
#include "game_shiptech.h"  /* for sounds used */
#include "gfxaux.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "options.h"
#include "palette.h"
#include "types.h"
#include "uicursor.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uiobj.h"
#include "uipal.h"
#include "uiobj.h"

/* -------------------------------------------------------------------------- */

#define UI_ICON_MAX 147
static int ui_icon = 146/*guardian*/;

/* -------------------------------------------------------------------------- */

void ui_extra_toggle_preset(bool enabled)
{
    ui_extra_enabled = enabled;
    ui_load_opts_extra = enabled;
    game_opt_skip_intro_always = enabled;
    ui_space_combat_autoresolve = enabled;
    ui_sm_ships_enabled = enabled;
    ui_sm_expanded_scroll = enabled;
    ui_sm_explicit_cursor_context = enabled;
    ui_illogical_hotkey_fix = enabled;
    ui_sm_smoother_scrolling = enabled;
    ui_kbd_cursor_keys_fix = enabled;
    ui_mouse_lmb_fix = enabled;
    ui_mouse_warp_disabled = enabled;
}

static bool check_ui_scale(void *var)
{
    int v = (int)(intptr_t)var;
    if ((v > 0) && (v <= UI_SCALE_MAX)) {
        return true;
    } else {
        log_error("invalid ui_scale %i, must be 0 < N <= %i\n", v, UI_SCALE_MAX);
        return false;
    }
}

static bool check_ui_icon(void *var)
{
    int v = (int)(intptr_t)var;
    if ((v >= 0) && (v < UI_ICON_MAX)) {
        return true;
    } else {
        log_error("invalid ui_icon %i, must be 0 <= N < %i\n", v, UI_ICON_MAX);
        return false;
    }
}

static bool check_ui_sm_scroll_speed(void *var)
{
    int v = (int)(intptr_t)var;
    if ((v >= 0) && (v <= UI_SCROLL_SPEED_MAX)) {
        return true;
    } else {
        log_error("invalid ui_sm_scroll_speed %i, must be 0 <= N <= %i\n", v, UI_SCROLL_SPEED_MAX);
        return false;
    }
}

const struct cfg_items_s ui_cfg_items[] = {
    CFG_ITEM_INT("uiscale", &ui_scale_hint, check_ui_scale),
    CFG_ITEM_BOOL("uiextra", &ui_extra_enabled),
    CFG_ITEM_BOOL("ui_delay_enabled", &ui_delay_enabled),
    CFG_ITEM_BOOL("illogical_hotkey_fix", &ui_illogical_hotkey_fix),
    CFG_ITEM_BOOL("load_opts_extra", &ui_load_opts_extra),
    CFG_ITEM_BOOL("space_combat_autoresolve", &ui_space_combat_autoresolve),
    CFG_ITEM_BOOL("ui_sm_ships_enabled", &ui_sm_ships_enabled),
    CFG_ITEM_BOOL("sm_expanded_scroll", &ui_sm_expanded_scroll),
    CFG_ITEM_BOOL("sm_explicit_cursor_context", &ui_sm_explicit_cursor_context),
    CFG_ITEM_BOOL("mouse_lmb_fix", &ui_mouse_lmb_fix),
    CFG_ITEM_BOOL("mouse_warp_disabled", &ui_mouse_warp_disabled),
    CFG_ITEM_BOOL("copy_protection_disabled", &ui_copyprotection_disabled),
    CFG_ITEM_BOOL("sm_mouseover_focus", &ui_sm_mouseover_focus),
    CFG_ITEM_BOOL("sm_smoother_scrolling", &ui_sm_smoother_scrolling),
    CFG_ITEM_BOOL("sm_mouse_scroll", &ui_sm_mouse_scroll),
    CFG_ITEM_BOOL("sm_uhjk_scroll", &ui_sm_uhjk_scroll),
    CFG_ITEM_BOOL("kbd_cursor_keys_fix", &ui_kbd_cursor_keys_fix),
    CFG_ITEM_COMMENT("0..146"),
    CFG_ITEM_INT("uiicon", &ui_icon, check_ui_icon),
    CFG_ITEM_COMMENT("Invert mouse wheel for sliders"),
    CFG_ITEM_BOOL("mwislider", &ui_mwi_slider),
    CFG_ITEM_COMMENT("Invert mouse wheel for counters"),
    CFG_ITEM_BOOL("mwicounter", &ui_mwi_counter),
    CFG_ITEM_COMMENT("Starmap scroll speed (1..10, 0 = instant)"),
    CFG_ITEM_INT("sm_scroll_speed", &ui_sm_scroll_speed, check_ui_sm_scroll_speed),
    CFG_ITEM_BOOL("kbd_repeat", &ui_kbd_repeat),
    CFG_ITEM_END
};

static int ui_options_set_scale(char **argv, void *var)
{
    int v = atoi(argv[1]);
    if (check_ui_scale((void *)(intptr_t)v)) {
        ui_scale_hint = v;
        return 0;
    }
    return -1;
}

static int ui_options_set_scroll_speed(char **argv, void *var)
{
    int v = atoi(argv[1]);
    if (check_ui_sm_scroll_speed((void *)(intptr_t)v)) {
        ui_sm_scroll_speed = v;
        return 0;
    }
    return -1;
}

const struct cmdline_options_s ui_cmdline_options[] = {
    { "-uiscale", 1,
      ui_options_set_scale, 0,
      "SCALE", "UI scaling factor" },
    { "-uiextra", 0,
      options_enable_bool_var, (void *)&ui_extra_enabled,
      NULL, "Enable UI extras" },
    { "-nouiextra", 0,
      options_disable_bool_var, (void *)&ui_extra_enabled,
      NULL, "Disable UI extras" },
    { "-mwislider", 0,
      options_enable_bool_var, (void *)&ui_mwi_slider,
      NULL, "Invert mouse wheel for sliders" },
    { "-nomwislider", 0,
      options_disable_bool_var, (void *)&ui_mwi_slider,
      NULL, "Do not invert mouse wheel for sliders" },
    { "-mwicounter", 0,
      options_enable_bool_var, (void *)&ui_mwi_counter,
      NULL, "Invert mouse wheel for counters" },
    { "-nomwicounter", 0,
      options_disable_bool_var, (void *)&ui_mwi_counter,
      NULL, "Do not invert mouse wheel for counters" },
    { "-uismscroll", 1,
      ui_options_set_scroll_speed, 0,
      "SPEED", "Starmap scroll speed (1..10, 0 = instant)" },
    { NULL, 0, NULL, NULL, NULL, NULL }
};

/* -------------------------------------------------------------------------- */

const char *idstr_ui = "classic";

struct ui_data_s ui_data = { 0 };

int ui_screen_w = 0;
int ui_screen_h = 0;
int ui_scale = 0;
int ui_scale_hint = 0;
int starmap_scale = 0;
bool ui_extra_enabled = false;
bool ui_fixbugs_enabled = false;
bool ui_illogical_hotkey_fix = false;
bool ui_load_opts_extra = false;
bool ui_space_combat_autoresolve = false;
bool ui_sm_ships_enabled = false;
bool ui_sm_expanded_scroll = false;
bool ui_sm_no_question_mark_cursor = false;
bool ui_sm_explicit_cursor_context = false;
bool ui_mouse_lmb_fix = false;
bool ui_mouse_warp_disabled = false;
bool ui_copyprotection_disabled = false;
bool ui_sm_mouseover_focus = false;
bool ui_sm_smoother_scrolling = false;
bool ui_sm_mouse_scroll = false;
bool ui_sm_uhjk_scroll = false;
bool ui_kbd_cursor_keys_fix = false;
bool ui_mwi_slider = false;
bool ui_mwi_counter = false;
int ui_sm_scroll_speed = 3;
bool ui_kbd_repeat = true;

bool ui_use_audio = true;

/* -------------------------------------------------------------------------- */

static void init_lbx_design(void)
{
    ui_data.gfx.design.blank = lbxfile_item_get(LBXFILE_DESIGN, 1);
    ui_data.gfx.design.icon_up = lbxfile_item_get(LBXFILE_DESIGN, 3);
    ui_data.gfx.design.icon_dn = lbxfile_item_get(LBXFILE_DESIGN, 2);
    ui_data.gfx.design.count_up = lbxfile_item_get(LBXFILE_DESIGN, 4);
    ui_data.gfx.design.count_dn = lbxfile_item_get(LBXFILE_DESIGN, 5);
    ui_data.gfx.design.bg = lbxfile_item_get(LBXFILE_DESIGN, 0);
    ui_data.gfx.design.pop1_ul = lbxfile_item_get(LBXFILE_DESIGN, 6);
    ui_data.gfx.design.pop1_ur = lbxfile_item_get(LBXFILE_DESIGN, 7);
    ui_data.gfx.design.pop1_dl = lbxfile_item_get(LBXFILE_DESIGN, 9);
    ui_data.gfx.design.pop1_dr = lbxfile_item_get(LBXFILE_PLANETS, 0x31);
    ui_data.gfx.design.titlebox = lbxfile_item_get(LBXFILE_DESIGN, 8);
    ui_data.gfx.design.popscrol_u = lbxfile_item_get(LBXFILE_DESIGN, 0xb);
    ui_data.gfx.design.popscrol_d = lbxfile_item_get(LBXFILE_DESIGN, 0xc);
}

static void init_lbx_space(void)
{
    for (int i = 0; i < 5; ++i) {
        ui_data.gfx.space.bg[i] = lbxfile_item_get(LBXFILE_SPACE, 0x1a + i);
    }
    ui_data.gfx.space.box = lbxfile_item_get(LBXFILE_SPACE, 1);
    ui_data.gfx.space.box_x = lbxfile_item_get(LBXFILE_SPACE, 0x32);
    ui_data.gfx.space.box_y = lbxfile_item_get(LBXFILE_SPACE, 0x33);
    ui_data.gfx.space.box_xy = lbxfile_item_get(LBXFILE_SPACE, 0x34);
    ui_data.gfx.space.done = lbxfile_item_get(LBXFILE_SPACE, 3);
    ui_data.gfx.space.retreat = lbxfile_item_get(LBXFILE_SPACE, 5);
    ui_data.gfx.space.retr_off = lbxfile_item_get(LBXFILE_SPACE, 2);
    ui_data.gfx.space.wait = lbxfile_item_get(LBXFILE_SPACE, 6);
    ui_data.gfx.space.autob = lbxfile_item_get(LBXFILE_SPACE, 0x11);
    ui_data.gfx.space.special = lbxfile_item_get(LBXFILE_SPACE, 0x12);
    ui_data.gfx.space.spec_off = lbxfile_item_get(LBXFILE_SPACE, 0x20);
    ui_data.gfx.space.scan = lbxfile_item_get(LBXFILE_SPACE, 0x15);
    ui_data.gfx.space.scan_off = lbxfile_item_get(LBXFILE_SPACE, 0x14);
    ui_data.gfx.space.planet = lbxfile_item_get(LBXFILE_SPACE, 0x16);
    ui_data.gfx.space.planet_off = lbxfile_item_get(LBXFILE_SPACE, 0x1f);
    for (int i = 0; i < 10; ++i) {
        ui_data.gfx.space.explos[i] = lbxfile_item_get(LBXFILE_SPACE, 7 + i);
    }
    ui_data.gfx.space.warp1 = lbxfile_item_get(LBXFILE_SPACE, 0x17);
    ui_data.gfx.space.warp2 = lbxfile_item_get(LBXFILE_SPACE, 0x2d);
    ui_data.gfx.space.warp3 = lbxfile_item_get(LBXFILE_SPACE, 0x2e);
    ui_data.gfx.space.warp4 = lbxfile_item_get(LBXFILE_SPACE, 0x2f);
    ui_data.gfx.space.technull = lbxfile_item_get(LBXFILE_SPACE, 0x18);
    ui_data.gfx.space.misbutt = lbxfile_item_get(LBXFILE_SPACE, 0x19);
    ui_data.gfx.space.misl_off = lbxfile_item_get(LBXFILE_SPACE, 4);
    ui_data.gfx.space.warpout = lbxfile_item_get(LBXFILE_SPACE, 0x21);
    ui_data.gfx.space.envterm = lbxfile_item_get(LBXFILE_SPACE, 0x22);
    ui_data.gfx.space.enviro = lbxfile_item_get(LBXFILE_SPACE, 0x13);
    ui_data.gfx.space.base_btn = lbxfile_item_get(LBXFILE_SPACE, 0x23);
    ui_data.gfx.space.dis_bem2 = lbxfile_item_get(LBXFILE_SPACE, 0x24);
    ui_data.gfx.space.stasis2 = lbxfile_item_get(LBXFILE_SPACE, 0x25);
    ui_data.gfx.space.vs2 = lbxfile_item_get(LBXFILE_SPACE, 0x27);
    ui_data.gfx.space.vp2_top = lbxfile_item_get(LBXFILE_SPACE, 0x2a);
    ui_data.gfx.space.vp2_data = lbxfile_item_get(LBXFILE_SPACE, 0x2b);
    ui_data.gfx.space.vp2_line = lbxfile_item_get(LBXFILE_SPACE, 0x2c);
    ui_data.gfx.space.vp2_bottom = lbxfile_item_get(LBXFILE_SPACE, 0x39);
    ui_data.gfx.space.blk_hole = lbxfile_item_get(LBXFILE_SPACE, 0x26);
    ui_data.gfx.space.bombs = lbxfile_item_get(LBXFILE_SPACE, 0x30);
    ui_data.gfx.space.biologic = lbxfile_item_get(LBXFILE_SPACE, 0x31);
    ui_data.gfx.space.circle = lbxfile_item_get(LBXFILE_SPACE, 0x35);
    ui_data.gfx.space.sphere2 = lbxfile_item_get(LBXFILE_SPACE, 0x36);
    ui_data.gfx.space.asteroid[0] = lbxfile_item_get(LBXFILE_SPACE, 0x28);
    ui_data.gfx.space.asteroid[1] = lbxfile_item_get(LBXFILE_SPACE, 0x29);
    ui_data.gfx.space.asteroid[2] = lbxfile_item_get(LBXFILE_SPACE, 0x37);
    ui_data.gfx.space.asteroid[3] = lbxfile_item_get(LBXFILE_SPACE, 0x38);
    for (int i = 0; i < 8; ++i) {
        ui_data.gfx.missile.missiles[i] = lbxfile_item_get(LBXFILE_MISSILE, i);
        ui_data.gfx.missile.antimatr[i] = lbxfile_item_get(LBXFILE_MISSILE, 0x8 + i);
        ui_data.gfx.missile.hellfire[i] = lbxfile_item_get(LBXFILE_MISSILE, 0x10 + i);
        ui_data.gfx.missile.proton[i] = lbxfile_item_get(LBXFILE_MISSILE, 0x18 + i);
        ui_data.gfx.missile.plasmaqt[i] = lbxfile_item_get(LBXFILE_MISSILE, 0x20 + i);
    }
}

static void init_lbx_news(void)
{
    ui_data.gfx.news.tv = lbxfile_item_get(LBXFILE_NEWSCAST, 0);
    ui_data.gfx.news.gnn = lbxfile_item_get(LBXFILE_NEWSCAST, 1);
    ui_data.gfx.news.nc = lbxfile_item_get(LBXFILE_NEWSCAST, 2);
    ui_data.gfx.news.world = lbxfile_item_get(LBXFILE_NEWSCAST, 3);
    ui_data.gfx.news.icon = 0;
}

static void init_gfx(void)
{
    for (int i = 0; i < NEBULA_MAX; ++i) {
        ui_data.gfx.starmap.nebula[i] = NULL;
        ui_data.gfx.starmap.smnebula[i] = NULL;
    }
    ui_data.gfx.starmap.mainview = lbxfile_item_get(LBXFILE_STARMAP, 0);
    ui_data.gfx.starmap.starback = lbxfile_item_get(LBXFILE_STARMAP, 1);
    ui_data.gfx.starmap.starbak2 = lbxfile_item_get(LBXFILE_STARMAP, 2);
    for (int i = 0; i < 12; ++i) {
        const int jtbl[12] = { 0, 1, 2, 4, 3, 5, 6, 7, 8, 10, 9, 11 };
        ui_data.gfx.starmap.stars[jtbl[i]] = lbxfile_item_get(LBXFILE_STARMAP, 3 + i);
    }
    ui_data.gfx.starmap.planbord = lbxfile_item_get(LBXFILE_STARMAP, 0x19);
    ui_data.gfx.starmap.yourplnt = lbxfile_item_get(LBXFILE_STARMAP, 0x1a);
    ui_data.gfx.starmap.unexplor = lbxfile_item_get(LBXFILE_STARMAP, 0x1b);
    ui_data.gfx.starmap.en_colny = lbxfile_item_get(LBXFILE_STARMAP, 0x1c);
    ui_data.gfx.starmap.no_colny = lbxfile_item_get(LBXFILE_STARMAP, 0x1d);
    ui_data.gfx.starmap.col_butt_ship = lbxfile_item_get(LBXFILE_STARMAP, 0x1e);
    ui_data.gfx.starmap.col_butt_reloc = lbxfile_item_get(LBXFILE_STARMAP, 0x1f);
    ui_data.gfx.starmap.col_butt_trans = lbxfile_item_get(LBXFILE_STARMAP, 0x20);
    ui_data.gfx.starmap.sky = lbxfile_item_get(LBXFILE_STARMAP, 0x2f);
    for (int i = 0; i < 6; ++i) {
        const int jtbl[6] = { 0, 1, 2, 4, 3, 5 };
        ui_data.gfx.starmap.smstars[jtbl[i]] = lbxfile_item_get(LBXFILE_STARMAP, 0x21 + i);
    }
    for (int i = 0; i < 6; ++i) {
        ui_data.gfx.starmap.smalflag[i] = lbxfile_item_get(LBXFILE_STARMAP, 0x27 + i);
    }
    ui_data.gfx.starmap.stargate = lbxfile_item_get(LBXFILE_STARMAP, 0x2d);
    ui_data.gfx.starmap.smallstr = lbxfile_item_get(LBXFILE_STARMAP, 0x2e);
    for (int i = 0; i < 0xa; ++i) {
        ui_data.gfx.starmap.smneb[i] = lbxfile_item_get(LBXFILE_STARMAP, 0x35 + i);
    }
    for (int i = 0; i < 0x1e; ++i) {
        ui_data.gfx.starmap.smneb[0xa + i] = lbxfile_item_get(LBXFILE_NEBULA, i);
    }
    ui_data.gfx.starmap.relocate = lbxfile_item_get(LBXFILE_STARMAP, 0x3f);
    ui_data.gfx.starmap.reloc_bu_cancel = lbxfile_item_get(LBXFILE_STARMAP, 0x40);
    ui_data.gfx.starmap.reloc_bu_accept = lbxfile_item_get(LBXFILE_STARMAP, 0x41);
    ui_data.gfx.starmap.tran_bar = lbxfile_item_get(LBXFILE_STARMAP, 0x42);
    for (int i = 0; i < 6; ++i) {
        ui_data.gfx.starmap.smalship[i] = lbxfile_item_get(LBXFILE_STARMAP, 0x43 + i);
        ui_data.gfx.starmap.smaltran[i] = lbxfile_item_get(LBXFILE_STARMAP, 0x49 + i);
        ui_data.gfx.starmap.tinyship[i] = lbxfile_item_get(LBXFILE_STARMAP, 0x4f + i);
        ui_data.gfx.starmap.tinytran[i] = lbxfile_item_get(LBXFILE_STARMAP, 0x55 + i);
    }
    ui_data.gfx.starmap.move_shi = lbxfile_item_get(LBXFILE_STARMAP, 0x5b);
    ui_data.gfx.starmap.move_but_p = lbxfile_item_get(LBXFILE_STARMAP, 0x5c);
    ui_data.gfx.starmap.move_but_m = lbxfile_item_get(LBXFILE_STARMAP, 0x5d);
    ui_data.gfx.starmap.move_but_a = lbxfile_item_get(LBXFILE_STARMAP, 0x5e);
    ui_data.gfx.starmap.move_but_n = lbxfile_item_get(LBXFILE_STARMAP, 0x5f);
    ui_data.gfx.starmap.shipbord = lbxfile_item_get(LBXFILE_STARMAP, 0x60);
    ui_data.gfx.starmap.movextra = lbxfile_item_get(LBXFILE_STARMAP, 0x61);
    ui_data.gfx.starmap.movextr2 = lbxfile_item_get(LBXFILE_STARMAP, 0x62);
    ui_data.gfx.starmap.movextr3 = lbxfile_item_get(LBXFILE_STARMAP, 0x63);
    ui_data.gfx.starmap.scanner = lbxfile_item_get(LBXFILE_STARMAP, 0x64);
    ui_data.gfx.starmap.tranship = lbxfile_item_get(LBXFILE_STARMAP, 0x65);
    ui_data.gfx.starmap.tranbord = lbxfile_item_get(LBXFILE_STARMAP, 0x66);
    ui_data.gfx.starmap.tranxtra = lbxfile_item_get(LBXFILE_STARMAP, 0x67);
    ui_data.gfx.starmap.dismiss = lbxfile_item_get(LBXFILE_STARMAP, 0x68);
    ui_data.gfx.starmap.fleetbut_view = lbxfile_item_get(LBXFILE_STARMAP, 0x69);
    ui_data.gfx.starmap.fleetbut_scrap = lbxfile_item_get(LBXFILE_STARMAP, 0x6a);
    ui_data.gfx.starmap.fleetbut_ok = lbxfile_item_get(LBXFILE_STARMAP, 0x6b);
    ui_data.gfx.starmap.fleetbut_down = lbxfile_item_get(LBXFILE_STARMAP, 0x6c);
    ui_data.gfx.starmap.fleetbut_up = lbxfile_item_get(LBXFILE_STARMAP, 0x6d);
    ui_data.gfx.starmap.viewship = lbxfile_item_get(LBXFILE_STARMAP, 0x6e);
    ui_data.gfx.starmap.viewshp2 = lbxfile_item_get(LBXFILE_STARMAP, 0x6f);
    ui_data.gfx.starmap.viewshbt = lbxfile_item_get(LBXFILE_STARMAP, 0x70);
    ui_data.gfx.starmap.scrap = lbxfile_item_get(LBXFILE_STARMAP, 0x71);
    ui_data.gfx.starmap.scrapbut_no = lbxfile_item_get(LBXFILE_STARMAP, 0x72);
    ui_data.gfx.starmap.scrapbut_yes = lbxfile_item_get(LBXFILE_STARMAP, 0x73);
    ui_data.gfx.starmap.reprtbut_ok = lbxfile_item_get(LBXFILE_STARMAP, 0x75);
    ui_data.gfx.starmap.reprtbut_up = lbxfile_item_get(LBXFILE_STARMAP, 0x76);
    ui_data.gfx.starmap.reprtbut_down = lbxfile_item_get(LBXFILE_STARMAP, 0x77);
    ui_data.gfx.starmap.gr_arrow_u = lbxfile_item_get(LBXFILE_STARMAP, 0x78);
    ui_data.gfx.starmap.gr_arrow_d = lbxfile_item_get(LBXFILE_STARMAP, 0x79);
    ui_data.gfx.starmap.slanbord = lbxfile_item_get(LBXFILE_STARMAP, 0x74);
    for (int i = 0; i < 0x23; ++i) {
        ui_data.gfx.planets.planet[i] = lbxfile_item_get(LBXFILE_PLANETS, i);
    }
    for (int i = 0; i < 10; ++i) {
        ui_data.gfx.planets.race[i] = lbxfile_item_get(LBXFILE_PLANETS, 0x23 + i);
    }
    ui_data.gfx.planets.smonster = lbxfile_item_get(LBXFILE_PLANETS, 0x2d);
    ui_data.gfx.planets.tmonster = lbxfile_item_get(LBXFILE_PLANETS, 0x2e);

    ui_data.gfx.screens.tech_but_up = lbxfile_item_get(LBXFILE_SCREENS, 1);
    ui_data.gfx.screens.tech_but_down = lbxfile_item_get(LBXFILE_SCREENS, 2);
    ui_data.gfx.screens.tech_but_ok = lbxfile_item_get(LBXFILE_SCREENS, 3);
    ui_data.gfx.screens.litebulb_off = lbxfile_item_get(LBXFILE_SCREENS, 4);
    ui_data.gfx.screens.litebulb_on = lbxfile_item_get(LBXFILE_SCREENS, 5);
    ui_data.gfx.screens.techback = lbxfile_item_get(LBXFILE_SCREENS, 6);
    ui_data.gfx.screens.race_pnt = lbxfile_item_get(LBXFILE_SCREENS, 8);
    ui_data.gfx.screens.races_bu.sabotage = lbxfile_item_get(LBXFILE_SCREENS, 0x9);
    ui_data.gfx.screens.races_bu.espionage = lbxfile_item_get(LBXFILE_SCREENS, 0xa);
    ui_data.gfx.screens.races_bu.hiding = lbxfile_item_get(LBXFILE_SCREENS, 0xb);
    ui_data.gfx.screens.races_bu.status = lbxfile_item_get(LBXFILE_SCREENS, 0xc);
    ui_data.gfx.screens.races_bu.report = lbxfile_item_get(LBXFILE_SCREENS, 0xd);
    ui_data.gfx.screens.races_bu.audience = lbxfile_item_get(LBXFILE_SCREENS, 0xe);
    ui_data.gfx.screens.races_bu.ok = lbxfile_item_get(LBXFILE_SCREENS, 0xf);

    init_lbx_design();
    init_lbx_space();
    ui_data.gfx.starmap.stargate2 = lbxfile_item_get(LBXFILE_V11, 5);

    for (int i = 0; i < 0x1c; ++i) {
        ui_data.gfx.colonies.d[i] = lbxfile_item_get(LBXFILE_COLONIES, i);
    }
    ui_data.gfx.colonies.current = NULL;
    init_lbx_news();

    ui_data.gfx.vgafileh = lib_malloc(UI_SCREEN_W * UI_SCREEN_H);

    gfx_aux_setup_wh(&ui_data.aux.screen, UI_SCREEN_W, UI_SCREEN_H);
    gfx_aux_setup_wh(&ui_data.aux.ship_overlay, 34, 26);
    gfx_aux_setup_wh(&ui_data.aux.btemp, 50, 40);

    ui_data.gfx.initialized = true;
}

static int init_lbx_ships(void)
{
    for (int i = 0; i < 0x48; ++i) {
        uint8_t *t;
        if (i < 0x46) {
            t = lbxfile_item_get(LBXFILE_SHIPS2, i);
        } else if (i == 0x46) {
            t = lbxfile_item_get(LBXFILE_PLANETS, i - 0x17);
        } else /*(i == 0x47)*/ {
            t = lbxfile_item_get(LBXFILE_SCREENS, 7);
        }
        ui_data.gfx.ships[i] = t;
        ui_data.gfx.ships[0x48 + i] = lbxfile_item_get(LBXFILE_SHIPS, i);
    }
    for (int i = 0; i < 3; ++i) {
        ui_data.gfx.ships[0x48 * 2 + i] = lbxfile_item_get(LBXFILE_SHIPS2, 0x48 + i);
    }
    gfx_aux_setup_wh(&ui_data.aux.ship_p1, 34, 26);
    return 0;
}

static int set_ui_icon(void)
{
    struct gfx_aux_s *aux = &ui_data.aux.ship_p1;
    uint8_t *gfx, *pal;
    gfx = ui_data.gfx.ships[ui_icon];
    pal = lbxfile_item_get(LBXFILE_FONTS, 2);
    memcpy(lbxpal_palette, pal, 256 * 3);
    gfx_aux_draw_frame_to(gfx, aux);
    hw_icon_set(aux->data, pal, aux->w, aux->h);    /* do not care if the icon got set */
    lbxfile_item_release(LBXFILE_FONTS, pal);
    return 0;
}

static void ui_fix_1_3a(void)
{
    ui_fixbugs_enabled = true;
    ui_sm_no_question_mark_cursor = true;
}

/* -------------------------------------------------------------------------- */

void ui_early_show_message_box(const char *msg)
{
    if (hw_video_init(320, 200)) {
        return;
    }
    bool initialized = false;
    int64_t init_time = hw_get_time_us();
    lbxpal_init();
    ui_palette_set_color(0, 0, 0, 0);
    ui_palette_set_color(1, 0x3f, 0x3f, 0x3f);
    hw_video_refresh_palette();
    while (1) {
        if (!initialized && (hw_get_time_us() - init_time) > 1000000) {
            memset(hw_video_get_buf(), 0, 320 * 200);
            font8x8_drawstr_rect(50, 20, 270, 180, 320, msg, 1, 0);
            hw_video_draw_buf();
            initialized = true;
        }
        hw_event_handle();
        if (kbd_have_keypress()) {
            uint32_t k;
            k = kbd_get_keypress();
            if ((KBD_GET_KEY(k) == MOO_KEY_SPACE)
              ||(KBD_GET_KEY(k) == MOO_KEY_ESCAPE)) {
                break;
            }
        }
    }
}

int ui_early_init(void)
{
    return 0;
}

int ui_init(void)
{
    memset(&ui_data, 0, sizeof(ui_data));
    return 0;
}

int ui_late_init(void)
{
    if (!lbxfile_exists(LBXFILE_V11)) {
        log_error("V11.LBX not found! Make sure that your MOO1 is updated to v1.3.\n");
        return 1;
    }
    ui_fix_1_3a();
    if (ui_scale_hint == 0) {
        ui_scale_hint = 1;
    }
    ui_scale = ui_scale_hint;
    starmap_scale = ui_scale_hint;
    ui_screen_w = UI_VGA_W * ui_scale;
    ui_screen_h = UI_VGA_H * ui_scale;
    ui_cursor_init(ui_scale);
    log_message("UI: scale %i -> %ix%i\n", ui_scale, ui_screen_w, ui_screen_h);
    uiobj_set_limits_all();
    if (0
      || lbxfont_init()
      || init_lbx_ships()
      || set_ui_icon()
      || hw_video_init(UI_SCREEN_W, UI_SCREEN_H)
      || lbxpal_init()
    ) {
        return 1;
    }
    if (opt_audio_enabled) {
        const uint8_t sounds[] = { /* sounds used by ui or game code */
            0x24, 0x06, /* these are the most common sounds and are prepared first */
            0x02, 0x09, 0x0e, 0x11, 0x13, 0x15, 0x16, 0x18, 0x1d, 0x20
        };
        uint32_t t0;
        int res;
        BOOLVEC_DECLARE(sound_added, NUM_SOUNDS);
        BOOLVEC_CLEAR(sound_added, NUM_SOUNDS);
        t0 = hw_get_time_us();
        res = hw_audio_sfx_batch_start(NUM_SOUNDS);
        if (res < 0) {
            return -1;
        } else if (res == 0) {
            log_message("Preparing sounds, this may take a while...\n");
        } else {
            log_message("Preparing sounds in a thread\n");
        }
        for (int ti = 0; ti < TBLLEN(sounds); ++ti) {
            uint32_t len;
            int i;
            i = sounds[ti];
            BOOLVEC_SET1(sound_added, i);
            LOG_DEBUG((4, "%s: sfx 0x%02x\n", __func__, i));
            ui_data.sfx[i] = lbxfile_item_get_with_len(LBXFILE_SOUNDFX, i, &len);
            hw_audio_sfx_init(i, ui_data.sfx[i], len);
        }
        for (int ti = 0; ti < WEAPON_NUM; ++ti) {
            uint32_t len;
            int i;
            i = tbl_shiptech_weap[ti].sound;
            if (BOOLVEC_IS0(sound_added, i)) {
                BOOLVEC_SET1(sound_added, i);
                LOG_DEBUG((4, "%s: sfx 0x%02x\n", __func__, i));
                ui_data.sfx[i] = lbxfile_item_get_with_len(LBXFILE_SOUNDFX, i, &len);
                hw_audio_sfx_init(i, ui_data.sfx[i], len);
            }
        }
        res = hw_audio_sfx_batch_end();
        if (res < 0) {
            return -1;
        } else if (res == 0) {
            uint32_t t1;
            t1 = hw_get_time_us();
            log_message("Preparing sounds took %i ms\n", (t1 - t0) / 1000);
        }
    }
    ui_data.music_i = -1;
    init_gfx();
    ui_data.have_help = lbxfile_exists(LBXFILE_HELP);
    if (!ui_data.have_help) {
        log_warning("Help disabled due to missing %s\n", lbxfile_name(LBXFILE_HELP));
    }
    uiobj_table_clear();
    kbd_clear();
    hw_kbd_set_repeat(ui_kbd_repeat);
    return 0;
}

void ui_shutdown(void)
{
    hw_audio_music_stop();
    hw_audio_sfx_stop();
    if (ui_data.mus) {
        hw_audio_music_release(0);
        lbxfile_item_release(LBXFILE_MUSIC, ui_data.mus);
    }
    for (int i = 0; i < NUM_SOUNDS; ++i) {
        if (ui_data.sfx[i]) {
            hw_audio_sfx_release(i);
            lbxfile_item_release(LBXFILE_SOUNDFX, ui_data.sfx[i]);
        }
    }
    if (ui_data.gfx.initialized) {
        gfx_aux_free(&ui_data.starmap.star_aux);
        gfx_aux_free(&ui_data.aux.screen);
        gfx_aux_free(&ui_data.aux.ship_p1);
        gfx_aux_free(&ui_data.aux.ship_overlay);
        gfx_aux_free(&ui_data.aux.btemp);
        memset(&ui_data.aux, 0, sizeof(ui_data.aux));
        lbxfile_item_release_file(LBXFILE_STARMAP);
        lbxfile_item_release_file(LBXFILE_NEBULA);
        lbxfile_item_release_file(LBXFILE_SHIPS);
        lbxfile_item_release_file(LBXFILE_SHIPS2);
        lbxfile_item_release_file(LBXFILE_SCREENS);
        lbxfile_item_release_file(LBXFILE_RESEARCH);
        lbxfile_item_release_file(LBXFILE_DIPLOMAT);
        lbxfile_item_release_file(LBXFILE_DESIGN);
        lbxfile_item_release_file(LBXFILE_SPACE);
        lbxfile_item_release_file(LBXFILE_MISSILE);
        lbxfile_item_release_file(LBXFILE_V11);
        lbxfile_item_release_file(LBXFILE_COLONIES);
        lbxfile_item_release_file(LBXFILE_NEWSCAST);
        lib_free(ui_data.gfx.vgafileh);
        memset(&ui_data.gfx, 0, sizeof(ui_data.gfx));
    }
    lbxfont_shutdown();
}

char *ui_get_strbuf(void)
{
    return ui_data.strbuf;
}

uint8_t *ui_gfx_get_ship(int look)
{
    return ui_data.gfx.ships[look];
}

uint8_t *ui_gfx_get_planet(int look)
{
    return ui_data.gfx.planets.planet[look];
}

uint8_t *ui_gfx_get_rock(int look)
{
    return ui_data.gfx.space.asteroid[look];
}
