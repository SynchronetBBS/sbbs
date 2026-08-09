#include <stddef.h>

#include "sgrmouse.h"

// See mouse.h for why 32 is tested before 64.
termgfx_sgr_kind_t
termgfx_sgr_classify(int b, int *button, int *wheel)
{
	int base = b & ~28;             // strip modifiers: 4 shift, 8 alt, 16 ctrl

	// A wheel notch that ALSO carries the motion bit. SyncTERM sets it
	// whenever the pointer moved in the same event, so a click made while
	// the hand is not perfectly still arrives as 96 or 97 -- and the
	// motion-first test below then classified it as a hover and threw it
	// away. Measured across four sessions on 1.9rc4 and 1.10a: one had 75
	// of these against 30 clean wheel reports.
	//
	// Only 97 can be rescued. SyncTERM's no-button hover is 96 exactly
	// (mouse_state() forces button 3, the wheel remap turns that into 64,
	// then the motion bit is added), so 96 means either wheel-up-with-
	// motion or a plain hover and NOTHING can tell them apart. 97 has the
	// low bit set, which a hover never does, so it is unambiguous.
	if ((base & 96) == 96 && (base & 1)) {
		if (wheel != NULL)
			*wheel = 1;         // 97: wheel DOWN, with motion
		return TERMGFX_SGR_WHEEL;
	}

	if (base & 32)
		return TERMGFX_SGR_MOVE;    // hover (xterm 35, SyncTERM 96) or drag (32|btn)

	if (base & 64) {                // wheel: 64 = up, 65 = down
		if (wheel != NULL)
			*wheel = (base & 1) ? 1 : -1;
		return TERMGFX_SGR_WHEEL;
	}

	if (button != NULL)
		*button = base & 3;         // 0 left, 1 middle, 2 right
	return TERMGFX_SGR_BUTTON;
}
