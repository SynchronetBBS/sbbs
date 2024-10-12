# Building Synchronet

## Source code
The vast majority of Synchronet C/C++ source files (including header files) are stored in in the `src` and `3rdp` directories of the repository.

### Generated source files:
- src/sbbs3/git_*.h
- src/sbbs3/text.h
- src/sbbs3/text_defaults.c

## Output files
The majority of Synchronet projects are cross-platform (binaries targetting multiple platforms are built from the same source files), so the generated output (e.g. object, library, executable) files are placed in sub-directories that attempt to uniquely identify the compiler and target platform. For example
- `*/msvc.win32.exe.debug/`
- `*/gcc.linux.x64.lib.release/`

Intermediary (e.g. object) files are stored in separate directories from generated executable and library files.

## Third Party Dependencies
Synchronet includes "vendored" 3rd party libraries:
- cryptlib (for SSH and TLS/SSL support)
- libmozjs (for JavaScript support)

These libraries are automatically extracted (from `3rdp/dist/*`), patched, and built as part of the Synchronet build process for UNIX.

## External Programs
Synchronet includes some external programs (primarily, BBS door games) - the building of which is not documented here:
- xtrn/*
- src/odoors/
- src/doors/*

## Cruft
Ignore the CMake and CodeBlocks related build files: they're not currently used or supported.

## Target-specific Build Instructions
<details><summary>UNIX</summary>

### Prerequisites
Building for UNIX-like operating systems (including macOS), requires:
- GNU make
- GNU C/C++ Compiler or Clang C/C++ Compiler
- libarchive library and headers
- ncurses library and headers
- pkgconf
- unzip
- patch
- Perl
- Python
- Netscape Portable Runtime Library and headers

Optionally:
- systemd library and headers
- Mosquitto (MQTT) library and headers
- Xorg library and headers
- SDL2 library and headers
- GTK+ library and headers
- GTK+ User Interface Builder (GLADE) library and headers

### Instructions
The `install/install-sbbs.mk` file is normally used to both build and install Synchronet on UNIX-like systems in a single operation.

`$ make -f path/to/install/install-sbbs.mk SYMLINK=1`

If you to build without installing or want to rebuild (e.g. after pulling upstream changes), then running `make` in the `src/sbbs3` directory is usually sufficient:

`$ make -C path/to/src/sbbs3`

The `src/sbbs3/GNUmakefile` is a parent Makefile that, by default, also builds projects in sub-directories of `src/sbbs3`:
- `scfg/`
- `uedit/`
- `umonitor/`

To build the GTK (GUI) sysop utilities, specify the `gtkutils` target on the make command-line:
$ make -C path/to/src/sbbs3 gtkutils

Persistent and global build options (e.g. `RELEASE=1` to specify a release instead of a debug build) may be specified in the `src/build/localdefs.mk` file.

It is customary to symolically link (symlink) Synchronet libaries and executables from the Synchronet `exec` directory to their respective build output directories (e.g. `gcc.linux.x64.exe.release/*`). To update symlinks, include the `symlinks` target on the `make` command-line.

Run `make help` or see `GNUmakefile` (in the `src/sbbs3` directory)  for a complete list of supported build options and targets.

</details>

<details><summary>Windows</summary>

Please note that prebuilt (releases and nightly development builds) for Windows are available for Windows and usually preferred by sysops.

## 32-bit Only
Although x64 versions of Windows can and usually are used to build Synchronet, we only build/support 32-bit Windows executables at this time.

## Third Party
Synchronet includes "vendored" 3rd party libraries pre-built for Windows:
- cryptlib (for SSH and TLS/SSL support)
- libmozjs and nspr (for JavaScript support)
- libarchive and zlib (for internal archive/zip file support)
- mosquitto (for MQTT support)
- sdl2 (for graphics and sound support in UIFC apps)

### Prerequisites
Building for Windows requires:
- Microsoft Visual Studio and Visual C++
- The `tr` GNU/UNIX utility

Optionally:
- Borland C++Builder 6 for the Win32 GUI components, e.g. `sbbsctrl.exe`
- Microsoft Visual C++ v1.52 for `dosxtrn.exe` (16-bit portion of the Synchronet FOSSIL driver)

### Instructions
1. Open `src/sbbs3/sbbs3.sln` in Microsoft Visual Studio and select _Build Solution_.
2. Alternatively, run `src/sbbs3/build.bat` in a _Developer Command Prompt_ Windows (targetting x86 builds), generating debug (instead of release) output files, by default.
3. Copy the successfully built output files (`*.exe`, `*.dll`) from the various `msvc.win32.*` sub-directories to your Synchronet `exec` directory.

Optionally (for Borland-built GUI components):
1. Run Borland `make` in the `src/xpdev/` directory
2. Run `build.bat` in the `src/sbbs3/` sub-directories: `ctrl`, `chat`, and `useredit`
3. Copy the successfully built `*.exe` files to your Synchronet `exec` directory.

</details>
