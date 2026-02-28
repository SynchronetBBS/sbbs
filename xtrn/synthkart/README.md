# SynthKart

**A synthwave kart racing game for Synchronet BBS, rendered in ANSI/CP437 graphics.**

Race through neon-lit highways, collect power-ups, and compete for high scores—all through your terminal! Inspired by classic arcade racers and kart games.

[![Gameplay Video](https://img.youtube.com/vi/17-NRsQ3L4Q/0.jpg)](https://www.youtube.com/watch?v=17-NRsQ3L4Q)

**[Watch Gameplay Video on YouTube](https://www.youtube.com/watch?v=17-NRsQ3L4Q)**

## Features

- **Pseudo-3D Racing** — Classic road rendering with horizon, curves, and hills
- **Synthwave Aesthetics** — Neon colors, palm trees, sunset skies in CP437
- **Kart Items** — Mushroom boosts, shells, bananas, and more
- **AI Opponents** — CPU drivers with different personalities
- **Multiple Tracks & Themes** — Race through cities, beaches, haunted hollows, and more
- **Cup Mode** — Compete in multi-race tournaments
- **High Scores** — Local file storage or networked leaderboards via json-service

## Controls

| Key | Action |
|-----|--------|
| W / ↑ | Accelerate |
| S / ↓ | Brake |
| A / ← | Steer Left |
| D / → | Steer Right |
| Space | Use Item |
| P | Pause |
| Q | Quit |

---

# Part 1: BBS Sysop Installation Guide

## Quick Install

1. Extract the SynthKart archive to your Synchronet `xtrn/` directory (e.g., `/sbbs/xtrn/synthkart/`)
2. Configure the external program in SCFG
3. Edit `synthkart.ini` to customize settings
4. Test it out on your BBS

## SCFG Configuration

Add SynthKart as an external program in Synchronet's SCFG:

1. Run `scfg` (Synchronet Configuration)
2. Navigate to: **External Programs → Online Programs (Doors)**
3. Select or create a section (e.g., "Games")
4. Add a new program with these settings:

| Setting | Value |
|---------|-------|
| Name | SynthKart |
| Internal Code | SYNTHKART |
| Start-up Directory | ../xtrn/synthkart |
| Command Line | ?synthkart.js |
| Multiple Concurrent Users | Yes |
| Intercept I/O | No |
| Native Executable | No |
| Use Shell | No |
| Modify User Data | No |
| Execute on Event | No |
| BBS Drop File Type | None |

> **Note:** The `?` prefix tells Synchronet to run the file with `jsexec`.

## Configuration File (synthkart.ini)

The `synthkart.ini` file controls game behavior. Edit it with any text editor.

### [highscores] Section

```ini
[highscores]
server = localhost
port = 10088
service_name = synthkart
file_path = highscores.json
```

| Option | Description | Default |
|--------|-------------|---------|
| `server` | High score server (`localhost`, hostname, or `file`) | `localhost` |
| `port` | json-service port | `10088` |
| `service_name` | Database scope in json-service | `synthkart` |
| `file_path` | Local file path when `server = file` | `highscores.json` |

### [ansi_tunnel] Section

The "Data Highway" track displays scrolling ANSI art. Configure the art source here:

```ini
[ansi_tunnel]
directory = ansi_art
max_rows = 2000
scroll_speed = 0.5
```

| Option | Description | Default |
|--------|-------------|---------|
| `directory` | Path to ANSI art files (relative or absolute) | `ansi_art` |
| `max_rows` | Maximum rows to load from ANSI files | `2000` |
| `scroll_speed` | How fast the ANSI art scrolls | `0.5` |

## High Score Configuration

SynthKart supports three high score modes:

### Option A: Local File Storage

Store scores in a local JSON file (simplest, no network required):

```ini
[highscores]
server = file
file_path = highscores.json
```

### Option B: Connect to a Remote BBS

Share scores with another BBS running json-service:

```ini
[highscores]
server = somebbs.synchro.net
port = 10088
service_name = synthkart
```

> The remote BBS must have json-service configured to accept `synthkart` connections.

### Option C: Host Your Own High Score Service

Allow other BBSes to submit scores to your system:

1. **Enable json-service** in your `ctrl/services.ini`:
   ```ini
   [JSON]
   Port = 10088
   Options = STATIC
   Command = json-service.js
   ```

2. **Configure json-service** in `ctrl/json-service.ini`:
   ```ini
   [synthkart]
   dir = ../xtrn/synthkart/
   ```

3. **Set your local game** to use localhost:
   ```ini
   [highscores]
   server = localhost
   port = 10088
   service_name = synthkart
   ```

4. **Open port 10088** in your firewall for remote BBSes to connect.

5. **Share your hostname** with other sysops so they can point their `server` setting to your BBS.

## Custom ANSI Art

### ANSI Tunnel Track Art

The "Data Highway" track displays scrolling ANSI art during gameplay. To customize:

1. Create a directory with `.ans` files (e.g., `/sbbs/text/coolansi/`)
2. Update `synthkart.ini`:
   ```ini
   [ansi_tunnel]
   directory = /sbbs/text/coolansi
   ```

The game loads all `.ans` files from the directory and scrolls through them as you race.

---

# Part 2: Developer Guide

## Prerequisites

- **Node.js** v18+ (v22 recommended) with npm
- **TypeScript** 5.3+
- **Synchronet BBS** (for testing)

## Build Chain Overview

SynthKart is written in TypeScript and compiled to a single JavaScript file that Synchronet can execute.

```
src/*.ts  →  tsc  →  dist/*.js  →  concatenate  →  synthkart.js
```

1. **TypeScript Compilation**: `tsc` compiles `.ts` files to `.js` in `dist/`
2. **Bundling**: `build.sh` concatenates all JS files in dependency order
3. **Output**: `synthkart.js` is the distributable file for Synchronet

## Node.js Path Conflict

**Important:** Synchronet includes its own `node` binary that conflicts with modern Node.js. The build script requires Node.js v18+ for TypeScript, but Synchronet's `node` is much older.

### The Problem

If Synchronet's `bin/` directory is in your PATH, running `node` or `npm` may invoke the wrong version:

```bash
$ which node
/sbbs/exec/node    # Synchronet's old node - won't work!
```

### The Solution

The `build.sh` script explicitly sets PATH to use nvm's Node.js:

```bash
export PATH=/home/sbbs/.nvm/versions/node/v22.20.0/bin:$PATH
```

**Adjust this path** to match your Node.js installation:

- **nvm users**: `~/.nvm/versions/node/vX.Y.Z/bin`
- **System Node.js**: `/usr/bin` or `/usr/local/bin`
- **Other**: Run `which node` in a clean shell to find the correct path

### Alternative: Use a Subshell

```bash
# Run build in a clean environment
env -i HOME=$HOME PATH=/usr/bin:/usr/local/bin ./build.sh
```

## Building

```bash
# Install dependencies (first time only)
npm install

# Build the game
./build.sh

# Output: synthkart.js (in project root)
```

Or using npm directly (if PATH is configured):

```bash
npm run build
```

## Project Structure

```
synthkart/
├── src/                 # TypeScript source code
│   ├── main.ts          # Entry point
│   ├── bootstrap.ts     # Synchronet compatibility layer
│   ├── game/            # Game loop, state machine
│   ├── entities/        # Vehicles, drivers (human & AI)
│   ├── physics/         # Movement, steering, collision
│   ├── render/          # ANSI/CP437 rendering
│   │   ├── cp437/       # Character-based rendering
│   │   ├── themes/      # Track visual themes
│   │   └── sprites/     # Vehicle sprites
│   ├── hud/             # Speedometer, minimap, etc.
│   ├── input/           # Keyboard controls
│   ├── items/           # Power-ups (mushroom, shell, banana)
│   ├── world/           # Tracks, checkpoints
│   ├── highscores/      # Score management
│   └── util/            # Math, logging, config
├── data/
│   └── tracks/          # (legacy - tracks now defined in TrackCatalog.ts)
├── assets/              # Binary art files (title.bin, exit.bin)
├── ansi_art/            # ANSI art for Data Highway track
├── dist/                # Compiled JS (intermediate)
├── build.sh             # Build script
├── synthkart.js         # Final distributable
├── synthkart.ini        # Runtime configuration
└── tsconfig.json        # TypeScript config
```

## Testing Locally

```bash
# Run directly with Synchronet's jsexec
jsexec synthkart.js

# Or from the Synchronet directory
/sbbs/exec/jsexec /sbbs/xtrn/synthkart/synthkart.js
```

## Technical Notes

- **Runtime**: Synchronet JavaScript (SpiderMonkey 1.8.5 with Synchronet extensions)
- **Terminal**: Requires 80×24 ANSI-compatible terminal
- **No ES6 modules**: Output must be a single concatenated file
- **No Node.js APIs**: Code runs in Synchronet's JS environment, not Node.js

## Adding New Tracks

Tracks are defined in [src/world/TrackCatalog.ts](src/world/TrackCatalog.ts) using the `TRACK_CATALOG` array. Each track definition includes:

```typescript
{
  id: 'my_track',           // Unique identifier
  name: 'My Track',         // Display name
  description: 'A fun track',
  difficulty: 3,            // 1-5 stars
  laps: 3,
  themeId: 'synthwave',     // Reference to TRACK_THEMES
  estimatedLapTime: 40,
  npcCount: 5,
  hidden: false,            // Set true for secret tracks
  sections: [
    { type: 'straight', length: 8 },
    { type: 'ease_in', length: 3, targetCurve: 0.4 },
    { type: 'curve', length: 8, curve: 0.4 },
    { type: 'ease_out', length: 3 },
    // ... more sections
  ]
}
```

Section types: `straight`, `curve`, `ease_in`, `ease_out`, `s_curve`

After adding a track, rebuild with `./build.sh`.

## Adding New Themes

1. Create sprite definitions in `src/render/themes/`
2. Create theme class extending base Theme
3. Register in the theme catalog
4. Add to `build.sh` concatenation list

---

## License

ISC License — See LICENSE file

## Credits

- Built for the Synchronet BBS community
- Inspired by classic arcade racers
- CP437/ANSI art rendering techniques
