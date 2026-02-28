/**
 * FrameRenderer - Main renderer using Frame.js layered architecture.
 * 
 * Replaces the old cell-by-cell SceneComposer approach with proper
 * frame-based rendering for efficiency and correct z-ordering.
 * 
 * Implements IRenderer interface for drop-in replacement of old Renderer.
 * 
 * Supports themes for different visual aesthetics.
 */

class FrameRenderer implements IRenderer {
  private frameManager: FrameManager;
  private width: number;
  private height: number;
  private horizonY: number;
  
  // Active theme
  private activeTheme: Theme;
  
  // Sprite cache (built from theme's roadside config)
  private spriteCache: { [name: string]: SpriteDefinition };
  
  // Parallax state (placeholders for future scrolling)
  private _mountainScrollOffset: number;
  
  // Sky grid animation phase (only updates when moving)
  private _skyGridAnimPhase: number;
  
  // Fire/lava animation phase (always animates for ambient effect)
  private _fireAnimPhase: number;
  
  // Flag to track if static elements need re-rendering
  private _staticElementsDirty: boolean;
  
  // SceneComposer for dedicated screens (results, etc.)
  private composer: SceneComposer;
  
  // Road data cached during renderRoad() for use in renderEntities()
  private _currentRoad: Road | null;
  private _currentTrackPosition: number;
  private _currentCameraX: number;
  
  // Lightning bolt visual effects
  private _lightningBolts: { x: number; startTime: number; targetY: number }[];
  
  // ANSI tunnel renderer for ansi_tunnel theme
  private _ansiTunnelRenderer: ANSITunnelRenderer | null;
  
  // Debug flags for one-time logging
  private _ansiDebugLogged: boolean;
  private _ansiFrameLogged: boolean;
  
  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.horizonY = 8;
    
    this._mountainScrollOffset = 0;
    this._staticElementsDirty = true;
    this._skyGridAnimPhase = 0;
    this._fireAnimPhase = 0;
    
    // Initialize road cache
    this._currentRoad = null;
    this._currentTrackPosition = 0;
    this._currentCameraX = 0;
    
    // Initialize lightning bolt effects
    this._lightningBolts = [];
    
    // Initialize ANSI tunnel renderer (lazy - created when theme is set)
    this._ansiTunnelRenderer = null;
    this._ansiDebugLogged = false;
    this._ansiFrameLogged = false;
    
    this.frameManager = new FrameManager(width, height, this.horizonY);
    this.composer = new SceneComposer(width, height);
    
    // Default to synthwave theme
    this.activeTheme = SynthwaveTheme;
    this.spriteCache = {};
  }
  
  /**
   * Set the active theme. Call before init() or use setTheme() after.
   */
  setTheme(themeName: string): void {
    var theme = getTheme(themeName);
    if (theme) {
      this.activeTheme = theme;
      this._staticElementsDirty = true;
      this.rebuildSpriteCache();
      
      // Initialize ANSI tunnel renderer if this is the ansi_tunnel theme
      if (themeName === 'ansi_tunnel') {
        this._ansiTunnelRenderer = new ANSITunnelRenderer();
        logInfo('ANSI Tunnel renderer initialized');
      } else {
        this._ansiTunnelRenderer = null;
      }
      
      // Re-render static elements if already initialized
      if (this.frameManager.getSunFrame()) {
        this.clearStaticFrames();
        this.renderStaticElements();
      }
      
      logInfo('Theme changed to: ' + themeName);
    } else {
      logWarning('Theme not found: ' + themeName);
    }
  }
  
  /**
   * Get available theme names.
   */
  getAvailableThemes(): string[] {
    return getThemeNames();
  }
  
  /**
   * Initialize the renderer. Must be called once at startup.
   */
  init(): void {
    // Load frame.js
    load('frame.js');
    
    // Initialize frame manager
    this.frameManager.init();
    
    // Build sprite cache from theme
    this.rebuildSpriteCache();
    
    // Render static elements based on theme
    this.renderStaticElements();
    
    logInfo('FrameRenderer initialized with theme: ' + this.activeTheme.name);
  }
  
  /**
   * Rebuild sprite cache from current theme's roadside pool.
   */
  private rebuildSpriteCache(): void {
    this.spriteCache = {};
    var pool = this.activeTheme.roadside.pool;
    
    for (var i = 0; i < pool.length; i++) {
      var entry = pool[i];
      var creator = ROADSIDE_SPRITES[entry.sprite];
      if (creator) {
        this.spriteCache[entry.sprite] = creator();
      }
    }
  }
  
  /**
   * Select a sprite from the weighted pool based on world position.
   * Returns { sprite: string, side: 'left' | 'right' | 'both' }
   */
  private selectFromPool(worldZ: number): { sprite: string; side: string } {
    var pool = this.activeTheme.roadside.pool;
    
    // Calculate total weight
    var totalWeight = 0;
    for (var i = 0; i < pool.length; i++) {
      totalWeight += pool[i].weight;
    }
    
    // Deterministic selection based on worldZ
    var hash = (Math.floor(worldZ) * 7919) % totalWeight;  // Prime for better distribution
    
    var cumulative = 0;
    for (var j = 0; j < pool.length; j++) {
      cumulative += pool[j].weight;
      if (hash < cumulative) {
        return { sprite: pool[j].sprite, side: pool[j].side || 'both' };
      }
    }
    
    // Fallback
    return { sprite: pool[0].sprite, side: pool[0].side || 'both' };
  }
  
  /**
   * Clear static frame contents for theme change.
   */
  private clearStaticFrames(): void {
    var sunFrame = this.frameManager.getSunFrame();
    var mtnsFrame = this.frameManager.getMountainsFrame();
    
    if (sunFrame) sunFrame.clear();
    if (mtnsFrame) mtnsFrame.clear();
  }
  
  /**
   * Render all static elements based on current theme.
   */
  private renderStaticElements(): void {
    // Celestial body
    if (this.activeTheme.celestial.type === 'sun') {
      this.renderSun();
    } else if (this.activeTheme.celestial.type === 'moon') {
      this.renderMoon();
    } else if (this.activeTheme.celestial.type === 'dual_moons') {
      this.renderDualMoons();
    } else if (this.activeTheme.celestial.type === 'monster') {
      this.renderMonsterSilhouette();
    } else if (this.activeTheme.celestial.type === 'mermaid') {
      this.renderMermaid();
    }
    
    // Background
    if (this.activeTheme.background.type === 'mountains') {
      this.renderMountains();
    } else if (this.activeTheme.background.type === 'skyscrapers') {
      this.renderSkyscrapers();
    } else if (this.activeTheme.background.type === 'ocean') {
      this.renderOceanIslands();
    } else if (this.activeTheme.background.type === 'forest') {
      this.renderForestTreeline();
    } else if (this.activeTheme.background.type === 'jungle_canopy') {
      this.renderJungleCanopy();
    } else if (this.activeTheme.background.type === 'candy_hills') {
      this.renderCandyHills();
    } else if (this.activeTheme.background.type === 'nebula') {
      this.renderNebula();
    } else if (this.activeTheme.background.type === 'castle_fortress') {
      this.renderCastleFortress();
    } else if (this.activeTheme.background.type === 'volcanic') {
      this.renderVolcanic();
    } else if (this.activeTheme.background.type === 'pyramids') {
      this.renderPyramids();
    } else if (this.activeTheme.background.type === 'dunes') {
      this.renderDunes();
    } else if (this.activeTheme.background.type === 'stadium') {
      this.renderStadium();
    } else if (this.activeTheme.background.type === 'destroyed_city') {
      this.renderDestroyedCity();
    } else if (this.activeTheme.background.type === 'underwater') {
      this.renderUnderwaterBackground();
    } else if (this.activeTheme.background.type === 'aquarium') {
      this.renderAquariumBackground();
    } else if (this.activeTheme.background.type === 'ansi') {
      // ANSI tunnel - static elements are minimal, dynamic rendering happens in renderRoad
      this.renderANSITunnelStatic();
    }
    
    this._staticElementsDirty = false;
    logDebug('Static elements rendered, dirty=' + this._staticElementsDirty);
  }
  
  // ============================================================
  // IRenderer interface implementation
  // ============================================================
  
  /**
   * Begin a new frame - no-op for Frame.js (we update in place).
   */
  beginFrame(): void {
    // Frame.js doesn't need explicit begin - we update frames in place
  }
  
  /**
   * Render sky (IRenderer interface).
   */
  renderSky(trackPosition: number, curvature?: number, playerSteer?: number, speed?: number, dt?: number): void {
    // Update glitch state if on glitch theme
    if (this.activeTheme.name === 'glitch_circuit' && typeof GlitchState !== 'undefined') {
      GlitchState.update(trackPosition, dt || 0.016);
    }
    
    // Always advance fire animation phase (for lava/fire effects even when stopped)
    this._fireAnimPhase += (dt || 0.016) * 8;  // Faster animation for more dramatic flames
    
    // Update sky background based on theme type (grid vs stars vs gradient vs water)
    if (this.activeTheme.sky.type === 'grid') {
      this.renderSkyGrid(speed || 0, dt || 0);
    } else if (this.activeTheme.sky.type === 'stars') {
      this.renderSkyStars(trackPosition);
    } else if (this.activeTheme.sky.type === 'gradient') {
      this.renderSkyGradient(trackPosition);
    } else if (this.activeTheme.sky.type === 'water') {
      this.renderSkyWater(trackPosition, speed || 0, dt || 0);
    } else if (this.activeTheme.sky.type === 'ansi') {
      // ANSI tunnel sky is rendered dynamically with the road
      // Just update the scroll position here
      if (this._ansiTunnelRenderer && this._currentRoad) {
        this._ansiTunnelRenderer.updateScroll(trackPosition, this._currentRoad.totalLength);
      }
    }
    // 'plain' type = no sky animation
    
    // Animate ocean waves if ocean background
    if (this.activeTheme.background.type === 'ocean') {
      this.renderOceanWaves(trackPosition);
    }
    
    // Update parallax (if params provided AND car is moving)
    if (curvature !== undefined && playerSteer !== undefined && speed !== undefined && dt !== undefined && speed > 0) {
      this.updateParallax(curvature, playerSteer, speed, dt);
    }
  }
  
  /**
   * Render road (IRenderer interface).
   */
  renderRoad(trackPosition: number, cameraX: number, _track: ITrack, road: Road): void {
    // Cache road data for use in renderEntities() (for NPC curve offset calculation)
    this._currentRoad = road;
    this._currentTrackPosition = trackPosition;
    this._currentCameraX = cameraX;
    
    // Special handling for ANSI tunnel theme - render entire tunnel effect
    var bgType = this.activeTheme.background ? this.activeTheme.background.type : 'none';
    
    // Debug: log once when game starts
    if (!this._ansiDebugLogged) {
      this._ansiDebugLogged = true;
      logInfo('renderRoad check: bgType=' + bgType + ' hasRenderer=' + (this._ansiTunnelRenderer ? 'yes' : 'no') + ' theme=' + this.activeTheme.name);
    }
    
    if (this._ansiTunnelRenderer && bgType === 'ansi') {
      this._ansiTunnelRenderer.updateScroll(trackPosition, road.totalLength);
      
      // Get both sky and road frames
      var skyFrame = this.frameManager.getSkyGridFrame();
      var roadFrame = this.frameManager.getRoadFrame();
      
      // Road frame height calculation:
      // Total road is height - horizonY (16 rows for 24-8 layout)
      // Combined with sky (8 rows) this gives full 24 rows of ANSI
      var roadFrameHeight = this.height - this.horizonY;  // Full road frame height
      var ansiRoadHeight = roadFrameHeight;  // Use full road height - HUD overlays on top
      
      // Debug: log frame info once
      if (!this._ansiFrameLogged) {
        this._ansiFrameLogged = true;
        logInfo('ANSI frames: sky=' + (skyFrame ? 'ok' : 'null') + ' road=' + (roadFrame ? 'ok' : 'null') + ' horizonY=' + this.horizonY + ' roadFrameHeight=' + roadFrameHeight + ' ansiRoadHeight=' + ansiRoadHeight);
      }
      
      // Clear road frame first to avoid artifacts
      if (roadFrame) {
        roadFrame.clear();
      }
      
      // Render ANSI tunnel to both frames
      this._ansiTunnelRenderer.renderTunnel(
        skyFrame,
        roadFrame,
        this.horizonY,
        ansiRoadHeight,
        this.width,
        trackPosition,
        cameraX,
        road,
        road.totalLength
      );
      
      // Skip renderRoadSurface for ANSI theme - it would clear our ANSI content!
      // Just render road stripes on top of ANSI if needed
      this.renderANSIRoadStripes(trackPosition, cameraX, road, ansiRoadHeight);
      
      // Build roadside objects from track/road
      var roadsideObjects = this.buildRoadsideObjects(trackPosition, cameraX, road);
      this.renderRoadsideSprites(roadsideObjects);
      return;
    }
    
    // Render ground pattern based on theme type
    if (this.activeTheme.ground) {
      if (this.activeTheme.ground.type === 'grid') {
        this.renderHolodeckFloor(trackPosition);
      } else if (this.activeTheme.ground.type === 'lava') {
        this.renderLavaGround(trackPosition);
      } else if (this.activeTheme.ground.type === 'candy') {
        this.renderCandyGround(trackPosition);
      } else if (this.activeTheme.ground.type === 'void') {
        this.renderVoidGround(trackPosition);
      } else if (this.activeTheme.ground.type === 'cobblestone') {
        this.renderCobblestoneGround(trackPosition);
      } else if (this.activeTheme.ground.type === 'jungle') {
        this.renderJungleGround(trackPosition);
      } else if (this.activeTheme.ground.type === 'dirt') {
        this.renderDirtGround(trackPosition);
      } else if (this.activeTheme.ground.type === 'water') {
        this.renderWaterGround(trackPosition);
      }
    }
    
    // Render road surface with curves
    this.renderRoadSurface(trackPosition, cameraX, road);
    
    // Build roadside objects from track/road
    var roadsideObjects = this.buildRoadsideObjects(trackPosition, cameraX, road);
    this.renderRoadsideSprites(roadsideObjects);
  }
  
  /**
   * Build roadside object list from road data.
   * 
   * ARCHITECTURE:
   * - Roadside objects exist at fixed WORLD positions (worldZ, side)
   * - We project world positions to SCREEN positions each frame
   * - Screen X is calculated based on road edge position at that depth
   * - This ensures objects stay alongside the road through curves
   */
  private buildRoadsideObjects(trackPosition: number, cameraX: number, road: Road): { x: number; y: number; distance: number; type: string }[] {
    var objects: { x: number; y: number; distance: number; type: string }[] = [];
    var roadHeight = this.height - this.horizonY;
    
    // Iterate through world Z positions that are visible
    // View distance in world units (matching road rendering)
    var viewDistanceWorld = 100;  // How far ahead in world units
    var startZ = trackPosition;
    var endZ = trackPosition + viewDistanceWorld;
    
    // Objects are placed at fixed intervals in the world
    var spacing = this.activeTheme.roadside.spacing;
    
    // Align to grid so objects stay at fixed world positions
    var firstObjectZ = Math.ceil(startZ / spacing) * spacing;
    
    // Sample at fixed world positions
    for (var worldZ = firstObjectZ; worldZ < endZ; worldZ += spacing) {
      // Select from weighted pool based on world position
      var selection = this.selectFromPool(worldZ);
      var spriteType = selection.sprite;
      var allowedSide = selection.side;
      var worldZInt = Math.floor(worldZ);
      
      // PROJECT: World Z -> Screen Y and distance
      // This is the inverse of the road rendering projection
      var relativeZ = worldZ - trackPosition;
      if (relativeZ <= 0) continue;
      
      // Distance factor (same formula as road uses)
      var distance = relativeZ / 5;  // Convert world units to distance units
      if (distance < 1 || distance > 20) continue;
      
      // Screen Y from distance (inverse of road formula)
      // Road uses: distance = 1 / (1 - t * 0.95) where t = (roadBottom - screenY) / roadBottom
      // Solving for screenY: t = 1 - 1/distance, screenY = roadBottom * (1 - t) = roadBottom / distance * 0.95
      var t = 1 - (1 / distance);
      var screenY = Math.round(this.horizonY + roadHeight * (1 - t));
      
      if (screenY <= this.horizonY || screenY >= this.height) continue;
      
      // Calculate road center at this screen Y (same as road rendering)
      // Accumulate curve from player to this point
      var accumulatedCurve = 0;
      for (var z = trackPosition; z < worldZ; z += 5) {
        var seg = road.getSegment(z);
        if (seg) accumulatedCurve += seg.curve * 0.5;
      }
      var curveOffset = accumulatedCurve * distance * 0.8;
      var centerX = 40 + Math.round(curveOffset) - Math.round(cameraX * 0.5);
      
      // Road width at this distance
      var roadHalfWidth = Math.round(20 / distance);
      
      // Left and right edges of road
      var leftEdge = centerX - roadHalfWidth;
      var rightEdge = centerX + roadHalfWidth;
      
      // Position objects OUTSIDE road edges
      // Offset from edge scales inversely with distance (larger offset when close)
      var edgeOffset = Math.round(15 / distance) + 3;  // Offset from road edge
      
      var leftX = leftEdge - edgeOffset;
      var rightX = rightEdge + edgeOffset;
      
      // Determine which side to place based on world position and allowed sides
      var preferredSide = (Math.floor(worldZ / spacing) % 2 === 0) ? 'left' : 'right';
      
      // Respect the side restriction from pool entry
      if (allowedSide === 'left' || (allowedSide === 'both' && preferredSide === 'left')) {
        if (leftX >= 0) {
          objects.push({ x: leftX, y: screenY, distance: distance, type: spriteType });
        }
      }
      if (allowedSide === 'right' || (allowedSide === 'both' && preferredSide === 'right')) {
        if (rightX < 80) {
          objects.push({ x: rightX, y: screenY, distance: distance, type: spriteType });
        }
      }
      
      // For denser themes, also place on opposite side sometimes (if allowed)
      if (this.activeTheme.roadside.density > 1.0 && (worldZInt % 2 === 0)) {
        if (allowedSide === 'both' || allowedSide === 'right') {
          if (preferredSide === 'left' && rightX < 80) {
            objects.push({ x: rightX, y: screenY, distance: distance, type: spriteType });
          }
        }
        if (allowedSide === 'both' || allowedSide === 'left') {
          if (preferredSide === 'right' && leftX >= 0) {
            objects.push({ x: leftX, y: screenY, distance: distance, type: spriteType });
          }
        }
      }
    }
    
    // Sort by distance (far to near) for proper z-ordering
    objects.sort(function(a, b) { return b.distance - a.distance; });
    return objects;
  }

  /**
   * Render entities (IRenderer interface).
   */
  renderEntities(playerVehicle: IVehicle, vehicles: IVehicle[], items: Item[], projectiles?: IProjectile[]): void {
    // Render item boxes (behind vehicles)
    this.renderItemBoxes(playerVehicle, items);
    
    // Render shell projectiles
    if (projectiles && projectiles.length > 0) {
      this.renderProjectiles(playerVehicle, projectiles);
    }
    
    // Render NPC vehicles (sorted by distance, far to near)
    this.renderNPCVehicles(playerVehicle, vehicles);
    
    // Render player vehicle with visual effects
    var v = playerVehicle as Vehicle;
    this.renderPlayerVehicle(
      playerVehicle.playerX, 
      playerVehicle.flashTimer > 0, 
      playerVehicle.boostTimer > 0,
      v.hasEffect ? v.hasEffect(ItemType.STAR) : false,
      v.hasEffect ? v.hasEffect(ItemType.BULLET) : false,
      v.hasEffect ? v.hasEffect(ItemType.LIGHTNING) : false,
      v.carId || 'sports',
      v.carColorId || 'yellow',
      this._currentBrakeLightsOn  // Use the brake light state set by Game
    );
  }
  
  /**
   * Render shell projectiles on the track.
   * Uses multi-character sprites with scaling like vehicles.
   */
  private renderProjectiles(playerVehicle: IVehicle, projectiles: IProjectile[]): void {
    var frame = this.frameManager.getRoadFrame();
    if (!frame) return;
    
    var visualHorizonY = 5;
    var roadBottom = this.height - 4;
    var roadHeight = roadBottom - visualHorizonY;
    
    // Shell sprites at different scales (from tiny to close)
    // Scale 0: dot (far away)
    // Scale 1: small circle
    // Scale 2: medium shell
    // Scale 3: large shell (close)
    var greenSprites = [
      ['.'],                    // Scale 0: dot
      ['o'],                    // Scale 1: small
      ['(O)'],                  // Scale 2: medium
      ['_/O\\_', ' \\O/']       // Scale 3: large (2 lines)
    ];
    var redSprites = [
      ['.'],
      ['o'],
      ['(O)'],
      ['_/O\\_', ' \\O/']
    ];
    var blueSprites = [
      ['*'],
      ['@'],
      ['<@>'],
      ['~/~@~\\~', ' ~\\@/~']   // Blue shell is spiky
    ];
    
    // Banana sprites (yellow peels)
    var bananaSprites = [
      ['.'],                    // Scale 0: dot
      ['o'],                    // Scale 1: small
      ['(o)'],                  // Scale 2: medium
      [' /\\\\ ', '(__)']         // Scale 3: large (banana peel shape)
    ];
    
    for (var i = 0; i < projectiles.length; i++) {
      var projectile = projectiles[i];
      if (projectile.isDestroyed) continue;
      
      // Check if it's a banana (speed = 0) or shell
      var isBanana = projectile.speed === 0;
      
      // Calculate distance from player
      var distZ = projectile.trackZ - playerVehicle.trackZ;
      
      // Render if ahead and within view, or slightly behind (just passed)
      if (distZ < -5 || distZ > 600) continue;
      
      // Use same perspective system as vehicles
      var maxViewDist = 500;
      var normalizedDist = Math.max(0.01, distZ / maxViewDist);
      var t = Math.max(0, Math.min(1, 1 - normalizedDist));
      
      // Screen Y position
      var screenY = Math.round(visualHorizonY + t * roadHeight);
      
      // Calculate curve offset for road alignment
      var curveOffset = 0;
      if (this._currentRoad && distZ > 0) {
        var projWorldZ = this._currentTrackPosition + distZ;
        var seg = this._currentRoad.getSegment(projWorldZ);
        if (seg) {
          curveOffset = seg.curve * t * 15;
        }
      }
      
      // Lateral position with perspective
      var perspectiveScale = t * t;
      var relativeX = projectile.playerX - playerVehicle.playerX;
      var screenX = Math.round(40 + curveOffset + relativeX * perspectiveScale * 25 - this._currentCameraX * 0.5);
      
      // Determine scale based on screen position (like vehicles)
      var screenProgress = (screenY - visualHorizonY) / roadHeight;
      var scaleIndex: number;
      if (screenProgress < 0.08) {
        scaleIndex = 0;  // Dot - right at horizon
      } else if (screenProgress < 0.20) {
        scaleIndex = 1;  // Small
      } else if (screenProgress < 0.40) {
        scaleIndex = 2;  // Medium
      } else {
        scaleIndex = 3;  // Large - close
      }
      
      // Select sprite set and color based on projectile type
      var sprites: string[][];
      var attr: number;
      if (isBanana) {
        sprites = bananaSprites;
        attr = makeAttr(YELLOW, BG_BLACK);
      } else {
        // Shell
        var shell = projectile as Shell;
        if (shell.shellType === ShellType.GREEN) {
          sprites = greenSprites;
          attr = makeAttr(LIGHTGREEN, BG_BLACK);
        } else if (shell.shellType === ShellType.RED) {
          sprites = redSprites;
          attr = makeAttr(LIGHTRED, BG_BLACK);
        } else {
          sprites = blueSprites;
          attr = makeAttr(LIGHTBLUE, BG_BLACK);
        }
      }
      
      // Get sprite lines for this scale
      var spriteLines = sprites[scaleIndex];
      var spriteWidth = spriteLines[0].length;
      var spriteHeight = spriteLines.length;
      
      // Center the sprite horizontally
      var startX = screenX - Math.floor(spriteWidth / 2);
      var startY = screenY - (spriteHeight - 1);  // Bottom of sprite at screenY
      
      // Render sprite lines
      for (var ly = 0; ly < spriteHeight; ly++) {
        var line = spriteLines[ly];
        var drawY = startY + ly;
        if (drawY < visualHorizonY || drawY >= roadBottom) continue;
        
        for (var lx = 0; lx < line.length; lx++) {
          var ch = line.charAt(lx);
          if (ch === ' ') continue;  // Skip spaces
          var drawX = startX + lx;
          if (drawX < 0 || drawX >= 80) continue;
          frame.setData(drawX, drawY, ch, attr);
        }
      }
    }
  }
  
  /**
   * Render item boxes on the track with super-scaling and themed colors.
   * Uses CP437 double-lined box characters for a proper "mystery box" look.
   */
  private renderItemBoxes(playerVehicle: IVehicle, items: Item[]): void {
    var frame = this.frameManager.getRoadFrame();
    if (!frame) return;
    
    // Get item box colors from theme or use defaults
    var colors = this.activeTheme.colors;
    var boxColors = colors.itemBox || {
      border: { fg: YELLOW, bg: BG_BLACK },
      fill: { fg: BROWN, bg: BG_BLACK },
      symbol: { fg: WHITE, bg: BG_BLACK }
    };
    
    var borderAttr = makeAttr(boxColors.border.fg, boxColors.border.bg);
    var fillAttr = makeAttr(boxColors.fill.fg, boxColors.fill.bg);
    var symbolAttr = makeAttr(boxColors.symbol.fg, boxColors.symbol.bg);
    
    var visualHorizonY = 5;
    var roadBottom = this.height - 4;
    
    // Collect items to render with their screen positions for sorting
    var itemsToRender: { item: Item; screenY: number; screenX: number; t: number }[] = [];
    
    // FIRST: Check for any player pickup effects (these render at screen-space, not world position)
    // Must do this before the distance filter which would skip items at/behind the player
    for (var p = 0; p < items.length; p++) {
      var pItem = items[p];
      if (pItem.isBeingDestroyed() && pItem.pickedUpByPlayer) {
        this.renderPlayerPickupEffect(frame, pItem.destructionTimer, pItem.destructionStartTime, symbolAttr);
      }
    }
    
    for (var i = 0; i < items.length; i++) {
      var item = items[i];
      
      // Skip items that are fully respawning (not available and not animating)
      if (!item.isAvailable() && !item.isBeingDestroyed()) continue;
      
      // Skip player pickups - already rendered above as screen-space effect
      if (item.isBeingDestroyed() && item.pickedUpByPlayer) continue;
      
      // Calculate relative position to player
      var relativeZ = item.z - playerVehicle.trackZ;
      var relativeX = item.x - (playerVehicle.playerX * 20);
      
      // Only render items ahead of player and within view distance
      if (relativeZ < 5 || relativeZ > 300) continue;
      
      // Perspective calculation
      var maxViewDist = 300;
      var normalizedDist = Math.max(0.01, relativeZ / maxViewDist);
      var t = Math.max(0, Math.min(1, 1 - normalizedDist));
      
      // Screen Y position
      var screenY = Math.round(visualHorizonY + t * (roadBottom - visualHorizonY));
      
      // Calculate curve offset for proper road alignment
      var curveOffset = 0;
      if (this._currentRoad && relativeZ > 0) {
        var itemWorldZ = this._currentTrackPosition + relativeZ;
        var seg = this._currentRoad.getSegment(itemWorldZ);
        if (seg) {
          curveOffset = seg.curve * t * 15;
        }
      }
      
      // Screen X position with curve offset
      // Item x ranges from -20 to +20 (road width)
      // At close range (t=1), we want full spread across ~60 char road width
      // So multiply by ~1.5 to convert item x to screen offset
      var xScale = t * 1.5;  // Scale with perspective
      var screenX = Math.round(40 + curveOffset + relativeX * xScale - this._currentCameraX * 0.5);
      
      // Bounds check (rough - we'll do precise checks when rendering)
      if (screenY < visualHorizonY - 2 || screenY >= this.height) continue;
      if (screenX < -3 || screenX >= this.width + 3) continue;
      
      itemsToRender.push({ item: item, screenY: screenY, screenX: screenX, t: t });
    }
    
    // Sort by Y (far to near) so closer boxes render on top
    itemsToRender.sort(function(a, b) { return a.screenY - b.screenY; });
    
    // Render each item box
    for (var j = 0; j < itemsToRender.length; j++) {
      var data = itemsToRender[j];
      var item = data.item;
      var screenX = data.screenX;
      var screenY = data.screenY;
      var t = data.t;
      
      // Animation pulse
      var pulse = Math.floor(Date.now() / 150) % 4;
      
      // Handle destruction animation (NPC pickups only - player pickups rendered above)
      if (item.isBeingDestroyed()) {
        this.renderItemBoxDestruction(frame, screenX, screenY, t, item.destructionTimer, item.destructionStartTime);
        continue;
      }
      
      // Determine scale based on distance (t)
      // t > 0.5: Large (3x3 box)
      // t > 0.25: Medium (3x2 box)  
      // t > 0.1: Small (1x1)
      // t <= 0.1: Tiny (single char)
      
      if (t > 0.5) {
        // Large scale: 3x3 double-lined box with "?" inside
        this.renderItemBoxLarge(frame, screenX, screenY, borderAttr, fillAttr, symbolAttr, pulse);
      } else if (t > 0.25) {
        // Medium scale: 3x2 box
        this.renderItemBoxMedium(frame, screenX, screenY, borderAttr, symbolAttr, pulse);
      } else if (t > 0.1) {
        // Small scale: simplified 1x1 with color variation
        this.renderItemBoxSmall(frame, screenX, screenY, borderAttr, symbolAttr, pulse);
      } else {
        // Tiny: just a dot
        if (screenX >= 0 && screenX < this.width && screenY >= 0 && screenY < this.height) {
          var tinyAttr = (pulse % 2 === 0) ? borderAttr : symbolAttr;
          frame.setData(screenX, screenY, '.', tinyAttr);
        }
      }
    }
  }
  
  /**
   * Render a large 3x3 item box with double-lined border and "?" inside.
   * Uses CP437 box drawing characters for that retro feel.
   */
  private renderItemBoxLarge(frame: Frame, cx: number, cy: number, 
                              borderAttr: number, _fillAttr: number, symbolAttr: number, pulse: number): void {
    // CP437 double-line box characters
    var TL = GLYPH.DBOX_TL;  // ╔
    var TR = GLYPH.DBOX_TR;  // ╗
    var BL = GLYPH.DBOX_BL;  // ╚
    var BR = GLYPH.DBOX_BR;  // ╝
    var H = GLYPH.DBOX_H;    // ═
    var V = GLYPH.DBOX_V;    // ║
    
    // Pulsing effect - alternate border brightness
    var activeBorderAttr = (pulse === 0 || pulse === 2) ? borderAttr : 
                           makeAttr(WHITE, borderAttr & 0xF0);
    
    // 3x3 layout centered at (cx, cy):
    //   ╔═╗   (cy-1)
    //   ║?║   (cy)
    //   ╚═╝   (cy+1)
    
    var left = cx - 1;
    var right = cx + 1;
    var top = cy - 1;
    var bottom = cy + 1;
    
    // Top row
    if (top >= 0 && top < this.height) {
      if (left >= 0 && left < this.width) frame.setData(left, top, TL, activeBorderAttr);
      if (cx >= 0 && cx < this.width) frame.setData(cx, top, H, activeBorderAttr);
      if (right >= 0 && right < this.width) frame.setData(right, top, TR, activeBorderAttr);
    }
    
    // Middle row with "?"
    if (cy >= 0 && cy < this.height) {
      if (left >= 0 && left < this.width) frame.setData(left, cy, V, activeBorderAttr);
      if (cx >= 0 && cx < this.width) {
        // Pulsing "?" color
        var qAttr = (pulse % 2 === 0) ? symbolAttr : makeAttr(YELLOW, symbolAttr & 0xF0);
        frame.setData(cx, cy, '?', qAttr);
      }
      if (right >= 0 && right < this.width) frame.setData(right, cy, V, activeBorderAttr);
    }
    
    // Bottom row
    if (bottom >= 0 && bottom < this.height) {
      if (left >= 0 && left < this.width) frame.setData(left, bottom, BL, activeBorderAttr);
      if (cx >= 0 && cx < this.width) frame.setData(cx, bottom, H, activeBorderAttr);
      if (right >= 0 && right < this.width) frame.setData(right, bottom, BR, activeBorderAttr);
    }
  }
  
  /**
   * Render a medium 3x2 item box (no top/bottom lines, just sides).
   */
  private renderItemBoxMedium(frame: Frame, cx: number, cy: number,
                               borderAttr: number, symbolAttr: number, pulse: number): void {
    // Simpler box for medium distance
    //   [?]   (cy)
    //   └─┘   (cy+1)
    
    var left = cx - 1;
    var right = cx + 1;
    
    var activeBorderAttr = (pulse % 2 === 0) ? borderAttr : makeAttr(WHITE, borderAttr & 0xF0);
    
    // Main row with "?"
    if (cy >= 0 && cy < this.height) {
      if (left >= 0 && left < this.width) frame.setData(left, cy, '[', activeBorderAttr);
      if (cx >= 0 && cx < this.width) {
        var qAttr = (pulse % 2 === 0) ? symbolAttr : makeAttr(YELLOW, symbolAttr & 0xF0);
        frame.setData(cx, cy, '?', qAttr);
      }
      if (right >= 0 && right < this.width) frame.setData(right, cy, ']', activeBorderAttr);
    }
    
    // Bottom hint (shadow/base)
    var bottom = cy + 1;
    if (bottom >= 0 && bottom < this.height) {
      if (cx >= 0 && cx < this.width) frame.setData(cx, bottom, GLYPH.LOWER_HALF, makeAttr(DARKGRAY, BG_BLACK));
    }
  }
  
  /**
   * Render a small item box (single character with styling).
   */
  private renderItemBoxSmall(frame: Frame, cx: number, cy: number,
                              borderAttr: number, symbolAttr: number, pulse: number): void {
    if (cx < 0 || cx >= this.width || cy < 0 || cy >= this.height) return;
    
    // Alternate between "?" and a box character
    var char = (pulse % 2 === 0) ? '?' : GLYPH.FULL_BLOCK;
    var attr = (pulse % 2 === 0) ? symbolAttr : borderAttr;
    
    frame.setData(cx, cy, char, attr);
  }
  
  /**
   * Render item box destruction effect - expanding burst of particles.
   */
  private renderItemBoxDestruction(frame: Frame, cx: number, cy: number, t: number,
                                    destructionTimer: number, startTime: number): void {
    // Calculate animation progress (0 = just started, 1 = finished)
    var totalDuration = 0.4;
    var progress = 1 - (destructionTimer / totalDuration);
    progress = Math.max(0, Math.min(1, progress));
    
    // Explosion colors - cycle through bright colors
    var colors = [YELLOW, WHITE, LIGHTCYAN, LIGHTMAGENTA, LIGHTRED];
    var colorIndex = Math.floor((Date.now() - startTime) / 50) % colors.length;
    var explosionAttr = makeAttr(colors[colorIndex], BG_BLACK);
    var sparkAttr = makeAttr(YELLOW, BG_BLACK);
    
    // Scale determines particle spread
    var scale = t > 0.5 ? 3 : (t > 0.25 ? 2 : 1);
    
    // Particle positions expand outward over time
    var spread = Math.floor(progress * scale * 2) + 1;
    
    // Explosion characters
    var chars = ['*', '+', GLYPH.LIGHT_SHADE, '.', GLYPH.MEDIUM_SHADE];
    
    // Center burst
    if (progress < 0.5) {
      if (cx >= 0 && cx < this.width && cy >= 0 && cy < this.height) {
        frame.setData(cx, cy, chars[0], explosionAttr);
      }
    }
    
    // Expanding particles
    var particleCount = Math.floor(progress * 8);
    for (var i = 0; i < particleCount; i++) {
      // Calculate particle position based on angle
      var angle = (i / 8) * Math.PI * 2;
      var dist = spread * (0.5 + progress * 0.5);
      var px = Math.round(cx + Math.cos(angle) * dist);
      var py = Math.round(cy + Math.sin(angle) * dist * 0.5);  // Compress vertically
      
      if (px >= 0 && px < this.width && py >= 0 && py < this.height) {
        var charIdx = (i + Math.floor(progress * 3)) % chars.length;
        var attr = (i % 2 === 0) ? explosionAttr : sparkAttr;
        frame.setData(px, py, chars[charIdx], attr);
      }
    }
    
    // Fading center sparkle
    if (progress > 0.3 && progress < 0.8) {
      var sparkles = [[0, 0], [-1, 0], [1, 0], [0, -1], [0, 1]];
      for (var s = 0; s < sparkles.length; s++) {
        var sx = cx + sparkles[s][0];
        var sy = cy + sparkles[s][1];
        if (sx >= 0 && sx < this.width && sy >= 0 && sy < this.height) {
          var sparkChar = (Date.now() % 100 < 50) ? '*' : '+';
          frame.setData(sx, sy, sparkChar, sparkAttr);
        }
      }
    }
  }
  
  /**
   * Render a screen-space pickup effect when the PLAYER picks up an item box.
   * This shows a burst around the car/bottom of screen so the player sees it.
   */
  private renderPlayerPickupEffect(frame: Frame, destructionTimer: number, startTime: number, themeAttr: number): void {
    // Calculate animation progress (0 = just started, 1 = finished)
    var totalDuration = 0.5;
    var progress = 1 - (destructionTimer / totalDuration);
    progress = Math.max(0, Math.min(1, progress));
    
    // Center of effect at bottom-center of screen (where player car is)
    var cx = Math.floor(this.width / 2);
    var cy = this.height - 5;  // Just above car position
    
    // Explosion colors - bright and attention-grabbing
    var colors = [WHITE, YELLOW, LIGHTCYAN, LIGHTMAGENTA, YELLOW];
    var colorIndex = Math.floor((Date.now() - startTime) / 40) % colors.length;
    var burstAttr = makeAttr(colors[colorIndex], BG_BLACK);
    var sparkAttr = makeAttr(YELLOW, BG_BLACK);
    
    // Use theme color for some particles
    var flashChars = ['*', '+', GLYPH.LIGHT_SHADE, '!', '?', GLYPH.MEDIUM_SHADE, GLYPH.FULL_BLOCK];
    
    // Phase 1: Initial bright flash at center (0 - 0.2)
    if (progress < 0.3) {
      // Bright center burst
      var flashSize = Math.floor(3 + progress * 6);
      for (var fy = -1; fy <= 1; fy++) {
        for (var fx = -flashSize; fx <= flashSize; fx++) {
          var px = cx + fx;
          var py = cy + fy;
          if (px >= 0 && px < this.width && py >= 0 && py < this.height) {
            var dist = Math.abs(fx) + Math.abs(fy);
            var charIdx = dist % flashChars.length;
            var attr = (dist % 2 === 0) ? burstAttr : themeAttr;
            frame.setData(px, py, flashChars[charIdx], attr);
          }
        }
      }
    }
    
    // Phase 2: Expanding ring of particles (0.1 - 0.6)
    if (progress > 0.1 && progress < 0.7) {
      var ringProgress = (progress - 0.1) / 0.5;
      var ringRadius = 2 + ringProgress * 10;
      var numParticles = 12;
      
      for (var i = 0; i < numParticles; i++) {
        var angle = (i / numParticles) * Math.PI * 2;
        var px = Math.round(cx + Math.cos(angle) * ringRadius);
        var py = Math.round(cy + Math.sin(angle) * ringRadius * 0.4);  // Compress vertically
        
        if (px >= 0 && px < this.width && py >= 0 && py < this.height) {
          var charIdx = (i + Math.floor(progress * 5)) % flashChars.length;
          var attr = (i % 3 === 0) ? burstAttr : (i % 3 === 1) ? sparkAttr : themeAttr;
          frame.setData(px, py, flashChars[charIdx], attr);
        }
      }
    }
    
    // Phase 3: Rising sparkles (0.2 - 1.0)
    if (progress > 0.2) {
      var riseProgress = (progress - 0.2) / 0.8;
      var numSparkles = 8;
      
      for (var s = 0; s < numSparkles; s++) {
        // Each sparkle rises from center
        var sx = cx + (s - numSparkles / 2) * 2;
        var sy = cy - Math.floor(riseProgress * (4 + (s % 3) * 2));
        
        if (sx >= 0 && sx < this.width && sy >= 0 && sy < this.height) {
          var sparkleChar = (Date.now() % 80 < 40) ? '*' : '+';
          var attr = (s % 2 === 0) ? burstAttr : sparkAttr;
          frame.setData(sx, sy, sparkleChar, attr);
        }
      }
    }
    
    // "Got Item!" indicator text at very start
    if (progress < 0.4) {
      var textY = cy - 3;
      if (progress < 0.15) {
        // Brief "!" flash
        if (cx >= 0 && cx < this.width && textY >= 0 && textY < this.height) {
          frame.setData(cx, textY, '!', burstAttr);
        }
      }
    }
  }
  
  /**
   * Render NPC vehicles relative to player.
   */
  private renderNPCVehicles(playerVehicle: IVehicle, vehicles: IVehicle[]): void {
    // Build list of visible NPCs with distance info
    var visibleNPCs: { vehicle: IVehicle; relativeZ: number; relativeX: number }[] = [];
    
    // Get road length for wrap-around calculations
    var roadLength = this._currentRoad ? this._currentRoad.totalLength : 10000;
    
    for (var i = 0; i < vehicles.length; i++) {
      var v = vehicles[i];
      if (!v.isNPC) continue;  // Skip player
      
      // Calculate relative position with wrap-around handling
      // This computes the signed distance on a circular track
      var rawDiff = v.trackZ - playerVehicle.trackZ;
      var relativeZ = rawDiff;
      
      // Handle wrap-around: if the raw difference is more than half the track,
      // the shorter path is the other way around
      if (rawDiff > roadLength / 2) {
        relativeZ = rawDiff - roadLength;  // NPC is actually behind (wrapped)
      } else if (rawDiff < -roadLength / 2) {
        relativeZ = rawDiff + roadLength;  // NPC is actually ahead (wrapped)
      }
      
      var relativeX = v.playerX - playerVehicle.playerX;
      
      // Render if within view distance (ahead or slightly behind)
      if (relativeZ > -10 && relativeZ < 600) {
        visibleNPCs.push({ vehicle: v, relativeZ: relativeZ, relativeX: relativeX });
      }
    }
    
    // Sort by distance (far to near, so closer vehicles render on top)
    visibleNPCs.sort(function(a, b) { return b.relativeZ - a.relativeZ; });
    
    // Render each NPC
    for (var j = 0; j < visibleNPCs.length; j++) {
      this.renderNPCVehicle(visibleNPCs[j].vehicle, visibleNPCs[j].relativeZ, visibleNPCs[j].relativeX);
    }
  }
  
  /**
   * Render a single NPC vehicle.
   */
  private renderNPCVehicle(vehicle: IVehicle, relativeZ: number, relativeX: number): void {
    // Get NPC sprite
    var sprite = getNPCSprite(vehicle.npcType, vehicle.npcColorIndex);
    
    // Calculate screen position and scale based on distance
    // t: 0 = far (horizon), 1 = close (player position)
    var maxViewDist = 500;  // World units before cars disappear
    
    // For cars ahead (positive relativeZ): map to screen Y from horizon to near-player
    // For cars behind (negative relativeZ): they should appear below/at player level
    //   but we limit rendering to just slightly behind (-10) to avoid weirdness
    
    // Calculate t: how close to player (1 = at player, 0 = at horizon)
    // Use absolute value and sign to determine position
    var t: number;
    if (relativeZ >= 0) {
      // Car is ahead - map distance to screen position
      var normalizedDist = Math.min(1, relativeZ / maxViewDist);
      t = 1 - normalizedDist;  // 0 at max distance, 1 at player position
    } else {
      // Car is behind - should appear at/below player (t > 1)
      // But we only render up to -10 behind, so this is a small effect
      t = 1 + Math.abs(relativeZ) / 50;  // Extends slightly past player
    }
    
    // Map t to screen Y 
    // Use the VISUAL horizon (where road actually meets sky), not this.horizonY
    // The visual road vanishing point is around row 4-5 (buildings row)
    var visualHorizonY = 5;  // Actual vanishing point of road on screen
    var roadBottom = this.height - 4;
    var screenY = Math.round(visualHorizonY + t * (roadBottom - visualHorizonY));
    
    // Calculate curve offset for visual curve following
    // Use a simple approach: sample the curve at the NPC's position and scale by t
    var curveOffset = 0;
    if (this._currentRoad && relativeZ > 0) {
      var npcWorldZ = this._currentTrackPosition + relativeZ;
      var seg = this._currentRoad.getSegment(npcWorldZ);
      if (seg) {
        // Use t (screen depth) to scale the curve effect
        // Cars further away (smaller t) have less curve offset
        // Cars closer (larger t) have more curve offset
        curveOffset = seg.curve * t * 15;
      }
    }
    
    // Lateral position scales with perspective
    // Include curve offset so cars follow the road visually
    var perspectiveScale = t * t;  // Non-linear for more realistic perspective
    var screenX = Math.round(40 + curveOffset + relativeX * perspectiveScale * 25 - this._currentCameraX * 0.5);
    
    // Select sprite scale based on screen position (5 scales now: 0=dot, 1=tiny, 2=small, 3=medium, 4=large)
    var roadHeight = roadBottom - visualHorizonY;
    var screenProgress = (screenY - visualHorizonY) / roadHeight;  // 0 at horizon, 1 at bottom
    
    // Scale thresholds spread across the road
    var scaleIndex: number;
    if (screenProgress < 0.04) {
      scaleIndex = 0;  // Dot - right at horizon (top 4%)
    } else if (screenProgress < 0.10) {
      scaleIndex = 1;  // Tiny - near horizon (4-10%)
    } else if (screenProgress < 0.20) {
      scaleIndex = 2;  // Small - upper road (10-20%)
    } else if (screenProgress < 0.35) {
      scaleIndex = 3;  // Medium - middle road (20-35%)
    } else {
      scaleIndex = 4;  // Large - lower road (35%+)
    }
    
    // Clamp to available scales
    scaleIndex = Math.min(scaleIndex, sprite.variants.length - 1);
    
    // Get sprite dimensions
    var size = getSpriteSize(sprite, scaleIndex);
    
    // Center sprite horizontally
    screenX -= Math.floor(size.width / 2);
    
    // Draw sprite directly to road frame (NPCs are part of the road scene)
    var variant = sprite.variants[scaleIndex];
    var frame = this.frameManager.getRoadFrame();
    if (!frame) return;
    
    // Apply flash effect if vehicle is flashing (damage) or has visual effects
    var isFlashing = vehicle.flashTimer > 0;
    var flashAttr = makeAttr(LIGHTRED, BG_BLACK);
    
    // Check for status effects that have visual flash
    var hasLightning = vehicle.hasEffect(ItemType.LIGHTNING);
    var hasStar = vehicle.hasEffect(ItemType.STAR);
    var hasBullet = vehicle.hasEffect(ItemType.BULLET);
    
    // Lightning: blue/cyan flash (slower cycle)
    if (hasLightning) {
      var lightningCycle = Math.floor(Date.now() / 150) % 2;
      flashAttr = lightningCycle === 0 ? makeAttr(CYAN, BG_BLACK) : makeAttr(LIGHTCYAN, BG_BLUE);
      isFlashing = true;
    }
    
    // Star: rainbow cycling (fast)
    if (hasStar) {
      var starColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTMAGENTA, WHITE];
      var starIndex = Math.floor(Date.now() / 80) % starColors.length;
      flashAttr = makeAttr(starColors[starIndex], BG_BLACK);
      isFlashing = true;
    }
    
    // Bullet: white/yellow flash
    if (hasBullet) {
      var bulletCycle = Math.floor(Date.now() / 100) % 2;
      flashAttr = bulletCycle === 0 ? makeAttr(WHITE, BG_BLACK) : makeAttr(YELLOW, BG_BLACK);
      isFlashing = true;
    }
    
    // Visual horizon for bounds check (matches visualHorizonY above)
    var visualHorizon = 5;
    
    for (var row = 0; row < variant.length; row++) {
      for (var col = 0; col < variant[row].length; col++) {
        var cell = variant[row][col];
        if (cell !== null && cell !== undefined) {
          var drawX = screenX + col;
          var drawY = screenY + row;
          
          // Bounds check - allow drawing up to visual horizon
          if (drawX >= 0 && drawX < this.width && drawY >= visualHorizon && drawY < this.height - 1) {
            var attr = isFlashing && (Math.floor(Date.now() / 100) % 2 === 0) ? flashAttr : cell.attr;
            frame.setData(drawX, drawY, cell.char, attr);
          }
        }
      }
    }
  }
  
  /**
   * End frame - apply glitch effects if needed, then push updates to screen.
   */
  endFrame(): void {
    // Apply glitch overlay if on glitch theme
    if (this.activeTheme.name === 'glitch_circuit' && typeof GlitchState !== 'undefined') {
      this.applyGlitchEffects();
    }
    
    // Render any active lightning bolt effects
    this.renderLightningBolts();
    
    this.cycle();
  }
  
  /**
   * Get the underlying scene composer for direct rendering.
   * Used for dedicated screens like game over.
   */
  getComposer(): SceneComposer {
    return this.composer;
  }
  
  /**
   * Apply glitch visual effects to the current frame.
   */
  private applyGlitchEffects(): void {
    var roadFrame = this.frameManager.getRoadFrame();
    var groundFrame = this.frameManager.getGroundGridFrame();
    
    // Scanline noise on random rows
    if (GlitchState.intensity > 0.2 && roadFrame) {
      for (var row = 0; row < this.height - this.horizonY; row++) {
        if (GlitchState.isNoiseRow(row)) {
          // Fill row with noise
          var noiseAttr = makeAttr(
            Math.random() < 0.5 ? GREEN : LIGHTGREEN, 
            BG_BLACK
          );
          for (var x = 0; x < this.width; x++) {
            if (Math.random() < 0.7) {
              roadFrame.setData(x, row, GlitchState.getNoiseChar(), noiseAttr);
            }
          }
        }
      }
    }
    
    // Color corruption on ground
    if (GlitchState.intensity > 0.4 && groundFrame) {
      var corruptRows = Math.floor(GlitchState.intensity * 3);
      for (var cr = 0; cr < corruptRows; cr++) {
        var corruptY = Math.floor(Math.random() * (this.height - this.horizonY));
        var corruptX = Math.floor(Math.random() * this.width);
        var corruptW = Math.floor(Math.random() * 20) + 5;
        
        // Corrupt a horizontal strip with wrong colors
        var corruptedColor = GlitchState.corruptColor(GREEN, BG_BLACK);
        var corruptAttr = makeAttr(corruptedColor.fg, corruptedColor.bg);
        
        for (var cx = corruptX; cx < corruptX + corruptW && cx < this.width; cx++) {
          groundFrame.setData(cx, corruptY, GlitchState.corruptChar('#'), corruptAttr);
        }
      }
    }
    
    // Horizontal tear effect - shift a section of the road
    if (Math.abs(GlitchState.tearOffset) > 0 && roadFrame) {
      var tearY = 5 + Math.floor(Math.random() * 8);
      var offset = GlitchState.tearOffset;
      
      // We can't easily shift existing frame data, so add visual tear markers
      var tearAttr = makeAttr(LIGHTCYAN, BG_BLACK);
      if (offset > 0) {
        for (var tx = 0; tx < Math.abs(offset); tx++) {
          roadFrame.setData(tx, tearY, '>', tearAttr);
        }
      } else {
        for (var tx2 = this.width - Math.abs(offset); tx2 < this.width; tx2++) {
          roadFrame.setData(tx2, tearY, '<', tearAttr);
        }
      }
    }
    
    // Random character corruption on high intensity
    if (GlitchState.intensity > 0.6 && roadFrame) {
      var numCorruptions = Math.floor(GlitchState.intensity * 10);
      for (var i = 0; i < numCorruptions; i++) {
        var rx = Math.floor(Math.random() * this.width);
        var ry = Math.floor(Math.random() * (this.height - this.horizonY));
        var glitchAttr = makeAttr(
          [LIGHTGREEN, LIGHTCYAN, LIGHTRED, WHITE][Math.floor(Math.random() * 4)],
          BG_BLACK
        );
        roadFrame.setData(rx, ry, GlitchState.corruptChar('X'), glitchAttr);
      }
    }
    
    // Sky/horizon glitch effects
    this.applySkyGlitchEffects();
  }
  
  /**
   * Apply glitch effects to the sky/horizon area.
   */
  private applySkyGlitchEffects(): void {
    var mountainsFrame = this.frameManager.getMountainsFrame();
    var skyGridFrame = this.frameManager.getSkyGridFrame();
    
    // Based on current sky glitch type
    switch (GlitchState.skyGlitchType) {
      case 1:  // Matrix rain
        if (skyGridFrame) {
          var rainAttr = makeAttr(LIGHTGREEN, BG_BLACK);
          var rainDimAttr = makeAttr(GREEN, BG_BLACK);
          for (var d = 0; d < GlitchState.matrixRainDrops.length; d++) {
            var drop = GlitchState.matrixRainDrops[d];
            var dropY = Math.floor(drop.y);
            if (dropY >= 0 && dropY < this.horizonY && drop.x >= 0 && drop.x < this.width) {
              skyGridFrame.setData(drop.x, dropY, drop.char, rainAttr);
              // Trail
              if (dropY > 0) {
                skyGridFrame.setData(drop.x, dropY - 1, drop.char, rainDimAttr);
              }
            }
          }
        }
        break;
        
      case 2:  // Binary corruption on mountains
        if (mountainsFrame) {
          var binaryAttr = makeAttr(LIGHTGREEN, BG_BLACK);
          var numBinary = 15 + Math.floor(GlitchState.intensity * 20);
          for (var b = 0; b < numBinary; b++) {
            var bx = Math.floor(Math.random() * this.width);
            var by = Math.floor(Math.random() * this.horizonY);
            var binaryChar = Math.random() < 0.5 ? '0' : '1';
            mountainsFrame.setData(bx, by, binaryChar, binaryAttr);
          }
        }
        break;
        
      case 3:  // BSOD effect
        if (skyGridFrame) {
          var bsodBgAttr = makeAttr(WHITE, BG_BLUE);
          var bsodTextAttr = makeAttr(WHITE, BG_BLUE);
          // Fill part of sky with blue
          var bsodHeight = 3 + Math.floor(Math.random() * 3);
          var bsodY = Math.floor(Math.random() * (this.horizonY - bsodHeight));
          for (var by2 = bsodY; by2 < bsodY + bsodHeight && by2 < this.horizonY; by2++) {
            for (var bx2 = 10; bx2 < this.width - 10; bx2++) {
              skyGridFrame.setData(bx2, by2, ' ', bsodBgAttr);
            }
          }
          // Error text
          var errorMsgs = ['FATAL_ERROR', 'MEMORY_CORRUPT', 'REALITY.SYS', 'STACK_OVERFLOW', '0x0000DEAD'];
          var msg = errorMsgs[Math.floor(Math.random() * errorMsgs.length)];
          var msgX = Math.floor((this.width - msg.length) / 2);
          for (var c = 0; c < msg.length; c++) {
            if (msgX + c >= 0 && msgX + c < this.width) {
              skyGridFrame.setData(msgX + c, bsodY + 1, msg[c], bsodTextAttr);
            }
          }
        }
        break;
        
      case 4:  // Color invert/static burst
        if (mountainsFrame) {
          var staticColors = [LIGHTCYAN, LIGHTMAGENTA, YELLOW, WHITE];
          var numStatic = 20 + Math.floor(GlitchState.intensity * 30);
          for (var s = 0; s < numStatic; s++) {
            var sx = Math.floor(Math.random() * this.width);
            var sy = Math.floor(Math.random() * this.horizonY);
            var staticAttr = makeAttr(
              staticColors[Math.floor(Math.random() * staticColors.length)],
              BG_BLACK
            );
            var staticChars = [GLYPH.FULL_BLOCK, GLYPH.DARK_SHADE, GLYPH.MEDIUM_SHADE, '#', '%'];
            mountainsFrame.setData(sx, sy, staticChars[Math.floor(Math.random() * staticChars.length)], staticAttr);
          }
        }
        break;
    }
    
    // Random mountain corruption at any intensity
    if (GlitchState.intensity > 0.3 && mountainsFrame && Math.random() < 0.3) {
      // Corrupt a random section of mountains
      var corruptX = Math.floor(Math.random() * this.width);
      var corruptY = Math.floor(Math.random() * this.horizonY);
      var corruptW = 3 + Math.floor(Math.random() * 8);
      var corruptH = 1 + Math.floor(Math.random() * 3);
      var corruptColors = [LIGHTGREEN, GREEN, LIGHTCYAN, CYAN];
      var corruptAttr2 = makeAttr(
        corruptColors[Math.floor(Math.random() * corruptColors.length)],
        BG_BLACK
      );
      
      for (var cy = corruptY; cy < corruptY + corruptH && cy < this.horizonY; cy++) {
        for (var cx = corruptX; cx < corruptX + corruptW && cx < this.width; cx++) {
          if (Math.random() < 0.6) {
            var glitchChars = ['/', '\\', '|', '-', '+', '#', '0', '1'];
            mountainsFrame.setData(cx, cy, glitchChars[Math.floor(Math.random() * glitchChars.length)], corruptAttr2);
          }
        }
      }
    }
    
    // Sun flicker effect
    if (GlitchState.intensity > 0.5 && Math.random() < 0.2) {
      var sunFrame = this.frameManager.getSunFrame();
      if (sunFrame) {
        var flickerAttr = makeAttr(
          Math.random() < 0.5 ? BLACK : LIGHTGREEN,
          Math.random() < 0.3 ? BG_GREEN : BG_BLACK
        );
        // Corrupt a few sun pixels
        for (var sf = 0; sf < 3; sf++) {
          var sfx = Math.floor(Math.random() * 8) + 36;
          var sfy = Math.floor(Math.random() * 4) + 2;
          sunFrame.setData(sfx, sfy, GlitchState.getNoiseChar(), flickerAttr);
        }
      }
    }
  }
  
  // ============================================================
  // Internal rendering methods  
  // ============================================================

  /**
   * Render the sun to its frame (static, rendered once).
   */
  private renderSun(): void {
    var sunFrame = this.frameManager.getSunFrame();
    if (!sunFrame) return;
    
    var colors = this.activeTheme.colors;
    var sunCoreAttr = makeAttr(colors.celestialCore.fg, colors.celestialCore.bg);
    var sunGlowAttr = makeAttr(colors.celestialGlow.fg, colors.celestialGlow.bg);
    
    // Sun position based on theme
    var celestial = this.activeTheme.celestial;
    var sunX = Math.floor(this.width * celestial.positionX) - 3;
    var sunY = Math.floor(this.horizonY * celestial.positionY);
    
    // Size based on theme (1-5 scale)
    var size = celestial.size;
    var coreWidth = size + 2;
    var coreHeight = Math.max(1, size);
    
    // Draw sun core
    for (var dy = 0; dy < coreHeight; dy++) {
      for (var dx = 0; dx < coreWidth; dx++) {
        sunFrame.setData(sunX + dx, sunY + dy, GLYPH.FULL_BLOCK, sunCoreAttr);
      }
    }
    
    // Draw glow around edges
    var glowChar = GLYPH.DARK_SHADE;
    // Top edge
    for (var x = sunX - 1; x <= sunX + coreWidth; x++) {
      sunFrame.setData(x, sunY - 1, glowChar, sunGlowAttr);
    }
    // Bottom edge
    for (var x = sunX - 1; x <= sunX + coreWidth; x++) {
      sunFrame.setData(x, sunY + coreHeight, glowChar, sunGlowAttr);
    }
    // Side edges
    for (var dy = 0; dy < coreHeight; dy++) {
      sunFrame.setData(sunX - 1, sunY + dy, glowChar, sunGlowAttr);
      sunFrame.setData(sunX + coreWidth, sunY + dy, glowChar, sunGlowAttr);
    }
  }
  
  /**
   * Render the moon to its frame (for night themes).
   * Creates a glowing crescent moon similar in prominence to the sun.
   */
  private renderMoon(): void {
    var moonFrame = this.frameManager.getSunFrame();  // Reuse sun frame slot
    if (!moonFrame) return;
    
    var colors = this.activeTheme.colors;
    var moonCoreAttr = makeAttr(colors.celestialCore.fg, colors.celestialCore.bg);
    var moonGlowAttr = makeAttr(colors.celestialGlow.fg, colors.celestialGlow.bg);
    var moonGlowDimAttr = makeAttr(CYAN, BG_BLACK);  // Outer glow
    
    var celestial = this.activeTheme.celestial;
    var moonX = Math.floor(this.width * celestial.positionX);
    var moonY = Math.max(1, Math.floor((this.horizonY - 2) * celestial.positionY));
    
    // Prominent crescent moon with glow halo
    // Core crescent shape (bright)
    moonFrame.setData(moonX, moonY, ')', moonCoreAttr);
    moonFrame.setData(moonX, moonY + 1, ')', moonCoreAttr);
    moonFrame.setData(moonX + 1, moonY, ')', moonCoreAttr);
    moonFrame.setData(moonX + 1, moonY + 1, ')', moonCoreAttr);
    
    // Inner glow (bright cyan/yellow)
    moonFrame.setData(moonX - 1, moonY, GLYPH.MEDIUM_SHADE, moonGlowAttr);
    moonFrame.setData(moonX - 1, moonY + 1, GLYPH.MEDIUM_SHADE, moonGlowAttr);
    moonFrame.setData(moonX + 2, moonY, GLYPH.MEDIUM_SHADE, moonGlowAttr);
    moonFrame.setData(moonX + 2, moonY + 1, GLYPH.MEDIUM_SHADE, moonGlowAttr);
    moonFrame.setData(moonX, moonY - 1, GLYPH.MEDIUM_SHADE, moonGlowAttr);
    moonFrame.setData(moonX + 1, moonY - 1, GLYPH.MEDIUM_SHADE, moonGlowAttr);
    moonFrame.setData(moonX, moonY + 2, GLYPH.MEDIUM_SHADE, moonGlowAttr);
    moonFrame.setData(moonX + 1, moonY + 2, GLYPH.MEDIUM_SHADE, moonGlowAttr);
    
    // Outer glow (dimmer, wider)
    moonFrame.setData(moonX - 2, moonY, GLYPH.DARK_SHADE, moonGlowDimAttr);
    moonFrame.setData(moonX - 2, moonY + 1, GLYPH.DARK_SHADE, moonGlowDimAttr);
    moonFrame.setData(moonX + 3, moonY, GLYPH.DARK_SHADE, moonGlowDimAttr);
    moonFrame.setData(moonX + 3, moonY + 1, GLYPH.DARK_SHADE, moonGlowDimAttr);
    moonFrame.setData(moonX - 1, moonY - 1, GLYPH.DARK_SHADE, moonGlowDimAttr);
    moonFrame.setData(moonX + 2, moonY - 1, GLYPH.DARK_SHADE, moonGlowDimAttr);
    moonFrame.setData(moonX - 1, moonY + 2, GLYPH.DARK_SHADE, moonGlowDimAttr);
    moonFrame.setData(moonX + 2, moonY + 2, GLYPH.DARK_SHADE, moonGlowDimAttr);
  }
  
  /**
   * Render dual moons for fantasy/enchanted themes.
   * One larger pale moon and one smaller colored moon.
   */
  private renderDualMoons(): void {
    var moonFrame = this.frameManager.getSunFrame();
    if (!moonFrame) return;
    
    var colors = this.activeTheme.colors;
    var moonCoreAttr = makeAttr(colors.celestialCore.fg, colors.celestialCore.bg);
    var moonGlowAttr = makeAttr(colors.celestialGlow.fg, colors.celestialGlow.bg);
    
    var celestial = this.activeTheme.celestial;
    
    // Main moon (larger, right of center based on positionX)
    var moon1X = Math.floor(this.width * celestial.positionX);
    var moon1Y = Math.max(1, Math.floor((this.horizonY - 3) * celestial.positionY));
    
    // Main moon - full/gibbous
    moonFrame.setData(moon1X, moon1Y, '(', moonCoreAttr);
    moonFrame.setData(moon1X + 1, moon1Y, ')', moonCoreAttr);
    moonFrame.setData(moon1X, moon1Y + 1, '(', moonCoreAttr);
    moonFrame.setData(moon1X + 1, moon1Y + 1, ')', moonCoreAttr);
    
    // Main moon glow
    moonFrame.setData(moon1X - 1, moon1Y, GLYPH.LIGHT_SHADE, moonGlowAttr);
    moonFrame.setData(moon1X + 2, moon1Y, GLYPH.LIGHT_SHADE, moonGlowAttr);
    moonFrame.setData(moon1X - 1, moon1Y + 1, GLYPH.LIGHT_SHADE, moonGlowAttr);
    moonFrame.setData(moon1X + 2, moon1Y + 1, GLYPH.LIGHT_SHADE, moonGlowAttr);
    moonFrame.setData(moon1X, moon1Y - 1, GLYPH.LIGHT_SHADE, moonGlowAttr);
    moonFrame.setData(moon1X + 1, moon1Y - 1, GLYPH.LIGHT_SHADE, moonGlowAttr);
    moonFrame.setData(moon1X, moon1Y + 2, GLYPH.LIGHT_SHADE, moonGlowAttr);
    moonFrame.setData(moon1X + 1, moon1Y + 2, GLYPH.LIGHT_SHADE, moonGlowAttr);
    
    // Secondary moon (smaller, different position)
    // Offset from main moon - upper left
    var moon2X = Math.floor(this.width * 0.25);
    var moon2Y = Math.max(1, Math.floor((this.horizonY - 2) * 0.2));
    var moon2Attr = makeAttr(LIGHTCYAN, BG_CYAN);  // Cyan/teal moon
    var moon2GlowAttr = makeAttr(CYAN, BG_BLACK);
    
    // Smaller crescent
    moonFrame.setData(moon2X, moon2Y, ')', moon2Attr);
    
    // Small glow
    moonFrame.setData(moon2X - 1, moon2Y, GLYPH.LIGHT_SHADE, moon2GlowAttr);
    moonFrame.setData(moon2X + 1, moon2Y, GLYPH.LIGHT_SHADE, moon2GlowAttr);
    moonFrame.setData(moon2X, moon2Y - 1, GLYPH.LIGHT_SHADE, moon2GlowAttr);
    moonFrame.setData(moon2X, moon2Y + 1, GLYPH.LIGHT_SHADE, moon2GlowAttr);
  }

  /**
   * Render DUAL monster silhouettes for kaiju theme.
   * Mothra on left, Godzilla on right - you drive between them!
   */
  private renderMonsterSilhouette(): void {
    var frame = this.frameManager.getSunFrame();
    if (!frame) return;
    
    // Ground the monsters at the horizon line (feet touch ground)
    var baseY = this.horizonY;
    
    // Position monsters closer together, between road and buildings
    var mothraX = 24;    // Mothra on left side
    var godzillaX = 56;  // Godzilla on right side
    
    // === MOTHRA (left side) - Giant moth viewed from front, wings spread WIDE ===
    var mothBody = makeAttr(BROWN, BG_BLACK);
    var mothWing = makeAttr(YELLOW, BG_BLACK);
    var mothWingLight = makeAttr(WHITE, BG_BROWN);
    var mothEye = makeAttr(LIGHTCYAN, BG_CYAN);
    
    // Antennae (curving outward at top)
    frame.setData(mothraX - 3, baseY - 8, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX + 3, baseY - 8, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX - 2, baseY - 7, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX + 2, baseY - 7, GLYPH.FULL_BLOCK, mothBody);
    
    // Head with big compound eyes
    frame.setData(mothraX - 2, baseY - 6, GLYPH.FULL_BLOCK, mothEye);
    frame.setData(mothraX - 1, baseY - 6, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX, baseY - 6, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX + 1, baseY - 6, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX + 2, baseY - 6, GLYPH.FULL_BLOCK, mothEye);
    
    // Upper wings - spread WIDE horizontally (main visual feature)
    // Left upper wing - wide and horizontal
    for (var wx = mothraX - 12; wx <= mothraX - 3; wx++) {
      frame.setData(wx, baseY - 5, GLYPH.FULL_BLOCK, mothWing);
    }
    for (var wx = mothraX - 11; wx <= mothraX - 3; wx++) {
      frame.setData(wx, baseY - 4, GLYPH.FULL_BLOCK, (wx > mothraX - 9) ? mothWingLight : mothWing);
    }
    for (var wx = mothraX - 10; wx <= mothraX - 3; wx++) {
      frame.setData(wx, baseY - 3, GLYPH.FULL_BLOCK, (wx > mothraX - 8) ? mothWingLight : mothWing);
    }
    
    // Right upper wing - mirror
    for (var wx = mothraX + 3; wx <= mothraX + 12; wx++) {
      frame.setData(wx, baseY - 5, GLYPH.FULL_BLOCK, mothWing);
    }
    for (var wx = mothraX + 3; wx <= mothraX + 11; wx++) {
      frame.setData(wx, baseY - 4, GLYPH.FULL_BLOCK, (wx < mothraX + 9) ? mothWingLight : mothWing);
    }
    for (var wx = mothraX + 3; wx <= mothraX + 10; wx++) {
      frame.setData(wx, baseY - 3, GLYPH.FULL_BLOCK, (wx < mothraX + 8) ? mothWingLight : mothWing);
    }
    
    // Thorax/body center (connects wings)
    frame.setData(mothraX - 2, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX - 1, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX + 1, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX + 2, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
    
    frame.setData(mothraX - 1, baseY - 4, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX, baseY - 4, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX + 1, baseY - 4, GLYPH.FULL_BLOCK, mothBody);
    
    // Lower wings (smaller, below body)
    for (var wx = mothraX - 8; wx <= mothraX - 2; wx++) {
      frame.setData(wx, baseY - 2, GLYPH.FULL_BLOCK, mothWing);
    }
    for (var wx = mothraX - 6; wx <= mothraX - 2; wx++) {
      frame.setData(wx, baseY - 1, GLYPH.FULL_BLOCK, mothWing);
    }
    for (var wx = mothraX + 2; wx <= mothraX + 8; wx++) {
      frame.setData(wx, baseY - 2, GLYPH.FULL_BLOCK, mothWing);
    }
    for (var wx = mothraX + 2; wx <= mothraX + 6; wx++) {
      frame.setData(wx, baseY - 1, GLYPH.FULL_BLOCK, mothWing);
    }
    
    // Abdomen (fuzzy body below thorax)
    frame.setData(mothraX - 1, baseY - 3, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX, baseY - 3, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX + 1, baseY - 3, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX, baseY - 2, GLYPH.FULL_BLOCK, mothBody);
    frame.setData(mothraX, baseY - 1, GLYPH.FULL_BLOCK, mothBody);
    
    // === GODZILLA (right side) - Green dinosaur ===
    var godzBody = makeAttr(GREEN, BG_BLACK);
    var godzLight = makeAttr(LIGHTGREEN, BG_BLACK);
    var godzEye = makeAttr(LIGHTRED, BG_RED);
    var godzSpine = makeAttr(LIGHTCYAN, BG_CYAN);
    var godzBreath = makeAttr(LIGHTCYAN, BG_BLACK);
    
    // Dorsal spines
    frame.setData(godzillaX + 1, baseY - 10, GLYPH.FULL_BLOCK, godzSpine);
    frame.setData(godzillaX + 2, baseY - 9, GLYPH.FULL_BLOCK, godzSpine);
    frame.setData(godzillaX + 3, baseY - 10, GLYPH.FULL_BLOCK, godzSpine);
    frame.setData(godzillaX + 4, baseY - 9, GLYPH.FULL_BLOCK, godzSpine);
    
    // Head (angular dinosaur) - solid, no gaps
    frame.setData(godzillaX - 3, baseY - 9, GLYPH.UPPER_HALF, godzLight);
    frame.setData(godzillaX - 2, baseY - 9, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 1, baseY - 9, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX, baseY - 9, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX + 1, baseY - 9, GLYPH.UPPER_HALF, godzBody);
    
    frame.setData(godzillaX - 4, baseY - 8, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 3, baseY - 8, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 2, baseY - 8, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 1, baseY - 8, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX, baseY - 8, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX + 1, baseY - 8, GLYPH.FULL_BLOCK, godzBody);
    
    // Eye row - solid
    frame.setData(godzillaX - 5, baseY - 7, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 4, baseY - 7, GLYPH.FULL_BLOCK, godzEye);  // LEFT EYE
    frame.setData(godzillaX - 3, baseY - 7, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 2, baseY - 7, GLYPH.FULL_BLOCK, godzEye);  // RIGHT EYE
    frame.setData(godzillaX - 1, baseY - 7, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX, baseY - 7, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 1, baseY - 7, GLYPH.FULL_BLOCK, godzBody);
    
    // Snout/jaw - open mouth
    frame.setData(godzillaX - 7, baseY - 6, GLYPH.LEFT_HALF, godzBody);
    frame.setData(godzillaX - 6, baseY - 6, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 5, baseY - 6, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 4, baseY - 6, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 3, baseY - 6, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 2, baseY - 6, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 1, baseY - 6, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX, baseY - 6, GLYPH.FULL_BLOCK, godzBody);
    
    // Atomic breath shooting LEFT toward Mothra!
    frame.setData(godzillaX - 8, baseY - 6, GLYPH.FULL_BLOCK, godzBreath);
    frame.setData(godzillaX - 9, baseY - 6, GLYPH.FULL_BLOCK, godzBreath);
    frame.setData(godzillaX - 10, baseY - 6, GLYPH.FULL_BLOCK, godzSpine);
    frame.setData(godzillaX - 11, baseY - 6, GLYPH.MEDIUM_SHADE, godzBreath);
    frame.setData(godzillaX - 10, baseY - 5, GLYPH.LIGHT_SHADE, godzBreath);
    frame.setData(godzillaX - 10, baseY - 7, GLYPH.LIGHT_SHADE, godzBreath);
    
    // Neck - solid connection to body
    frame.setData(godzillaX - 3, baseY - 5, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 2, baseY - 5, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 1, baseY - 5, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX, baseY - 5, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX + 1, baseY - 5, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 2, baseY - 5, GLYPH.FULL_BLOCK, godzBody);
    
    // Body - wider torso
    frame.setData(godzillaX - 4, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 3, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 2, baseY - 4, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 1, baseY - 4, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX, baseY - 4, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX + 1, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 2, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 3, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
    
    // Arms reaching
    frame.setData(godzillaX - 6, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 5, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
    
    // Lower body
    frame.setData(godzillaX - 3, baseY - 3, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 2, baseY - 3, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 1, baseY - 3, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX, baseY - 3, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX + 1, baseY - 3, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 2, baseY - 3, GLYPH.FULL_BLOCK, godzBody);
    
    // Legs - LEFT LEG
    frame.setData(godzillaX - 3, baseY - 2, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 2, baseY - 2, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX - 3, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX - 2, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
    
    // Legs - RIGHT LEG
    frame.setData(godzillaX + 1, baseY - 2, GLYPH.FULL_BLOCK, godzLight);
    frame.setData(godzillaX + 2, baseY - 2, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 1, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 2, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
    
    // Tail going right
    frame.setData(godzillaX + 3, baseY - 3, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 4, baseY - 2, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 5, baseY - 2, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 6, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
    frame.setData(godzillaX + 7, baseY - 1, GLYPH.RIGHT_HALF, godzBody);
  }

  /**
   * Render mermaid silhouette for underwater theme.
   * Graceful swimming figure with flowing hair and tail.
   */
  private renderMermaid(): void {
    var frame = this.frameManager.getSunFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var coreAttr = makeAttr(colors.celestialCore.fg, colors.celestialCore.bg);
    var glowAttr = makeAttr(colors.celestialGlow.fg, colors.celestialGlow.bg);
    var hairAttr = makeAttr(LIGHTMAGENTA, BG_BLUE);
    var tailAttr = makeAttr(LIGHTCYAN, BG_BLUE);
    var tailGlowAttr = makeAttr(CYAN, BG_BLUE);
    
    // Position based on theme settings
    var posX = Math.floor(this.width * (this.activeTheme.celestial.positionX || 0.7));
    var posY = Math.floor(this.horizonY * (this.activeTheme.celestial.positionY || 0.4));
    
    // Flowing hair (left side, streaming behind)
    frame.setData(posX - 5, posY - 2, '~', hairAttr);
    frame.setData(posX - 4, posY - 2, '~', hairAttr);
    frame.setData(posX - 3, posY - 1, '~', hairAttr);
    frame.setData(posX - 4, posY - 1, '~', hairAttr);
    frame.setData(posX - 5, posY, '~', hairAttr);
    frame.setData(posX - 4, posY, '~', hairAttr);
    frame.setData(posX - 3, posY, '~', hairAttr);
    
    // Head (circular)
    frame.setData(posX - 2, posY - 1, '(', coreAttr);
    frame.setData(posX - 1, posY - 1, GLYPH.FULL_BLOCK, coreAttr);
    frame.setData(posX, posY - 1, ')', coreAttr);
    
    // Upper body/torso
    frame.setData(posX - 1, posY, GLYPH.FULL_BLOCK, coreAttr);
    frame.setData(posX, posY, GLYPH.FULL_BLOCK, coreAttr);
    frame.setData(posX + 1, posY, '\\', coreAttr);  // Arm reaching forward
    
    // Waist transition
    frame.setData(posX, posY + 1, GLYPH.FULL_BLOCK, glowAttr);
    frame.setData(posX + 1, posY + 1, GLYPH.FULL_BLOCK, tailAttr);
    
    // Tail (curving right and up)
    frame.setData(posX + 2, posY + 1, GLYPH.FULL_BLOCK, tailAttr);
    frame.setData(posX + 3, posY + 1, GLYPH.FULL_BLOCK, tailAttr);
    frame.setData(posX + 4, posY, GLYPH.FULL_BLOCK, tailAttr);
    frame.setData(posX + 5, posY, GLYPH.FULL_BLOCK, tailAttr);
    frame.setData(posX + 6, posY - 1, GLYPH.FULL_BLOCK, tailAttr);
    
    // Tail fin (forked)
    frame.setData(posX + 7, posY - 2, '/', tailGlowAttr);
    frame.setData(posX + 7, posY - 1, GLYPH.FULL_BLOCK, tailGlowAttr);
    frame.setData(posX + 7, posY, '\\', tailGlowAttr);
    frame.setData(posX + 8, posY - 2, '/', tailGlowAttr);
    frame.setData(posX + 8, posY, '\\', tailGlowAttr);
    
    // Glow effect around mermaid
    frame.setData(posX - 2, posY - 2, GLYPH.LIGHT_SHADE, glowAttr);
    frame.setData(posX + 1, posY - 2, GLYPH.LIGHT_SHADE, glowAttr);
    frame.setData(posX - 2, posY + 1, GLYPH.LIGHT_SHADE, glowAttr);
    frame.setData(posX + 2, posY + 2, GLYPH.LIGHT_SHADE, glowAttr);
  }

  /**
   * Render underwater background - rock formations, kelp forest, coral.
   * Creates a deep sea grotto atmosphere.
   */
  private renderUnderwaterBackground(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var rockAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var rockLightAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var kelpAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    var coralAttr = makeAttr(LIGHTMAGENTA, BG_BLUE);
    var coralYellowAttr = makeAttr(YELLOW, BG_BLUE);
    
    // Rock formations/cave walls at sides
    // Left cave wall
    for (var y = 0; y < this.horizonY; y++) {
      var wallWidth = 6 - Math.floor(y * 0.5);
      for (var x = 0; x < wallWidth && x < 10; x++) {
        var char = (x === wallWidth - 1) ? GLYPH.RIGHT_HALF : GLYPH.FULL_BLOCK;
        var attr = ((x + y) % 3 === 0) ? rockLightAttr : rockAttr;
        frame.setData(x, y, char, attr);
      }
    }
    
    // Right cave wall
    for (var y = 0; y < this.horizonY; y++) {
      var wallWidth = 5 - Math.floor(y * 0.4);
      for (var x = 0; x < wallWidth && x < 8; x++) {
        var rx = this.width - 1 - x;
        var char = (x === wallWidth - 1) ? GLYPH.LEFT_HALF : GLYPH.FULL_BLOCK;
        var attr = ((x + y) % 3 === 0) ? rockLightAttr : rockAttr;
        frame.setData(rx, y, char, attr);
      }
    }
    
    // Kelp forest (swaying plants at mid-ground)
    var kelpPositions = [12, 18, 25, 55, 62, 68];
    for (var i = 0; i < kelpPositions.length; i++) {
      var kx = kelpPositions[i];
      var kelpHeight = 3 + (i % 3);
      for (var ky = 0; ky < kelpHeight; ky++) {
        var y = this.horizonY - 1 - ky;
        if (y >= 0) {
          var kchar = (ky % 2 === 0) ? ')' : '(';
          frame.setData(kx, y, kchar, kelpAttr);
          // Double kelp strands
          if (i % 2 === 0 && kx + 1 < this.width) {
            frame.setData(kx + 1, y, (ky % 2 === 0) ? '(' : ')', kelpAttr);
          }
        }
      }
    }
    
    // Coral clusters at bottom
    var coralPositions = [8, 22, 35, 48, 58, 72];
    for (var i = 0; i < coralPositions.length; i++) {
      var cx = coralPositions[i];
      var y = this.horizonY - 1;
      var attr = (i % 2 === 0) ? coralAttr : coralYellowAttr;
      frame.setData(cx, y, '*', attr);
      frame.setData(cx + 1, y, 'Y', attr);
      frame.setData(cx + 2, y, '*', attr);
      if (y - 1 >= 0) {
        frame.setData(cx + 1, y - 1, '^', attr);
      }
    }
    
    // Distant bubbles/particles in the water (static, animated ones are in ground)
    var bubblePositions = [[30, 2], [45, 3], [50, 1], [15, 4], [65, 2]];
    for (var i = 0; i < bubblePositions.length; i++) {
      var bx = bubblePositions[i][0];
      var by = bubblePositions[i][1];
      frame.setData(bx, by, 'o', makeAttr(WHITE, BG_BLUE));
    }
  }

  /**
   * Render aquarium background - glass panels on sides with underwater scene
   * and a beautiful mermaid swimming in the center aquarium tank.
   */
  private renderAquariumBackground(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    // Glass panel colors (the gray columns the user liked)
    var glassFrameAttr = makeAttr(LIGHTGRAY, BG_BLACK);
    var glassHighlightAttr = makeAttr(WHITE, BG_BLACK);
    var glassDarkAttr = makeAttr(DARKGRAY, BG_BLACK);
    
    // Aquarium interior colors
    var waterAttr = makeAttr(CYAN, BG_BLUE);
    var waterDeepAttr = makeAttr(BLUE, BG_BLUE);
    var coralAttr = makeAttr(LIGHTRED, BG_BLUE);
    var coralYellowAttr = makeAttr(YELLOW, BG_BLUE);
    var plantAttr = makeAttr(LIGHTGREEN, BG_BLUE);
    var sandAttr = makeAttr(YELLOW, BG_BLUE);
    var rockAttr = makeAttr(DARKGRAY, BG_BLUE);
    
    // Mermaid colors
    var mermaidSkinAttr = makeAttr(LIGHTMAGENTA, BG_BLUE);
    var mermaidHairAttr = makeAttr(LIGHTRED, BG_BLUE);
    var mermaidTailAttr = makeAttr(LIGHTCYAN, BG_BLUE);
    var mermaidTailGlowAttr = makeAttr(CYAN, BG_BLUE);
    
    // Left glass panel (column) - x: 0-7
    for (var y = 0; y < this.horizonY; y++) {
      // Outer frame
      frame.setData(0, y, GLYPH.FULL_BLOCK, glassDarkAttr);
      frame.setData(1, y, GLYPH.FULL_BLOCK, glassFrameAttr);
      // Glass highlight
      frame.setData(2, y, GLYPH.LIGHT_SHADE, glassHighlightAttr);
      // Inner frame
      for (var x = 3; x < 7; x++) {
        frame.setData(x, y, GLYPH.FULL_BLOCK, glassFrameAttr);
      }
      frame.setData(7, y, GLYPH.RIGHT_HALF, glassFrameAttr);
    }
    
    // Right glass panel (column) - x: 72-79
    for (var y = 0; y < this.horizonY; y++) {
      frame.setData(72, y, GLYPH.LEFT_HALF, glassFrameAttr);
      for (var x = 73; x < 77; x++) {
        frame.setData(x, y, GLYPH.FULL_BLOCK, glassFrameAttr);
      }
      // Glass highlight
      frame.setData(77, y, GLYPH.LIGHT_SHADE, glassHighlightAttr);
      frame.setData(78, y, GLYPH.FULL_BLOCK, glassFrameAttr);
      frame.setData(79, y, GLYPH.FULL_BLOCK, glassDarkAttr);
    }
    
    // Center aquarium tank (between the pillars)
    // Fill with water
    for (var y = 0; y < this.horizonY; y++) {
      for (var x = 8; x < 72; x++) {
        var attr = (y < 4) ? waterAttr : waterDeepAttr;
        frame.setData(x, y, ' ', attr);
      }
    }
    
    // Sandy bottom
    var bottomY = this.horizonY - 1;
    for (var x = 8; x < 72; x++) {
      var sandChar = (x % 3 === 0) ? GLYPH.LOWER_HALF : GLYPH.LIGHT_SHADE;
      frame.setData(x, bottomY, sandChar, sandAttr);
    }
    
    // Rocks on the sand
    var rockPositions = [12, 25, 45, 62];
    for (var i = 0; i < rockPositions.length; i++) {
      var rx = rockPositions[i];
      frame.setData(rx, bottomY, GLYPH.FULL_BLOCK, rockAttr);
      frame.setData(rx + 1, bottomY, GLYPH.FULL_BLOCK, rockAttr);
      if (bottomY - 1 >= 0) {
        frame.setData(rx, bottomY - 1, GLYPH.LOWER_HALF, rockAttr);
      }
    }
    
    // Coral clusters
    var coralPositions = [15, 32, 50, 65];
    for (var i = 0; i < coralPositions.length; i++) {
      var cx = coralPositions[i];
      var attr = (i % 2 === 0) ? coralAttr : coralYellowAttr;
      frame.setData(cx, bottomY, 'Y', attr);
      frame.setData(cx + 1, bottomY, '*', attr);
      frame.setData(cx + 2, bottomY, 'Y', attr);
      if (bottomY - 1 >= 0) {
        frame.setData(cx + 1, bottomY - 1, '^', attr);
      }
      if (bottomY - 2 >= 0) {
        frame.setData(cx + 1, bottomY - 2, '*', attr);
      }
    }
    
    // Seaweed/kelp on sides
    var kelpLeft = [10, 14, 18];
    var kelpRight = [58, 62, 68];
    var kelpPositions = kelpLeft.concat(kelpRight);
    for (var i = 0; i < kelpPositions.length; i++) {
      var kx = kelpPositions[i];
      for (var ky = 0; ky < 4; ky++) {
        var y = bottomY - 1 - ky;
        if (y >= 0) {
          var kchar = (ky % 2 === 0) ? ')' : '(';
          frame.setData(kx, y, kchar, plantAttr);
        }
      }
    }
    
    // === MERMAID in center of aquarium ===
    var mermaidX = 38;
    var mermaidY = 5;
    
    // Flowing hair (streams behind)
    frame.setData(mermaidX - 6, mermaidY - 1, '~', mermaidHairAttr);
    frame.setData(mermaidX - 5, mermaidY - 1, '~', mermaidHairAttr);
    frame.setData(mermaidX - 4, mermaidY - 1, '~', mermaidHairAttr);
    frame.setData(mermaidX - 5, mermaidY, '~', mermaidHairAttr);
    frame.setData(mermaidX - 4, mermaidY, '~', mermaidHairAttr);
    frame.setData(mermaidX - 3, mermaidY, '~', mermaidHairAttr);
    frame.setData(mermaidX - 4, mermaidY + 1, '~', mermaidHairAttr);
    frame.setData(mermaidX - 3, mermaidY + 1, '~', mermaidHairAttr);
    
    // Head
    frame.setData(mermaidX - 2, mermaidY, '(', mermaidSkinAttr);
    frame.setData(mermaidX - 1, mermaidY, GLYPH.FULL_BLOCK, mermaidSkinAttr);
    frame.setData(mermaidX, mermaidY, ')', mermaidSkinAttr);
    
    // Torso  
    frame.setData(mermaidX - 1, mermaidY + 1, GLYPH.FULL_BLOCK, mermaidSkinAttr);
    frame.setData(mermaidX, mermaidY + 1, GLYPH.FULL_BLOCK, mermaidSkinAttr);
    frame.setData(mermaidX + 1, mermaidY + 1, '\\', mermaidSkinAttr);  // Arm
    
    // Hip/waist
    frame.setData(mermaidX, mermaidY + 2, GLYPH.FULL_BLOCK, mermaidTailAttr);
    frame.setData(mermaidX + 1, mermaidY + 2, GLYPH.FULL_BLOCK, mermaidTailAttr);
    
    // Tail (curves right)
    frame.setData(mermaidX + 2, mermaidY + 2, GLYPH.FULL_BLOCK, mermaidTailAttr);
    frame.setData(mermaidX + 3, mermaidY + 2, GLYPH.FULL_BLOCK, mermaidTailAttr);
    frame.setData(mermaidX + 4, mermaidY + 1, GLYPH.FULL_BLOCK, mermaidTailAttr);
    frame.setData(mermaidX + 5, mermaidY + 1, GLYPH.FULL_BLOCK, mermaidTailAttr);
    frame.setData(mermaidX + 6, mermaidY, GLYPH.FULL_BLOCK, mermaidTailAttr);
    
    // Tail fin (forked)
    frame.setData(mermaidX + 7, mermaidY - 1, '/', mermaidTailGlowAttr);
    frame.setData(mermaidX + 7, mermaidY, GLYPH.FULL_BLOCK, mermaidTailGlowAttr);
    frame.setData(mermaidX + 7, mermaidY + 1, '\\', mermaidTailGlowAttr);
    frame.setData(mermaidX + 8, mermaidY - 1, '/', mermaidTailGlowAttr);
    frame.setData(mermaidX + 8, mermaidY + 1, '\\', mermaidTailGlowAttr);
    
    // Small fish near mermaid
    var fishAttr = makeAttr(YELLOW, BG_BLUE);
    frame.setData(mermaidX - 8, mermaidY + 3, '<', fishAttr);
    frame.setData(mermaidX - 7, mermaidY + 3, '>', fishAttr);
    
    frame.setData(mermaidX + 12, mermaidY - 2, '<', makeAttr(LIGHTRED, BG_BLUE));
    frame.setData(mermaidX + 13, mermaidY - 2, '>', makeAttr(LIGHTRED, BG_BLUE));
    
    // Bubbles rising
    frame.setData(mermaidX + 2, mermaidY - 2, 'o', makeAttr(WHITE, BG_BLUE));
    frame.setData(mermaidX + 3, mermaidY - 3, '.', makeAttr(WHITE, BG_BLUE));
    frame.setData(20, 3, 'o', makeAttr(WHITE, BG_BLUE));
    frame.setData(55, 2, 'o', makeAttr(WHITE, BG_BLUE));
  }

  /**
   * Render ANSI tunnel static elements - clear all overlapping frames.
   * The ANSI tunnel renders dynamically to sky and road frames,
   * so we need to ensure mountains/sun frames are clear (transparent).
   */
  private renderANSITunnelStatic(): void {
    // Clear the mountains frame completely so it doesn't obscure the sky
    var mtnsFrame = this.frameManager.getMountainsFrame();
    if (mtnsFrame) {
      mtnsFrame.clear();  // Make fully transparent
    }
    
    // Clear the sun frame completely  
    var sunFrame = this.frameManager.getSunFrame();
    if (sunFrame) {
      sunFrame.clear();  // Make fully transparent
    }
    
    // Sky grid frame will be rendered dynamically by ANSITunnelRenderer
    // Road frame will be rendered dynamically by ANSITunnelRenderer
  }

  /**
   * Render mountains to their frame (can be scrolled for parallax).
   */
  private renderMountains(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var mountainAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var highlightAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    
    // Mountain silhouettes at horizon
    var mountains = [
      { x: 5, height: 4, width: 12 },
      { x: 20, height: 6, width: 16 },
      { x: 42, height: 5, width: 14 },
      { x: 60, height: 4, width: 10 },
      { x: 72, height: 3, width: 8 }
    ];
    
    for (var i = 0; i < mountains.length; i++) {
      this.drawMountainToFrame(frame, mountains[i].x, this.horizonY - 1, 
                               mountains[i].height, mountains[i].width,
                               mountainAttr, highlightAttr);
    }
  }
  
  /**
   * Render skyscrapers to their frame (for city themes).
   */
  private renderSkyscrapers(): void {
    var frame = this.frameManager.getMountainsFrame();  // Reuse mountains frame slot
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var wallAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var windowAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var antennaAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    
    // Skyscraper silhouettes - varied heights and widths
    var buildings = [
      { x: 2, height: 5, width: 6 },
      { x: 10, height: 7, width: 4 },
      { x: 16, height: 4, width: 5 },
      { x: 23, height: 6, width: 7 },
      { x: 32, height: 8, width: 5 },
      { x: 39, height: 5, width: 6 },
      { x: 47, height: 7, width: 4 },
      { x: 53, height: 4, width: 5 },
      { x: 60, height: 6, width: 6 },
      { x: 68, height: 5, width: 5 },
      { x: 75, height: 4, width: 4 }
    ];
    
    for (var i = 0; i < buildings.length; i++) {
      this.drawBuildingToFrame(frame, buildings[i].x, this.horizonY - 1, 
                               buildings[i].height, buildings[i].width,
                               wallAttr, windowAttr, antennaAttr);
    }
  }
  
  /**
   * Draw a single building shape to a frame.
   */
  private drawBuildingToFrame(frame: Frame, baseX: number, baseY: number, 
                               height: number, width: number,
                               wallAttr: number, windowAttr: number, antennaAttr: number): void {
    // Draw building body
    for (var h = 0; h < height; h++) {
      var y = baseY - h;
      if (y < 0) continue;
      
      for (var dx = 0; dx < width; dx++) {
        var x = baseX + dx;
        if (x >= 0 && x < this.width) {
          // Windows pattern - checkerboard of lit/unlit
          var isWindow = (dx > 0 && dx < width - 1 && h > 0 && h < height - 1);
          var isLit = ((dx + h) % 3 === 0);
          
          if (isWindow && isLit) {
            frame.setData(x, y, '.', windowAttr);
          } else {
            frame.setData(x, y, GLYPH.FULL_BLOCK, wallAttr);
          }
        }
      }
    }
    
    // Antenna on some buildings
    if (width >= 5 && height >= 5) {
      var antennaX = baseX + Math.floor(width / 2);
      var antennaY = baseY - height;
      if (antennaY >= 0) {
        frame.setData(antennaX, antennaY, '|', antennaAttr);
        frame.setData(antennaX, antennaY - 1, '*', antennaAttr);
      }
    }
  }
  
  /**
   * Render distant islands for ocean background (static).
   */
  private renderOceanIslands(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var islandAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var highlightAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    
    // Distant island silhouettes (small, far away)
    var islands = [
      { x: 15, height: 2, width: 8 },   // Small distant island
      { x: 55, height: 3, width: 12 },  // Medium island
    ];
    
    for (var i = 0; i < islands.length; i++) {
      this.drawMountainToFrame(frame, islands[i].x, this.horizonY - 1, 
                               islands[i].height, islands[i].width,
                               islandAttr, highlightAttr);
    }
  }
  
  /**
   * Render forest treeline silhouette at horizon.
   * Creates a dense forest of varied tree shapes.
   */
  private renderForestTreeline(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var treeAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var topAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var trunkAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    
    // Dense forest - many overlapping trees
    // Use various tree shapes: pointed conifers, rounded deciduous
    var treeChars = ['^', GLYPH.TRIANGLE_UP, 'A', '*'];
    
    // Fill the horizon with trees at varying heights
    for (var x = 0; x < this.width; x++) {
      // Deterministic "random" height based on position
      var hash = (x * 17 + 5) % 13;
      var treeHeight = 2 + (hash % 4);  // 2-5 rows tall
      var treeType = hash % treeChars.length;
      
      // Draw tree column
      for (var h = 0; h < treeHeight; h++) {
        var y = this.horizonY - 1 - h;
        if (y < 0) continue;
        
        if (h === treeHeight - 1) {
          // Tree top - pointed
          frame.setData(x, y, treeChars[treeType], topAttr);
        } else if (h === 0 && treeHeight >= 3) {
          // Tree trunk base (only for taller trees)
          frame.setData(x, y, '|', trunkAttr);
        } else {
          // Tree body
          var bodyChar = (h === treeHeight - 2) ? GLYPH.TRIANGLE_UP : GLYPH.MEDIUM_SHADE;
          frame.setData(x, y, bodyChar, treeAttr);
        }
      }
      
      // Add some gaps/variety - skip some positions
      if ((x * 7) % 11 === 0) {
        x++;  // Create small gaps
      }
    }
  }
  
  /**
   * Render animated ocean waves at the horizon line.
   * Called every frame to animate the waves.
   */
  renderOceanWaves(trackPosition: number): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var waveAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    var foamAttr = makeAttr(WHITE, BG_BLACK);
    
    // Wave animation phase based on track position
    var wavePhase = Math.floor(trackPosition / 8) % 4;
    
    // Render 2 rows of waves at horizon
    for (var row = 0; row < 2; row++) {
      var y = this.horizonY - row;
      if (y < 0) continue;
      
      for (var x = 0; x < this.width; x++) {
        // Skip where islands are drawn (roughly)
        if ((x >= 15 && x <= 23) || (x >= 55 && x <= 67)) {
          if (row === 0) continue;  // Don't overwrite bottom of islands
        }
        
        // Wave pattern - multiple overlapping sine waves
        var wave1 = Math.sin((x + wavePhase * 3) * 0.3);
        var wave2 = Math.sin((x - wavePhase * 2 + 10) * 0.5);
        var combined = wave1 + wave2 * 0.5;
        
        // Choose character based on wave height and phase
        var char: string;
        var attr: number;
        
        if (combined > 0.8) {
          // Wave crest / foam
          char = (wavePhase % 2 === 0) ? '~' : '^';
          attr = foamAttr;
        } else if (combined > 0.2) {
          // Rising wave
          char = '~';
          attr = waveAttr;
        } else if (combined > -0.3) {
          // Flat water
          char = (row === 0) ? '-' : '~';
          attr = waveAttr;
        } else {
          // Wave trough
          char = '_';
          attr = waveAttr;
        }
        
        // Add some randomness for sparkle effect
        var sparkle = ((x * 17 + wavePhase * 31) % 23) === 0;
        if (sparkle && row === 0) {
          char = '*';
          attr = foamAttr;
        }
        
        frame.setData(x, y, char, attr);
      }
    }
  }
  
  /**
   * Render dense jungle canopy - layered foliage with hanging vines.
   * Creates depth through multiple overlapping leaf layers.
   */
  private renderJungleCanopy(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var leafDark = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var leafLight = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var vineAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    
    var canopyHeight = Math.min(this.horizonY - 1, 7);
    
    // Layer 1: Dense canopy top (furthest back)
    for (var x = 0; x < this.width; x++) {
      var hash = (x * 23 + 7) % 17;
      var topY = this.horizonY - canopyHeight + (hash % 3);
      
      // Bumpy canopy top
      for (var y = topY; y < this.horizonY; y++) {
        var depth = y - topY;
        var leafChar: string;
        
        if (depth === 0) {
          // Canopy crown - rounded shapes
          leafChar = ((x + hash) % 3 === 0) ? '@' : ((x % 2 === 0) ? 'O' : 'o');
          frame.setData(x, y, leafChar, leafLight);
        } else if (depth < 2) {
          // Upper canopy - dense
          leafChar = ((x + depth) % 2 === 0) ? GLYPH.MEDIUM_SHADE : GLYPH.DARK_SHADE;
          frame.setData(x, y, leafChar, leafDark);
        } else {
          // Lower canopy - with gaps
          if ((x * 13 + depth * 7) % 5 !== 0) {
            leafChar = GLYPH.LIGHT_SHADE;
            frame.setData(x, y, leafChar, leafDark);
          }
        }
      }
    }
    
    // Layer 2: Hanging vines
    for (var vineX = 3; vineX < this.width - 3; vineX += 7 + ((vineX * 11) % 5)) {
      var vineLength = 2 + ((vineX * 7) % 4);
      var vineStartY = this.horizonY - canopyHeight + 2;
      
      for (var vy = 0; vy < vineLength && vineStartY + vy < this.horizonY; vy++) {
        var vineChar = (vy === vineLength - 1) ? ')' : '|';
        frame.setData(vineX, vineStartY + vy, vineChar, vineAttr);
      }
    }
    
    // Layer 3: Occasional flower highlights
    for (var fx = 5; fx < this.width - 5; fx += 11 + ((fx * 3) % 7)) {
      var fy = this.horizonY - canopyHeight + 1 + ((fx * 5) % 2);
      if (fy < this.horizonY - 1) {
        frame.setData(fx, fy, '*', makeAttr(LIGHTMAGENTA, BG_BLACK));
      }
    }
  }
  
  /**
   * Render whimsical candy hills - rounded, colorful, swirly.
   * Soft curves instead of sharp mountain peaks.
   */
  private renderCandyHills(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var hill1 = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var hill2 = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var sparkle = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    
    // Candy hills - rounded sine wave shapes
    var hills = [
      { centerX: 12, height: 4, width: 20, color: hill1 },
      { centerX: 35, height: 5, width: 24, color: hill2 },
      { centerX: 58, height: 3, width: 18, color: hill1 },
      { centerX: 75, height: 4, width: 16, color: hill2 }
    ];
    
    for (var i = 0; i < hills.length; i++) {
      var hill = hills[i];
      
      for (var dx = -hill.width / 2; dx <= hill.width / 2; dx++) {
        var x = Math.floor(hill.centerX + dx);
        if (x < 0 || x >= this.width) continue;
        
        // Sine wave height - smooth rounded top
        var t = dx / (hill.width / 2);
        var hillHeight = Math.round(hill.height * Math.cos(t * Math.PI / 2));
        
        for (var h = 0; h < hillHeight; h++) {
          var y = this.horizonY - 1 - h;
          if (y < 0) continue;
          
          var char: string;
          if (h === hillHeight - 1) {
            // Rounded top
            char = (Math.abs(dx) < hill.width / 4) ? '@' : 'o';
          } else {
            // Body with swirl pattern
            char = ((x + h) % 3 === 0) ? '~' : GLYPH.MEDIUM_SHADE;
          }
          frame.setData(x, y, char, hill.color);
        }
      }
    }
    
    // Add sparkles/sprinkles
    for (var sx = 2; sx < this.width - 2; sx += 5 + ((sx * 7) % 4)) {
      var sy = this.horizonY - 2 - ((sx * 3) % 3);
      if (sy > 0) {
        frame.setData(sx, sy, '*', sparkle);
      }
    }
  }
  
  /**
   * Render cosmic nebula clouds - swirling gas clouds with stars peeking through.
   */
  private renderNebula(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var nebula1 = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var nebula2 = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var starAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    
    // Nebula clouds - irregular blobs
    var clouds = [
      { x: 5, y: this.horizonY - 5, w: 15, h: 4 },
      { x: 25, y: this.horizonY - 7, w: 20, h: 5 },
      { x: 50, y: this.horizonY - 4, w: 12, h: 3 },
      { x: 65, y: this.horizonY - 6, w: 14, h: 4 }
    ];
    
    // Render nebula clouds with varying density
    for (var c = 0; c < clouds.length; c++) {
      var cloud = clouds[c];
      var cloudAttr = (c % 2 === 0) ? nebula1 : nebula2;
      
      for (var cy = 0; cy < cloud.h; cy++) {
        for (var cx = 0; cx < cloud.w; cx++) {
          var px = cloud.x + cx;
          var py = cloud.y + cy;
          if (px < 0 || px >= this.width || py < 0 || py >= this.horizonY) continue;
          
          // Cloud density varies - denser in center
          var distFromCenter = Math.abs(cx - cloud.w / 2) / (cloud.w / 2) + 
                               Math.abs(cy - cloud.h / 2) / (cloud.h / 2);
          var density = 1 - distFromCenter * 0.6;
          var hash = (px * 17 + py * 31) % 100;
          
          if (hash < density * 80) {
            var char: string;
            if (density > 0.7) {
              char = GLYPH.MEDIUM_SHADE;
            } else if (density > 0.4) {
              char = GLYPH.LIGHT_SHADE;
            } else {
              char = '.';
            }
            frame.setData(px, py, char, cloudAttr);
          }
        }
      }
    }
    
    // Bright stars peeking through
    for (var sx = 1; sx < this.width - 1; sx += 8 + ((sx * 5) % 6)) {
      var sy = ((sx * 13) % (this.horizonY - 2)) + 1;
      if (sy > 0 && sy < this.horizonY - 1) {
        frame.setData(sx, sy, '*', starAttr);
      }
    }
  }
  
  /**
   * Render castle fortress silhouette - towers, walls, battlements.
   */
  private renderCastleFortress(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var stone = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var window = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var torch = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    
    // Castle sections: towers and connecting walls
    var sections = [
      { type: 'tower', x: 5, h: 7, w: 5 },
      { type: 'wall', x: 10, h: 4, w: 12 },
      { type: 'tower', x: 22, h: 8, w: 6 },  // Main tower
      { type: 'wall', x: 28, h: 4, w: 15 },
      { type: 'tower', x: 43, h: 6, w: 5 },
      { type: 'gate', x: 48, h: 5, w: 8 },
      { type: 'tower', x: 56, h: 7, w: 5 },
      { type: 'wall', x: 61, h: 4, w: 10 },
      { type: 'tower', x: 71, h: 5, w: 5 }
    ];
    
    for (var i = 0; i < sections.length; i++) {
      var sec = sections[i];
      var baseY = this.horizonY - 1;
      
      if (sec.type === 'tower') {
        // Tower body
        for (var h = 0; h < sec.h; h++) {
          var y = baseY - h;
          if (y < 0) continue;
          
          for (var dx = 0; dx < sec.w; dx++) {
            var x = sec.x + dx;
            if (x >= this.width) continue;
            
            if (h === sec.h - 1) {
              // Crenellations at top
              frame.setData(x, y, (dx % 2 === 0) ? GLYPH.FULL_BLOCK : ' ', stone);
            } else if (h === sec.h - 2 && dx === Math.floor(sec.w / 2)) {
              // Window
              frame.setData(x, y, '.', window);
            } else if (h === 1 && dx === Math.floor(sec.w / 2)) {
              // Torch at base
              frame.setData(x, y, '*', torch);
            } else {
              frame.setData(x, y, GLYPH.FULL_BLOCK, stone);
            }
          }
        }
        // Pointed roof
        var roofX = sec.x + Math.floor(sec.w / 2);
        frame.setData(roofX, baseY - sec.h, GLYPH.TRIANGLE_UP, stone);
        
      } else if (sec.type === 'wall') {
        // Wall with battlements
        for (var h = 0; h < sec.h; h++) {
          var y = baseY - h;
          if (y < 0) continue;
          
          for (var dx = 0; dx < sec.w; dx++) {
            var x = sec.x + dx;
            if (x >= this.width) continue;
            
            if (h === sec.h - 1) {
              // Crenellations
              frame.setData(x, y, (dx % 3 === 0) ? GLYPH.FULL_BLOCK : ' ', stone);
            } else {
              frame.setData(x, y, GLYPH.MEDIUM_SHADE, stone);
            }
          }
        }
        
      } else if (sec.type === 'gate') {
        // Gate with arch
        for (var h = 0; h < sec.h; h++) {
          var y = baseY - h;
          if (y < 0) continue;
          
          for (var dx = 0; dx < sec.w; dx++) {
            var x = sec.x + dx;
            if (x >= this.width) continue;
            
            // Arch opening in center
            var inArch = (dx > 1 && dx < sec.w - 2 && h < sec.h - 2);
            if (inArch) {
              // Portcullis pattern
              frame.setData(x, y, (h % 2 === 0) ? '-' : '#', stone);
            } else {
              frame.setData(x, y, GLYPH.FULL_BLOCK, stone);
            }
          }
        }
      }
    }
  }
  
  /**
   * Render volcanic landscape - jagged mountains with lava glow and smoke.
   */
  private renderVolcanic(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var rock = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var lava = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var smoke = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
    
    // Volcanic peaks - jagged and irregular
    var peaks = [
      { x: 8, h: 5, w: 14, hasLava: false },
      { x: 28, h: 8, w: 20, hasLava: true },  // Main volcano
      { x: 55, h: 6, w: 16, hasLava: true },
      { x: 72, h: 4, w: 10, hasLava: false }
    ];
    
    for (var i = 0; i < peaks.length; i++) {
      var peak = peaks[i];
      var peakX = peak.x + Math.floor(peak.w / 2);
      
      // Draw jagged mountain
      for (var h = 0; h < peak.h; h++) {
        var y = this.horizonY - 1 - h;
        if (y < 0) continue;
        
        var rowWidth = Math.floor((peak.h - h) * peak.w / peak.h / 2);
        
        for (var dx = -rowWidth; dx <= rowWidth; dx++) {
          var x = peakX + dx;
          if (x < 0 || x >= this.width) continue;
          
          // Jagged edge effect
          var edgeDist = Math.abs(dx) - rowWidth + 1;
          if (edgeDist >= 0 && ((x * 7 + h * 3) % 3 === 0)) {
            continue;  // Skip for jagged effect
          }
          
          var char: string;
          if (h === peak.h - 1 && peak.hasLava) {
            // Crater rim with lava glow
            char = '^';
            frame.setData(x, y, char, lava);
          } else if (dx < 0) {
            char = '/';
            frame.setData(x, y, char, rock);
          } else if (dx > 0) {
            char = '\\';
            frame.setData(x, y, char, rock);
          } else {
            char = GLYPH.BOX_V;
            frame.setData(x, y, char, rock);
          }
        }
      }
      
      // Lava in crater
      if (peak.hasLava) {
        var craterY = this.horizonY - peak.h;
        if (craterY >= 0) {
          frame.setData(peakX - 1, craterY, '*', lava);
          frame.setData(peakX, craterY, GLYPH.FULL_BLOCK, lava);
          frame.setData(peakX + 1, craterY, '*', lava);
        }
        
        // Smoke above volcano
        for (var sy = 1; sy <= 2; sy++) {
          var smokeY = craterY - sy;
          if (smokeY >= 0) {
            frame.setData(peakX + (sy % 2), smokeY, '~', smoke);
            frame.setData(peakX - (sy % 2), smokeY, '~', smoke);
          }
        }
      }
    }
    
    // Lava pools at base
    for (var lx = 0; lx < this.width; lx += 12 + ((lx * 5) % 7)) {
      var ly = this.horizonY - 1;
      frame.setData(lx, ly, '~', lava);
      if (lx + 1 < this.width) frame.setData(lx + 1, ly, '*', lava);
    }
  }
  
  /**
   * Render Egyptian pyramids with sphinx and obelisks.
   */
  private renderPyramids(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var stone = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var gold = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    
    // Pyramids - perfect triangles
    var pyramids = [
      { x: 15, h: 6 },   // Smaller pyramid
      { x: 40, h: 8 },   // Great pyramid
      { x: 62, h: 5 }    // Distant pyramid
    ];
    
    for (var i = 0; i < pyramids.length; i++) {
      var pyr = pyramids[i];
      var baseY = this.horizonY - 1;
      
      for (var h = 0; h < pyr.h; h++) {
        var y = baseY - h;
        if (y < 0) continue;
        
        var halfWidth = pyr.h - h - 1;
        
        // Draw pyramid row
        for (var dx = -halfWidth; dx <= halfWidth; dx++) {
          var x = pyr.x + dx;
          if (x < 0 || x >= this.width) continue;
          
          var char: string;
          var attr = stone;
          
          if (h === pyr.h - 1) {
            // Gold capstone
            char = GLYPH.TRIANGLE_UP;
            attr = gold;
          } else if (dx === -halfWidth) {
            char = '/';
          } else if (dx === halfWidth) {
            char = '\\';
          } else {
            // Interior with brick pattern
            char = ((h + dx) % 4 === 0) ? '-' : GLYPH.LIGHT_SHADE;
          }
          
          frame.setData(x, y, char, attr);
        }
      }
    }
    
    // Sphinx silhouette (left side)
    var sphinxX = 2;
    var sphinxY = this.horizonY - 1;
    // Body
    frame.setData(sphinxX, sphinxY, GLYPH.MEDIUM_SHADE, stone);
    frame.setData(sphinxX + 1, sphinxY, GLYPH.MEDIUM_SHADE, stone);
    frame.setData(sphinxX + 2, sphinxY, GLYPH.MEDIUM_SHADE, stone);
    frame.setData(sphinxX + 3, sphinxY, '_', stone);
    // Head
    frame.setData(sphinxX, sphinxY - 1, ')', stone);
    frame.setData(sphinxX + 1, sphinxY - 1, GLYPH.FULL_BLOCK, stone);
    frame.setData(sphinxX + 1, sphinxY - 2, GLYPH.TRIANGLE_UP, gold);  // Headdress
    
    // Obelisks
    var obeliskPositions = [28, 52, 75];
    for (var oi = 0; oi < obeliskPositions.length; oi++) {
      var ox = obeliskPositions[oi];
      frame.setData(ox, this.horizonY - 1, '|', stone);
      frame.setData(ox, this.horizonY - 2, '|', stone);
      frame.setData(ox, this.horizonY - 3, GLYPH.TRIANGLE_UP, gold);
    }
  }
  
  /**
   * Render sand dunes - smooth rolling hills for desert.
   */
  private renderDunes(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var sandLight = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var sandDark = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    
    // Rolling dunes using sine waves
    for (var x = 0; x < this.width; x++) {
      // Multiple overlapping sine waves for natural dune shapes
      var wave1 = Math.sin(x * 0.08) * 2;
      var wave2 = Math.sin(x * 0.15 + 1) * 1.5;
      var wave3 = Math.sin(x * 0.04 + 2) * 2.5;
      var duneHeight = Math.round(2 + wave1 + wave2 + wave3);
      
      for (var h = 0; h < duneHeight; h++) {
        var y = this.horizonY - 1 - h;
        if (y < 0) continue;
        
        // Alternate shading for depth
        var char: string;
        var attr = (h % 2 === 0) ? sandLight : sandDark;
        
        if (h === duneHeight - 1) {
          // Dune crest
          char = '~';
        } else {
          char = GLYPH.LIGHT_SHADE;
        }
        
        frame.setData(x, y, char, attr);
      }
    }
  }

  /**
   * Render motorsport stadium background with grandstands, lights, scoreboards.
   */
  private renderStadium(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    var colors = this.activeTheme.colors;
    var structure = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
    var lights = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
    var crowd1 = makeAttr(LIGHTRED, BG_BLACK);
    var crowd2 = makeAttr(YELLOW, BG_BLACK);
    var crowd3 = makeAttr(LIGHTCYAN, BG_BLACK);
    var crowdColors = [crowd1, crowd2, crowd3];
    
    // Left grandstand
    this.drawGrandstand(frame, 0, 18, structure, crowdColors);
    
    // Right grandstand  
    this.drawGrandstand(frame, 62, 18, structure, crowdColors);
    
    // Center scoreboard structure
    this.drawScoreboard(frame, 30, lights, structure);
    
    // Floodlight towers
    this.drawFloodlight(frame, 20, lights, structure);
    this.drawFloodlight(frame, 59, lights, structure);
    
    // Light beams from floodlights (rays going down)
    for (var beamX = 22; beamX < 28; beamX++) {
      for (var beamY = 4; beamY < this.horizonY - 2; beamY += 3) {
        frame.setData(beamX, beamY, '|', makeAttr(YELLOW, BG_BLACK));
      }
    }
    for (var beamX2 = 52; beamX2 < 58; beamX2++) {
      for (var beamY2 = 4; beamY2 < this.horizonY - 2; beamY2 += 3) {
        frame.setData(beamX2, beamY2, '|', makeAttr(YELLOW, BG_BLACK));
      }
    }
  }

  /**
   * Draw a grandstand section with animated crowd.
   */
  private drawGrandstand(frame: Frame, startX: number, width: number, 
                          structure: number, crowdColors: number[]): void {
    var standHeight = 6;
    
    for (var tier = 0; tier < standHeight; tier++) {
      var y = this.horizonY - 1 - tier;
      if (y < 0) continue;
      
      var tierWidth = width - tier * 2;
      var tierStart = startX + tier;
      
      for (var x = tierStart; x < tierStart + tierWidth && x < this.width; x++) {
        if (x < 0) continue;
        
        if (tier === 0) {
          // Safety barrier at bottom
          frame.setData(x, y, '=', structure);
        } else if (x === tierStart || x === tierStart + tierWidth - 1) {
          // Structural supports
          frame.setData(x, y, GLYPH.FULL_BLOCK, structure);
        } else {
          // Crowd - mix of colors
          var colorIdx = (x * 3 + tier * 7) % crowdColors.length;
          var crowdChars = ['o', 'O', '@', 'o', '*'];
          var charIdx = (x * 5 + tier * 11) % crowdChars.length;
          frame.setData(x, y, crowdChars[charIdx], crowdColors[colorIdx]);
        }
      }
    }
    
    // Roof over grandstand
    var roofY = this.horizonY - 1 - standHeight;
    if (roofY >= 0) {
      for (var rx = startX + standHeight; rx < startX + width - standHeight && rx < this.width; rx++) {
        if (rx >= 0) {
          frame.setData(rx, roofY, '_', structure);
        }
      }
    }
  }

  /**
   * Draw a jumbotron/scoreboard structure.
   */
  private drawScoreboard(frame: Frame, centerX: number, lights: number, structure: number): void {
    var boardWidth = 20;
    var boardHeight = 4;
    var boardTop = 1;
    var startX = centerX - Math.floor(boardWidth / 2);
    
    // Screen frame
    for (var y = boardTop; y < boardTop + boardHeight; y++) {
      for (var x = startX; x < startX + boardWidth; x++) {
        if (x < 0 || x >= this.width) continue;
        
        if (y === boardTop || y === boardTop + boardHeight - 1) {
          // Top/bottom border
          frame.setData(x, y, '-', structure);
        } else if (x === startX || x === startX + boardWidth - 1) {
          // Side borders
          frame.setData(x, y, '|', structure);
        } else {
          // Screen content - flickering display
          var displayChars = [GLYPH.FULL_BLOCK, GLYPH.DARK_SHADE, GLYPH.MEDIUM_SHADE];
          var charIdx = (x + y * 3) % displayChars.length;
          frame.setData(x, y, displayChars[charIdx], lights);
        }
      }
    }
    
    // Support poles
    var poleY1 = boardTop + boardHeight;
    var poleY2 = this.horizonY - 1;
    for (var py = poleY1; py < poleY2; py++) {
      frame.setData(startX + 3, py, '|', structure);
      frame.setData(startX + boardWidth - 4, py, '|', structure);
    }
  }

  /**
   * Draw a floodlight tower.
   */
  private drawFloodlight(frame: Frame, centerX: number, lights: number, structure: number): void {
    // Tower pole
    for (var y = 0; y < this.horizonY - 1; y++) {
      frame.setData(centerX, y, '|', structure);
    }
    
    // Light bank at top
    var lightBank = ['[', '*', '*', '*', ']'];
    for (var i = 0; i < lightBank.length; i++) {
      var lx = centerX - 2 + i;
      if (lx >= 0 && lx < this.width) {
        frame.setData(lx, 0, lightBank[i], lights);
      }
    }
    
    // Second light row
    for (var i2 = 0; i2 < 3; i2++) {
      var lx2 = centerX - 1 + i2;
      if (lx2 >= 0 && lx2 < this.width) {
        frame.setData(lx2, 1, '*', lights);
      }
    }
  }

  /**
   * Render destroyed city background for Kaiju Rampage theme.
   * Buildings on edges only - CENTER is clear for the giant monster.
   */
  private renderDestroyedCity(): void {
    var frame = this.frameManager.getMountainsFrame();
    if (!frame) return;
    
    // Simple color palette
    var fire = makeAttr(LIGHTRED, BG_BLACK);
    var fireGlow = makeAttr(YELLOW, BG_BLACK);
    var heli = makeAttr(GREEN, BG_BLACK);
    var tracer = makeAttr(YELLOW, BG_BLACK);
    
    // === MILITARY attacking from the right ===
    
    // Helicopter on far right
    this.drawSimpleHelicopter(frame, 70, 3, heli);
    // Tracer fire going left toward monster
    frame.setData(67, 4, '-', tracer);
    frame.setData(65, 4, '-', tracer);
    frame.setData(63, 4, '-', tracer);
    frame.setData(61, 5, '*', fire);
    
    // Second helicopter
    this.drawSimpleHelicopter(frame, 75, 6, heli);
    
    // === BUILDINGS ON FAR LEFT (behind monster's tail area) ===
    this.drawSimpleBuilding(frame, 2, 6, 3, true);
    this.drawSimpleBuilding(frame, 7, 8, 3, false);
    this.drawSimpleBuilding(frame, 12, 5, 3, true);
    
    // === BUILDINGS ON FAR RIGHT ===
    this.drawSimpleBuilding(frame, 62, 7, 3, true);
    this.drawSimpleBuilding(frame, 68, 9, 4, true);
    // Fire on this building
    frame.setData(70, this.horizonY - 10, '^', fireGlow);
    frame.setData(70, this.horizonY - 11, '*', fire);
    
    this.drawSimpleBuilding(frame, 74, 6, 3, false);
    
    // === GROUND FIRES on edges ===
    frame.setData(5, this.horizonY - 1, '^', fireGlow);
    frame.setData(5, this.horizonY - 2, '*', fire);
    
    frame.setData(72, this.horizonY - 1, '^', fireGlow);
    frame.setData(72, this.horizonY - 2, '*', fire);
  }

  /**
   * Draw a simple, clean helicopter shape.
   */
  private drawSimpleHelicopter(frame: Frame, x: number, y: number, attr: number): void {
    // Rotor
    frame.setData(x - 1, y - 1, '-', attr);
    frame.setData(x, y - 1, '+', attr);
    frame.setData(x + 1, y - 1, '-', attr);
    // Body
    frame.setData(x - 1, y, '<', attr);
    frame.setData(x, y, GLYPH.FULL_BLOCK, attr);
    frame.setData(x + 1, y, GLYPH.FULL_BLOCK, attr);
    frame.setData(x + 2, y, '-', attr);
    frame.setData(x + 3, y, '>', attr);
  }

  /**
   * Draw a simple building silhouette.
   */
  private drawSimpleBuilding(frame: Frame, x: number, height: number, width: number, damaged: boolean): void {
    var building = makeAttr(DARKGRAY, BG_BLACK);
    var window = makeAttr(BLACK, BG_BLACK);
    var baseY = this.horizonY - 1;
    
    for (var h = 0; h < height; h++) {
      var y = baseY - h;
      if (y < 0) continue;
      
      // Damaged buildings have jagged tops
      if (damaged && h >= height - 1) {
        // Only draw some blocks at top
        for (var w = 0; w < width; w++) {
          if ((x + w) % 2 === 0) {
            frame.setData(x + w, y, GLYPH.DARK_SHADE, building);
          }
        }
      } else {
        // Normal building row
        for (var w = 0; w < width; w++) {
          // Windows every other row
          if (h % 2 === 1 && w > 0 && w < width - 1) {
            frame.setData(x + w, y, GLYPH.MEDIUM_SHADE, window);
          } else {
            frame.setData(x + w, y, GLYPH.FULL_BLOCK, building);
          }
        }
      }
    }
  }

  /**
   * Draw a single mountain shape to a frame.
   */
  private drawMountainToFrame(frame: Frame, baseX: number, baseY: number, 
                               height: number, width: number,
                               attr: number, highlightAttr: number): void {
    var peakX = baseX + Math.floor(width / 2);
    
    for (var h = 0; h < height; h++) {
      var y = baseY - h;
      if (y < 0) continue;
      
      var halfWidth = Math.floor((height - h) * width / height / 2);
      
      // Draw mountain row
      for (var dx = -halfWidth; dx <= halfWidth; dx++) {
        var x = peakX + dx;
        if (x >= 0 && x < this.width) {
          if (dx < 0) {
            frame.setData(x, y, '/', attr);
          } else if (dx > 0) {
            frame.setData(x, y, '\\', attr);
          } else {
            // Peak
            if (h === height - 1) {
              frame.setData(x, y, GLYPH.TRIANGLE_UP, highlightAttr);
            } else {
              frame.setData(x, y, GLYPH.BOX_V, attr);
            }
          }
        }
      }
    }
  }
  
  /**
   * Update sky grid animation (called each frame) - synthwave style.
   * Grid is always centered - does NOT parallax with steering.
   * Animation only runs when car is moving (speed > 0).
   */
  renderSkyGrid(speed: number, dt: number): void {
    var frame = this.frameManager.getSkyGridFrame();
    if (!frame) return;
    
    frame.clear();
    
    // Update animation phase ONLY when moving (with threshold to handle float imprecision)
    if (speed > 1) {
      // NEGATIVE = lines move TOWARD camera (UP on screen, away from horizon)
      // When driving forward, horizontal lines emerge from horizon and rush toward you
      this._skyGridAnimPhase -= speed * dt * 0.004;
      while (this._skyGridAnimPhase < 0) this._skyGridAnimPhase += 1;
      while (this._skyGridAnimPhase >= 1) this._skyGridAnimPhase -= 1;
    }
    
    var colors = this.activeTheme.colors;
    var gridAttr = makeAttr(colors.skyGrid.fg, colors.skyGrid.bg);
    var glowAttr = makeAttr(colors.skyGridGlow.fg, colors.skyGridGlow.bg);
    
    // Grid is ALWAYS centered at screen center - no parallax shift
    // This keeps it aligned with the road vanishing point
    var vanishX = 40;
    
    for (var y = this.horizonY - 1; y >= 1; y--) {
      var distFromHorizon = this.horizonY - y;
      var spread = distFromHorizon * 6;
      
      // Vertical converging lines - extend to screen edges
      if (this.activeTheme.sky.converging) {
        for (var offset = 0; offset <= 40; offset += 8) {
          if (offset <= spread) {
            if (offset === 0) {
              frame.setData(vanishX, y, GLYPH.BOX_V, gridAttr);
            } else {
              var leftX = vanishX - offset;
              var rightX = vanishX + offset;
              if (leftX >= 0 && leftX < this.width) frame.setData(leftX, y, '/', glowAttr);
              if (rightX >= 0 && rightX < this.width) frame.setData(rightX, y, '\\', glowAttr);
            }
          }
        }
      }
      
      // Horizontal lines (animated) - lines emerge from horizon and move toward camera
      if (this.activeTheme.sky.horizontal) {
        // Phase offset by distance: lines appear at horizon first, then move up
        var scanlinePhase = (this._skyGridAnimPhase + distFromHorizon * 0.25) % 1;
        if (scanlinePhase < 0) scanlinePhase += 1;
        
        if (scanlinePhase < 0.33) {
          var lineSpread = Math.min(spread, 39);
          for (var x = vanishX - lineSpread; x <= vanishX + lineSpread; x++) {
            if (x >= 0 && x < this.width) {
              frame.setData(x, y, GLYPH.BOX_H, glowAttr);
            }
          }
        }
      }
    }
  }
  
  /**
   * Render star field to sky background (for night themes).
   */
  renderSkyStars(trackPosition: number): void {
    var frame = this.frameManager.getSkyGridFrame();
    if (!frame) return;
    
    frame.clear();
    
    var colors = this.activeTheme.colors;
    var brightAttr = makeAttr(colors.starBright.fg, colors.starBright.bg);
    var dimAttr = makeAttr(colors.starDim.fg, colors.starDim.bg);
    
    var density = this.activeTheme.stars.density;
    var numStars = Math.floor(this.width * this.horizonY * density * 0.15);
    
    // Parallax offset for stars (very slow)
    var parallaxOffset = Math.round(this._mountainScrollOffset * 0.1);
    
    // Twinkle phase based on track position (if enabled)
    var twinklePhase = this.activeTheme.stars.twinkle ? Math.floor(trackPosition / 30) : 0;
    
    // Deterministic star positions
    for (var i = 0; i < numStars; i++) {
      var baseX = (i * 17 + 5) % this.width;
      var x = (baseX + parallaxOffset + this.width) % this.width;  // Wrap around
      var y = (i * 13 + 3) % (this.horizonY - 1);  // Keep in sky area
      
      // Twinkle: some stars change brightness
      var twinkleState = (i + twinklePhase) % 5;
      var isBright = (i % 3 === 0) ? (twinkleState !== 0) : (twinkleState === 0);
      var char = isBright ? '*' : '.';
      
      if (x >= 0 && x < this.width && y >= 0 && y < this.horizonY) {
        frame.setData(x, y, char, isBright ? brightAttr : dimAttr);
      }
    }
  }
  
  /**
   * Render warm gradient sky (for sunset themes).
   * Creates horizontal bands of color fading from top to horizon.
   */
  renderSkyGradient(trackPosition: number): void {
    var frame = this.frameManager.getSkyGridFrame();
    if (!frame) return;
    
    frame.clear();
    
    var colors = this.activeTheme.colors;
    var highAttr = makeAttr(colors.skyTop.fg, colors.skyTop.bg);
    var midAttr = makeAttr(colors.skyMid.fg, colors.skyMid.bg);
    var lowAttr = makeAttr(colors.skyHorizon.fg, colors.skyHorizon.bg);
    
    // Subtle cloud movement based on track position
    var cloudOffset = Math.floor(trackPosition / 50) % this.width;
    
    // Divide sky into three zones
    var topZone = Math.floor(this.horizonY * 0.35);
    var midZone = Math.floor(this.horizonY * 0.7);
    
    for (var y = 0; y < this.horizonY; y++) {
      var attr: number;
      var chars: string[];
      
      if (y < topZone) {
        // Top zone - deep color, sparse texture
        attr = highAttr;
        chars = [' ', ' ', ' ', '.', ' '];
      } else if (y < midZone) {
        // Middle zone - warm transition
        attr = midAttr;
        chars = [' ', '~', ' ', ' ', '-'];
      } else {
        // Low zone near horizon - bright warm
        attr = lowAttr;
        chars = ['~', '-', '~', ' ', '='];
      }
      
      for (var x = 0; x < this.width; x++) {
        // Create subtle cloud/haze pattern
        var hash = ((x + cloudOffset) * 31 + y * 17) % 37;
        var charIndex = hash % chars.length;
        var char = chars[charIndex];
        
        frame.setData(x, y, char, attr);
      }
    }
  }
  
  /**
   * Render underwater sky - wavy blue water with swimming fish and bubbles.
   * Creates the impression of being deep underwater looking up at the surface.
   */
  renderSkyWater(trackPosition: number, speed: number, dt: number): void {
    var frame = this.frameManager.getSkyGridFrame();
    if (!frame) return;
    
    frame.clear();
    
    // Update water animation phase
    if (speed > 1) {
      this._skyGridAnimPhase += dt * 2.0;  // Gentle wave speed
      while (this._skyGridAnimPhase >= 1) this._skyGridAnimPhase -= 1;
    }
    
    // Water colors - gradient from deep to surface
    var deepAttr = makeAttr(BLUE, BG_BLUE);
    var midAttr = makeAttr(CYAN, BG_BLUE);
    var surfaceAttr = makeAttr(LIGHTCYAN, BG_CYAN);
    var waveAttr = makeAttr(WHITE, BG_CYAN);
    var bubbleAttr = makeAttr(WHITE, BG_BLUE);
    var lightrayAttr = makeAttr(LIGHTCYAN, BG_BLUE);
    
    // Fish colors
    var fish1Attr = makeAttr(YELLOW, BG_BLUE);
    var fish2Attr = makeAttr(LIGHTRED, BG_BLUE);
    var fish3Attr = makeAttr(LIGHTGREEN, BG_BLUE);
    
    // Divide sky into depth zones
    var surfaceZone = 3;  // Top 3 rows = surface
    var lightZone = Math.floor(this.horizonY * 0.5);
    
    // Render water background with wavy texture
    for (var y = 0; y < this.horizonY; y++) {
      var attr: number;
      var baseChars: string[];
      
      if (y < surfaceZone) {
        // Surface zone - bright, wavy
        attr = surfaceAttr;
        baseChars = ['~', '~', GLYPH.UPPER_HALF, '~', GLYPH.LOWER_HALF, '~'];
      } else if (y < lightZone) {
        // Light zone - medium blue, some waves
        attr = midAttr;
        baseChars = ['~', ' ', GLYPH.LIGHT_SHADE, ' ', '~', ' '];
      } else {
        // Deep zone - dark, sparse
        attr = deepAttr;
        baseChars = [' ', ' ', GLYPH.LIGHT_SHADE, ' ', ' ', ' ', ' ', ' '];
      }
      
      // Create wave pattern that moves
      var waveOffset = Math.floor(this._skyGridAnimPhase * 20 + y * 0.5);
      
      for (var x = 0; x < this.width; x++) {
        var charIndex = (x + waveOffset) % baseChars.length;
        var char = baseChars[charIndex];
        var cellAttr = attr;
        
        // Add surface wave highlights
        if (y < surfaceZone) {
          var wavePhase = Math.sin((x + trackPosition * 0.1 + this._skyGridAnimPhase * 10) * 0.3);
          if (wavePhase > 0.7) {
            cellAttr = waveAttr;
            char = GLYPH.FULL_BLOCK;
          }
        }
        
        frame.setData(x, y, char, cellAttr);
      }
    }
    
    // Add light rays from surface (vertical shimmering)
    var numRays = 5;
    for (var i = 0; i < numRays; i++) {
      var rayX = 10 + i * 15;
      var rayWobble = Math.sin(this._skyGridAnimPhase * 6.28 + i * 2) * 2;
      var actualX = Math.floor(rayX + rayWobble);
      
      for (var ry = surfaceZone; ry < lightZone; ry++) {
        if (actualX >= 0 && actualX < this.width) {
          // Fade ray as it goes deeper
          var fade = 1 - (ry - surfaceZone) / (lightZone - surfaceZone);
          if (Math.random() < fade * 0.6) {
            frame.setData(actualX, ry, GLYPH.LIGHT_SHADE, lightrayAttr);
          }
        }
      }
    }
    
    // Add swimming fish
    var fishPhase = trackPosition * 0.02 + this._skyGridAnimPhase * 5;
    
    // Fish 1 - yellow, swimming right
    var fish1X = Math.floor((fishPhase * 30) % (this.width + 10)) - 5;
    var fish1Y = 5 + Math.floor(Math.sin(fishPhase * 2) * 2);
    if (fish1X >= 0 && fish1X < this.width - 3 && fish1Y >= 0 && fish1Y < this.horizonY) {
      frame.setData(fish1X, fish1Y, '<', fish1Attr);
      frame.setData(fish1X + 1, fish1Y, GLYPH.FULL_BLOCK, fish1Attr);
      frame.setData(fish1X + 2, fish1Y, '>', fish1Attr);
    }
    
    // Fish 2 - red, swimming left (opposite direction)
    var fish2X = this.width - Math.floor((fishPhase * 25) % (this.width + 8));
    var fish2Y = 8 + Math.floor(Math.sin(fishPhase * 1.5 + 1) * 2);
    if (fish2X >= 2 && fish2X < this.width && fish2Y >= 0 && fish2Y < this.horizonY) {
      frame.setData(fish2X, fish2Y, '<', fish2Attr);
      frame.setData(fish2X - 1, fish2Y, GLYPH.FULL_BLOCK, fish2Attr);
      frame.setData(fish2X - 2, fish2Y, '>', fish2Attr);
    }
    
    // Fish school - small fish in background
    for (var f = 0; f < 4; f++) {
      var schoolX = Math.floor(((fishPhase + f * 0.7) * 20) % this.width);
      var schoolY = 3 + f * 2;
      if (schoolY < this.horizonY - 2) {
        frame.setData(schoolX, schoolY, '<', fish3Attr);
        frame.setData((schoolX + 1) % this.width, schoolY, '>', fish3Attr);
      }
    }
    
    // Add rising bubbles
    var bubblePositions = [8, 22, 38, 55, 72];
    for (var i = 0; i < bubblePositions.length; i++) {
      var bubbleBaseX = bubblePositions[i];
      // Bubbles rise over time
      var bubbleY = this.horizonY - 2 - Math.floor((trackPosition * 0.5 + i * 7) % (this.horizonY - 2));
      var wobble = Math.sin(trackPosition * 0.1 + i * 3) * 1.5;
      var bx = Math.floor(bubbleBaseX + wobble);
      
      if (bubbleY >= 0 && bubbleY < this.horizonY && bx >= 0 && bx < this.width) {
        frame.setData(bx, bubbleY, 'o', bubbleAttr);
        // Smaller bubble trailing
        if (bubbleY + 2 < this.horizonY) {
          frame.setData(bx, bubbleY + 2, '.', bubbleAttr);
        }
      }
    }
  }
  
  /**
   * Update parallax scroll based on steering/curvature.
   * Classic Super Scaler: backgrounds scroll horizontally when turning.
   */
  updateParallax(curvature: number, steer: number, speed: number, dt: number): void {
    // Accumulate horizontal scroll based on curve and steering
    var scrollAmount = (curvature * 0.8 + steer * 0.3) * speed * dt * 0.15;
    this._mountainScrollOffset += scrollAmount;
    
    // Clamp to reasonable range (will wrap in rendering)
    if (this._mountainScrollOffset > 80) this._mountainScrollOffset -= 80;
    if (this._mountainScrollOffset < -80) this._mountainScrollOffset += 80;
  }
  
  /**
   * Render the holodeck grid floor - mirrors the sky grid logic.
   * Ground grid is essentially a reflection of sky grid below the horizon.
   */
  private renderHolodeckFloor(trackPosition: number): void {
    var frame = this.frameManager.getGroundGridFrame();
    if (!frame) return;
    
    var ground = this.activeTheme.ground;
    if (!ground) return;
    
    frame.clear();
    
    var primaryAttr = makeAttr(ground.primary.fg, ground.primary.bg);
    var frameHeight = this.height - this.horizonY;
    var vanishX = Math.floor(this.width / 2);
    var radialSpacing = 6;  // Pixels between radial lines
    
    // Mirror sky grid logic - iterate from horizon downward
    for (var y = 0; y < frameHeight - 1; y++) {
      var distFromHorizon = y + 1;  // 1 at top row, increases going down
      var spread = distFromHorizon * 6;  // How far radials have spread at this row
      
      // === VERTICAL/DIAGONAL CONVERGING LINES ===
      // Draw radials that have emerged AND extend to edges
      // A radial at offset N emerges when spread >= N
      // Once emerged, draw it at that row
      
      // Center line
      frame.setData(vanishX, y, GLYPH.BOX_V, primaryAttr);
      
      // Side radials - draw every radial that has emerged by this row
      for (var offset = radialSpacing; offset <= spread; offset += radialSpacing) {
        var leftX = vanishX - offset;
        var rightX = vanishX + offset;
        if (leftX >= 0) {
          frame.setData(leftX, y, '/', primaryAttr);
        }
        if (rightX < this.width) {
          frame.setData(rightX, y, '\\', primaryAttr);
        }
      }
      
      // === HORIZONTAL LINES ===
      // Animate with trackPosition, span FULL WIDTH
      var linePhase = Math.floor(trackPosition / 40 + distFromHorizon * 1.5) % 5;
      if (linePhase === 0) {
        // Draw horizontal line across entire screen width
        for (var x = 0; x < this.width; x++) {
          // Check if we're on a radial line for intersection
          var distFromVanish = Math.abs(x - vanishX);
          var isOnRadial = (distFromVanish === 0) || (distFromVanish <= spread && (distFromVanish % radialSpacing) === 0);
          frame.setData(x, y, isOnRadial ? '+' : GLYPH.BOX_H, primaryAttr);
        }
      }
    }
  }
  
  /**
   * Render flowing lava ground - dynamic fire and lava animation.
   * Creates a hellscape of burning flames, glowing lava pools, and molten rivers.
   * Animates continuously even when the vehicle is stopped.
   */
  private renderLavaGround(_trackPosition: number): void {
    var frame = this.frameManager.getGroundGridFrame();
    if (!frame) return;
    
    var ground = this.activeTheme.ground;
    if (!ground) return;
    
    frame.clear();
    
    var frameHeight = this.height - this.horizonY;
    var firePhase = this._fireAnimPhase;
    
    // Color palette for fire/lava effects
    var blackAttr = makeAttr(BLACK, BG_BLACK);
    var darkRockAttr = makeAttr(DARKGRAY, BG_BLACK);
    var hotRockAttr = makeAttr(RED, BG_BLACK);
    var lavaAttr = makeAttr(LIGHTRED, BG_RED);
    var lavaGlowAttr = makeAttr(YELLOW, BG_RED);
    var lavaBrightAttr = makeAttr(WHITE, BG_RED);
    var fireAttr = makeAttr(YELLOW, BG_BLACK);
    var fireBrightAttr = makeAttr(WHITE, BG_RED);
    var emberAttr = makeAttr(LIGHTRED, BG_BLACK);
    
    // Process each cell in the ground area
    for (var y = 0; y < frameHeight - 1; y++) {
      // Distance from camera (0 = horizon, increases toward bottom)
      var depthFactor = y / frameHeight;  // 0.0 to 1.0
      
      for (var x = 0; x < this.width; x++) {
        // === LAVA RIVERS (wavy flowing patterns) ===
        // Multiple rivers with different phases
        var river1 = Math.sin((x * 0.15) + (y * 0.3) + firePhase * 0.7) * 1.5;
        var river2 = Math.sin((x * 0.1) - (y * 0.2) + firePhase * 0.5 + 2) * 1.2;
        var riverIntensity = river1 + river2;
        
        // === FIRE/FLAME PILLARS (vertical rising flames) ===
        // Flames rise upward (decreasing y), so animate against y
        var flameBase = Math.sin(x * 0.25 + firePhase * 1.5) + 
                        Math.sin(x * 0.4 - firePhase * 2.0) * 0.5;
        var flameFlicker = Math.sin(x * 0.8 + y * 0.5 + firePhase * 4) * 0.5;
        var flameIntensity = flameBase + flameFlicker;
        
        // === EMBER SPARKLES (random twinkling embers) ===
        var emberSeed = (x * 7919 + y * 104729) % 1000;
        var emberPhase = Math.sin(firePhase * 3 + emberSeed * 0.01);
        var isEmber = emberSeed < 30 && emberPhase > 0.7;
        
        // === LAVA POOLS (large glowing areas) ===
        var poolCenterX1 = 15 + Math.sin(firePhase * 0.3) * 3;
        var poolCenterX2 = 55 + Math.sin(firePhase * 0.4 + 1) * 4;
        var poolCenterX3 = 35 + Math.sin(firePhase * 0.2 + 2) * 2;
        
        var dist1 = Math.sqrt(Math.pow(x - poolCenterX1, 2) + Math.pow(y - 8, 2));
        var dist2 = Math.sqrt(Math.pow(x - poolCenterX2, 2) + Math.pow(y - 12, 2));
        var dist3 = Math.sqrt(Math.pow(x - poolCenterX3, 2) + Math.pow(y - 5, 2));
        
        var poolBubble = Math.sin(firePhase * 2 + x * 0.3 + y * 0.2);
        var inPool1 = dist1 < 6 + poolBubble;
        var inPool2 = dist2 < 5 + poolBubble * 0.8;
        var inPool3 = dist3 < 4 + poolBubble * 0.6;
        
        // === DETERMINE CELL APPEARANCE ===
        var char: string;
        var attr: number;
        
        if (isEmber && depthFactor > 0.3) {
          // Floating ember sparkle
          char = '*';
          attr = emberAttr;
        } else if (inPool1 || inPool2 || inPool3) {
          // Inside a lava pool - bubbling molten rock
          var poolDist = inPool1 ? dist1 : (inPool2 ? dist2 : dist3);
          var bubblePhase = Math.sin(firePhase * 3 + poolDist * 0.5);
          
          if (bubblePhase > 0.7) {
            char = 'O';  // Bubble
            attr = lavaBrightAttr;
          } else if (bubblePhase > 0.3) {
            char = GLYPH.FULL_BLOCK;
            attr = lavaGlowAttr;
          } else if (bubblePhase > -0.2) {
            char = GLYPH.MEDIUM_SHADE;
            attr = lavaAttr;
          } else {
            char = '~';
            attr = lavaAttr;
          }
        } else if (riverIntensity > 1.5) {
          // Bright lava river center
          char = GLYPH.FULL_BLOCK;
          attr = lavaGlowAttr;
        } else if (riverIntensity > 0.8) {
          // Lava river edge
          char = '~';
          attr = lavaAttr;
        } else if (riverIntensity > 0.3) {
          // Hot rock near lava
          char = GLYPH.LIGHT_SHADE;
          attr = hotRockAttr;
        } else if (flameIntensity > 1.2 && depthFactor < 0.6) {
          // Bright flame tip
          char = '^';
          attr = fireBrightAttr;
        } else if (flameIntensity > 0.6 && depthFactor < 0.7) {
          // Flame body
          var flameChar = ((x + Math.floor(firePhase * 5)) % 3 === 0) ? '^' : '*';
          char = flameChar;
          attr = fireAttr;
        } else if (flameIntensity > 0.2 && depthFactor < 0.8) {
          // Flame glow
          char = GLYPH.MEDIUM_SHADE;
          attr = emberAttr;
        } else if (Math.random() < 0.02 && depthFactor > 0.4) {
          // Occasional ember on ground
          char = '.';
          attr = emberAttr;
        } else {
          // Dark scorched rock base
          var rockPattern = ((x * 3 + y * 7) % 5);
          if (rockPattern === 0) {
            char = GLYPH.DARK_SHADE;
            attr = darkRockAttr;
          } else if (rockPattern === 1) {
            char = GLYPH.MEDIUM_SHADE;
            attr = blackAttr;
          } else {
            char = ' ';
            attr = blackAttr;
          }
        }
        
        frame.setData(x, y, char, attr);
      }
    }
  }
  
  /**
   * Render animated underwater ground - flowing water, bubbles, sandy floor.
   * Creates a living ocean floor with caustics, ripples, and marine elements.
   * Animates continuously even when stopped.
   */
  private renderWaterGround(_trackPosition: number): void {
    var frame = this.frameManager.getGroundGridFrame();
    if (!frame) return;
    
    var ground = this.activeTheme.ground;
    if (!ground) return;
    
    frame.clear();
    
    var frameHeight = this.height - this.horizonY;
    var waterPhase = this._fireAnimPhase;  // Reuse the fire phase for continuous animation
    
    // Color palette for water effects
    var deepBlueAttr = makeAttr(BLUE, BG_BLUE);
    var mediumBlueAttr = makeAttr(LIGHTBLUE, BG_BLUE);
    var lightBlueAttr = makeAttr(LIGHTCYAN, BG_BLUE);
    var cyanAttr = makeAttr(CYAN, BG_CYAN);
    var whiteAttr = makeAttr(WHITE, BG_BLUE);
    var sandAttr = makeAttr(YELLOW, BG_BLUE);
    var sandDarkAttr = makeAttr(BROWN, BG_BLUE);
    var seaweedAttr = makeAttr(GREEN, BG_BLUE);
    var bubbleAttr = makeAttr(WHITE, BG_CYAN);
    
    // Process each cell
    for (var y = 0; y < frameHeight - 1; y++) {
      var depthFactor = y / frameHeight;  // 0.0 (horizon) to 1.0 (bottom)
      
      for (var x = 0; x < this.width; x++) {
        // === WATER CAUSTICS (rippling light patterns) ===
        var caustic1 = Math.sin((x * 0.2) + (y * 0.15) + waterPhase * 0.8);
        var caustic2 = Math.sin((x * 0.15) - (y * 0.1) + waterPhase * 0.6 + 1.5);
        var causticIntensity = (caustic1 + caustic2) / 2;
        
        // === WATER CURRENT (horizontal flow) ===
        var currentWave = Math.sin(y * 0.3 + waterPhase * 1.2) * 0.5;
        var currentIntensity = Math.sin((x + waterPhase * 3) * 0.1 + currentWave);
        
        // === BUBBLES (rising particles) ===
        var bubbleSeed = (x * 7919 + y * 104729) % 1000;
        var bubblePhase = Math.sin(waterPhase * 2 - y * 0.3 + bubbleSeed * 0.01);
        var isBubble = bubbleSeed < 20 && bubblePhase > 0.8 && depthFactor > 0.2;
        
        // === SAND PATCHES (bottom of ocean) ===
        var sandNoise = Math.sin(x * 0.4 + y * 0.2) + Math.sin(x * 0.2 - y * 0.3);
        var isSandy = sandNoise > 0.8 && depthFactor > 0.5;
        
        // === SEAWEED PATCHES ===
        var seaweedSeed = (x * 31 + y * 17) % 100;
        var seaweedWave = Math.sin(waterPhase * 1.5 + x * 0.5);
        var isSeaweed = seaweedSeed < 8 && depthFactor > 0.3 && seaweedWave > 0.3;
        
        // === DETERMINE CELL APPEARANCE ===
        var char: string;
        var attr: number;
        
        if (isBubble) {
          // Rising bubble
          char = 'o';
          attr = bubbleAttr;
        } else if (isSeaweed) {
          // Swaying seaweed
          var seaweedChar = seaweedWave > 0.6 ? ')' : '(';
          char = seaweedChar;
          attr = seaweedAttr;
        } else if (isSandy) {
          // Sandy ocean floor
          var sandRipple = Math.sin(x * 0.5 + waterPhase * 0.5);
          if (sandRipple > 0.5) {
            char = '~';
            attr = sandAttr;
          } else if (sandRipple > 0) {
            char = '.';
            attr = sandDarkAttr;
          } else {
            char = ',';
            attr = sandAttr;
          }
        } else if (causticIntensity > 0.7) {
          // Bright caustic (light beam through water)
          char = GLYPH.LIGHT_SHADE;
          attr = whiteAttr;
        } else if (causticIntensity > 0.3) {
          // Medium caustic
          char = '~';
          attr = lightBlueAttr;
        } else if (currentIntensity > 0.6) {
          // Water current highlight
          char = '~';
          attr = cyanAttr;
        } else if (currentIntensity > 0.2) {
          // Gentle current
          char = '~';
          attr = mediumBlueAttr;
        } else if (currentIntensity > -0.2) {
          // Calm water
          char = GLYPH.MEDIUM_SHADE;
          attr = mediumBlueAttr;
        } else {
          // Deep water
          char = GLYPH.DARK_SHADE;
          attr = deepBlueAttr;
        }
        
        frame.setData(x, y, char, attr);
      }
    }
  }
  
  /**
   * Render candy ground - colorful sprinkles and swirls.
   */
  private renderCandyGround(trackPosition: number): void {
    var frame = this.frameManager.getGroundGridFrame();
    if (!frame) return;
    
    var ground = this.activeTheme.ground;
    if (!ground) return;
    
    frame.clear();
    
    var candy1 = makeAttr(ground.primary.fg, ground.primary.bg);
    var candy2 = makeAttr(ground.secondary.fg, ground.secondary.bg);
    var frameHeight = this.height - this.horizonY;
    var sparklePhase = Math.floor(trackPosition / 30) % 6;
    
    // Base frosting pattern
    for (var y = 0; y < frameHeight - 1; y++) {
      for (var x = 0; x < this.width; x++) {
        // Swirl pattern
        var swirl = Math.sin(x * 0.2 + y * 0.3) + Math.cos(x * 0.15 - y * 0.25);
        var char: string;
        var attr: number;
        
        if (swirl > 0.5) {
          char = '@';
          attr = candy1;
        } else if (swirl > -0.5) {
          char = GLYPH.LIGHT_SHADE;
          attr = candy2;
        } else {
          char = '.';
          attr = candy1;
        }
        
        // Sprinkles
        var isSprinkle = ((x * 7 + y * 13 + sparklePhase) % 17) === 0;
        if (isSprinkle) {
          var sprinkleColors = [LIGHTRED, LIGHTGREEN, LIGHTCYAN, YELLOW, LIGHTMAGENTA];
          var colorIdx = (x + y) % sprinkleColors.length;
          char = '*';
          attr = makeAttr(sprinkleColors[colorIdx], BG_BLACK);
        }
        
        frame.setData(x, y, char, attr);
      }
    }
  }
  
  /**
   * Render void/space ground - stars and cosmic dust floating below.
   */
  private renderVoidGround(trackPosition: number): void {
    var frame = this.frameManager.getGroundGridFrame();
    if (!frame) return;
    
    var ground = this.activeTheme.ground;
    if (!ground) return;
    
    frame.clear();
    
    var voidAttr = makeAttr(ground.primary.fg, ground.primary.bg);
    var starAttr = makeAttr(ground.secondary.fg, ground.secondary.bg);
    var frameHeight = this.height - this.horizonY;
    var twinklePhase = Math.floor(trackPosition / 15) % 4;
    
    // Deep space with occasional stars and nebula wisps
    for (var y = 0; y < frameHeight - 1; y++) {
      for (var x = 0; x < this.width; x++) {
        var hash = (x * 31 + y * 17) % 100;
        
        if (hash < 3) {
          // Bright star
          var twinkle = ((x + y + twinklePhase) % 4) === 0;
          frame.setData(x, y, twinkle ? '*' : '+', starAttr);
        } else if (hash < 8) {
          // Dim star
          frame.setData(x, y, '.', voidAttr);
        } else if (hash < 15) {
          // Nebula wisp
          frame.setData(x, y, GLYPH.LIGHT_SHADE, voidAttr);
        }
        // Rest is empty black void
      }
    }
    
    // Rainbow road edge glow effect - converging colored lines
    var vanishX = Math.floor(this.width / 2);
    var colors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
    
    for (var y = 0; y < frameHeight - 1; y++) {
      var distFromHorizon = y + 1;
      
      // Colored guide lines
      for (var ci = 0; ci < colors.length; ci++) {
        var offset = (ci - 2.5) * 8;  // Spread around center
        var lineX = Math.floor(vanishX + offset * distFromHorizon / 5);
        
        if (lineX >= 0 && lineX < this.width) {
          var glowPhase = (trackPosition / 10 + ci) % colors.length;
          var colorIdx = Math.floor(glowPhase + ci) % colors.length;
          frame.setData(lineX, y, GLYPH.BOX_V, makeAttr(colors[colorIdx], BG_BLACK));
        }
      }
    }
  }
  
  /**
   * Render cobblestone ground - medieval stone pattern.
   */
  private renderCobblestoneGround(trackPosition: number): void {
    var frame = this.frameManager.getGroundGridFrame();
    if (!frame) return;
    
    var ground = this.activeTheme.ground;
    if (!ground) return;
    
    frame.clear();
    
    var stone = makeAttr(ground.primary.fg, ground.primary.bg);
    var mortar = makeAttr(ground.secondary.fg, ground.secondary.bg);
    var frameHeight = this.height - this.horizonY;
    
    // Cobblestone pattern - irregular but repeating
    for (var y = 0; y < frameHeight - 1; y++) {
      for (var x = 0; x < this.width; x++) {
        // Create stone/mortar pattern
        var stoneSize = 3;
        var inMortar = ((x % stoneSize) === 0) || (((y + Math.floor(x / stoneSize)) % 2 === 0) && ((y % stoneSize) === 0));
        
        if (inMortar) {
          frame.setData(x, y, '+', mortar);
        } else {
          // Vary stone texture
          var texture = ((x * 7 + y * 11) % 5);
          var char = (texture === 0) ? GLYPH.MEDIUM_SHADE : GLYPH.DARK_SHADE;
          frame.setData(x, y, char, stone);
        }
      }
    }
    
    // Occasional puddle reflections
    var puddlePhase = Math.floor(trackPosition / 50) % 10;
    for (var py = frameHeight / 2; py < frameHeight - 1; py += 4) {
      var px = (py * 13 + puddlePhase * 7) % this.width;
      frame.setData(px, py, '~', makeAttr(DARKGRAY, BG_BLACK));
    }
  }
  
  /**
   * Render jungle ground - dense vegetation and vines.
   */
  private renderJungleGround(trackPosition: number): void {
    var frame = this.frameManager.getGroundGridFrame();
    if (!frame) return;
    
    var ground = this.activeTheme.ground;
    if (!ground) return;
    
    frame.clear();
    
    var leaf = makeAttr(ground.primary.fg, ground.primary.bg);
    var dirt = makeAttr(ground.secondary.fg, ground.secondary.bg);
    var frameHeight = this.height - this.horizonY;
    var rustlePhase = Math.floor(trackPosition / 25) % 4;
    
    // Dense jungle floor
    for (var y = 0; y < frameHeight - 1; y++) {
      for (var x = 0; x < this.width; x++) {
        var hash = (x * 23 + y * 19) % 20;
        var char: string;
        var attr: number;
        
        if (hash < 8) {
          // Ferns and leaves
          var leafChars = ['"', 'v', 'V', 'Y', 'y'];
          char = leafChars[(x + y + rustlePhase) % leafChars.length];
          attr = leaf;
        } else if (hash < 12) {
          // Exposed dirt
          char = GLYPH.LIGHT_SHADE;
          attr = dirt;
        } else if (hash < 15) {
          // Fallen leaves
          char = ',';
          attr = makeAttr(BROWN, BG_BLACK);
        } else {
          // Grass/undergrowth
          char = GLYPH.MEDIUM_SHADE;
          attr = leaf;
        }
        
        frame.setData(x, y, char, attr);
      }
    }
    
    // Occasional mushrooms
    for (var mx = 5; mx < this.width - 5; mx += 15 + ((mx * 3) % 7)) {
      var my = (mx * 7) % (frameHeight - 2) + 1;
      frame.setData(mx, my, 'o', makeAttr(LIGHTRED, BG_BLACK));
    }
  }

  /**
   * Render dirt/packed earth ground for stadium/rally theme.
   */
  private renderDirtGround(trackPosition: number): void {
    var frame = this.frameManager.getGroundGridFrame();
    if (!frame) return;
    
    var ground = this.activeTheme.ground;
    if (!ground) return;
    
    frame.clear();
    
    var dirt = makeAttr(ground.primary.fg, ground.primary.bg);
    var dirtDark = makeAttr(ground.secondary.fg, ground.secondary.bg);
    var tireTrack = makeAttr(DARKGRAY, BG_BLACK);
    var frameHeight = this.height - this.horizonY;
    var dustPhase = Math.floor(trackPosition / 20) % 8;
    
    // Packed dirt terrain
    for (var y = 0; y < frameHeight - 1; y++) {
      for (var x = 0; x < this.width; x++) {
        var hash = (x * 37 + y * 23 + dustPhase) % 25;
        var char: string;
        var attr: number;
        
        // Tire tracks on sides
        var inTireTrack = (x >= 5 && x <= 12) || (x >= this.width - 12 && x <= this.width - 5);
        
        if (inTireTrack && y > 2 && ((y + dustPhase) % 3 === 0)) {
          // Tire tread marks
          char = '=';
          attr = tireTrack;
        } else if (hash < 8) {
          // Packed dirt
          char = GLYPH.MEDIUM_SHADE;
          attr = dirt;
        } else if (hash < 14) {
          // Darker packed earth
          char = GLYPH.DARK_SHADE;
          attr = dirtDark;
        } else if (hash < 17) {
          // Pebbles
          char = '.';
          attr = makeAttr(LIGHTGRAY, BG_BLACK);
        } else if (hash < 20) {
          // Dust/fine dirt
          char = GLYPH.LIGHT_SHADE;
          attr = dirt;
        } else {
          // Scattered dirt clumps
          var clumps = ['`', "'", ','];
          char = clumps[(x + y) % clumps.length];
          attr = dirtDark;
        }
        
        frame.setData(x, y, char, attr);
      }
    }
    
    // Occasional puddle/wet spots
    for (var px = 8; px < this.width - 8; px += 20 + ((px * 7) % 10)) {
      var py = (px * 5) % (frameHeight - 3) + 1;
      var pw = 3 + (px % 3);
      for (var pox = 0; pox < pw; pox++) {
        if (px + pox < this.width) {
          frame.setData(px + pox, py, '~', makeAttr(DARKGRAY, BG_BLACK));
        }
      }
    }
  }

  /**
   * Render just road stripes and center line for ANSI tunnel theme.
   * Unlike renderRoadSurface, this doesn't clear the frame or render the entire road.
   * @param roadHeight - The height of the ANSI road area (to avoid drawing into HUD)
   */
  private renderANSIRoadStripes(trackPosition: number, cameraX: number, road: Road, roadHeight?: number): void {
    var frame = this.frameManager.getRoadFrame();
    if (!frame) return;
    
    // DON'T clear the frame - ANSI content is already there!
    
    // Use provided roadHeight or calculate default
    var roadBottom = roadHeight ? roadHeight - 1 : this.height - this.horizonY - 4;
    var accumulatedCurve = 0;
    
    // Render just the lane stripes on top of ANSI
    for (var screenY = roadBottom; screenY >= 0; screenY--) {
      var t = (roadBottom - screenY) / Math.max(1, roadBottom);
      var distance = 1 / (1 - t * 0.95);
      var worldZ = trackPosition + distance * 5;
      var segment = road.getSegment(worldZ);
      
      if (segment) {
        accumulatedCurve += segment.curve * 0.5;
      }
      
      var roadWidth = Math.round(40 / distance);
      var halfWidth = Math.floor(roadWidth / 2);
      var curveOffset = accumulatedCurve * distance * 0.8;
      var centerX = 40 + Math.round(curveOffset) - Math.round(cameraX * 0.5);
      
      // Only render center stripe
      var stripePhase = Math.floor((trackPosition + distance * 5) / 15) % 2;
      if (stripePhase === 0 && halfWidth > 2) {
        // Yellow center stripe
        frame.setData(centerX, screenY, '-', makeAttr(YELLOW, BG_BLACK));
      }
      
      // Render edge lines (left and right)
      var leftEdge = centerX - halfWidth;
      var rightEdge = centerX + halfWidth;
      
      if (leftEdge >= 0 && leftEdge < this.width) {
        frame.setData(leftEdge, screenY, '|', makeAttr(WHITE, BG_BLACK));
      }
      if (rightEdge >= 0 && rightEdge < this.width) {
        frame.setData(rightEdge, screenY, '|', makeAttr(WHITE, BG_BLACK));
      }
    }
  }

  /**
   * Render the road surface to its frame (internal method).
   */
  private renderRoadSurface(trackPosition: number, cameraX: number, road: Road): void {
    var frame = this.frameManager.getRoadFrame();
    if (!frame) return;
    
    frame.clear();
    
    var roadBottom = this.height - this.horizonY - 1;  // Frame-relative Y
    var roadLength = road.totalLength;
    
    // Accumulate curvature for perspective curve effect
    var accumulatedCurve = 0;
    
    for (var screenY = roadBottom; screenY >= 0; screenY--) {
      var t = (roadBottom - screenY) / roadBottom;
      var distance = 1 / (1 - t * 0.95);
      
      // Get road segment at this distance
      var worldZ = trackPosition + distance * 5;
      var segment = road.getSegment(worldZ);
      
      // Accumulate curve - each segment's curve affects the road center
      // Further segments have more accumulated curvature
      if (segment) {
        accumulatedCurve += segment.curve * 0.5;
      }
      
      // Road width narrows with distance
      var roadWidth = Math.round(40 / distance);
      var halfWidth = Math.floor(roadWidth / 2);
      
      // Apply curve offset - road center shifts based on accumulated curvature
      var curveOffset = accumulatedCurve * distance * 0.8;
      var centerX = 40 + Math.round(curveOffset) - Math.round(cameraX * 0.5);
      
      var leftEdge = centerX - halfWidth;
      var rightEdge = centerX + halfWidth;
      
      // Stripe phase for animated dashes
      var stripePhase = Math.floor((trackPosition + distance * 5) / 15) % 2;
      
      // Check finish line
      var wrappedZ = worldZ % roadLength;
      if (wrappedZ < 0) wrappedZ += roadLength;
      var isFinishLine = (wrappedZ < 200) || (wrappedZ > roadLength - 200);
      
      this.renderRoadScanline(frame, screenY, centerX, leftEdge, rightEdge, 
                              distance, stripePhase, isFinishLine, accumulatedCurve, worldZ);
    }
  }
  
  /**
   * Render a single road scanline.
   */
  private renderRoadScanline(frame: Frame, y: number, centerX: number,
                              leftEdge: number, rightEdge: number,
                              distance: number, stripePhase: number,
                              isFinishLine: boolean, curve?: number, worldZ?: number): void {
    var colors = this.activeTheme.colors;
    
    // Get base colors
    var baseSurfaceFg = distance < 10 ? colors.roadSurfaceAlt.fg : colors.roadSurface.fg;
    var baseSurfaceBg = distance < 10 ? colors.roadSurfaceAlt.bg : colors.roadSurface.bg;
    var baseGridFg = colors.roadGrid.fg;
    var baseGridBg = colors.roadGrid.bg;
    var baseEdgeFg = colors.roadEdge.fg;
    var baseEdgeBg = colors.roadEdge.bg;
    var baseStripeFg = colors.roadStripe.fg;
    var baseStripeBg = colors.roadStripe.bg;
    var baseShoulderFg = colors.shoulderPrimary.fg;
    var baseShoulderBg = colors.shoulderPrimary.bg;
    
    // Apply rainbow road colors - cycles through spectrum based on WORLD position
    // worldZ makes the colors move as you drive!
    var isRainbowRoad = this.activeTheme.road && this.activeTheme.road.rainbow;
    if (isRainbowRoad) {
      var rainbowColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
      // Use worldZ (track position) so colors change as you drive
      var trackPos = worldZ || 0;
      var colorIndex = Math.floor(trackPos * 0.02) % rainbowColors.length;
      var nextColorIndex = (colorIndex + 1) % rainbowColors.length;
      baseSurfaceFg = rainbowColors[colorIndex];
      baseSurfaceBg = BG_BLACK;
      baseGridFg = rainbowColors[nextColorIndex];
      baseGridBg = BG_BLACK;
      baseEdgeFg = rainbowColors[colorIndex];
      baseEdgeBg = BG_BLACK;
      baseStripeFg = WHITE;
      baseStripeBg = BG_BLACK;
      // Extend rainbow to shoulders for seamless blend
      baseShoulderFg = rainbowColors[(colorIndex + 2) % rainbowColors.length];
      baseShoulderBg = BG_BLACK;
    }
    
    // Apply glitch color corruption if on glitch theme
    if (this.activeTheme.name === 'glitch_circuit' && typeof GlitchState !== 'undefined' && GlitchState.roadColorGlitch !== 0) {
      var surfaceGlitch = GlitchState.getGlitchedRoadColor(baseSurfaceFg, baseSurfaceBg, distance);
      baseSurfaceFg = surfaceGlitch.fg;
      baseSurfaceBg = surfaceGlitch.bg;
      
      var gridGlitch = GlitchState.getGlitchedRoadColor(baseGridFg, baseGridBg, distance);
      baseGridFg = gridGlitch.fg;
      baseGridBg = gridGlitch.bg;
      
      var edgeGlitch = GlitchState.getGlitchedRoadColor(baseEdgeFg, baseEdgeBg, distance);
      baseEdgeFg = edgeGlitch.fg;
      baseEdgeBg = edgeGlitch.bg;
      
      var stripeGlitch = GlitchState.getGlitchedRoadColor(baseStripeFg, baseStripeBg, distance);
      baseStripeFg = stripeGlitch.fg;
      baseStripeBg = stripeGlitch.bg;
      
      var shoulderGlitch = GlitchState.getGlitchedRoadColor(baseShoulderFg, baseShoulderBg, distance);
      baseShoulderFg = shoulderGlitch.fg;
      baseShoulderBg = shoulderGlitch.bg;
    }
    
    var roadAttr = makeAttr(baseSurfaceFg, baseSurfaceBg);
    var gridAttr = makeAttr(baseGridFg, baseGridBg);
    var edgeAttr = makeAttr(baseEdgeFg, baseEdgeBg);
    var stripeAttr = makeAttr(baseStripeFg, baseStripeBg);
    var shoulderAttr = makeAttr(baseShoulderFg, baseShoulderBg);
    var hideEdgeMarkers = this.activeTheme.road && this.activeTheme.road.hideEdgeMarkers;
    
    for (var x = 0; x < this.width; x++) {
      if (x >= leftEdge && x <= rightEdge) {
        // On road
        if (isFinishLine) {
          this.renderFinishCell(frame, x, y, centerX, leftEdge, rightEdge, distance);
        } else if ((x === leftEdge || x === rightEdge) && !hideEdgeMarkers) {
          frame.setData(x, y, GLYPH.BOX_V, edgeAttr);
        } else if (Math.abs(x - centerX) < 1 && stripePhase === 0) {
          frame.setData(x, y, GLYPH.BOX_V, stripeAttr);
        } else {
          var gridPhase = Math.floor(distance) % 3;
          if (gridPhase === 0 && distance > 5) {
            frame.setData(x, y, GLYPH.BOX_H, gridAttr);
          } else {
            // For rainbow road, use full blocks to show the color
            if (isRainbowRoad) {
              frame.setData(x, y, GLYPH.FULL_BLOCK, roadAttr);
            } else {
              frame.setData(x, y, ' ', roadAttr);
            }
          }
        }
      } else {
        // Off road - render ground pattern based on theme
        var distFromRoad = (x < leftEdge) ? leftEdge - x : x - rightEdge;
        this.renderGroundCell(frame, x, y, distFromRoad, distance, leftEdge, rightEdge, shoulderAttr, curve || 0);
      }
    }
  }
  
  /**
   * Render finish line cell.
   */
  private renderFinishCell(frame: Frame, x: number, y: number, 
                           centerX: number, leftEdge: number, rightEdge: number,
                           distance: number): void {
    if (x === leftEdge || x === rightEdge) {
      frame.setData(x, y, GLYPH.BOX_V, makeAttr(WHITE, BG_BLACK));
      return;
    }
    
    var checkerSize = Math.max(1, Math.floor(3 / distance));
    var checkerX = Math.floor((x - centerX) / checkerSize);
    var checkerY = Math.floor(y / 2);
    var isWhite = ((checkerX + checkerY) % 2) === 0;
    
    if (isWhite) {
      frame.setData(x, y, GLYPH.FULL_BLOCK, makeAttr(WHITE, BG_LIGHTGRAY));
    } else {
      frame.setData(x, y, ' ', makeAttr(BLACK, BG_BLACK));
    }
  }
  
  /**
   * Render a single ground/off-road cell based on theme ground config.
   * Supports solid, grid (holodeck), dither (dirt), grass, and sand patterns.
   */
  private renderGroundCell(frame: Frame, x: number, y: number, 
                           distFromRoad: number, distance: number,
                           _leftEdge: number, _rightEdge: number,
                           shoulderAttr: number, _curve?: number): void {
    var ground = this.activeTheme.ground;
    
    // Default behavior if no ground config - just shoulder then black
    if (!ground) {
      if (distFromRoad <= 2) {
        frame.setData(x, y, GLYPH.GRASS, shoulderAttr);
      }
      return;
    }
    
    // Determine which type of ground pattern to render
    switch (ground.type) {
      case 'grid':
      case 'lava':
      case 'candy':
      case 'void':
      case 'cobblestone':
      case 'jungle':
      case 'dirt':
      case 'water':
        // These types are rendered on dedicated ground grid layer - leave transparent here
        // Just don't render anything, let the ground grid layer show through
        return;
      case 'dither':
        this.renderDitherGround(frame, x, y, distFromRoad, distance, ground);
        break;
      case 'grass':
        this.renderGrassGround(frame, x, y, distFromRoad, distance, ground);
        break;
      case 'sand':
        this.renderSandGround(frame, x, y, distFromRoad, distance, ground);
        break;
      case 'solid':
      default:
        // Solid color fill
        if (distFromRoad <= 2) {
          frame.setData(x, y, GLYPH.GRASS, shoulderAttr);
        } else {
          var solidAttr = makeAttr(ground.primary.fg, ground.primary.bg);
          frame.setData(x, y, ' ', solidAttr);
        }
        break;
    }
  }
  
  /**
   * Render dithered dirt/gravel ground pattern.
   */
  private renderDitherGround(frame: Frame, x: number, y: number,
                              _distFromRoad: number, distance: number,
                              ground: any): void {
    var pattern = ground.pattern || {};
    var density = pattern.ditherDensity || 0.3;
    var chars = pattern.ditherChars || ['.', ',', "'"];
    
    var primaryAttr = makeAttr(ground.primary.fg, ground.primary.bg);
    var secondaryAttr = makeAttr(ground.secondary.fg, ground.secondary.bg);
    
    // Pseudo-random based on position
    var hash = (x * 31 + y * 17 + Math.floor(distance)) % 100;
    var normalized = hash / 100;
    
    if (normalized < density) {
      var charIndex = hash % chars.length;
      frame.setData(x, y, chars[charIndex], secondaryAttr);
    } else {
      frame.setData(x, y, ' ', primaryAttr);
    }
  }
  
  /**
   * Render grass ground pattern with tufts.
   */
  private renderGrassGround(frame: Frame, x: number, y: number,
                             _distFromRoad: number, distance: number,
                             ground: any): void {
    var pattern = ground.pattern || {};
    var density = pattern.grassDensity || 0.4;
    var chars = pattern.grassChars || ['"', "'", ',', '.'];
    
    var primaryAttr = makeAttr(ground.primary.fg, ground.primary.bg);
    var secondaryAttr = makeAttr(ground.secondary.fg, ground.secondary.bg);
    
    // Pseudo-random grass placement
    var hash = (x * 23 + y * 41 + Math.floor(distance * 2)) % 100;
    var normalized = hash / 100;
    
    if (normalized < density) {
      var charIndex = hash % chars.length;
      // Alternate colors for depth
      var attr = ((x + y) % 3 === 0) ? secondaryAttr : primaryAttr;
      frame.setData(x, y, chars[charIndex], attr);
    } else {
      frame.setData(x, y, ' ', primaryAttr);
    }
  }
  
  /**
   * Render sand/beach ground pattern.
   */
  private renderSandGround(frame: Frame, x: number, y: number,
                            _distFromRoad: number, distance: number,
                            ground: any): void {
    var primaryAttr = makeAttr(ground.primary.fg, ground.primary.bg);
    var secondaryAttr = makeAttr(ground.secondary.fg, ground.secondary.bg);
    
    // Subtle wave pattern in sand  
    var hash = (x * 17 + y * 29 + Math.floor(distance)) % 100;
    
    if (hash < 15) {
      // Occasional ripple marks
      frame.setData(x, y, '~', secondaryAttr);
    } else if (hash < 25) {
      frame.setData(x, y, '.', primaryAttr);
    } else {
      frame.setData(x, y, ' ', primaryAttr);
    }
  }
  
  /**
   * Render roadside sprites using the sprite pool.
   * Called with list of visible roadside objects.
   */
  renderRoadsideSprites(objects: { x: number; y: number; distance: number; type: string }[]): void {
    // Sort by distance (far to near) for proper z-order
    objects.sort(function(a, b) { return b.distance - a.distance; });
    
    var poolSize = this.frameManager.getRoadsidePoolSize();
    var used = 0;
    
    // Check if we should apply glitch color effects
    var applyGlitch = this.activeTheme.name === 'glitch_circuit' && 
                       typeof GlitchState !== 'undefined' && 
                       GlitchState.roadsideColorShift !== 0;
    
    for (var i = 0; i < objects.length && used < poolSize; i++) {
      var obj = objects[i];
      var spriteFrame = this.frameManager.getRoadsideFrame(used);
      if (!spriteFrame) continue;
      
      // Select sprite and scale based on type and distance
      // Select sprite from cache based on type
      var sprite = this.spriteCache[obj.type];
      if (!sprite) {
        // Fallback: try to find any available sprite from pool
        var pool = this.activeTheme.roadside.pool;
        if (pool.length > 0) {
          sprite = this.spriteCache[pool[0].sprite];
        }
        if (!sprite) continue;  // No sprite available
      }
      
      // Scale index based on distance
      var scaleIndex = this.getScaleForDistance(obj.distance);
      
      // Render sprite to frame
      renderSpriteToFrame(spriteFrame, sprite, scaleIndex);
      
      // Apply glitch color corruption to roadside sprites
      if (applyGlitch) {
        this.applyGlitchToSpriteFrame(spriteFrame, sprite, scaleIndex);
      }
      
      // Position frame
      var size = getSpriteSize(sprite, scaleIndex);
      var frameX = Math.round(obj.x - size.width / 2);
      var frameY = Math.round(obj.y - size.height + 1);
      
      this.frameManager.positionRoadsideFrame(used, frameX, frameY, true);
      used++;
    }
    
    // Hide unused frames
    for (var j = used; j < poolSize; j++) {
      this.frameManager.positionRoadsideFrame(j, 0, 0, false);
    }
  }
  
  /**
   * Apply glitch color effects to a sprite frame.
   */
  private applyGlitchToSpriteFrame(frame: Frame, sprite: SpriteDefinition, scaleIndex: number): void {
    var variant = sprite.variants[scaleIndex];
    if (!variant) variant = sprite.variants[sprite.variants.length - 1];
    
    for (var row = 0; row < variant.length; row++) {
      for (var col = 0; col < variant[row].length; col++) {
        var cell = variant[row][col];
        if (cell !== null && cell !== undefined) {
          // Extract current colors from attr
          var fg = cell.attr & 0x0F;
          var bg = cell.attr & 0xF0;
          
          // Get glitched colors
          var glitched = GlitchState.getGlitchedSpriteColor(fg, bg);
          var newAttr = makeAttr(glitched.fg, glitched.bg);
          
          // Occasionally corrupt the character too
          var char = cell.char;
          if (Math.random() < GlitchState.intensity * 0.15) {
            char = GlitchState.corruptChar(char);
          }
          
          frame.setData(col, row, char, newAttr);
        }
      }
    }
  }
  
  /**
   * Get sprite scale index for a given distance.
   */
  private getScaleForDistance(distance: number): number {
    if (distance > 8) return 0;
    if (distance > 5) return 1;
    if (distance > 3) return 2;
    if (distance > 1.5) return 3;
    return 4;
  }
  
  // Track current brake state for sprite caching
  private _currentBrakeLightsOn: boolean = false;
  
  /**
   * Set brake light state for next render.
   * Called by Game based on player input/deceleration.
   */
  setBrakeLightState(brakeLightsOn: boolean): void {
    this._currentBrakeLightsOn = brakeLightsOn;
  }
  
  /**
   * Render player vehicle with visual effects.
   * 
   * @param playerX - Lateral position for steering offset
   * @param isFlashing - Collision/damage flash
   * @param isBoosting - Mushroom boost active
   * @param hasStar - Star power active
   * @param hasBullet - Bullet power active
   * @param hasLightning - Lightning debuff active
   * @param carId - Car type from CarCatalog
   * @param carColorId - Color from CarCatalog
   * @param brakeLightsOn - Whether brake lights should be lit
   */
  renderPlayerVehicle(playerX: number, isFlashing?: boolean, isBoosting?: boolean, 
                      hasStar?: boolean, hasBullet?: boolean, hasLightning?: boolean,
                      carId?: string, carColorId?: string, brakeLightsOn?: boolean): void {
    var frame = this.frameManager.getVehicleFrame(0);
    if (!frame) return;
    
    // Get the appropriate sprite based on car selection and brake state
    var effectiveCarId = carId || 'sports';
    var effectiveColorId = carColorId || 'yellow';
    var effectiveBrake = brakeLightsOn !== undefined ? brakeLightsOn : this._currentBrakeLightsOn;
    
    // Get car definition for body style
    var carDef = getCarDefinition(effectiveCarId);
    var bodyStyle: CarBodyStyle = carDef ? carDef.bodyStyle : 'sports';
    
    // Get the parameterized sprite
    var sprite = getPlayerCarSprite(bodyStyle, effectiveColorId, effectiveBrake);
    
    // Render sprite to frame
    renderSpriteToFrame(frame, sprite, 0);
    
    var now = Date.now();
    
    // Get the effect flash color for this car color (ensures visibility)
    var effectFlashColor = getEffectFlashColor(effectiveColorId);
    
    // STAR EFFECT: Rainbow cycling colors (most prominent effect)
    if (hasStar) {
      var starColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
      var colorIndex = Math.floor(now / 60) % starColors.length;
      var starColor = starColors[colorIndex];
      var starAttr = makeAttr(starColor, BG_BLACK);
      // Color the entire car with cycling rainbow
      for (var y = 0; y < 3; y++) {
        for (var x = 0; x < 5; x++) {
          var cell = sprite.variants[0][y] ? sprite.variants[0][y][x] : null;
          if (cell) {
            frame.setData(x, y, cell.char, starAttr);
          }
        }
      }
    }
    // BULLET EFFECT: Fast white/yellow flash + speed lines look
    else if (hasBullet) {
      var bulletColor = (Math.floor(now / 40) % 2 === 0) ? WHITE : effectFlashColor;
      var bulletAttr = makeAttr(bulletColor, BG_BLACK);
      // Whole car flashes fast
      for (var y = 0; y < 3; y++) {
        for (var x = 0; x < 5; x++) {
          var cell = sprite.variants[0][y] ? sprite.variants[0][y][x] : null;
          if (cell) {
            frame.setData(x, y, cell.char, bulletAttr);
          }
        }
      }
    }
    // LIGHTNING HIT: Blue/cyan crackling effect
    else if (hasLightning) {
      var lightningColor = (Math.floor(now / 120) % 3 === 0) ? BLUE : 
                          (Math.floor(now / 120) % 3 === 1) ? LIGHTCYAN : CYAN;
      var lightningAttr = makeAttr(lightningColor, BG_BLACK);
      // Car flickers blue when slowed by lightning
      for (var y = 0; y < 3; y++) {
        for (var x = 0; x < 5; x++) {
          var cell = sprite.variants[0][y] ? sprite.variants[0][y][x] : null;
          if (cell) {
            frame.setData(x, y, cell.char, lightningAttr);
          }
        }
      }
    }
    // DAMAGE FLASH: Red/white alternating (collision recovery)
    else if (isFlashing) {
      var flashColor = (Math.floor(now / 100) % 2 === 0) ? WHITE : LIGHTRED;
      var flashAttr = makeAttr(flashColor, BG_BLACK);
      for (var y = 0; y < 3; y++) {
        for (var x = 0; x < 5; x++) {
          var cell = sprite.variants[0][y] ? sprite.variants[0][y][x] : null;
          if (cell) {
            frame.setData(x, y, cell.char, flashAttr);
          }
        }
      }
    }
    // MUSHROOM BOOST: Cyan/yellow shimmer on exhaust
    else if (isBoosting) {
      var boostColor = (Math.floor(now / 80) % 2 === 0) ? LIGHTCYAN : effectFlashColor;
      var boostAttr = makeAttr(boostColor, BG_BLACK);
      // Just color the bottom row (exhaust/wheels area) for boost effect
      for (var bx = 0; bx < 5; bx++) {
        var cell = sprite.variants[0][2] ? sprite.variants[0][2][bx] : null;
        if (cell) {
          frame.setData(bx, 2, cell.char, boostAttr);
        }
      }
    }
    
    // Player is always at bottom center-ish
    var screenX = 40 + Math.round(playerX * 5) - 2;
    var screenY = this.height - 3;
    
    this.frameManager.positionVehicleFrame(0, screenX, screenY, true);
  }
  
  /**
   * Render HUD elements.
   */
  renderHud(hudData: HudData): void {
    var frame = this.frameManager.getHudFrame();
    if (!frame) return;
    
    frame.clear();
    
    var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
    var valueAttr = colorToAttr(PALETTE.HUD_VALUE);
    
    // Get HUD customization from theme (if any)
    var hudConfig = this.activeTheme.hud;
    var timeLabel = (hudConfig && hudConfig.timeLabel) ? hudConfig.timeLabel : 'TIME';
    var lapLabel = (hudConfig && hudConfig.lapLabel) ? hudConfig.lapLabel : 'LAP';
    var positionPrefix = (hudConfig && hudConfig.positionPrefix) ? hudConfig.positionPrefix : '';
    var speedMultiplier = (hudConfig && hudConfig.speedMultiplier) ? hudConfig.speedMultiplier : 1;
    var speedSuffix = (hudConfig && hudConfig.speedLabel) ? ' ' + hudConfig.speedLabel : '';
    
    // Top bar - TIME or custom label (centered)
    this.writeStringToFrame(frame, 35, 0, timeLabel, labelAttr);
    this.writeStringToFrame(frame, 35 + timeLabel.length + 1, 0, LapTimer.format(hudData.lapTime), valueAttr);
    
    // Bottom bar (row 23):
    // LEFT:  [====LAP 2/3====]
    // RIGHT: [300] [=====SPD=====]
    // Position (8th) on row above, far left
    var bottomY = this.height - 1;
    
    // POSITION - One row up, far left (with optional prefix like "Node ")
    var posStr = positionPrefix + hudData.position + PositionIndicator.getOrdinalSuffix(hudData.position);
    this.writeStringToFrame(frame, 0, bottomY - 1, posStr, valueAttr);
    
    // LEFT SIDE - Lap progress bar with "LAP 2/3" or custom label centered inside
    this.renderLapProgressBar(frame, hudData.lap, hudData.totalLaps, hudData.lapProgress, 0, bottomY, 16, lapLabel);
    
    // RIGHT SIDE - Speed numeric and bar (full width)
    // Apply speed multiplier for themed display (e.g., MPH -> Kbps)
    var displaySpeed = Math.round(hudData.speed * speedMultiplier);
    var maxDisplaySpeed = Math.round(300 * speedMultiplier);
    var speedDisplay = displaySpeed > maxDisplaySpeed ? maxDisplaySpeed + '+' : this.padLeft(displaySpeed.toString(), 3);
    if (speedSuffix && displaySpeed <= maxDisplaySpeed) {
      // For themed speed with suffix (e.g., "28.8 Kbps"), adjust display
      speedDisplay = displaySpeed.toFixed(1) + speedSuffix;
    }
    var speedAttr = displaySpeed > maxDisplaySpeed ? colorToAttr({ fg: LIGHTRED, bg: BG_BLACK }) : valueAttr;
    var speedX = 79 - speedDisplay.length;  // Right-align before bar
    this.writeStringToFrame(frame, speedX - 12, bottomY, speedDisplay, speedAttr);
    this.renderSpeedometerBarCompact(frame, hudData.speed, hudData.speedMax, 67, bottomY, 11);
    
    // Held item display - ABOVE speedometer (rows 20-22, right side)
    this.renderItemSlotWithIcon(frame, hudData.heldItem);
    
    // Render countdown stoplight if race hasn't started
    if (hudData.countdown > 0 && hudData.raceMode === RaceMode.GRAND_PRIX) {
      this.renderStoplight(frame, hudData.countdown);
    }
  }
  
  /**
   * Render item slot with visual icon ABOVE the speedometer.
   * Layout: [divider] [quantity if >1] [icon]
   * Icon area: rows 19-21, columns 67-79 (same width as speedometer below)
   * Row 22 is margin between icons and speedometer on row 23.
   */
  private renderItemSlotWithIcon(frame: any, heldItem: HeldItemData | null): void {
    var slotLeft = 67;      // Align with speedometer
    var slotRight = 79;     // Right edge of screen
    var slotTop = 19;       // 4 rows above bottom (moved up 1 for margin)
    var slotBottom = 21;    // 2 rows above speedometer
    var slotHeight = 3;
    var slotWidth = slotRight - slotLeft + 1;  // 13 chars wide
    
    var separatorAttr = makeAttr(DARKGRAY, BG_BLACK);
    
    // Draw vertical divider on left edge with fancy ends
    frame.setData(slotLeft, slotTop, GLYPH.BOX_VD_HD, separatorAttr);      // ╤ top
    for (var row = slotTop + 1; row < slotBottom; row++) {
      frame.setData(slotLeft, row, GLYPH.BOX_V, separatorAttr);            // │ middle
    }
    frame.setData(slotLeft, slotBottom, GLYPH.BOX_VD_HU, separatorAttr);   // ╧ bottom
    
    if (heldItem === null) {
      // No item - just show empty slot (no text)
      return;
    }
    
    var itemType = heldItem.type;
    var uses = heldItem.uses;
    var isActivated = heldItem.activated;
    
    // Get icon data - all icons are 3 rows tall, cells have individual colors
    var icon = this.getItemIconCP437(itemType);
    
    // Calculate icon position (centered in remaining space after divider)
    var iconWidth = icon.cells[0].length;
    var availableWidth = slotWidth - 1;  // Minus divider
    var iconX = slotLeft + 1 + Math.floor((availableWidth - iconWidth) / 2);
    
    // If we have uses > 1, shift icon right and show count
    if (uses > 1) {
      iconX = slotLeft + 3;  // Leave room for count
      var countAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
      frame.setData(slotLeft + 1, slotTop + 1, String(uses).charAt(0), countAttr);
    }
    
    // Render icon cells (3 rows)
    for (var row = 0; row < icon.cells.length && row < slotHeight; row++) {
      var cellRow = icon.cells[row];
      for (var col = 0; col < cellRow.length; col++) {
        var cellData = cellRow[col];
        if (cellData.char !== ' ') {
          var attr = this.getIconAttrCP437(cellData.attr, itemType, isActivated);
          frame.setData(iconX + col, slotTop + row, cellData.char, attr);
        }
      }
    }
  }
  
  /**
   * Get CP437-based icon for item type.
   * All icons are exactly 3 rows tall for consistency.
   * Uses background colors for body with foreground details on top.
   * Returns cells with individual char+attr for each position.
   */
  private getItemIconCP437(itemType: ItemType): { cells: { char: string, attr: number }[][], mainColor: number, altColor?: number } {
    // Block characters:
    // █ = FULL_BLOCK (219)    ▄ = LOWER_HALF (220)    ▀ = UPPER_HALF (223)
    // ▌ = LEFT_HALF (221)     ▐ = RIGHT_HALF (222)
    // ░ = LIGHT_SHADE (176)   ▒ = MEDIUM_SHADE (177)  ▓ = DARK_SHADE (178)
    
    var FB = GLYPH.FULL_BLOCK;    // █
    var LH = GLYPH.LOWER_HALF;    // ▄
    var UH = GLYPH.UPPER_HALF;    // ▀
    var DS = GLYPH.DARK_SHADE;    // ▓
    var LS = GLYPH.LIGHT_SHADE;   // ░
    var __ = ' ';  // Space (transparent)
    
    // Helper to create a cell
    function cell(ch: string, fg: number, bg: number): { char: string, attr: number } {
      return { char: ch, attr: makeAttr(fg, bg) };
    }
    
    // Empty/transparent cell
    var X = cell(__, BLACK, BG_BLACK);
    
    switch (itemType) {
      // === MUSHROOM - Red cap with white spots, stem ===
      case ItemType.MUSHROOM:
      case ItemType.MUSHROOM_TRIPLE:
        // Use BG_RED for cap, draw white spots on top
        //   ▄██▄      (red on black edges, solid red middle)
        //  █o██o█    (white 'o' spots on red background)
        //    ██      (white stem)
        return {
          cells: [
            [X, cell(LH, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(LH, LIGHTRED, BG_BLACK), X],
            [cell(FB, LIGHTRED, BG_BLACK), cell('o', WHITE, BG_RED), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell('o', WHITE, BG_RED), cell(FB, LIGHTRED, BG_BLACK)],
            [X, X, cell(FB, WHITE, BG_BLACK), cell(FB, WHITE, BG_BLACK), X, X]
          ],
          mainColor: LIGHTRED
        };
        
      case ItemType.MUSHROOM_GOLDEN:
        // Golden version - yellow cap with white sparkles
        return {
          cells: [
            [X, cell(LH, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(LH, YELLOW, BG_BLACK), X],
            [cell(FB, YELLOW, BG_BLACK), cell('*', WHITE, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell('*', WHITE, BG_BLACK), cell(FB, YELLOW, BG_BLACK)],
            [X, X, cell(FB, WHITE, BG_BLACK), cell(FB, WHITE, BG_BLACK), X, X]
          ],
          mainColor: YELLOW
        };
      
      // === SHELLS - Turtle shell with pattern ===
      case ItemType.GREEN_SHELL:
      case ItemType.GREEN_SHELL_TRIPLE:
        // BG_GREEN body with lighter pattern details
        //  ▄██▄
        // █░██░█   (light shade pattern on green bg)
        //  ▀██▀
        return {
          cells: [
            [X, cell(LH, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(LH, LIGHTGREEN, BG_BLACK), X],
            [cell(FB, LIGHTGREEN, BG_BLACK), cell(LS, YELLOW, BG_GREEN), cell(FB, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(LS, YELLOW, BG_GREEN), cell(FB, LIGHTGREEN, BG_BLACK)],
            [X, cell(UH, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(UH, LIGHTGREEN, BG_BLACK), X]
          ],
          mainColor: LIGHTGREEN
        };
        
      case ItemType.RED_SHELL:
      case ItemType.RED_SHELL_TRIPLE:
      case ItemType.SHELL:
      case ItemType.SHELL_TRIPLE:
        return {
          cells: [
            [X, cell(LH, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(LH, LIGHTRED, BG_BLACK), X],
            [cell(FB, LIGHTRED, BG_BLACK), cell(LS, YELLOW, BG_RED), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(LS, YELLOW, BG_RED), cell(FB, LIGHTRED, BG_BLACK)],
            [X, cell(UH, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(UH, LIGHTRED, BG_BLACK), X]
          ],
          mainColor: LIGHTRED
        };
        
      case ItemType.BLUE_SHELL:
        // Blue shell with "wings" effect using extended width
        // ░▄██▄░    (light edges for wing effect)
        // ▀ ▓▓ ▀   (dark pattern, wing tips)
        //  ▀██▀
        return {
          cells: [
            [cell(LS, LIGHTCYAN, BG_BLACK), cell(LH, LIGHTCYAN, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(LH, LIGHTCYAN, BG_BLACK), cell(LS, LIGHTCYAN, BG_BLACK)],
            [cell(UH, WHITE, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(DS, WHITE, BG_CYAN), cell(DS, WHITE, BG_CYAN), cell(FB, LIGHTCYAN, BG_BLACK), cell(UH, WHITE, BG_BLACK)],
            [X, cell(UH, LIGHTCYAN, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(UH, LIGHTCYAN, BG_BLACK), X]
          ],
          mainColor: LIGHTCYAN,
          altColor: WHITE
        };
      
      // === BANANA - Yellow peel with green tip ===
      case ItemType.BANANA:
      case ItemType.BANANA_TRIPLE:
        // Curved banana shape - yellow with green ends
        //  ▄██▄       (top curve)
        // █▀  ▀█      (open middle - peel shape)
        // ▀    ▀      (bottom tips)
        return {
          cells: [
            [X, cell(LH, GREEN, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(LH, GREEN, BG_BLACK), X],
            [cell(FB, YELLOW, BG_BLACK), cell(UH, YELLOW, BG_BLACK), X, X, cell(UH, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK)],
            [cell(UH, YELLOW, BG_BLACK), X, X, X, X, cell(UH, YELLOW, BG_BLACK)]
          ],
          mainColor: YELLOW
        };
      
      // === STAR - Invincibility star, pure yellow ===
      case ItemType.STAR:
        // Star shape - all yellow blocks
        //   ▄█▄       (top point)
        // ▀█████▀     (arms extend with solid center)
        //   ▀ ▀       (bottom points)
        return {
          cells: [
            [X, X, cell(LH, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(LH, YELLOW, BG_BLACK), X, X],
            [cell(UH, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(UH, YELLOW, BG_BLACK)],
            [X, X, cell(UH, YELLOW, BG_BLACK), X, cell(UH, YELLOW, BG_BLACK), X, X]
          ],
          mainColor: YELLOW
        };
      
      // === LIGHTNING - ASCII zigzag bolt ===
      case ItemType.LIGHTNING:
        // Sharp ASCII bolt - distinct from banana
        //  /\         
        // /--\        
        //    \/       
        return {
          cells: [
            [X, cell('/', LIGHTCYAN, BG_BLACK), cell('\\', LIGHTCYAN, BG_BLACK), X, X],
            [cell('/', LIGHTCYAN, BG_BLACK), cell('-', YELLOW, BG_BLACK), cell('-', YELLOW, BG_BLACK), cell('\\', LIGHTCYAN, BG_BLACK), X],
            [X, X, X, cell('\\', LIGHTCYAN, BG_BLACK), cell('/', LIGHTCYAN, BG_BLACK)]
          ],
          mainColor: YELLOW,
          altColor: LIGHTCYAN
        };
      
      // === BULLET BILL - Solid bullet with definition ===
      case ItemType.BULLET:
        // Bullet shape - light gray body, dark outline, white eyes
        // ▄▓▓▓▓▄     (rounded top, dark outline)
        // █oo▓▓▀     (eyes, gray body, pointed tip)
        // ▀▓▓▓▓▀     (rounded bottom)
        return {
          cells: [
            [cell(LH, DARKGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(LH, DARKGRAY, BG_BLACK)],
            [cell(FB, DARKGRAY, BG_BLACK), cell('O', WHITE, BG_BLACK), cell('O', WHITE, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(UH, LIGHTGRAY, BG_BLACK)],
            [cell(UH, DARKGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(UH, DARKGRAY, BG_BLACK)]
          ],
          mainColor: LIGHTGRAY,
          altColor: WHITE
        };
      
      default:
        return {
          cells: [
            [X, X, cell('?', YELLOW, BG_BLACK), X, X],
            [X, cell('?', YELLOW, BG_BLACK), cell('?', YELLOW, BG_BLACK), cell('?', YELLOW, BG_BLACK), X],
            [X, X, cell('?', YELLOW, BG_BLACK), X, X]
          ],
          mainColor: YELLOW
        };
    }
  }
  
  /**
   * Get attribute for icon cell, with animation support.
   */
  private getIconAttrCP437(baseAttr: number, itemType: ItemType, isActivated: boolean): number {
    // Special animation handling
    switch (itemType) {
      case ItemType.STAR:
        // Rainbow cycling ONLY when activated
        if (isActivated) {
          var starColors = [YELLOW, LIGHTRED, LIGHTGREEN, LIGHTCYAN, LIGHTMAGENTA, WHITE];
          var colorIdx = Math.floor(Date.now() / 100) % starColors.length;
          // Keep the background but change foreground
          var bg = (baseAttr >> 4) & 0x07;
          return makeAttr(starColors[colorIdx], bg << 4);
        }
        break;
        
      case ItemType.LIGHTNING:
        // Flash cyan/yellow when activated
        if (isActivated && (Math.floor(Date.now() / 150) % 2 === 0)) {
          var bg2 = (baseAttr >> 4) & 0x07;
          return makeAttr(LIGHTCYAN, bg2 << 4);
        }
        break;
    }
    
    return baseAttr;
  }
  
  /**
   * Render stoplight countdown graphic - horizontal layout above track.
   */
  private renderStoplight(frame: Frame, countdown: number): void {
    var countNum = Math.ceil(countdown);
    var centerX = 40;
    var topY = 3;  // Near top, below HUD bar
    
    // Frame/housing colors
    var frameAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
    
    // Light states based on countdown
    var redOn = countNum >= 3;
    var yellowOn = countNum === 2;
    var greenOn = countNum === 1;
    
    // Light colors (on vs off) - brighter when on
    var redAttr = redOn ? colorToAttr({ fg: LIGHTRED, bg: BG_RED }) : colorToAttr({ fg: RED, bg: BG_BLACK });
    var yellowAttr = yellowOn ? colorToAttr({ fg: YELLOW, bg: BG_BROWN }) : colorToAttr({ fg: BROWN, bg: BG_BLACK });
    var greenAttr = greenOn ? colorToAttr({ fg: LIGHTGREEN, bg: BG_GREEN }) : colorToAttr({ fg: GREEN, bg: BG_BLACK });
    
    // Horizontal stoplight (15 chars wide x 3 tall)
    // [###] [###] [###]
    var boxX = centerX - 7;
    
    // Top border
    frame.setData(boxX, topY, GLYPH.DBOX_TL, frameAttr);
    for (var i = 1; i < 14; i++) {
      frame.setData(boxX + i, topY, GLYPH.DBOX_H, frameAttr);
    }
    frame.setData(boxX + 14, topY, GLYPH.DBOX_TR, frameAttr);
    
    // Middle row with lights: | ## | ## | ## |
    frame.setData(boxX, topY + 1, GLYPH.DBOX_V, frameAttr);
    
    // RED light (positions 1-3)
    frame.setData(boxX + 1, topY + 1, GLYPH.FULL_BLOCK, redAttr);
    frame.setData(boxX + 2, topY + 1, GLYPH.FULL_BLOCK, redAttr);
    frame.setData(boxX + 3, topY + 1, GLYPH.FULL_BLOCK, redAttr);
    
    frame.setData(boxX + 4, topY + 1, GLYPH.DBOX_V, frameAttr);
    
    // YELLOW light (positions 5-7)
    frame.setData(boxX + 5, topY + 1, GLYPH.FULL_BLOCK, yellowAttr);
    frame.setData(boxX + 6, topY + 1, GLYPH.FULL_BLOCK, yellowAttr);
    frame.setData(boxX + 7, topY + 1, GLYPH.FULL_BLOCK, yellowAttr);
    
    frame.setData(boxX + 8, topY + 1, GLYPH.DBOX_V, frameAttr);
    
    // GREEN light (positions 9-11)
    frame.setData(boxX + 9, topY + 1, GLYPH.FULL_BLOCK, greenAttr);
    frame.setData(boxX + 10, topY + 1, GLYPH.FULL_BLOCK, greenAttr);
    frame.setData(boxX + 11, topY + 1, GLYPH.FULL_BLOCK, greenAttr);
    
    frame.setData(boxX + 12, topY + 1, ' ', frameAttr);
    frame.setData(boxX + 13, topY + 1, countNum.toString(), colorToAttr({ fg: WHITE, bg: BG_BLACK }));
    frame.setData(boxX + 14, topY + 1, GLYPH.DBOX_V, frameAttr);
    
    // Bottom border
    frame.setData(boxX, topY + 2, GLYPH.DBOX_BL, frameAttr);
    for (var j = 1; j < 14; j++) {
      frame.setData(boxX + j, topY + 2, GLYPH.DBOX_H, frameAttr);
    }
    frame.setData(boxX + 14, topY + 2, GLYPH.DBOX_BR, frameAttr);
  }
  
  /**
   * Render lap progress bar with "LAP X/Y" text centered inside.
   * Background fills from BG_BLACK to BG_BLUE as progress increases.
   * Text is always YELLOW to stand out on either background.
   */
  private renderLapProgressBar(frame: Frame, lap: number, totalLaps: number, progress: number, x: number, y: number, width: number, label?: string): void {
    var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
    
    // Draw brackets
    frame.setData(x, y, '[', labelAttr);
    frame.setData(x + width + 1, y, ']', labelAttr);
    
    // Calculate fill width (inside the brackets)
    var fillWidth = Math.round(progress * width);
    
    // Build the "LAP X/Y" text to center inside (use custom label if provided)
    var lapLabel = label || 'LAP';
    var lapText = lapLabel + ' ' + lap + '/' + totalLaps;
    var textStart = Math.floor((width - lapText.length) / 2);
    
    // Render each character inside the bar
    for (var i = 0; i < width; i++) {
      var isFilled = i < fillWidth;
      var bg = isFilled ? BG_BLUE : BG_BLACK;
      
      // Check if this position has a character from the lap text
      var textIndex = i - textStart;
      if (textIndex >= 0 && textIndex < lapText.length) {
        // Render lap text character in YELLOW
        var attr = colorToAttr({ fg: YELLOW, bg: bg });
        frame.setData(x + 1 + i, y, lapText.charAt(textIndex), attr);
      } else {
        // Render fill or empty space
        if (isFilled) {
          var filledAttr = colorToAttr({ fg: LIGHTBLUE, bg: BG_BLUE });
          frame.setData(x + 1 + i, y, ' ', filledAttr);
        } else {
          var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
          frame.setData(x + 1 + i, y, GLYPH.LIGHT_SHADE, emptyAttr);
        }
      }
    }
  }
  
  /**
   * Render compact speedometer bar at specified position.
   */
  private renderSpeedometerBarCompact(frame: Frame, speed: number, maxSpeed: number, x: number, y: number, width: number): void {
    var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
    var filledAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
    var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
    var highAttr = colorToAttr({ fg: LIGHTRED, bg: BG_BLACK });
    var boostAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
    
    frame.setData(x, y, '[', labelAttr);
    
    // Clamp fill to 100% for bar display, but show boost color if over
    var fillAmount = Math.min(1.0, speed / maxSpeed);
    var isBoost = speed > maxSpeed;
    var fillWidth = Math.round(fillAmount * width);
    
    for (var i = 0; i < width; i++) {
      var attr: number;
      if (i < fillWidth) {
        if (isBoost) {
          attr = boostAttr;  // Cyan when boosting
        } else if (fillAmount > 0.8) {
          attr = highAttr;   // Red when near max
        } else {
          attr = filledAttr; // Green normally
        }
      } else {
        attr = emptyAttr;
      }
      var char = (i < fillWidth) ? GLYPH.FULL_BLOCK : GLYPH.LIGHT_SHADE;
      frame.setData(x + 1 + i, y, char, attr);
    }
    
    frame.setData(x + width + 1, y, ']', labelAttr);
  }

  /**
   * Helper to write a string to a frame.
   */
  private writeStringToFrame(frame: Frame, x: number, y: number, str: string, attr: number): void {
    for (var i = 0; i < str.length; i++) {
      frame.setData(x + i, y, str.charAt(i), attr);
    }
  }
  
  /**
   * Pad string on left.
   */
  private padLeft(str: string, len: number): string {
    while (str.length < len) {
      str = ' ' + str;
    }
    return str;
  }
  
  /**
   * Trigger a lightning bolt visual effect at a specific X position.
   * The bolt will animate from the top of the screen to the target Y.
   * 
   * @param targetX - X position on screen (0-79)
   * @param targetY - Y position where bolt terminates (default: player car)
   */
  triggerLightningBolt(targetX?: number, targetY?: number): void {
    var x = targetX !== undefined ? targetX : 40;  // Default to center
    var y = targetY !== undefined ? targetY : this.height - 3;  // Default to player car
    
    this._lightningBolts.push({
      x: x,
      startTime: Date.now(),
      targetY: y
    });
  }
  
  /**
   * Trigger lightning bolts for all affected vehicles (called when lightning item is used).
   * Creates bolts at the player position and random positions across the screen.
   * 
   * @param hitCount - Number of vehicles hit (determines number of bolts)
   */
  triggerLightningStrike(hitCount: number): void {
    // Always add a main bolt at center (player's screen)
    this.triggerLightningBolt(40, this.height - 3);
    
    // Add additional bolts at random positions for dramatic effect
    var additionalBolts = Math.min(hitCount, 4);  // Cap at 4 extra bolts
    for (var i = 0; i < additionalBolts; i++) {
      var randomX = 10 + Math.floor(Math.random() * 60);  // Spread across screen
      var randomY = 8 + Math.floor(Math.random() * 10);   // Various depths on road
      this.triggerLightningBolt(randomX, randomY);
    }
  }
  
  /**
   * Render active lightning bolt effects.
   * Called during endFrame() to draw bolts on top of everything.
   */
  private renderLightningBolts(): void {
    if (this._lightningBolts.length === 0) return;
    
    var now = Date.now();
    var roadFrame = this.frameManager.getRoadFrame();
    if (!roadFrame) return;
    
    // Lightning bolt duration (ms) and animation speed
    var boltDuration = 300;  // Total effect duration
    var flashCycleMs = 40;   // How fast colors alternate
    
    // Process each active bolt
    for (var i = this._lightningBolts.length - 1; i >= 0; i--) {
      var bolt = this._lightningBolts[i];
      var elapsed = now - bolt.startTime;
      
      // Remove expired bolts
      if (elapsed > boltDuration) {
        this._lightningBolts.splice(i, 1);
        continue;
      }
      
      // Calculate bolt progress (0 = starting, 1 = complete)
      var progress = elapsed / boltDuration;
      
      // Bolt descends from sky to target
      var startY = 1;
      var currentEndY = Math.floor(startY + (bolt.targetY - startY) * Math.min(1, progress * 2));
      
      // Flash colors: white/yellow/cyan cycling
      var colorPhase = Math.floor(now / flashCycleMs) % 3;
      var boltColor = colorPhase === 0 ? WHITE : (colorPhase === 1 ? YELLOW : LIGHTCYAN);
      var boltAttr = makeAttr(boltColor, BG_BLACK);
      var brightAttr = makeAttr(WHITE, BG_BLUE);  // Extra bright flash at impact
      
      // Draw the bolt using / and \ characters with some randomness
      var x = bolt.x;
      for (var y = startY; y <= currentEndY; y++) {
        // Add slight horizontal jitter for jagged look
        var jitter = (y % 3 === 0) ? ((y % 6 === 0) ? 1 : -1) : 0;
        var drawX = x + jitter;
        
        // Choose character based on jitter direction
        var char = jitter > 0 ? '\\' : (jitter < 0 ? '/' : '|');
        
        // Extra bright at the bolt tip
        var attr = (y === currentEndY && progress < 0.7) ? brightAttr : boltAttr;
        
        if (drawX >= 0 && drawX < this.width && y >= 0 && y < this.height - 1) {
          roadFrame.setData(drawX, y, char, attr);
        }
        
        // Update x for next segment based on jitter
        x = drawX;
      }
      
      // Flash effect at impact point when bolt reaches target
      if (progress > 0.3 && progress < 0.8) {
        var flashRadius = 2;
        var impactAttr = makeAttr(WHITE, BG_CYAN);
        for (var fx = -flashRadius; fx <= flashRadius; fx++) {
          var impactX = bolt.x + fx;
          if (impactX >= 0 && impactX < this.width && bolt.targetY < this.height - 1) {
            roadFrame.setData(impactX, bolt.targetY, '*', impactAttr);
          }
        }
      }
    }
  }
  
  /**
   * Cycle all frames - push updates to screen.
   */
  cycle(): void {
    this.frameManager.cycle();
  }
  
  /**
   * Get root frame for overlays.
   */
  getRootFrame(): Frame | null {
    return this.frameManager ? this.frameManager.getRootFrame() : null;
  }

  /**
   * Get HUD frame for top-layer overlays.
   */
  getHudFrame(): Frame | null {
    return this.frameManager ? this.frameManager.getHudFrame() : null;
  }

  /**
   * Shutdown renderer.
   */
  shutdown(): void {
    this.frameManager.shutdown();
    console.clear(BG_BLACK, false);
  }
}
