bool hw_video_in_gfx = false;

void hw_video_refresh(int front)
{
    video.render(video.bufi ^ front);
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
