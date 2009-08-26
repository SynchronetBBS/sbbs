/*
 * TimeSlicer stuff
 */

#include "defines.h"
#include "tasker.h"

struct {
	_INT16 Poll;
	BOOL UseSlicer;
	BOOL CurSlice;
} TSlicer = { 0, FALSE, 0 };

void t_slice()
{
}
void TSlicer_Update(void)
{
	static BOOL SliceOn = FALSE;

	/* release time slices */
	if (!SliceOn && TSlicer.UseSlicer && TSlicer.CurSlice == TSlicer.Poll) {
		t_slice();
		SliceOn = TRUE;
		TSlicer.CurSlice = 0;
	}
	else {
		SliceOn = FALSE;
		TSlicer.CurSlice++;
	}

	// FIXME: read in online.flg file and see if message in it
}

void TSlicer_SetPoll(_INT16 Poll)
{
	TSlicer.Poll = Poll;
}

void TSlicer_Init(void)
{
	TSlicer.UseSlicer = TRUE;
	TSlicer.CurSlice = 0;

	TSlicer_SetPoll(100);
}
