#ifndef TERMGFX_SGRMOUSE_H_
#define TERMGFX_SGRMOUSE_H_

// sgrmouse.h -- decoding the button field of an xterm SGR mouse report
// (ESC[<b;col;row M  /  ...m), shared by the game doors.
//
// Only the classification of `b` lives here: it is terminal-protocol
// knowledge, not game logic, and getting it wrong is not obvious from
// reading the code. Each door still owns its own coordinate mapping and
// whatever it does with a button/wheel event.

// What a report's button field means.
typedef enum {
	TERMGFX_SGR_MOVE,       // pointer motion (hover or drag): position only
	TERMGFX_SGR_BUTTON,     // press/release; *button is 0=left 1=middle 2=right
	TERMGFX_SGR_WHEEL       // wheel notch; *wheel is -1 (up) or +1 (down)
} termgfx_sgr_kind_t;

// Classify the button field `b` of an SGR mouse report. Modifier bits
// (4 shift, 8 alt, 16 ctrl) are stripped. Fills whichever of *button /
// *wheel the returned kind uses; either pointer may be NULL.
//
// THE MOTION BIT (32) IS TESTED BEFORE THE WHEEL BIT (64), and that order is
// load-bearing, because a no-button hover has three encodings in the wild:
//
//   32   what cterm.adoc specifies for mode 1003 -- "32 is added to the
//        button number for movement events. If no button is pressed, it acts
//        as though button 0 is" (and 1006: "Pb remains the same").
//   35   what xterm actually sends: motion bit + button 3 (the no-button
//        sentinel).
//   96   what SyncTERM actually sends, matching neither, including its own
//        spec above. syncterm/term.c's mouse_state() forces the no-button
//        case to bit 4 -> button 3, which then collides with the wheel remap
//        `if (button >= 3) button += 61` (there to turn CIOLIB's wheel
//        buttons into xterm's 64/65) -- giving 64 -- before the 32 motion bit
//        is added.
//
// A decoder that tests 64 first therefore reads every SyncTERM hover as a
// wheel notch. No terminal sets the motion bit on a genuine wheel report, so
// testing 32 first classifies all three hovers, a drag (32|button) and a real
// wheel (64 up / 65 down) correctly.
termgfx_sgr_kind_t termgfx_sgr_classify(int b, int *button, int *wheel);

#endif // TERMGFX_SGRMOUSE_H_
