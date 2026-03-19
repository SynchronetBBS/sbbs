/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h> /* for kVK_ virtual keycodes */

#include <stdbool.h>
#include <pthread.h>

#include <threadwrap.h>
#include <genwrap.h>
#include <semwrap.h>

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#define BITMAP_CIOLIB_DRIVER
#include "ciolib.h"
#include "cg_cio.h"
#include "bitmap_con.h"
#include "scale.h"

/*
 * gen_defs.h defines BOOL as int, which conflicts with Objective-C's BOOL
 * (which is bool on modern macOS).  Undefine it so AppKit headers work
 * correctly.  Our C code uses gen_defs BOOL only through WORD/DWORD types,
 * not as BOOL directly.
 */
#undef BOOL

/* From cg_cio.m */
extern sem_t cg_init_complete;
extern sem_t cg_mode_set;
extern int cg_initialized;
extern void cg_send_key(uint16_t key);

/*
 * Quartz view — renders bitmap_con data via Core Graphics.
 * Receives keyboard and mouse events.
 */
@interface SyncTermView : NSView
@property (nonatomic) BOOL isFullscreen;
@end

@interface SyncTermWindow : NSWindow
@end

@interface SyncTermAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

/* Global state */
static NSWindow *cg_window;
static SyncTermView *cg_view;
static SyncTermAppDelegate *cg_delegate;
static bool cg_fullscreen;
static enum ciolib_scaling cg_stype;
static pthread_mutex_t cg_rectlock;
static pthread_mutex_t cg_stypelock;
static struct rectlist *cg_update_list;
static struct rectlist *cg_update_list_tail;
static struct rectlist *cg_last;
static struct rectlist *cg_next;
static struct graphics_buffer *cg_scaled_last;
static struct graphics_buffer *cg_scaled_next;
static pthread_mutex_t cg_nextlock;
static int cg_init_mode;
static CGFloat cg_backing_scale = 1.0;

static bool cg_app_shutting_down;

/*
 * macOS virtual keycode → AT Set 1 scancode mapping.
 * macOS VK codes are sparse (0x00–0x7E), so we use a lookup table.
 * Entry of -1 means no mapping (modifier key, etc).
 * The AT scancode indexes into our ScanCodes[] table.
 */
static const int vk_to_at[128] = {
	[kVK_ANSI_A]             = 30,
	[kVK_ANSI_S]             = 31,
	[kVK_ANSI_D]             = 32,
	[kVK_ANSI_F]             = 33,
	[kVK_ANSI_H]             = 35,
	[kVK_ANSI_G]             = 34,
	[kVK_ANSI_Z]             = 44,
	[kVK_ANSI_X]             = 45,
	[kVK_ANSI_C]             = 46,
	[kVK_ANSI_V]             = 47,
	[kVK_ANSI_B]             = 48,
	[kVK_ANSI_Q]             = 16,
	[kVK_ANSI_W]             = 17,
	[kVK_ANSI_E]             = 18,
	[kVK_ANSI_R]             = 19,
	[kVK_ANSI_Y]             = 21,
	[kVK_ANSI_T]             = 20,
	[kVK_ANSI_1]             = 2,
	[kVK_ANSI_2]             = 3,
	[kVK_ANSI_3]             = 4,
	[kVK_ANSI_4]             = 5,
	[kVK_ANSI_6]             = 7,
	[kVK_ANSI_5]             = 6,
	[kVK_ANSI_Equal]         = 13,
	[kVK_ANSI_9]             = 10,
	[kVK_ANSI_7]             = 8,
	[kVK_ANSI_Minus]         = 12,
	[kVK_ANSI_8]             = 9,
	[kVK_ANSI_0]             = 11,
	[kVK_ANSI_RightBracket]  = 27,
	[kVK_ANSI_O]             = 24,
	[kVK_ANSI_U]             = 22,
	[kVK_ANSI_LeftBracket]   = 26,
	[kVK_ANSI_I]             = 23,
	[kVK_ANSI_P]             = 25,
	[kVK_Return]             = 28,
	[kVK_ANSI_L]             = 38,
	[kVK_ANSI_J]             = 36,
	[kVK_ANSI_Quote]         = 40,
	[kVK_ANSI_K]             = 37,
	[kVK_ANSI_Semicolon]     = 39,
	[kVK_ANSI_Backslash]     = 43,
	[kVK_ANSI_Comma]         = 51,
	[kVK_ANSI_Slash]         = 53,
	[kVK_ANSI_N]             = 49,
	[kVK_ANSI_M]             = 50,
	[kVK_ANSI_Period]        = 52,
	[kVK_Tab]                = 15,
	[kVK_Space]              = 57,
	[kVK_ANSI_Grave]         = 41,
	[kVK_Delete]             = 14,  /* backspace */
	[kVK_Escape]             = 1,
	[kVK_F1]                 = 59,
	[kVK_F2]                 = 60,
	[kVK_F3]                 = 61,
	[kVK_F4]                 = 62,
	[kVK_F5]                 = 63,
	[kVK_F6]                 = 64,
	[kVK_F7]                 = 65,
	[kVK_F8]                 = 66,
	[kVK_F9]                 = 67,
	[kVK_F10]                = 68,
	[kVK_F11]                = 87,
	[kVK_F12]                = 88,
	[kVK_Home]               = 71,
	[kVK_PageUp]             = 73,
	[kVK_ForwardDelete]      = 83,
	[kVK_End]                = 79,
	[kVK_PageDown]           = 81,
	[kVK_LeftArrow]          = 75,
	[kVK_RightArrow]         = 77,
	[kVK_DownArrow]          = 80,
	[kVK_UpArrow]            = 72,
	[kVK_ANSI_KeypadDecimal] = 83,
	[kVK_ANSI_KeypadMultiply]= 55,
	[kVK_ANSI_KeypadPlus]    = 78,
	[kVK_ANSI_KeypadMinus]   = 74,
	[kVK_ANSI_KeypadEnter]   = 28,
	[kVK_ANSI_Keypad0]       = 82,
	[kVK_ANSI_Keypad1]       = 79,
	[kVK_ANSI_Keypad2]       = 80,
	[kVK_ANSI_Keypad3]       = 81,
	[kVK_ANSI_Keypad4]       = 75,
	[kVK_ANSI_Keypad5]       = 76,
	[kVK_ANSI_Keypad6]       = 77,
	[kVK_ANSI_Keypad7]       = 71,
	[kVK_ANSI_Keypad8]       = 72,
	[kVK_ANSI_Keypad9]       = 73,
};

/*
 * AT Set 1 scan code table — same as Wayland/X11 backends.
 * Index is AT Set 1 make code.
 */
static struct {
	WORD base;
	WORD shift;
	WORD ctrl;
	WORD alt;
} ScanCodes[] = {
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key  0 */
	{	0x011b, 0x011b, 0x011b, 0xffff }, /* key  1 - Escape */
	{	0x0231, 0x0221, 0xffff, 0x7800 }, /* key  2 - '1' */
	{	0x0332, 0x0340, 0x0300, 0x7900 }, /* key  3 - '2' */
	{	0x0433, 0x0423, 0xffff, 0x7a00 }, /* key  4 - '3' */
	{	0x0534, 0x0524, 0xffff, 0x7b00 }, /* key  5 - '4' */
	{	0x0635, 0x0625, 0xffff, 0x7c00 }, /* key  6 - '5' */
	{	0x0736, 0x075e, 0x071e, 0x7d00 }, /* key  7 - '6' */
	{	0x0837, 0x0826, 0xffff, 0x7e00 }, /* key  8 - '7' */
	{	0x0938, 0x092a, 0xffff, 0x7f00 }, /* key  9 - '8' */
	{	0x0a39, 0x0a28, 0xffff, 0x8000 }, /* key 10 - '9' */
	{	0x0b30, 0x0b29, 0xffff, 0x8100 }, /* key 11 - '0' */
	{	0x0c2d, 0x0c5f, 0x0c1f, 0x8200 }, /* key 12 - '-' */
	{	0x0d3d, 0x0d2b, 0xffff, 0x8300 }, /* key 13 - '=' */
	{	0x0e08, 0x0e08, 0x0e7f, 0xffff }, /* key 14 - backspace */
	{	0x0f09, 0x0f00, 0xffff, 0xffff }, /* key 15 - tab */
	{	0x1071, 0x1051, 0x1011, 0x1000 }, /* key 16 - 'Q' */
	{	0x1177, 0x1157, 0x1117, 0x1100 }, /* key 17 - 'W' */
	{	0x1265, 0x1245, 0x1205, 0x1200 }, /* key 18 - 'E' */
	{	0x1372, 0x1352, 0x1312, 0x1300 }, /* key 19 - 'R' */
	{	0x1474, 0x1454, 0x1414, 0x1400 }, /* key 20 - 'T' */
	{	0x1579, 0x1559, 0x1519, 0x1500 }, /* key 21 - 'Y' */
	{	0x1675, 0x1655, 0x1615, 0x1600 }, /* key 22 - 'U' */
	{	0x1769, 0x1749, 0x1709, 0x1700 }, /* key 23 - 'I' */
	{	0x186f, 0x184f, 0x180f, 0x1800 }, /* key 24 - 'O' */
	{	0x1970, 0x1950, 0x1910, 0x1900 }, /* key 25 - 'P' */
	{	0x1a5b, 0x1a7b, 0x1a1b, 0xffff }, /* key 26 - '[' */
	{	0x1b5d, 0x1b7d, 0x1b1d, 0xffff }, /* key 27 - ']' */
	{	0x1c0d, 0x1c0d, 0x1c0a, 0xffff }, /* key 28 - CR */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 29 - control */
	{	0x1e61, 0x1e41, 0x1e01, 0x1e00 }, /* key 30 - 'A' */
	{	0x1f73, 0x1f53, 0x1f13, 0x1f00 }, /* key 31 - 'S' */
	{	0x2064, 0x2044, 0x2004, 0x2000 }, /* key 32 - 'D' */
	{	0x2166, 0x2146, 0x2106, 0x2100 }, /* key 33 - 'F' */
	{	0x2267, 0x2247, 0x2207, 0x2200 }, /* key 34 - 'G' */
	{	0x2368, 0x2348, 0x2308, 0x2300 }, /* key 35 - 'H' */
	{	0x246a, 0x244a, 0x240a, 0x2400 }, /* key 36 - 'J' */
	{	0x256b, 0x254b, 0x250b, 0x2500 }, /* key 37 - 'K' */
	{	0x266c, 0x264c, 0x260c, 0x2600 }, /* key 38 - 'L' */
	{	0x273b, 0x273a, 0xffff, 0xffff }, /* key 39 - ';' */
	{	0x2827, 0x2822, 0xffff, 0xffff }, /* key 40 - ''' */
	{	0x2960, 0x297e, 0xffff, 0xffff }, /* key 41 - '`' */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 42 - left shift */
	{	0x2b5c, 0x2b7c, 0x2b1c, 0xffff }, /* key 43 - '\' */
	{	0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 }, /* key 44 - 'Z' */
	{	0x2d78, 0x2d58, 0x2d18, 0x2d00 }, /* key 45 - 'X' */
	{	0x2e63, 0x2e43, 0x2e03, 0x2e00 }, /* key 46 - 'C' */
	{	0x2f76, 0x2f56, 0x2f16, 0x2f00 }, /* key 47 - 'V' */
	{	0x3062, 0x3042, 0x3002, 0x3000 }, /* key 48 - 'B' */
	{	0x316e, 0x314e, 0x310e, 0x3100 }, /* key 49 - 'N' */
	{	0x326d, 0x324d, 0x320d, 0x3200 }, /* key 50 - 'M' */
	{	0x332c, 0x333c, 0xffff, 0xffff }, /* key 51 - ',' */
	{	0x342e, 0x343e, 0xffff, 0xffff }, /* key 52 - '.' */
	{	0x352f, 0x353f, 0xffff, 0xffff }, /* key 53 - '/' */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 54 - right shift */
	{	0x372a, 0xffff, 0x3772, 0xffff }, /* key 55 - prt-scr */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 56 - Alt */
	{	0x3920, 0x3920, 0x3920, 0x3920 }, /* key 57 - space bar */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 58 - caps-lock */
	{	0x3b00, 0x5400, 0x5e00, 0x6800 }, /* key 59 - F1 */
	{	0x3c00, 0x5500, 0x5f00, 0x6900 }, /* key 60 - F2 */
	{	0x3d00, 0x5600, 0x6000, 0x6a00 }, /* key 61 - F3 */
	{	0x3e00, 0x5700, 0x6100, 0x6b00 }, /* key 62 - F4 */
	{	0x3f00, 0x5800, 0x6200, 0x6c00 }, /* key 63 - F5 */
	{	0x4000, 0x5900, 0x6300, 0x6d00 }, /* key 64 - F6 */
	{	0x4100, 0x5a00, 0x6400, 0x6e00 }, /* key 65 - F7 */
	{	0x4200, 0x5b00, 0x6500, 0x6f00 }, /* key 66 - F8 */
	{	0x4300, 0x5c00, 0x6600, 0x7000 }, /* key 67 - F9 */
	{	0x4400, 0x5d00, 0x6700, 0x7100 }, /* key 68 - F10 */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 69 - num-lock */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 70 - scroll-lock */
	{	0x4700, 0x3700, 0x7700, 0xffff }, /* key 71 - home */
	{	0x4800, 0x3800, 0x8d00, 0x9800 }, /* key 72 - cursor up */
	{	0x4900, 0x3900, 0x8400, 0xffff }, /* key 73 - page up */
	{	0x4a2d, 0x4a2d, 0xffff, 0xffff }, /* key 74 - minus sign */
	{	0x4b00, 0x3400, 0x7300, 0xffff }, /* key 75 - cursor left */
	{	0xffff, 0x4c35, 0xffff, 0xffff }, /* key 76 - center key */
	{	0x4d00, 0x3600, 0x7400, 0xffff }, /* key 77 - cursor right */
	{	0x4e2b, 0x4e2b, 0xffff, 0xffff }, /* key 78 - plus sign */
	{	0x4f00, 0x3100, 0x7500, 0xffff }, /* key 79 - end */
	{	0x5000, 0x3200, 0x9100, 0xa000 }, /* key 80 - cursor down */
	{	0x5100, 0x3300, 0x7600, 0xffff }, /* key 81 - page down */
	{	CIO_KEY_IC, CIO_KEY_SHIFT_IC, CIO_KEY_CTRL_IC, CIO_KEY_ALT_IC }, /* key 82 - insert */
	{	CIO_KEY_DC, CIO_KEY_SHIFT_DC, CIO_KEY_CTRL_DC, CIO_KEY_ALT_DC }, /* key 83 - delete */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 84 - sys key */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 85 */
	{	0xffff, 0xffff, 0xffff, 0xffff }, /* key 86 */
	{	0x8500, 0x8700, 0x8900, 0x8b00 }, /* key 87 - F11 */
	{	0x8600, 0x8800, 0x8a00, 0x8c00 }, /* key 88 - F12 */
};

/*
 * Display data for drawRect — either raw rectlist (external scaling,
 * CG does the upscale) or pre-scaled graphics_buffer (internal scaling,
 * 1:1 backing pixel mapping).
 */
struct cg_display_data {
	uint32_t *data;
	int width;
	int height;
};

static struct cg_display_data
get_display_data(void)
{
	struct cg_display_data dd = { NULL, 0, 0 };

	assert_pthread_mutex_lock(&cg_nextlock);
	/* Prefer scaled data when available (internal scaling) */
	if (cg_scaled_next != NULL) {
		if (cg_scaled_last != NULL)
			release_buffer(cg_scaled_last);
		cg_scaled_last = cg_scaled_next;
		cg_scaled_next = NULL;
	}
	if (cg_scaled_last != NULL) {
		dd.data = cg_scaled_last->data;
		dd.width = cg_scaled_last->w;
		dd.height = cg_scaled_last->h;
		/* Also free any stale raw rects */
		if (cg_next != NULL) {
			bitmap_drv_free_rect(cg_next);
			cg_next = NULL;
		}
		if (cg_last != NULL) {
			bitmap_drv_free_rect(cg_last);
			cg_last = NULL;
		}
	}
	else {
		/* External scaling — use raw rectlist */
		if (cg_next != NULL) {
			if (cg_last != NULL)
				bitmap_drv_free_rect(cg_last);
			cg_last = cg_next;
			cg_next = NULL;
		}
		if (cg_last != NULL) {
			dd.data = cg_last->data;
			dd.width = cg_last->rect.width;
			dd.height = cg_last->rect.height;
		}
	}
	assert_pthread_mutex_unlock(&cg_nextlock);
	return dd;
}

static void
next_rect(struct rectlist *rect)
{
	assert_pthread_mutex_lock(&cg_nextlock);
	if (cg_next != NULL)
		bitmap_drv_free_rect(cg_next);
	cg_next = rect;
	assert_pthread_mutex_unlock(&cg_nextlock);
}

static void
next_scaled(struct graphics_buffer *buf)
{
	assert_pthread_mutex_lock(&cg_nextlock);
	if (cg_scaled_next != NULL)
		release_buffer(cg_scaled_next);
	cg_scaled_next = buf;
	/* Clear raw rects — we're in internal scaling mode now */
	if (cg_next != NULL) {
		bitmap_drv_free_rect(cg_next);
		cg_next = NULL;
	}
	if (cg_last != NULL) {
		bitmap_drv_free_rect(cg_last);
		cg_last = NULL;
	}
	assert_pthread_mutex_unlock(&cg_nextlock);
}

static void
snap_resize(bool grow)
{
	int w, h;

	assert_rwlock_wrlock(&vstatlock);
	bitmap_snap(grow, 0, 0);
	w = vstat.winwidth;
	h = vstat.winheight;
	assert_rwlock_unlock(&vstatlock);

	CGFloat bs = cg_backing_scale;
	dispatch_async(dispatch_get_main_queue(), ^{
		[cg_window setContentSize:NSMakeSize(w / bs, h / bs)];
	});
	bitmap_drv_request_pixels();
}

#pragma mark - SyncTermView

@implementation SyncTermView

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)isFlipped
{
	return NO;
}

- (void)drawRect:(NSRect)dirtyRect
{
	struct cg_display_data dd = get_display_data();
	if (dd.data == NULL || dd.width <= 0 || dd.height <= 0)
		return;

	/*
	 * Pixel data is 32-bit ARGB (0x00RRGGBB, alpha unused).
	 * For internal scaling, dd contains pre-scaled pixels at
	 * backing resolution (1:1 physical pixel mapping).
	 * For external scaling, dd contains unscaled screen pixels
	 * and CG stretches them to fit the view.
	 */
	CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
	CGContextRef bctx = CGBitmapContextCreate(
	    dd.data, dd.width, dd.height, 8, dd.width * 4,
	    cs, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little);
	CGColorSpaceRelease(cs);
	if (bctx == NULL)
		return;

	CGImageRef img = CGBitmapContextCreateImage(bctx);
	CGContextRelease(bctx);
	if (img == NULL)
		return;

	NSGraphicsContext *gc = [NSGraphicsContext currentContext];
	CGContextRef ctx = [gc CGContext];
	CGContextSetInterpolationQuality(ctx, kCGInterpolationDefault);

	NSRect bounds = self.bounds;
	CGContextDrawImage(ctx, CGRectMake(0, 0, bounds.size.width, bounds.size.height), img);
	CGImageRelease(img);
}

- (void)keyDown:(NSEvent *)event
{
	NSEventModifierFlags mods = event.modifierFlags;
	unsigned short vk = event.keyCode;
	BOOL hasCmd = (mods & NSEventModifierFlagCommand) != 0;
	BOOL hasCtrl = (mods & NSEventModifierFlagControl) != 0;
	BOOL hasShift = (mods & NSEventModifierFlagShift) != 0;
	BOOL hasOpt = (mods & NSEventModifierFlagOption) != 0;

	/*
	 * On macOS, Option = Alt for BBS/terminal keycodes.
	 * Most Option+key combos fall through to the scancode table
	 * and are sent as Alt keycodes.  SyncTERM handles Alt+Q (quit),
	 * Alt+V (paste), etc. at the application level.
	 *
	 * Backend-specific features (fullscreen, snap resize) are
	 * intercepted here, matching what X11/Wayland backends do.
	 */

	/* Cmd+Q: macOS quit convention → CIO_KEY_QUIT */
	if (hasCmd && vk == kVK_ANSI_Q) {
		cg_send_key(CIO_KEY_QUIT);
		return;
	}

	/* Cmd+V: macOS paste convention → Shift-Insert */
	if (hasCmd && vk == kVK_ANSI_V) {
		cg_send_key(CIO_KEY_SHIFT_IC);
		return;
	}

	/* Opt+Enter: toggle fullscreen */
	if (hasOpt && vk == kVK_Return) {
		[cg_window toggleFullScreen:nil];
		return;
	}

	/* Opt+Left/Right: snap resize */
	if (hasOpt && vk == kVK_LeftArrow) {
		snap_resize(false);
		return;
	}
	if (hasOpt && vk == kVK_RightArrow) {
		snap_resize(true);
		return;
	}

	/* Handle typed characters for non-extended keys */
	if (!hasOpt && !hasCmd) {
		NSString *chars = event.characters;
		if (chars.length > 0) {
			unichar ch = [chars characterAtIndex:0];
			/* Control key combos that produce control chars */
			if (hasCtrl && ch >= 1 && ch <= 26) {
				/* Check if the scancode table has a better mapping */
				if (vk < 128) {
					int at = vk_to_at[vk];
					if (at > 0 && at < (int)(sizeof(ScanCodes)/sizeof(ScanCodes[0]))) {
						WORD val = ScanCodes[at].ctrl;
						if (val != 0xffff) {
							cg_send_key(val);
							return;
						}
					}
				}
				cg_send_key(ch);
				return;
			}
			/* Normal printable character */
			if (!hasCtrl && ch >= 0x20 && ch < 0x7f) {
				cg_send_key(ch);
				return;
			}
			/* CR, Tab, Escape via characters */
			if (ch == 0x0d || ch == 0x09 || ch == 0x1b) {
				cg_send_key(ch);
				return;
			}
			/* macOS Delete key (backspace) generates 0x7f as char */
			if (ch == 0x7f && vk == kVK_Delete) {
				cg_send_key(0x08);
				return;
			}
		}
	}

	/* Fall through to scancode table for function/nav keys and Alt combos */
	if (vk < 128) {
		int at = vk_to_at[vk];
		if (at > 0 && at < (int)(sizeof(ScanCodes)/sizeof(ScanCodes[0]))) {
			WORD val;
			if (hasOpt)
				val = ScanCodes[at].alt;
			else if (hasCtrl)
				val = ScanCodes[at].ctrl;
			else if (hasShift)
				val = ScanCodes[at].shift;
			else
				val = ScanCodes[at].base;
			if (val != 0xffff) {
				cg_send_key(val);
				return;
			}
		}
	}
}

- (void)flagsChanged:(NSEvent *)event
{
	/* We don't need to track modifier state separately — NSEvent
	 * provides modifierFlags on every event. */
}

static void
cg_handle_mouse(NSEvent *event, int evtype)
{
	NSPoint loc = [cg_view convertPoint:event.locationInWindow fromView:nil];
	NSRect bounds = cg_view.bounds;
	int ww = (int)bounds.size.width;
	int wh = (int)bounds.size.height;
	int sw, sh, cw, ch, cols, rows;

	if (ww <= 0 || wh <= 0)
		return;

	int x = (int)loc.x;
	/* Cocoa y-origin is bottom-left; convert to top-left */
	int y = wh - 1 - (int)loc.y;

	/* Clamp to window bounds */
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x >= ww) x = ww - 1;
	if (y >= wh) y = wh - 1;

	/* Map window pixel coords to screen bitmap coords */
	assert_rwlock_rdlock(&vstatlock);
	sw = vstat.scrnwidth;
	sh = vstat.scrnheight;
	cw = vstat.charwidth;
	ch = vstat.charheight;
	cols = vstat.cols;
	rows = vstat.rows;
	assert_rwlock_unlock(&vstatlock);

	if (sw <= 0 || sh <= 0)
		return;

	x = (x * sw) / ww;
	y = (y * sh) / wh;

	int cx = x / cw + 1;
	int cy = y / ch + 1;
	if (cx < 1) cx = 1;
	if (cy < 1) cy = 1;
	if (cx > cols) cx = cols;
	if (cy > rows + 1) cy = rows + 1;

	int mods = 0;
	NSUInteger flags = event.modifierFlags;
	if (flags & NSEventModifierFlagShift)
		mods |= CIOLIB_KMOD_SHIFT;
	if (flags & NSEventModifierFlagControl)
		mods |= CIOLIB_KMOD_CTRL;
	if (flags & NSEventModifierFlagOption)
		mods |= CIOLIB_KMOD_ALT;

	ciomouse_gotevent(evtype, cx, cy, x, y, mods);
}

- (void)mouseDown:(NSEvent *)event
{
	cg_handle_mouse(event, CIOLIB_BUTTON_PRESS(1));
}

- (void)mouseUp:(NSEvent *)event
{
	cg_handle_mouse(event, CIOLIB_BUTTON_RELEASE(1));
}

- (void)rightMouseDown:(NSEvent *)event
{
	cg_handle_mouse(event, CIOLIB_BUTTON_PRESS(3));
}

- (void)rightMouseUp:(NSEvent *)event
{
	cg_handle_mouse(event, CIOLIB_BUTTON_RELEASE(3));
}

- (void)otherMouseDown:(NSEvent *)event
{
	if (event.buttonNumber == 2)
		cg_handle_mouse(event, CIOLIB_BUTTON_PRESS(2));
}

- (void)otherMouseUp:(NSEvent *)event
{
	if (event.buttonNumber == 2)
		cg_handle_mouse(event, CIOLIB_BUTTON_RELEASE(2));
}

- (void)mouseMoved:(NSEvent *)event
{
	cg_handle_mouse(event, CIOLIB_MOUSE_MOVE);
}

- (void)mouseDragged:(NSEvent *)event
{
	cg_handle_mouse(event, CIOLIB_MOUSE_MOVE);
}

- (void)rightMouseDragged:(NSEvent *)event
{
	cg_handle_mouse(event, CIOLIB_MOUSE_MOVE);
}

- (void)otherMouseDragged:(NSEvent *)event
{
	cg_handle_mouse(event, CIOLIB_MOUSE_MOVE);
}

- (void)scrollWheel:(NSEvent *)event
{
	if (event.deltaY > 0)
		cg_handle_mouse(event, CIOLIB_BUTTON_PRESS(4));
	else if (event.deltaY < 0)
		cg_handle_mouse(event, CIOLIB_BUTTON_PRESS(5));
}

@end

#pragma mark - SyncTermWindow

@implementation SyncTermWindow

- (BOOL)canBecomeKeyWindow
{
	return YES;
}

- (BOOL)canBecomeMainWindow
{
	return YES;
}

@end

#pragma mark - App Delegate

@implementation SyncTermAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
	extern sem_t cg_launched_sem;
	sem_post(&cg_launched_sem);
}

- (void)doQuit:(id)sender
{
	cg_send_key(CIO_KEY_QUIT);
}

- (void)doPaste:(id)sender
{
	cg_send_key(CIO_KEY_SHIFT_IC);
}

- (void)doSnapBigger:(id)sender
{
	snap_resize(true);
}

- (void)doSnapSmaller:(id)sender
{
	snap_resize(false);
}

- (void)doFullscreen:(id)sender
{
	[cg_window toggleFullScreen:nil];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	if (cg_app_shutting_down)
		return NSTerminateNow;
	cg_send_key(CIO_KEY_QUIT);
	return NSTerminateCancel;
}

- (void)windowDidResize:(NSNotification *)notification
{
	NSSize sz = [cg_view frame].size;
	CGFloat bs = cg_backing_scale;
	int w = (int)(sz.width * bs);
	int h = (int)(sz.height * bs);

	assert_rwlock_wrlock(&vstatlock);
	vstat.winwidth = w;
	vstat.winheight = h;
	vstat.scaling = bitmap_double_mult_inside(w, h);
	assert_rwlock_unlock(&vstatlock);

	bitmap_drv_request_pixels();
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
	CGFloat old_bs = cg_backing_scale;
	CGFloat new_bs = cg_window.backingScaleFactor;
	cg_backing_scale = new_bs;

	/* Rescale vstat dimensions to new backing pixel units */
	if (old_bs != new_bs) {
		NSSize sz = [cg_view frame].size;
		assert_rwlock_wrlock(&vstatlock);
		vstat.winwidth = (int)(sz.width * new_bs);
		vstat.winheight = (int)(sz.height * new_bs);
		vstat.scaling = bitmap_double_mult_inside(
		    vstat.winwidth, vstat.winheight);
		assert_rwlock_unlock(&vstatlock);
	}
	bitmap_drv_request_pixels();
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
	cg_send_key(CIO_KEY_QUIT);
	return NO;
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
	cg_fullscreen = true;
	cio_api.mode = CIOLIB_MODE_QUARTZ_FULLSCREEN;
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
	cg_fullscreen = false;
	cio_api.mode = CIOLIB_MODE_QUARTZ;
}

@end

#pragma mark - Public API (called from main thread via dispatch)

void
cg_events_init(int mode)
{
	cg_init_mode = mode;

	dispatch_async(dispatch_get_main_queue(), ^{
		assert_pthread_mutex_init(&cg_rectlock, NULL);
		assert_pthread_mutex_init(&cg_stypelock, NULL);
		assert_pthread_mutex_init(&cg_nextlock, NULL);

		cg_stype = ciolib_initial_scaling_type;
		if (ciolib_initial_scaling < 1.0)
			ciolib_initial_scaling = 1.0;

		bitmap_drv_init(cg_drawrect, cg_flush);
		bitmap_drv_init_mode(ciolib_initial_mode, NULL, NULL, 0, 0);

		/*
		 * vstat.winwidth/winheight are in backing pixels (physical
		 * screen pixels).  Cocoa APIs use points.  Convert via
		 * backingScaleFactor at the NSWindow boundary.
		 */
		CGFloat bs = [NSScreen mainScreen].backingScaleFactor;

		int w, h;
		assert_rwlock_wrlock(&vstatlock);
		vstat.scaling = ciolib_initial_scaling;
		bitmap_get_scaled_win_size(vstat.scaling,
		    &vstat.winwidth, &vstat.winheight, 0, 0);
		if (cg_init_mode == CIOLIB_MODE_QUARTZ_FULLSCREEN) {
			cg_fullscreen = true;
			NSScreen *scr = [NSScreen mainScreen];
			NSRect frame = scr.frame;
			vstat.winwidth = (int)(frame.size.width * bs);
			vstat.winheight = (int)(frame.size.height * bs);
			vstat.scaling = bitmap_double_mult_inside(
			    vstat.winwidth, vstat.winheight);
		}
		w = vstat.winwidth;
		h = vstat.winheight;
		assert_rwlock_unlock(&vstatlock);

		NSUInteger style = NSWindowStyleMaskTitled
		    | NSWindowStyleMaskClosable
		    | NSWindowStyleMaskMiniaturizable
		    | NSWindowStyleMaskResizable;

		NSRect contentRect = NSMakeRect(0, 0, w / bs, h / bs);
		cg_window = [[SyncTermWindow alloc]
		    initWithContentRect:contentRect
		    styleMask:style
		    backing:NSBackingStoreBuffered
		    defer:NO];

		cg_view = [[SyncTermView alloc] initWithFrame:contentRect];
		[cg_window setContentView:cg_view];
		[cg_window setDelegate:cg_delegate];
		[cg_window setTitle:@"SyncTERM"];
		[cg_window setAcceptsMouseMovedEvents:YES];
		[cg_window center];
		[cg_window makeKeyAndOrderFront:nil];
		cg_backing_scale = cg_window.backingScaleFactor;

		if (cg_fullscreen)
			[cg_window toggleFullScreen:nil];

		cg_initialized = 1;
		if (cg_fullscreen)
			cio_api.mode = CIOLIB_MODE_QUARTZ_FULLSCREEN;
		else
			cio_api.mode = CIOLIB_MODE_QUARTZ;
		sem_post(&cg_init_complete);
	});
}

void
cg_events_textmode(int mode)
{
	dispatch_async(dispatch_get_main_queue(), ^{
		int w, h;
		CGFloat bs = cg_backing_scale;

		bitmap_drv_init_mode(mode, NULL, NULL, 0, 0);
		assert_rwlock_rdlock(&vstatlock);
		w = vstat.winwidth;
		h = vstat.winheight;
		assert_rwlock_unlock(&vstatlock);
		[cg_window setContentSize:NSMakeSize(w / bs, h / bs)];
		bitmap_drv_request_pixels();
		sem_post(&cg_mode_set);
	});
}

void
cg_events_settitle(const char *title)
{
	NSString *str = [NSString stringWithUTF8String:title];
	dispatch_async(dispatch_get_main_queue(), ^{
		[cg_window setTitle:str];
	});
}

void
cg_events_setname(const char *name)
{
	NSString *str = [NSString stringWithUTF8String:name];
	dispatch_async(dispatch_get_main_queue(), ^{
		[cg_window setMiniwindowTitle:str];
	});
}

void
cg_events_seticon(const void *icon, unsigned long size)
{
	if (!icon || size == 0)
		return;

	const uint32_t *src = icon;
	unsigned long npixels = size * size;

	/*
	 * ciolib icons are ABGR (A in high byte, R in low byte).
	 * NSBitmapImageRep wants RGBA.
	 */
	uint8_t *rgba = malloc(npixels * 4);
	if (!rgba)
		return;
	for (unsigned long i = 0; i < npixels; i++) {
		uint32_t px = src[i];
		rgba[i * 4 + 0] = px & 0xff;          /* R */
		rgba[i * 4 + 1] = (px >> 8) & 0xff;   /* G */
		rgba[i * 4 + 2] = (px >> 16) & 0xff;  /* B */
		rgba[i * 4 + 3] = (px >> 24) & 0xff;  /* A */
	}

	dispatch_async(dispatch_get_main_queue(), ^{
		unsigned char *planes[1] = { rgba };
		NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
		    initWithBitmapDataPlanes:planes
		    pixelsWide:size
		    pixelsHigh:size
		    bitsPerSample:8
		    samplesPerPixel:4
		    hasAlpha:YES
		    isPlanar:NO
		    colorSpaceName:NSDeviceRGBColorSpace
		    bytesPerRow:size * 4
		    bitsPerPixel:32];
		if (rep) {
			NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(size, size)];
			[img addRepresentation:rep];
			[NSApp setApplicationIconImage:img];
		}
		free(rgba);
	});
}

void
cg_events_drawrect(struct rectlist *data)
{
	data->next = NULL;
	assert_pthread_mutex_lock(&cg_rectlock);
	if (cg_update_list == NULL)
		cg_update_list = cg_update_list_tail = data;
	else {
		cg_update_list_tail->next = data;
		cg_update_list_tail = data;
	}
	assert_pthread_mutex_unlock(&cg_rectlock);
}

void
cg_events_flush(void)
{
	struct rectlist *list;
	struct rectlist *old_next;
	bool internal;

	assert_pthread_mutex_lock(&cg_stypelock);
	internal = (cg_stype == CIOLIB_SCALING_INTERNAL);
	assert_pthread_mutex_unlock(&cg_stypelock);

	assert_pthread_mutex_lock(&cg_rectlock);
	list = cg_update_list;
	cg_update_list = cg_update_list_tail = NULL;
	assert_pthread_mutex_unlock(&cg_rectlock);

	for (; list; list = old_next) {
		old_next = list->next;
		if (list->next == NULL) {
			if (internal) {
				int sw, sh, scw, sch;

				assert_rwlock_rdlock(&vstatlock);
				scw = vstat.scrnwidth;
				sch = vstat.scrnheight;
				bitmap_get_scaled_win_size(vstat.scaling,
				    &sw, &sh,
				    vstat.winwidth, vstat.winheight);
				assert_rwlock_unlock(&vstatlock);

				if (scw != list->rect.width
				    || sch != list->rect.height
				    || sw < list->rect.width
				    || sh < list->rect.height) {
					bitmap_drv_free_rect(list);
				}
				else {
					struct graphics_buffer *scaled;

					scaled = do_scale(list, sw, sh);
					bitmap_drv_free_rect(list);
					if (scaled) {
						next_scaled(scaled);
						dispatch_async(
						    dispatch_get_main_queue(),
						    ^{
							[cg_view
							    setNeedsDisplay:YES];
						});
					}
				}
			}
			else {
				/* Clear any stale scaled buffers */
				assert_pthread_mutex_lock(&cg_nextlock);
				if (cg_scaled_last != NULL) {
					release_buffer(cg_scaled_last);
					cg_scaled_last = NULL;
				}
				if (cg_scaled_next != NULL) {
					release_buffer(cg_scaled_next);
					cg_scaled_next = NULL;
				}
				assert_pthread_mutex_unlock(&cg_nextlock);

				next_rect(list);
				dispatch_async(dispatch_get_main_queue(), ^{
					[cg_view setNeedsDisplay:YES];
				});
			}
		}
		else
			bitmap_drv_free_rect(list);
	}
}

void
cg_events_setscaling(double newval)
{
	int w, h;

	assert_rwlock_wrlock(&vstatlock);
	bitmap_get_scaled_win_size(newval, &vstat.winwidth, &vstat.winheight, 0, 0);
	vstat.scaling = newval;
	w = vstat.winwidth;
	h = vstat.winheight;
	assert_rwlock_unlock(&vstatlock);

	CGFloat bs = cg_backing_scale;
	dispatch_async(dispatch_get_main_queue(), ^{
		[cg_window setContentSize:NSMakeSize(w / bs, h / bs)];
	});
	bitmap_drv_request_pixels();
}

double
cg_events_getscaling(void)
{
	double ret;

	assert_rwlock_rdlock(&vstatlock);
	ret = bitmap_double_mult_inside(vstat.winwidth, vstat.winheight);
	assert_rwlock_unlock(&vstatlock);
	return ret;
}

void
cg_events_setscaling_type(enum ciolib_scaling newval)
{
	assert_pthread_mutex_lock(&cg_stypelock);
	cg_stype = newval;
	assert_pthread_mutex_unlock(&cg_stypelock);
	bitmap_drv_request_pixels();
}

enum ciolib_scaling
cg_events_getscaling_type(void)
{
	enum ciolib_scaling ret;

	assert_pthread_mutex_lock(&cg_stypelock);
	ret = cg_stype;
	assert_pthread_mutex_unlock(&cg_stypelock);
	return ret;
}

void
cg_events_setwinposition(int x, int y)
{
	dispatch_async(dispatch_get_main_queue(), ^{
		NSScreen *scr = cg_window.screen ?: [NSScreen mainScreen];
		NSRect sf = scr.frame;
		/* Cocoa uses bottom-left origin; convert from top-left */
		NSPoint pt = NSMakePoint(x, sf.size.height - y);
		[cg_window setFrameTopLeftPoint:pt];
	});
}

void
cg_events_setwinsize(int w, int h)
{
	CGFloat bs = cg_backing_scale;
	dispatch_async(dispatch_get_main_queue(), ^{
		[cg_window setContentSize:NSMakeSize(w / bs, h / bs)];
	});
}

int
cg_events_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	__block int w, h, x, y;
	dispatch_sync(dispatch_get_main_queue(), ^{
		CGFloat bs = cg_window.backingScaleFactor;
		NSRect frame = cg_window.frame;
		NSRect content = [cg_window contentRectForFrameRect:frame];
		NSScreen *scr = cg_window.screen ?: [NSScreen mainScreen];
		NSRect sf = scr.frame;
		w = (int)(content.size.width * bs);
		h = (int)(content.size.height * bs);
		x = (int)(content.origin.x * bs);
		/* Convert from bottom-left to top-left */
		y = (int)((sf.size.height - content.origin.y - content.size.height) * bs);
	});
	if (width) *width = w;
	if (height) *height = h;
	if (xpos) *xpos = x;
	if (ypos) *ypos = y;
	return 1;
}

void
cg_events_copytext(const char *text, size_t buflen)
{
	NSString *str = [[NSString alloc] initWithBytes:text
	    length:buflen encoding:NSUTF8StringEncoding];
	if (!str)
		str = [[NSString alloc] initWithBytes:text
		    length:buflen encoding:NSISOLatin1StringEncoding];
	if (!str)
		return;

	dispatch_async(dispatch_get_main_queue(), ^{
		NSPasteboard *pb = [NSPasteboard generalPasteboard];
		[pb clearContents];
		[pb setString:str forType:NSPasteboardTypeString];
	});
}

char *
cg_events_getcliptext(void)
{
	__block char *ret = NULL;
	dispatch_sync(dispatch_get_main_queue(), ^{
		NSPasteboard *pb = [NSPasteboard generalPasteboard];
		NSString *str = [pb stringForType:NSPasteboardTypeString];
		if (str)
			ret = strdup(str.UTF8String);
	});
	return ret;
}

int
cg_events_mousepointer(enum ciolib_mouse_ptr type)
{
	dispatch_async(dispatch_get_main_queue(), ^{
		NSCursor *cur;
		switch (type) {
		case CIOLIB_MOUSEPTR_ARROW:
			cur = [NSCursor arrowCursor];
			break;
		case CIOLIB_MOUSEPTR_BAR:
			cur = [NSCursor IBeamCursor];
			break;
		default:
			cur = [NSCursor arrowCursor];
			break;
		}
		[cg_window invalidateCursorRectsForView:cg_view];
		[cur set];
	});
	return 1;
}

#pragma mark - NSApplication event loop (runs on main thread)

void
cg_events_stop(void)
{
	cg_app_shutting_down = true;
	dispatch_async(dispatch_get_main_queue(), ^{
		[NSApp terminate:nil];
	});
}

static void
cg_create_menus(void)
{
	NSMenu *mainMenu = [[NSMenu alloc] init];

	/* SyncTERM app menu */
	NSMenuItem *appItem = [[NSMenuItem alloc] init];
	NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"SyncTERM"];
	NSMenuItem *quitItem = [[NSMenuItem alloc]
	    initWithTitle:@"Quit SyncTERM"
	    action:@selector(doQuit:)
	    keyEquivalent:@"q"];
	[appMenu addItem:quitItem];
	[appItem setSubmenu:appMenu];
	[mainMenu addItem:appItem];

	/* Edit menu */
	NSMenuItem *editItem = [[NSMenuItem alloc] init];
	NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
	NSMenuItem *pasteItem = [[NSMenuItem alloc]
	    initWithTitle:@"Paste"
	    action:@selector(doPaste:)
	    keyEquivalent:@"v"];
	[editMenu addItem:pasteItem];
	[editItem setSubmenu:editMenu];
	[mainMenu addItem:editItem];

	/* View menu */
	NSMenuItem *viewItem = [[NSMenuItem alloc] init];
	NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];

	NSMenuItem *biggerItem = [[NSMenuItem alloc]
	    initWithTitle:@"Snap Bigger"
	    action:@selector(doSnapBigger:)
	    keyEquivalent:@"\U0000F703"]; /* right arrow */
	[biggerItem setKeyEquivalentModifierMask:NSEventModifierFlagOption];
	[viewMenu addItem:biggerItem];

	NSMenuItem *smallerItem = [[NSMenuItem alloc]
	    initWithTitle:@"Snap Smaller"
	    action:@selector(doSnapSmaller:)
	    keyEquivalent:@"\U0000F702"]; /* left arrow */
	[smallerItem setKeyEquivalentModifierMask:NSEventModifierFlagOption];
	[viewMenu addItem:smallerItem];

	[viewMenu addItem:[NSMenuItem separatorItem]];

	NSMenuItem *fsItem = [[NSMenuItem alloc]
	    initWithTitle:@"Toggle Fullscreen"
	    action:@selector(doFullscreen:)
	    keyEquivalent:@"\r"];
	[fsItem setKeyEquivalentModifierMask:NSEventModifierFlagOption];
	[viewMenu addItem:fsItem];

	[viewItem setSubmenu:viewMenu];
	[mainMenu addItem:viewItem];

	[NSApp setMainMenu:mainMenu];
}

void
cg_run_event_loop(void)
{
	@autoreleasepool {
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

		cg_delegate = [[SyncTermAppDelegate alloc] init];
		[NSApp setDelegate:cg_delegate];
		[NSWindow setAllowsAutomaticWindowTabbing:NO];
		cg_create_menus();
		[NSApp activateIgnoringOtherApps:YES];
		[NSApp run];
	}
}
