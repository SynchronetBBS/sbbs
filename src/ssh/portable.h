#ifndef DEUCE_SSH_PLATWRAP_H
#define DEUCE_SSH_PLATWRAP_H

#include <stddef.h>

/*
 * This file is here to hold all the ugly compiler and platform
 * "stuff" that ends up being needed.
 */

// C23
#ifndef unreachable
	#ifdef __GNUC__
		#define unreachable() (__builtin_unreachable())
	#elif defined _MSC_VER
		#define unreachable() (__assume(false))
	#endif
#endif

#endif
