#ifndef SYNCMOO1_MSVC_COMPAT_H_
#define SYNCMOO1_MSVC_COMPAT_H_

/*
 * Force-included (/FI) into every translation unit of the `syncmoo1` target
 * under MSVC -- see CMakeLists.txt. Not on any other toolchain, and not into
 * the xpdev / termgfx sub-targets, which are built separately.
 *
 * This exists for exactly one vendored construct, and is a build-side shim so
 * that the frozen 1oom/ tree stays unedited (CLAUDE.md's ours-vs-vendored
 * contract):
 *
 *   1oom/src/log.h:14 declares
 *       extern void log_fatal_and_die(const char *, ...) __attribute__((noreturn));
 *   MSVC cannot parse GCC's __attribute__ at all, so every TU that includes
 *   log.h -- which is most of the engine -- fails with C2061/C2059.
 *
 * Defining it away costs only the `noreturn` optimization hint. It cannot leak
 * into xpdev's own __attribute__((format(printf,...))) uses, which are already
 * `#if defined(__GNUC__)` - gated and so invisible to MSVC anyway.
 *
 * This is deliberately NOT done with /D: MSVC's command-line /D cannot define a
 * function-like macro (`/D"__attribute__(x)="` is accepted and then ignored),
 * which is why the neutralization has to live in a header.
 */

#ifndef _MSC_VER
#error "compat/msvc_compat.h is the MSVC-only force-include; other toolchains must not see it"
#endif

#define __attribute__(x)

#endif /* SYNCMOO1_MSVC_COMPAT_H_ */
