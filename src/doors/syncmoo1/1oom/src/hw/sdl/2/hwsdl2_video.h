#ifndef INC_1OOM_HWSDL2_VIDEO_H
#define INC_1OOM_HWSDL2_VIDEO_H

extern int hw_video_get_window_id(void);
extern void hw_video_set_visible(bool visible);
extern void hw_video_mouse_warp(int mx, int my);

extern bool hw_video_toggle_aspect(void);
extern bool hw_video_toggle_vsync(void);
extern bool hw_video_toggle_int_scaling(void);
extern bool hw_video_filter_next(void);

#endif
