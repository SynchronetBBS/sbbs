#ifndef INC_1OOM_HWSDL_VIDEO_H
#define INC_1OOM_HWSDL_VIDEO_H

extern bool hw_video_in_gfx;

extern void hw_video_shutdown(void);
extern void hw_video_update(void);
extern void hw_video_refresh(int front);
extern void hw_video_input_grab(bool grab);

#endif
