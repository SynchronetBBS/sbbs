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
//   32   what cterm.adoc SPECIFIES for mode 1003 -- "32 is added to the
//        button number for movement events. If no button is pressed, it acts
//        as though button 0 is" (and 1006: "Pb remains the same").
//   35   what xterm actually sends. xterm's ctlseqs only documents the low
//        two bits (0=MB1, 1=MB2, 2=MB3, 3=release) and that motion adds 32;
//        it never says which value a no-button motion uses. Its source
//        settles it -- button.c's BtnCode() takes button<0 (FirstBitN(0), no
//        button held) and does `result += 3`, and the SGR path then subtracts
//        the X10 +32 bias: 32+32+3-32 = 35.
//   96   what SyncTERM sent BEFORE bbf7c4c79b (dice-12-carpet, 2026-07-11),
//        matching neither -- not even its own
//        spec above. syncterm/term.c's mouse_state() forces the no-button
//        case to bit 4 -> button 3, which then collides with the wheel remap
//        `if (button >= 3) button += 61` (there to turn CIOLIB's wheel
//        buttons into xterm's 64/65) -- giving 64 -- before the 32 motion bit
//        is added.
//
// THE EXCEPTION IS 97. SyncTERM sets the motion bit on a wheel notch too
// whenever the pointer moved in the same event, so a click made without a
// perfectly still hand arrives as 96 (up) or 97 (down). 97 is decoded as a
// wheel because a hover is 96 exactly -- the low bit is never set on one --
// while 96 stays MOVE because nothing can tell wheel-up-with-motion from a
// hover. Half the notches are recoverable; the other half are a collision in
// the terminal's own encoding.
//
// BOTH OF THOSE ARE HISTORY IN A CURRENT SyncTERM, and the handling stays
// anyway. bbf7c4c79b (dice-12-carpet, 2026-07-11) stopped routing the
// no-button sentinel through the wheel remap, so motion is now reported as
// xterm's 35 and a notch as a clean 64/65 -- captured live 2026-08-10 from
// CTerm 1.332: twelve motion reports at b=35, wheel at b=64/65, no 96 or 97
// at all. Doors serve whatever client dials in, including releases built
// before that, so 96 and 97 must keep decoding as they do above; what
// changes is that they are no longer the common case.
//
// Note the wheel bit is independent of the low two bits: xterm sets it from
// `button & 4`, not from a value in 0..3. That is exactly the invariant
// SyncTERM's arithmetic remap breaks.
//
// A decoder that tests 64 first therefore reads every SyncTERM hover as a
// wheel notch. Motion and wheel are never combined in practice (a wheel
// "button" is momentary; no motion is reported while it is held), so testing
// 32 first classifies all three hovers, a drag (32|button) and a real wheel
// (64 up / 65 down) correctly.
termgfx_sgr_kind_t termgfx_sgr_classify(int b, int *button, int *wheel);

#endif // TERMGFX_SGRMOUSE_H_
