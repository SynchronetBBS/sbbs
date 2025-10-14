/*
 * TimeSlicer stuff
 */

#include "defines.h"
#include "tasker.h"

struct {
	int16_t Poll;
	bool UseSlicer;
	bool CurSlice;
} TSlicer = { 0, false, 0 };

void t_slice(void)
{
#warning Do something here...
}

void TSlicer_Update(void)
{
	static bool SliceOn = false;

	/* release time slices */
	if (!SliceOn && TSlicer.UseSlicer && TSlicer.CurSlice == TSlicer.Poll) {
		t_slice();
		SliceOn = true;
		TSlicer.CurSlice = 0;
	}
	else {
		SliceOn = false;
		TSlicer.CurSlice++;
	}

	// FIXME: read in online.flg file and see if message in it
}

void TSlicer_SetPoll(int16_t Poll)
{
	TSlicer.Poll = Poll;
}

void TSlicer_Init(void)
{
	TSlicer.UseSlicer = true;
	TSlicer.CurSlice = 0;

	TSlicer_SetPoll(100);
}
