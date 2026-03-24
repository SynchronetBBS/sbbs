Space Invaders for Synchronet BBS
by Claude, assisted by Nelgin.

Classic Space Invaders gameplay for Synchronet BBS.  Terminals running
SyncTERM in graphics/pixel mode will see full sprite graphics (32x32
pixel sprites via JXL).  All other ANSI terminals get a clean text-art
fallback automatically.


Controls:
  Z / A / Left arrow   Move left
  X / D / Right arrow  Move right
  Space                Fire
  P                    Pause
  Q                    Quit


Scoring:
  Squid  (top rows)     30 pts x level
  Crab   (middle row)   20 pts x level
  Octopus (lower rows)  10 pts x level
  UFO saucer            50-300 pts x level (random)

A high-score is saved per user between sessions.
Each level increases invader speed and score multiplier.


Setup (manual):

  Copy spaceinv-dist.ini to spaceinv.ini (no edits required).

  Add to External Programs > Online Programs (Door) in scfg:

  [SynSpaceInv]
   1: Name                       Space Invaders
   2: Internal Code              SPACEINV
   3: Start-up Directory         ../xtrn/spaceinv
   4: Command Line               ?spaceinv
   5: Clean-up Command Line
   6: Execution Cost             None
   7: Access Requirements        ANSI AND COLS 80
   8: Execution Requirements
   9: Multiple Concurrent Users  Yes
  10: Native Executable          No
  11: I/O Method                 Standard
  12: Use Shell or New Context   No
  13: Modify User Data           No
  14: Execute on Event           No
  15: Pause After Execution      No
  16: Disable Local Display      No

  Or use the XTRN installer — see install-xtrn.ini.


JXL graphics (SyncTERM):

  SyncTERM must be launched in graphics/pixel mode (not ANSI/curses) for
  sprite graphics to display.  Text rendering is used automatically as a
  fallback.  See SynChess readme for SyncTERM/libjxl compilation notes.


Change Log:

23 Mar 2026   Initial release.
