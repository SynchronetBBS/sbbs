#include <stddef.h>

#include "sgrmouse.h"

// See mouse.h for why 32 is tested before 64.
termgfx_sgr_kind_t
termgfx_sgr_classify(int b, int *button, int *wheel)
{
	int base = b & ~28;             // strip modifiers: 4 shift, 8 alt, 16 ctrl

	// THE MOTION BIT IS AUTHORITATIVE. 96 and 97 are both pointer motion,
	// and neither is treated as a wheel notch.
	//
	// 97 was briefly rescued as wheel-down-with-motion, on the reasoning
	// that a hover never sets the low bit and on a session measured at 75
	// such reports against 30 clean ones. A later session inverts that
	// ratio completely -- 1492 clean 64/65 detents against 16 of 97 --
	// and several of the 97s arrive 6 to 8 ms apart with the coordinates
	// walking one cell at a time, which is a pointer being moved and not
	// a wheel any hand can turn that fast.
	//
	// What the rescue actually bought was a phantom scroll on every
	// pointer movement: the door's block selection changed whenever the
	// mouse moved, and behaved correctly only while the hand was held
	// perfectly still. Real detents arrive as 64 and 65 WITHOUT the
	// motion bit, in their thousands, so nothing is lost by trusting it.
	if (base & 32)
		return TERMGFX_SGR_MOVE;    // hover (xterm 35, SyncTERM 96/97) or drag (32|btn)

	if (base & 64) {                // wheel: 64 = up, 65 = down
		if (wheel != NULL)
			*wheel = (base & 1) ? 1 : -1;
		return TERMGFX_SGR_WHEEL;
	}

	if (button != NULL)
		*button = base & 3;         // 0 left, 1 middle, 2 right
	return TERMGFX_SGR_BUTTON;
}
