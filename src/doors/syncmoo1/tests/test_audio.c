#include <assert.h>
#include <string.h>
#include "hash.h"   /* termgfx: termgfx_fnv1a */

int main(void)
{
    /* Canonical FNV-1a 32-bit vectors (offset basis 0x811c9dc5, prime 0x01000193). */
    assert(termgfx_fnv1a("", 0)          == 0x811c9dc5u);
    assert(termgfx_fnv1a("a", 1)         == 0xe40c292cu);
    assert(termgfx_fnv1a("foobar", 6)    == 0xbf9cf968u);

    /* Binary-safe: embedded NULs are hashed, not treated as terminators. */
    assert(termgfx_fnv1a("a\0b", 3) != termgfx_fnv1a("a\0c", 3));

    /* Same bytes -> same hash (the property content-addressing depends on). */
    {
        const char v[] = "Creative Voice File\x1a";
        assert(termgfx_fnv1a(v, sizeof v - 1) == termgfx_fnv1a(v, sizeof v - 1));
    }
    return 0;
}
