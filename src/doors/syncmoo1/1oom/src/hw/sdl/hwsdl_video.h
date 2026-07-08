#ifndef INC_1OOM_HWSDL_VIDEO_H
#define INC_1OOM_HWSDL_VIDEO_H

extern void hw_video_shutdown(void);
extern void hw_video_update(void);
extern void hw_video_refresh(int front);
extern int hw_video_resize(int w, int h);
extern void hw_video_shrink(void);
extern void hw_video_enlarge(void);
extern bool hw_video_toggle_borderless(void);
extern bool hw_video_toggle_fullscreen(void);
extern bool hw_video_update_aspect(void);
extern void hw_video_input_grab(bool grab);

#endif
