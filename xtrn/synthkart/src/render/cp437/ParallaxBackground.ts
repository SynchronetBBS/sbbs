/**
 * ParallaxBackground - Wide scrolling background using Frame for efficient parallax.
 *
 * Classic Super Scaler technique:
 * - Background is drawn once into a wide buffer (2-3x screen width)
 * - Horizontal scrolling creates parallax effect when steering
 * - Background does NOT move forward (infinite distance)
 * - Multiple layers can scroll at different speeds
 */

class ParallaxBackground {
  private width: number;          // Visible width (80)
  private height: number;         // Sky height (horizon Y)
  private bufferWidth: number;    // Wide buffer (240 = 3x screen)
  
  // Pre-rendered background buffer (wider than screen)
  private skyBuffer: RenderCell[][];
  private mountainBuffer: RenderCell[][];
  
  // Current scroll offset (accumulated from steering/curves)
  private scrollOffset: number;
  
  // Parallax speed (mountains scroll slower than foreground)
  private readonly MOUNTAIN_PARALLAX: number = 0.3;  // 30% of steering
  
  constructor(visibleWidth: number, horizonY: number) {
    this.width = visibleWidth;
    this.height = horizonY;
    this.bufferWidth = visibleWidth * 3;  // 240 chars wide
    // Start centered so sun is visible (buffer center = width offset)
    this.scrollOffset = this.width;  // Start at center of buffer
    
    this.skyBuffer = [];
    this.mountainBuffer = [];
    
    // Pre-render the background layers
    this.renderBackgroundLayers();
  }
  
  /**
   * Pre-render background layers into wide buffers.
   * Called once at init, or when theme changes.
   */
  renderBackgroundLayers(): void {
    // Initialize buffers
    this.skyBuffer = [];
    this.mountainBuffer = [];
    
    for (var y = 0; y < this.height; y++) {
      var skyRow: RenderCell[] = [];
      var mtRow: RenderCell[] = [];
      for (var x = 0; x < this.bufferWidth; x++) {
        skyRow.push({ char: ' ', attr: makeAttr(BLACK, BG_BLACK) });
        mtRow.push({ char: ' ', attr: makeAttr(BLACK, BG_BLACK) });  // Transparent
      }
      this.skyBuffer.push(skyRow);
      this.mountainBuffer.push(mtRow);
    }
    
    // Render sun (centered in buffer, doesn't scroll much)
    this.renderSunToBuffer();
    
    // Render mountains across the wide buffer (they tile/repeat)
    this.renderMountainsToBuffer();
  }
  
  /**
   * Render the sun into the sky buffer.
   */
  private renderSunToBuffer(): void {
    var sunCoreAttr = makeAttr(YELLOW, BG_RED);
    var sunGlowAttr = makeAttr(LIGHTRED, BG_BLACK);
    
    // Sun is centered in the buffer (should appear in initial view)
    var sunCenterX = Math.floor(this.bufferWidth / 2);
    var sunY = this.height - 5;  // Near horizon but visible
    
    // Sun core - solid 5x3 block using full blocks
    for (var dy = -1; dy <= 1; dy++) {
      for (var dx = -2; dx <= 2; dx++) {
        var y = sunY + dy;
        var x = sunCenterX + dx;
        if (y >= 0 && y < this.height && x >= 0 && x < this.bufferWidth) {
          this.skyBuffer[y][x] = { char: GLYPH.FULL_BLOCK, attr: sunCoreAttr };
        }
      }
    }
    
    // Sun glow around edges using shading
    var glowOffsets = [
      { dx: -3, dy: -1 }, { dx: -3, dy: 0 }, { dx: -3, dy: 1 },
      { dx: 3, dy: -1 }, { dx: 3, dy: 0 }, { dx: 3, dy: 1 },
      { dx: -2, dy: -2 }, { dx: -1, dy: -2 }, { dx: 0, dy: -2 }, { dx: 1, dy: -2 }, { dx: 2, dy: -2 },
      { dx: -2, dy: 2 }, { dx: -1, dy: 2 }, { dx: 0, dy: 2 }, { dx: 1, dy: 2 }, { dx: 2, dy: 2 }
    ];
    
    for (var i = 0; i < glowOffsets.length; i++) {
      var g = glowOffsets[i];
      var y = sunY + g.dy;
      var x = sunCenterX + g.dx;
      if (y >= 0 && y < this.height && x >= 0 && x < this.bufferWidth) {
        this.skyBuffer[y][x] = { char: GLYPH.DARK_SHADE, attr: sunGlowAttr };
      }
    }
  }
  
  /**
   * Render mountains into the mountain buffer (tiled across width).
   */
  private renderMountainsToBuffer(): void {
    var mountainAttr = colorToAttr(PALETTE.MOUNTAIN);
    var highlightAttr = colorToAttr(PALETTE.MOUNTAIN_HIGHLIGHT);
    
    // Mountain definitions - these tile across the buffer
    var mountainPattern = [
      { xOffset: 0, height: 4, width: 12 },
      { xOffset: 15, height: 6, width: 16 },
      { xOffset: 35, height: 3, width: 10 },
      { xOffset: 50, height: 5, width: 14 },
      { xOffset: 68, height: 4, width: 11 }
    ];
    
    var patternWidth = 80;  // Pattern repeats every 80 chars
    var mountainBaseY = this.height - 1;
    
    // Tile the pattern across the buffer
    for (var tileX = 0; tileX < this.bufferWidth; tileX += patternWidth) {
      for (var m = 0; m < mountainPattern.length; m++) {
        var mt = mountainPattern[m];
        var baseX = tileX + mt.xOffset;
        this.drawMountainToBuffer(baseX, mountainBaseY, mt.height, mt.width, mountainAttr, highlightAttr);
      }
    }
  }
  
  /**
   * Draw a mountain into the mountain buffer.
   */
  private drawMountainToBuffer(baseX: number, baseY: number, height: number, width: number, attr: number, highlightAttr: number): void {
    var peakX = baseX + Math.floor(width / 2);
    
    for (var h = 0; h < height; h++) {
      var y = baseY - h;
      if (y < 0 || y >= this.height) continue;
      
      var halfWidth = Math.floor((height - h) * width / height / 2);
      
      // Left slope
      for (var dx = -halfWidth; dx < 0; dx++) {
        var x = peakX + dx;
        if (x >= 0 && x < this.bufferWidth) {
          this.mountainBuffer[y][x] = { char: '/', attr: attr };
        }
      }
      
      // Peak or center
      if (peakX >= 0 && peakX < this.bufferWidth) {
        if (h === height - 1) {
          this.mountainBuffer[y][peakX] = { char: GLYPH.TRIANGLE_UP, attr: highlightAttr };
        } else {
          this.mountainBuffer[y][peakX] = { char: GLYPH.BOX_V, attr: attr };
        }
      }
      
      // Right slope
      for (var dx = 1; dx <= halfWidth; dx++) {
        var x = peakX + dx;
        if (x >= 0 && x < this.bufferWidth) {
          this.mountainBuffer[y][x] = { char: '\\', attr: attr };
        }
      }
    }
  }
  
  /**
   * Update scroll offset based on road curvature and steering.
   * Called every frame with the current curve value.
   *
   * @param curvature Current road curvature (-1 to 1)
   * @param playerSteer Player's steering input (-1 to 1)
   * @param speed Current speed (affects how fast parallax scrolls)
   * @param dt Delta time
   */
  updateScroll(curvature: number, playerSteer: number, speed: number, dt: number): void {
    // Parallax is driven by road curves and player steering
    // When road curves right, background scrolls left (and vice versa)
    // Player steering adds a small amount too
    
    var scrollSpeed = (curvature * 0.8 + playerSteer * 0.2) * speed * dt * 0.1;
    this.scrollOffset += scrollSpeed;
    
    // Wrap scroll offset to prevent overflow
    if (this.scrollOffset < 0) {
      this.scrollOffset += this.bufferWidth;
    } else if (this.scrollOffset >= this.bufferWidth) {
      this.scrollOffset -= this.bufferWidth;
    }
  }
  
  /**
   * Render the parallax background to the composer.
   * Copies the visible portion of the wide buffer based on scroll offset.
   */
  render(composer: SceneComposer): void {
    // Calculate scroll positions for each layer
    var mountainScroll = Math.floor(this.scrollOffset * this.MOUNTAIN_PARALLAX) % this.bufferWidth;
    var skyScroll = Math.floor(this.scrollOffset * 0.1) % this.bufferWidth;  // Sun barely moves
    
    // Render sky/sun layer first
    for (var y = 0; y < this.height; y++) {
      for (var x = 0; x < this.width; x++) {
        var srcX = (x + skyScroll) % this.bufferWidth;
        var cell = this.skyBuffer[y][srcX];
        
        // Only draw non-empty cells
        if (cell.char !== ' ') {
          composer.setCell(x, y, cell.char, cell.attr);
        }
      }
    }
    
    // Render mountain layer on top (with transparency)
    for (var y = 0; y < this.height; y++) {
      for (var x = 0; x < this.width; x++) {
        var srcX = (x + mountainScroll) % this.bufferWidth;
        var cell = this.mountainBuffer[y][srcX];
        
        // Only draw non-empty mountain cells (transparent otherwise)
        if (cell.char !== ' ') {
          composer.setCell(x, y, cell.char, cell.attr);
        }
      }
    }
  }
  
  /**
   * Get current scroll offset (for debugging).
   */
  getScrollOffset(): number {
    return this.scrollOffset;
  }
  
  /**
   * Reset scroll to center position (so sun is visible).
   */
  resetScroll(): void {
    this.scrollOffset = this.width;  // Center of buffer
  }
}
