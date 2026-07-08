#ifndef INC_1OOM_GAME_NEW_H
#define INC_1OOM_GAME_NEW_H

#include "game.h"

struct game_new_options_s {
    bool improved_galaxy_generator;
    bool nebulae;
    uint32_t galaxy_seed;
    galaxy_size_t galaxy_size;
    difficulty_t difficulty;
    uint32_t ai_id;
    uint32_t players;
    struct {
        uint32_t max_pop;
        planet_special_t special;
        uint32_t num_dist_checks;
        uint32_t num_ok_planet_checks;
        uint32_t num_scouts;
        uint32_t num_fighters;
        uint32_t num_colony_ships;
        bool armed_colony_ships;
    } homeworlds;
    struct {
        char playername[EMPEROR_NAME_LEN];
        char homename[PLANET_NAME_LEN];
        race_t race;
        banner_t banner;
        bool is_ai;
    } pdata[PLAYER_NUM];
};

#define GAME_NEW_OPTS_DEFAULT \
    { \
        0, 1, 0, GALAXY_SIZE_SMALL, DIFFICULTY_SIMPLE, 0, 2, \
        { \
            100, PLANET_SPECIAL_NORMAL, \
            2, 2, \
            2, 0, 1, false \
        }, \
        { \
            { "", "", RACE_HUMAN, BANNER_BLUE, false }, \
            { "", "", RACE_RANDOM, BANNER_RANDOM, true }, \
            { "", "", RACE_RANDOM, BANNER_RANDOM, true }, \
            { "", "", RACE_RANDOM, BANNER_RANDOM, true }, \
            { "", "", RACE_RANDOM, BANNER_RANDOM, true }, \
            { "", "", RACE_RANDOM, BANNER_RANDOM, true }  \
        } \
    }

#define GAME_NEW_OPTS_CHALLENGE_118835000 \
    { \
        1, 1, 118835000, GALAXY_SIZE_SMALL, DIFFICULTY_IMPOSSIBLE, 0, 6, \
        { \
            100, PLANET_SPECIAL_NORMAL, \
            6, 6, \
            2, 0, 1, false \
        }, \
        { \
            { "", "", RACE_HUMAN, BANNER_BLUE, false }, \
            { "", "", RACE_MRRSHAN, BANNER_RANDOM, true }, \
            { "", "", RACE_SAKKRA, BANNER_RANDOM, true }, \
            { "", "", RACE_PSILON, BANNER_RANDOM, true }, \
            { "", "", RACE_KLACKON, BANNER_RANDOM, true }, \
            { "", "", RACE_SILICOID, BANNER_RANDOM, true }  \
        } \
    }

struct game_aux_s;

extern int game_new(struct game_s *g, struct game_aux_s *gaux, struct game_new_options_s *opt);
extern int game_new_tutor(struct game_s *g, struct game_aux_s *gaux);

extern void game_new_generate_emperor_name(race_t race, char *buf, size_t bufsize);
extern void game_new_generate_home_name(race_t race, char *buf, size_t bufsize);

extern void game_new_generate_other_emperor_name(struct game_s *g, player_id_t player);

#endif
