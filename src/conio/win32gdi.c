#include <windows.h>
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

static FILE *debug;
static uint8_t *title;

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
static pthread_mutex_t rect_lock;

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

static LRESULT CALLBACK
gdi_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	PAINTSTRUCT ps;
	HBITMAP di;
	static HDC memDC = NULL;
	WPARAM highpair;
	uint32_t cp;
	uint8_t ch;
	uint16_t repeat;
	uint16_t i;
	bool alt;
	HDC winDC;
	struct rectlist *list;
	int w,h;

	switch(msg) {
		case WM_PAINT:
			list = get_rect();
			if (list == NULL)
				return 0;
			winDC = BeginPaint(hwnd, &ps);
			if (memDC == NULL)
				memDC = CreateCompatibleDC(winDC);
			b5hdr.bV5Width = list->rect.width;
			b5hdr.bV5Height = -list->rect.height;
			b5hdr.bV5SizeImage = list->rect.width * list->rect.height * 4;
			di = CreateDIBitmap(winDC, (BITMAPINFOHEADER *)&b5hdr, CBM_INIT, list->data, (BITMAPINFO *)&b5hdr, 0/*DIB_RGB_COLORS*/);
			di = SelectObject(memDC, di);
			pthread_mutex_lock(&vstatlock);
			w = vstat.winwidth;
			h = vstat.winheight;
			pthread_mutex_unlock(&vstatlock);
			StretchBlt(winDC, 0, 0, w, h, memDC, 0, 0, list->rect.width, list->rect.height, SRCCOPY);
			di = SelectObject(memDC, di);
			DeleteObject(di);
			EndPaint(hwnd, &ps);
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_CHAR:
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

	return DefWindowProcW(hwnd, msg, wParam, lParam);
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
		case WM_USER_INVALIDATE:
			InvalidateRect(win, NULL, FALSE);
			return true;
		case WM_USER_SETSIZE:
			pthread_mutex_lock(&vstatlock);
			// Now make the inside of the window the size we want (sigh)
			r.left = r.top = 0;
			r.right = vstat.winwidth = msg.wParam;
			r.bottom = vstat.winheight = msg.lParam;
			pthread_mutex_unlock(&vstatlock);
			AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);
			SetWindowPos(win, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
			return true;
		case WM_USER_SETPOS:
			SetWindowPos(win, NULL, msg.wParam, msg.lParam, 0, 0, SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_NOZORDER);
			return true;
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

			if (msg.message == WM_KEYUP)
				return false;

			for (i = 0; keyval[i].VirtualKeyCode != 0; i++) {
				if (keyval[i].VirtualKeyCode == msg.wParam) {
					if (msg.lParam & (0x2000)) {
						if (keyval[i].ALT > 255) {
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
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);
	win = CreateWindowW(wc.lpszClassName, L"SyncConsole", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);

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
	// TODO: This is called before there's a window...
	gdi_setwinsize(vstat.winwidth, vstat.winheight);
	pthread_mutex_unlock(&vstatlock);
	bitmap_drv_request_pixels();

	return;
}

void
gdi_setname(const char *name)
{
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
}

void
gdi_copytext(const char *text, size_t buflen)
{
}

char *
gdi_getcliptext(void)
{
	return NULL;
}

int
gdi_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	pthread_mutex_lock(&vstatlock);
	if(width)
		*width=vstat.winwidth;
	if(height)
		*height=vstat.winheight;
	// TODO
	if(xpos)
		*xpos=0;
	if(ypos)
		*ypos=0;
	pthread_mutex_unlock(&vstatlock);
	
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
	//FreeConsole();
	cio_api.options |= CONIO_OPT_PALETTE_SETTING | CONIO_OPT_SET_TITLE | CONIO_OPT_SET_NAME | CONIO_OPT_SET_ICON;
	return(0);
}

int
gdi_initciolib(int mode)
{
debug = fopen("gdi.log", "w");
	pthread_mutex_init(&gdi_headlock, NULL);
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

void
gdi_setscaling(int newval)
{
}

int
gdi_getscaling(void)
{
	return 1;
}

int
gdi_mousepointer(enum ciolib_mouse_ptr type)
{
	return 0;
}

int
gdi_showmouse(void)
{
	return 1;
}

int
gdi_hidemouse(void)
{
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
