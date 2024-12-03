#ifndef DEUCE_SSH_PLATWRAP_H
#define DEUCE_SSH_PLATWRAP_H

/*
 * This file is here to hold all the ugly compiler and platform
 * "stuff" that ends up being needed.
 */

#ifdef __has_c_attribute
 #if __has_c_attribute(maybe_unused)
  #define MAYBE_UNUSED [[maybe_unused]]
 #endif
#endif

#ifndef MAYBE_UNUSED
 #define MAYBE_UNUSED
#endif

#endif
