#include <stddef.h>

#include "sgrmouse.h"

// See mouse.h for why 32 is tested before 64.
termgfx_sgr_kind_t
termgfx_sgr_classify(int b, int *button, int *wheel)
{
	int base = b & ~28;             // strip modifiers: 4 shift, 8 alt, 16 ctrl

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
