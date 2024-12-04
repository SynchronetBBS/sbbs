#ifndef DEUCE_SSH_PLATWRAP_H
#define DEUCE_SSH_PLATWRAP_H

#include <stddef.h>

/*
 * This file is here to hold all the ugly compiler and platform
 * "stuff" that ends up being needed.
 */

// C23
#ifdef __has_c_attribute
	#if __has_c_attribute(maybe_unused)
		#define MAYBE_UNUSED [[maybe_unused]]
	#endif
#endif

#ifndef MAYBE_UNUSED
	#define MAYBE_UNUSED
#endif

// C23
#ifndef unreachable
	#ifdef __GNUC__
		#define unreachable() (__builtin_unreachable())
	#elif defined _MSC_VER
		#define unreachable() (__assume(false))
	#endif
#endif

#endif
