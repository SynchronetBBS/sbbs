#include "config.h"

#include <stdio.h>

#include "uisound.h"
#include "hw.h"
#include "lbx.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "ui.h"
#include "uidefs.h"

/* -------------------------------------------------------------------------- */

void ui_sound_play_sfx(int sfxi)
{
    if ((sfxi < 0) || (sfxi >= NUM_SOUNDS)) {
        log_error("uisound: invalid sfx num %i\n", sfxi);
        hw_audio_sfx_stop();
        return;
    }

    hw_audio_sfx_play(sfxi);
}

void ui_sound_play_sfx_24(void)
{
    ui_sound_play_sfx(0x24);
}

void ui_sound_play_sfx_06(void)
{
    ui_sound_play_sfx(0x06);
}

void ui_sound_stop_sfx(void)
{
    hw_audio_sfx_stop();
}

void ui_sound_play_music(int musici)
{
    if ((musici < 0) || (musici >= NUM_MUSICS)) {
        log_error("uisound: invalid music num %i\n", musici);
        return;
    }
    hw_audio_music_stop();
    {
        uint8_t *n;
        uint32_t len;
        n = lbxfile_item_get_with_len(LBXFILE_MUSIC, musici, &len);
        if (ui_data.mus) {
            hw_audio_music_release(0);
            lbxfile_item_release(LBXFILE_MUSIC, ui_data.mus);
        }
        hw_audio_music_init(0, n, len);
        ui_data.mus = n;
        ui_data.music_i = musici;
    }
    hw_audio_music_play(0);
}

void ui_sound_stop_music(void)
{
    hw_audio_music_stop();
}
