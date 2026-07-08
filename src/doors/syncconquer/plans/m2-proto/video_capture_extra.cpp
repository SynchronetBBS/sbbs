// Prototype NEW_VIDEO_BUILD glue for the null video backend:
// - Get_Video_Mouse: fixed center position (no real input yet)
// - Video_Render_Frame: dump the visible page + palette as P6 PPM
//   every 30th render into the current working directory.
#include "gbuffer.h"
#include "palette.h"
#include <cstdio>

extern GraphicBufferClass VisiblePage;

void Get_Video_Mouse(int& x, int& y)
{
    x = 320;
    y = 200;
}

void Video_Render_Frame()
{
    static int frame;
    frame++;
    if (frame <= 5 || frame % 100 == 0) {
        fprintf(stderr, "capture: call %d\n", frame);
    }
    if (frame % 30 != 1) {
        return;
    }
    int w = VisiblePage.Get_Width();
    int h = VisiblePage.Get_Height();
    if (!VisiblePage.Lock()) {
        return;
    }
    unsigned char* px = (unsigned char*)VisiblePage.Get_Offset();
    if (px == nullptr || w <= 0 || h <= 0) {
        VisiblePage.Unlock();
        static bool warned;
        if (!warned) {
            fprintf(stderr, "capture: VisiblePage buffer null (w=%d h=%d)\n", w, h);
            warned = true;
        }
        return;
    }
    char name[64];
    snprintf(name, sizeof(name), "frame_%05d.ppm", frame);
    FILE* f = fopen(name, "wb");
    if (f == nullptr) {
        VisiblePage.Unlock();
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        unsigned char idx = px[i];
        fputc(CurrentPalette[idx * 3 + 0] << 2, f);
        fputc(CurrentPalette[idx * 3 + 1] << 2, f);
        fputc(CurrentPalette[idx * 3 + 2] << 2, f);
    }
    fclose(f);
    VisiblePage.Unlock();
    fprintf(stderr, "capture: wrote %s\n", name);
}

// Engine hands us its cursor sprite here; the door design uses the
// player's OS pointer instead, so this is deliberately a no-op.
void Set_Video_Cursor(void* cursor, int w, int h, int hotx, int hoty)
{
}
