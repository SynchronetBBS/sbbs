#include <OpenDoor.h>     // Must be included in all doors with odoors
#include <stdio.h>        //library include files
#include <string.h>
#include <stdlib.h>
#include <dirwrap.h>
#include <genwrap.h>
#ifdef ODPLAT_NIX
#include <ciowrap.h>
#else
#include <conio.h>
#endif
#include <limits.h>

#include "../structs.h"
