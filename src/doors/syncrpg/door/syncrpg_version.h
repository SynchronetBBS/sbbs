/* SyncRPG -- the door's own build-identity string. GPLv3+, like the EasyRPG
 * tree.
 *
 * Kept in its own header because it is the one door symbol a VENDORED engine
 * source calls: easyrpg/src/scene_logo.cpp draws it on the startup splash
 * (PROVENANCE "Local patches" #1). Pulling in a fatter door header there would
 * drag termgfx/xpdev declarations into an engine translation unit for no
 * reason -- and the two trees have colliding short header names (see the
 * include-order note in ../CMakeLists.txt), so the less door/ that engine
 * sources see, the better. syncrpg/CMakeLists.txt puts door/ on the include
 * path for that ONE engine source.
 */
#ifndef SYNCRPG_VERSION_H_
#define SYNCRPG_VERSION_H_

/* The door's version, short git hash, build date and site, e.g.
 * "v0.1 ~9378ed4ee2 Jul 23 2026 synchro.net" -- the version/git/date/site
 * stamp the sibling termgfx doors paint on their menus (SyncDOOM's m_menu.c,
 * SyncDuke's menues.c, SyncRetro's main.c, syncmoo1's sm_door_version_line()).
 * The hash and date come from the generated git_hash.h; a leading '~' on the
 * hash is gitinfo.cmake's dirty-tree marker ("based on this commit, plus
 * uncommitted work"), in which case the date is the BUILD time rather than the
 * commit's.
 *
 * Single-spaced, unlike the siblings' double, to fit the splash row it shares
 * with EasyRPG's own version -- see the width arithmetic in syncrpg.cpp.
 * Returns a static buffer built on first call; the contents never change
 * afterwards. */
const char *syncrpg_version_line();

#endif /* SYNCRPG_VERSION_H_ */
