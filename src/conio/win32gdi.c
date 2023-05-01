#include <windows.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include <xp_dl.h>

#define BITMAP_CIOLIB_DRIVER
#include "win32gdi.h"
#include "bitmap_con.h"
#include "win32cio.h"
#include "scale.h"

static HWND win;
static HANDLE rch;
static HANDLE wch;
static bool maximized = false;
static uint16_t winxpos, winypos;
static const DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
static HCURSOR cursor;
static HANDLE init_sem;
static int xoff, yoff;
static int dwidth = 640;
static int dheight = 480;

#define WM_USER_INVALIDATE WM_USER
#define WM_USER_SETSIZE (WM_USER + 1)
#define WM_USER_SETPOS (WM_USER + 2)
#define WM_USER_SETCURSOR (WM_USER + 3)

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
static pthread_mutex_t off_lock;
static bool ciolib_scaling = false;

// Internal implementation

static LPWSTR
utf8_to_utf16(const uint8_t *str8, int buflen)
{
	int sz;
	LPWSTR ret;

	// First, get the size required for the conversion...
	sz = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, str8, buflen, NULL, 0);
	if (sz == 0)
		return NULL;
	ret = (LPWSTR)malloc((sz + 1) * sizeof(*ret));
	if (sz == MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, str8, buflen, ret, sz)) {
		ret[sz] = 0;
		return ret;
	}
	free(ret);
	return NULL;
}

static uint8_t *
utf16_to_utf8(const LPWSTR str16)
{
	int sz;
	uint8_t *ret;

	// First, get the size required for the conversion...
	sz = WideCharToMultiByte(CP_UTF8, 0, str16, -1, NULL, 0, NULL, NULL);
	if (sz == 0)
		return NULL;
	ret = (uint8_t *)malloc(sz * sizeof(*ret));
	if (sz == WideCharToMultiByte(CP_UTF8, 0, str16, -1, ret, sz * sizeof(*ret), NULL, NULL)) {
		return ret;
	}
	free(ret);
	return NULL;
}

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
gdi_add_key(uint16_t key)
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

static void
gdi_mouse_thread(void *data)
{
	SetThreadName("GDI Mouse");
	while(1) {
		if(mouse_wait())
			gdi_add_key(CIO_KEY_MOUSE);
	}
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
	aspect_fix_inside(&w, &h, vstat.aspect_width, vstat.aspect_height);
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
	static HBITMAP di = NULL;
	static int diw = -1, dih = -1;
	static int sww = -1, swh = -1;
	static int lww = -1, lwh = -1;

	PAINTSTRUCT ps;
	struct rectlist *list;
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
		if (di == NULL || diw != gb->w || dih != gb->h) {
			lww = -1;
			if (di != NULL) {
				DeleteObject(di);
				di = NULL;
			}
			diw = gb->w;
			b5hdr.bV5Width = gb->w;
			dih = gb->h;
			b5hdr.bV5Height = -gb->h;
			b5hdr.bV5SizeImage = gb->w * gb->h * 4;
			pthread_mutex_lock(&off_lock);
			dwidth = diw;
			dheight = dih;
			pthread_mutex_unlock(&off_lock);
		}
		pthread_mutex_lock(&off_lock);
		xoff = (w - diw) / 2;
		yoff = (h - dih) / 2;
		pthread_mutex_unlock(&off_lock);
		data = gb->data;
	}
	else {
		if (di != NULL || diw != list->rect.width || dih != list->rect.height) {
			if (di != NULL) {
				DeleteObject(di);
				di = NULL;
			}
			diw = list->rect.width;
			b5hdr.bV5Width = list->rect.width;
			dih = list->rect.height;
			b5hdr.bV5Height = -list->rect.height;
			b5hdr.bV5SizeImage = list->rect.width * list->rect.height * 4;
		}
		data = list->data;
		if (lww != w || lwh != h) {
			sww = w;
			swh = h;
			aspect_fix_inside(&sww, &swh, aw, ah);
			pthread_mutex_lock(&off_lock);
			xoff = (w - sww) / 2;
			yoff = (h - swh) / 2;
			dwidth = sww;
			dheight = swh;
			pthread_mutex_unlock(&off_lock);
			lww = w;
			lwh = h;
		}
	}
	winDC = BeginPaint(hwnd, &ps);
	if (memDC == NULL) {
		memDC = CreateCompatibleDC(winDC);
	}
	if (di == NULL)
		di = CreateDIBitmap(winDC, (BITMAPINFOHEADER *)&b5hdr, CBM_INIT, data, (BITMAPINFO *)&b5hdr, DIB_RGB_COLORS);
	else
		SetDIBits(winDC, di, 0, dih, data, (BITMAPINFO *)&b5hdr, DIB_RGB_COLORS);
	// Clear to black first
	StretchBlt(winDC, 0, 0, w, h, memDC, 0, 0, 1, 1, BLACKNESS);
	di = SelectObject(memDC, di);
	pthread_mutex_lock(&off_lock);
	if (ciolib_scaling) {
		BitBlt(winDC, xoff, yoff, dwidth, dheight, memDC, 0, 0, SRCCOPY);
	}
	else {
		StretchBlt(winDC, xoff, yoff, dwidth, dheight, memDC, 0, 0, diw, dih, SRCCOPY);
	}
	pthread_mutex_unlock(&off_lock);
	EndPaint(hwnd, &ps);
	di = SelectObject(memDC, di);
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
			gdi_add_key(ch);
	}
	return 0;
}

struct gdi_mouse_pos {
	int tx;
	int ty;
	int px;
	int py;
};

static bool
win_to_pos(LPARAM lParam, struct gdi_mouse_pos *p)
{
	uint16_t cx, cy;
	bool ret = true;
	int cols, rows, scrnheight, scrnwidth;

	cx = lParam & 0xffff;
	cy = (lParam >> 16) & 0xffff;

	pthread_mutex_lock(&vstatlock);
	cols = vstat.cols;
	rows = vstat.rows;
	scrnheight = vstat.scrnheight;
	scrnwidth = vstat.scrnwidth;
	pthread_mutex_unlock(&vstatlock);

	cx = lParam & 0xffff;
	pthread_mutex_lock(&off_lock);
	if (cx < xoff || cy < yoff) {
		pthread_mutex_unlock(&off_lock);
		return false;
	}
	cx -= xoff;
	cy -= yoff;

	p->tx = cx / (((float)dwidth) / cols) + 1;
	if (p->tx > cols)
		ret = false;

	p->px = cx * (scrnwidth) / dwidth;

	p->ty = cy / (((float)dheight) / rows) + 1;
	if (p->ty > rows)
		ret = false;

	p->py = cy * (scrnheight) / dheight;
	pthread_mutex_unlock(&off_lock);
	cx = lParam & 0xffff;
	return ret;
}

static LRESULT
gdi_handle_mouse_button(LPARAM lParam, int event)
{
	struct gdi_mouse_pos p;

	if (win_to_pos(lParam, &p))
		ciomouse_gotevent(event, p.tx, p.ty, p.px, p.py);
	return 0;
}

static LRESULT
gdi_handle_mouse_wheel(int16_t distance, LPARAM lParam)
{
	if (distance < 0) // Forward
		ciomouse_gotevent(CIOLIB_BUTTON_PRESS(4), -1, -1 ,-1 ,-1);
	else
		ciomouse_gotevent(CIOLIB_BUTTON_PRESS(5), -1, -1 ,-1 ,-1);
	return 0;
}

static LRESULT
gdi_handle_activate(HWND hwnd, WPARAM wParam)
{
	static LPCTSTR lc = IDC_IBEAM;
	uint16_t lw = wParam & 0xffff;

	// TODO: We may need to read the state of CTRL and SHIFT keys for extended key input...
	return 0;
}

static LRESULT CALLBACK
gdi_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	POINT p;
	RECT r;

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
		case WM_LBUTTONDOWN:
			return gdi_handle_mouse_button(lParam, CIOLIB_BUTTON_PRESS(1));
		case WM_LBUTTONUP:
			return gdi_handle_mouse_button(lParam, CIOLIB_BUTTON_RELEASE(1));
		case WM_MBUTTONDOWN:
			return gdi_handle_mouse_button(lParam, CIOLIB_BUTTON_PRESS(2));
		case WM_MBUTTONUP:
			return gdi_handle_mouse_button(lParam, CIOLIB_BUTTON_RELEASE(2));
		case WM_MOUSEMOVE:
			return gdi_handle_mouse_button(lParam, CIOLIB_MOUSE_MOVE);
		case WM_MOUSEWHEEL:
			return gdi_handle_mouse_wheel((wParam >> 16) & 0xffff, lParam);
		case WM_RBUTTONDOWN:
			return gdi_handle_mouse_button(lParam, CIOLIB_BUTTON_PRESS(3));
		case WM_RBUTTONUP:
			return gdi_handle_mouse_button(lParam, CIOLIB_BUTTON_RELEASE(3));
		case WM_ACTIVATE:
			return gdi_handle_activate(hwnd, wParam);
		case WM_USER_SETCURSOR:
			if (!GetClientRect(hwnd, &r))
				break;
			if (!GetCursorPos(&p))
				break;
			if (!ScreenToClient(hwnd, &p))
				break;
			if (p.x < 0 || p.y < 0 || p.x > (r.right - r.left) || p.y > (r.bottom - r.top))
				break;
			SetCursor(cursor);
			break;
		case WM_SETCURSOR:
			if ((lParam & 0xffff) == HTCLIENT) {
				SetCursor(cursor);
				return 0;
			}
			break;
	}

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void
gdi_snap(bool grow)
{
	bool wc;
	int w, h;
	int mw, mh;

	if (maximized)
		return;
	mw = GetSystemMetrics(SM_CXMAXTRACK);
	mh = GetSystemMetrics(SM_CYMAXTRACK);
	UnadjustWindowSize(&mw, &mh);
	pthread_mutex_lock(&vstatlock);
	w = vstat.winwidth;
	h = vstat.winheight;
	aspect_fix_inside(&w, &h, vstat.aspect_width, vstat.aspect_height);
	if (vstat.aspect_width == 0 || vstat.aspect_height == 0)
		wc = true;
	else
		wc = lround((double)(h * vstat.aspect_width) / vstat.aspect_height * vstat.scrnwidth / vstat.scrnheight) > w;
	if (wc)
		mw = mw - mw % vstat.scrnwidth;
	else
		mh = mh - mh % vstat.scrnheight;
	mh = mw - mw % vstat.scrnwidth;
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
	if (w > mw)
		w = mw;
	if (h > mh)
		h = mh;
	aspect_fix_inside(&w, &h, vstat.aspect_width, vstat.aspect_height);
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
							gdi_add_key(keyval[i].ALT);
							return true;
						}
					}
					else if (mods & (WMOD_CTRL | WMOD_LCTRL | WMOD_RCTRL)) {
						if (keyval[i].CTRL > 255) {
							gdi_add_key(keyval[i].CTRL);
							return true;
						}
					}
					else if (mods & (WMOD_SHIFT | WMOD_LSHIFT | WMOD_RSHIFT)) {
						if (keyval[i].Shift > 255) {
							gdi_add_key(keyval[i].Shift);
							return true;
						}
					}
					else {
						if (keyval[i].Key > 255) {
							gdi_add_key(keyval[i].Key);
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
	wc.hIcon         = LoadIcon(NULL, MAKEINTRESOURCE(1));
	wc.hCursor       = LoadCursor(NULL, IDC_IBEAM);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = L"SyncConsole";

	cursor = wc.hCursor;
	RegisterClassW(&wc);
	pthread_mutex_lock(&vstatlock);
	// Now make the inside of the window the size we want (sigh)
	r.left = r.top = 0;
	r.right = vstat.winwidth;
	r.bottom = vstat.winheight;
	pthread_mutex_unlock(&vstatlock);
	AdjustWindowRect(&r, style, FALSE);
	win = CreateWindowW(wc.lpszClassName, L"SyncConsole", style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
	ReleaseSemaphore(init_sem, 1, NULL);

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!magic_message(msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	// This may not be necessary...
	DestroyWindow(win);
	UnregisterClassW(wc.lpszClassName, NULL);
	gdi_add_key(CIO_KEY_QUIT);
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
	aspect_fix_inside(&vstat.winwidth, &vstat.winheight, vstat.aspect_width, vstat.aspect_height);
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
	set_ciolib_scaling();
	gdi_setwinsize(vstat.winwidth, vstat.winheight);
	pthread_mutex_unlock(&vstatlock);
	bitmap_drv_request_pixels();

	return;
}

void
gdi_settitle(const char *newTitle)
{
	LPWSTR utf16;

	utf16 = utf8_to_utf16(newTitle, -1);
	if (utf16 != NULL) {
		SetWindowTextW(win, utf16);
		free(utf16);
	}
}

void
gdi_seticon(const void *icon, unsigned long size)
{
	HICON icn = NULL;
	BITMAPINFOHEADER *bmi;
	uint8_t *mi;
	size_t mlen;
	size_t blen;
	size_t isz;
	size_t x,y;
	uint32_t *bdata;
	const uint32_t *sdata = icon;
	uint8_t *mdata;
	uint32_t tmp;
	uint8_t r, g, b, a;
	uint32_t *brow;
	const uint32_t *srow;
	uint8_t *mrow;

	blen = size * size * sizeof(uint32_t);
	mlen = size * (size + 7) / 8;
	isz = sizeof(BITMAPINFOHEADER) + blen + mlen;
	mi = (uint8_t *)calloc(1, isz);
	if (mi == NULL)
		return;
	bmi = (BITMAPINFOHEADER *)mi;
	bdata = (uint32_t *)&mi[sizeof(BITMAPINFOHEADER)];
	mdata = ((uint8_t *)bdata) + blen;
	bmi->biSize = sizeof(BITMAPINFOHEADER);
	bmi->biWidth = size;
	bmi->biHeight = size * 2;
	bmi->biPlanes = 1;
	bmi->biBitCount = 32;
	bmi->biCompression = BI_RGB;
	bmi->biSizeImage = blen;
	for (y = 0; y < size; y++) {
		srow = &sdata[y * size];
		brow = &bdata[(size - y - 1) * size];
		mrow = &mdata[(size - y - 1) * (size + 7) / 8];
		for (x = 0; x < size; x++) {
			tmp = srow[x];
			a = (tmp & 0xff000000) >> 24;
			r = (tmp & 0x00ff0000) >> 16;
			g = (tmp & 0x0000ff00) >> 8;
			b = tmp & 0x000000ff;
			brow[x] = (a << 24) | (b << 16) | (g << 8) | (r);
			if (a > 127)
				mrow[x / 8] |= 1 << (x % 8);
		}
	}

	icn = CreateIconFromResource(mi, isz, TRUE, 0x00030000);
	free(mi);
	SendMessage(win, WM_SETICON, ICON_SMALL, (LPARAM)icn);
	SendMessage(win, WM_SETICON, ICON_BIG, (LPARAM)icn);
}

void
gdi_copytext(const char *text, size_t buflen)
{
	LPWSTR utf16;
	LPWSTR clipStr;
	HGLOBAL clipBuf;

	utf16 = utf8_to_utf16(text, buflen);
	if (utf16 != NULL) {
		if (OpenClipboard(NULL)) {
			EmptyClipboard();
			clipBuf = GlobalAlloc(GMEM_MOVEABLE, (wcslen(utf16) + 1) * sizeof(utf16[0]));
			if (clipBuf != NULL) {
				clipStr = GlobalLock(clipBuf);
				wcscpy(clipStr, utf16);
				GlobalUnlock(clipBuf);
				SetClipboardData(CF_UNICODETEXT, clipBuf);
			}
			CloseClipboard();
		}
		free(utf16);
	}
}

char *
gdi_getcliptext(void)
{
	char *ret = NULL;
	HANDLE dat;
	LPWSTR datstr;

	if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
		return NULL;
	if (!OpenClipboard(NULL))
		return NULL;
	dat = GetClipboardData(CF_UNICODETEXT);
	if (dat != NULL) {
		datstr = GlobalLock(dat);
		if (datstr != NULL) {
			ret = utf16_to_utf8(datstr);
			GlobalUnlock(dat);
		}
	}
	CloseClipboard();
	return ret;
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

	// code that tells windows we're High DPI aware so it doesn't scale our windows
	// taken from Yamagi Quake II

	typedef enum D3_PROCESS_DPI_AWARENESS {
		D3_PROCESS_DPI_UNAWARE = 0,
		D3_PROCESS_SYSTEM_DPI_AWARE = 1,
		D3_PROCESS_PER_MONITOR_DPI_AWARE = 2
	} YQ2_PROCESS_DPI_AWARENESS;

	/* For Vista, Win7 and Win8 */
	BOOL(WINAPI *SetProcessDPIAware)(void) = NULL;

	/* Win8.1 and later */
	HRESULT(WINAPI *SetProcessDpiAwareness)(enum D3_PROCESS_DPI_AWARENESS dpiAwareness) = NULL;

	const char* user32dll[] = {"User32", NULL};
	dll_handle userDLL = xp_dlopen(user32dll, RTLD_LAZY, 0);

	if (userDLL)
	{
		SetProcessDPIAware = xp_dlsym(userDLL, SetProcessDPIAware);
	}

	const char* shcoredll[] = {"SHCore", NULL};
	dll_handle shcoreDLL = xp_dlopen(shcoredll, RTLD_LAZY, 0);

	if (shcoreDLL)
	{
		SetProcessDpiAwareness = xp_dlsym(shcoreDLL, SetProcessDpiAwareness);
	}

	if (SetProcessDpiAwareness) {
		SetProcessDpiAwareness(D3_PROCESS_PER_MONITOR_DPI_AWARE);
	}
	else if (SetProcessDPIAware) {
		SetProcessDPIAware();
	}
	_beginthread(gdi_mouse_thread, 0, NULL);
	_beginthread(gdi_thread, 0, NULL);
	WaitForSingleObject(init_sem, INFINITE);
	CloseHandle(init_sem);

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
	pthread_mutex_init(&off_lock, NULL);
	init_sem = CreateSemaphore(NULL, 0, INT_MAX, NULL);

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
	switch (type) {
		case CIOLIB_MOUSEPTR_ARROW:
			cursor = LoadCursor(NULL, IDC_ARROW);
			break;
		case CIOLIB_MOUSEPTR_BAR:
			cursor = LoadCursor(NULL, IDC_IBEAM);
			break;
	}
	PostMessageW(win, WM_USER_SETCURSOR, 0, 0);
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
