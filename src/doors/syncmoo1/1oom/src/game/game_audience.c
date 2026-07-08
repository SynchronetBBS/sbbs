#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_audience.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_spy.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_AUDIENCE 3

static void game_audience_clear_strtbl(struct audience_s *au)
{
    for (int i = 0; i < AUDIENCE_STR_MAX; ++i) {
        au->strtbl[i] = NULL;
    }
}

/* Prepare a menu with options for the player; condtbl indicates which options
 * are active. As a shortcut, if it is NULL, all options are active. The menu
 * can be presented to the player using the ui_audience_ask* functions. */
static void game_audience_choice(struct audience_s *au, const char *query, const char **options, const bool *condtbl, uint8_t num_entries)
{
    int i;
    au->buf = query;
    if (num_entries >= AUDIENCE_STR_MAX) {
        log_fatal_and_die("Too many strings in au->strtbl\n");
    }
    for (i = 0; i < num_entries; ++i) {
        au->strtbl[i] = options[i];
    }
    au->strtbl[i] = NULL;
    au->condtbl = condtbl;
}

static void game_audience_tech_choice(struct audience_s *au, const char *query, tech_field_t *fields, uint8_t *techs, int num_techs, bool forget_it_opt)
{
    struct strbuild_s strbuild = strbuild_init(au->strtbl_buf, AUDIENCE_STRTBL_BUFSIZE);
    int num_entries = forget_it_opt ? num_techs + 1 : num_techs;
    char tech_name[64];
    au->buf = query;
    if (num_entries >= AUDIENCE_STR_MAX) {
        log_fatal_and_die("Too many strings in au->strtbl\n");
    }

    int i;
    for (i = 0; i < num_techs; i++) {
        game_tech_get_name(au->g->gaux, fields[i], techs[i], tech_name, sizeof(tech_name));
        strbuild_catf(&strbuild, "%s %s", game_str_au_bull, tech_name);
        au->strtbl[i] = strbuild_finish(&strbuild);
    }
    if (forget_it_opt) {
        au->strtbl[i++] = game_str_au_opts_agree[1];  /* "Forget It" */
    }
    au->strtbl[i] = NULL;
    au->condtbl = NULL;
}

static void game_audience_prepare(struct audience_s *au, player_id_t ph, player_id_t pa)
{
    au->buf = NULL;
    au->ph = ph;
    au->pa = pa;
    game_audience_clear_strtbl(au);
    au->gfxi = 0;
    au->musi = 0;
    au->dtype_next = 0;
}

static void game_audience_start_human(struct audience_s *au)
{
    game_ai->aud_start_human(au);
}

static void game_audience_str_append_offer(struct strbuild_s *strbuild, const struct game_s *g, tech_field_t field, uint8_t tech, uint16_t bc)
{
    strbuild_append_char(strbuild, ' ');
    if (tech != 0) {
        char tech_name[64];
        game_tech_get_name(g->gaux, field, tech, tech_name, sizeof(tech_name));
        strbuild_catf(strbuild, "%s", tech_name);
        strbuild_catf(strbuild, " %s", game_str_au_tech);
    } else if (bc != 0) {
        strbuild_catf(strbuild, "%u %s", bc, game_str_bc);
    }
}

static void game_audience_append_tech(struct strbuild_s *str, struct game_s *g, tech_field_t field, uint8_t tech)
{
    char tech_name[64];
    game_tech_get_name(g->gaux, field, tech, tech_name, sizeof(tech_name));
    strbuild_catf(str, "%s", tech_name);
}

static void audience_build_diplo_msg(struct audience_s *au, bool framed)
{
    struct game_s *g = au->g;
    uint8_t dtype = au->dtype;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    const char *msg;
    struct strbuild_s strbuild = strbuild_init(au->diplo_msg, AUDIENCE_DIPLO_MSG_SIZE);

    /* Randomly select a varation of the message type. */
    int v = rnd_0_nm1(g->gaux->diplomat.d0[dtype], &g->seed);
    if (g->players == 2) {
        --v;
        SETMAX(v, 0);
    }
    g->evn.diplo_msg_subtype = v;
    msg = DIPLOMAT_MSG_PTR(g->gaux, v, dtype);

    /* Fill in placeholders in the message. */
    for (int i = 0; (i < DIPLOMAT_MSG_LEN) && (msg[i] != 0); ++i) {
        char c;
        c = msg[i];
        if (c & 0x80) {
            const char *s;
            const char *field;
            s = NULL;
            switch (c & 0x7f) {
                case 0:  /* Player race */
                    s = game_str_tbl_race[eh->race];
                    break;
                case 1:  /* AI race */
                    s = game_str_tbl_race[ea->race];
                    break;
                case 0x16:  /* "a" or "an" */
                    s = (strchr("aeiou", tolower(game_str_tbl_race[ea->race][0])) != NULL) ? "n" : "";
                    break;
                case 2:  /* "factories" or "bases" (sabotage) */
                    s = (!eh->diplo_p2[pa]) ? game_str_au_facts : game_str_au_bases;
                    break;
                case 0xb:  /* Current treaty */
                    if (eh->treaty[pa] == TREATY_ALLIANCE) {
                        s = game_str_au_allian;
                    } else if (eh->treaty[pa] == TREATY_NONAGGRESSION) {
                        s = game_str_au_nonagg;
                    } else if (eh->trade_bc[pa] != 0) {
                        s = game_str_au_tradea;
                    } else {
                        s = game_str_au_treaty;
                    }
                    break;
                case 0x17:  /* Broken treaty (by AI) */
                    if (ea->broken_treaty[ph] == TREATY_ALLIANCE) {
                        s = game_str_au_allian;
                    } else if (ea->broken_treaty[ph] == TREATY_NONAGGRESSION) {
                        s = game_str_au_nonagg;
                    } else if (ea->broken_treaty[ph] == TREATY_NONE) {
                        s = game_str_au_tradea;
                    } else {
                        s = game_str_au_treaty;
                    }
                    break;
                case 3:  /* Player name */
                    s = g->emperor_names[ph];
                    break;
                case 9:  /* AI leader name */
                    s = g->emperor_names[pa];
                    break;
                case 4:
                    field = game_str_tbl_te_field[eh->diplo_p2[pa]];
                    while (*field != '\0') {
                        strbuild_append_char(&strbuild, tolower(*field++));
                    }
                    /* WASBUG MOO1 printed "force Field" */
                    break;
                case 5:
                    s = g->planet[eh->diplo_p1[pa]].name;
                    break;
                case 6:
                    s = game_str_tbl_race[g->eto[eh->diplo_p2[pa]].race];
                    break;
                case 8:
                    s = game_str_tbl_race[g->eto[eh->attack_bounty[pa]].race];
                    break;
                case 0xc:
                    s = game_str_tbl_race[g->eto[eh->au_ask_break_treaty[pa]].race];
                    break;
                case 0xf:
                    strbuild_catf(&strbuild, "%i", g->year + YEAR_BASE);
                    break;
                case 0x13:
                    strbuild_catf(&strbuild, "%i", au->tribute_bc);
                    break;
                case 7:
                    strbuild_catf(&strbuild, "%u", eh->au_want_trade[pa]);
                    break;
                case 0x15:
                    strbuild_catf(&strbuild, "%u", eh->trade_bc[pa]);
                    break;
                case 0xa:
                    strbuild_catf(&strbuild, "\x02 %s\x01", game_str_au_amreca);
                    break;
                case 0x11:
                    s = game_str_tbl_race[g->eto[au->pstartwar].race];
                    break;
                case 0x12:
                    s = game_str_tbl_race[g->eto[au->pwar].race];
                    break;
                case 0xd:
                    if (eh->attack_gift_bc[pa] != 0) {
                        strbuild_catf(&strbuild, "%i %s.", eh->attack_gift_bc[pa], game_str_bc);
                    } else {
                        game_audience_append_tech(&strbuild, g, eh->attack_gift_field[pa], eh->attack_gift_tech[pa]);
                        strbuild_catf(&strbuild, " %s.", game_str_au_tech);
                    }
                    break;
                case 0xe:
                    game_audience_append_tech(&strbuild, g, eh->au_want_field[pa], eh->au_want_tech[pa]);
                    strbuild_catf(&strbuild, " %s.", game_str_au_tech);
                    break;
                case 0x10:
                    game_audience_append_tech(&strbuild, g, au->tribute_field, au->tribute_tech);
                    break;
                case 0x14:
                    game_audience_append_tech(&strbuild, g, au->tribute_field, au->tribute_tech);
                    strbuild_catf(&strbuild, " %s.", game_str_au_tech);
                    break;
                default:
                    strbuild_append_char(&strbuild, c);
                    break;
            }
            if (s) {
                strbuild_catf(&strbuild, "%s", s);
            }
        } else {
            strbuild_append_char(&strbuild, c);
        }
    }
    if (framed) {
        strbuild_catf(&strbuild, "\x02 %s\x01", game_str_au_framed);
    }
    au->gfxi = DIPLOMAT_MSG_GFX(msg);
    au->musi = DIPLOMAT_MSG_MUS(msg);
}

/* Build the diplomat message if necessary and set it to be displayed. */
static void game_audience_get_diplo_msg(struct audience_s *au)
{
    struct game_s *g = au->g;
    uint8_t dtype = au->dtype;
    bool framed = false;
    if ((dtype == 5) || (dtype == 7) || (dtype == 35) || (dtype == 37) || (dtype == 43) || (dtype == 45) || (dtype == 51) || (dtype == 53)) {
        framed = true;
        au->dtype -= 1;
    }
    if (g->evn.diplo_msg_subtype == -1) {
        audience_build_diplo_msg(au, framed);  /* Sets diplo_msg_subtype. */
    }
    au->buf = au->diplo_msg;
}

static void game_audience_ai_offers_bounty(struct audience_s *au)
{
    ui_audience_show2(au);
}

static void game_audience_ai_pays_bounty(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);

    ui_audience_show2(au);
    if (game_num_aud_bounty_give) {  /* WASBUG MOO1 never gives the bounty */
        eh->reserve_bc += eh->attack_gift_bc[pa];
        if (eh->attack_gift_tech[pa] != 0) {
            game_tech_get_new(g, ph, eh->attack_gift_field[pa], eh->attack_gift_tech[pa], TECHSOURCE_TRADE, pa, PLAYER_NONE, false);
        }
    }
}

static void game_audience_set_dtype(struct audience_s *au, uint8_t dtype, int a2)
{
    struct game_s *g = au->g;
    au->dtype = game_ai->aud_get_dtype(au, dtype, a2);
    g->evn.diplo_msg_subtype = -1;
    game_audience_get_diplo_msg(au);
    ui_audience_show3(au);
}

static void game_audience_ai_offers_tech_trade(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    int num_techs = MIN(eh->au_tech_trade_num[pa], 4);
    int16_t selected = -1, default_selected = num_techs + 1;

    ui_audience_show2(au);
    game_audience_tech_choice(au, game_str_au_inxchng, eh->au_tech_trade_field[pa], eh->au_tech_trade_tech[pa], num_techs, true);
    selected = ui_audience_ask2a(au);
    selected = (selected != -1) ? selected : default_selected;
    game_tech_get_new(g, ph, eh->au_tech_trade_field[pa][selected], eh->au_tech_trade_tech[pa][selected], TECHSOURCE_TRADE, pa, PLAYER_NONE, false);
    /* WASBUG? 1oom v1.0 did not give a tech to the AI. Not sure if this bug
     * was present in MOO1 or introduced in 1oom. */
    game_tech_get_new(g, pa, eh->au_want_field[pa], eh->au_want_tech[pa], TECHSOURCE_TRADE, pa, PLAYER_NONE, false);
}

static void game_audience_ai_breaks_alliance(struct audience_s *au)
{
    game_diplo_break_treaty(au->g, au->pa, au->ph);
    au->mode = 6;
    game_audience_set_dtype(au, 77, 3);
}

static bool game_audience_ai_offers_treaty(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    uint8_t dtype = au->dtype;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int16_t selected = -1;

    game_audience_clear_strtbl(au);
    if (dtype == 27) {
        game_audience_choice(au, au->diplo_msg, game_str_au_opts_agree, NULL, 2);
    }
    else {
        game_audience_choice(au, au->diplo_msg, game_str_au_opts_accept, NULL, 2);
    }
    selected = ui_audience_ask2b(au);
    /* If the player refuses, the AI may offer to sweeten the deal. */
    if (((selected == 1) || (selected == -1)) && ((eh->offer_tech[pa] != 0) || (eh->offer_bc[pa] != 0))) {
        struct strbuild_s strbuild = strbuild_init(au->diplo_msg, AUDIENCE_DIPLO_MSG_SIZE);
        bool flag_qtype = (!rnd_0_nm1(2, &g->seed));
        strbuild_catf(&strbuild, "%s", flag_qtype ? game_str_au_whatif1 : game_str_au_perrec1);
        game_audience_str_append_offer(&strbuild, g, eh->offer_field[pa], eh->offer_tech[pa], eh->offer_bc[pa]);
        if (flag_qtype) {
            strbuild_catf(&strbuild, " %s", game_str_au_whatif2);
        }
        strbuild_catf(&strbuild, "%s", game_str_au_ques);
        au->buf = au->diplo_msg;
        /* We still have accept/reject or agree/forget-it in au->strtbl. */
        selected = ui_audience_ask2b(au);
    }
    if (selected == 0) {  /* Player accepted. */
        switch (au->dtype) {
            case 24:
                game_diplo_set_treaty(g, ph, pa, TREATY_NONAGGRESSION);
                break;
            case 25:
                game_diplo_set_treaty(g, ph, pa, TREATY_ALLIANCE);
                break;
            case 26:
                game_diplo_set_trade(g, ph, pa, eh->au_want_trade[pa]);
                break;
            case 27:  /* Player breaks alliance with the AI's enemy. */
                game_diplo_break_treaty(g, ph, eh->au_ask_break_treaty[pa]);
                break;
            case 30:
                game_diplo_stop_war(g, ph, pa);
                if (eh->relation1[pa] < 80) {
                    eh->relation1[pa] += 20;
                    ea->relation1[ph] = eh->relation1[pa];
                }
                game_diplo_annoy(g, ph, pa, 2);
                break;
            case 76:  /* Player joins their ally's war. */
                game_diplo_start_war(g, ph, au->pwar);
                au->mode = 6;
                game_audience_set_dtype(au, 78, 3);
                break;
            default:
                log_fatal_and_die("%s: BUG unhandled dtype %u\n", __func__, au->dtype);
                break;
        }
        /* BUG? Give the deal-sweetener. Note that this happens even if the
         * player didn't refuse at first and thus didn't see the sweetener.
         * This may silently add some money to the player's reserve. */

        /* BUG? AI doesn't give a deal-sweetener for honoring your alliance.
         * That makes some sense, but can it ever happen that the AI offers
         * a sweetener and doesn't give it? */
        if (dtype != 76) {
            eh->reserve_bc += eh->offer_bc[pa];
            /* BUG? AI doesn't lose money? */
            if (eh->offer_tech[pa] != 0) {
                game_tech_get_new(g, ph, eh->offer_field[pa], eh->offer_tech[pa], TECHSOURCE_TRADE, pa, PLAYER_NONE, false);
            }
        }
    }
    else {  /* Player refused. */
        switch (dtype) {
            case 27:  /* Player didn't break alliance with the AI's enemy. */
                /* ea->relation1[ph] -= rnd_1_n(6, &g->seed) + 6; SETMAX(ea->relation1[ph], -100); BUG? useless */
                ea->relation1[ph] = -100;
                eh->relation1[pa] = -100;
                break;
            case 76:  /* Player did not honor their alliance. */
                game_audience_ai_breaks_alliance(au);
                break;
            default:
                break;
        }
    }
    return selected == 0;
}

/* If the AI wants to offer a deal to the player, but the player is
 * allied with one of its war enemies, it asks the player to break the
 * alliance and does not make the offer if they refuse.
 *
 * The function returns true if either
 *   - the player is not allied with an enemy of the AI, or
 *   - the player chose to break the alliance
 * and false otherwise. */
static bool game_audience_check_alliances(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa, pf = PLAYER_NONE;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    bool retval;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if ((i != ph) && (i != pa) && (ea->treaty[i] >= TREATY_WAR) && (eh->treaty[i] == TREATY_ALLIANCE)) {
            pf = i;
        }
    }
    if (pf == PLAYER_NONE) {
        return true;
    }
    au->dtype = 27;
    eh->au_ask_break_treaty[pa] = pf;
    g->evn.diplo_msg_subtype = -1;
    game_audience_get_diplo_msg(au);
    /* Note: The below game_audience_ai_offers_treaty call is for asking the
     * player to break the alliance. The main offer will be made in
     * game_audience_do. */
    retval = game_audience_ai_offers_treaty(au);
    if ((au->dtype_next == 76) && !retval) {
        /* The AI is allied with the player and was going to ask them to join
         * a war against an enemy. But the player also has an alliance with one
         * of the AI's enemies and refused to break it, so the AI now cancels
         * the alliance with the player. In the grimdark future of the 23rd
         * century, you can't be friends with everyone. */
        game_audience_ai_breaks_alliance(au);
    }
    return retval;
}

static int game_audience_sweeten(struct audience_s *au, int a0)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int bc = 0;
    tech_field_t field = 0;
    uint8_t tech = 0;
    bool flag_bc;
    int16_t selected = 0;
    char query[320];
    struct strbuild_s strbuild;
    if (!game_ai->aud_sweeten(au, &bc, &field, &tech)) {
        return 0;
    }
    if ((bc == 0) && (tech == 0)) {
        return 0;
    }
    if (((tech != 0) && (a0 == 1)) || (bc == 0)) {
        flag_bc = false;
    } else {
        tech = 0;
        flag_bc = true;
    }
    strbuild = strbuild_init(query, sizeof(query));
    if (!rnd_0_nm1(2, &g->seed)) {
        strbuild_catf(&strbuild, "%s", game_str_au_perthr1);
        game_audience_str_append_offer(&strbuild, g, field, tech, bc);
        strbuild_catf(&strbuild, " %s", game_str_au_perthr2);
    } else {
        strbuild_catf(&strbuild, "%s", game_str_au_alsoof1);
        game_audience_str_append_offer(&strbuild, g, field, tech, bc);
        strbuild_catf(&strbuild, " %s", game_str_au_alsoof2);
    }
    game_audience_choice(au, query, game_str_au_opts_agree, NULL, 2);
    selected = ui_audience_ask3(au);
    if ((selected == -1) || (selected == 1)) {
        return 1;
    }
    if (!flag_bc) {
        game_tech_get_new(g, pa, field, tech, TECHSOURCE_TRADE, ph, PLAYER_NONE, false);
    } else {
        eh->reserve_bc -= bc;
        ea->reserve_bc += bc;
    }
    return 3;
}

static player_id_t audience_menu_race(struct audience_s *au, player_id_t *rtbl, uint8_t rnum, const char *titlestr)
{
    struct game_s *g = au->g;
    struct strbuild_s strtbl_build = strbuild_init(au->strtbl_buf, AUDIENCE_STRTBL_BUFSIZE);
    int16_t selected = 0;
    for (int i = 0; i < rnum; ++i) {
        empiretechorbit_t *e = &(g->eto[rtbl[i]]);
        strbuild_catf(&strtbl_build, "%s %s", game_str_au_bull, game_str_tbl_races[e->race]);
        au->strtbl[i] = strbuild_finish(&strtbl_build);
    }
    au->strtbl[rnum] = game_str_au_opts_agree[1];  /* "Forget It" */
    au->strtbl[rnum + 1] = NULL;
    au->buf = titlestr;
    au->condtbl = NULL;
    selected = ui_audience_ask4(au);
    if ((selected >= 0) && (selected < rnum)) {
        return rtbl[selected];
    }
    return PLAYER_NONE;
}

static void audience_menu_treaty(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int16_t selected = 0;
    bool condtbl[6];
    uint8_t war_num, all_num, dtype;
    player_id_t war_tbl[PLAYER_NUM], all_tbl[PLAYER_NUM];
    int si;
    for (int i = 0; i < TBLLEN(condtbl); ++i) {
        condtbl[i] = true;
    }
    if (eh->treaty[pa] != TREATY_NONE) {
        condtbl[0] = false;
    }
    if (eh->treaty[pa] >= TREATY_ALLIANCE) {
        condtbl[1] = false;
    }
    if (eh->treaty[pa] < TREATY_WAR) {
        condtbl[2] = false;
    }
    if (eh->treaty[pa] >= TREATY_WAR) {
        condtbl[3] = false;
    }
    war_num = 0;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if ((i != ph) && (i != pa) && (ea->treaty[i] < TREATY_WAR)) {
            war_tbl[war_num++] = i;
        }
    }
    if (war_num == 0) {
        condtbl[3] = false;
    }
    all_num = 0;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if ((i != ph) && (i != pa) && BOOLVEC_IS1(ea->contact, i) && ((ea->treaty[i] == TREATY_ALLIANCE) || (game_num_aud_ask_break_nap && (ea->treaty[i] == TREATY_NONAGGRESSION)))) {
            all_tbl[all_num++] = i;
        }
    }
    if (all_num == 0) {
        condtbl[4] = false;
    }
    game_audience_choice(au, game_str_au_youprte, game_str_au_opts_treaty, condtbl, 6);
    selected = ui_audience_ask4(au);
    switch (selected) {
        case 0: /* Non-Aggression Pact */
            si = game_ai->aud_treaty_nap(au);
            if ((si == 1) || (si == 2)) {
                si = game_audience_sweeten(au, si);
            }
            if (si == 3) {
                game_diplo_set_treaty(g, ph, pa, TREATY_NONAGGRESSION);
            }
            dtype = 62;
            break;
        case 1: /* Alliance */
            si = game_ai->aud_treaty_alliance(au);
            if ((si == 1) || (si == 2)) {
                si = game_audience_sweeten(au, si);
            }
            if (si == 3) {
                game_diplo_set_treaty(g, ph, pa, TREATY_ALLIANCE);
            }
            dtype = 63;
            break;
        case 2: /* Peace Treaty */
            si = game_ai->aud_treaty_peace(au);
            if ((si == 1) || (si == 2)) {
                si = game_audience_sweeten(au, si);
            }
            if (si == 3) {
                game_diplo_stop_war(g, ph, pa);
            }
            game_diplo_annoy(g, ph, pa, 2);
            dtype = 65;
            break;
        case 3: /* Declaration of War on Another Race */
            dtype = 67;
            au->pstartwar = audience_menu_race(au, war_tbl, war_num, game_str_au_whowar);
            if (au->pstartwar != PLAYER_NONE) {
                si = game_ai->aud_treaty_declare_war(au);
                if ((si == 1) || (si == 2)) {
                    si = game_audience_sweeten(au, si);
                }
                if (si == 3) {
                    game_diplo_start_war(g, pa, au->pstartwar);
                }
            } else {
                selected = -1;
            }
            break;
        case 4: /* Break Alliance With Another Race */
            dtype = 68;
            au->pwar = audience_menu_race(au, all_tbl, all_num, game_str_au_whobrk);
            if (au->pwar != PLAYER_NONE) {
                si = game_ai->aud_treaty_break_alliance(au);
                if ((si == 1) || (si == 2)) {
                    si = game_audience_sweeten(au, si);
                }
                if (si == 3) {
                    game_diplo_break_treaty(g, pa, au->pwar);
                }
            } else {
                selected = -1;
            }
            break;
        case 5: /* Forget It */
        default:
            selected = -1;
            dtype = 0;
            si = 0;
            break;
    }
    if ((selected != -1) && (si != 1)) {
        game_audience_set_dtype(au, dtype, si);
    }
}

static void audience_menu_trade(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    int16_t selected = 0;
    struct strbuild_s str = strbuild_init(au->strtbl_buf, AUDIENCE_STRTBL_BUFSIZE);
    au->buf = game_str_au_youprta;
    for (int i = 0; i < AUDIENCE_BC_MAX; ++i) {
        strbuild_catf(&str, "%s %i %s", game_str_au_bull, au->bctbl[i], game_str_au_bcpery);
        au->strtbl[i] = strbuild_finish(&str);
    }
    au->strtbl[au->num_bc] = game_str_au_opts_agree[1];  /* "Forget It" */
    au->strtbl[au->num_bc + 1] = NULL;
    au->condtbl = NULL;
    selected = ui_audience_ask4(au);
    game_diplo_annoy(g, ph, pa, 1);
    eh->mood_trade[pa] -= rnd_1_n(30, &g->seed);
    if ((selected != -1) && (selected != au->num_bc)) {
        int si;
        au->new_trade_bc = au->bctbl[selected];
        si = game_ai->aud_trade(au);
        if (si < 3) {
            si = 0;
        } else {
            game_diplo_set_trade(g, ph, pa, au->new_trade_bc);
        }
        game_audience_set_dtype(au, 64, si);
    }
}

static void audience_menu_threat(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    int16_t selected = 0;
    bool condtbl[5];
    uint8_t dtype = 0;
    for (int i = 0; i < TBLLEN(condtbl); ++i) {
        condtbl[i] = true;
    }
    if (eh->treaty[pa] != TREATY_NONAGGRESSION) {
        condtbl[0] = false;
    }
    if (eh->treaty[pa] != TREATY_ALLIANCE) {
        condtbl[1] = false;
    }
    if (eh->trade_bc[pa] == 0) {
        condtbl[2] = false;
    }
    game_audience_choice(au, game_str_au_youract, game_str_au_opts_threaten, condtbl, 5);
    au->condtbl = condtbl;
    selected = ui_audience_ask4(au);
    switch (selected) {
        case 0: /* Break Non-Aggression Pact */
        case 1: /* Break Alliance */
            game_diplo_break_treaty(g, ph, pa);
            dtype = 73;
            selected = 0;
            break;
        case 2: /* Break Trade Agreement */
            game_diplo_break_trade(g, ph, pa);
            dtype = 73;
            selected = 0;
            break;
        case 3: /* Threaten To Attack */
            selected = 0;
            dtype = game_ai->aud_threaten(au);
            break;
        default:
            break;
    }
    if ((g->ai_id == GAME_AI_CLASSIC) || ((selected != -1) && (selected != 4))) {
        game_diplo_annoy(g, ph, pa, 10);    /* WASBUG MOO1 does this before the if, annoying by merely entering the menu */
    }
    if ((selected != -1) && (selected != 4)) {
        game_audience_set_dtype(au, dtype, 3);
    }
}

static void audience_menu_tribute(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int16_t selected = 0;
    uint8_t bcnum;
    uint32_t reserve = eh->reserve_bc;
    uint16_t bctbl[4];
    struct strbuild_s strtbl_build = strbuild_init(au->strtbl_buf, AUDIENCE_STRTBL_BUFSIZE);
    au->buf = game_str_au_whattr;
    SETMIN(reserve, game_num_max_tribute_bc);
    reserve = ((reserve) / 25) * 25;
    if (reserve < 100) {
        bcnum = reserve / 25;
        bctbl[0] = 25;
        bctbl[1] = 50;
        bctbl[2] = 75;
        bctbl[3] = 100;
    } else {
        bcnum = 4;
        bctbl[0] = ((reserve / 4) / 25) * 25;
        bctbl[1] = ((reserve / 2) / 25) * 25;
        bctbl[2] = (((3 * reserve) / 4) / 25) * 25;
        bctbl[3] = reserve;
    }
    for (int i = 0; i < bcnum; ++i) {
        strbuild_catf(&strtbl_build, "%s %i %s", game_str_au_bull, bctbl[i], game_str_bc);
        au->strtbl[i] = strbuild_finish(&strtbl_build);
    }
    au->strtbl[bcnum] = game_str_au_techn;
    au->strtbl[bcnum + 1] = NULL;
    au->condtbl = NULL;
    selected = ui_audience_ask4(au);
    if (selected == -1) {
        return;
    }
    if (selected < bcnum) {
        eh->reserve_bc -= bctbl[selected];
        ea->reserve_bc += bctbl[selected];
        game_ai->aud_tribute_bc(au, selected, bctbl[selected]);
        game_audience_set_dtype(au, 1, 3);
    } else {
        struct spy_esp_s s[1];
        s->spy = pa;
        s->target = ph;
        /* WASBUG
           MOO1 does game_spy_esp_sub1(g, s, tav[i], 0) where tav is a global table also used by audience_menu_tech and i is bcnum.
           If the tech menu is never visited or the table is not filled up to bcnum then the value is 0.
           Values larger than 0 only filter out tech of lesser worth, and who would not want to use those as bribes?
        */
        if (game_spy_esp_sub1(g, s, 0, 0) > 0) {
            int num_techs = MIN(s->tnum, 4);
            game_audience_tech_choice(au, game_str_au_whattr, s->tbl_field, s->tbl_tech2, num_techs, true);
            selected = ui_audience_ask4(au);
            if ((selected != -1) && (selected < s->tnum) && (selected < 4)) {
                game_tech_get_new(g, pa, s->tbl_field[selected], s->tbl_tech2[selected], TECHSOURCE_TRADE, ph, PLAYER_NONE, false);
                game_ai->aud_tribute_tech(au, selected, s->tbl_field[selected], s->tbl_tech2[selected]);
                eh->tribute_field[pa] = s->tbl_field[selected];
                eh->tribute_tech[pa] = s->tbl_tech2[selected];
                game_audience_set_dtype(au, 1, 3);
            }
        } else {
            game_audience_set_dtype(au, 75, 3);
        }
    }
}

static void audience_menu_tech(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    int v = game_ai->aud_tech_scale(au);
    {
        struct spy_esp_s s[1];
        tech_field_t taf[TECH_SPY_MAX]; /* diplo_p2_sub1_field */
        uint8_t tat[TECH_SPY_MAX];
        int tav[TECH_SPY_MAX];
        tech_field_t thf[TECH_SPY_MAX][TECH_SPY_MAX];
        uint8_t tht[TECH_SPY_MAX][TECH_SPY_MAX];
        tech_field_t thaf[TECH_SPY_MAX * TECH_SPY_MAX];
        uint8_t that[TECH_SPY_MAX * TECH_SPY_MAX];
        int tanum, thnum[TECH_SPY_MAX], total_thnum;
        s->spy = ph;
        s->target = pa;
        if (game_spy_esp_sub1(g, s, 0, 1) > 0) {
            tanum = s->tnum;
            for (int i = 0; i < tanum; ++i) {
                taf[i] = s->tbl_field[i];
                tat[i] = s->tbl_tech2[i];
                tav[i] = (s->tbl_value[i] * v) / 100;
            }
            s->spy = pa;
            s->target = ph;
            total_thnum = 0;
            for (int i = 0; i < tanum; ++i) {
                if (game_spy_esp_sub1(g, s, tav[i], 0) > 0) {
                    int n;
                    n = s->tnum;
                    thnum[total_thnum] = n;
                    for (int j = 0; j < n; ++j) {
                        thf[total_thnum][j] = s->tbl_field[j];
                        tht[total_thnum][j] = s->tbl_tech2[j];
                    }
                    thaf[total_thnum] = taf[i];
                    that[total_thnum] = tat[i];
                    ++total_thnum;
                }
            }
            /*6568e*/
            if (total_thnum > 0) {
                int16_t selected = 0;
                int num_techs = MIN(total_thnum, 5);
                game_diplo_annoy(g, ph, pa, 1);
                eh->mood_tech[pa] -= rnd_1_n(50, &g->seed) + 20;
                game_audience_tech_choice(au, game_str_au_whatech, thaf, that, num_techs, false);
                selected = ui_audience_ask4(au);
                /*65724*/
                if (selected != -1) {
                    tech_field_t gotf = thaf[selected];
                    uint8_t gott = that[selected];
                    int selected2 = selected;
                    int num_techs = MIN(thnum[selected2], 4);
                    game_audience_tech_choice(au, game_str_au_whatrad, thf[selected2], tht[selected2], num_techs, true);
                    selected = ui_audience_ask4(au);
                    if ((selected != -1) && (selected < num_techs)) {
                        g->evn.newtech[ph].num = 0;
                        g->evn.newtech[pa].num = 0;
                        game_tech_get_new(g, ph, gotf, gott, TECHSOURCE_TRADE, pa, PLAYER_NONE, false);
                        game_tech_get_new(g, pa, thf[selected2][selected], tht[selected2][selected], TECHSOURCE_TRADE, ph, PLAYER_NONE, false); /* WASBUG last ph was pa */
                        if (g->evn.newtech[ph].num != 0) {
                            if (IS_HUMAN(g, pa) && (g->evn.newtech[pa].num != 0)) {
                                game_tech_finish_new(au->g, pa);
                                game_tech_finish_new(au->g, ph);
                                ui_audience_newtech(au, PLAYER_NONE);
                            } else {
                                game_tech_finish_new(au->g, ph);
                                ui_audience_newtech(au, ph);
                            }
                        } else if (IS_HUMAN(g, pa) && (g->evn.newtech[pa].num != 0)) {
                            game_tech_finish_new(au->g, pa);
                            ui_audience_newtech(au, pa);
                        }
                    }
                }
            } else {
                game_audience_set_dtype(au, 75, 3);
            }
        } else {
            game_audience_set_dtype(au, 75, 3);
        }
    }
    game_diplo_annoy(g, ph, pa, 3);
    SETMIN(eh->mood_tech[pa], 50);
}

static void audience_menu_main(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    bool flag_done = false;
    bool condtbl[6];
    while (!flag_done) {
        int16_t selected;
        for (int i = 0; i < TBLLEN(condtbl); ++i) {
            condtbl[i] = true;
        }
        if (eh->treaty[pa] >= TREATY_WAR) {
            condtbl[1] = false;
            condtbl[2] = false;
            condtbl[4] = false;
        }
        {
            struct spy_esp_s s[1];
            s->spy = pa;
            s->target = ph;
            if ((eh->reserve_bc < 25) && (game_spy_esp_sub1(g, s, 0, 0) == 0)) {
                condtbl[3] = false;
            }
        }
        {
            int prod, want_trade, cur_trade = eh->trade_bc[pa];
            prod = MIN(eh->total_production_bc, g->eto[pa].total_production_bc) / 4;
            SETMIN(prod, 32000);
            want_trade = (prod / 25) * 25 - cur_trade;
            if (want_trade <= 0) {
                condtbl[1] = false;
            } else if (want_trade < 125) {
                au->num_bc = want_trade / 25;
                au->bctbl[0] = cur_trade + 25;
                au->bctbl[1] = cur_trade + 50;
                au->bctbl[2] = cur_trade + 75;
                au->bctbl[3] = cur_trade + 100;
            } else {
                au->num_bc = AUDIENCE_BC_MAX;
                au->bctbl[0] = want_trade / 5 + cur_trade;
                au->bctbl[1] = (((want_trade * 2) / 5) / 25) * 25 + cur_trade;
                au->bctbl[2] = (((want_trade * 3) / 5) / 25) * 25 + cur_trade;
                au->bctbl[3] = (((want_trade * 4) / 5) / 25) * 25 + cur_trade;
                au->bctbl[4] = want_trade + cur_trade;
            }
        }
        if (game_ai->aud_later(au)) {
            game_audience_set_dtype(au, 74, 3);
            break;
        }
        game_audience_choice(au, game_str_au_howmay, game_str_au_opts_main, condtbl, 6);
        selected = ui_audience_ask4(au);
        switch (selected) {
            case 0: /* Propose Treaty */
                audience_menu_treaty(au);
                break;
            case 1: /* Form Trade Agreement */
                audience_menu_trade(au);
                break;
            case 2: /* Threaten/Break Treaty and Trade */
                audience_menu_threat(au);
                break;
            case 3: /* Offer Tribute */
                audience_menu_tribute(au);
                break;
            case 4: /* Exchange Technology */
                audience_menu_tech(au);
                break;
            default:
            case -1:
            case 5: /* Good Bye */
                flag_done = true;
                break;
        }
    }
}

static void game_audience_do(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    if ((au->mode >= 0) && (au->mode <= 2)) {
        /* Greeting */
        g->evn.diplo_msg_subtype = -1;
        game_audience_get_diplo_msg(au);
        ui_audience_show1(au);
    }
    switch (au->mode) {
        case 2:  /* AI initiated audience. */
            if (game_audience_check_alliances(au)) {
                au->dtype = au->dtype_next;
                g->evn.diplo_msg_subtype = -1;
                game_audience_get_diplo_msg(au);
                switch (au->dtype) {
                    case 28:
                        game_audience_ai_offers_bounty(au);
                        break;
                    case 58:
                        game_audience_ai_pays_bounty(au);
                        break;
                    case 29:
                        game_audience_ai_offers_tech_trade(au);
                        break;
                    default:
                        game_audience_ai_offers_treaty(au);
                        break;
                }
            }
            /*607a9*/
            /* After offering a deal, the AI becomes less likely to accept or
             * propose similar deals in the near future, independent of the
             * player's response. This even happens when the player didn't see
             * the actual offer because they're allied with one of the AI's
             * enemies, so we cannot do it in the game_audience_ai_offers_*
             * functions. */
            game_diplo_annoy(g, ph, pa, 1);
            if ((au->dtype == 24) || (au->dtype == 25)) {
                eh->mood_treaty[pa] -= rnd_1_n(30, &g->seed) + 20;
            }
            if (au->dtype == 26) {
                eh->mood_trade[pa] -= rnd_1_n(30, &g->seed) + 20;
            }
            if (au->dtype == 30) {
                eh->mood_peace[pa] -= rnd_1_n(50, &g->seed) + 50;
            }
            if (au->dtype == 29) {
                eh->mood_tech[pa] -= rnd_1_n(30, &g->seed) + 20;
            }
            break;
        case 6:  /* Single message from the AI, no player response. */
            game_audience_set_dtype(au, au->dtype, 3);
            break;
        case 0:  /* Player initiated audience. */
            audience_menu_main(au);
            break;
        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */

void game_audience(struct game_s *g, player_id_t ph, player_id_t pa)
{
    struct audience_s au[1];
    empiretechorbit_t *eh = &(g->eto[ph]);
    au->g = g;
    game_diplo_limit_mood_treaty(g);
    game_audience_prepare(au, ph, pa);
    ui_audience_start(au);
    au->mode = 6;
    au->dtype = eh->diplo_type[pa];
    if ((au->dtype == 66) || (au->dtype == 1)) {
        /* FIXME BUG? no ui_audience_end call after _start */
        ui_audience_end(au);
        return;
    }
    if (au->dtype == 0) {
        game_audience_start_human(au);
    }
    g->evn.diplo_msg_subtype = -1;
    if (((au->dtype >= 24) && (au->dtype <= 30)) || (au->dtype == 58) || (au->dtype == 76)) {
        au->dtype_next = au->dtype;
        au->dtype = 22;
        au->mode = 2;
        au->pwar = eh->au_ally_attacker[pa];
    }
    game_audience_do(au);
    ui_audience_end(au);
    if (au->dtype == 32) {
        game_diplo_break_treaty(g, pa, ph);
    }
    if (game_num_aud_update_tech) {
        game_update_tech_util(g);
        game_update_within_range(g);
        game_update_visibility(g);
    }
}
