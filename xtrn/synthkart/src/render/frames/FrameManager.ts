/**
 * FrameManager - Manages layered Frame.js frames for efficient rendering.
 * 
 * Layer architecture:
 *   Layer 0: Root frame (80x24, black background)
 *   Layer 1: Sky grid frame (animated, scrolls with forward motion)
 *   Layer 2: Sun frame (transparent, celestial bodies behind scenery)
 *   Layer 3: Mountains frame (transparent, horizontal parallax scenery)
 *   Layer 4: Ground grid frame (holodeck floor effect)
 *   Layer 5: Road frame (the pseudo-3D road surface)
 *   Layer 5+: Roadside sprite frames (pooled, z-sorted by distance)
 *   Layer N: Vehicle frames (player + AI cars)
 *   Top: HUD frame (static overlay)
 *
 * Transparency: Frame.transparent = true means undefined cells show through.
 * We use this to layer sprites over background without overwriting.
 */

interface FrameLayer {
  frame: Frame;
  name: string;
  zIndex: number;
}

class FrameManager {
  private rootFrame: Frame;
  private layers: FrameLayer[];
  private width: number;
  private height: number;
  private horizonY: number;
  
  // Named layer references for quick access
  private skyGridFrame: Frame | null;
  private mountainsFrame: Frame | null;
  private sunFrame: Frame | null;
  private groundGridFrame: Frame | null;  // Holodeck grid floor
  private roadFrame: Frame | null;
  private hudFrame: Frame | null;
  
  // Sprite pools for roadside objects and vehicles
  private roadsidePool: Frame[];
  private vehicleFrames: Frame[];
  
  constructor(width: number, height: number, horizonY: number) {
    this.width = width;
    this.height = height;
    this.horizonY = horizonY;
    this.layers = [];
    this.roadsidePool = [];
    this.vehicleFrames = [];
    
    this.skyGridFrame = null;
    this.mountainsFrame = null;
    this.sunFrame = null;
    this.groundGridFrame = null;
    this.roadFrame = null;
    this.hudFrame = null;
    this.rootFrame = null as any;  // Will be set in init()
  }
  
  /**
   * Initialize all frames. Must be called after load("frame.js").
   */
  init(): void {
    // Root frame - full screen, black background
    this.rootFrame = new Frame(1, 1, this.width, this.height, BG_BLACK);
    this.rootFrame.open();
    
    // Sky grid layer (covers sky area, will be drawn with grid pattern)
    this.skyGridFrame = new Frame(1, 1, this.width, this.horizonY, BG_BLACK, this.rootFrame);
    this.skyGridFrame.open();
    this.addLayer(this.skyGridFrame, 'skyGrid', 1);
    
    // Sun layer (transparent, for celestial bodies - behind scenery)
    this.sunFrame = new Frame(1, 1, this.width, this.horizonY, BG_BLACK, this.rootFrame);
    this.sunFrame.transparent = true;
    this.sunFrame.open();
    this.addLayer(this.sunFrame, 'sun', 2);
    
    // Mountains layer (transparent, for parallax scrolling scenery - in front of celestials)
    this.mountainsFrame = new Frame(1, 1, this.width, this.horizonY, BG_BLACK, this.rootFrame);
    this.mountainsFrame.transparent = true;
    this.mountainsFrame.open();
    this.addLayer(this.mountainsFrame, 'mountains', 3);
    
    // Ground grid layer - holodeck floor effect (below road surface)
    // Covers same area as road, but renders the grid pattern
    var roadHeight = this.height - this.horizonY;
    this.groundGridFrame = new Frame(1, this.horizonY + 1, this.width, roadHeight, BG_BLACK, this.rootFrame);
    this.groundGridFrame.open();
    this.addLayer(this.groundGridFrame, 'groundGrid', 4);
    
    // Road layer (covers road area from horizon to bottom, transparent for off-road)
    this.roadFrame = new Frame(1, this.horizonY + 1, this.width, roadHeight, BG_BLACK, this.rootFrame);
    this.roadFrame.transparent = true;  // Road is transparent so ground grid shows through off-road
    this.roadFrame.open();
    this.addLayer(this.roadFrame, 'road', 5);
    
    // HUD layer (transparent overlay, full screen)
    this.hudFrame = new Frame(1, 1, this.width, this.height, BG_BLACK, this.rootFrame);
    this.hudFrame.transparent = true;
    this.hudFrame.open();
    this.addLayer(this.hudFrame, 'hud', 100);
    
    // Pre-create a pool of roadside sprite frames
    this.initRoadsidePool(20);  // 20 reusable sprite frames
    
    // Create vehicle frames (player + up to 7 AI)
    this.initVehicleFrames(8);
  }
  
  /**
   * Add a frame to our layer tracking.
   */
  private addLayer(frame: Frame, name: string, zIndex: number): void {
    this.layers.push({ frame: frame, name: name, zIndex: zIndex });
  }
  
  /**
   * Initialize pool of reusable roadside sprite frames.
   */
  private initRoadsidePool(count: number): void {
    for (var i = 0; i < count; i++) {
      // Max size for a roadside sprite: 8 wide x 6 tall
      var spriteFrame = new Frame(1, 1, 8, 6, BG_BLACK, this.rootFrame);
      spriteFrame.transparent = true;
      // Don't open yet - will open when assigned to a sprite
      this.roadsidePool.push(spriteFrame);
    }
  }
  
  /**
   * Initialize vehicle sprite frames.
   */
  private initVehicleFrames(count: number): void {
    for (var i = 0; i < count; i++) {
      // Vehicles are about 5 wide x 3 tall max
      var vehicleFrame = new Frame(1, 1, 7, 4, BG_BLACK, this.rootFrame);
      vehicleFrame.transparent = true;
      this.vehicleFrames.push(vehicleFrame);
    }
  }
  
  /**
   * Get the sky grid frame for rendering.
   */
  getSkyGridFrame(): Frame | null {
    return this.skyGridFrame;
  }
  
  /**
   * Get the mountains frame for parallax rendering.
   */
  getMountainsFrame(): Frame | null {
    return this.mountainsFrame;
  }
  
  /**
   * Get the sun frame.
   */
  getSunFrame(): Frame | null {
    return this.sunFrame;
  }
  
  /**
   * Get the road frame.
   */
  getRoadFrame(): Frame | null {
    return this.roadFrame;
  }
  
  /**
   * Get the ground grid frame (holodeck floor).
   */
  getGroundGridFrame(): Frame | null {
    return this.groundGridFrame;
  }
  
  /**
   * Get the HUD frame.
   */
  getHudFrame(): Frame | null {
    return this.hudFrame;
  }
  
  /**
   * Get a roadside sprite frame from the pool.
   * Returns null if pool is exhausted.
   */
  getRoadsideFrame(index: number): Frame | null {
    if (index >= 0 && index < this.roadsidePool.length) {
      return this.roadsidePool[index];
    }
    return null;
  }
  
  /**
   * Get a vehicle sprite frame.
   */
  getVehicleFrame(index: number): Frame | null {
    if (index >= 0 && index < this.vehicleFrames.length) {
      return this.vehicleFrames[index];
    }
    return null;
  }
  
  /**
   * Get the number of roadside frames in the pool.
   */
  getRoadsidePoolSize(): number {
    return this.roadsidePool.length;
  }
  
  /**
   * Position a roadside frame at screen coordinates.
   */
  positionRoadsideFrame(index: number, x: number, y: number, visible: boolean): void {
    var frame = this.roadsidePool[index];
    if (!frame) return;
    
    if (visible) {
      frame.moveTo(x, y);
      if (!frame.is_open) {
        frame.open();
      }
    } else {
      if (frame.is_open) {
        frame.close();
      }
    }
  }
  
  /**
   * Position a vehicle frame at screen coordinates.
   */
  positionVehicleFrame(index: number, x: number, y: number, visible: boolean): void {
    var frame = this.vehicleFrames[index];
    if (!frame) return;
    
    if (visible) {
      frame.moveTo(x, y);
      if (!frame.is_open) {
        frame.open();
      }
      frame.top();  // Bring to front
    } else {
      if (frame.is_open) {
        frame.close();
      }
    }
  }
  
  /**
   * Cycle all frames to update display.
   * Only changed cells are redrawn.
   */
  cycle(): void {
    // Cycling the root frame cycles all children
    this.rootFrame.cycle();
  }
  
  /**
   * Clear a specific frame.
   */
  clearFrame(frame: Frame | null): void {
    if (frame) {
      frame.clear();
    }
  }
  
  /**
   * Get the root frame for overlays.
   */
  getRootFrame(): Frame {
    return this.rootFrame;
  }

  /**
   * Shutdown - close all frames.
   */
  shutdown(): void {
    // Close in reverse z-order
    if (this.hudFrame) this.hudFrame.close();
    
    for (var i = 0; i < this.vehicleFrames.length; i++) {
      if (this.vehicleFrames[i].is_open) {
        this.vehicleFrames[i].close();
      }
    }
    
    for (var j = 0; j < this.roadsidePool.length; j++) {
      if (this.roadsidePool[j].is_open) {
        this.roadsidePool[j].close();
      }
    }
    
    if (this.roadFrame) this.roadFrame.close();
    if (this.sunFrame) this.sunFrame.close();
    if (this.mountainsFrame) this.mountainsFrame.close();
    if (this.skyGridFrame) this.skyGridFrame.close();
    
    this.rootFrame.close();
  }
}
