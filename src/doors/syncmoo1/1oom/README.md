This page is no longer maintained as of April 2026. The project has moved to [sourcecraft](https://sourcecraft.dev/fork1oom/1oom).

The owner of this account (MyName aka 1oom-fork aka Duzh87_54MSU) uses the following pages
for further publications: [Mirrors](MIRRORS.md).

1oom
====

DISCLAIMER: All versions of 1oom published before 2026, including the original 1.0 release from 2018, may be misleading regarding deviations from the original.
It is recommended to use the documentation as a guide, not as a 100% reliable source of information.

1oom aims to accurately reproduce the original DOS version of Master of Orion (1993) in a form that can be run on modern computers.
1oom is Free Software (GPLv2), see [COPYING](COPYING).

Versions [1.0 and earlier](https://gitlab.com/KilgoreTroutMaskReplicant/1oom) contain hundreds of obvious inconsistencies with the original logic and even fatal errors in the code.
WARNING: 1.0 is not the vanilla version.

Versions 2019-2021 (Tapani's fork) breaks numerous vanilla mechanics, including the interface. It's one of the most unreliable versions of the game.

Versions 1.7.6, 1.8.1-1.11.8 ([MyName's fork](https://github.com/1oom-fork/1oom)) cleans up Tapani's fork, fixes most of the most notable bugs, but not all, and also changes some interface elements.

Version 2.0 ([MyName's vanilla](https://sourcecraft.dev/fork1oom/1oom)) aims to accurately reproduce the original artwork created in 1993.

An analysis of unofficial patches for the original game shows the need for a more responsible approach.
I'm working on creating a separate patch 1.3a for the original MOO1 game based on 1oom vanilla.
This patch is inextricably linked to 1oom vanilla and will be published together with it.

1 Installation
===============

1oom requires a copy of the Master of Orion (v1.3) LBX files.
Please note that older saved games may not work in newer versions of 1oom.

1.1 Windows, MSDOS
------------------

Simply extract the desired release to your MOO1 directory.
See [usage_common](doc/usage_common.txt) if you prefer to use a separate directory.

1.2 Unix (Linux)
----------------

1oom requires the following libraries:

- SDL (libsdl1.2 or libsdl2):
http://www.libsdl.org
- SDL_mixer (libsdl-mixer1.2 or libsdl2-mixer):
http://www.libsdl.org/projects/SDL_mixer/

Also recommended:

- libsamplerate:
http://www.mega-nerd.com/libsamplerate/

Check your distribution's package manager or the library
website on how to install them.

1.3 Android (DOSBox)
--------------------

To play on Android you will need the 1oom executable for MSDOS,
DOSBox application for Android and DPMI extension, for example
CWSDPMI. You can build the 1oom executable for MSDOS using i586
DJGPP and Allegro4.


2 Configuration
===============

See [usage_common](doc/usage_common.txt).


3 Development
=============

See [COMPILING](COMPILING) and [HACKING](HACKING) for more information.


4 Executables
=============

1oom consists of a few executables:

- [1oom_classic_*](doc/usage_classic.txt)    (the 1993 UI)
- [1oom_cmdline](doc/usage_cmdline.txt)      (proof of concept textual UI)
- [1oom_lbxview_*](doc/usage_lbxview.txt)    (for viewing LBX files)
- [1oom_lbxedit](doc/usage_lbxedit.txt)      (for editing LBX files)
- [1oom_pbxmake](doc/usage_pbxmake.txt)      (for creating PBX files)
- [1oom_pbxdump](doc/usage_pbxdump.txt)      (for dumping PBX files)
- [1oom_gfxconv](doc/usage_gfxconv.txt)      (for converting GFX for use in PBX files)
- [1oom_saveconv](doc/usage_saveconv.txt)     (for converting save game files)
- 1oom_*_sdl1       (using SDL 1.2.x)
- 1oom_*_sdl2       (using SDL 2.x)
- 1oom_*_alleg4     (using Allegro 4.x)

See [usage_common](doc/usage_common.txt) for common documentation.

Some ports have shorter filenames due to filesystem limitations.


5 Abbreviations
===============

The following abbreviations appear in the documentation and code:

- MOO1: Master of Orion 1 as represented by the v1.3 DOS binaries
- OSG: The Official Strategy Guide (ISBN 1-55958-507-2)


6 Acknowledgements
==================

Most of the credit for this software belongs to the progammer who authored
[1oom v1.0](https://kilgoretroutmaskreplicant.gitlab.io/plain-html) 
under the pseudonym Kilgore Trout Mask Replicant. Thank you for
creating this and publishing it free and open source!

Thanks to MyName aka Duzh87_54MSU for restoring 1oom from its broken state
and for creating [1oom v2.0 (vanilla)](https://sourcecraft.dev/fork1oom/1oom).

The original game Master of Orion was developed  by Simtex Software and
published 1993 by MicroProse. Thanks for the great game!

Thanks to Alan Emrich and Tom Hughes for documenting the game mechanics and AI
decision making in great detail in the official strategy guide.

Ideas and text snippets have been taken from kyrub's unofficial patch 1.40m
Readme. Thanks for the patch!

Thanks to [shikadi.net](http://www.shikadi.net) for documenting the
[music format](http://www.shikadi.net/wiki/modding/index.php?title=XMI_Format&oldid=6874).
https://moddingwiki.shikadi.net/wiki/XMI_Format

Thanks to CivFanatics forum user sargon0 for
[partial save game format info](http://forums.civfanatics.com/threads/moo-save-file-layout.275055/).

Special thanks to Zachary Kline (BlindGuyNW) for improving the text version of the game.

Thanks to those who contributed code, ideas or bug reports.

Some code has been pilfered from Chocolate Doom and VICE.

The files [HACKING](HACKING) and [PHILOSOPHY](PHILOSOPHY) are based on Chocolate Doom.
