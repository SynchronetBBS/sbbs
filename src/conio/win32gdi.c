#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

#define BITMAP_CIOLIB_DRIVER
#include "win32gdi.h"
#include "bitmap_con.h"
#include "win32cio.h"
#include "scale.h"

HBITMAP bmp;
HWND win;
HANDLE rch;
HANDLE wch;

#define LCS_WINDOWS_COLOR_SPACE 0x57696E20

// Used to create a DI bitmap from bitmap_con data
BITMAPV5HEADER b5hdr = {
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
static pthread_mutex_t gdi_headlock;
static pthread_mutex_t bmp_lock;
static int bitmap_width,bitmap_height;

// Internal implementation

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
	HBITMAP obmp;
	HDC memDC;
	HDC winDC;
	WPARAM highpair;
	uint32_t cp;
	uint8_t ch;
	uint16_t repeat;
	uint16_t i;
	bool alt;

	switch(msg) {
		case WM_PAINT:
			winDC = BeginPaint(hwnd, &ps);
			memDC = CreateCompatibleDC(winDC);
			pthread_mutex_lock(&bmp_lock);
			obmp = SelectObject(memDC, bmp);
			pthread_mutex_lock(&vstatlock);
			//BitBlt(winDC, 0, 0, vstat.winwidth, vstat.winheight, memDC, 0, 0, SRCCOPY);
			StretchBlt(winDC, 0, 0, vstat.winwidth, vstat.winheight, memDC, 0, 0, vstat.scrnwidth, vstat.scrnheight, SRCCOPY);
			pthread_mutex_unlock(&vstatlock);
			SelectObject(memDC, obmp);
			pthread_mutex_unlock(&bmp_lock);
			DeleteDC(memDC);
			EndPaint(hwnd, &ps);
			break;
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

// Must be called with vstatlock held
static void
setup_bitmaps(void)
{
	HDC winDC;

	pthread_mutex_lock(&bmp_lock);
	if (bmp)
		DeleteObject(bmp);
	winDC = GetDC(win);
	bmp = CreateCompatibleBitmap(winDC, vstat.scrnwidth, vstat.scrnheight);
	ReleaseDC(win, winDC);
	pthread_mutex_unlock(&bmp_lock);
	b5hdr.bV5Width = vstat.scrnwidth;
	b5hdr.bV5Height = -vstat.scrnheight;
	b5hdr.bV5SizeImage = b5hdr.bV5Width * b5hdr.bV5Height * 4;
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

	// TODO: When window gets focus, poll mods... *sigh*
	// TODO: Check "extended" for RCTRL and RSHIFT
	if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP || msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) {
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
		while (keyval[i].VirtualKeyCode != 0) {
			if (keyval[i].VirtualKeyCode == msg.wParam)
				break;
			i++;
		}
		if (keyval[i].VirtualKeyCode != 0) {
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
		}
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
	wc.lpszClassName = L"SyncTERM";

	RegisterClassW(&wc);
	pthread_mutex_lock(&vstatlock);
	setup_bitmaps();
	// Now make the inside of the window the size we want (sigh)
	r.left = r.top = 0;
	r.right = vstat.winwidth;
	r.bottom = vstat.winheight;
	pthread_mutex_unlock(&vstatlock);
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);
	win = CreateWindowW(wc.lpszClassName, L"SyncTERM", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);

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
	DWORD avail;

	PeekNamedPipe(rch, NULL, 0, NULL, &avail, NULL);
	return (avail > 0);
}

int
gdi_getch(void)
{
	uint8_t ch;
	DWORD got;

	do {
		ReadFile(rch, &ch, 1, &got, NULL);
	} while (got == 0);
	return ch;
}

void
gdi_beep(void)
{
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
	bitmap_drv_init_mode(mode, &bitmap_width, &bitmap_height);
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
	pthread_mutex_unlock(&vstatlock);

	return;
}

void
gdi_setname(const char *name)
{
}

void
gdi_settitle(const char *title)
{
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
	pthread_mutex_init(&bmp_lock, NULL);

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
	HBITMAP di;
	HDC mDC1, mDC2, winDC;
	HBITMAP obmp1, obmp2;

	pthread_mutex_lock(&gdi_headlock);
	list = update_list;
	update_list = update_list_tail = NULL;
	pthread_mutex_unlock(&gdi_headlock);
	for (; list; list = old_next) {
		old_next = list->next;
		if (list->next == NULL) {
			// Create the DI bitmap and blit to bitmap, then update
			winDC = GetDC(win);
			mDC1 = CreateCompatibleDC(winDC);
			mDC2 = CreateCompatibleDC(mDC1);
			di = CreateDIBitmap(winDC, (BITMAPINFOHEADER *)&b5hdr, CBM_INIT, list->data, (BITMAPINFO *)&b5hdr, 0/*DIB_RGB_COLORS*/);
			ReleaseDC(win, winDC);
			obmp1 = SelectObject(mDC1, di);
			pthread_mutex_lock(&bmp_lock);
			obmp2 = SelectObject(mDC2, bmp);
			BitBlt(mDC2, list->rect.x, list->rect.y, list->rect.width, list->rect.height, mDC1, 0, 0, SRCCOPY);
			SelectObject(mDC1, obmp1);
			SelectObject(mDC2, obmp2);
			pthread_mutex_unlock(&bmp_lock);
			DeleteObject(di);
			DeleteDC(mDC2);
			DeleteDC(mDC1);
			InvalidateRect(win, NULL, FALSE);
		}
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
	return -1;// 0 on success
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
}

void
gdi_setwinsize(int w, int h)
{
}
