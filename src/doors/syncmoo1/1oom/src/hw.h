#ifndef INC_1OOM_HW_H
#define INC_1OOM_HW_H

/* API to hw/ */

#include "cfg.h"
#include "options.h"
#include "types.h"

extern const char *idstr_hw;

extern int hw_early_init(void);
extern int hw_init(void);
extern void hw_shutdown(void);

extern const struct cmdline_options_s hw_cmdline_options[];
extern const struct cmdline_options_s hw_cmdline_options_extra[];

extern const struct cfg_items_s hw_cfg_items[];
extern const struct cfg_items_s hw_cfg_items_extra[];

extern void hw_opt_menu_make_page_video(void);

extern void hw_log_message(const char *msg);
extern void hw_log_warning(const char *msg);
extern void hw_log_error(const char *msg);

extern int hw_event_handle(void);

extern void hw_textinput_start(void);
extern void hw_textinput_stop(void);
extern bool hw_kbd_set_repeat(bool enabled);
extern void hw_mouse_set_xy(int mx, int my);

extern int hw_icon_set(const uint8_t *data, const uint8_t *pal, int w, int h);

extern int hw_video_init(int w, int h);
extern void hw_video_set_palette(const uint8_t *palette, int first, int num);
extern void hw_video_set_palette_color(int i, uint8_t r, uint8_t g, uint8_t b);
extern void hw_video_refresh_palette(void);
/* Return back buffer. */
extern uint8_t *hw_video_get_buf(void);
/* Return front buffer. */
extern uint8_t *hw_video_get_buf_front(void);
/* Draw the current back buffer and return new back buffer. */
extern uint8_t *hw_video_draw_buf(void);
/* Redraw the front buffer. */
extern void hw_video_redraw_front(void);
/* Copy front buffer to back buffer. */
extern void hw_video_copy_buf(void);
/* Copy back buffer to pointed buffer. */
extern void hw_video_copy_buf_out(uint8_t *buf);
extern void hw_video_copy_back_to_page2(void);
extern void hw_video_copy_back_from_page2(void);
extern void hw_video_copy_back_to_page3(void);
extern void hw_video_copy_back_from_page3(void);

extern int hw_audio_music_init(int mus_index, const uint8_t *data, uint32_t len);
extern void hw_audio_music_release(int mus_index);
extern void hw_audio_music_play(int mus_index);
extern void hw_audio_music_fadeout(void);
extern void hw_audio_music_stop(void);
extern bool hw_audio_music_volume(int volume/*0..128*/);

extern int hw_audio_sfx_batch_start(int sfx_index_max);
extern int hw_audio_sfx_batch_end(void);
extern int hw_audio_sfx_init(int sfx_index, const uint8_t *data, uint32_t len);
extern void hw_audio_sfx_release(int sfx_index);
extern void hw_audio_sfx_play(int sfx_index);
extern void hw_audio_sfx_stop(void);
extern bool hw_audio_sfx_volume(int volume/*0..128*/);

extern int64_t hw_get_time_us(void);

#endif
