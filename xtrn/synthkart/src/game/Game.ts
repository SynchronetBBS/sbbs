/**
 * Game - Main game orchestrator.
 *
 * Coordinates all subsystems: input, physics, rendering, etc.
 */

interface GameConfig {
  screenWidth: number;
  screenHeight: number;
  tickRate: number;
  maxTicksPerFrame: number;
}

var DEFAULT_CONFIG: GameConfig = {
  screenWidth: 80,
  screenHeight: 24,
  tickRate: 60,
  maxTicksPerFrame: 5
};

class Game {
  private config: GameConfig;
  private running: boolean;
  private paused: boolean;

  // Subsystems
  private clock: Clock;
  private timestep: FixedTimestep;
  private inputMap: InputMap;
  private controls: Controls;
  private renderer: IRenderer;
  private trackLoader: TrackLoader;
  private hud: Hud;
  private physicsSystem: PhysicsSystem;
  private raceSystem: RaceSystem;
  private itemSystem: ItemSystem;
  private highScoreManager: HighScoreManager | null;

  // State
  private state: GameState | null;

  constructor(config?: GameConfig, highScoreManager?: HighScoreManager) {
    this.config = config || DEFAULT_CONFIG;
    this.running = false;
    this.paused = false;

    // Initialize subsystems
    this.clock = new Clock();
    this.timestep = new FixedTimestep({
      tickRate: this.config.tickRate,
      maxTicksPerFrame: this.config.maxTicksPerFrame
    });
    this.inputMap = new InputMap();
    this.controls = new Controls(this.inputMap);
    // Use FrameRenderer for layered Frame.js rendering
    this.renderer = new FrameRenderer(this.config.screenWidth, this.config.screenHeight);
    this.trackLoader = new TrackLoader();
    this.hud = new Hud();
    this.physicsSystem = new PhysicsSystem();
    this.raceSystem = new RaceSystem();
    this.itemSystem = new ItemSystem();
    this.highScoreManager = highScoreManager || null;

    this.state = null;
  }

  /**
   * Initialize the game with a track definition.
   * @param trackDef - Track definition
   * @param raceMode - Race mode (defaults to GRAND_PRIX)
   * @param carSelection - Optional car selection (carId and colorId)
   */
  initWithTrack(trackDef: TrackDefinition, raceMode?: RaceMode, carSelection?: { carId: string; colorId: string }): void {
    logInfo("Game.initWithTrack(): " + trackDef.name + " mode: " + (raceMode || RaceMode.GRAND_PRIX));

    // Default to Grand Prix mode (racing against opponents)
    var mode = raceMode || RaceMode.GRAND_PRIX;

    // Initialize renderer
    this.renderer.init();
    
    // Set theme based on track's themeId
    var themeMapping: { [key: string]: string } = {
      'synthwave': 'synthwave',
      'midnight_city': 'city_night',
      'beach_paradise': 'sunset_beach',
      'forest_night': 'twilight_forest',
      'haunted_hollow': 'haunted_hollow',
      'winter_wonderland': 'winter_wonderland',
      'cactus_canyon': 'cactus_canyon',
      'tropical_jungle': 'tropical_jungle',
      'candy_land': 'candy_land',
      'rainbow_road': 'rainbow_road',
      'dark_castle': 'dark_castle',
      'villains_lair': 'villains_lair',
      'ancient_ruins': 'ancient_ruins',
      'thunder_stadium': 'thunder_stadium',
      'glitch_circuit': 'glitch_circuit',
      'kaiju_rampage': 'kaiju_rampage',
      'underwater_grotto': 'underwater_grotto',
      'ansi_tunnel': 'ansi_tunnel'
    };
    var themeName = themeMapping[trackDef.themeId] || 'synthwave';
    if (this.renderer.setTheme) {
      this.renderer.setTheme(themeName);
    }

    // Build the road from the track definition
    var road = buildRoadFromDefinition(trackDef);

    // Load legacy track structure (for checkpoints/items - will be removed later)
    var track = this.trackLoader.load("neon_coast_01");
    track.laps = trackDef.laps;  // Override with definition's lap count
    track.name = trackDef.name;

    // Apply car stats if a car was selected
    var selectedCarId = carSelection ? carSelection.carId : 'sports';
    var selectedColorId = carSelection ? carSelection.colorId : 'yellow';
    applyCarStats(selectedCarId);

    // Create player vehicle with selected car
    var playerVehicle = new Vehicle();
    playerVehicle.driver = new HumanDriver(this.controls);
    playerVehicle.isNPC = false;  // Ensure player is not marked as NPC
    playerVehicle.carId = selectedCarId;
    playerVehicle.carColorId = selectedColorId;
    
    // Get the car color for display
    var carColor = getCarColor(selectedColorId);
    playerVehicle.color = carColor ? carColor.body : YELLOW;

    // Create game state with road and race mode
    this.state = createInitialState(track, trackDef, road, playerVehicle, mode);

    // Spawn vehicles based on race mode
    if (mode === RaceMode.GRAND_PRIX) {
      // Grand Prix: spawn 7 CPU racers on starting grid, no commuters
      this.spawnRacers(7, road);
      // Position all vehicles on starting grid
      this.positionOnStartingGrid(road);
      
      // Block ALL drivers from moving during countdown
      for (var i = 0; i < this.state.vehicles.length; i++) {
        var v = this.state.vehicles[i];
        var drv = v.driver as any;
        if (drv && drv.setCanMove) {
          drv.setCanMove(false);
        }
      }
    } else {
      // Time Trial: spawn some commuter traffic for obstacles
      var npcCount = trackDef.npcCount !== undefined ? trackDef.npcCount : 5;
      this.spawnNPCs(npcCount, road);
      // Player starts at beginning
      playerVehicle.trackZ = 0;
      playerVehicle.playerX = 0;
    }

    // Initialize systems
    this.physicsSystem.init(this.state);
    this.raceSystem.init(this.state);
    this.itemSystem.initFromTrack(track, road);
    
    // Register item system callbacks for visual effects
    var renderer = this.renderer;
    this.itemSystem.setCallbacks({
      onLightningStrike: function(hitCount: number) {
        if (renderer.triggerLightningStrike) {
          renderer.triggerLightningStrike(hitCount);
        }
      }
    });

    // Initialize HUD with race start time (will be reset to 0 when countdown finishes)
    this.hud.init(this.state.time);

    this.running = true;
    // Don't set racing=true yet - wait for countdown to finish
    this.state.racing = false;

    debugLog.info("Game initialized with track: " + trackDef.name);
    debugLog.info("  Race mode: " + mode);
    debugLog.info("  Road segments: " + road.segments.length);
    debugLog.info("  Road length: " + road.totalLength);
    debugLog.info("  Laps: " + road.laps);
    debugLog.info("  Total racers: " + this.state.vehicles.length);
  }

  /**
   * Initialize the game (legacy - uses default track).
   */
  init(): void {
    logInfo("Game.init()");
    // Use the default test track for backwards compatibility
    var defaultTrack = getTrackDefinition('test_oval');
    if (defaultTrack) {
      this.initWithTrack(defaultTrack, RaceMode.GRAND_PRIX);
    } else {
      // Fallback to hardcoded if catalog fails
      this.initWithTrack({
        id: 'fallback',
        name: 'Fallback Track',
        description: 'Default fallback',
        difficulty: 1,
        laps: 2,
        themeId: 'synthwave',
        estimatedLapTime: 30,
        sections: [
          { type: 'straight', length: 15 },
          { type: 'curve', length: 15, curve: 0.5 },
          { type: 'straight', length: 15 },
          { type: 'curve', length: 15, curve: 0.5 }
        ]
      });
    }
  }

  /**
   * Main game loop.
   */
  run(): void {
    debugLog.info("Entering game loop");

    this.clock.reset();
    var frameCount = 0;
    var lastLogTime = 0;

    while (this.running) {
      // 1. Measure elapsed real time
      var deltaMs = this.clock.getDelta();
      frameCount++;

      // 2. Process input
      this.processInput();

      // 3. Run fixed timestep logic updates
      if (!this.paused && this.state) {
        var ticks = this.timestep.update(deltaMs);
        for (var i = 0; i < ticks; i++) {
          this.tick(this.timestep.getDt());
        }
        
        // Clear just-pressed flags after tick processing
        this.controls.endFrame();
        
        // Log vehicle state every second
        if (this.state.time - lastLogTime >= 1.0) {
          debugLog.logVehicle(this.state.playerVehicle);
          lastLogTime = this.state.time;
        }
        
        // Check for race finish - show game over screen
        if (this.state.finished && this.state.racing === false) {
          debugLog.info("Race complete! Final time: " + this.state.time.toFixed(2));
          this.showGameOverScreen();
          this.running = false;
        }
      }

      // 4. Render
      this.render();

      // 5. Yield to Synchronet
      mswait(1);
    }
  }

  /**
   * Process input (called every frame).
   */
  private processInput(): void {
    var now = this.clock.now();

    // Read all available keys
    var key: string;
    while ((key = console.inkey(K_NONE, 0)) !== '') {
      this.controls.handleKey(key, now);
    }

    // Update held state (decays old inputs)
    this.controls.update(now);

    // Handle immediate actions AFTER processing all keys
    if (this.controls.wasJustPressed(GameAction.QUIT)) {
      debugLog.info("QUIT action triggered - exiting game loop");
      this.running = false;
      this.controls.endFrame();  // Clear just-pressed flags
      return;
    }
    if (this.controls.wasJustPressed(GameAction.PAUSE)) {
      this.togglePause();
      this.controls.endFrame();  // Clear just-pressed flags
      return;
    }
    // NOTE: endFrame() is now called after tick() in the main loop
    // so that USE_ITEM and other actions can be detected in tick()
  }

  /**
   * Single logic tick.
   */
  private tick(dt: number): void {
    if (!this.state) return;

    // Handle countdown before race starts
    if (!this.state.raceStarted && this.state.raceMode === RaceMode.GRAND_PRIX) {
      this.state.countdown -= dt;
      
      if (this.state.countdown <= 0) {
        this.state.raceStarted = true;
        this.state.racing = true;  // Now the race is actually racing
        this.state.countdown = 0;
        // Reset time to 0 when race actually starts
        this.state.time = 0;
        this.state.lapStartTime = 0;
        // Reinitialize HUD times now that race is starting for real
        this.hud.init(0);
        
        // Enable ALL drivers to move
        for (var i = 0; i < this.state.vehicles.length; i++) {
          var vehicle = this.state.vehicles[i];
          var driver = vehicle.driver as any;
          if (driver && driver.setCanMove) {
            driver.setCanMove(true);
          }
        }
        debugLog.info("Race started! GO!");
      }
      // During countdown, don't update physics - just render
      return;
    }

    // Update game time (only after race starts)
    this.state.time += dt;

    // Update physics
    this.physicsSystem.update(this.state, dt);

    // Update race progress
    this.raceSystem.update(this.state, dt);

    // Activate dormant NPCs when player approaches (not for racers)
    if (this.state.raceMode !== RaceMode.GRAND_PRIX) {
      this.activateDormantNPCs();
      // Apply NPC pacing - commuters drive faster when far, slower when close
      this.applyNPCPacing();
    }

    // Update items (including shell projectiles)
    this.itemSystem.update(dt, this.state.vehicles, this.state.road.totalLength);
    this.itemSystem.checkPickups(this.state.vehicles, this.state.road);

    // Use item if requested (consume to prevent multi-tick triggering)
    if (this.controls.consumeJustPressed(GameAction.USE_ITEM)) {
      // Determine if we should fire backward (for green shells)
      // Fire backward if the last accel/decel action was braking
      var fireBackward = this.controls.getLastAccelAction() < 0;
      
      // Store current speed and accel state to preserve it after item use
      var currentSpeed = this.state.playerVehicle.speed;
      var currentAccel = this.controls.getAcceleration();
      
      this.itemSystem.useItem(this.state.playerVehicle, this.state.vehicles, fireBackward);
      
      // Preserve speed after item use - don't let the BBS input quirk kill our momentum
      // Only restore if we weren't braking (currentAccel >= 0) and we lost speed
      if (currentAccel >= 0 && this.state.playerVehicle.speed < currentSpeed) {
        this.state.playerVehicle.speed = currentSpeed;
      }
    }
    
    // Process AI item usage (racers only, not commuters)
    for (var i = 0; i < this.state.vehicles.length; i++) {
      var vehicle = this.state.vehicles[i];
      if (vehicle === this.state.playerVehicle) continue;  // Skip player
      if (!vehicle.isRacer) continue;  // Only racers use items
      if (!vehicle.driver) continue;
      
      // Get AI intent (already generated in PhysicsSystem, but we need it here)
      // Check if this vehicle wants to use an item
      var intent = vehicle.driver.update(vehicle, this.state.track, dt);
      if (intent.useItem && vehicle.heldItem !== null) {
        this.itemSystem.useItem(vehicle, this.state.vehicles);
      }
    }

    // Process vehicle-to-vehicle collisions
    Collision.processVehicleCollisions(this.state.vehicles);

    // Respawn NPCs that have fallen behind the player (not in race mode)
    if (this.state.raceMode !== RaceMode.GRAND_PRIX) {
      this.checkNPCRespawn();
    }

    // Update camera to follow player
    this.state.cameraX = this.state.playerVehicle.x;

    // Check for race finish - now handled by game over screen
    // Don't exit immediately, let the render loop show results
  }

  /**
   * Wait for the user to press ENTER to dismiss game over screen.
   */
  private showGameOverScreen(): void {
    if (!this.state) return;
    
    debugLog.info("Showing game over screen, waiting for ENTER...");
    
    // Calculate final results
    var player = this.state.playerVehicle;
    var finalPosition = player.racePosition;
    var finalTime = this.state.time;
    var bestLap = this.state.bestLapTime > 0 ? this.state.bestLapTime : 0;
    
    // Check if player qualified for high scores
    var trackTimePosition = 0;
    var lapTimePosition = 0;
    
    if (this.highScoreManager && this.state.trackDefinition) {
      var trackId = this.state.trackDefinition.id;
      
      // Check track time qualification
      trackTimePosition = this.highScoreManager.checkQualification(
        HighScoreType.TRACK_TIME,
        trackId,
        finalTime
      );
      
      // Check best lap time qualification
      if (bestLap > 0) {
        lapTimePosition = this.highScoreManager.checkQualification(
          HighScoreType.LAP_TIME,
          trackId,
          bestLap
        );
      }
      
      // Submit scores if qualified
      var playerName = "Player";
      try {
        if (typeof user !== 'undefined' && user && user.alias) {
          playerName = user.alias;
        }
      } catch (e) {
        // user not available, keep default
      }
      
      if (trackTimePosition > 0) {
        this.highScoreManager.submitScore(
          HighScoreType.TRACK_TIME,
          trackId,
          playerName,
          finalTime,
          this.state.track.name
        );
        logInfo("NEW HIGH SCORE! Track time #" + trackTimePosition + ": " + finalTime.toFixed(2));
      }
      
      if (lapTimePosition > 0) {
        this.highScoreManager.submitScore(
          HighScoreType.LAP_TIME,
          trackId,
          playerName,
          bestLap,
          this.state.track.name
        );
        logInfo("NEW HIGH SCORE! Lap time #" + lapTimePosition + ": " + bestLap.toFixed(2));
      }
    }
    
    // Render results screen once (simple stats only)
    this.renderResultsScreen(finalPosition, finalTime, bestLap);
    
    // Wait for ENTER key
    while (true) {
      var key = console.inkey(K_NONE, 100);
      if (key === '\r' || key === '\n') {
        debugLog.info("ENTER pressed, exiting game over screen");
        break;
      }
    }
    
    // Show two-column high scores if player made any list
    if (this.highScoreManager && this.state.trackDefinition && (trackTimePosition > 0 || lapTimePosition > 0)) {
      var trackId = this.state.trackDefinition.id;
      showTwoColumnHighScores(
        trackId,
        this.state.track.name,
        this.highScoreManager,
        trackTimePosition,
        lapTimePosition
      );
    }
  }
  
  /**
   * Render a dedicated results screen (no game view, just results).
   * Uses fixed 80x24 viewport for consistent layout.
   */
  private renderResultsScreen(
    position: number,
    totalTime: number,
    bestLap: number
  ): void {
    // Clear the entire screen first
    console.clear(BG_BLACK, false);
    
    // Colors
    var titleAttr = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
    var labelAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
    var valueAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
    var boxAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
    var promptAttr = colorToAttr({ fg: LIGHTMAGENTA, bg: BG_BLACK });
    
    // Use fixed 80x24 viewport, centered within that
    var viewWidth = 80;
    var viewHeight = 24;
    var boxWidth = 40;
    var boxHeight = 12;
    var boxX = Math.floor((viewWidth - boxWidth) / 2);
    var topY = Math.floor((viewHeight - boxHeight) / 2);
    
    // Draw box border using gotoxy for precise positioning
    console.gotoxy(boxX + 1, topY + 1);
    console.attributes = boxAttr;
    console.print(GLYPH.DBOX_TL);
    for (var i = 1; i < boxWidth - 1; i++) {
      console.print(GLYPH.DBOX_H);
    }
    console.print(GLYPH.DBOX_TR);
    
    for (var j = 1; j < boxHeight - 1; j++) {
      console.gotoxy(boxX + 1, topY + 1 + j);
      console.print(GLYPH.DBOX_V);
      // Fill interior with spaces
      for (var i = 1; i < boxWidth - 1; i++) {
        console.print(' ');
      }
      console.print(GLYPH.DBOX_V);
    }
    
    console.gotoxy(boxX + 1, topY + boxHeight);
    console.print(GLYPH.DBOX_BL);
    for (var i = 1; i < boxWidth - 1; i++) {
      console.print(GLYPH.DBOX_H);
    }
    console.print(GLYPH.DBOX_BR);
    
    // Title
    var title = "=== RACE COMPLETE ===";
    console.gotoxy(boxX + 1 + Math.floor((boxWidth - title.length) / 2), topY + 3);
    console.attributes = titleAttr;
    console.print(title);
    
    // Position
    var posSuffix = PositionIndicator.getOrdinalSuffix(position);
    console.gotoxy(boxX + 5, topY + 5);
    console.attributes = labelAttr;
    console.print("FINAL POSITION:");
    console.gotoxy(boxX + 23, topY + 5);
    console.attributes = valueAttr;
    console.print(position + posSuffix);
    
    // Total time
    console.gotoxy(boxX + 5, topY + 6);
    console.attributes = labelAttr;
    console.print("TOTAL TIME:");
    console.gotoxy(boxX + 23, topY + 6);
    console.attributes = valueAttr;
    console.print(LapTimer.format(totalTime));
    
    // Best lap
    console.gotoxy(boxX + 5, topY + 7);
    console.attributes = labelAttr;
    console.print("BEST LAP:");
    console.gotoxy(boxX + 23, topY + 7);
    console.attributes = valueAttr;
    console.print(bestLap > 0 ? LapTimer.format(bestLap) : "--:--.--");
    
    // Track name
    console.gotoxy(boxX + 5, topY + 9);
    console.attributes = labelAttr;
    console.print("TRACK:");
    console.gotoxy(boxX + 23, topY + 9);
    console.attributes = valueAttr;
    console.print(this.state!.track.name);
    
    // Prompt
    var prompt = "Press ENTER to continue";
    console.gotoxy(boxX + 1 + Math.floor((boxWidth - prompt.length) / 2), topY + 11);
    console.attributes = promptAttr;
    console.print(prompt);
  }

  /**
   * Activate dormant NPCs when the player approaches them.
   * NPCs start stationary and "wake up" when player is within range.
   */
  private activateDormantNPCs(): void {
    if (!this.state) return;
    
    var playerZ = this.state.playerVehicle.trackZ;
    var roadLength = this.state.road.totalLength;
    
    for (var i = 0; i < this.state.vehicles.length; i++) {
      var npc = this.state.vehicles[i];
      if (!npc.isNPC || npc.isRacer) continue;  // Skip non-NPCs and racers
      
      var driver = npc.driver as CommuterDriver;
      if (driver.isActive()) continue;  // Already active
      
      // Calculate distance ahead (handling wrap-around)
      var dist = npc.trackZ - playerZ;
      if (dist < 0) dist += roadLength;  // Handle wrap-around
      
      // Activate if player is approaching (within activation range)
      if (dist < driver.getActivationRange()) {
        driver.activate();
        debugLog.info("NPC activated at distance " + dist.toFixed(0));
      }
    }
  }

  /**
   * Check if any NPCs need to be respawned ahead.
   * NPCs are respawned to maintain even distribution around the track.
   */
  private checkNPCRespawn(): void {
    if (!this.state) return;
    
    var playerZ = this.state.playerVehicle.trackZ;
    var roadLength = this.state.road.totalLength;
    var respawnDistance = 100;  // Respawn if this far behind player
    
    // Get all commuter NPCs (not racers)
    var npcs: IVehicle[] = [];
    for (var i = 0; i < this.state.vehicles.length; i++) {
      if (this.state.vehicles[i].isNPC && !this.state.vehicles[i].isRacer) {
        npcs.push(this.state.vehicles[i]);
      }
    }
    
    if (npcs.length === 0) return;
    
    // Calculate ideal spacing for even distribution around track
    var idealSpacing = roadLength / npcs.length;
    
    // Count how many NPCs need respawning this frame
    var respawnCount = 0;
    for (var j = 0; j < npcs.length; j++) {
      var npc = npcs[j];
      var distBehind = playerZ - npc.trackZ;
      if (distBehind > respawnDistance) {
        // Respawn this NPC at its own unique offset ahead of player
        // Each respawning NPC gets a different slot to prevent bunching
        var slotOffset = idealSpacing * (respawnCount + 1);
        var newZ = (playerZ + slotOffset) % roadLength;
        
        npc.trackZ = newZ;
        npc.z = newZ;
        
        // Reset to dormant so it activates when player approaches
        var driver = npc.driver as CommuterDriver;
        driver.deactivate();
        npc.speed = 0;
        
        // Randomize lane
        var laneChoice = Math.random();
        if (laneChoice < 0.4) {
          npc.playerX = -0.35 + (Math.random() - 0.5) * 0.2;
        } else if (laneChoice < 0.8) {
          npc.playerX = 0.35 + (Math.random() - 0.5) * 0.2;
        } else {
          npc.playerX = (Math.random() - 0.5) * 0.3;
        }
        
        npc.isCrashed = false;
        npc.crashTimer = 0;
        npc.flashTimer = 0;
        
        respawnCount++;
      }
    }
  }

  /**
   * Apply NPC pacing - commuters slow down slightly when player approaches
   * to ensure player can catch and pass them.
   */
  private applyNPCPacing(): void {
    if (!this.state) return;
    
    var playerZ = this.state.playerVehicle.trackZ;
    var roadLength = this.state.road.totalLength;
    
    for (var i = 0; i < this.state.vehicles.length; i++) {
      var npc = this.state.vehicles[i];
      if (!npc.isNPC) continue;
      
      // Skip dormant NPCs
      var driver = npc.driver as CommuterDriver;
      if (!driver.isActive()) continue;
      
      // Calculate distance ahead (handling wrap-around)
      var distance = npc.trackZ - playerZ;
      if (distance < 0) distance += roadLength;
      
      // Commuters always drive slower than max so player can catch them
      // Base speed is 30-50% of max (from CommuterDriver.speedFactor)
      var commuterBaseSpeed = VEHICLE_PHYSICS.MAX_SPEED * driver.getSpeedFactor();
      
      // When player is close (within 100 units), slow down slightly
      // This ensures the player can always catch up
      if (distance < 100) {
        var slowFactor = 0.7 + (distance / 100) * 0.3;  // 70-100% of base speed
        npc.speed = commuterBaseSpeed * slowFactor;
      } else {
        // Normal commuter speed when player is far
        npc.speed = commuterBaseSpeed;
      }
    }
  }

  /**
   * Render current state.
   */
  private render(): void {
    if (!this.state) return;

    var trackZ = this.state.playerVehicle.z;
    var vehicle = this.state.playerVehicle;
    var road = this.state.road;
    
    // Get curvature at player position for parallax
    var curvature = road.getCurvature(trackZ);
    var playerSteer = vehicle.playerX;  // Player's lateral position indicates steering direction
    // Pass 0 speed when paused so animations freeze
    var speed = this.paused ? 0 : vehicle.speed;
    var dt = 1.0 / this.config.tickRate;  // Fixed timestep

    // Set brake light state based on acceleration input
    // Brake lights on when: actively braking, stopped, or coasting (not accelerating)
    var accel = this.controls.getAcceleration();
    var brakeLightsOn = accel <= 0;  // On when braking (accel < 0) or not accelerating (accel === 0)
    if (this.renderer.setBrakeLightState) {
      this.renderer.setBrakeLightState(brakeLightsOn);
    }

    this.renderer.beginFrame();
    this.renderer.renderSky(trackZ, curvature, playerSteer, speed, dt);
    this.renderer.renderRoad(trackZ, this.state.cameraX, this.state.track, this.state.road);
    this.renderer.renderEntities(
      this.state.playerVehicle,
      this.state.vehicles,
      this.itemSystem.getItemBoxes(),
      this.itemSystem.getProjectiles()
    );

    // Compute and render HUD
    var hudData = this.hud.compute(
      this.state.playerVehicle,
      this.state.track,
      this.state.road,
      this.state.vehicles,
      this.state.time,
      this.state.countdown,
      this.state.raceMode
    );
    this.renderer.renderHud(hudData);

    // Render pause overlay if paused
    if (this.paused) {
      this.renderPauseOverlay();
    }

    this.renderer.endFrame();
  }

  /**
   * Render pause overlay with flashing rainbow "PAUSED" text.
   */
  private renderPauseOverlay(): void {
    // Large ASCII art "PAUSED" text (5 rows tall)
    var pausedArt = [
      ' ####   ###  #   # ### #### ####  ',
      ' #   # #   # #   # #   #    #   # ',
      ' ####  ##### #   # ### #### #   # ',
      ' #     #   # #   #   # #    #   # ',
      ' #     #   #  ###  ### #### ####  '
    ];
    
    // Rainbow color sequence
    var rainbowColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
    
    // Cycle color based on time (using system.timer)
    var colorIndex = Math.floor((system.timer * 8)) % rainbowColors.length;
    var textColor = rainbowColors[colorIndex];
    var attr = makeAttr(textColor, BG_BLACK);
    
    // Calculate center position
    var textWidth = pausedArt[0].length;
    var textHeight = pausedArt.length;
    var startX = Math.floor((80 - textWidth) / 2);
    var startY = Math.floor((24 - textHeight) / 2);
    
    // Get the HUD frame from renderer to draw overlay (top layer)
    var hudFrame = this.renderer.getHudFrame ? this.renderer.getHudFrame() : null;
    if (!hudFrame) return;
    
    // Draw semi-transparent background box (Frame uses 1-based coords)
    var boxPadding = 2;
    var boxAttr = makeAttr(DARKGRAY, BG_BLACK);
    for (var by = startY - boxPadding; by < startY + textHeight + boxPadding; by++) {
      for (var bx = startX - boxPadding; bx < startX + textWidth + boxPadding; bx++) {
        if (bx >= 0 && bx < 80 && by >= 0 && by < 24) {
          hudFrame.setData(bx + 1, by + 1, GLYPH.MEDIUM_SHADE, boxAttr);
        }
      }
    }
    
    // Draw the PAUSED text
    for (var row = 0; row < textHeight; row++) {
      var line = pausedArt[row];
      for (var col = 0; col < line.length; col++) {
        var ch = line.charAt(col);
        var x = startX + col;
        var y = startY + row;
        if (x >= 0 && x < 80 && y >= 0 && y < 24) {
          if (ch !== ' ') {
            hudFrame.setData(x + 1, y + 1, GLYPH.FULL_BLOCK, attr);
          }
        }
      }
    }
    
    // Draw "Press P to resume" below
    var resumeText = 'Press P to resume';
    var resumeX = Math.floor((80 - resumeText.length) / 2);
    var resumeY = startY + textHeight + 2;
    var resumeAttr = makeAttr(WHITE, BG_BLACK);
    for (var i = 0; i < resumeText.length; i++) {
      if (resumeX + i >= 0 && resumeX + i < 80 && resumeY >= 0 && resumeY < 24) {
        hudFrame.setData(resumeX + i + 1, resumeY + 1, resumeText.charAt(i), resumeAttr);
      }
    }
  }

  /**
   * Toggle pause state.
   */
  private togglePause(): void {
    this.paused = !this.paused;
    if (!this.paused) {
      this.clock.reset();
      this.timestep.reset();
    }
    logInfo("Game " + (this.paused ? "paused" : "resumed"));
  }

  /**
   * Spawn CPU racer vehicles for Grand Prix mode.
   * Creates skilled AI opponents that actually race.
   */
  private spawnRacers(count: number, _road: Road): void {
    if (!this.state) return;
    
    // Racer colors - distinct from player's yellow
    var racerColors = [
      { body: LIGHTRED, highlight: WHITE },
      { body: LIGHTBLUE, highlight: LIGHTCYAN },
      { body: LIGHTGREEN, highlight: WHITE },
      { body: LIGHTMAGENTA, highlight: WHITE },
      { body: LIGHTCYAN, highlight: WHITE },
      { body: WHITE, highlight: LIGHTGRAY },
      { body: BROWN, highlight: YELLOW }
    ];
    
    // Skill levels for 7 opponents - Road Rash style mix
    // 2 front-runners, 2 mid-pack, 3 back-markers for testability
    var skillLevels = [0.82, 0.75, 0.58, 0.52, 0.42, 0.38, 0.35];
    
    for (var i = 0; i < count && i < racerColors.length; i++) {
      var racer = new Vehicle();
      
      // Use RacerDriver for competitive AI
      var skill = skillLevels[i] || 0.6;
      racer.driver = new RacerDriver(skill);
      racer.isNPC = true;   // AI-controlled vehicle
      racer.isRacer = true; // Mark as racer for position calculation
      
      // Randomize vehicle type
      var typeIndex = Math.floor(Math.random() * NPC_VEHICLE_TYPES.length);
      racer.npcType = NPC_VEHICLE_TYPES[typeIndex];
      
      // Assign distinct color
      var colorPalette = racerColors[i];
      racer.color = colorPalette.body;
      racer.npcColorIndex = i;
      
      // Position will be set by positionOnStartingGrid()
      racer.trackZ = 0;
      racer.z = 0;
      racer.playerX = 0;
      
      this.state.vehicles.push(racer);
    }
    
    debugLog.info("Spawned " + count + " CPU racers for Grand Prix");
  }

  /**
   * Position all vehicles on a starting grid.
   * Player starts at front, AI racers in 2-wide rows behind.
   */
  private positionOnStartingGrid(_road: Road): void {
    if (!this.state) return;
    
    var vehicles = this.state.vehicles;
    var gridRowSpacing = 20;    // Z spacing between rows
    var gridColSpacing = 0.5;   // X spacing for 2-wide formation
    
    // Find player vehicle
    var playerIdx = -1;
    for (var p = 0; p < vehicles.length; p++) {
      if (!vehicles[p].isNPC) {
        playerIdx = p;
        break;
      }
    }
    
    // Calculate total grid length (7 AI cars = 4 rows: 1 solo + 3 pairs = 4 rows)
    // Player goes at the front, ahead of all AI cars
    var numAICars = vehicles.length - 1;
    var totalGridLength = Math.ceil(numAICars / 2) * gridRowSpacing;
    
    // Position vehicles from back to front
    // Player gets highest trackZ (furthest ahead)
    var aiSlot = 0;
    for (var v = 0; v < vehicles.length; v++) {
      var vehicle = vehicles[v];
      
      if (v === playerIdx) {
        // Player at very front (highest Z)
        vehicle.trackZ = totalGridLength + gridRowSpacing;
        vehicle.z = vehicle.trackZ;
        vehicle.playerX = 0;  // Center
      } else {
        // AI cars in 2-wide rows behind player
        var row = Math.floor(aiSlot / 2);
        var col = aiSlot % 2;
        
        vehicle.trackZ = totalGridLength - (row * gridRowSpacing);
        vehicle.z = vehicle.trackZ;
        vehicle.playerX = (col === 0) ? -gridColSpacing : gridColSpacing;
        
        aiSlot++;
      }
      
      // Reset vehicle state
      vehicle.speed = 0;
      vehicle.lap = 1;
      vehicle.checkpoint = 0;
      vehicle.racePosition = 1;  // Will be calculated properly first frame
      
      debugLog.info("Grid slot " + v + " (NPC=" + vehicle.isNPC + "): trackZ=" + vehicle.trackZ.toFixed(0) + " X=" + vehicle.playerX.toFixed(2));
    }
    
    debugLog.info("Positioned " + vehicles.length + " vehicles on starting grid (player at front)");
  }

  /**
   * Spawn NPC commuter vehicles.
   * Distributes them evenly around the entire track.
   */
  private spawnNPCs(count: number, road: Road): void {
    if (!this.state) return;
    
    var roadLength = road.totalLength;
    
    // Distribute NPCs evenly around the ENTIRE track
    // This ensures consistent traffic density regardless of where player is
    var spacing = roadLength / count;
    
    for (var i = 0; i < count; i++) {
      var npc = new Vehicle();
      
      // Use CommuterDriver for traffic
      npc.driver = new CommuterDriver();
      npc.isNPC = true;
      
      // Randomize vehicle type and color
      var typeIndex = Math.floor(Math.random() * NPC_VEHICLE_TYPES.length);
      npc.npcType = NPC_VEHICLE_TYPES[typeIndex];
      npc.npcColorIndex = Math.floor(Math.random() * NPC_VEHICLE_COLORS.length);
      
      // Set color from palette for minimap display
      var colorPalette = NPC_VEHICLE_COLORS[npc.npcColorIndex];
      npc.color = colorPalette.body;
      
      // Distribute NPCs evenly around the track with some randomness
      // Base position is evenly spaced around the whole track
      var baseZ = spacing * i;
      var jitter = spacing * 0.2 * (Math.random() - 0.5);  // +/- 10% of spacing
      npc.trackZ = (baseZ + jitter + roadLength) % roadLength;  // Wrap around
      npc.z = npc.trackZ;
      
      // Random lateral position (stay on road, alternate left/right bias)
      var laneOffset = (i % 2 === 0) ? -0.3 : 0.3;  // Alternate lanes
      npc.playerX = laneOffset + (Math.random() - 0.5) * 0.4;  // Stay in lane with some variance
      
      this.state.vehicles.push(npc);
    }
    
    debugLog.info("Spawned " + count + " NPC commuters");
  }

  /**
   * Check if game is running.
   */
  isRunning(): boolean {
    return this.running;
  }

  /**
   * Get final race results for cup tracking.
   * Returns null if race not finished.
   */
  getFinalRaceResults(): { positions: { id: number; position: number }[]; playerTime: number; playerBestLap: number } | null {
    if (!this.state || !this.state.finished) return null;
    
    // Build positions array for all vehicles
    var positions: { id: number; position: number }[] = [];
    
    // Player is always ID 1
    positions.push({
      id: 1,
      position: this.state.playerVehicle.racePosition
    });
    
    // AI racers (IDs 2-8)
    var aiId = 2;
    for (var i = 0; i < this.state.vehicles.length; i++) {
      var v = this.state.vehicles[i];
      if (v !== this.state.playerVehicle && v.isRacer) {
        positions.push({
          id: aiId++,
          position: v.racePosition
        });
      }
    }
    
    return {
      positions: positions,
      playerTime: this.state.time,
      playerBestLap: this.state.bestLapTime > 0 ? this.state.bestLapTime : this.state.time / this.state.track.laps
    };
  }

  /**
   * Shutdown the game.
   */
  shutdown(): void {
    logInfo("Game.shutdown()");
    this.renderer.shutdown();
    this.controls.clearAll();
  }
}
