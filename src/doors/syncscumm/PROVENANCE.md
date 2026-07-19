# SyncSCUMM provenance

## Vendored engine: ScummVM

- Upstream: https://github.com/scummvm/scummvm
- Version: tag v2026.3.0, commit fed42f2068dcafc6aafa1c28c77e4c88def74b66
- Fetched: GitHub release tarball, sha256
  54ec34519be9edb24f952afda9deb9a49d5ace35c3539feaa654a09fb08ce81a
- License: GPLv2+ (see scummvm/COPYING; compatible with this repo's use)

## Pruning (deletions only -- no upstream file is modified)

- engines/: all engine subdirectories except scumm, sky, queen, lure,
  drascula, agi, sci (engines/ root sources kept -- they are core). agi and
  sci were restored from the pinned tarball after the initial prune; sci
  carries its built-by-default sci32 sub-engine (SCI2-3). Neither needs a
  dists/engine-data file -- both read the game's own resource files.
- backends/platform/: all except null.
- dists/: all except engine-data (pruned to the curated engines' runtime
  files: sky.cpt, lure.dat, drascula.dat, queen.tbl, README) and the three
  engine_data*.mk manifests (see "restored for the Windows build" below).
- doc/, po/, test/: removed.
- devtools/: all removed EXCEPT devtools/create_project (restored for the
  Windows build -- see below).

Re-adding an engine = restore its engines/<id>/ dir and any
dists/engine-data file from the pinned tarball, add it to the configure
--enable-engine list in build.sh AND the create_project --enable-engine list
in build.bat, and door-test the title(s). After pruning, remove now-empty
directories: `find dists/engine-data -mindepth 1 -type d -empty -delete`.

## Local additions to the vendored tree

- backends/platform/synchronet -- a directory SYMLINK to ../../../door (all
  code we author lives outside the vendored tree in door/). Git stores it with
  mode 120000; on a Windows checkout (core.symlinks=false) it materializes as a
  13-byte text file, so build.bat replaces it with a directory JUNCTION to
  door/ at build time (and `git update-index --skip-worktree`s it so the
  junction doesn't show as a change). build.sh relies on the real symlink.

## Modified upstream files

Three upstream files are modified; everything else is deletions only.

- configure: one added case in the backend switch (search "synchronet") --
  the *nix (gcc/make) build's backend selection.
- backends/module.mk: the WIN32 block's SDL/win32 platform files (audio-CD,
  MIDI, printing, saves, dialogs, taskbar, updates, plugin provider -- all of
  which #include backends/platform/sdl/win32/win32_wrapper.h, pruned with the
  SDL backend) are gated behind `ifdef SDL_BACKEND`, leaving only the
  SDL-independent fs/windows files unconditional under WIN32. Needed because the
  door defines WIN32 (for backends/fs/windows) but not SDL_BACKEND; upstream
  never has that combination. Search "SyncSCUMM" in the file.
- devtools/create_project/: ScummVM's own MSVC project generator, RESTORED
  from the pinned tarball (Windows has no gcc/configure path, so build.bat
  drives create_project instead). Patched with a `--synchronet` option that
  emits our backends/platform/synchronet module in place of the SDL platform
  backend and defines USE_SYNCHRONET_DRIVER instead of SDL_BACKEND -- the MSVC
  analogue of configure's `synchronet)` case (search "SyncSCUMM local patch"
  in create_project.cpp / .h).

## Restored for the Windows build (from the pinned tarball)

- dists/engine-data/engine_data.mk, engine_data_big.mk, engine_data_core.mk --
  create_project reads these to list the bundled engine-data files; they were
  removed by the dists/ pruning and are restored verbatim (unmodified).
- backends/platform/sdl/win32/win32_wrapper.{h,cpp} -- a Win32-namespace
  Unicode/path helper. Despite its path it uses no SDL; core backend files
  (fs/stdiostream.cpp, backends/fs/windows/*) #include it unconditionally on
  WIN32, so it is restored (the only two files kept from the pruned SDL
  platform) and added to backends/module.mk's WIN32 block. Restored verbatim.
