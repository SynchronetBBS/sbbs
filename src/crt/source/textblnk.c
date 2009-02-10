#include <ciolib.h>

void settextblink (int cmd) //if cmd==1 => set attribute bit 7 as enable blink
//bit (default), otherwise it becomes background intensity bit
 {
	int flags;

	flags=getvideoflags();
	if(cmd==1)
		flags &= ~CIOLIB_VIDEO_BGBRIGHT;
	else
		flags |= CIOLIB_VIDEO_BGBRIGHT;
	setvideoflags(flags);
 }
