static bool hw_kbd_check_hotkey(uint32_t key, uint32_t smod)
{
    if ((smod & KB_CTRL_FLAG) && (!(smod & KB_ALT_FLAG))) {
        if ((key >> 8) == KEY_ESC) {
            log_message("Alleg: got Ctrl-ESC, quitting now\n");
            exit(EXIT_SUCCESS);
#if 0
        } else if ((key >> 8) == KEY_F10) {
            hw_mouse_toggle_grab();
            return true;
#endif
        } else if ((key >> 8) == KEY_CLOSEBRACE) {
            if (smod & KB_SHIFT_FLAG) {
                hw_audio_music_volume(opt_music_volume + 4);
            } else {
                hw_audio_sfx_volume(opt_sfx_volume + 4);
            }
            return true;
        } else if ((key >> 8) == KEY_OPENBRACE) {
            if (smod & KB_SHIFT_FLAG) {
                hw_audio_music_volume(opt_music_volume - 4);
            } else {
                hw_audio_sfx_volume(opt_sfx_volume - 4);
            }
            return true;
        }
    }
    return false;
}

/* -------------------------------------------------------------------------- */

void hw_log_message(const char *msg)
{
    if (!hw_video_in_gfx) {
        fputs(msg, stdout);
    }
}

void hw_log_warning(const char *msg)
{
    if (!hw_video_in_gfx) {
        fputs(msg, stderr);
    }
}

void hw_log_error(const char *msg)
{
    if (!hw_video_in_gfx) {
        fputs(msg, stderr);
    }
}

int hw_icon_set(const uint8_t *data, const uint8_t *pal, int w, int h)
{
    return 0;
}
