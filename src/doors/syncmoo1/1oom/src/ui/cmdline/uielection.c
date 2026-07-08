#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_election.h"
#include "game_str.h"
#include "types.h"
#include "uidefs.h"
#include "uiinput.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

void ui_election_start(struct election_s *el)
{
    ui_switch_all(el->g);
}

void ui_election_show(struct election_s *el)
{
    struct game_s *g = el->g;
    printf("Election | ");
    if (el->flag_show_votes) {
        char vbuf[0x20];
        uint16_t n;
        n = el->got_votes[0];
        printf("%s %s, ", game_election_print_votes(n, vbuf, sizeof(vbuf)), g->emperor_names[el->candidate[0]]);
        n = el->got_votes[1];
        printf("%s %s, ", game_election_print_votes(n, vbuf, sizeof(vbuf)), g->emperor_names[el->candidate[1]]);
        n = el->total_votes;
        printf("%s %s | ", game_election_print_votes(n, vbuf, sizeof(vbuf)), game_str_el_total);
    }
    if (el->cur_i != PLAYER_NONE) {
        printf("(%s) | ", game_str_tbl_race[g->eto[el->tbl_ei[el->cur_i]].race]);
    }
    if (el->str) {
        puts(el->str);
    }
}

void ui_election_delay(struct election_s *el, int delay)
{
}

int ui_election_vote(struct election_s *el, int player_i)
{
    struct input_list_s il_vote[] = {
        { 1, "1", NULL, NULL },
        { 2, "2", NULL, NULL },
        { 0, "0", NULL, game_str_el_abs },
        { 0, NULL, NULL, NULL }
    };
    for (int i = 0; i < 2; ++i) {
        player_id_t pi;
        pi = el->candidate[i];
        il_vote[i].display = (pi == player_i) ? game_str_el_self : el->g->emperor_names[pi];
    }
    return ui_input_list(el->str, "> ", il_vote);
}

bool ui_election_accept(struct election_s *el, int player_i)
{
    int v;
    v = ui_input_list(el->str, "> ", il_yes_no);
    return (v == 1);
}

void ui_election_end(struct election_s *el)
{
    ui_switch_wait(el->g);
}
