static int video_check_opt_screen_winwh(void)
{
    int minw = video.bufw;
    int minh = video.bufh;
    if ((hw_opt_screen_winw != 0) && (hw_opt_screen_winh != 0)) {
        if ((hw_opt_screen_winw < minw) || (hw_opt_screen_winh < minh)) {
            log_warning("ignoring too small configured resolution %ix%i < %ix%i\n", hw_opt_screen_winw, hw_opt_screen_winh, minw, minh);
        } else {
            return 0;
        }
    }
    return -1;
}

/* -------------------------------------------------------------------------- */

void hw_video_refresh(int front)
{
    if (SDL_MUSTLOCK(video.screen)) {
        if (SDL_LockSurface(video.screen) < 0) {
            return;
        }
    }

    video.render(video.bufi ^ front);

    if (SDL_MUSTLOCK(video.screen)) {
        SDL_UnlockSurface(video.screen);
    }

    video.update();
}

void hw_video_update(void)
{
    video.update();
}

void hw_video_set_palette(const uint8_t *pal, int first, int num)
{
    ui_palette_set(pal, first, num);
    video.setpal(pal, first, num);
}

void hw_video_set_palette_color(int i, uint8_t r, uint8_t g, uint8_t b)
{
    ui_palette_set_color(i, r, g, b);
}

void hw_video_refresh_palette(void)
{
    video.setpal(ui_palette, 0, 256);
}

uint8_t *hw_video_get_buf(void)
{
    return video.buf[video.bufi];
}

uint8_t *hw_video_get_buf_front(void)
{
    return video.buf[video.bufi ^ 1];
}

uint8_t *hw_video_draw_buf(void)
{
    hw_video_refresh(0);
    video.bufi ^= 1;
    return video.buf[video.bufi];
}

void hw_video_redraw_front(void)
{
    hw_video_refresh(1);
}

void hw_video_copy_buf(void)
{
    memcpy(video.buf[video.bufi], video.buf[video.bufi ^ 1], video.bufw * video.bufh);
}

void hw_video_copy_buf_out(uint8_t *buf)
{
    memcpy(buf, video.buf[video.bufi], video.bufw * video.bufh);
}

void hw_video_copy_back_to_page2(void)
{
    memcpy(video.buf[2], video.buf[video.bufi], video.bufw * video.bufh);
}

void hw_video_copy_back_from_page2(void)
{
    memcpy(video.buf[video.bufi], video.buf[2], video.bufw * video.bufh);
}

void hw_video_copy_back_to_page3(void)
{
    memcpy(video.buf[3], video.buf[video.bufi], video.bufw * video.bufh);
}

void hw_video_copy_back_from_page3(void)
{
    memcpy(video.buf[video.bufi], video.buf[3], video.bufw * video.bufh);
}
