/*****************************************************************************
 *
 * File ..................: vbbsutil.cpp
 * Purpose ...............: Misc Utilties
 *
 *****************************************************************************
 * Copyright (C) 1999-2002  Darryl Perry
 * Copyright (C) 2002-2003	Michael Montague
 *
 *****************************************************************************/

#include "vbbs.h"
#include "vbbsutil.h"

u32 vbbs_util_getday()
{
	time_t t = time(NULL);
	return (t / (24L * 60L * 60L));
}


