# Keeping the OpenDoors Repositories in Sync

This directory is Synchronet's bundled copy of OpenDoors. The standalone
checkout at `~/src/OpenDoors` tracks the GitHub repository at
`https://github.com/RealDeuce/OpenDoors`. Run the commands below from the
Synchronet repository root.

The standalone checkout is read-only during a Synchronet sync. Do not fetch,
pull, switch branches, edit files, or otherwise change it from this workflow.
If it needs updating, update it separately first, then begin the sync using
whatever revision is already checked out.

## Current synchronization point

The non-excluded content in this directory is synchronized through standalone
OpenDoors commit `b784af760f7437b8fdb84b49ceabbd1673f77545`.

Update this revision whenever another sync is committed.

## Excluded and Synchronet-only content

Never propagate these paths or changes from the standalone repository:

- `.github/`
- `.gitignore`
- `gitignore`, the legacy un-dotted ignore file

Synchronet also retains generated artifacts even when the standalone
repository deletes or ignores them. This includes:

- `ODRes.aps`
- `ODoorW.lib`
- `ODoors62.dll`
- `exe-*`, `libs-*`, and `objs-*` directories

The bundled copy also carries one integration adaptation in `odoors.props`:
Win32 builds prefer the retained root `ODoorW.lib` when it exists, before
falling back to the standalone repository's CMake output directory. Preserve
that conditional when importing future changes to the property sheet.

Do not use a mirroring command with deletion enabled. In particular, an
unqualified `rsync --delete` would remove files that belong only to the
Synchronet copy.

## Sync procedure

1. Record the standalone revision and confirm its worktree state without
   changing it:

   ```sh
   git -C ~/src/OpenDoors status --short --branch
   git -C ~/src/OpenDoors rev-parse HEAD
   ```

   Untracked or ignored build output is not source input. Import only files
   tracked by the standalone repository.

2. Start from a clean `src/odoors` worktree, or commit any pending OpenDoors
   work before importing another snapshot:

   ```sh
   git status --short -- src/odoors
   ```

3. Review the standalone changes since the revision recorded above:

   ```sh
   git -C ~/src/OpenDoors diff --name-status \
       <previous-revision>..HEAD
   ```

   Drop `.github`, both ignore files, and generated-artifact deletions from
   the import. Review renames and deletions individually instead of copying
   the whole tree.

4. Copy or apply the remaining tracked source changes into `src/odoors`.
   Preserve file modes and exact byte content. Do not copy `.git`, untracked
   files, ignored build trees, or local generated output from the standalone
   checkout.

5. Compare the imported files with their standalone blobs. Except for the
   documented `odoors.props` adaptation, these two hashes should match for
   each imported path:

   ```sh
   git hash-object src/odoors/<path>
   git -C ~/src/OpenDoors rev-parse HEAD:<path>
   ```

6. Confirm that Synchronet-only content was not changed or removed:

   ```sh
   git diff -- src/odoors/.gitignore \
       src/odoors/ODRes.aps \
       src/odoors/ODoorW.lib \
       src/odoors/ODoors62.dll
   test ! -e src/odoors/.github
   ```

7. Build and test out of tree so verification does not alter retained
   artifacts:

   ```sh
   cmake -S src/odoors -B /tmp/odoors-cmake-check \
       -DOPENDOORS_BUILD_EXAMPLES=OFF
   cmake --build /tmp/odoors-cmake-check
   ctest --test-dir /tmp/odoors-cmake-check --output-on-failure
   git diff --check -- src/odoors
   ```

8. Update the synchronization revision in this document and commit the import.
   Include the standalone revision in the commit message or body so the next
   sync has an unambiguous starting point.

When changes need to flow in the other direction, make them in the standalone
repository through its normal development process. After they are committed
there, use this procedure to import them back into Synchronet.
