// NEW_VIDEO_BUILD keyboard backend: the WWKeyboardClass subclass
// (wwkeyboard.h:911, pure virtual Fill_Buffer_From_System) that the null
// video branch needs in place of wwkeyboard_null.cpp (which the door build
// deliberately excludes -- see ../CMakeLists.txt -- since both would
// otherwise define CreateWWKeyboardClass()).
//
// Fill_Buffer_From_System() drives the pump/parse/inject chain every poll:
// door_pump() (door_core.c/door_io.c, Task 2) reads the socket and feeds raw
// bytes into door_io.c's ring; door_input_drain() (door_input.c, Task 3)
// turns whatever's in that ring into discrete key/mouse events; the loop
// below drains THAT queue and injects each one via Put_Key_Message()/
// Put_Mouse_Message() -- mirroring wwkeyboard_sdl2.cpp's SDL_KEYDOWN/
// SDL_KEYUP/SDL_MOUSEBUTTONDOWN/SDL_MOUSEBUTTONUP/SDL_MOUSEWHEEL handling,
// the injection contract this backend must not be distinguishable from. Both
// Put_*_Message() are `protected` on WWKeyboardClass (wwkeyboard.h) --
// reachable here because this is a member function of a subclass, same as
// wwkeyboard_sdl2.cpp.
//
// door_input.c is plain C and never includes this header (KeyASCIIType/
// WWKeyboardClass need a C++ compiler), so it emits/consumes plain VK_*
// integers by convention rather than the enum -- see door_input.h/.c's
// comments. To_ASCII is a thin wrapper over door_input_ascii_from_vk(), the
// sdl_keymap.h equivalent for that VK space.
#include "wwkeyboard.h"
#include "door.h"
#include "door_input.h"

class WWKeyboardClassTermGfx : public WWKeyboardClass
{
public:
virtual ~WWKeyboardClassTermGfx()
{
}

virtual void Fill_Buffer_From_System(void)
{
	door_pump();
	door_input_drain();

	door_input_event_t ev;
	while (!Is_Buffer_Full() && door_input_next_event(&ev)) {
		switch (ev.kind) {
			case DOOR_IN_KEY:
				Put_Key_Message(ev.vk, ev.release != 0);
				break;
			case DOOR_IN_MOUSE:
				Put_Mouse_Message(ev.vk, ev.x, ev.y, ev.release != 0);
				break;
		}
	}
}

// Mirrors wwkeyboard_sdl2.cpp's shape (drop release-tagged keys, strip the
// mod bits, table lookup) against door_input.c's VK-space table instead of
// sdl_keymap[] -- WWKEY_SHIFT_BIT is the only bit that changes the result
// (Ctrl/Alt don't remap the character, same as the real Win32 VK_* space).
virtual KeyASCIIType To_ASCII(unsigned short key)
{
	if (key & WWKEY_RLS_BIT) {
		return KA_NONE;
	}
	int shifted = (key & WWKEY_SHIFT_BIT) != 0;
	int ascii   = door_input_ascii_from_vk((unsigned short)(key & 0xFF), shifted);
	if (ascii == 0) {
		return KA_NONE;
	}
	return (KeyASCIIType)ascii;
}
};

WWKeyboardClass* CreateWWKeyboardClass(void)
{
	return new WWKeyboardClassTermGfx;
}
