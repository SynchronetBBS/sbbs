#include "config.h"

#include <stdio.h>
#include <string.h>

#include "SDL.h"
#ifdef HAVE_SDL1GL
#include "SDL_opengl.h"
#endif

#include "hw.h"
#include "hwsdl_video.h"
#include "hwsdl_mouse.h"
#include "hwsdl_opt.h"
#include "lib.h"
#include "log.h"
#include "palette.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

/* double buffering + 2 aux buffers */
#define NUM_VIDEOBUF    4

static struct sdl_video_s {
    SDL_Surface *screen;
#ifdef HAVE_SDL1GL
    SDL_Surface *hwrenderbuf;
#endif
    void (*render)(int bufi);
    void (*update)(void);
    void (*setpal)(const uint8_t *pal, int first, int num);

    /* buffers used by UI */
    uint8_t *buf[NUM_VIDEOBUF];
    int bufw;
    int bufh;
    int bufi;
#ifdef HAVE_SDL1GL
    /* precalculated palette for used >8 bpp */
    union {
        uint8_t p8[256 * 3];
        uint32_t p32[256];
    } ppal;
    int bpp;
    int rgb_mode;
    int rsize, gsize, bsize;
    int actual_w, actual_h;
#endif
    /* "best" video mode reported by SDL */
    struct {
        int w, h, bpp;
    } bestmode;
} video = { 0 };

/* -------------------------------------------------------------------------- */

#include "hwsdl_video.c"

/* -------------------------------------------------------------------------- */

static void video_render_8bpp(int bufi)
{
    int pitch = video.screen->pitch;
    Uint8 *p = (Uint8 *)video.screen->pixels;
    uint8_t *q = video.buf[bufi];
    for (int y = 0; y < video.bufh; ++y) {
        memcpy(p, q, video.bufw);
        p += pitch;
        q += video.bufw;
    }
}

static void video_update_8bpp(void)
{
    SDL_UpdateRect(video.screen, 0, 0, video.screen->w, video.screen->h);
}

static void video_setpal_8bpp(const uint8_t *pal, int first, int num)
{
    SDL_Color color[256];
    for (int i = first; i < (first + num); ++i) {
        color[i].r = (*pal << 2) | ((*pal >> 4) & 3);
        pal++;
        color[i].g = (*pal << 2) | ((*pal >> 4) & 3);
        pal++;
        color[i].b = (*pal << 2) | ((*pal >> 4) & 3);
        pal++;
    }
    SDL_SetColors(video.screen, &color[first], first, num);
    video_update_8bpp();
}

#ifdef HAVE_SDL1GL

static void video_render_gl_24bpp(int bufi)
{
    int pitch_skip = (video.bufw * 3) - video.hwrenderbuf->pitch;

    uint8_t *p = (uint8_t *)video.hwrenderbuf->pixels;
    uint8_t *q = video.buf[bufi];
    for (int y = 0; y < video.bufh; ++y) {
        for (int x = 0; x < video.bufw; ++x) {
            uint8_t *c;
            c = &(video.ppal.p8[*q++ * 3]);
            *p++ = *c++;
            *p++ = *c++;
            *p++ = *c++;
        }
        p += pitch_skip;
    }
}

static void video_setpal_gl_24bpp(const uint8_t *pal, int f, int num)
{
    for (int i = 0; i < num; ++i) {
        for (int j = 0; j < 3; ++j) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            video.ppal.p8[(f + i) * 3 + 0] = (pal[i * 3 + 2] << 2) | ((pal[i * 3 + 2] >> 4) & 3);
            video.ppal.p8[(f + i) * 3 + 1] = (pal[i * 3 + 1] << 2) | ((pal[i * 3 + 1] >> 4) & 3);
            video.ppal.p8[(f + i) * 3 + 2] = (pal[i * 3 + 0] << 2) | ((pal[i * 3 + 0] >> 4) & 3);
#else
            video.ppal.p8[(f + i) * 3 + 0] = (pal[i * 3 + 0] << 2) | ((pal[i * 3 + 0] >> 4) & 3);
            video.ppal.p8[(f + i) * 3 + 1] = (pal[i * 3 + 1] << 2) | ((pal[i * 3 + 1] >> 4) & 3);
            video.ppal.p8[(f + i) * 3 + 2] = (pal[i * 3 + 2] << 2) | ((pal[i * 3 + 2] >> 4) & 3);
#endif
        }
    }
    hw_video_refresh(1);
}

static void video_render_gl_32bpp(int bufi)
{
    int pitch_skip = ((video.bufw * sizeof(Uint32)) - video.hwrenderbuf->pitch) / sizeof(Uint32);

    Uint32 *p = (Uint32 *)video.hwrenderbuf->pixels;
    uint8_t *q = video.buf[bufi];
    for (int y = 0; y < video.bufh; ++y) {
        for (int x = 0; x < video.bufw; ++x) {
            *p++ = video.ppal.p32[*q++];
        }
        p += pitch_skip;
    }
}

static void video_update_gl(void)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    {
        int tbl[] = { GL_NEAREST, GL_LINEAR };
        int filter = tbl[hw_opt_gl_filter];

        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, filter);
    }
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, video.rgb_mode, video.bufw, video.bufh, 0, video.rgb_mode, GL_UNSIGNED_BYTE, video.hwrenderbuf->pixels);

    glBegin(GL_QUADS);

    /* Lower Right Of Texture */
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, 1.0f);

    /* Upper Right Of Texture */
    glTexCoord2f(0.0f, (float)(video.bufh));
    glVertex2f(-1.0f, -1.0f);

    /* Upper Left Of Texture */
    glTexCoord2f((float)(video.bufw), (float)(video.bufh));
    glVertex2f(1.0f, -1.0f);

    /* Lower Left Of Texture */
    glTexCoord2f((float)(video.bufw), 0.0f);
    glVertex2f(1.0f, 1.0f);

    glEnd();

    SDL_GL_SwapBuffers();
}

static void video_setpal_gl_32bpp(const uint8_t *pal, int f, int num)
{
    for (int i = 0; i < num; ++i) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        video.ppal.p32[f + i] = (palette_6bit_to_8bit(pal[i * 3 + 0]) << 24)
                              | (palette_6bit_to_8bit(pal[i * 3 + 1]) << 16)
                              | (palette_6bit_to_8bit(pal[i * 3 + 2]) << 8)
                              ;
#else
        video.ppal.p32[f + i] = (palette_6bit_to_8bit(pal[i * 3 + 0]) << 0)
                              | (palette_6bit_to_8bit(pal[i * 3 + 1]) << 8)
                              | (palette_6bit_to_8bit(pal[i * 3 + 2]) << 16)
                              ;
#endif
    }
    hw_video_refresh(1);
}

#endif /* HAVE_SDL1GL */

/* -------------------------------------------------------------------------- */

#ifdef HAVE_SDL1GL
static void set_viewport(unsigned int src_w, unsigned int src_h, unsigned int dest_w, unsigned int dest_h)
{
    int dest_x = 0, dest_y = 0;

    if (hw_opt_aspect != 0) {
        double aspect = ((double)(hw_opt_aspect)) / 1000000.;
        if (dest_w * src_h < src_w * aspect * dest_h) {
            dest_y = dest_h;
            dest_h = (unsigned int)((dest_w * src_h + src_w * aspect / 2) / (src_w * aspect));
            dest_y = (dest_y - dest_h) / 2;
        } else {
            dest_x = dest_w;
            dest_w = (unsigned int)((dest_h * src_w * aspect + src_h / 2) / src_h);
            dest_x = (dest_x - dest_w) / 2;
        }
    }
    glViewport(dest_x, dest_y, dest_w, dest_h);
}
#endif /* HAVE_SDL1GL */

/* -------------------------------------------------------------------------- */

static int video_sw_set(int w, int h)
{
    int flags;
    flags = SDL_SWSURFACE | SDL_DOUBLEBUF;
    if (hw_opt_borderless) {
        flags |= SDL_NOFRAME;
    }
    if (hw_opt_fullscreen) {
        flags |= SDL_FULLSCREEN;
    }
    log_message("SDL_SetVideoMode(%i, %i, %i, 0x%x)\n", w, h, 8, flags);
    video.screen = SDL_SetVideoMode(w, h, 8, flags);
    if (!video.screen) {
        log_error("SDL_SetVideoMode failed: %s\n", SDL_GetError());
        return -1;
    }
    hw_mouse_set_scale(w, h);
    return 0;
}

/* -------------------------------------------------------------------------- */

int hw_video_resize(int w, int h)
{
#ifdef HAVE_SDL1GL
    unsigned int actual_w, actual_h;
    int flags;

    log_message("SDL: resize %ix%i (%s)\n", w, h, hw_opt_fullscreen ? "full" : "window");

    if (hw_opt_force_sw) {
        return 0;
    }

    if ((w < 0) || (h < 0)) {
        w = video.bufw;
        h = video.bufh;
    }

    flags = SDL_OPENGL | SDL_RESIZABLE;

    if (hw_opt_borderless) {
        flags |= SDL_NOFRAME;
    }

    if (hw_opt_fullscreen) {
        flags |= SDL_FULLSCREEN;
        if (hw_opt_screen_fsw && hw_opt_screen_fsh) {
            actual_w = hw_opt_screen_fsw;
            actual_h = hw_opt_screen_fsh;
        } else {
            actual_w = video.bestmode.w;
            actual_h = video.bestmode.h;
        }
    } else {
        hw_opt_screen_winw = actual_w = w;
        hw_opt_screen_winh = actual_h = h;
    }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, video.rsize);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, video.gsize);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, video.bsize);

    log_message("SDL_SetVideoMode(%i, %i, %i, 0x%x)\n", actual_w, actual_h, video.bpp, flags);
    video.screen = SDL_SetVideoMode(actual_w, actual_h, video.bpp, flags);
    if (!video.screen) {
        log_error("SDL_SetVideoMode failed!\n");
        log_error("Resize %s failed. Run with -%s or set width/height with -%sw W -%sh H.\n",
                hw_opt_fullscreen ? "fullscreen" : "window",
                hw_opt_fullscreen ? "window" : "fs",
                hw_opt_fullscreen ? "fs" : "win",
                hw_opt_fullscreen ? "fs" : "win"
                );
        goto fail;
    }
    set_viewport(video.bufw, video.bufh, actual_w, actual_h);
    if (hw_opt_fullscreen) {
        hw_mouse_grab();
    }
    hw_mouse_set_scale(actual_w, actual_h);
    video.actual_w = actual_w;
    video.actual_h = actual_h;
    return 0;
    fail:
    return -1;
#else
    return 0;
#endif
}

void hw_video_shrink(void)
{

}

void hw_video_enlarge(void)
{

}

bool hw_video_toggle_fullscreen(void)
{
    int (*func)(int, int) = video_sw_set;
#ifdef HAVE_SDL1GL
    if (!hw_opt_force_sw) {
        func = hw_video_resize;
    }
#endif /* HAVE_SDL1GL */
    hw_opt_fullscreen = !hw_opt_fullscreen;
    if (func(hw_opt_screen_winw, hw_opt_screen_winh)) {
        hw_opt_fullscreen = !hw_opt_fullscreen; /* restore the setting for the config file */
        return false;
    }
#ifdef HAVE_SDL1GL
    if (hw_opt_force_sw)
#endif /* HAVE_SDL1GL */
    {
        hw_video_refresh_palette();
    }
    return true;
}

#ifdef HAVE_SDL1GL
bool hw_video_update_aspect(void)
{
    return (hw_video_resize(video.actual_w, video.actual_h) == 0);
}
#endif

int hw_video_init(int w, int h)
{
    hw_mouse_set_limits(w, h);
    video.bufw = w;
    video.bufh = h;
    {
        const SDL_VideoInfo *p = SDL_GetVideoInfo();
        video.bestmode.w = p->current_w;
        video.bestmode.h = p->current_h;
        video.bestmode.bpp = p->vfmt->BitsPerPixel;
        log_message("SDL_GetVideoInfo -> %ix%i %ibpp\n", video.bestmode.w, video.bestmode.h, video.bestmode.bpp);
    }

#ifdef HAVE_SDL1GL
    if (hw_opt_force_sw)
#endif
    {
        video.render = video_render_8bpp;
        video.update = video_update_8bpp;
        video.setpal = video_setpal_8bpp;
        if (video_sw_set(w, h)) {
            return -1;
        }
    }
#ifdef HAVE_SDL1GL
    else {
        int texturebpp;
        Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        rmask = 0xff000000; gmask = 0x00ff0000; bmask = 0x0000ff00; amask = 0x000000ff;
#else
        rmask = 0x000000ff; gmask = 0x0000ff00; bmask = 0x00ff0000; amask = 0xff000000;
#endif
        if (hw_opt_bpp != 0) {
            video.bpp = hw_opt_bpp;
        } else {
            video.bpp = video.bestmode.bpp;
        }
        video.rsize = 8; video.gsize = 8; video.bsize = 8;
        video.render = video_render_gl_24bpp;
        video.setpal = video_setpal_gl_24bpp;
        video.rgb_mode = GL_RGB;
        texturebpp = 24;
        switch (video.bpp) {
            case 15:
                video.rsize = 5; video.gsize = 5; video.bsize = 5;
                amask = 0;
                break;
            case 16:
                video.rsize = 5; video.gsize = 6; video.bsize = 5;
                amask = 0;
                break;
            case 24:
                amask = 0;
                break;
            case 32:
                video.render = video_render_gl_32bpp;
                video.setpal = video_setpal_gl_32bpp;
                video.rgb_mode = GL_RGBA;
                texturebpp = 32;
                break;
            default:
                log_error("SDL: %ibpp is unsupported, choose another with -bpp N or use -nogl\n", video.bpp);
                return -1;
        }
        video.update = video_update_gl;
        log_message("SDL_CreateRGBSurface(SDL_SWSURFACE, %i, %i, %i, 0x%x, 0x%x, 0x%x, 0x%x)\n", w, h, texturebpp, rmask, gmask, bmask, amask);
        video.hwrenderbuf = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, texturebpp, rmask, gmask, bmask, amask);
        if (!video.hwrenderbuf) {
            log_error("SDL_CreateRGBSurface failed!\n");
            log_error("Run with -nogl to disable OpenGL.\n");
            return -1;
        }
        if ((video.hwrenderbuf->pitch % sizeof(Uint32)) != 0) {
            log_warning("SDL renderbuf pitch mod %i == %i\n", sizeof(Uint32), video.hwrenderbuf->pitch);
        }
        if (video_check_opt_screen_winwh() == 0) {
            w = hw_opt_screen_winw;
            h = hw_opt_screen_winh;
        } else {
            int scale = (video.bestmode.w - 50/*window borders*/) / video.bufw + 1;
            if (scale > 1) {
                do {
                    --scale;
                    h = video.bufh * scale;
                    if (hw_opt_aspect != 0) {
                        h = (int)(((double)(h) * 1000000.) / ((double)(hw_opt_aspect)));
                    }
                } while ((scale > 1) && ((h + 50/*space for window borders, taskbar etc*/) > video.bestmode.h));
                w = video.bufw * scale;
            }
        }
        if (hw_video_resize(w, h)) {
            return -1;
        }
    }
#endif

    video.buf[0] = lib_malloc(video.bufw * video.bufh * NUM_VIDEOBUF);
    for (int i = 1; i < NUM_VIDEOBUF; ++i) {
        video.buf[i] = video.buf[0] + video.bufw * video.bufh * i;
    }
    video.bufi = 0;
    return 0;
}

void hw_video_shutdown(void)
{
    if (video.screen) {
        SDL_FreeSurface(video.screen);
        video.screen = NULL;
    }
#ifdef HAVE_SDL1GL
    if (video.hwrenderbuf) {
        SDL_FreeSurface(video.hwrenderbuf);
        video.hwrenderbuf = NULL;
    }
#endif
    lib_free(video.buf[0]);
    for (int i = 0; i < NUM_VIDEOBUF; ++i) {
        video.buf[i] = NULL;
    }
}

void hw_video_input_grab(bool grab)
{
    SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

int hw_icon_set(const uint8_t *data, const uint8_t *pal, int w, int h)
{
    SDL_Color color[256];
    SDL_Surface *icon;
    Uint8 *mask = 0;
    Uint8 *p;
    uint8_t maxb = 0;
    uint32_t mi = 0;
    icon = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 8, 0, 0, 0, 0);
    if (!icon) {
        log_error("Icon: SDL_CreateRGBSurface failed!\n");
        return -1;
    }
    mask = lib_malloc((w * h + 7) / 8);
    p = (Uint8 *)icon->pixels;
    mi = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t b;
            b = *data++;
            p[x] = b;
            if (b) {
                mask[mi >> 3] |= (1 << (7 - (mi & 7)));
                if (b > maxb) {
                    maxb = b;
                }
            }
            ++mi;
        }
        p += icon->pitch;
    }
    for (int i = 0; i < 256; ++i) {
        color[i].r = *pal++ << 2;
        color[i].g = *pal++ << 2;
        color[i].b = *pal++ << 2;
    }
    SDL_SetColors(icon, color, 0, maxb + 1);
    SDL_WM_SetIcon(icon, mask);
    SDL_FreeSurface(icon);
    icon = NULL;
    lib_free(mask);
    mask = NULL;
    return 0;
}
