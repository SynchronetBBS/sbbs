# Working in syncdrive

## Ours vs vendored

`tdport/` is a frozen copy of upstream (see PROVENANCE.md). Do not edit or
reformat it; the one exception is a patch recorded in PROVENANCE.md "Local
patches". Everything under `door/`, `tests/`, `compat/` and the build files is
ours: follow `../../uncrustify.cfg` and run uncrustify on changed files as
the last step.

The engine reaches the platform only through `tdport/src/host.h`, implemented
by `door/host_term.c`. Pure logic goes in its own `door/*.c` with a test in
`tests/`, so it can be tested without the engine or a terminal.

## Build and test

    ./build.sh            # Linux/Unix: configure, build, run unit tests
    ./build/syncdrive --check --game-dir=game
    build.bat             :: Windows: Win32 MSVC release + unit tests

Our code has to compile under GCC/Clang **and** MSVC (the vendored `tdport/`
already does). Reach POSIX facilities through the xpdev wrappers --
`stricmp()` from `genwrap.h`, `open`/`xp_lockfile` from `filewrap.h`, paths
from `dirwrap.h` -- rather than including `<strings.h>` or `<unistd.h>`, and
leave clock/sleep/socket I/O to termgfx, which already carries that seam.

Dev game data lives in `game/` (gitignored). Fetch it from
https://archive.org/download/TestDrive_1987/Test%20Drive.zip; TDEGA.EXE md5
cdf8d1a767d559db559a0b829e1bb3b0.

## Headless smoke test

    cd game
    SYNCDRIVE_SIXELOUT=/path/to/syncdrive.six \
        SYNCDRIVE_KEYS='wait:4000,enter,wait:3000,enter,wait:3000,enter,wait:3000,quit' \
        timeout 60 ../build/syncdrive --game-dir=. --data-dir=/path/to/data

Split the sixel capture on DCS ... ST and convert each with
`convert sixel:<file> <png>` to look at the frames. This script runs the
Accolade logo, the "Accolade presents" card with the car reveal, and the
car-select spec screen (a car with its stats table, around frame 150); the
last `enter` selects that car, so the final frames show it driving off before
the script quits.
