/*
 * mocks_od.h
 *
 * Shared mocks for OpenDoors library functionality.  Provides tODControl
 * global and od_clr_scr stub used by 6+ test files.
 */

#ifndef MOCKS_OD_H
#define MOCKS_OD_H

#include <OpenDoor.h>

tODControl od_control;

void od_clr_scr(void)
{
}

#endif /* MOCKS_OD_H */
