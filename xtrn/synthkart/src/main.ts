/// <reference path="synchro/Compat.d.ts" />
/// <reference path="util/Math2D.ts" />
/// <reference path="util/Rand.ts" />
/// <reference path="util/DebugLogger.ts" />
/// <reference path="util/Logging.ts" />
/// <reference path="timing/Clock.ts" />
/// <reference path="timing/FixedTimestep.ts" />
/// <reference path="input/InputMap.ts" />
/// <reference path="input/Controls.ts" />
/// <reference path="entities/Entity.ts" />
/// <reference path="entities/Driver.ts" />
/// <reference path="entities/HumanDriver.ts" />
/// <reference path="entities/CpuDriver.ts" />
/// <reference path="entities/Vehicle.ts" />
/// <reference path="world/Road.ts" />
/// <reference path="world/TrackCatalog.ts" />
/// <reference path="world/Track.ts" />
/// <reference path="world/TrackLoader.ts" />
/// <reference path="world/Checkpoints.ts" />
/// <reference path="world/SpawnPoints.ts" />
/// <reference path="physics/Kinematics.ts" />
/// <reference path="physics/Steering.ts" />
/// <reference path="physics/Collision.ts" />
/// <reference path="items/Item.ts" />
/// <reference path="items/Mushroom.ts" />
/// <reference path="items/Shell.ts" />
/// <reference path="items/ItemSystem.ts" />
/// <reference path="hud/Hud.ts" />
/// <reference path="hud/Minimap.ts" />
/// <reference path="hud/Speedometer.ts" />
/// <reference path="hud/LapTimer.ts" />
/// <reference path="hud/PositionIndicator.ts" />
/// <reference path="highscores/HighScoreManager.ts" />
/// <reference path="highscores/HighScoreDisplay.ts" />
/// <reference path="render/cp437/Palette.ts" />
/// <reference path="render/cp437/GlyphAtlas.ts" />
/// <reference path="render/cp437/SceneComposer.ts" />
/// <reference path="render/cp437/RoadRenderer.ts" />
/// <reference path="render/cp437/SkylineRenderer.ts" />
/// <reference path="render/cp437/SpriteRenderer.ts" />
/// <reference path="render/cp437/HudRenderer.ts" />
/// <reference path="render/ansi/ANSILoader.ts" />
/// <reference path="render/Renderer.ts" />
/// <reference path="game/GameState.ts" />
/// <reference path="game/Systems.ts" />
/// <reference path="game/Cup.ts" />
/// <reference path="game/Game.ts" />
/// <reference path="ui/TrackSelector.ts" />
/// <reference path="ui/CupStandings.ts" />

/**
 * OutRun ANSI - Main Entry Point
 *
 * A synthwave racing game for Synchronet BBS.
 * Inspired by OutRun and Mario Kart.
 *
 * This file is the entry point loaded by Synchronet.
 * Build: npm run build → dist/outrun.js
 *
 * @author OutRun Team
 * @version 0.1.0 (Iteration 0 - Bootstrap)
 */

// ============================================================
// DEBUG LOGGER INITIALIZATION
// ============================================================

// Initialize file-based debug logger FIRST (even before BBS check)
// This ensures we capture any errors that occur during startup
// Uncomment the line below to enable debug logging to /sbbs/ctrl/outrun_debug.log
// debugLog.init();

// ============================================================
// BBS ENVIRONMENT CHECK
// ============================================================

// Check if we're running in a proper BBS terminal session
if (typeof console === 'undefined' || console === null) {
  // debugLog.error("No console object - not running in BBS session");
  // debugLog.close();
  print("ERROR: OutRun ANSI must be run from a Synchronet BBS terminal session.");
  print("This game cannot run directly with jsexec.");
  print("");
  print("To test, you need to either:");
  print("  1. Run from an actual BBS login session");
  print("  2. Use a terminal emulator that connects to your BBS");
  exit(1);
}

// ============================================================
// SYNCHRONET LIBRARY LOADING
// ============================================================

// Load Synchronet standard definitions
// This provides constants like K_NONE, color constants, etc.
// (Already loaded by bootstrap.js, but safe to call again)
// load("sbbsdefs.js");

// ============================================================
// TITLE SCREEN
// ============================================================

/**
 * Display the title screen.
 * Tries to load custom ANSI art from title.ans, falls back to built-in ASCII art.
 */
function showTitleScreen(): void {
  console.clear(BG_BLACK, false);
  
  // Try to load custom title screen (.bin preferred, .ans fallback)
  var titleFile = "";
  var assetsDir = js.exec_dir + "assets/";
  var f = new File(assetsDir + "title.bin");
  
  if (f.exists) {
    titleFile = assetsDir + "title.bin";
  } else {
    f = new File(assetsDir + "title.ans");
    if (f.exists) {
      titleFile = assetsDir + "title.ans";
    }
  }
  
  if (titleFile !== "") {
    try {
      // Load frame.js library
      load('frame.js');
      
      // Display custom art using Frame
      var titleFrame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK);
      titleFrame.open();
      titleFrame.load(titleFile);
      titleFrame.draw();
      titleFrame.close();
    } catch (e) {
      logError("Error loading custom title: " + e);
      // Fall through to built-in title
    }
  } else {
    // Fallback to built-in ASCII art
    console.attributes = LIGHTMAGENTA;

    var title = [
      "",
      "     .d88b.  db    db d888888b d8888b. db    db d8b   db",
      "    .8P  Y8. 88    88 `~~88~~' 88  `8D 88    88 888o  88",
      "    88    88 88    88    88    88oobY' 88    88 88V8o 88",
      "    88    88 88    88    88    88`8b   88    88 88 V8o88",
      "    `8b  d8' 88b  d88    88    88 `88. 88b  d88 88  V888",
      "     `Y88P'  ~Y8888P'    YP    88   YD ~Y8888P' VP   V8P",
      ""
    ];

    for (var i = 0; i < title.length; i++) {
      console.print(title[i] + "\r\n");
    }

    console.attributes = CYAN;
    console.print("              =========================================\r\n");
    console.attributes = LIGHTCYAN;
    console.print("                   A N S I   S Y N T H W A V E\r\n");
    console.print("                        R A C E R\r\n");
    console.attributes = CYAN;
    console.print("              =========================================\r\n");
    console.print("\r\n");

    // Synthwave grid decoration
    console.attributes = DARKGRAY;
    console.print("         /     |     \\         /     |     \\\r\n");
    console.print("        /      |      \\       /      |      \\\r\n");
    console.attributes = CYAN;
    console.print("    ===/=======+=======\\=====/=======+=======\\===\r\n");
    console.attributes = DARKGRAY;
    console.print("      /        |        \\   /        |        \\\r\n");
    console.print("\r\n");

    // Instructions
    console.attributes = WHITE;
    console.print("                    Controls:\r\n");
    console.attributes = LIGHTGRAY;
    console.print("        W/Up = Accelerate   A/Left = Steer Left\r\n");
    console.print("        S/Dn = Brake        D/Right = Steer Right\r\n");
    console.print("        SPACE = Use Item    P = Pause\r\n");
    console.print("\r\n");

    console.attributes = YELLOW;
    console.print("              Press any key to start racing...\r\n");
    console.print("                     Q to quit\r\n");
    console.print("\r\n");

    console.attributes = DARKGRAY;
    console.print("     Version 0.1.0 (Iteration 0) - Bootstrap Build\r\n");

    console.attributes = LIGHTGRAY;
  }
}

/**
 * Display the exit screen.
 * Tries to load custom ANSI art from exit.bin, falls back to simple text message.
 */
function showExitScreen(): void {
  console.clear(BG_BLACK, false);
  
  // Try to load custom exit screen (.bin preferred, .ans fallback)
  var exitFile = "";
  var assetsDir = js.exec_dir + "assets/";
  var f = new File(assetsDir + "exit.bin");
  
  if (f.exists) {
    exitFile = assetsDir + "exit.bin";
  } else {
    f = new File(assetsDir + "exit.ans");
    if (f.exists) {
      exitFile = assetsDir + "exit.ans";
    }
  }
  
  if (exitFile !== "") {
    try {
      // Load frame.js library
      load('frame.js');
      
      // Display custom art using Frame
      var exitFrame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK);
      exitFrame.open();
      exitFrame.load(exitFile);
      exitFrame.draw();
      exitFrame.close();
    } catch (e) {
      logError("Error loading custom exit screen: " + e);
      // Fall through to built-in message
      console.attributes = LIGHTGRAY;
      console.print("Thanks for playing SynthKart!\r\n");
    }
  } else {
    // Fallback to simple text message
    console.attributes = LIGHTGRAY;
    console.print("Thanks for playing SynthKart!\r\n");
  }
  
  // Wait for a keystroke before exiting
  console.pause();
}

/**
 * Wait for user input on title screen.
 * Returns true to start game, false to quit.
 */
function waitForTitleInput(): boolean {
  while (true) {
    var key = console.inkey(K_UPPER, 1000);

    if (key !== '') {
      if (key === 'Q') {
        return false;
      }
      return true;
    }
  }
}

/**
 * Show race end screen with results.
 * Waits for user to press a key before returning.
 */
function showRaceEndScreen(): void {
  console.clear(BG_BLACK, false);
  
  console.attributes = LIGHTMAGENTA;
  console.print("\r\n\r\n");
  console.print("  ========================================\r\n");
  console.attributes = LIGHTCYAN;
  console.print("             RACE COMPLETE!\r\n");
  console.attributes = LIGHTMAGENTA;
  console.print("  ========================================\r\n");
  console.print("\r\n");
  
  console.attributes = LIGHTGRAY;
  console.print("         Thanks for racing!\r\n");
  console.print("\r\n");
  
  console.attributes = YELLOW;
  console.print("    Press any key to continue...\r\n");
  console.print("\r\n");
  
  console.attributes = LIGHTGRAY;
  console.inkey(K_NONE, 30000);  // Wait up to 30 seconds
}

// ============================================================
// MAIN EXECUTION
// ============================================================

/**
 * Main game loop - cycles through splash -> track select -> race -> repeat
 * Only exits when user quits from splash screen.
 */
function main(): void {
  debugLog.separator("GAME START");
  debugLog.info("Entering main()");

  // Load json-db for high scores
  load('json-db.js');
  
  // Initialize high score manager
  var highScoreManager = new HighScoreManager();
  
  // Initialize cup manager for cup races
  var cupManager = new CupManager();

  try {
    // Main application loop - keeps running until user quits from splash
    var keepPlaying = true;
    
    while (keepPlaying) {
      // Show title screen
      debugLog.info("Showing title screen");
      showTitleScreen();

      // Wait for user input on title
      if (!waitForTitleInput()) {
        debugLog.info("User quit from title screen");
        keepPlaying = false;
        break;
      }

      // Show track selector
      debugLog.info("Showing track selector");
      var trackSelection = showTrackSelector(highScoreManager);
      
      if (!trackSelection.selected || !trackSelection.track) {
        // User pressed Q/ESC in track select - go back to splash
        debugLog.info("User went back from track selection");
        continue;  // Loop back to splash screen
      }

      debugLog.info("Selected track: " + trackSelection.track.name);

      // Show car selector - create a SceneComposer for the UI
      debugLog.info("Showing car selector");
      var carComposer = new SceneComposer(80, 24);
      var carSelection = CarSelector.show(carComposer);
      
      if (!carSelection.confirmed) {
        // User cancelled car selection - go back to track selector
        debugLog.info("User cancelled car selection");
        continue;
      }
      
      debugLog.info("Selected car: " + carSelection.carId + " color: " + carSelection.colorId);

      // Check if this is a cup (circuit) race
      if (trackSelection.isCircuitMode && trackSelection.circuitTracks) {
        // Cup mode - run multiple races
        runCupMode(
          trackSelection.circuitTracks,
          cupManager,
          highScoreManager,
          trackSelection.circuitId || 'custom_cup',
          trackSelection.circuitName || 'Circuit Cup',
          carSelection
        );
      } else {
        // Single race mode
        runSingleRace(trackSelection.track, highScoreManager, carSelection);
      }
      
      // Loop continues - back to splash screen
      debugLog.info("Returning to splash screen");
    }

    // Final exit
    showExitScreen();

  } catch (e) {
    // Error handling - capture to debug log
    debugLog.separator("FATAL ERROR");
    debugLog.exception("Uncaught exception in main()", e);
    
    console.clear(BG_BLACK, false);
    console.attributes = LIGHTRED;
    console.print("An error occurred: " + e + "\r\n");
    console.attributes = LIGHTGRAY;
    console.print("\r\nError details written to: outrun_debug.log\r\n");
    console.print("Press any key to exit...\r\n");
    console.inkey(K_NONE);
  } finally {
    // Always restore terminal state and close log
    console.attributes = LIGHTGRAY;
    // debugLog.separator("LOG END");
    // debugLog.close();
  }
}

/**
 * Run a single race (non-cup mode).
 */
function runSingleRace(track: TrackDefinition, highScoreManager: HighScoreManager, carSelection?: CarSelection): void {
  // Create and initialize game with selected track
  debugLog.separator("GAME INIT");
  var game = new Game(undefined, highScoreManager);
  game.initWithTrack(track, undefined, carSelection ? { carId: carSelection.carId, colorId: carSelection.colorId } : undefined);

  // Run game loop
  debugLog.separator("GAME LOOP");
  debugLog.info("Entering game loop");
  game.run();

  // Shutdown game instance
  debugLog.separator("GAME END");
  debugLog.info("Game loop ended");
  game.shutdown();

  // Show race results briefly
  showRaceEndScreen();
}

/**
 * Run a cup (circuit) with multiple races.
 */
function runCupMode(
  tracks: TrackDefinition[],
  cupManager: CupManager,
  highScoreManager: HighScoreManager,
  circuitId: string,
  circuitName: string,
  carSelection?: CarSelection
): void {
  debugLog.separator("CUP MODE START");
  debugLog.info("Starting cup with " + tracks.length + " tracks: " + circuitId);
  
  // Generate AI racer names
  var aiNames = ['MAX', 'LUNA', 'BLAZE', 'NOVA', 'TURBO', 'DASH', 'FLASH'];
  
  // Create cup definition using the actual circuit ID and name
  var cupDef: CupDefinition = {
    id: circuitId,
    name: circuitName,
    trackIds: [],
    description: circuitName + ' circuit'
  };
  for (var t = 0; t < tracks.length; t++) {
    cupDef.trackIds.push(tracks[t].id);
  }
  
  // Start the cup
  cupManager.startCup(cupDef, aiNames);
  
  // Show pre-race standings (Race 1 intro)
  showCupStandings(cupManager, true);
  
  // Race each track in sequence
  while (!cupManager.isCupComplete()) {
    var trackId = cupManager.getCurrentTrackId();
    if (!trackId) break;
    
    // Get track definition
    var track = getTrackDefinitionForCup(tracks, trackId);
    if (!track) {
      debugLog.error("Track not found in cup: " + trackId);
      break;
    }
    
    debugLog.info("Cup race " + cupManager.getCurrentRaceNumber() + ": " + track.name);
    
    // Create and run game for this race
    var game = new Game(undefined, highScoreManager);
    game.initWithTrack(track, undefined, carSelection ? { carId: carSelection.carId, colorId: carSelection.colorId } : undefined);
    game.run();
    
    // Get race results before shutdown
    var raceResults = game.getFinalRaceResults();
    game.shutdown();
    
    // Record results in cup manager
    if (raceResults) {
      cupManager.recordRaceResult(
        track.id,
        track.name,
        raceResults.positions,
        raceResults.playerTime,
        raceResults.playerBestLap
      );
    }
    
    // Show standings after each race
    if (!cupManager.isCupComplete()) {
      showCupStandings(cupManager, true);  // Pre-race for next race
    }
  }
  
  // Cup complete - show final standings then winner's circle
  showCupStandings(cupManager, false);  // Final standings
  showWinnersCircle(cupManager);
  
  // Handle cup high score (circuit time)
  var cupState = cupManager.getState();
  if (cupState && highScoreManager) {
    var position = highScoreManager.checkQualification(
      HighScoreType.CIRCUIT_TIME,
      cupDef.id,
      cupState.totalTime
    );
    
    if (position > 0) {
      var playerName = "Player";
      try {
        if (typeof user !== 'undefined' && user && user.alias) {
          playerName = user.alias;
        }
      } catch (e) {
        // user not available
      }
      
      highScoreManager.submitScore(
        HighScoreType.CIRCUIT_TIME,
        cupDef.id,
        playerName,
        cupState.totalTime,
        undefined,
        cupDef.name
      );
      
      // Show circuit high scores with player highlighted
      showHighScoreList(
        HighScoreType.CIRCUIT_TIME,
        cupDef.id,
        "=== CIRCUIT HIGH SCORES ===",
        cupDef.name,
        highScoreManager,
        position
      );
    }
  }
  
  // Clear cup state
  cupManager.clear();
  
  debugLog.separator("CUP MODE END");
}

/**
 * Find a track definition from a list by ID.
 */
function getTrackDefinitionForCup(tracks: TrackDefinition[], trackId: string): TrackDefinition | null {
  for (var i = 0; i < tracks.length; i++) {
    if (tracks[i].id === trackId) {
      return tracks[i];
    }
  }
  return null;
}

// Run the game
main();
