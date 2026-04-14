/* mozjs_stubs.cpp
 * Stubs for mozjs-128 symbols normally provided by mozglue.dll.
 * We cannot use mozglue.dll because it hooks the global malloc/free with
 * jemalloc on load, which corrupts heap state when loaded after the CRT.
 *
 * InvalidArrayIndex_CRASH: called from mozilla::Array<> bounds checks.
 * moz_arena_malloc/free/realloc/calloc: jemalloc arena allocator; we fall
 *   back to the CRT allocator since we don't need separate arenas.
 *
 * /alternatename directives redirect the __declspec(dllimport) __imp_* thunks
 * to our local definitions so no headers need to be modified.
 */

#include <stdlib.h>

namespace mozilla {
namespace detail {
[[noreturn]] void __cdecl InvalidArrayIndex_CRASH(unsigned int, unsigned int) {
	abort();
}
} /* namespace detail */
} /* namespace mozilla */

extern "C" {
void* __cdecl moz_arena_malloc(size_t /*arena*/, size_t bytes) {
	return malloc(bytes);
}
void* __cdecl moz_arena_calloc(size_t /*arena*/, size_t num, size_t size) {
	return calloc(num, size);
}
void* __cdecl moz_arena_realloc(size_t /*arena*/, void* ptr, size_t bytes) {
	return realloc(ptr, bytes);
}
void  __cdecl moz_arena_free(size_t /*arena*/, void* ptr) {
	free(ptr);
}
} /* extern "C" */

#pragma comment(linker, "/alternatename:__imp_?InvalidArrayIndex_CRASH@detail@mozilla@@YAXII@Z=?InvalidArrayIndex_CRASH@detail@mozilla@@YAXII@Z")
#pragma comment(linker, "/alternatename:__imp__moz_arena_malloc=_moz_arena_malloc")
#pragma comment(linker, "/alternatename:__imp__moz_arena_calloc=_moz_arena_calloc")
#pragma comment(linker, "/alternatename:__imp__moz_arena_realloc=_moz_arena_realloc")
#pragma comment(linker, "/alternatename:__imp__moz_arena_free=_moz_arena_free")
