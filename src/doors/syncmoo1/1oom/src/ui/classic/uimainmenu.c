#include "config.h"

#include "ui.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_event.h"
#include "game_new.h"
#include "game_num.h"
#include "game_nump.h"
#include "game_save.h"
#include "game_str.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "menu.h"
#include "rnd.h"
#include "types.h"
#include "uicursor.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiload.h"
#include "uinewgame.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "version.h"

/* -------------------------------------------------------------------------- */

static bool force_restart = false;

static bool main_menu_game_active(void)
{
    return !force_restart;
}

static bool main_menu_have_save_continue(void) {
    return game_save_tbl_have_save[GAME_SAVE_I_CONTINUE];
}

static bool main_menu_have_save_any(void) {
    const int num_saves = ui_extra_enabled ? NUM_ALL_SAVES : NUM_ALL_SAVES;
    for (int i = 0; i < num_saves; ++i) {
        if (game_save_tbl_have_save[i]) {
            return true;
        }
    }
    return false;
}

static bool main_menu_toggle_kbd_repeat(void) {
    ui_kbd_repeat = !ui_kbd_repeat;
    hw_kbd_set_repeat(ui_kbd_repeat);
    return true;
}

static bool main_menu_toggle_music(void) {
    if (opt_music_enabled) {
        ui_sound_stop_music();
        opt_music_enabled = false;
    } else {
        opt_music_enabled = true;
        ui_sound_play_music(1);
    }
    return true;
}

static bool main_menu_update_music_volume(void) {
    hw_audio_music_volume(opt_music_volume);
    return true;
}

static bool main_menu_update_sfx_volume(void) {
    hw_audio_sfx_volume(opt_sfx_volume);
    return true;
}

static const char* mm_get_custom_difficulty_value(uint32_t i) {
    return game_str_tbl_diffic[i];
}
static const char* mm_get_custom_galaxy_size_value(uint32_t i) {
    return game_str_tbl_gsize[i];
}

static const char* mm_get_custom_ai_id_value(uint32_t i) {
    return game_ais[i]->name;
}

static const char* mm_get_custom_galaxy_seed_str(void) {
    static char buf[64];
    if (game_opt_custom.galaxy_seed == 0) {
        lib_sprintf(buf, 64, "Off");
    } else {
        lib_sprintf(buf, 64, "%d", game_opt_custom.galaxy_seed);
    }
    return buf;
}

static bool mm_gen_custom_galaxy_seed(void) {
    game_opt_custom.galaxy_seed = game_opt_custom.galaxy_seed ? 0 : rnd_get_new_seed();
    return true;
}

const char *game_str_tbl_sm_pspecial_mm[PLANET_SPECIAL_NUM] = {
    "Ultra Poor", "Poor", "Normal", "Artifacts", "Rich", "Ultra Rich", "4x Tech"
};

static const char* mm_get_custom_special_value(uint32_t i) {
    return game_str_tbl_sm_pspecial_mm[i];
}

static bool mm_enable_preset_classic(void) {
    ui_extra_toggle_preset(false);
    return true;
}

static bool mm_enable_preset_1oom(void) {
    ui_extra_toggle_preset(true);
    return true;
}

/* -------------------------------------------------------------------------- */

#define MM_PAGE_STACK_SIZE 4

typedef enum {
    MAIN_MENU_PAGE_MAIN,
    MAIN_MENU_PAGE_GAME,
    MAIN_MENU_PAGE_GAME_CUSTOM,
    MAIN_MENU_PAGE_GAME_CUSTOM_GALAXY,
    MAIN_MENU_PAGE_GAME_CUSTOM_HOMEWORLDS,
    MAIN_MENU_PAGE_OPTIONS,
    MAIN_MENU_PAGE_OPTIONS_INPUT,
    MAIN_MENU_PAGE_OPTIONS_SOUND,
    MAIN_MENU_PAGE_OPTIONS_VIDEO,
    MAIN_MENU_PAGE_OPTIONS_ADDONS,
    MAIN_MENU_PAGE_OPTIONS_ADDONS_MESSAGE_FILTER,
    MAIN_MENU_PAGE_OPTIONS_MISC,
    MAIN_MENU_PAGE_OPTIONS_STARMAP,
    MAIN_MENU_PAGE_OPTIONS_RULES,
    MAIN_MENU_PAGE_OPTIONS_RULES_AI,
    MAIN_MENU_PAGE_OPTIONS_RULES_BATTLE,
    MAIN_MENU_PAGE_OPTIONS_RULES_DIFFICULTY,
    MAIN_MENU_PAGE_OPTIONS_RULES_FLEET_BEHAVIOR,
    MAIN_MENU_PAGE_OPTIONS_RULES_ORBITAL_BOMBARDMENT,
    MAIN_MENU_PAGE_OPTIONS_RULES_PLANETARY_DEVELOPMENT,
    MAIN_MENU_PAGE_OPTIONS_RULES_SLIDER_BEHAVIOR,
    MAIN_MENU_PAGE_OPTIONS_RULES_OTHER,
    MAIN_MENU_PAGE_PRESET,
    MAIN_MENU_PAGE_NUM,
} main_menu_page_id_t;

struct main_menu_item_s {
    struct menu_item_data_s data;
    uint16_t font_i;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    int16_t oi;
    int16_t oi_shift;
    int16_t oi_wheel;
    bool active;
};

struct main_menu_data_s {
    void (*set_item_dimensions)(struct main_menu_data_s *, int i);
    main_menu_page_id_t page_stack[MM_PAGE_STACK_SIZE];
    int page_stack_i;
    int current_page_i;
    struct main_menu_item_s items[MENU_MAX_ITEMS_PER_PAGE];
    uint8_t item_count;
    int16_t oi_plus, oi_minus, oi_equals;
    int16_t scrollmisc;
    int frame;
    bool refresh;
    main_menu_action_t ret;
    bool flag_done;
    int clicked_i;
    int shift_i;
    int wheel_i;
    int highlight;
    uint8_t *gfx_vortex;
    uint8_t *gfx_title;
};

static void load_mainmenu_data(struct main_menu_data_s *d)
{
    d->gfx_vortex = lbxfile_item_get(LBXFILE_VORTEX, 0);
    d->gfx_title = lbxfile_item_get(LBXFILE_V11, 0);
}

static void free_mainmenu_data(struct main_menu_data_s *d)
{
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_vortex);
    lbxfile_item_release(LBXFILE_V11, d->gfx_title);
}

/* -------------------------------------------------------------------------- */

static void main_menu_get_item_string(const struct main_menu_item_s *it, char *buf, int len)
{
    if (it->data.type == MENU_ITEM_TYPE_BOOL) {
        bool *v = it->data.value_ptr;
        lib_sprintf(buf, len, "%s %s", it->data.text, *v ? "On" : "Off");
    } else if (it->data.type == MENU_ITEM_TYPE_INT) {
        int *v = it->data.value_ptr;
        lib_sprintf(buf, len, "%s %d", it->data.text, *v);
    } else if (it->data.type == MENU_ITEM_TYPE_ENUM) {
        int *v = it->data.value_ptr;
        lib_sprintf(buf, len, "%s %s", it->data.text, it->data.get_text_value(*v));
    } else if (it->data.type == MENU_ITEM_TYPE_STR) {
        const char *(*f)(void) = it->data.get_text_value2;
        lib_sprintf(buf, len, "%s %s", it->data.text, f());
    } else {
        lib_sprintf(buf, len, "%s", it->data.text);
    }
}

static void main_menu_draw_cb(void *vptr)
{
    struct main_menu_data_s *d = vptr;
    char buf[64];
    ui_draw_erase_buf();
    if (d->refresh) {
        lbxgfx_set_frame_0(d->gfx_vortex);
        d->refresh = false;
    } else {
        hw_video_copy_back_from_page2();
    }
    lbxgfx_draw_frame(0, 0, d->gfx_vortex, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame_offs(0, 0, d->gfx_title, 0, 0, UI_VGA_W - 1, 191, UI_SCREEN_W, ui_scale);
    hw_video_copy_back_to_page2();
    lbxfont_select(2, 7, 0, 0);
    lbxfont_print_str_right(315, 193, PACKAGE_NAME " " VERSION_STR, UI_SCREEN_W, ui_scale);
    if (force_restart) {
        lbxfont_select(2, 2, 0, 0);
        lbxfont_print_str_center(160, 60, "Application restart required", UI_SCREEN_W, ui_scale);
    }
    for (int i = 0; i < d->item_count; ++i) {
        struct main_menu_item_s *it = &d->items[i];
        if (it->active) {
            lbxfont_select(it->font_i, (d->highlight == i) ? 3 : 2, 0, 0);
        } else {
            lbxfont_select(it->font_i, 7, 0, 0);
        }
        main_menu_get_item_string(it, buf, sizeof(buf));
        lbxfont_print_str_center(it->x, it->y, buf, UI_SCREEN_W, ui_scale);
    }
    d->frame = (d->frame + 1) % 0x30;
}

/* -------------------------------------------------------------------------- */

static void main_menu_set_item_wh(struct main_menu_data_s *d, struct main_menu_item_s *it)
{
    char buf[64];
    lbxfont_select(it->font_i, 7, 0, 0);
    main_menu_get_item_string(it, buf, sizeof(buf));
    it->w = lbxfont_calc_str_width(buf);
    it->h = lbxfont_get_height();
}

static void main_menu_set_item_dimensions(struct main_menu_data_s *d, int i)
{
    struct main_menu_item_s *it = &d->items[i];
    uint16_t step_y;
    it->font_i = 4;
    main_menu_set_item_wh(d, it);
    step_y = 0x40 / d->item_count;
    it->x = 0xa0;
    it->y = 0x7f + step_y * i;
}

static void mm_game_set_item_dimensions(struct main_menu_data_s *d, int i)
{
    struct main_menu_item_s *it = &d->items[i];
    uint16_t step_y;
    it->font_i = 4;
    main_menu_set_item_wh(d, it);
    step_y = 0x40 / ((d->item_count + 1) / 2);
    it->x = i%2 ? 0xd0 : 0x70;
    if (i%2 == 0 && i == d->item_count - 1) {
        it->x = 0xa0;
    }
    it->y = 0x7f + step_y * (i/2);
}

static void mm_options_set_item_dimensions(struct main_menu_data_s *d, int i)
{
    struct main_menu_item_s *it = &d->items[i];
    uint16_t step_y;
    it->font_i = 5;
    main_menu_set_item_wh(d, it);
    if (d->item_count <= 6) {
        step_y = 0x48 / d->item_count;
        it->x = 0xa0;
        it->y = 0x77 + step_y * i;
    } else {
        step_y = 0x48 / ((d->item_count + 1) / 2);
        it->x = i%2 ? 0xe0 : 0x60;
        if (i%2 == 0 && i == d->item_count - 1) {
            it->x = 0xa0;
        }
        it->y = 0x77 + step_y * (i/2);
    }
}

static void mm_custom_set_item_dimensions(struct main_menu_data_s *d, int i)
{
    struct main_menu_item_s *it = &d->items[i];
    uint16_t step_y;
    it->font_i = 5;
    main_menu_set_item_wh(d, it);
    step_y = 0x40 / ((d->item_count + 1) / 2);
    it->x = i%2 ? 0xe0 : 0x60;
    it->y = 0x7f + step_y * (i/2);
    if (i == d->item_count - 1) {
        it->x = 0xe0;
        it->y = 0x7f + 0x30;
    }
    if (i == d->item_count - 2) {
        it->x = 0x60;
        it->y = 0x7f + 0x30;
    }
}

static void main_menu_make_main_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = main_menu_set_item_dimensions;
    menu_make_page_conditional(menu_allocate_item(), "Game", MAIN_MENU_PAGE_GAME, main_menu_game_active, MOO_KEY_g);
    menu_make_page(menu_allocate_item(), "Rules", MAIN_MENU_PAGE_OPTIONS_RULES, MOO_KEY_r);
    menu_make_page(menu_allocate_item(), "UI Options", MAIN_MENU_PAGE_OPTIONS, MOO_KEY_o);
    menu_make_action(menu_allocate_item(), "Quit to OS", MAIN_MENU_ACT_QUIT_GAME, MOO_KEY_q);
}

static void main_menu_make_game_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_game_set_item_dimensions;
    menu_make_action(menu_allocate_item(), "Tutor", MAIN_MENU_ACT_TUTOR, MOO_KEY_t);
    menu_make_action_conditional(menu_allocate_item(), "Continue", MAIN_MENU_ACT_CONTINUE_GAME, main_menu_have_save_continue, MOO_KEY_c);
    menu_make_action_conditional(menu_allocate_item(), "Load Game", MAIN_MENU_ACT_LOAD_GAME, main_menu_have_save_any, MOO_KEY_l);
    menu_make_action(menu_allocate_item(), "Load Game MOO", MAIN_MENU_ACT_LOAD_GAME_MOO13, MOO_KEY_m);
    menu_make_action(menu_allocate_item(), "New Game", MAIN_MENU_ACT_NEW_GAME, MOO_KEY_n);
    menu_make_page(menu_allocate_item(), "Custom Game", MAIN_MENU_PAGE_GAME_CUSTOM, MOO_KEY_u);
    menu_make_action(menu_allocate_item(), "Challenge", MAIN_MENU_ACT_CHALLENGE_GAME, MOO_KEY_h);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_game_custom_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_custom_set_item_dimensions;
    menu_make_enum(menu_allocate_item(), "Difficulty", mm_get_custom_difficulty_value, &game_opt_custom.difficulty, 0, DIFFICULTY_NUM - 1, MOO_KEY_d);
    menu_make_enum(menu_allocate_item(), "AI ID", mm_get_custom_ai_id_value, &game_opt_custom.ai_id, 0, GAME_AI_NUM - 1, MOO_KEY_a);
    menu_make_page(menu_allocate_item(), "Galaxy", MAIN_MENU_PAGE_GAME_CUSTOM_GALAXY, MOO_KEY_g);
    menu_make_page(menu_allocate_item(), "Homeworlds", MAIN_MENU_PAGE_GAME_CUSTOM_HOMEWORLDS, MOO_KEY_h);
    menu_make_back(menu_allocate_item());
    menu_make_action(menu_allocate_item(), "Next", MAIN_MENU_ACT_CUSTOM_GAME, MOO_KEY_n);
}

static void main_menu_make_game_custom_galaxy_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_enum(menu_allocate_item(), "Size", mm_get_custom_galaxy_size_value, &game_opt_custom.galaxy_size, 0, GALAXY_SIZE_HUGE, MOO_KEY_s);
    menu_make_str_func(menu_allocate_item(), "Galaxy Seed", mm_get_custom_galaxy_seed_str, mm_gen_custom_galaxy_seed, MOO_KEY_e);
    menu_make_int(menu_allocate_item(), "Players", &game_opt_custom.players, 2, PLAYER_NUM, MOO_KEY_p);
    menu_make_bool(menu_allocate_item(), "Improved generator", &game_opt_custom.improved_galaxy_generator, MOO_KEY_g);
    menu_make_int(menu_allocate_item(), "Num dist checks", &game_opt_custom.homeworlds.num_dist_checks, 0, PLAYER_NUM, MOO_KEY_d);
    menu_make_int(menu_allocate_item(), "Num OK planet checks", &game_opt_custom.homeworlds.num_ok_planet_checks, 0, PLAYER_NUM, MOO_KEY_o);
    menu_make_bool(menu_allocate_item(), "Nebulae", &game_opt_custom.nebulae, MOO_KEY_n);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_game_custom_homeworlds_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_int(menu_allocate_item(), "Max population", &game_opt_custom.homeworlds.max_pop, 50, 120, MOO_KEY_p);
    menu_make_enum(menu_allocate_item(), "Special", mm_get_custom_special_value, &game_opt_custom.homeworlds.special, 0, PLANET_SPECIAL_4XTECH, MOO_KEY_e);
    menu_make_int(menu_allocate_item(), "Num scouts", &game_opt_custom.homeworlds.num_scouts, 0, 5, MOO_KEY_s);
    menu_make_int(menu_allocate_item(), "Num fighters", &game_opt_custom.homeworlds.num_fighters, 0, 10, MOO_KEY_f);
    menu_make_int(menu_allocate_item(), "Num colony ships", &game_opt_custom.homeworlds.num_colony_ships, 0, 2, MOO_KEY_c);
    menu_make_bool(menu_allocate_item(), "Armed colony ships", &game_opt_custom.homeworlds.armed_colony_ships, MOO_KEY_a);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_game_set_item_dimensions;
    menu_make_page(menu_allocate_item(), "Input", MAIN_MENU_PAGE_OPTIONS_INPUT, MOO_KEY_i);
    menu_make_page(menu_allocate_item(), "Add-ons", MAIN_MENU_PAGE_OPTIONS_ADDONS, MOO_KEY_a);
    menu_make_page(menu_allocate_item(), "Sound", MAIN_MENU_PAGE_OPTIONS_SOUND, MOO_KEY_s);
    menu_make_page(menu_allocate_item(), "Starmap", MAIN_MENU_PAGE_OPTIONS_STARMAP, MOO_KEY_t);
    menu_make_page(menu_allocate_item(), "Video", MAIN_MENU_PAGE_OPTIONS_VIDEO, MOO_KEY_v);
    menu_make_page(menu_allocate_item(), "Misc", MAIN_MENU_PAGE_OPTIONS_MISC, MOO_KEY_m);
    menu_make_page(menu_allocate_item(), "Preset", MAIN_MENU_PAGE_PRESET, MOO_KEY_p);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_input_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Disable Mouse Warp", &ui_mouse_warp_disabled, MOO_KEY_w);
    menu_make_bool(menu_allocate_item(), "LMB Behavior Fix", &ui_mouse_lmb_fix, MOO_KEY_l);
    menu_make_bool(menu_allocate_item(), "Invert slider", &ui_mwi_slider, MOO_KEY_i);
    menu_make_bool(menu_allocate_item(), "Invert counter", &ui_mwi_counter, MOO_KEY_n);
    menu_make_bool_func(menu_allocate_item(), "Keyboard repeat", &ui_kbd_repeat, main_menu_toggle_kbd_repeat, MOO_KEY_k);
    menu_make_bool(menu_allocate_item(), "Direction Keys Fix", &ui_kbd_cursor_keys_fix, MOO_KEY_d);
    menu_make_bool(menu_allocate_item(), "Illogical Hotkey Fix", &ui_illogical_hotkey_fix, MOO_KEY_h);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_sound_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool_func(menu_allocate_item(), "Music", &opt_music_enabled, main_menu_toggle_music, MOO_KEY_m);
    menu_make_int_func(menu_allocate_item(), "Music volume", &opt_music_volume, 0, 128, main_menu_update_music_volume, MOO_KEY_u);
    menu_make_bool(menu_allocate_item(), "SFX", &opt_sfx_enabled, MOO_KEY_s);
    menu_make_int_func(menu_allocate_item(), "SFX volume", &opt_sfx_volume, 0, 128, main_menu_update_sfx_volume, MOO_KEY_f);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_video_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_int(menu_item_force_restart(menu_allocate_item()), "UI scale", &ui_scale_hint, 1, UI_SCALE_MAX, MOO_KEY_s);
    hw_opt_menu_make_page_video();
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_addons_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "UI Extra", &ui_extra_enabled, MOO_KEY_e);
    menu_make_bool(menu_allocate_item(), "Combat Autoresolve", &ui_space_combat_autoresolve, MOO_KEY_v);
    menu_make_bool(menu_allocate_item(), "UI SM Ships", &ui_sm_ships_enabled, MOO_KEY_s);
    menu_make_bool(menu_allocate_item(), "Load Options Extra", &ui_load_opts_extra, MOO_KEY_o);
    menu_make_page(menu_allocate_item(), "Message Filter", MAIN_MENU_PAGE_OPTIONS_ADDONS_MESSAGE_FILTER, MOO_KEY_m);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_addons_message_filter_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Max Factories", &game_opt_message_filter[FINISHED_FACT], MOO_KEY_f);
    menu_make_bool(menu_allocate_item(), "Max Population", &game_opt_message_filter[FINISHED_POPMAX], MOO_KEY_f);
    menu_make_bool(menu_allocate_item(), "Atmos / Soil", &game_opt_message_filter[FINISHED_SOILATMOS], MOO_KEY_f);
    menu_make_bool(menu_allocate_item(), "Stargate", &game_opt_message_filter[FINISHED_STARGATE], MOO_KEY_f);
    menu_make_bool(menu_allocate_item(), "Planetary Shield", &game_opt_message_filter[FINISHED_SHIELD], MOO_KEY_f);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_misc_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Skip Intro", &game_opt_skip_intro_always, MOO_KEY_i);
    menu_make_bool(menu_allocate_item(), "Skip Random News", &game_opt_skip_random_news, MOO_KEY_n);
    menu_make_bool(menu_allocate_item(), "Skip Copy Protection", &ui_copyprotection_disabled, MOO_KEY_p);
    menu_make_bool(menu_allocate_item(), "News Orion Colonized", &game_num_news_orion, MOO_KEY_o);
    menu_make_bool(menu_allocate_item(), "UI Delay Enabled", &ui_delay_enabled, MOO_KEY_d);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_starmap_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Explicit Cursor Context", &ui_sm_explicit_cursor_context, MOO_KEY_c);
    menu_make_bool(menu_allocate_item(), "Expanded Scroll", &ui_sm_expanded_scroll, MOO_KEY_e);
    menu_make_bool(menu_allocate_item(), "Mouseover Focus", &ui_sm_mouseover_focus, MOO_KEY_m);
    menu_make_bool(menu_allocate_item(), "Scroll by mouse", &ui_sm_mouse_scroll, MOO_KEY_s);
    menu_make_bool(menu_allocate_item(), "UHJK scroll", &ui_sm_uhjk_scroll, MOO_KEY_u);
    menu_make_bool(menu_allocate_item(), "Smoother scrolling", &ui_sm_smoother_scrolling, MOO_KEY_o);
    menu_make_int(menu_allocate_item(), "Scroll speed", &ui_sm_scroll_speed, 0, UI_SCROLL_SPEED_MAX, MOO_KEY_p);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_page(menu_allocate_item(), "AI Behavior", MAIN_MENU_PAGE_OPTIONS_RULES_AI, MOO_KEY_a);
    menu_make_page(menu_allocate_item(), "Difficulty Modifiers", MAIN_MENU_PAGE_OPTIONS_RULES_DIFFICULTY, MOO_KEY_d);
    menu_make_page(menu_allocate_item(), "Fleet Behavior", MAIN_MENU_PAGE_OPTIONS_RULES_FLEET_BEHAVIOR, MOO_KEY_f);
    menu_make_page(menu_allocate_item(), "Orbital Bombardment", MAIN_MENU_PAGE_OPTIONS_RULES_ORBITAL_BOMBARDMENT, MOO_KEY_o);
    menu_make_page(menu_allocate_item(), "Planetary Development", MAIN_MENU_PAGE_OPTIONS_RULES_PLANETARY_DEVELOPMENT, MOO_KEY_p);
    menu_make_page(menu_allocate_item(), "Slider Behavior", MAIN_MENU_PAGE_OPTIONS_RULES_SLIDER_BEHAVIOR, MOO_KEY_l);
    menu_make_page(menu_allocate_item(), "Space Battle", MAIN_MENU_PAGE_OPTIONS_RULES_BATTLE, MOO_KEY_s);
    menu_make_page(menu_allocate_item(), "Other", MAIN_MENU_PAGE_OPTIONS_RULES_OTHER, MOO_KEY_t);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_ai_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Transport Range Fix", &game_num_ai_trans_range_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Doom Stack Fix", &game_num_doom_stack_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "First Tech Cost Fix", &game_num_ai_first_tech_cost_fix, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_battle_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "No Tohit Accumulation", &game_num_bt_no_tohit_acc, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Precap Tohit", &game_num_bt_precap_tohit, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Wait No Reload", &game_num_bt_wait_no_reload, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_difficulty_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "AI Fleet Cheating Fix", &game_num_ai_fleet_cheating_fix, MOO_KEY_UNKNOWN);
    menu_make_int(menu_allocate_item(), "Tech Cost Multiplier", &game_num_tech_costmul, 50, 400, MOO_KEY_UNKNOWN);
    menu_make_int(menu_allocate_item(), "AI Tech Cost Multiplier", &game_num_tech_costmula2, 50, 100, MOO_KEY_UNKNOWN);
    menu_make_int(menu_allocate_item(), "Human Tech Cost Multiplier", &game_num_tech_costmuld2, 100, 400, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_fleet_behavior_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Monster Rest Attack", &game_num_monster_rest_att, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Retreat Redirection Fix", &game_num_retreat_redir_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Stargate Redirection Fix", &game_num_stargate_redir_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Transport Redirection Fix", &game_num_trans_redir_fix, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_orbital_bombardment_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Bio Damage Fix", &game_num_orbital_bio_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Allow Weapon 4", &game_num_orbital_weap_4, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Computer Bonus Fix", &game_num_orbital_comp_fix, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_planetary_development_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Factory Cost Fix", &game_num_factory_cost_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Hidden Child Labor Fix", &game_num_hidden_child_labor_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Leaving Transport Fix", &game_num_leaving_trans_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Soil Rounding Fix", &game_num_soil_rounding_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Waste Calc Fix", &game_num_waste_calc_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Colonized Factories Fix", &game_num_colonized_factories_fix, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_slider_behavior_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Cond Switch To Ind Fix", &game_num_cond_switch_to_ind_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Eco Done Fix", &game_num_slider_eco_done_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Newtech Adjust Fix", &game_num_newtech_adjust_fix, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_allocate_item(), "Waste Adjust Fix", &game_num_waste_adjust_fix, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_options_rules_other_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = mm_options_set_item_dimensions;
    menu_make_bool(menu_allocate_item(), "Deterministic RNG", &game_num_deterministic, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_item_force_restart(menu_allocate_item()), "Fix Other Bugs", &game_opt_fix_bugs, MOO_KEY_UNKNOWN);
    menu_make_bool(menu_item_force_restart(menu_allocate_item()), "Starting Ships Fix", &game_opt_fix_starting_ships, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

static void main_menu_make_preset_page(struct main_menu_data_s *d)
{
    d->set_item_dimensions = main_menu_set_item_dimensions;
    menu_make_func(menu_allocate_item(), "Classic", mm_enable_preset_classic, MOO_KEY_c);
    menu_make_func(menu_allocate_item(), "1oom", mm_enable_preset_1oom, MOO_KEY_1);
    menu_make_bool(menu_item_force_restart(menu_allocate_item()), "Reset To Default Settings", &opt_cfg_reset, MOO_KEY_UNKNOWN);
    menu_make_back(menu_allocate_item());
}

/* -------------------------------------------------------------------------- */

struct main_menu_page_s {
    void (*make_page)(struct main_menu_data_s *);
};

static struct main_menu_page_s mm_pages[MAIN_MENU_PAGE_NUM] = {
    {
        main_menu_make_main_page,
    },
    {
        main_menu_make_game_page,
    },
    {
        main_menu_make_game_custom_page,
    },
    {
        main_menu_make_game_custom_galaxy_page,
    },
    {
        main_menu_make_game_custom_homeworlds_page,
    },
    {
        main_menu_make_options_page,
    },
    {
        main_menu_make_options_input_page,
    },
    {
        main_menu_make_options_sound_page,
    },
    {
        main_menu_make_options_video_page,
    },
    {
        main_menu_make_options_addons_page,
    },
    {
        main_menu_make_options_addons_message_filter_page,
    },
    {
        main_menu_make_options_misc_page,
    },
    {
        main_menu_make_options_starmap_page,
    },
    {
        main_menu_make_options_rules_page,
    },
    {
        main_menu_make_options_rules_ai_page,
    },
    {
        main_menu_make_options_rules_battle_page,
    },
    {
        main_menu_make_options_rules_difficulty_page,
    },
    {
        main_menu_make_options_rules_fleet_behavior_page,
    },
    {
        main_menu_make_options_rules_orbital_bombardment_page,
    },
    {
        main_menu_make_options_rules_planetary_development_page,
    },
    {
        main_menu_make_options_rules_slider_behavior_page,
    },
    {
        main_menu_make_options_rules_other_page,
    },
    {
        main_menu_make_preset_page,
    },
};

static bool main_menu_load_page(struct main_menu_data_s *d, main_menu_page_id_t page_i)
{
    if (page_i < 0 || page_i >= MAIN_MENU_PAGE_NUM) {
        return false;
    }
    d->current_page_i = page_i;
    uiobj_table_clear();
    menu_clear();
    mm_pages[page_i].make_page(d);
    d->item_count = menu_get_item_count();
    d->scrollmisc = 0;
    d->oi_plus = uiobj_add_inputkey(MOO_KEY_PLUS);
    d->oi_minus = uiobj_add_inputkey(MOO_KEY_MINUS);
    d->oi_equals = uiobj_add_inputkey(MOO_KEY_EQUALS);
    for (int i = 0; i < d->item_count; ++i) {
        struct main_menu_item_s *it = &d->items[i];
        it->data = *menu_get_item(i);
        d->set_item_dimensions(d, i);
        it->active = it->data.is_active ? it->data.is_active() : true;
        if (it->active) {
            it->oi = uiobj_add_mousearea(it->x - it->w / 2, it->y, it->x + it->w / 2, it->y + it->h - 2, it->data.key);
            it->oi_shift = uiobj_add_inputkey(it->data.key | MOO_MOD_SHIFT);
            it->oi_wheel = uiobj_add_mousewheel(it->x - it->w / 2, it->y, it->x + it->w / 2, it->y + it->h - 2, &d->scrollmisc);
        } else {
            it->oi = UIOBJI_INVALID;
            it->oi_shift = UIOBJI_INVALID;
            it->oi_wheel = UIOBJI_INVALID;
        }
    }
    return true;
}

static void main_menu_push_page(struct main_menu_data_s *d, main_menu_page_id_t page_i)
{
    if (d->page_stack_i + 1 >= MM_PAGE_STACK_SIZE) {
        return;
    }
    if (main_menu_load_page(d, page_i)) {
        ++d->page_stack_i;
        d->page_stack[d->page_stack_i] = page_i;
    }
}

static void main_menu_pop_page(struct main_menu_data_s *d) {
    if (d->page_stack_i > 0) {
        ui_sound_play_sfx_06();
        --d->page_stack_i;
        main_menu_load_page(d, d->page_stack[d->page_stack_i]);
    }
}

static int main_menu_get_item(struct main_menu_data_s *d, int16_t oi)
{
    for (int i = 0; i < d->item_count; ++i) {
        if (oi == d->items[i].oi) {
            return i;
        }
    }
    return -1;
}

static int main_menu_get_item_shift(struct main_menu_data_s *d, int16_t oi)
{
    for (int i = 0; i < d->item_count; ++i) {
        if (oi == d->items[i].oi_shift) {
            return i;
        }
    }
    return -1;
}

static int main_menu_get_item_wheel(struct main_menu_data_s *d, int16_t oi)
{
    for (int i = 0; i < d->item_count; ++i) {
        if (oi == d->items[i].oi_wheel) {
            return i;
        }
    }
    return -1;
}

static void main_menu_item_do_plus(struct main_menu_data_s *d, int item_i)
{
    const struct main_menu_item_s *it = &d->items[item_i];
    if (it->data.need_restart) {
        force_restart = true;
    }
    if (it->data.type == MENU_ITEM_TYPE_RETURN) {
        d->ret = it->data.action_i;
        d->flag_done = true;
    } else if (it->data.type == MENU_ITEM_TYPE_PAGE) {
        main_menu_push_page(d, it->data.action_i);
    } else if (it->data.type == MENU_ITEM_TYPE_PAGE_BACK) {
        main_menu_pop_page(d);
    } else if (it->data.type == MENU_ITEM_TYPE_FUNCTION) {
        if (it->data.func) {
            it->data.func();
        }
    } else if (it->data.type == MENU_ITEM_TYPE_BOOL) {
        if (it->data.func) {
            it->data.func();
        } else {
            bool *v = it->data.value_ptr;
            *v = !*v;
        }
    } else if (it->data.type == MENU_ITEM_TYPE_INT
            || it->data.type == MENU_ITEM_TYPE_ENUM) {
        uint32_t *v = it->data.value_ptr;
        uint32_t old_value = *v;
        uint32_t step = 1;
        if (!kbd_is_modifier(MOO_MOD_CTRL)) {
            step = (it->data.value_max - it->data.value_min + 9) / 10;
        }
        *v += step;
        if (kbd_is_modifier(MOO_MOD_ALT)) {
            *v = it->data.value_max;
        }
        SETMIN(*v, it->data.value_max);
        if (it->data.func && old_value != *v) {
            it->data.func();
        }
    } else if (it->data.type == MENU_ITEM_TYPE_STR) {
        if (it->data.func) {
            it->data.func();
        }
    }
}

static void main_menu_item_do_minus(struct main_menu_data_s *d, int item_i)
{
    const struct main_menu_item_s *it = &d->items[item_i];
    if (it->data.need_restart) {
        force_restart = true;
    }
    if (it->data.type == MENU_ITEM_TYPE_INT
     || it->data.type == MENU_ITEM_TYPE_ENUM) {
        uint32_t *v = it->data.value_ptr;
        uint32_t old_value = *v;
        uint32_t step = 1;
        if (!kbd_is_modifier(MOO_MOD_CTRL)) {
            step = (it->data.value_max - it->data.value_min + 9) / 10;
        }
        if (((*v - it->data.value_min) < step) || kbd_is_modifier(MOO_MOD_ALT)) {
            *v = it->data.value_min;
        } else {
            *v -= step;
        }
        if (it->data.func && old_value != *v) {
            it->data.func();
        }
    }
    else {
        main_menu_item_do_plus(d, item_i);
    }
}

static main_menu_action_t main_menu_do(struct main_menu_data_s *d)
{
    bool flag_fadein = false;

    d->page_stack_i = -1;
    d->frame = 0;
    d->refresh = true;
    d->flag_done = false;
    d->ret = -1;
    if (ui_draw_finish_mode != 0) {
        ui_palette_fadeout_19_19_1();
    }
    lbxpal_select(1, -1, 0);
    ui_cursor_setup_area(1, &ui_cursor_area_all_i1);
    uiobj_table_clear();

    ui_draw_erase_buf();
    hw_video_copy_back_to_page2();

    main_menu_push_page(d, MAIN_MENU_PAGE_MAIN);

    d->highlight = main_menu_get_item(d, uiobj_at_cursor());
    uiobj_set_callback_and_delay(main_menu_draw_cb, d, 2);
    while (!d->flag_done) {
        int16_t oi1, oi2;
        ui_delay_prepare();
        oi1 = uiobj_handle_input_cond();
        oi2 = uiobj_at_cursor();
        d->highlight = main_menu_get_item(d, oi2);
        main_menu_draw_cb(d);
        d->clicked_i = main_menu_get_item(d, oi1);
        d->shift_i = main_menu_get_item_shift(d, oi1);
        d->wheel_i = main_menu_get_item_wheel(d, oi1);
        if ((oi1 == d->oi_plus || oi1 == d->oi_equals) && d->highlight != -1) {
            ui_sound_play_sfx_24();
            main_menu_item_do_plus(d, d->highlight);
        } else if ((oi1 == d->oi_minus) && d->highlight != -1) {
            ui_sound_play_sfx_24();
            main_menu_item_do_minus(d, d->highlight);
        } else if (d->wheel_i != -1) {
            if ((d->scrollmisc >= 0) != (ui_mwi_counter && 1) ) {
                ui_sound_play_sfx_24();
                main_menu_item_do_plus(d, d->wheel_i);
            } else {
                ui_sound_play_sfx_24();
                main_menu_item_do_minus(d, d->wheel_i);
            }
            d->scrollmisc = 0;
        } else if (d->clicked_i != -1) {
            ui_sound_play_sfx_24();
            main_menu_item_do_plus(d, d->clicked_i);
        } else if (d->shift_i != -1) {
            ui_sound_play_sfx_24();
            main_menu_item_do_minus(d, d->shift_i);
        } else if (oi1 == UIOBJI_ESC) {
            if (d->highlight != -1) {
                ui_sound_play_sfx_24();
                main_menu_item_do_minus(d, d->highlight);
            } else {
                main_menu_pop_page(d);
            }
        }

        uiobj_finish_frame();
        if ((ui_draw_finish_mode != 0) && !flag_fadein) {
            ui_palette_fadein_4b_19_1();
            flag_fadein = true;
        }
        ui_delay_ticks_or_click(2);
    }
    uiobj_unset_callback();
    return d->ret;
}

/* -------------------------------------------------------------------------- */

main_menu_action_t ui_main_menu(struct game_new_options_s *newopts, struct game_new_options_s *customopts, struct game_new_options_s *challengeopts, int *load_game_i_ptr)
{
    struct main_menu_data_s d;
    bool flag_done = false;
    main_menu_action_t ret = MAIN_MENU_ACT_QUIT_GAME;
    uiobj_set_help_id(-1);
    load_mainmenu_data(&d);
    ui_sound_play_music(1);
    ui_draw_finish_mode = 1;
    ui_cursor_setup_area(1, &ui_cursor_area_all_i0);
    while (!flag_done) {
        ret = main_menu_do(&d);
        ui_draw_finish_mode = 0;
        switch (ret) {
            case MAIN_MENU_ACT_NEW_GAME:
                flag_done = ui_new_game(newopts);
                ui_draw_finish_mode = 1;
                break;
            case MAIN_MENU_ACT_CUSTOM_GAME:
                flag_done = ui_custom_game(customopts);
                ui_draw_finish_mode = 1;
                break;
            case MAIN_MENU_ACT_CHALLENGE_GAME:
                flag_done = ui_challenge_game(challengeopts);
                ui_draw_finish_mode = 1;
                break;
            case MAIN_MENU_ACT_LOAD_GAME:
                {
                    int i;
                    i = ui_load_game();
                    if (i >= 0) {
                        *load_game_i_ptr = i;
                        flag_done = true;
                    }
                    ui_draw_finish_mode = 1;
                }
                break;
            case MAIN_MENU_ACT_LOAD_GAME_MOO13:
                {
                    int i;
                    i = ui_load_game_moo13();
                    if (i >= 0) {
                        *load_game_i_ptr = i;
                        flag_done = true;
                    }
                    ui_draw_finish_mode = 1;
                }
                break;
            case MAIN_MENU_ACT_QUIT_GAME:
                flag_done = true;
                break;
            default:
                flag_done = true;
                break;
        }
    }
    ui_sound_stop_music();
    /* call run_starmap_exe */
    if (ret != MAIN_MENU_ACT_QUIT_GAME) {
        ui_palette_fadeout_19_19_1();
    }
    free_mainmenu_data(&d);
    return ret;
}
