# Vendored source provenance

## vanilla/ — Vanilla Conquer

- Upstream: https://github.com/TheAssemblyArmada/Vanilla-Conquer
- Commit: 7f351daed0c19d7c4764dc4ebae1a70c7809ac1f (2025-11-14)
- License: GPLv3 with EA additional terms (vanilla/License.txt)
- Vendored subset: CMakeLists.txt, CMakePresets.json, License.txt,
  README.md, cmake/, common/, redalert/, resources/
- Not vendored: tiberiandawn/ (deferred syncdawn title), tests/, tools/,
  scripts/, .github/

## Local patches (keep this list complete)

1. `common/CMakeLists.txt`: add `wwkeyboard_null.cpp` to the
   no-video-backend source list (upstream has no keyboard backend for
   headless builds; vanillara otherwise fails to link).
2. `common/wwkeyboard_null.cpp`: NEW FILE — null keyboard backend
   (template for the M2 terminal keyboard shim).
3. `CMakeLists.txt`: guard `add_subdirectory(tiberiandawn)` with
   `BUILD_VANILLATD OR BUILD_REMASTERTD` and `add_subdirectory(tools)`
   with `BUILD_TOOLS`, so the vendored subset configures.

## Updating

Re-vendor by diffing upstream at a new commit against this subset,
re-applying the local patches above, and updating the commit hash here.
