#ifndef INC_1OOM_HWSDL_AUDIO_H
#define INC_1OOM_HWSDL_AUDIO_H

extern int hw_audio_init(void);
extern void hw_audio_shutdown_pre(void);
extern void hw_audio_shutdown(void);
extern int hw_audio_set_sdlmixer_sf(const char *path);
extern int hw_audio_check_process(void);

#endif
