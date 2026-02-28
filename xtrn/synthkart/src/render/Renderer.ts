/**
 * Renderer - Main rendering coordinator.
 *
 * Uses frame.js for efficient screen updates.
 * All rendering is delegated to specialized sub-renderers.
 *
 * NOTE: For Iteration 0, we use a simplified direct console output
 * until frame.js integration is fully tested.
 */

interface IRenderer {
  init(): void;
  beginFrame(): void;
  renderSky(trackPosition: number, curvature?: number, playerSteer?: number, speed?: number, dt?: number): void;
  renderRoad(trackPosition: number, cameraX: number, track: ITrack, road: Road): void;
  renderEntities(playerVehicle: IVehicle, vehicles: IVehicle[], items: Item[], projectiles?: IProjectile[]): void;
  renderHud(hudData: HudData): void;
  endFrame(): void;
  shutdown(): void;
  getComposer(): SceneComposer;
  
  // Optional theme support
  setTheme?(themeName: string): void;
  getAvailableThemes?(): string[];
  
  // Optional brake light support (for FrameRenderer)
  setBrakeLightState?(brakeLightsOn: boolean): void;
  
  // Optional lightning bolt visual effect
  triggerLightningStrike?(hitCount: number): void;
  
  // Get root frame for overlays
  getRootFrame?(): Frame | null;
  getHudFrame?(): Frame | null;
}

class Renderer implements IRenderer {
  private width: number;
  private height: number;
  private horizonY: number;
  private frame: Frame | null;
  private composer: SceneComposer;
  private skylineRenderer: SkylineRenderer;
  private roadRenderer: RoadRenderer;
  private spriteRenderer: SpriteRenderer;
  private hudRenderer: HudRenderer;
  private useFrame: boolean;

  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.horizonY = 8;
    this.frame = null;
    this.useFrame = false;

    // Initialize sub-renderers
    this.composer = new SceneComposer(width, height);
    this.skylineRenderer = new SkylineRenderer(this.composer, this.horizonY);
    this.roadRenderer = new RoadRenderer(this.composer, this.horizonY);
    this.spriteRenderer = new SpriteRenderer(this.composer, this.horizonY);
    this.hudRenderer = new HudRenderer(this.composer);
  }

  /**
   * Initialize the renderer.
   */
  init(): void {
    // Try to load frame.js
    try {
      load("frame.js");
      this.frame = new Frame(1, 1, this.width, this.height, BG_BLACK);
      this.frame.open();
      this.useFrame = true;
      logInfo("Renderer: Using frame.js");
    } catch (e) {
      logWarning("Renderer: frame.js not available, using direct console");
      this.useFrame = false;
    }

    // Clear screen
    console.clear(BG_BLACK, false);
  }

  /**
   * Begin a new frame.
   */
  beginFrame(): void {
    this.composer.clear();
  }

  /**
   * Get the scene composer for direct rendering.
   */
  getComposer(): SceneComposer {
    return this.composer;
  }

  /**
   * Render sky and horizon with parallax scrolling.
   */
  renderSky(trackPosition: number, curvature?: number, playerSteer?: number, speed?: number, dt?: number): void {
    this.skylineRenderer.render(trackPosition, curvature, playerSteer, speed, dt);
  }

  /**
   * Render the road.
   */
  renderRoad(trackPosition: number, cameraX: number, track: ITrack, road: Road): void {
    this.roadRenderer.render(trackPosition, cameraX, track, road);
  }

  /**
   * Render all entities.
   */
  renderEntities(playerVehicle: IVehicle, vehicles: IVehicle[], items: Item[]): void {
    // Render item boxes
    for (var i = 0; i < items.length; i++) {
      var item = items[i];
      if (!item.isAvailable()) continue;
      var relZ = item.z - playerVehicle.z;
      var relX = item.x - playerVehicle.x;
      this.spriteRenderer.renderItemBox(relZ, relX);
    }

    // Render other vehicles (sorted by distance, far to near)
    var sortedVehicles = vehicles.slice();
    sortedVehicles.sort(function(a, b) {
      return (b.z - playerVehicle.z) - (a.z - playerVehicle.z);
    });

    for (var j = 0; j < sortedVehicles.length; j++) {
      var v = sortedVehicles[j];
      if (v.id === playerVehicle.id) continue;
      var relZ = v.z - playerVehicle.z;
      var relX = v.x - playerVehicle.x;
      this.spriteRenderer.renderOtherVehicle(relZ, relX, v.color);
    }

    // Render player vehicle (always on top)
    this.spriteRenderer.renderPlayerVehicle(playerVehicle.x);
  }

  /**
   * Render the HUD.
   */
  renderHud(hudData: HudData): void {
    this.hudRenderer.render(hudData);
  }

  /**
   * Commit frame to screen.
   */
  endFrame(): void {
    var buffer = this.composer.getBuffer();

    if (this.useFrame && this.frame) {
      // Use frame.js for efficient updates
      for (var y = 0; y < this.height; y++) {
        for (var x = 0; x < this.width; x++) {
          var cell = buffer[y][x];
          this.frame.setData(x + 1, y + 1, cell.char, cell.attr);
        }
      }
      this.frame.draw();
    } else {
      // Direct console output (slower, but works without frame.js)
      console.home();
      for (var y = 0; y < this.height; y++) {
        var line = '';
        for (var x = 0; x < this.width; x++) {
          line += buffer[y][x].char;
        }
        console.print(line);
        if (y < this.height - 1) {
          console.print("\r\n");
        }
      }
    }
  }

  /**
   * Get root frame for overlays.
   */
  getRootFrame(): Frame | null {
    return this.frame;
  }

  /**
   * Shutdown renderer and restore terminal.
   */
  shutdown(): void {
    if (this.frame) {
      this.frame.close();
      this.frame = null;
    }

    // Restore terminal state
    console.attributes = LIGHTGRAY;
    console.clear(BG_BLACK, false);
  }
}
