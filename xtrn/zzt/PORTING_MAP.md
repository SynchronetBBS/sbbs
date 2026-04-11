# Pascal -> TypeScript Port Map

Source root: `reconstruction-of-zzt-master/SRC`

## Unit Mapping (Current)

- `ZZT.PAS` -> `src/zzt.ts` (startup/config/arg parsing/main loop bootstrap)
- `GAMEVARS.PAS` -> `src/gamevars.ts` (typed globals/state/constants)
- `SOUNDS.PAS` -> `src/sounds.ts` (note/drum queue + bridge output + optional world MP3 playback)
- `INPUT.PAS` + `KEYS.PAS` -> `src/input.ts` (Synchronet key decoding, control-state globals, directional/fire handling)
- `VIDEO.PAS` -> `src/video.ts` (screen buffer/write/move primitives for Synchronet console)
- `TXTWIND.PAS` -> `src/txtwind.ts` (text window rendering, pagination, link/select behavior)
- `OOP.PAS` -> `src/oop.ts` (object script parser/executor including `#PLAY`)
- `ELEMENTS.PAS` -> `src/elements.ts` (element registry + tick/touch/draw behavior for core gameplay)
- `GAME.PAS` -> `src/game.ts` (board/world load/save, title/play loops, sidebar/UI, editor flows, transitions)
- `EDITOR.PAS` -> integrated into `src/game.ts` editor routines (`EditorLoop` and related helpers), not split into a standalone module

## BBS-Oriented Extensions (Beyond DOS Parity)

- High scores are backed by JSON (`data/highscores.json`) keyed per world id, with `user @ bbs` identity display for inter-BBS play.
- Save files (`.SAV`) are user-scoped under `zzt_files/saves/<user-key>/` to avoid collisions in multiuser doors.
- Optional runtime overrides are read from `ZZT.INI` (`HIGH_SCORE_JSON`, `HIGH_SCORE_BBS`, `SAVE_ROOT`).

## Consolidation Notes

- The TypeScript port is intentionally organized by runtime subsystem rather than a strict 1:1 file split with Pascal units.
- `KEYS.PAS` constants/behavior are folded into `input.ts`.
- `EDITOR.PAS` logic is folded into `game.ts` editor sections.

## Remaining Parity Work (Known)

- Some DOS-era visual timing/animation details are approximated in the Synchronet renderer.
- Printer-oriented paths (for example Alt-P flows) are non-essential and not a target for strict modern parity.
- Sound output is parity-oriented for ZZT note/drum semantics, but not a cycle-perfect PC speaker emulation.
