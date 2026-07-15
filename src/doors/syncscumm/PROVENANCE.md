# SyncSCUMM provenance

## Vendored engine: ScummVM

- Upstream: https://github.com/scummvm/scummvm
- Version: tag v2026.3.0, commit fed42f2068dcafc6aafa1c28c77e4c88def74b66
- Fetched: GitHub release tarball, sha256
  54ec34519be9edb24f952afda9deb9a49d5ace35c3539feaa654a09fb08ce81a
- License: GPLv2+ (see scummvm/COPYING; compatible with this repo's use)

## Pruning (deletions only -- no upstream file is modified)

- engines/: all engine subdirectories except scumm, sky, queen, lure,
  drascula (engines/ root sources kept -- they are core).
- backends/platform/: all except null.
- dists/: all except engine-data, itself pruned to the curated engines'
  runtime files (sky.cpt, lure.dat, drascula.dat, queen.tbl, README).
- devtools/, doc/, po/, test/: removed.

Re-adding an engine = restore its engines/<id>/ dir and any
dists/engine-data file from the pinned tarball, add it to the configure
--enable-engine list in build.sh, and door-test the title(s).

## Local additions to the vendored tree

- backends/platform/synchronet -- a directory SYMLINK to ../../../door
  (all code we author lives outside the vendored tree in door/).
- configure: one added case in the backend switch (search "synchronet").
  This is the only modified upstream file.
