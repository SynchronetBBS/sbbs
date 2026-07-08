# M2 keystone prototype — verified facts (2026-07-07)

Proven on this host: the RA engine runs **fully headless and autonomous**
to the interactive main menu and renders correct 640×400×8 frames through
the backend seam. Proof: `menu-headless.png` (converted from a captured
P6 frame). ~115 `Video_Render_Frame` calls/sec at the menu; menu reached
~8s after launch with `-NOMOVIES`.

Artifacts here:
- `keystone.patch` — the complete working prototype diff against the
  vendored tree (video_null surface memory, bNoMovies reconnection,
  CMake source-list line). Apply to reproduce.
- `video_capture_extra.cpp` — the NEW-file TU from the prototype
  (Get_Video_Mouse, Set_Video_Cursor no-op, Video_Render_Frame with
  Lock/Get_Offset/Unlock frame access + PPM dump).

## The seam (all verified by execution)

1. **Build**: `-DSDL2=OFF -DOPENAL=OFF -DBUILD_VANILLATD=OFF` selects the
   null branch; compile with `NEW_VIDEO_BUILD` defined (prototype used
   CMAKE_CXX_FLAGS; production should define it for the null/termgfx
   branch the way the SDL branches do in common/CMakeLists.txt:204-207).
2. **NEW_VIDEO_BUILD link requirements** beyond video_null.cpp: exactly
   `Get_Video_Mouse(int&,int&)`, `Set_Video_Cursor(...)` (no-op — OS
   pointer design), `Video_Render_Frame()`.
3. **Surface**: VideoSurfaceDummy must hold real memory: alloc w*h in
   ctor, GetData→buf, GetPitch→w, LockWait/Unlock→true, IsReadyToBlit→
   true, IsAllocated→false (false == "video memory OK" in startup.cpp's
   inverted check).
4. **Frame access**: `VisiblePage.Lock()` → `Get_Offset()` = pixel ptr
   (VIDEOMEM pages have Buffer==NULL; Get_Buffer() is WRONG) → read w*h
   bytes → `Unlock()`. Palette: `CurrentPalette[768]` (common/palette.h),
   VGA 6-bit — shift `<<2` for 8-bit RGB.
5. **GameInFocus**: initializes true under NEW_VIDEO_BUILD
   (redalert/globals.cpp:193-195); without it Bootstrap() spins forever
   (redalert/init.cpp:2412) — the M1 stall.
6. **Movies**: upstream `-NOMOVIES` is dead code: `bNoMovies` extern'd
   (init.cpp:80, was CHEAT_KEYS-gated) and assigned (init.cpp:1500,
   CHEAT_KEYS-gated) but **defined nowhere and never read**. Fix (in
   keystone.patch): define `bool bNoMovies = false;` in
   redalert/globals.cpp, un-gate both, early-return in
   `Play_Movie(char const*,...)` (conquer.cpp, after the Debug_Map
   check). Movies already skip in MP (`Session.Type != GAME_NORMAL`).
   Candidate upstream contribution to Vanilla Conquer.
7. **Frame pacing**: `Frame_Limiter()` (common/framelimit.cpp) calls
   `Video_Render_Frame()` each frame under NEW_VIDEO_BUILD (menu passes
   FL_FORCE_RENDER). The door throttles/paces inside its present hook —
   ADDITIONALLY `Settings.Video.FrameLimit` exists engine-side.
8. **Input seam**: `WWKeyboardClass::Fill_Buffer_From_System()` (pure
   virtual, wwkeyboard.h:911) is the pump — the door parses socket bytes
   there and feeds Put_Key etc.; `To_ASCII` (wwkeyboard.h:874) is the
   other pure virtual. `Get_Video_Mouse` returns door-tracked pointer
   coords (from SGR reports mapped through the displayed-image rect).
9. **Paths**: DataPath resolves from argv[0]'s directory when
   REDALERT.MIX is beside the binary (common/paths.cpp). UserPath
   defaults to `~/.config/vanilla-conquer/vanillara/` (redalert.ini
   written there) — the door must point it at the per-user home
   (`Paths.Init` ini `UserPath` key, or argv layout).
10. **Door architecture consequence**: engine `main()` stays untouched.
    The whole door lives in the three backend TUs + a door core module
    called from them (present hook = frames out; keyboard pump = input
    in; soundio = audio out).

## Gotcha log (cost real time)

- CP437/latin-1 bytes in redalert/conquer.cpp — edit byte-safe
  (latin-1 round-trip), never UTF-8.
- `timeout`'s SIGTERM is ignored by the engine — always
  `timeout -s KILL`.
- The engine process cwd is where relative fopen lands (no chdir by the
  engine); UserPath files go to ~/.config regardless.
