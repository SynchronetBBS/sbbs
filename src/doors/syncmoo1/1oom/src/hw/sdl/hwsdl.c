static bool hw_kbd_check_hotkey(SDL1or2Key key, SDL1or2Mod smod, char c)
{
    if ((smod & KMOD_CTRL) && (!(smod & KMOD_ALT))) {
        if (key == SDLK_ESCAPE) {
            log_message("SDL: got Ctrl-ESC, quitting now\n");
            hw_audio_shutdown_pre();
            exit(EXIT_SUCCESS);
        } else if (key == SDLK_F10) {
            hw_mouse_toggle_grab();
            return true;
        } else if (key == SDLK_RIGHTBRACKET) {
            if (smod & KMOD_SHIFT) {
                hw_audio_music_volume(opt_music_volume + 4);
            } else {
                hw_audio_sfx_volume(opt_sfx_volume + 4);
            }
            return true;
        } else if (key == SDLK_LEFTBRACKET) {
            if (smod & KMOD_SHIFT) {
                hw_audio_music_volume(opt_music_volume - 4);
            } else {
                hw_audio_sfx_volume(opt_sfx_volume - 4);
            }
            return true;
        }
    } else if ((smod & KMOD_ALT) && (!(smod & KMOD_CTRL))) {
        if (key == SDLK_RETURN) {
            if (!hw_video_toggle_fullscreen()) {
                log_message("SDL: fs toggle failure, quitting now\n");
                exit(EXIT_FAILURE);
            }
            return true;
        } else if (key == SDLK_LEFTBRACKET) {
            hw_video_shrink();
        } else if (key == SDLK_RIGHTBRACKET) {
            hw_video_enlarge();
        }
    }
    return false;
}

/* -------------------------------------------------------------------------- */

void hw_log_message(const char *msg)
{
    fputs(msg, stdout);
}

void hw_log_warning(const char *msg)
{
    fputs(msg, stderr);
}

void hw_log_error(const char *msg)
{
    fputs(msg, stderr);
}

/* -------------------------------------------------------------------------- */

int64_t hw_get_time_us(void)
{
    return SDL_GetTicks() * 1000;
}
