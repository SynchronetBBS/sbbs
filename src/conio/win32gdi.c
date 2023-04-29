#include <windows.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define BITMAP_CIOLIB_DRIVER
#include "win32gdi.h"
#include "bitmap_con.h"
#include "win32cio.h"
#include "scale.h"

static HWND win;
static HANDLE rch;
static HANDLE wch;
static bool maximized = false;
static uint8_t *title;
static uint16_t winxpos, winypos;
static const DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

#define WM_USER_INVALIDATE WM_USER
#define WM_USER_SETSIZE (WM_USER + 1)
#define WM_USER_SETPOS (WM_USER + 2)

#define LCS_WINDOWS_COLOR_SPACE 0x57696E20

// Used to create a DI bitmap from bitmap_con data
static BITMAPV5HEADER b5hdr = {
	.bV5Size = sizeof(BITMAPV5HEADER),
	.bV5Width = 640,
	.bV5Height = -400,
	.bV5Planes = 1,
	.bV5BitCount = 32,
	.bV5Compression = BI_BITFIELDS,
	.bV5SizeImage = 640 * 400 * 4,
	.bV5RedMask = 0x00ff0000,
	.bV5GreenMask = 0x0000ff00,
	.bV5BlueMask = 0x000000ff,
	.bV5CSType = LCS_WINDOWS_COLOR_SPACE,
	.bV5Intent = LCS_GM_BUSINESS,
};

extern HINSTANCE WinMainHInst;
static struct rectlist *update_list = NULL;
static struct rectlist *update_list_tail = NULL;
static struct rectlist *last = NULL;
static struct rectlist *next = NULL;
static pthread_mutex_t gdi_headlock;
static pthread_mutex_t winpos_lock;
static pthread_mutex_t rect_lock;
static bool ciolib_scaling = false;

// Internal implementation

static struct rectlist *
get_rect(void)
{
	struct rectlist *ret;

	pthread_mutex_lock(&rect_lock);
	if (next != NULL) {
		if (last != NULL)
			bitmap_drv_free_rect(last);
		last = next;
		ret = next;
		next = NULL;
	}
	else
		ret = last;
	pthread_mutex_unlock(&rect_lock);
	return ret;
}

static void
next_rect(struct rectlist *rect)
{
	pthread_mutex_lock(&rect_lock);
	if (next != NULL)
		bitmap_drv_free_rect(next);
	next = rect;
	pthread_mutex_unlock(&rect_lock);
}

static void
add_key(uint16_t key)
{
	uint8_t buf[2];
	uint8_t *bp = buf;
	DWORD added;
	DWORD remain;

	if (key < 256) {
		buf[0] = key;
		remain = 1;
	}
	else {
		buf[0] = key & 0xff;
		buf[1] = key >> 8;
		remain = 2;
	}
	do {
		WriteFile(wch, bp, remain, &added, NULL);
		remain -= added;
		bp += added;
	} while (remain > 0);
}

static uint32_t
sp_to_codepoint(uint16_t high, uint16_t low)
{
	return (high - 0xd800) * 0x400 + (low - 0xdc00) + 0x10000;
}

// vstatlock must be held
static void
set_ciolib_scaling(void)
{
	int w, h;
	int fw, fh;

	w = vstat.winwidth;
	h = vstat.winheight;
	aspect_fix_low(&w, &h, vstat.aspect_width, vstat.aspect_height);
	fw = w;
	fh = h;
	aspect_reverse(&w, &h, vstat.scrnwidth, vstat.scrnheight, vstat.aspect_width, vstat.aspect_height);
	if ((fw == w) || (fh == h)) {
		if (fw > vstat.winwidth || fh > vstat.winheight)
			ciolib_scaling = false;
		else
			ciolib_scaling = true;
	}
	else
		ciolib_scaling = false;
}

static bool
UnadjustWindowSize(int *w, int *h)
{
	RECT r = {0};
	bool ret;

	ret = AdjustWindowRect(&r, style, FALSE);
	if (ret) {
		w += r.left - r.right;
		h += r.top - r.bottom;
	}
	return ret;
}

static LRESULT
gdi_handle_wm_size(WPARAM wParam, LPARAM lParam)
{
	RECT r;
	int w, h;

	switch (wParam) {
		case SIZE_MAXIMIZED:
			maximized = true;
			break;
		case SIZE_MINIMIZED:
		case SIZE_RESTORED:
			maximized = false;
			break;
	}
	if (wParam == SIZE_MINIMIZED)
		return 0;
	w = lParam & 0xffff;
	h = (lParam >> 16) & 0xffff;
	UnadjustWindowSize(&w, &h);
	pthread_mutex_lock(&vstatlock);
	vstat.winwidth = w;
	vstat.winheight = h;
	set_ciolib_scaling();
	pthread_mutex_unlock(&vstatlock);

	return 0;
}

static LRESULT
gdi_handle_wm_paint(HWND hwnd)
{
	static HDC memDC = NULL;

	PAINTSTRUCT ps;
	struct rectlist *list;
	HBITMAP di;
	HDC winDC;
	int w,h;
	int aw,ah;
	int sw,sh;
	int xscale, yscale;
	struct graphics_buffer *gb;
	void *data;

	list = get_rect();
	if (list == NULL)
		return 0;
	pthread_mutex_lock(&vstatlock);
	w = vstat.winwidth;
	h = vstat.winheight;
	aw = vstat.aspect_width;
	ah = vstat.aspect_height;
	sw = vstat.scrnwidth;
	sh = vstat.scrnheight;
	pthread_mutex_unlock(&vstatlock);
	if (ciolib_scaling) {
		calc_scaling_factors(&xscale, &yscale, w, h, aw, ah, sw, sh);
		gb = do_scale(list, xscale, yscale, aw, ah);
		b5hdr.bV5Width = gb->w;
		b5hdr.bV5Height = -gb->h;
		b5hdr.bV5SizeImage = gb->w * gb->h * 4;
		data = gb->data;
	}
	else {
		b5hdr.bV5Width = list->rect.width;
		b5hdr.bV5Height = -list->rect.height;
		b5hdr.bV5SizeImage = list->rect.width * list->rect.height * 4;
		data = list->data;
	}
	winDC = BeginPaint(hwnd, &ps);
	if (memDC == NULL)
		memDC = CreateCompatibleDC(winDC);
	// Scale...
	di = CreateDIBitmap(winDC, (BITMAPINFOHEADER *)&b5hdr, CBM_INIT, data, (BITMAPINFO *)&b5hdr, 0/*DIB_RGB_COLORS*/);
	di = SelectObject(memDC, di);
	if (ciolib_scaling) {
		BitBlt(winDC, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
	}
	else {
		StretchBlt(winDC, 0, 0, w, h, memDC, 0, 0, list->rect.width, list->rect.height, SRCCOPY);
	}
	EndPaint(hwnd, &ps);
	di = SelectObject(memDC, di);
	DeleteObject(di);
	if (ciolib_scaling) {
		release_buffer(gb);
	}
	return 0;
}

static LRESULT
gdi_handle_wm_char(WPARAM wParam, LPARAM lParam)
{
	static WPARAM highpair;
	uint32_t cp;
	uint8_t ch;
	uint16_t repeat;
	uint16_t i;
	bool alt;

	repeat = lParam & 0xffff;
	alt = lParam & (1 << 29);
	if (IS_HIGH_SURROGATE(wParam)) {
		highpair = wParam;
		return 0;
	}
	else if (IS_LOW_SURROGATE(wParam)) {
		cp = sp_to_codepoint(highpair, wParam);
	}
	else {
		cp = wParam;
	}
	// Translate from unicode to codepage...
	ch = cpchar_from_unicode_cpoint(getcodepage(), cp, 0);
	if (ch) {
		for (i = 0; i < repeat; i++)
			add_key(ch);
	}
	return 0;
}

static LRESULT CALLBACK
gdi_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_PAINT:
			return gdi_handle_wm_paint(hwnd);
		case WM_CHAR:
			return gdi_handle_wm_char(wParam, lParam);
		case WM_MOVE:
			pthread_mutex_lock(&winpos_lock);
			winxpos = lParam & 0xffff;
			winypos = (lParam >> 16) & 0xffff;
			pthread_mutex_unlock(&winpos_lock);
			return 0;
		case WM_SIZE:
			return gdi_handle_wm_size(wParam, lParam);
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void
gdi_snap(bool grow)
{
	bool wc;
	int w, h;

	if (maximized)
		return;
	pthread_mutex_lock(&vstatlock);
	w = vstat.winwidth;
	h = vstat.winheight;
	aspect_fix(&w, &h, vstat.aspect_width, vstat.aspect_height);
	if (vstat.aspect_width == 0 || vstat.aspect_height == 0)
		wc = true;
	else
		wc = lround((double)(h * vstat.aspect_width) / vstat.aspect_height * vstat.scrnwidth / vstat.scrnheight) > w;
	if (grow) {
		if (wc)
			w = (w - w % vstat.scrnwidth) + vstat.scrnwidth;
		else
			h = (h - h % vstat.scrnheight) + vstat.scrnheight;
	}
	else {
		if (wc) {
			if (w % (vstat.scrnwidth)) {
				w = w - w % vstat.scrnwidth;
			}
			else {
				w -= vstat.scrnwidth;
				if (w < vstat.scrnwidth)
					w = vstat.scrnwidth;
			}
		}
		else {
			if (h % (vstat.scrnheight)) {
				h = h - h % vstat.scrnheight;
			}
			else {
				h -= vstat.scrnheight;
				if (h < vstat.scrnheight)
					h = vstat.scrnheight;
			}
		}
	}
	if (wc)
		h = INT_MAX;
	else
		w = INT_MAX;
	aspect_fix(&w, &h, vstat.aspect_width, vstat.aspect_height);
	if (w > 16384 || h > 16384)
		gdi_beep();
	else {
		vstat.winwidth = w;
		vstat.winheight = h;
		set_ciolib_scaling();
		gdi_setwinsize(w, h);
	}
	pthread_mutex_unlock(&vstatlock);
}

#define WMOD_CTRL     1
#define WMOD_LCTRL    2
#define WMOD_RCTRL    4
#define WMOD_SHIFT    8
#define WMOD_LSHIFT  16
#define WMOD_RSHIFT  32
bool
magic_message(MSG msg)
{
	static uint8_t mods = 0;
	size_t i;
	uint8_t set = 0;
	int *hack;
	RECT r;

	switch(msg.message) {
		// User messages
		case WM_USER_INVALIDATE:
			InvalidateRect(win, NULL, FALSE);
			return true;
		case WM_USER_SETSIZE:
			pthread_mutex_lock(&vstatlock);
			// Now make the inside of the window the size we want (sigh)
			r.left = r.top = 0;
			r.right = vstat.winwidth = msg.wParam;
			r.bottom = vstat.winheight = msg.lParam;
			set_ciolib_scaling();
			pthread_mutex_unlock(&vstatlock);
			AdjustWindowRect(&r, style, FALSE);
			SetWindowPos(win, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
			return true;
		case WM_USER_SETPOS:
			SetWindowPos(win, NULL, msg.wParam, msg.lParam, 0, 0, SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_NOZORDER);
			return true;

		// Keyboard stuff
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			switch (msg.wParam) {
				case VK_CONTROL:
					set = WMOD_CTRL;
					break;
				case VK_LCONTROL:
					set = WMOD_LCTRL;
					break;
				case VK_RCONTROL:
					set = WMOD_RCTRL;
					break;
				case VK_SHIFT:
					set = WMOD_SHIFT;
					break;
				case VK_LSHIFT:
					set = WMOD_LSHIFT;
					break;
				case VK_RSHIFT:
					set = WMOD_RSHIFT;
					break;
			}
			if (set) {
				if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
					mods |= set;
				else
					mods &= ~set;
				return false;
			}

			if (msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP)
				return false;

			for (i = 0; keyval[i].VirtualKeyCode != 0; i++) {
				if (keyval[i].VirtualKeyCode == msg.wParam) {
					if (msg.lParam & (1 << 29)) {
						if (keyval[i].ALT > 255) {
							if (keyval[i].VirtualKeyCode == VK_LEFT) {
								gdi_snap(false);
							}
							else if (keyval[i].VirtualKeyCode == VK_RIGHT) {
								gdi_snap(true);
							}
							add_key(keyval[i].ALT);
							return true;
						}
					}
					else if (mods & (WMOD_CTRL | WMOD_LCTRL | WMOD_RCTRL)) {
						if (keyval[i].CTRL > 255) {
							add_key(keyval[i].CTRL);
							return true;
						}
					}
					else if (mods & (WMOD_SHIFT | WMOD_LSHIFT | WMOD_RSHIFT)) {
						if (keyval[i].Shift > 255) {
							add_key(keyval[i].Shift);
							return true;
						}
					}
					else {
						if (keyval[i].Key > 255) {
							add_key(keyval[i].Key);
							return true;
						}
					}
					break;
				}
			}
			break;
	}

	return false;
}

static void
gdi_thread(void *arg)
{
	WNDCLASSW wc = {0};
	MSG  msg;
	RECT r;

	SetThreadName("GDI Events");

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = gdi_WndProc;
	wc.hInstance     = WinMainHInst;
	//wc.hIcon         = ICON;        // TODO: Icon from ciolib.rc
	wc.hCursor       = LoadCursor(0, IDC_IBEAM);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = L"SyncConsole";

	RegisterClassW(&wc);
	pthread_mutex_lock(&vstatlock);
	// Now make the inside of the window the size we want (sigh)
	r.left = r.top = 0;
	r.right = vstat.winwidth;
	r.bottom = vstat.winheight;
	pthread_mutex_unlock(&vstatlock);
	AdjustWindowRect(&r, style, FALSE);
	win = CreateWindowW(wc.lpszClassName, L"SyncConsole", style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!magic_message(msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	// This may not be necessary...
	DestroyWindow(win);
	UnregisterClassW(wc.lpszClassName, NULL);
	add_key(CIO_KEY_QUIT);
}

// Public API

int
gdi_kbhit(void)
{
	DWORD avail = 0;

	PeekNamedPipe(rch, NULL, 0, NULL, &avail, NULL);
	return (avail > 0);
}

int
gdi_getch(void)
{
	uint8_t ch;
	DWORD got;

	ReadFile(rch, &ch, 1, &got, NULL);
	if (got == 0)
		return 0;
	return ch;
}

void
gdi_beep(void)
{
	MessageBeep(MB_ICONWARNING);
}

void
gdi_textmode(int mode)
{
	int oldcols;
	int scaling = 1;

	if (mode != CIOLIB_MODE_CUSTOM) {
		pthread_mutex_lock(&vstatlock);
		if (mode == vstat.mode) {
			pthread_mutex_unlock(&vstatlock);
			return;
		}
		pthread_mutex_unlock(&vstatlock);
	}

	pthread_mutex_lock(&vstatlock);
	oldcols = vstat.cols;
	bitmap_drv_init_mode(mode, NULL, NULL);
	if (vstat.scrnwidth > 0) {
		for (scaling = 1; (scaling + 1) * vstat.scrnwidth < vstat.winwidth; scaling++)
			;
	}
	vstat.winwidth = vstat.scrnwidth * scaling;
	vstat.winheight = vstat.scrnheight * scaling;
	aspect_fix(&vstat.winwidth, &vstat.winheight, vstat.aspect_width, vstat.aspect_height);
	if (oldcols != vstat.cols) {
		if (oldcols == 0) {
			if (ciolib_initial_window_width > 0)
				vstat.winwidth = ciolib_initial_window_width;
			if (ciolib_initial_window_height > 0)
				vstat.winheight = ciolib_initial_window_height;
			if (vstat.cols == 40)
				oldcols = 40;
		}
		if (oldcols == 40) {
			vstat.winwidth /= 2;
			vstat.winheight /= 2;
		}
		if (vstat.cols == 40) {
			vstat.winwidth *= 2;
			vstat.winheight *= 2;
		}
	}
	if (vstat.winwidth < vstat.scrnwidth)
		vstat.winwidth = vstat.scrnwidth;
	if (vstat.winheight < vstat.scrnheight)
		vstat.winheight = vstat.scrnheight;
	// TODO? This is called before there's a window...
	set_ciolib_scaling();
	gdi_setwinsize(vstat.winwidth, vstat.winheight);
	pthread_mutex_unlock(&vstatlock);
	bitmap_drv_request_pixels();

	return;
}

void
gdi_settitle(const char *newTitle)
{
	uint8_t *utf8;
	size_t sz;

	free(title);
	utf8 = cp_to_utf8(getcodepage(), newTitle, strlen(newTitle), NULL);
	// Overkill
	sz = strlen(utf8) * 4;
	title = malloc(sz);
	if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, (LPWSTR)title, sz / 2))
		PostMessageW(win, WM_SETTEXT, 0, (LPARAM)title);
	free(utf8);
}

void
gdi_seticon(const void *icon, unsigned long size)
{
	// TODO
}

void
gdi_copytext(const char *text, size_t buflen)
{
	// TODO
}

char *
gdi_getcliptext(void)
{
	// TODO
	return NULL;
}

int
gdi_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	WINDOWPLACEMENT wp;

	pthread_mutex_lock(&vstatlock);
	if(width)
		*width=vstat.winwidth;
	if(height)
		*height=vstat.winheight;
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_lock(&winpos_lock);
	if(xpos)
		*xpos=winxpos;
	if(ypos)
		*ypos=winypos;
	pthread_mutex_unlock(&winpos_lock);
	
	return(1);
}

int
gdi_init(int mode)
{
	CreatePipe(&rch, &wch, NULL, 0);

	bitmap_drv_init(gdi_drawrect, gdi_flush);
	gdi_textmode(mode);

	_beginthread(gdi_thread, 0, NULL);

	cio_api.mode=CIOLIB_MODE_GDI;
	FreeConsole();
	cio_api.options |= CONIO_OPT_PALETTE_SETTING | CONIO_OPT_SET_TITLE | CONIO_OPT_SET_NAME | CONIO_OPT_SET_ICON;
	return(0);
}

int
gdi_initciolib(int mode)
{
	pthread_mutex_init(&gdi_headlock, NULL);
	pthread_mutex_init(&winpos_lock, NULL);
	pthread_mutex_init(&rect_lock, NULL);

	return(gdi_init(mode));
}

void
gdi_drawrect(struct rectlist *data)
{
	data->next = NULL;
	pthread_mutex_lock(&gdi_headlock);
	if (update_list == NULL)
		update_list = update_list_tail = data;
	else {
		update_list_tail->next = data;
		update_list_tail = data;
	}
	pthread_mutex_unlock(&gdi_headlock);
}

void
gdi_flush(void)
{
	struct rectlist *list;
	struct rectlist *old_next;

	pthread_mutex_lock(&gdi_headlock);
	list = update_list;
	update_list = update_list_tail = NULL;
	pthread_mutex_unlock(&gdi_headlock);
	for (; list; list = old_next) {
		old_next = list->next;
		if (list->next == NULL) {
			next_rect(list);
			PostMessageW(win, WM_USER_INVALIDATE, 0, 0);
		}
		else
			bitmap_drv_free_rect(list);
	}
}

int
gdi_mousepointer(enum ciolib_mouse_ptr type)
{
	// TODO
	return 0;
}

void
gdi_setwinposition(int x, int y)
{
	PostMessageW(win, WM_USER_SETPOS, x, y);
}

void
gdi_setwinsize(int w, int h)
{
	PostMessageW(win, WM_USER_SETSIZE, w, h);
}
