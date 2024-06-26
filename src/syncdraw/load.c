#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <genwrap.h>		/* chdir() */

#include <dirwrap.h>
#include <gen_defs.h>
#include <ciolib.h>

#include "key.h"
#include "options.h"
#include "syncdraw.h"
#include "miscfunctions.h"
#include "sauce.h"

const unsigned char   LoadAnsi[4096] = {
	177, 42, 223, 2, 32, 2, 220, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 220,
	8, 32, 8, 32, 8, 32, 40, 220, 42, 32, 42, 32, 42, 32, 42, 219, 2, 223, 2, 32,
	2, 220, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 254, 8, 220, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 254, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223,
	8, 223, 8, 220, 8, 32, 8, 32, 8, 32, 8, 223, 8, 220, 8, 220, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 223, 8, 220, 8, 32, 8, 32, 40, 176, 42, 177, 42, 254, 42, 176,
	42, 32, 8, 219, 8, 32, 8, 220, 2, 219, 2, 219, 2, 219, 2, 219, 2, 220, 2, 32,
	2, 219, 8, 32, 8, 32, 8, 32, 40, 176, 42, 177, 42, 32, 42, 223, 2, 32, 2, 219,
	8, 32, 8, 220, 2, 219, 2, 219, 2, 219, 2, 220, 2, 32, 2, 254, 8, 220, 2, 219,
	2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219,
	2, 219, 2, 220, 2, 32, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219,
	2, 220, 2, 32, 2, 219, 8, 32, 8, 32, 8, 32, 8, 222, 8, 219, 8, 32, 8, 68,
	15, 105, 7, 83, 8, 80, 8, 76, 8, 65, 8, 89, 8, 255, 8, 70, 15, 105, 7, 76,
	8, 69, 8, 83, 8, 32, 8, 219, 8, 221, 8, 32, 8, 32, 40, 178, 42, 177, 42, 176,
	42, 32, 8, 219, 8, 32, 8, 219, 2, 219, 2, 219, 2, 176, 42, 177, 42, 219, 2, 220,
	2, 32, 2, 219, 8, 32, 8, 32, 40, 177, 42, 176, 42, 223, 2, 32, 2, 219, 8, 32,
	8, 220, 2, 219, 2, 176, 42, 177, 42, 254, 2, 219, 2, 220, 2, 222, 2, 219, 2, 219,
	2, 219, 2, 219, 114, 176, 42, 177, 42, 176, 42, 219, 114, 176, 42, 177, 42, 219, 114, 254,
	2, 219, 114, 219, 2, 221, 2, 219, 2, 219, 2, 219, 2, 176, 42, 177, 42, 176, 42, 219,
	114, 219, 2, 220, 2, 32, 2, 219, 8, 32, 8, 32, 8, 222, 8, 219, 7, 32, 7, 87,
	15, 105, 7, 84, 8, 72, 8, 255, 8, 84, 15, 72, 8, 105, 7, 83, 8, 255, 8, 32,
	8, 32, 8, 32, 8, 222, 8, 219, 7, 221, 8, 32, 8, 32, 40, 176, 42, 178, 42, 176,
	42, 32, 8, 223, 8, 220, 8, 222, 2, 219, 2, 219, 2, 177, 42, 219, 114, 176, 42, 219,
	2, 32, 2, 219, 8, 32, 8, 32, 40, 176, 42, 223, 2, 32, 2, 219, 8, 32, 8, 220,
	2, 219, 2, 176, 42, 177, 42, 219, 2, 177, 42, 219, 114, 219, 2, 220, 2, 223, 2, 219,
	2, 219, 114, 176, 42, 177, 42, 219, 114, 223, 2, 223, 2, 219, 114, 219, 114, 176, 42, 219,
	114, 219, 2, 223, 2, 220, 2, 219, 2, 176, 42, 177, 42, 219, 2, 177, 42, 219, 114, 177,
	42, 219, 114, 219, 2, 220, 2, 32, 2, 254, 8, 32, 8, 32, 8, 254, 8, 32, 8, 69,
	15, 88, 8, 84, 8, 69, 8, 78, 8, 83, 8, 105, 7, 79, 8, 78, 8, 83, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 254, 8, 32, 8, 32, 8, 32, 40, 176, 42, 178, 42, 176,
	42, 32, 42, 32, 8, 254, 8, 32, 8, 219, 2, 219, 2, 177, 42, 177, 42, 254, 2, 219,
	2, 32, 2, 219, 8, 32, 8, 32, 8, 223, 2, 32, 2, 219, 8, 32, 8, 220, 2, 219,
	2, 254, 2, 219, 114, 219, 2, 223, 2, 219, 2, 177, 42, 176, 42, 219, 2, 220, 2, 222,
	2, 219, 2, 176, 42, 219, 114, 219, 114, 32, 8, 32, 8, 219, 114, 219, 2, 177, 42, 219,
	2, 221, 2, 32, 2, 219, 2, 219, 2, 219, 114, 177, 42, 219, 2, 222, 2, 176, 42, 177,
	42, 176, 42, 219, 2, 219, 2, 32, 2, 219, 8, 32, 8, 32, 8, 254, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 219, 8, 32, 8, 32, 8, 32, 40, 177, 42, 177, 42, 176,
	42, 32, 8, 220, 8, 223, 8, 222, 2, 219, 2, 176, 42, 177, 42, 219, 114, 176, 42, 219,
	2, 32, 2, 254, 8, 223, 8, 223, 8, 223, 8, 223, 8, 254, 8, 32, 8, 219, 2, 219,
	2, 219, 114, 177, 42, 219, 2, 220, 2, 219, 2, 177, 42, 176, 42, 219, 114, 219, 2, 222,
	2, 219, 2, 176, 42, 177, 42, 219, 114, 220, 2, 220, 2, 219, 2, 177, 42, 219, 114, 219,
	2, 221, 2, 219, 2, 219, 2, 219, 2, 219, 2, 177, 42, 219, 2, 32, 2, 219, 2, 177,
	42, 219, 114, 219, 114, 219, 2, 221, 2, 222, 8, 221, 8, 32, 8, 254, 8, 32, 8, 91,
	8, 32, 15, 93, 8, 32, 8, 42, 7, 46, 7, 65, 15, 78, 7, 83, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 254, 8, 254, 8, 32, 8, 32, 8, 32, 40, 176, 42, 177, 42, 32,
	8, 220, 8, 223, 8, 32, 8, 219, 2, 219, 2, 177, 42, 219, 114, 177, 42, 219, 2, 220,
	2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 220, 2, 32, 2, 223, 2, 219,
	2, 219, 114, 176, 42, 177, 42, 219, 2, 177, 42, 219, 114, 176, 42, 219, 2, 219, 2, 222,
	2, 219, 2, 219, 114, 177, 42, 219, 114, 219, 2, 219, 2, 219, 114, 178, 42, 177, 42, 219,
	2, 221, 2, 223, 2, 219, 2, 219, 2, 219, 114, 178, 42, 219, 2, 32, 2, 32, 2, 177,
	42, 176, 42, 219, 114, 219, 2, 221, 2, 222, 8, 221, 8, 32, 8, 250, 8, 32, 8, 91,
	8, 32, 15, 93, 8, 32, 8, 42, 7, 46, 7, 65, 15, 86, 7, 84, 8, 32, 8, 32,
	8, 32, 8, 254, 8, 254, 8, 254, 8, 32, 8, 32, 40, 254, 42, 176, 42, 178, 42, 220,
	8, 223, 8, 32, 8, 219, 2, 219, 2, 176, 42, 219, 114, 176, 42, 176, 42, 177, 42, 177,
	42, 176, 42, 176, 42, 177, 42, 177, 42, 176, 42, 219, 2, 219, 114, 219, 2, 220, 2, 223,
	2, 219, 2, 219, 114, 176, 42, 177, 42, 219, 114, 254, 2, 219, 2, 219, 2, 32, 2, 222,
	2, 219, 2, 176, 42, 177, 42, 219, 2, 32, 2, 32, 2, 219, 2, 177, 42, 219, 114, 219,
	2, 221, 2, 32, 2, 219, 2, 219, 2, 176, 42, 178, 42, 177, 42, 220, 2, 219, 114, 176,
	42, 254, 2, 219, 2, 219, 2, 32, 2, 219, 8, 32, 8, 32, 8, 254, 8, 32, 8, 91,
	8, 32, 15, 93, 8, 32, 8, 42, 7, 46, 7, 66, 15, 73, 7, 78, 8, 32, 8, 32,
	8, 254, 8, 254, 8, 254, 8, 32, 8, 32, 40, 176, 42, 176, 42, 177, 42, 178, 42, 219,
	8, 32, 8, 219, 2, 219, 2, 254, 2, 176, 42, 177, 42, 219, 114, 219, 114, 219, 2, 219,
	2, 219, 2, 219, 2, 219, 2, 219, 114, 32, 34, 177, 42, 254, 2, 219, 114, 219, 2, 32,
	2, 219, 2, 219, 114, 219, 114, 176, 42, 219, 114, 219, 114, 219, 2, 32, 2, 32, 2, 219,
	114, 219, 114, 254, 2, 219, 2, 219, 2, 32, 2, 32, 2, 219, 2, 219, 2, 254, 2, 176,
	42, 219, 2, 32, 2, 223, 2, 219, 2, 219, 2, 176, 42, 219, 114, 254, 2, 177, 42, 219,
	114, 219, 2, 219, 2, 32, 2, 220, 8, 223, 8, 32, 8, 32, 8, 254, 8, 32, 8, 91,
	8, 32, 15, 93, 8, 32, 8, 42, 7, 46, 7, 80, 15, 67, 7, 66, 8, 32, 8, 254,
	8, 254, 8, 223, 8, 32, 8, 32, 40, 177, 42, 178, 42, 177, 42, 178, 42, 178, 42, 219,
	8, 32, 8, 223, 2, 219, 2, 219, 2, 219, 2, 219, 2, 223, 2, 32, 2, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 32, 8, 223, 2, 219, 2, 219, 2, 219, 2, 223, 2, 254,
	8, 32, 8, 223, 2, 219, 2, 219, 2, 219, 2, 223, 2, 32, 2, 254, 8, 223, 2, 219,
	2, 219, 114, 219, 114, 219, 2, 223, 2, 254, 8, 223, 2, 219, 2, 219, 2, 219, 2, 219,
	114, 219, 114, 223, 2, 32, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219, 2, 219,
	2, 223, 2, 32, 2, 220, 8, 223, 8, 32, 8, 32, 8, 32, 8, 219, 8, 32, 8, 91,
	8, 120, 15, 93, 8, 32, 8, 42, 7, 46, 7, 42, 15, 32, 7, 32, 8, 32, 8, 254,
	8, 32, 8, 32, 8, 32, 40, 177, 42, 176, 42, 177, 42, 219, 42, 178, 42, 219, 42, 220,
	2, 223, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 223, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 223, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 223,
	8, 254, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 254, 8, 223, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 223, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 254, 8, 254, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 223, 8, 32, 8, 32, 8, 220, 8, 220, 8, 223, 8, 32, 8, 223, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 223,
	8, 32, 8, 32, 40, 176, 42, 254, 42, 178, 42, 219, 42, 178, 42, 219, 42, 254, 42, 254,
	8, 223, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 220,
	8, 254, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 223, 8, 254, 8, 220, 8, 254, 8, 223, 8, 223, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 223, 8, 223, 8, 91, 8, 84, 7, 105, 7, 84, 7, 76, 7, 69,
	7, 93, 8, 223, 8, 254, 8, 220, 8, 254, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223,
	8, 223, 8, 91, 8, 80, 7, 65, 7, 105, 7, 78, 7, 84, 7, 69, 7, 82, 7, 93,
	8, 223, 8, 254, 8, 220, 8, 254, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 91,
	8, 71, 7, 82, 7, 79, 7, 85, 7, 80, 7, 93, 8, 223, 8, 254, 8, 254, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 219,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 219, 8, 223,
	8, 254, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 254, 8, 223, 8, 254, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 254, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 254, 8, 223, 8, 254, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 254, 8, 223, 8, 254, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 254, 8, 254, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32, 8, 32,
	7, 32, 7, 32, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 223, 8, 32,
	8, 223, 8, 32, 8, 223, 8, 32, 8, 32, 8, 32, 8, 223, 8, 223, 8, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7};

int             CursX, CursY, b;
unsigned char   avt_color = FALSE, avt_rep = FALSE, avt_command = FALSE,
                c, cc, TxtCol;
unsigned char   pcb_color = FALSE, pcb_code = FALSE, d, pcb_col = 0;
unsigned char   ans_esc = FALSE, ans_code = FALSE, ans_rep, e, ans_attr = 0;
unsigned char   sync_code = FALSE;
int             acx, acy;
char            ansicode[200];
char            copytmp[20];
char            code[20];

char           *
copy(char *str, int aa, int bb)
{
	int             x;
	sprintf(copytmp, "  ");
	for (x = aa; x <= bb; x++) {
		copytmp[x - aa] = str[x];
	}
	return copytmp;
}


unsigned char 
display_ansi(char ch)
{
	int             x, y, b, l;

	if (ans_rep > 0) {
		ans_rep--;
		if (ans_rep == 0)
			return 0;
		return 20;
	}
	if ((ans_esc) & (ch == '[')) {
		ans_esc = FALSE;
		ans_code = TRUE;
		return 0;
	} else
		ans_esc = FALSE;
	if (ans_code) {
		ans_code = FALSE;
		switch (ch) {
		case 'm':
			ans_attr = TxtCol;
			l=strlen(ansicode);
			for (x = 0; x < l; x++)
				if ((ansicode[x] >= '0') | (ansicode[x] <= '9')) {
					b = 0;
					code[b++] = ansicode[x];
					if ((ansicode[x + 1] >= '0') & (ansicode[x + 1] <= '9'))
						code[b++] = ansicode[++x];
					code[b] = 0;
					switch (strtol(code, NULL, 0)) {
					case 0:
						ans_attr = 7;
						break;
					case 1:
						ans_attr = (ans_attr & 247) + 8;
						break;
					case 5:
						ans_attr = (ans_attr & 127) + 128;
						break;
					case 30:
						ans_attr = ans_attr & 248;
						break;
					case 34:
						ans_attr = (ans_attr & 248) + 1;
						break;
					case 32:
						ans_attr = (ans_attr & 248) + 2;
						break;
					case 36:
						ans_attr = (ans_attr & 248) + 3;
						break;
					case 31:
						ans_attr = (ans_attr & 248) + 4;
						break;
					case 35:
						ans_attr = (ans_attr & 248) + 5;
						break;
					case 33:
						ans_attr = (ans_attr & 248) + 6;
						break;
					case 37:
						ans_attr = (ans_attr & 248) + 7;
						break;
					case 40:
						ans_attr = ans_attr & 143;
						break;
					case 44:
						ans_attr = (ans_attr & 143) + 16;
						break;
					case 42:
						ans_attr = (ans_attr & 143) + 32;
						break;
					case 46:
						ans_attr = (ans_attr & 143) + 48;
						break;
					case 41:
						ans_attr = (ans_attr & 143) + 64;
						break;
					case 45:
						ans_attr = (ans_attr & 143) + 80;
						break;
					case 43:
						ans_attr = (ans_attr & 143) + (6 << 4);
						break;
					case 47:
						ans_attr = (ans_attr & 143) + (7 << 4);
						break;
					}
					x++;
				};
			TxtCol = ans_attr;
			return 0;
		case 'H':
			y=0;
			l=strlen(ansicode);
			for (x = 0; x < l; x++)
				if (ansicode[x] == ';')
					y = x;
			CursY = strtol(copy(ansicode, 0, y?(y - 1):0), NULL, 0)-1;
			CursX = strtol(copy(ansicode, y?(y + 1):(strlen(ansicode)-1), strlen(ansicode) - 1), NULL, 0)-1;
			break;
		case 'C':
			ans_rep = strtol(ansicode, NULL, 0) + 1;
			return 0;
		case 'A':
			CursY--;
			break;
		case 'S':
		case 's':
			acx = CursX;
			acy = CursY;
			break;
		case 'U':
		case 'u':
			CursX = acx;
			CursY = acy;
			break;
		case 'J':
			CursX = 0;
			CursY = 0;
			break;
		default:
			sprintf(strchr(ansicode,0), "%c", ch);
			ans_code = TRUE;
			break;
		}
		return 0;
	}
	switch (ch) {
	case '':
		ans_code = FALSE;
		ans_esc = TRUE;
		ansicode[0] = 0;
		return 0;
	default:
		return ch;
	}
}

unsigned char 
display_PCBoard(char ch)
{
	char           c[6];
	if (pcb_color) {
		d++;
		if (d < 3) {
			switch (d) {
			case 1:
				sprintf(c, "0x%c", ch);
				pcb_col = strtol(c, NULL, 0) * 16;
				break;
			case 2:
				sprintf(c, "0x0%c", ch);
				pcb_col += strtol(c, NULL, 0);
				break;
			}
			return 0;
		}
		TxtCol = pcb_col;
		pcb_color = FALSE;
		pcb_code = FALSE;
		return ch;
	}
	d = 0;
	if (pcb_code) {
		switch (ch) {
		case '@':
			pcb_code = FALSE;
			break;
		case 'X':
			pcb_color = TRUE;
			break;
		}
		return 0;
	}
	switch (ch) {
	case '@':
		pcb_code = TRUE;
		break;
	default:
		return ch;
	}
	return 0;
}

unsigned char 
display_avatar(unsigned char ch)
{
	if (avt_rep) {
		switch (b) {
			case 1:
			c = ch;
			b = 2;
			return 0;
		case 2:
			cc = ch;
			b = 3;
			return 0;
		case 3:
			cc--;
			if (cc > 0)
				return c;
			break;
		}
		avt_rep = FALSE;
	}
	b = 0;
	if (avt_color) {
		TxtCol = ch;
		avt_command = FALSE;
		avt_color = FALSE;
		return 0;
	}
	if (avt_command) {
		switch (ch) {
		case 1:
			avt_color = TRUE;
			break;
		default:
			avt_command = FALSE;
		}
		return 0;
	}
	switch (ch) {
	case '':
		avt_rep = TRUE;
		b = 1;
		break;
	case '':
		avt_command = TRUE;
		break;
	default:
		return ch;
	}
	return 0;
}

unsigned char 
display_sync(unsigned char ch)
{
	if (sync_code) {
		switch (toupper(ch)) {
		case 'L':	/* Clear Screen */
			CursX = 0;
			CursY = 0;
			break;
		case 'N':
			ans_attr = 7;
			break;
		case 'H':
			ans_attr = (ans_attr & 247) + 8;
			break;
		case 'I':
			ans_attr = (ans_attr & 127) + 128;
			break;
		case 'K':
			ans_attr = ans_attr & 248;
			break;
		case 'B':
			ans_attr = (ans_attr & 248) + 1;
			break;
		case 'G':
			ans_attr = (ans_attr & 248) + 2;
			break;
		case 'C':
			ans_attr = (ans_attr & 248) + 3;
			break;
		case 'R':
			ans_attr = (ans_attr & 248) + 4;
			break;
		case 'M':
			ans_attr = (ans_attr & 248) + 5;
			break;
		case 'Y':
			ans_attr = (ans_attr & 248) + 6;
			break;
		case 'W':
			ans_attr = (ans_attr & 248) + 7;
			break;
		case '0':
			ans_attr = ans_attr & 143;
			break;
		case '4':
			ans_attr = (ans_attr & 143) + 16;
			break;
		case '2':
			ans_attr = (ans_attr & 143) + 32;
			break;
		case '6':
			ans_attr = (ans_attr & 143) + 48;
			break;
		case '1':
			ans_attr = (ans_attr & 143) + 64;
			break;
		case '5':
			ans_attr = (ans_attr & 143) + 80;
			break;
		case '3':
			ans_attr = (ans_attr & 143) + (6 << 4);
			break;
		case '7':
			ans_attr = (ans_attr & 143) + (7 << 4);
			break;
		}
		if (ch > 126) {
			CursX += ch - 127;
		}
		TxtCol = ans_attr;
		sync_code = FALSE;
		return 0;
	}
	if (ch == 1) {
		sync_code = TRUE;
		ch = 0;
	}
	return ch;
}

unsigned char 
display_ctrl(unsigned char ch)
{
	switch(ch) {
		case 10:	// LF
			CursX = 0;
			CursY++;
			ch=0;
			break;
		case 12:	// FF
			CursX = 0;
			CursY = 0;
			break;
		case 13:	// CR
			CursX = 0;
			ch=0;
			break;
	}
	return ch;
}

void 
LoadFile(char *Name)
{
	FILE           *fp;
	int             ch;
	int				PCB=TRUE;
	int				ANSI=TRUE;
	int				SYNC=TRUE;
	int				AVATAR=TRUE;
	char			*ext;

	erasescreen();
	fp = fopen(Name, "rb");
	if(fp==NULL)
		return;
	CursX = 0;
	CursY = 0;
	TxtCol = 7;
	LastLine = 0;
	ch = strlen(Name) - 3;
	if ((toupper(Name[ch++]) == 'B') && (toupper(Name[ch++]) == 'I') && (toupper(Name[ch]) == 'N')) {
		unsigned char   line[160];
		do {
			fread(&line, 1, 160, fp);
			memcpy(Screen[ActivePage][CursY], &line, 160);
			CursY++;
		} while (!feof(fp));
	} else {
		/* Check for "known" extensions */
		ext=getfext(Name);
		if(ext) {
			if(stricmp(ext,".asc")==0 || stricmp(ext, ".msg")==0) {
				PCB=FALSE;
				ANSI=FALSE;
				AVATAR=FALSE;
			}
			else if(stricmp(ext,".ans")==0) {
				PCB=FALSE;
				AVATAR=FALSE;
				SYNC=FALSE;
			}
			else if(stricmp(ext,".avt")==0) {
				PCB=FALSE;
				ANSI=FALSE;
				SYNC=FALSE;
			}
			else if(stricmp(ext,".pcb")==0) {
				ANSI=FALSE;
				AVATAR=FALSE;
				SYNC=FALSE;
			}
		}

		do {
			ch=fgetc(fp);
			if (ch == 26 && !avt_command && !avt_rep)
				break;
			do {
				if(AVATAR)
					ch=display_avatar(ch);
				if(SYNC)
					ch=display_sync(ch);
				if(ANSI)
					ch=display_ansi(ch);
				if(PCB)
					ch=display_PCBoard(ch);
				ch=display_ctrl(ch);
				if (ch > 0) {
					if (CursX > 79) {
						CursX = 0;
						CursY++;
					}
					if (ans_rep == 0) {
						Screen[ActivePage][CursY][CursX * 2] = ch;
						Screen[ActivePage][CursY][(CursX * 2) + 1] = TxtCol;
					}
					CursX++;
				}
			} while ((avt_rep && b == 3) || (ans_rep > 0));
		} while (!feof(fp));
	}
	ReadSauce(fp);
	fclose(fp);
	LastLine = CursY;
}

void 
load(void)
{
	char            *dirs[2000], isdir[2000];
	char            titel[2000][28], paintrec[2000][18], grouprec[2000][15];
	static int      num = 0;
	int             readnew = 1;
	int             x, y=0, z=0, ch, a, attr;
	FILE           *fp;
	SauceRecord     Tmp;
	struct dirent  *direntp;
	DIR            *dirp;
	struct stat     stats;
	struct			text_info	ti;
	struct			mouse_event	me;
	char		buf[77*9*2];
	int		ansf=0, avtf=0, binf=0, pcbf=0, allf=1;
	char		enabled[2]={'x',15};
	char		disabled[2]={' ',15};
	char		uarrow[2]={24,8};	/* Up arrow */
	char		darrow[2]={25,8};	/* Down arrow */
	char		ublock[2]={223,8};
	char		dblock[2]={220,8};
	char		*ext;

	gettextinfo(&ti);
	clrscr();
	memset(dirs,0,sizeof(dirs));
	puttext(1,1,80,ti.screenheight>24?25:ti.screenheight,(char *)LoadAnsi);
	do {
		if (readnew == 1) {
			z = 0;
			y = 0;
			readnew = 0;
			num = 0;
			for(a=0; a<sizeof(dirs)/sizeof(char *); a++) {
				if(dirs[a] != NULL)
					free(dirs[a]);
			}
			memset(dirs,0,sizeof(dirs));
			dirp = opendir(".");
			/* direntp = readdir(dirp); */
			if (dirp != NULL) {
				for (;;) {
					direntp = readdir(dirp);
					if (direntp == NULL)
						break;
					stat(direntp->d_name, &stats);
					if ((stats.st_mode & S_IFMT) == S_IFDIR) {
						dirs[++num]=strdup(direntp->d_name);
						titel[num][0] = 0;
						paintrec[num][0] = 0;
						grouprec[num][0] = 0;
						isdir[num] = 1;
					}
					else {
						ext=strrchr(direntp->d_name,'.');
						if(ext==NULL)
							continue;
						if(!allf) {
							int	matched=0;

							if(ansf) {
								if(!stricmp(ext,".ans"))
									matched=1;
							}
							if(avtf) {
								if(!stricmp(ext,".avt"))
									matched=1;
							}
							if(binf) {
								if(!stricmp(ext,".bin"))
									matched=1;
							}
							if(pcbf) {
								if(!stricmp(ext,".pcb"))
									matched=1;
							}
							if(!matched)
								continue;
						}
						dirs[++num]=strdup(direntp->d_name);
						titel[num][0] = 0;
						paintrec[num][0] = 0;
						grouprec[num][0] = 0;
						isdir[num] = 0;
						fp = fopen(direntp->d_name, "rb");
						if(fp!=NULL) {
							fseek(fp, -128, SEEK_END);
							fread(&Tmp.id, 5, 1, fp);
							if ((Tmp.id[0] == 'S') & (Tmp.id[1] == 'A') &
							    (Tmp.id[2] == 'U') & (Tmp.id[3] == 'C') & (Tmp.id[4] == 'E')) {
								fread(&Tmp.Version, 2, 1, fp);
								fread(&Tmp.Title, 1, 35, fp);
								sprintf(titel[num], "%-27s", Tmp.Title);
								titel[num][27] = 0;
								fread(&Tmp.Author, 1, 20, fp);
								sprintf(paintrec[num], "%-17s", Tmp.Author);
								paintrec[num][17] = 0;
								fread(&Tmp.Group, 1, 20, fp);
								sprintf(grouprec[num], "%-14s", Tmp.Group);
								grouprec[num][14] = 0;
								fclose(fp);
							}
						}
					}
				}
			}
			closedir(dirp);
		}
		memset(buf,0,sizeof(buf));
		for (x = 0; x <= 8; x++) {
			if (dirs[1 + x + z] != NULL && 1 + x + z <= num) {
				if (y == x)
					attr=(15+16);
				else
					attr=15;
				if (isdir[1 + x + z] == 1) {
					if (y == x)
						attr=7 + 16;
					else
						attr=7;
				}
				bufprintf(buf+x*77*2,attr,"%-12.12s ", dirs[1 + x + z]);
				if (isdir[1 + x + z] == 0) {
					if (y == x)
						attr=(7 + 16);
					else
						attr=(7);
					buf[(x*77+13)*2]='\xfa';
					buf[(x*77+13)*2+1]=attr;
					if (y == x)
						attr=(15 + 16);
					else
						attr=(15);
					bufprintf(buf+(x*77+14)*2,attr," %-27.27s", titel[1 + x + z]);
					if (y == x)
						attr=(7 + 16);
					else
						attr=(7);
					buf[(x*77+42)*2]='\xfa';
					buf[(x*77+42)*2+1]=attr;
					if (y == x)
						attr=(15 + 16);
					else
						attr=(15);
					bufprintf(buf+(x*77+43)*2,attr," %-17.17s", paintrec[1 + x + z]);
					if (y == x)
						textattr(7 + 16);
					else
						textattr(7);
					buf[(x*77+61)*2]='\xfa';
					buf[(x*77+61)*2+1]=attr;
					if (y == x)
						attr=(15 + 16);
					else
						attr=(15);
					bufprintf(buf+(x*77+62)*2,attr," %-14.14s", grouprec[1 + x + z]);
				} else {
					if (y == x)
						attr=(8 + 16);
					else
						attr=(8);
					bufprintf(buf+(x*77+13)*2,attr,"  <DiRECTORY>                                                   ");
				}
			} else {
				attr=8;
				bufprintf(buf+(x*77)*2,attr,"                                                                             ");
			}
		}
		puttext(3,14,79,22,buf);
		puttext(5,13,5,13,z==0?ublock:uarrow);
		puttext(5,23,5,23,(num > 9 && z == num - 9)?dblock:darrow);
		puttext(12,13,12,13,z == 0?ublock:uarrow);
		puttext(12,23,12,23,(num > 9 && z == num - 9)?dblock:darrow);
		ch = newgetch();
		if(ch==CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.event == CIOLIB_BUTTON_3_CLICK)
				ch = 27;
			if(me.endx >= 3 && me.endx <= 79
					&& me.endy >= 13 && me.endy <= 23) {
				if(me.endy >= 14 && me.endy <= 22) {
					y = me.endy - 14;
					if (me.event == CIOLIB_BUTTON_1_CLICK)
						ch=13;
				}
				if(me.event == CIOLIB_BUTTON_1_CLICK) {
					if(me.endy == 13)
						ch = CIO_KEY_PPAGE;
					if(me.endy == 23)
						ch = CIO_KEY_NPAGE;
				}
			}
			if(me.endx >= 61 && me.endx <= 69 && me.endy >= 6 && me.endy <= 10 
					&& me.event == CIOLIB_BUTTON_1_CLICK) {
				switch(me.endy) {
				case 6:
					ansf = !ansf;
					puttext(62,6,62,6,ansf?enabled:disabled);
					readnew=1;
					break;
				case 7:
					avtf = !avtf;
					puttext(62,7,62,7,avtf?enabled:disabled);
					readnew=1;
					break;
				case 8:
					binf = !binf;
					puttext(62,8,62,8,binf?enabled:disabled);
					readnew=1;
					break;
				case 9:
					pcbf = !pcbf;
					puttext(62,9,62,9,pcbf?enabled:disabled);
					readnew=1;
					break;
				case 10:
					allf = !allf;
					puttext(62,10,62,10,allf?enabled:disabled);
					readnew=1;
					break;
				}
			}
		}
		switch (ch) {
		case 13:
			if (isdir[1 + y + z] == 1) {
				chdir(dirs[1 + y + z]);
				readnew = 1;
			} else {
				LoadFile(dirs[1 + y + z]);
				ch = 27;
			}
			break;
		case CIO_KEY_PPAGE:
			y -= 8;
			if (y < 0) {
				z += y;
				y = 0;
			}
			break;
		case CIO_KEY_NPAGE:
			y += 8;
			if(y > 8) {
				z += y-8;
				y = 8;
			}
			break;
		case CIO_KEY_UP:
			y--;
			if (y < 0) {
				y = 0;
				if (z > 0)
					z--;
				else {
					y=8;
					z=num-9;
				}
			}
			break;
		case CIO_KEY_DOWN:
			y++;
			if (z + y == num) {
				y = 0;
				z = 0;
			}
			else if (y > 8) {
				y = 8;
				if (z + 9 < num)
					z++;
			}
			break;
		}
		if(y<0)
			y=0;
		if(y>8)
			y=8;
		if (y + 1 > num)
			y = num - 1;
		if(z>num-9)
			z=num-9;
		if(z<0)
			z=0;
	} while (ch != 27);
	for(a=0; a<sizeof(dirs)/sizeof(char *); a++) {
		if(dirs[a] != NULL)
		free(dirs[a]);
	}
}
