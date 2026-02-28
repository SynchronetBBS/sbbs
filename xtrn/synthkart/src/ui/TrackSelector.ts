/**
 * TrackSelector - Circuit-based track selection UI inspired by Mario Kart.
 *
 * Layout:
 * - Left panel (cols 1-22): Circuit/Track selector with icons
 * - Right panel (cols 23-80): Track info, route visualization, stats
 *
 * Circuits:
 * - 4 circuits with 4 tracks each (16 total, excluding test tracks)
 * - User can play full circuit or select individual tracks
 */

// ============================================================
// CIRCUIT DEFINITIONS
// ============================================================

interface Circuit {
  id: string;
  name: string;
  icon: string[];     // 5-line ASCII art icon
  color: number;      // Primary color
  trackIds: string[]; // 4 track IDs in this circuit
  description: string;
}

/**
 * The four racing circuits, each with 4 tracks.
 * 16 unique tracks across all circuits.
 */
var CIRCUITS: Circuit[] = [
  {
    id: 'retro_cup',
    name: 'RETRO CUP',
    icon: [
      '  ____  ',
      ' /    \\ ',
      '|  RC  |',
      ' \\____/ ',
      '   ||   '
    ],
    color: LIGHTCYAN,
    trackIds: ['neon_coast', 'downtown_dash', 'sunset_beach', 'twilight_grove'],
    description: 'Classic retro vibes'
  },
  {
    id: 'nature_cup',
    name: 'NATURE CUP',
    icon: [
      '   /\\   ',
      '  /  \\  ',
      ' /    \\ ',
      '/______\\',
      '   NC   '
    ],
    color: LIGHTGREEN,
    trackIds: ['winter_wonderland', 'cactus_canyon', 'jungle_run', 'sugar_rush'],
    description: 'Wild natural courses'
  },
  {
    id: 'dark_cup',
    name: 'DARK CUP',
    icon: [
      ' _\\||/_ ',
      '  \\||/  ',
      '  /||\\ ',
      ' /_||\\_',
      '   DC   '
    ],
    color: LIGHTMAGENTA,
    trackIds: ['haunted_hollow', 'fortress_rally', 'inferno_speedway', 'mermaid_lagoon'],
    description: 'Dangerous & mysterious'
  },
  {
    id: 'special_cup',
    name: 'SPECIAL CUP',
    icon: [
      '   **   ',
      '  *  *  ',
      ' * SC * ',
      '  *  *  ',
      '   **   '
    ],
    color: YELLOW,
    trackIds: ['celestial_circuit', 'data_highway', 'glitch_circuit', 'kaiju_rampage'],
    description: 'Ultimate challenge'
  }
];

// ============================================================
// UI STATE
// ============================================================

interface SelectorState {
  mode: 'circuit' | 'tracks';  // Current view mode
  circuitIndex: number;        // Selected circuit (0-3)
  trackIndex: number;          // Selected track within circuit (0-3), or 4 for "Play Circuit"
}

// ============================================================
// RESULT TYPE
// ============================================================

interface TrackSelectionResult {
  selected: boolean;
  track: TrackDefinition | null;
  isCircuitMode?: boolean;
  circuitTracks?: TrackDefinition[] | null;
  circuitId?: string;
  circuitName?: string;
}

// ============================================================
// LAYOUT CONSTANTS
// ============================================================

var LEFT_PANEL_WIDTH = 22;
var RIGHT_PANEL_START = 24;
var SCREEN_WIDTH = 80;

// ============================================================
// MAIN SELECTOR FUNCTION
// ============================================================

/**
 * Display the track selector and wait for user input.
 */
function showTrackSelector(highScoreManager?: HighScoreManager): TrackSelectionResult {
  var state: SelectorState = {
    mode: 'circuit',
    circuitIndex: 0,
    trackIndex: 0
  };

  // Initial draw
  console.clear(LIGHTGRAY, false);
  drawSelectorUI(state, highScoreManager);

  while (true) {
    var key = console.inkey(K_UPPER, 100);
    if (key === '') continue;

    var needsRedraw = false;

    if (state.mode === 'circuit') {
      // Circuit selection mode
      if (key === KEY_UP || key === 'W' || key === '8') {
        state.circuitIndex = (state.circuitIndex - 1 + CIRCUITS.length) % CIRCUITS.length;
        needsRedraw = true;
      }
      else if (key === KEY_DOWN || key === 'S' || key === '2') {
        state.circuitIndex = (state.circuitIndex + 1) % CIRCUITS.length;
        needsRedraw = true;
      }
      else if (key === '\r' || key === '\n' || key === ' ' || key === KEY_RIGHT || key === 'D' || key === '6') {
        // Enter circuit - switch to track selection
        state.mode = 'tracks';
        state.trackIndex = 0;
        needsRedraw = true;
      }
      else if (key === 'H' && highScoreManager) {
        // Show circuit high scores
        var circuit = CIRCUITS[state.circuitIndex];
        showHighScoreList(
          HighScoreType.CIRCUIT_TIME,
          circuit.id,
          '=== CIRCUIT HIGH SCORES ===',
          circuit.name,
          highScoreManager
        );
        needsRedraw = true;
      }
      else if (key === 'Q' || key === KEY_ESC) {
        return { selected: false, track: null };
      }
      else if (key === '?') {
        // Secret tracks menu
        var secretResult = showSecretTracksMenu();
        if (secretResult.selected && secretResult.track) {
          return secretResult;
        }
        needsRedraw = true;
      }
    }
    else {
      // Track selection mode within a circuit
      if (key === KEY_UP || key === 'W' || key === '8') {
        state.trackIndex--;
        if (state.trackIndex < 0) state.trackIndex = 4; // 0-3 = tracks, 4 = "Play Circuit"
        needsRedraw = true;
      }
      else if (key === KEY_DOWN || key === 'S' || key === '2') {
        state.trackIndex++;
        if (state.trackIndex > 4) state.trackIndex = 0;
        needsRedraw = true;
      }
      else if (key === '\r' || key === '\n' || key === ' ') {
        var circuit = CIRCUITS[state.circuitIndex];
        if (state.trackIndex === 4) {
          // Play full circuit
          var circuitTracks = getCircuitTracks(circuit);
          return {
            selected: true,
            track: circuitTracks[0],
            isCircuitMode: true,
            circuitTracks: circuitTracks,
            circuitId: circuit.id,
            circuitName: circuit.name
          };
        } else {
          // Play single track
          var trackDef = getTrackDefinition(circuit.trackIds[state.trackIndex]);
          if (trackDef) {
            return {
              selected: true,
              track: trackDef,
              isCircuitMode: false,
              circuitTracks: null
            };
          }
        }
      }
      else if (key === KEY_LEFT || key === 'A' || key === '4' || key === KEY_ESC) {
        // Back to circuit selection
        state.mode = 'circuit';
        needsRedraw = true;
      }
      else if (key === 'H' && highScoreManager) {
        // Show high scores for selected track or circuit
        var circuit = CIRCUITS[state.circuitIndex];
        if (state.trackIndex === 4) {
          // "Play Cup" selected - show circuit high scores
          showHighScoreList(
            HighScoreType.CIRCUIT_TIME,
            circuit.id,
            '=== CIRCUIT HIGH SCORES ===',
            circuit.name,
            highScoreManager
          );
        } else {
          // Individual track selected - show track high scores (two-column)
          var trackId = circuit.trackIds[state.trackIndex];
          var trackDef = getTrackDefinition(trackId);
          if (trackDef) {
            showTwoColumnHighScores(
              trackId,
              trackDef.name,
              highScoreManager,
              0,  // No player position highlight
              0
            );
          }
        }
        needsRedraw = true;
      }
      else if (key === 'Q') {
        return { selected: false, track: null };
      }
      else if (key === '?') {
        // Secret tracks menu
        var secretResult = showSecretTracksMenu();
        if (secretResult.selected && secretResult.track) {
          return secretResult;
        }
        needsRedraw = true;
      }
    }

    if (needsRedraw) {
      console.clear(LIGHTGRAY, false);
      drawSelectorUI(state, highScoreManager);
    }
  }
}

/**
 * Get all tracks in a circuit.
 */
function getCircuitTracks(circuit: Circuit): TrackDefinition[] {
  var tracks: TrackDefinition[] = [];
  for (var i = 0; i < circuit.trackIds.length; i++) {
    var track = getTrackDefinition(circuit.trackIds[i]);
    if (track) tracks.push(track);
  }
  return tracks;
}

/**
 * Get all hidden/secret tracks from the catalog.
 */
function getSecretTracks(): TrackDefinition[] {
  var secrets: TrackDefinition[] = [];
  var allTracks = getAllTracks();
  for (var i = 0; i < allTracks.length; i++) {
    if (allTracks[i].hidden) {
      secrets.push(allTracks[i]);
    }
  }
  return secrets;
}

/**
 * Show the secret tracks menu.
 * Returns a track selection result if user picks a track, or { selected: false } to go back.
 */
function showSecretTracksMenu(): TrackSelectionResult {
  var secretTracks = getSecretTracks();
  
  if (secretTracks.length === 0) {
    // No secrets yet - show a teaser
    console.clear(LIGHTGRAY, false);
    console.gotoxy(1, 10);
    console.attributes = LIGHTMAGENTA;
    console.print('       ' + GLYPH.DBOX_TL + repeatChar(GLYPH.DBOX_H, 44) + GLYPH.DBOX_TR + '\r\n');
    console.print('       ' + GLYPH.DBOX_V + '         NO SECRETS FOUND... YET          ' + GLYPH.DBOX_V + '\r\n');
    console.print('       ' + GLYPH.DBOX_V + '                                          ' + GLYPH.DBOX_V + '\r\n');
    console.print('       ' + GLYPH.DBOX_V + '  Keep racing to unlock hidden tracks!    ' + GLYPH.DBOX_V + '\r\n');
    console.print('       ' + GLYPH.DBOX_BL + repeatChar(GLYPH.DBOX_H, 44) + GLYPH.DBOX_BR + '\r\n');
    console.print('\r\n');
    console.attributes = DARKGRAY;
    console.print('                 Press any key to return...\r\n');
    console.inkey(K_NONE, 5000);
    return { selected: false, track: null };
  }
  
  var selectedIndex = 0;
  
  while (true) {
    // Draw secret tracks menu
    console.clear(LIGHTGRAY, false);
    
    // Header with CP437 ASCII art
    console.gotoxy(1, 1);
    console.attributes = LIGHTRED;
    console.print(repeatChar(GLYPH.DBOX_H, 79) + '\r\n');
    console.attributes = YELLOW;
    console.print('                     #### SECRET TRACKS ####\r\n');
    console.attributes = LIGHTRED;
    console.print(repeatChar(GLYPH.DBOX_H, 79) + '\r\n');
    console.print('\r\n');
    
    console.attributes = DARKGRAY;
    console.print('  These tracks are hidden from the main menu. Shh, it\'s a secret!\r\n\r\n');
    
    // List secret tracks
    for (var i = 0; i < secretTracks.length; i++) {
      var track = secretTracks[i];
      console.gotoxy(5, 9 + i * 2);
      
      if (i === selectedIndex) {
        console.attributes = LIGHTCYAN;
        console.print('>>  ');
      } else {
        console.attributes = DARKGRAY;
        console.print('    ');
      }
      
      console.attributes = i === selectedIndex ? WHITE : LIGHTGRAY;
      console.print(track.name);
      
      console.attributes = DARKGRAY;
      console.print(' - ');
      console.print(track.description);
      
      // Difficulty stars
      console.gotoxy(65, 9 + i * 2);
      console.attributes = YELLOW;
      console.print(renderDifficultyStars(track.difficulty));
    }
    
    // Controls
    console.gotoxy(5, 20);
    console.attributes = DARKGRAY;
    console.print(repeatChar(GLYPH.BOX_H, 68) + '\r\n');
    console.gotoxy(5, 21);
    console.attributes = CYAN;
    console.print('[');
    console.attributes = WHITE;
    console.print('W/S');
    console.attributes = CYAN;
    console.print('] Select   [');
    console.attributes = WHITE;
    console.print('ENTER');
    console.attributes = CYAN;
    console.print('] Race   [');
    console.attributes = WHITE;
    console.print('ESC/Q');
    console.attributes = CYAN;
    console.print('] Back');
    
    // Handle input
    var key = console.inkey(K_UPPER, 100);
    if (key === '') continue;
    
    if (key === KEY_UP || key === 'W' || key === '8') {
      selectedIndex = (selectedIndex - 1 + secretTracks.length) % secretTracks.length;
    }
    else if (key === KEY_DOWN || key === 'S' || key === '2') {
      selectedIndex = (selectedIndex + 1) % secretTracks.length;
    }
    else if (key === '\r' || key === '\n' || key === ' ') {
      return {
        selected: true,
        track: secretTracks[selectedIndex],
        isCircuitMode: false,
        circuitTracks: null
      };
    }
    else if (key === 'Q' || key === KEY_ESC) {
      return { selected: false, track: null };
    }
  }
}

// ============================================================
// UI DRAWING
// ============================================================

/**
 * Draw the complete selector UI.
 */
function drawSelectorUI(state: SelectorState, highScoreManager?: HighScoreManager): void {
  drawHeader();
  drawLeftPanel(state);
  drawRightPanel(state, highScoreManager);
  drawControls(state);
}

/**
 * Draw the header.
 */
function drawHeader(): void {
  console.gotoxy(1, 1);
  console.attributes = LIGHTMAGENTA;
  console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH));
  
  console.gotoxy(1, 2);
  console.attributes = LIGHTCYAN;
  var title = '  SELECT YOUR RACE  ';
  var padding = Math.floor((SCREEN_WIDTH - title.length) / 2);
  console.print(repeatChar(' ', padding) + title);
  
  console.gotoxy(1, 3);
  console.attributes = LIGHTMAGENTA;
  console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH));
}

/**
 * Draw the left panel - circuit and track selection.
 */
function drawLeftPanel(state: SelectorState): void {
  var y = 5;
  
  // Draw vertical separator
  for (var sy = 4; sy <= 22; sy++) {
    console.gotoxy(LEFT_PANEL_WIDTH, sy);
    console.attributes = DARKGRAY;
    console.print(GLYPH.BOX_V);
  }
  
  if (state.mode === 'circuit') {
    drawCircuitSelector(state, y);
  } else {
    drawTrackList(state, y);
  }
}

/**
 * Draw the circuit selector (vertical list with icons).
 */
function drawCircuitSelector(state: SelectorState, startY: number): void {
  console.gotoxy(2, startY);
  console.attributes = WHITE;
  console.print('SELECT CIRCUIT');
  
  // Draw all 4 circuits as selectable icons
  for (var i = 0; i < CIRCUITS.length; i++) {
    var circuit = CIRCUITS[i];
    var isSelected = (i === state.circuitIndex);
    var baseY = startY + 2 + (i * 4);
    
    // Selection indicator
    console.gotoxy(1, baseY + 1);
    if (isSelected) {
      console.attributes = circuit.color;
      console.print(GLYPH.TRIANGLE_RIGHT);
    } else {
      console.print(' ');
    }
    
    // Circuit name (compact for left panel)
    console.gotoxy(3, baseY);
    console.attributes = isSelected ? circuit.color : DARKGRAY;
    
    // Show abbreviated name that fits
    var shortName = circuit.name.substring(0, 10);
    console.print(shortName);
    
    // Small icon indicator
    console.gotoxy(3, baseY + 1);
    console.attributes = isSelected ? WHITE : DARKGRAY;
    console.print(circuit.icon[2].substring(0, 8)); // Middle line of icon
    
    // Track count
    console.gotoxy(3, baseY + 2);
    console.attributes = isSelected ? LIGHTGRAY : DARKGRAY;
    console.print('4 tracks');
  }
}

/**
 * Draw the track list for selected circuit.
 */
function drawTrackList(state: SelectorState, startY: number): void {
  var circuit = CIRCUITS[state.circuitIndex];
  
  // Circuit name header with back indicator
  console.gotoxy(2, startY);
  console.attributes = DARKGRAY;
  console.print(GLYPH.TRIANGLE_LEFT + ' ');
  console.attributes = circuit.color;
  console.print(circuit.name.substring(0, 14));
  
  console.gotoxy(2, startY + 1);
  console.attributes = DARKGRAY;
  console.print(repeatChar(GLYPH.BOX_H, LEFT_PANEL_WIDTH - 4));
  
  // Track list
  for (var i = 0; i < circuit.trackIds.length; i++) {
    var track = getTrackDefinition(circuit.trackIds[i]);
    if (!track) continue;
    
    var isSelected = (i === state.trackIndex);
    var y = startY + 3 + (i * 3);
    
    // Selection indicator
    console.gotoxy(1, y);
    if (isSelected) {
      console.attributes = circuit.color;
      console.print(GLYPH.TRIANGLE_RIGHT);
    } else {
      console.print(' ');
    }
    
    // Track number
    console.gotoxy(3, y);
    console.attributes = isSelected ? WHITE : LIGHTGRAY;
    console.print((i + 1) + '.');
    
    // Track name (truncate if needed)
    console.gotoxy(6, y);
    console.attributes = isSelected ? circuit.color : CYAN;
    var name = track.name.substring(0, 13);
    console.print(name);
    
    // Difficulty stars
    console.gotoxy(3, y + 1);
    console.attributes = isSelected ? YELLOW : BROWN;
    console.print(renderDifficultyStars(track.difficulty));
  }
  
  // "Play Circuit" option
  var playY = startY + 3 + (4 * 3);
  var isPlaySelected = (state.trackIndex === 4);
  
  console.gotoxy(1, playY);
  if (isPlaySelected) {
    console.attributes = LIGHTGREEN;
    console.print(GLYPH.TRIANGLE_RIGHT);
  } else {
    console.print(' ');
  }
  
  console.gotoxy(3, playY);
  console.attributes = isPlaySelected ? LIGHTGREEN : GREEN;
  console.print(GLYPH.TRIANGLE_RIGHT + ' PLAY CUP');
}

/**
 * Draw the right panel - track info and route visualization.
 */
function drawRightPanel(state: SelectorState, highScoreManager?: HighScoreManager): void {
  var circuit = CIRCUITS[state.circuitIndex];
  
  // Panel header
  console.gotoxy(RIGHT_PANEL_START, 5);
  console.attributes = WHITE;
  
  if (state.mode === 'circuit' || (state.mode === 'tracks' && state.trackIndex === 4)) {
    // Circuit selection mode OR "Play Cup" selected - show circuit info
    console.print('CIRCUIT INFO');
    
    console.gotoxy(RIGHT_PANEL_START, 6);
    console.attributes = DARKGRAY;
    console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH - RIGHT_PANEL_START - 1));
    
    drawCircuitInfo(circuit, highScoreManager);
  } else {
    // Individual track selected - show track info
    var track = getTrackDefinition(circuit.trackIds[state.trackIndex]);
    if (track) {
      console.print('TRACK INFO');
      
      console.gotoxy(RIGHT_PANEL_START, 6);
      console.attributes = DARKGRAY;
      console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH - RIGHT_PANEL_START - 1));
      
      drawTrackInfo(track, circuit.color, highScoreManager);
      drawTrackRoute(track);
    }
  }
}

/**
 * Draw circuit info - shows track listing for the circuit.
 */
function drawCircuitInfo(circuit: Circuit, _highScoreManager?: HighScoreManager): void {
  var y = 8;
  
  // Circuit name with icon
  console.gotoxy(RIGHT_PANEL_START, y);
  console.attributes = circuit.color;
  console.print(circuit.name);
  
  // Draw circuit icon
  for (var iconLine = 0; iconLine < circuit.icon.length; iconLine++) {
    console.gotoxy(RIGHT_PANEL_START + 20, y - 1 + iconLine);
    console.attributes = circuit.color;
    console.print(circuit.icon[iconLine]);
  }
  
  console.gotoxy(RIGHT_PANEL_START, y + 1);
  console.attributes = LIGHTGRAY;
  console.print(circuit.description);
  
  // Track list
  console.gotoxy(RIGHT_PANEL_START, y + 3);
  console.attributes = WHITE;
  console.print('Tracks in this circuit:');
  
  var totalLaps = 0;
  var totalTime = 0;
  
  for (var i = 0; i < circuit.trackIds.length; i++) {
    var track = getTrackDefinition(circuit.trackIds[i]);
    if (!track) continue;
    
    console.gotoxy(RIGHT_PANEL_START + 2, y + 5 + i);
    console.attributes = CYAN;
    console.print((i + 1) + '. ' + padRight(track.name, 22));
    
    console.attributes = YELLOW;
    console.print(renderDifficultyStars(track.difficulty));
    
    totalLaps += track.laps;
    totalTime += track.estimatedLapTime * track.laps;
  }
  
  // Circuit totals
  console.gotoxy(RIGHT_PANEL_START, y + 11);
  console.attributes = DARKGRAY;
  console.print(repeatChar(GLYPH.BOX_H, 40));
  
  console.gotoxy(RIGHT_PANEL_START, y + 12);
  console.attributes = LIGHTGRAY;
  console.print('Total Races: ');
  console.attributes = WHITE;
  console.print(circuit.trackIds.length.toString());
  
  console.gotoxy(RIGHT_PANEL_START + 20, y + 12);
  console.attributes = LIGHTGRAY;
  console.print('Total Laps: ');
  console.attributes = WHITE;
  console.print(totalLaps.toString());
  
  console.gotoxy(RIGHT_PANEL_START, y + 13);
  console.attributes = LIGHTGRAY;
  console.print('Est. Time: ');
  console.attributes = WHITE;
  console.print(formatTime(totalTime));
}

/**
 * Draw track info - large themed track map as the hero element.
 */
function drawTrackInfo(track: TrackDefinition, _accentColor: number, highScoreManager?: HighScoreManager): void {
  var theme = getTrackTheme(track);
  
  // Track name in theme color with difficulty stars
  console.gotoxy(RIGHT_PANEL_START, 8);
  console.attributes = theme.road.edge.fg;
  console.print(track.name);
  console.attributes = YELLOW;
  console.print('  ' + renderDifficultyStars(track.difficulty));
  
  // Just laps count - minimal stats
  console.gotoxy(RIGHT_PANEL_START, 9);
  console.attributes = DARKGRAY;
  console.print(track.laps + ' laps');
  
  // High scores for this track
  if (highScoreManager) {
    var trackTimeScore = highScoreManager.getTopScore(HighScoreType.TRACK_TIME, track.id);
    var lapTimeScore = highScoreManager.getTopScore(HighScoreType.LAP_TIME, track.id);
    
    if (trackTimeScore || lapTimeScore) {
      console.gotoxy(RIGHT_PANEL_START, 10);
      console.attributes = DARKGRAY;
      console.print('High Scores:');
      
      if (trackTimeScore) {
        console.gotoxy(RIGHT_PANEL_START + 2, 11);
        console.attributes = LIGHTGRAY;
        console.print('Track: ');
        console.attributes = LIGHTGREEN;
        console.print(LapTimer.format(trackTimeScore.time) + ' ');
        console.attributes = DARKGRAY;
        console.print('(' + trackTimeScore.playerName + ')');
      }
      
      if (lapTimeScore) {
        console.gotoxy(RIGHT_PANEL_START + 2, 12);
        console.attributes = LIGHTGRAY;
        console.print('Lap:   ');
        console.attributes = LIGHTGREEN;
        console.print(LapTimer.format(lapTimeScore.time) + ' ');
        console.attributes = DARKGRAY;
        console.print('(' + lapTimeScore.playerName + ')');
      }
    }
  }
  
  // Large track map visualization - this is the hero
  drawThemedTrackMap(track, theme);
}

/**
 * Draw a large, theme-styled track map using half-block rendering.
 * Each character row represents 2 vertical pixels for higher resolution.
 */
function drawThemedTrackMap(track: TrackDefinition, theme: TrackTheme): void {
  // Map area in characters - expanded to use more screen space
  var mapX = RIGHT_PANEL_START;
  var mapY = 11;
  var mapWidth = SCREEN_WIDTH - RIGHT_PANEL_START - 1; // Use nearly full width
  var mapHeight = 13; // Character rows (expanded from 11)
  
  // Virtual pixel dimensions (2x vertical resolution)
  var pixelWidth = mapWidth - 2;
  var pixelHeight = (mapHeight - 1) * 2; // Double the vertical pixels
  
  // Generate track loop points at pixel resolution
  var points = generateTrackLoop(track, pixelWidth, pixelHeight);
  
  // Create pixel grid (0=empty, 1=edge, 2=road surface, 3=infield)
  var pixels = createPixelGrid(pixelWidth, pixelHeight);
  
  // Rasterize track onto pixel grid
  rasterizeTrack(points, pixels, pixelWidth, pixelHeight);
  
  // Flood-fill from edges to identify outside area, remaining empty = infield
  markInfield(pixels, pixelWidth, pixelHeight);
  
  // Render using half-blocks with theme colors
  renderHalfBlockMap(pixels, mapX + 2, mapY + 1, pixelWidth, pixelHeight, theme);
  
  // Draw start/finish marker
  drawStartFinishHiRes(points, mapX + 2, mapY + 1, pixelWidth, pixelHeight, theme);
}

/**
 * Create an empty pixel grid.
 */
function createPixelGrid(width: number, height: number): number[][] {
  var grid: number[][] = [];
  for (var y = 0; y < height; y++) {
    grid[y] = [];
    for (var x = 0; x < width; x++) {
      grid[y][x] = 0;
    }
  }
  return grid;
}

/**
 * Mark infield pixels using flood-fill from edges.
 * After this: 0=outside, 1=edge, 2=road, 3=infield
 * Any empty pixel (0) not reachable from edges becomes infield (3).
 */
function markInfield(pixels: number[][], width: number, height: number): void {
  // Use -1 as temporary marker for "visited from outside"
  var OUTSIDE_MARKER = -1;
  
  // Queue-based flood fill from all edge pixels
  var queue: { x: number; y: number }[] = [];
  
  // Seed queue with all edge pixels that are empty
  for (var x = 0; x < width; x++) {
    if (pixels[0][x] === 0) {
      pixels[0][x] = OUTSIDE_MARKER;
      queue.push({ x: x, y: 0 });
    }
    if (pixels[height - 1][x] === 0) {
      pixels[height - 1][x] = OUTSIDE_MARKER;
      queue.push({ x: x, y: height - 1 });
    }
  }
  for (var y = 0; y < height; y++) {
    if (pixels[y][0] === 0) {
      pixels[y][0] = OUTSIDE_MARKER;
      queue.push({ x: 0, y: y });
    }
    if (pixels[y][width - 1] === 0) {
      pixels[y][width - 1] = OUTSIDE_MARKER;
      queue.push({ x: width - 1, y: y });
    }
  }
  
  // Flood fill - mark all reachable empty pixels as outside
  while (queue.length > 0) {
    var p = queue.shift();
    if (!p) break;
    
    // Check 4 neighbors
    var neighbors = [
      { x: p.x - 1, y: p.y },
      { x: p.x + 1, y: p.y },
      { x: p.x, y: p.y - 1 },
      { x: p.x, y: p.y + 1 }
    ];
    
    for (var i = 0; i < neighbors.length; i++) {
      var n = neighbors[i];
      if (n.x >= 0 && n.x < width && n.y >= 0 && n.y < height) {
        if (pixels[n.y][n.x] === 0) {
          pixels[n.y][n.x] = OUTSIDE_MARKER;
          queue.push(n);
        }
      }
    }
  }
  
  // Convert markers: -1 -> 0 (outside), 0 -> 3 (infield - unreachable from edge)
  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      if (pixels[y][x] === OUTSIDE_MARKER) {
        pixels[y][x] = 0; // Outside
      } else if (pixels[y][x] === 0) {
        pixels[y][x] = 3; // Infield (wasn't reached by flood fill)
      }
    }
  }
}

/**
 * Rasterize track path onto pixel grid - draws a thin road, not a filled shape.
 * The track should look like a loop with an empty infield in the center.
 */
function rasterizeTrack(points: { x: number; y: number }[], pixels: number[][], width: number, height: number): void {
  if (points.length < 2) return;
  
  // Road width in pixels (thin enough to see the infield)
  var roadWidth = 2;
  var edgeWidth = 1;
  
  // Draw the track as a thin line following the path
  for (var i = 0; i < points.length - 1; i++) {
    var x1 = points[i].x;
    var y1 = points[i].y;
    var x2 = points[i + 1].x;
    var y2 = points[i + 1].y;
    
    // Calculate perpendicular for road width
    var dx = x2 - x1;
    var dy = y2 - y1;
    var len = Math.sqrt(dx * dx + dy * dy);
    if (len === 0) continue;
    
    // Perpendicular unit vector
    var perpX = -dy / len;
    var perpY = dx / len;
    
    // Step along the line segment
    var steps = Math.max(Math.ceil(len), 1);
    for (var s = 0; s <= steps; s++) {
      var t = s / steps;
      var cx = x1 + dx * t;
      var cy = y1 + dy * t;
      
      // Draw road width perpendicular to direction
      for (var w = -(roadWidth + edgeWidth); w <= (roadWidth + edgeWidth); w++) {
        var px = Math.floor(cx + perpX * w);
        var py = Math.floor(cy + perpY * w);
        
        if (px >= 0 && px < width && py >= 0 && py < height) {
          if (Math.abs(w) <= roadWidth) {
            pixels[py][px] = 2; // Road surface
          } else if (pixels[py][px] === 0) {
            pixels[py][px] = 1; // Edge
          }
        }
      }
    }
  }
}

/**
 * Render pixel grid using half-block characters.
 * Each character cell displays 2 vertical pixels.
 * Pixel values: 0=outside, 1=edge, 2=road surface, 3=infield
 */
function renderHalfBlockMap(pixels: number[][], screenX: number, screenY: number, width: number, height: number, theme: TrackTheme): void {
  var charRows = Math.floor(height / 2);
  
  // Helper to get color for a pixel type
  function getPixelColor(pixelType: number): number {
    switch (pixelType) {
      case 0: return theme.offroad.groundColor.fg; // Outside - terrain
      case 1: return theme.road.edge.fg;           // Edge
      case 2: return theme.road.surface.fg;        // Road surface
      case 3: return BG_BLACK;                     // Infield - empty/black
      default: return theme.offroad.groundColor.fg;
    }
  }
  
  // Helper to check if pixel is "filled" (not empty background)
  function isFilledPixel(pixelType: number): boolean {
    return pixelType === 1 || pixelType === 2; // Edge or road
  }
  
  for (var charRow = 0; charRow < charRows; charRow++) {
    var topPixelY = charRow * 2;
    var bottomPixelY = charRow * 2 + 1;
    
    for (var col = 0; col < width; col++) {
      var topPixel = (topPixelY < height) ? pixels[topPixelY][col] : 0;
      var bottomPixel = (bottomPixelY < height) ? pixels[bottomPixelY][col] : 0;
      
      console.gotoxy(screenX + col, screenY + charRow);
      
      var topFilled = isFilledPixel(topPixel);
      var bottomFilled = isFilledPixel(bottomPixel);
      
      if (topFilled && bottomFilled) {
        // Both road/edge - full block with appropriate color
        // Priority: road > edge
        var pixelType = (topPixel === 2 || bottomPixel === 2) ? 2 : 1;
        console.attributes = getPixelColor(pixelType);
        console.print(GLYPH.FULL_BLOCK);
      } else if (topFilled) {
        // Only top is road/edge - upper half
        // fg = road color, bg = what's below (infield or outside)
        var topColor = getPixelColor(topPixel);
        var bottomColor = getPixelColor(bottomPixel);
        console.attributes = topColor + (bottomColor << 4);
        console.print(GLYPH.UPPER_HALF);
      } else if (bottomFilled) {
        // Only bottom is road/edge - lower half
        var topColor = getPixelColor(topPixel);
        var bottomColor = getPixelColor(bottomPixel);
        console.attributes = bottomColor + (topColor << 4);
        console.print(GLYPH.LOWER_HALF);
      } else {
        // Neither is road - show background (outside or infield)
        if (topPixel === 3 || bottomPixel === 3) {
          // At least one is infield - show as empty/black
          console.attributes = BLACK;
          console.print(' ');
        } else {
          // Both outside - show terrain texture
          console.attributes = theme.offroad.groundColor.fg;
          console.print(GLYPH.LIGHT_SHADE);
        }
      }
    }
  }
}

/**
 * Generate a closed loop of points representing the track.
 * Uses track characteristics to create a visually distinct shape that forms a proper loop.
 */
function generateTrackLoop(track: TrackDefinition, width: number, height: number): { x: number; y: number }[] {
  var sections = track.sections;
  var difficulty = track.difficulty || 3;
  var trackId = track.id || '';
  
  // Calculate track "character" from sections
  var totalLength = 0;
  var totalCurve = 0;
  var hasSCurves = false;
  var sharpestCurve = 0;
  
  if (sections && sections.length > 0) {
    for (var i = 0; i < sections.length; i++) {
      var section = sections[i];
      totalLength += section.length || 0;
      if (section.type === 'curve') {
        totalCurve += Math.abs(section.curve || 0) * (section.length || 1);
        if (Math.abs(section.curve || 0) > sharpestCurve) {
          sharpestCurve = Math.abs(section.curve || 0);
        }
      }
      if (section.type === 's_curve') hasSCurves = true;
    }
  }
  
  // Generate shape based on track character
  var points: { x: number; y: number }[] = [];
  var cx = width / 2;
  var cy = height / 2;
  var rx = (width - 8) / 2;
  var ry = (height - 6) / 2;
  var numPoints = 80;
  
  // Use track properties to influence the shape
  var complexity = Math.min(difficulty, 5);
  var wobbleFreq = 2 + complexity;
  var wobbleAmp = 0.05 + (sharpestCurve * 0.15);
  
  // Special shapes for certain track types
  if (trackId.indexOf('figure') >= 0 || (hasSCurves && difficulty >= 4)) {
    // Figure-8 style
    for (var i = 0; i <= numPoints; i++) {
      var t = (i / numPoints) * Math.PI * 2;
      var fx = Math.sin(t);
      var fy = Math.sin(t) * Math.cos(t);
      points.push({
        x: cx + fx * rx,
        y: cy + fy * ry * 1.5
      });
    }
  } else {
    // Oval with wobbles based on track character
    for (var i = 0; i <= numPoints; i++) {
      var angle = (i / numPoints) * Math.PI * 2;
      var wobble = Math.sin(angle * wobbleFreq) * wobbleAmp;
      var wobble2 = Math.cos(angle * (wobbleFreq + 1)) * wobbleAmp * 0.5;
      points.push({
        x: cx + Math.cos(angle) * rx * (1 + wobble),
        y: cy + Math.sin(angle) * ry * (1 + wobble2)
      });
    }
  }
  
  return points;
}

/**
 * Generate a path by simulating driving through track sections.
 * The curvature is scaled so the track naturally completes one full loop.
 */
function generatePathFromSections(sections: any[]): { x: number; y: number }[] {
  // First pass: calculate total "intended" curvature from sections
  var totalIntendedCurve = 0;
  for (var i = 0; i < sections.length; i++) {
    var section = sections[i];
    var segmentCount = section.length || 10;
    
    switch (section.type) {
      case 'curve':
        totalIntendedCurve += (section.curve || 0) * segmentCount;
        break;
      case 'ease_in':
        // Average curve over the section
        totalIntendedCurve += (section.targetCurve || 0.5) * segmentCount * 0.5;
        break;
      case 'ease_out':
        totalIntendedCurve += (section.targetCurve || 0.5) * segmentCount * 0.5;
        break;
      case 's_curve':
        // S-curves cancel out
        break;
    }
  }
  
  // Scale factor to make total curvature = 2*PI (one full loop)
  // If no curve data, default to making a loop
  var curveScale = 0.1;
  if (Math.abs(totalIntendedCurve) > 0.1) {
    curveScale = (Math.PI * 2) / Math.abs(totalIntendedCurve);
  }
  
  // Determine direction (clockwise or counterclockwise)
  var direction = totalIntendedCurve >= 0 ? 1 : -1;
  curveScale = Math.abs(curveScale) * direction;
  
  // Second pass: generate path with scaled curvature
  var points: { x: number; y: number }[] = [];
  var x = 0;
  var y = 0;
  var heading = 0; // Angle in radians, 0 = going right/east
  var currentCurve = 0;
  var stepSize = 1.0;
  
  points.push({ x: x, y: y });
  
  for (var i = 0; i < sections.length; i++) {
    var section = sections[i];
    var segmentCount = section.length || 10;
    
    switch (section.type) {
      case 'straight':
        // Move forward without turning
        for (var s = 0; s < segmentCount; s++) {
          x += Math.cos(heading) * stepSize;
          y += Math.sin(heading) * stepSize;
          points.push({ x: x, y: y });
        }
        currentCurve = 0;
        break;
        
      case 'curve':
        // Constant curve - turn while moving
        var curvature = (section.curve || 0) * curveScale;
        for (var s = 0; s < segmentCount; s++) {
          heading += curvature;
          x += Math.cos(heading) * stepSize;
          y += Math.sin(heading) * stepSize;
          points.push({ x: x, y: y });
        }
        currentCurve = section.curve || 0;
        break;
        
      case 'ease_in':
        // Gradually increase curvature
        var targetCurve = (section.targetCurve || 0.5) * curveScale;
        for (var s = 0; s < segmentCount; s++) {
          var t = s / segmentCount;
          var easedCurve = currentCurve * curveScale + (targetCurve - currentCurve * curveScale) * t;
          heading += easedCurve;
          x += Math.cos(heading) * stepSize;
          y += Math.sin(heading) * stepSize;
          points.push({ x: x, y: y });
        }
        currentCurve = section.targetCurve || 0.5;
        break;
        
      case 'ease_out':
        // Gradually decrease curvature to zero
        var startCurve = currentCurve * curveScale;
        for (var s = 0; s < segmentCount; s++) {
          var t = s / segmentCount;
          var easedCurve = startCurve * (1 - t);
          heading += easedCurve;
          x += Math.cos(heading) * stepSize;
          y += Math.sin(heading) * stepSize;
          points.push({ x: x, y: y });
        }
        currentCurve = 0;
        break;
        
      case 's_curve':
        // S-curve: turn one way then the other (mostly cancels out)
        var halfLen = Math.floor(segmentCount / 2);
        var sCurve = 0.06 * curveScale;
        // First half - turn one way
        for (var s = 0; s < halfLen; s++) {
          heading += sCurve;
          x += Math.cos(heading) * stepSize;
          y += Math.sin(heading) * stepSize;
          points.push({ x: x, y: y });
        }
        // Second half - turn back
        for (var s = 0; s < halfLen; s++) {
          heading -= sCurve;
          x += Math.cos(heading) * stepSize;
          y += Math.sin(heading) * stepSize;
          points.push({ x: x, y: y });
        }
        break;
    }
  }
  
  // Close the loop smoothly back to start
  var startX = points[0].x;
  var startY = points[0].y;
  var endX = points[points.length - 1].x;
  var endY = points[points.length - 1].y;
  
  var closeSteps = 15;
  for (var s = 1; s <= closeSteps; s++) {
    var t = s / closeSteps;
    // Smooth interpolation using ease function
    var smoothT = t * t * (3 - 2 * t);
    points.push({
      x: endX + (startX - endX) * smoothT,
      y: endY + (startY - endY) * smoothT
    });
  }
  
  return points;
}

/**
 * Normalize path to fit within display bounds with padding.
 */
function normalizeAndCenterPath(points: { x: number; y: number }[], width: number, height: number): { x: number; y: number }[] {
  if (points.length === 0) return points;
  
  // Find bounds
  var minX = points[0].x, maxX = points[0].x;
  var minY = points[0].y, maxY = points[0].y;
  
  for (var i = 1; i < points.length; i++) {
    if (points[i].x < minX) minX = points[i].x;
    if (points[i].x > maxX) maxX = points[i].x;
    if (points[i].y < minY) minY = points[i].y;
    if (points[i].y > maxY) maxY = points[i].y;
  }
  
  var pathWidth = maxX - minX;
  var pathHeight = maxY - minY;
  
  // Add padding
  var padding = 3;
  var availWidth = width - padding * 2;
  var availHeight = height - padding * 2;
  
  // Calculate scale to fit (maintain aspect ratio)
  var scaleX = pathWidth > 0 ? availWidth / pathWidth : 1;
  var scaleY = pathHeight > 0 ? availHeight / pathHeight : 1;
  var scale = Math.min(scaleX, scaleY);
  
  // Center the track
  var scaledWidth = pathWidth * scale;
  var scaledHeight = pathHeight * scale;
  var offsetX = padding + (availWidth - scaledWidth) / 2;
  var offsetY = padding + (availHeight - scaledHeight) / 2;
  
  // Transform points
  var result: { x: number; y: number }[] = [];
  for (var i = 0; i < points.length; i++) {
    result.push({
      x: (points[i].x - minX) * scale + offsetX,
      y: (points[i].y - minY) * scale + offsetY
    });
  }
  
  return result;
}

/**
 * Generate an oval track (simple).
 */
function generateOvalTrack(width: number, height: number): { x: number; y: number }[] {
  var points: { x: number; y: number }[] = [];
  var cx = width / 2;
  var cy = height / 2;
  var rx = (width - 8) / 2;
  var ry = (height - 4) / 2;
  
  var numPoints = 60;
  for (var i = 0; i <= numPoints; i++) {
    var angle = (i / numPoints) * Math.PI * 2;
    points.push({
      x: cx + Math.cos(angle) * rx,
      y: cy + Math.sin(angle) * ry
    });
  }
  
  return points;
}

/**
 * Draw the track surface - legacy, now handled by rasterizeTrack + renderHalfBlockMap.
 */
function drawTrackSurface(_points: { x: number; y: number }[], _x: number, _y: number, _width: number, _height: number, _theme: TrackTheme): void {
  // Now handled by rasterizeTrack and renderHalfBlockMap
}

/**
 * Draw start/finish line marker at hi-res pixel coordinates.
 */
function drawStartFinishHiRes(points: { x: number; y: number }[], screenX: number, screenY: number, _pixelWidth: number, pixelHeight: number, theme: TrackTheme): void {
  if (points.length < 2) return;
  
  // Find start point in pixel coordinates
  var startPixelX = Math.floor(points[0].x);
  var startPixelY = Math.floor(points[0].y);
  
  // Convert to character coordinates
  var charX = startPixelX;
  var charY = Math.floor(startPixelY / 2);
  
  // Clamp to bounds
  var maxCharY = Math.floor(pixelHeight / 2) - 1;
  charY = Math.max(0, Math.min(maxCharY, charY));
  
  // Draw checkered start marker
  console.gotoxy(screenX + charX, screenY + charY);
  console.attributes = WHITE;
  console.print(GLYPH.CHECKER);
  
  // Draw "S" marker
  console.gotoxy(screenX + charX + 1, screenY + charY);
  console.attributes = theme.sun.color.fg;
  console.print('S');
}

/**
 * Draw start/finish line marker - legacy.
 */
function drawStartFinish(_points: { x: number; y: number }[], _x: number, _y: number, _width: number, _height: number, _theme: TrackTheme): void {
  // Now handled by drawStartFinishHiRes
}

/**
 * Legacy function - kept for compatibility.
 */
function drawTrackRoute(_track: TrackDefinition): void {
  // Now handled by drawThemedTrackMap
}

/**
 * Draw controls at the bottom.
 */
function drawControls(state: SelectorState): void {
  console.gotoxy(1, 23);
  console.attributes = LIGHTMAGENTA;
  console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH));
  
  console.gotoxy(1, 24);
  console.attributes = LIGHTGRAY;
  
  if (state.mode === 'circuit') {
    console.print('  ');
    console.attributes = WHITE;
    console.print(String.fromCharCode(24) + '/' + String.fromCharCode(25));
    console.attributes = LIGHTGRAY;
    console.print(' Select  ');
    console.attributes = WHITE;
    console.print('ENTER');
    console.attributes = LIGHTGRAY;
    console.print(' Open  ');
    console.attributes = WHITE;
    console.print('H');
    console.attributes = LIGHTGRAY;
    console.print(' Scores  ');
    console.attributes = WHITE;
    console.print('Q');
    console.attributes = LIGHTGRAY;
    console.print(' Quit  ');
    console.attributes = DARKGRAY;
    console.print('?');
    console.attributes = DARKGRAY;
    console.print(' ???');
  } else {
    console.print('  ');
    console.attributes = WHITE;
    console.print(String.fromCharCode(24) + '/' + String.fromCharCode(25));
    console.attributes = LIGHTGRAY;
    console.print(' Select  ');
    console.attributes = WHITE;
    console.print('ENTER');
    console.attributes = LIGHTGRAY;
    console.print(' Race  ');
    console.attributes = WHITE;
    console.print('H');
    console.attributes = LIGHTGRAY;
    console.print(' Scores  ');
    console.attributes = WHITE;
    console.print(String.fromCharCode(27));
    console.attributes = LIGHTGRAY;
    console.print(' Back  ');
    console.attributes = WHITE;
    console.print('Q');
    console.attributes = LIGHTGRAY;
    console.print(' Quit  ');
    console.attributes = DARKGRAY;
    console.print('?');
    console.attributes = DARKGRAY;
    console.print(' ???');
  }
  
  console.gotoxy(1, 25);
  console.attributes = LIGHTMAGENTA;
  console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH));
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

/**
 * Repeat a character n times.
 */
function repeatChar(char: string, count: number): string {
  var result = '';
  for (var i = 0; i < count; i++) {
    result += char;
  }
  return result;
}

/**
 * Format time in seconds to mm:ss.
 */
function formatTime(seconds: number): string {
  var mins = Math.floor(seconds / 60);
  var secs = Math.floor(seconds % 60);
  return mins + ':' + (secs < 10 ? '0' : '') + secs;
}

/**
 * Pad string to the right.
 */
function padRight(str: string, len: number): string {
  while (str.length < len) {
    str += ' ';
  }
  return str.substring(0, len);
}
