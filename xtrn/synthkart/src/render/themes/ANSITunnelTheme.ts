/**
 * ANSITunnelTheme - A surreal theme that renders ANSI art files as the road surface
 * and sky reflection, creating a "cyberspace tunnel" effect.
 * 
 * The theme loads ANSI art files from a directory and:
 * - Renders ANSI content as the road surface with perspective distortion
 * - Mirrors/reflects the ANSI in the sky area for a tunnel effect
 * - Scrolls through the ANSI based on track position
 * - Loops seamlessly when reaching the end
 */

// Configuration for ANSI loading
// Track math: Data Highway = 35 segments × 200 = 7000 units/lap × 0.75 scroll = 5250 rows/lap
// 2000 rows = ~38% of a lap (repeats ~3x per lap vs 10+ times with old 6-file limit)
// Configuration is now loaded from outrun.ini
function getANSITunnelConfig() {
  return {
    directory: OUTRUN_CONFIG.ansiTunnel.directory,
    scrollSpeed: OUTRUN_CONFIG.ansiTunnel.scrollSpeed,
    mirrorSky: true,
    colorShift: true,
    maxRowsToLoad: OUTRUN_CONFIG.ansiTunnel.maxRows
  };
}

var ANSITunnelConfig = getANSITunnelConfig();

var ANSITunnelTheme: Theme = {
  name: 'ansi_tunnel',
  description: 'Race through scrolling ANSI art - a digital cyberspace tunnel',
  
  colors: {
    // Dark cyber sky
    skyTop: { fg: BLACK, bg: BG_BLACK },
    skyMid: { fg: DARKGRAY, bg: BG_BLACK },
    skyHorizon: { fg: CYAN, bg: BG_BLACK },
    
    // Cyan grid lines
    skyGrid: { fg: CYAN, bg: BG_BLACK },
    skyGridGlow: { fg: LIGHTCYAN, bg: BG_BLACK },
    
    // No celestial (replaced by ANSI)
    celestialCore: { fg: WHITE, bg: BG_BLACK },
    celestialGlow: { fg: CYAN, bg: BG_BLACK },
    
    // Digital "stars" (pixels)
    starBright: { fg: LIGHTCYAN, bg: BG_BLACK },
    starDim: { fg: CYAN, bg: BG_BLACK },
    
    // Cyber scenery
    sceneryPrimary: { fg: CYAN, bg: BG_BLACK },
    scenerySecondary: { fg: LIGHTCYAN, bg: BG_BLACK },
    sceneryTertiary: { fg: WHITE, bg: BG_BLACK },
    
    // Dark road with cyan accents
    roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
    roadSurfaceAlt: { fg: BLACK, bg: BG_BLACK },
    roadStripe: { fg: LIGHTCYAN, bg: BG_BLACK },
    roadEdge: { fg: CYAN, bg: BG_BLACK },
    roadGrid: { fg: DARKGRAY, bg: BG_BLACK },
    
    // Cyber shoulders
    shoulderPrimary: { fg: DARKGRAY, bg: BG_BLACK },
    shoulderSecondary: { fg: BLACK, bg: BG_BLACK },
    
    // Roadside object colors
    roadsideColors: {
      'data_beacon': {
        primary: { fg: LIGHTCYAN, bg: BG_BLACK },
        secondary: { fg: CYAN, bg: BG_BLACK }
      },
      'data_node': {
        primary: { fg: CYAN, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK }
      },
      'signal_pole': {
        primary: { fg: LIGHTCYAN, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK }
      },
      'binary_pillar': {
        primary: { fg: LIGHTCYAN, bg: BG_BLACK },
        secondary: { fg: CYAN, bg: BG_BLACK }
      }
    },
    
    // Item boxes - digital theme
    itemBox: {
      border: { fg: LIGHTCYAN, bg: BG_BLACK },
      fill: { fg: DARKGRAY, bg: BG_BLACK },
      symbol: { fg: WHITE, bg: BG_BLACK }
    }
  },
  
  // Sky handled by ANSI renderer
  sky: {
    type: 'ansi',  // Special type for ANSI rendering
    converging: false,
    horizontal: false
  },
  
  // Background handled by ANSI renderer
  background: {
    type: 'ansi',
    config: {
      parallaxSpeed: 0.0  // No parallax - ANSI scrolls with track
    }
  },
  
  // No celestial body
  celestial: {
    type: 'none',
    size: 0,
    positionX: 0.5,
    positionY: 0.5
  },
  
  // Sparse digital stars
  stars: {
    enabled: true,
    density: 0.1,
    twinkle: true
  },
  
  // Dark cyber ground
  ground: {
    type: 'solid',
    primary: { fg: BLACK, bg: BG_BLACK },
    secondary: { fg: DARKGRAY, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.1,
      ditherChars: ['.', GLYPH.LIGHT_SHADE]
    }
  },
  
  // Minimal roadside - let ANSI dominate
  roadside: {
    pool: [
      { sprite: 'data_beacon', weight: 3, side: 'both' },
      { sprite: 'data_node', weight: 2, side: 'both' },
      { sprite: 'signal_pole', weight: 2, side: 'both' },
      { sprite: 'binary_pillar', weight: 3, side: 'both' }
    ],
    spacing: 60,  // Sparse - let ANSI show through
    density: 0.5
  },
  
  // Special HUD overrides for retro-modem theme
  hud: {
    speedLabel: 'Kbps',
    speedMultiplier: 0.24,  // 120 mph -> 28.8 Kbps
    positionPrefix: 'Node ',
    lapLabel: 'SECTOR',
    timeLabel: 'CONNECT'
  }
};

registerTheme(ANSITunnelTheme);

/**
 * ANSI Tunnel renderer - handles the perspective road surface and sky reflection.
 * Pre-loads a fixed set of ANSI files at startup and loops through them seamlessly.
 */
class ANSITunnelRenderer {
  // Combined ANSI canvas (files concatenated vertically)
  private combinedCells: ANSICell[][] = [];
  private combinedWidth: number = 80;
  private combinedHeight: number = 0;
  private scrollOffset: number = 0;
  private loaded: boolean = false;
  private _renderDebugLogged: boolean = false;
  
  // Characters that cause beeps or display issues - filter these out
  private static CONTROL_CHARS: { [key: number]: boolean } = {
    0: true,   // NUL
    7: true,   // BEL (beep!)
    8: true,   // BS
    9: true,   // TAB
    10: true,  // LF
    12: true,  // FF
    13: true,  // CR
    27: true   // ESC
  };
  
  constructor() {
    this.loadANSIFiles();
  }
  
  /**
   * Load ANSI files at startup and concatenate into one canvas.
   */
  private loadANSIFiles(): void {
    var files = ANSILoader.scanDirectory(ANSITunnelConfig.directory);
    
    if (files.length === 0) {
      logWarning("ANSITunnelRenderer: No ANSI files found in " + ANSITunnelConfig.directory);
      return;
    }
    
    // Shuffle randomly for variety each race
    for (var i = files.length - 1; i > 0; i--) {
      var j = Math.floor(Math.random() * (i + 1));
      var temp = files[i];
      files[i] = files[j];
      files[j] = temp;
    }
    
    // Load files until we hit row limit (not file count)
    var maxRows = ANSITunnelConfig.maxRowsToLoad || 2000;
    var filesLoaded = 0;
    
    logInfo("ANSITunnelRenderer: Loading ANSI files (max " + maxRows + " rows)...");
    
    // Load and concatenate until we hit the row limit
    this.combinedCells = [];
    this.combinedHeight = 0;
    
    for (var i = 0; i < files.length && this.combinedHeight < maxRows; i++) {
      var img = ANSILoader.load(files[i]);
      if (img && img.height > 0) {
        // Check if adding this file would exceed limit
        var rowsToAdd = Math.min(img.height, maxRows - this.combinedHeight);
        
        for (var row = 0; row < rowsToAdd; row++) {
          var newRow: ANSICell[] = [];
          for (var col = 0; col < this.combinedWidth; col++) {
            if (col < img.width && img.cells[row] && img.cells[row][col]) {
              var cell = img.cells[row][col];
              var ch = cell.char;
              var code = ch.charCodeAt(0);
              if (ANSITunnelRenderer.CONTROL_CHARS[code]) {
                ch = ' ';
              }
              newRow.push({ char: ch, attr: cell.attr });
            } else {
              newRow.push({ char: ' ', attr: 7 });
            }
          }
          this.combinedCells.push(newRow);
          this.combinedHeight++;
        }
        filesLoaded++;
        logInfo("ANSITunnelRenderer: Loaded " + files[i].split('/').pop() + " (" + img.height + " rows, total: " + this.combinedHeight + ")");
      }
    }
    
    this.loaded = this.combinedHeight > 0;
    var estimatedKB = Math.round(this.combinedHeight * this.combinedWidth * 10 / 1024); // ~10 bytes per cell estimate
    logInfo("ANSITunnelRenderer: Loaded " + filesLoaded + "/" + files.length + " files, " + this.combinedHeight + " rows (~" + estimatedKB + "KB)");
  }
  
  /**
   * Get a cell from the combined canvas at the given coordinates.
   * Wraps vertically for seamless looping.
   */
  getCell(row: number, col: number): ANSICell {
    if (!this.loaded || this.combinedHeight === 0) {
      return { char: ' ', attr: 7 };
    }
    // Wrap row for seamless looping
    var wrappedRow = Math.floor(row) % this.combinedHeight;
    if (wrappedRow < 0) wrappedRow += this.combinedHeight;
    // Clamp column
    var clampedCol = Math.max(0, Math.min(Math.floor(col), this.combinedWidth - 1));
    
    if (this.combinedCells[wrappedRow] && this.combinedCells[wrappedRow][clampedCol]) {
      return this.combinedCells[wrappedRow][clampedCol];
    }
    return { char: ' ', attr: 7 };
  }
  
  /**
   * Update scroll position based on track progress.
   */
  updateScroll(trackZ: number, _trackLength: number): void {
    if (!this.loaded || this.combinedHeight === 0) return;
    
    var scrollMultiplier = 0.5;
    this.scrollOffset = (trackZ * scrollMultiplier) % this.combinedHeight;
    if (this.scrollOffset < 0) this.scrollOffset += this.combinedHeight;
  }
  
  /**
   * Render the ANSI tunnel effect using separate frames for sky and road.
   * Displays 24 continuous rows of ANSI - just like opening it in a viewer.
   * 
   * NEW: Road surface is black with white dividers, ANSI shows only on roadsides.
   */
  renderTunnel(skyFrame: Frame | null, roadFrame: Frame | null, horizonY: number, roadHeight: number, screenWidth: number, trackPosition: number, cameraX: number, road: Road, roadLength: number): void {
    // Debug: log once per session
    if (!this._renderDebugLogged) {
      this._renderDebugLogged = true;
      logInfo('ANSITunnelRenderer.renderTunnel: canvas=' + this.combinedWidth + 'x' + this.combinedHeight + ' horizonY=' + horizonY + ' roadHeight=' + roadHeight);
    }
    
    if (!this.loaded || this.combinedHeight === 0) {
      this.renderFallback(skyFrame, roadFrame, horizonY, roadHeight, screenWidth);
      return;
    }
    
    // Reverse scroll: show bottom of ANSI first, scroll upward through it
    // This makes the car feel like it's driving "up" through the ANSI
    // - startRow is the ANSI row that appears at the top of our 24-row window
    // - As scrollOffset increases, startRow decreases, revealing earlier ANSI rows at top
    // - Content shifts DOWN on screen, like driving forward into the image
    var startRow = (this.combinedHeight - 1) - Math.floor(this.scrollOffset);
    
    // Render sky (8 rows starting from startRow)
    if (skyFrame) {
      for (var frameY = 0; frameY < horizonY; frameY++) {
        var ansiRow = startRow + frameY;
        for (var frameX = 0; frameX < screenWidth; frameX++) {
          var cell = this.getCell(ansiRow, frameX);
          skyFrame.setData(frameX, frameY, cell.char, cell.attr);
        }
      }
    }
    
    // Render road (16 rows continuing from where sky left off)
    // Road surface is black, ANSI shows only on roadsides
    if (roadFrame) {
      var roadBottom = roadHeight - 1;
      var blackAttr = makeAttr(BLACK, BG_BLACK);
      var accumulatedCurve = 0;
      
      // Iterate from bottom to top (like renderANSIRoadStripes) to accumulate curves correctly
      for (var screenY = roadBottom; screenY >= 0; screenY--) {
        var ansiRow = startRow + horizonY + screenY;
        
        // Calculate perspective road boundaries for this scanline
        // This MUST match the logic in renderANSIRoadStripes for alignment
        var t = (roadBottom - screenY) / Math.max(1, roadBottom);
        var distance = 1 / (1 - t * 0.95);
        
        // Get road segment and accumulate curvature
        var worldZ = trackPosition + distance * 5;
        var segment = road.getSegment(worldZ);
        if (segment) {
          accumulatedCurve += segment.curve * 0.5;
        }
        
        var roadWidth = Math.round(40 / distance);
        var halfWidth = Math.floor(roadWidth / 2);
        
        // Apply curve offset and camera position (matches renderANSIRoadStripes exactly)
        var curveOffset = accumulatedCurve * distance * 0.8;
        var centerX = 40 + Math.round(curveOffset) - Math.round(cameraX * 0.5);
        
        var leftEdge = centerX - halfWidth;
        var rightEdge = centerX + halfWidth;
        
        // Check if this is the finish line
        var wrappedZ = worldZ % roadLength;
        if (wrappedZ < 0) wrappedZ += roadLength;
        var isFinishLine = (wrappedZ < 200) || (wrappedZ > roadLength - 200);
        
        for (var frameX = 0; frameX < screenWidth; frameX++) {
          // Check if this pixel is on the road surface
          var onRoad = frameX >= leftEdge && frameX <= rightEdge;
          
          if (onRoad) {
            if (isFinishLine) {
              // Checkered finish line pattern
              var checkerSize = Math.max(1, Math.floor(3 / distance));
              var checkerX = Math.floor((frameX - centerX) / checkerSize);
              var checkerY = Math.floor(screenY / 2);
              var isWhite = ((checkerX + checkerY) % 2) === 0;
              
              if (isWhite) {
                roadFrame.setData(frameX, screenY, GLYPH.FULL_BLOCK, makeAttr(WHITE, BG_LIGHTGRAY));
              } else {
                roadFrame.setData(frameX, screenY, ' ', makeAttr(BLACK, BG_BLACK));
              }
            } else {
              // Road surface: black background
              roadFrame.setData(frameX, screenY, ' ', blackAttr);
            }
          } else {
            // Roadside: show ANSI art
            var cell = this.getCell(ansiRow, frameX);
            roadFrame.setData(frameX, screenY, cell.char, cell.attr);
          }
        }
      }
    }
  }
  
  /**
   * Fallback rendering when no ANSI is loaded.
   */
  private renderFallback(skyFrame: Frame | null, roadFrame: Frame | null, horizonY: number, roadHeight: number, screenWidth: number): void {
    // Simple cyber grid fallback
    var gridAttr = makeAttr(DARKGRAY, BG_BLACK);
    
    // Sky - simple gradient
    if (skyFrame) {
      for (var y = 0; y < horizonY; y++) {
        var attr = y < 2 ? makeAttr(BLACK, BG_BLACK) : makeAttr(DARKGRAY, BG_BLACK);
        for (var x = 0; x < screenWidth; x++) {
          skyFrame.setData(x, y, ' ', attr);
        }
      }
    }
    
    // Road - simple lines
    if (roadFrame) {
      for (var y = 0; y < roadHeight; y++) {
        for (var x = 0; x < screenWidth; x++) {
          var ch = (y + x) % 4 === 0 ? '.' : ' ';
          roadFrame.setData(x, y, ch, gridAttr);
        }
      }
    }
  }
  
  /**
   * Check if ANSI files are loaded.
   */
  isLoaded(): boolean {
    return this.loaded;
  }
}

// Singleton instance
var ansiTunnelRenderer: ANSITunnelRenderer | null = null;

function getANSITunnelRenderer(): ANSITunnelRenderer {
  if (!ansiTunnelRenderer) {
    ansiTunnelRenderer = new ANSITunnelRenderer();
  }
  return ansiTunnelRenderer;
}
