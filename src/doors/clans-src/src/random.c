#include <limits.h>
#include <stdlib.h>

#include "console.h"
#include "random.h"

/*
 * There's two historical problems with rand()...
 * 1) Doing rand() % x give modulo bias (fairly easy to avoid)
 * 2) The low bits of rand() have been complete garbage and very
 *    predictable (harder to avoid)
 */
int my_random(int limit)
{
	unsigned ulimit = limit - 1;
	// -1 for the sign bit, -1 because it's base 0
	unsigned maxbit = sizeof(int) * CHAR_BIT - 2;
	unsigned mask = 0;
	unsigned val;
	int found = 0;
	int shift;

	// Don't mess around with garbage
	if (limit <= 0)
		System_Error("Bad value passed to my_random()");
	if (limit == 1)
		return 0;
	if (limit >= RAND_MAX)
		System_Error("Broken Random (range too small)!");

	/*
	 * Walk through all the bits from highest to lowest.
	 * Once we find the highest set bit in limit, set every bit below
	 * it in mask.
	 */
	for (int bit = maxbit; bit >= 0; bit--) {
		if (ulimit & (1 << bit))
			found = bit + 1;
		if (found)
			mask |= (1 << bit);
	}

	/*
	 * So we now have a mask of a value that can hold limit.
	 * Next up, we need to find the highest value less than RAND_MAX
	 * that can hold the mask.
	 */
	for (shift = maxbit - (found - 1); shift >= 0; shift--) {
		if ((mask << shift) <= RAND_MAX)
			break;
	}
	if (shift < 0)
		System_Error("Broken Random (impossible mask)!");
	mask <<= shift;
	do {
		val = (rand() & mask) >> shift;
	} while(val > ulimit);
	return val;
	
}
