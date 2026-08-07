#include <stddef.h>

#include "OpenDoor.h"

static DWORD (ODCALL * volatile pSaveScreenSize)(void) =
    od_save_screen_size;
static BOOL (ODCALL * volatile pSaveScreenEx)(void *, DWORD) =
    od_save_screen_ex;
static BOOL (ODCALL * volatile pRestoreScreenEx)(const void *, DWORD) =
    od_restore_screen_ex;

int main(void)
{
    return od_control_get() == &od_control && pSaveScreenSize != NULL
        && pSaveScreenEx != NULL && pRestoreScreenEx != NULL ? 0 : 1;
}
