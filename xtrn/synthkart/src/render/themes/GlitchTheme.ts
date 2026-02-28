/**
 * GlitchTheme.ts - Corrupted reality racing through a broken simulation.
 * Visual glitches, color corruption, and unstable reality.
 */

// Glitch state - tracks active corruption effects
var GlitchState = {
  intensity: 0.3,           // Current glitch intensity (0-1)
  intensityTarget: 0.3,     // Target intensity (smoothly approaches)
  wavePhase: 0,             // For oscillating effects
  lastSpikeTime: 0,         // When last intensity spike happened
  colorCorruptPhase: 0,     // Which color corruption is active
  tearOffset: 0,            // Current horizontal tear amount
  noiseRows: [] as number[],// Which rows have scanline noise
  
  // Sky glitch state
  skyGlitchType: 0,         // 0=none, 1=matrix rain, 2=binary, 3=BSOD, 4=color invert
  skyGlitchTimer: 0,        // How long current sky glitch lasts
  matrixRainDrops: [] as { x: number, y: number, speed: number, char: string }[],
  
  // Road color glitch state
  roadColorGlitch: 0,       // 0=normal, 1=inverted, 2=rainbow, 3=flash, 4=corrupt
  roadGlitchTimer: 0,       // Duration of current road glitch
  roadsideColorShift: 0,    // Color offset for roadside sprites
  
  // Update glitch state each frame
  update: function(trackPosition: number, dt: number): void {
    // Oscillate base intensity
    this.wavePhase += dt * 2;
    var baseIntensity = 0.2 + Math.sin(this.wavePhase) * 0.1;
    
    // Random intensity spikes
    if (Math.random() < 0.02) {
      this.intensityTarget = 0.6 + Math.random() * 0.4;  // Spike to 0.6-1.0
      this.lastSpikeTime = trackPosition;
    }
    
    // Decay back to base
    if (this.intensityTarget > baseIntensity) {
      this.intensityTarget -= dt * 0.5;
    }
    
    // Smooth approach to target
    this.intensity += (this.intensityTarget - this.intensity) * dt * 5;
    
    // Update color corruption (cycles through phases)
    if (Math.random() < this.intensity * 0.1) {
      this.colorCorruptPhase = Math.floor(Math.random() * 6);
    }
    
    // Update horizontal tear
    if (Math.random() < this.intensity * 0.15) {
      this.tearOffset = Math.floor(Math.random() * 8) - 4;
    } else if (Math.random() < 0.3) {
      this.tearOffset = 0;  // Reset tear
    }
    
    // Update noise rows
    if (Math.random() < this.intensity * 0.2) {
      this.noiseRows = [];
      var numNoiseRows = Math.floor(Math.random() * 3 * this.intensity);
      for (var i = 0; i < numNoiseRows; i++) {
        this.noiseRows.push(Math.floor(Math.random() * 25));
      }
    } else if (Math.random() < 0.2) {
      this.noiseRows = [];
    }
    
    // Update sky glitch
    this.skyGlitchTimer -= dt;
    if (this.skyGlitchTimer <= 0) {
      // Chance to trigger new sky glitch based on intensity
      if (Math.random() < this.intensity * 0.08) {
        this.skyGlitchType = Math.floor(Math.random() * 5);  // 0-4
        this.skyGlitchTimer = 0.5 + Math.random() * 2.0;  // 0.5-2.5 seconds
        
        // Initialize matrix rain if that's the effect
        if (this.skyGlitchType === 1) {
          this.matrixRainDrops = [];
          for (var r = 0; r < 15; r++) {
            this.matrixRainDrops.push({
              x: Math.floor(Math.random() * 80),
              y: Math.floor(Math.random() * 8),
              speed: 0.5 + Math.random() * 1.5,
              char: String.fromCharCode(48 + Math.floor(Math.random() * 10))  // 0-9
            });
          }
        }
      } else {
        this.skyGlitchType = 0;  // No glitch
      }
    }
    
    // Update matrix rain positions
    if (this.skyGlitchType === 1) {
      for (var d = 0; d < this.matrixRainDrops.length; d++) {
        var drop = this.matrixRainDrops[d];
        drop.y += drop.speed * dt * 10;
        if (drop.y > 8) {
          drop.y = 0;
          drop.x = Math.floor(Math.random() * 80);
          drop.char = String.fromCharCode(48 + Math.floor(Math.random() * 10));
        }
        // Randomly change character
        if (Math.random() < 0.1) {
          drop.char = String.fromCharCode(48 + Math.floor(Math.random() * 10));
        }
      }
    }
    
    // Update road color glitch
    this.roadGlitchTimer -= dt;
    if (this.roadGlitchTimer <= 0) {
      // Chance to trigger new road color glitch
      if (Math.random() < this.intensity * 0.12) {
        this.roadColorGlitch = 1 + Math.floor(Math.random() * 4);  // 1-4
        this.roadGlitchTimer = 0.3 + Math.random() * 1.5;  // 0.3-1.8 seconds
      } else {
        this.roadColorGlitch = 0;  // Normal colors
      }
    }
    
    // Update roadside color shift (cycles through colors)
    if (Math.random() < this.intensity * 0.15) {
      this.roadsideColorShift = Math.floor(Math.random() * 8);  // 0-7 color offset
    } else if (Math.random() < 0.1) {
      this.roadsideColorShift = 0;  // Reset
    }
  },
  
  // Get a corrupted color based on current glitch state
  corruptColor: function(originalFg: number, originalBg: number): { fg: number, bg: number } {
    if (Math.random() > this.intensity * 0.5) {
      return { fg: originalFg, bg: originalBg };
    }
    
    switch(this.colorCorruptPhase) {
      case 0: // Invert
        return { fg: 15 - originalFg, bg: originalBg };
      case 1: // Swap fg/bg
        return { fg: originalBg & 0x07, bg: (originalFg << 4) & 0x70 };
      case 2: // Shift hue
        return { fg: (originalFg + 8) % 16, bg: originalBg };
      case 3: // Monochrome green
        return { fg: LIGHTGREEN, bg: BG_BLACK };
      case 4: // Monochrome cyan (CRT)
        return { fg: LIGHTCYAN, bg: BG_BLACK };
      case 5: // Red alert
        return { fg: LIGHTRED, bg: BG_BLACK };
      default:
        return { fg: originalFg, bg: originalBg };
    }
  },
  
  // Get a glitched character replacement
  corruptChar: function(original: string): string {
    if (Math.random() > this.intensity * 0.3) {
      return original;
    }
    
    var glitchChars = [
      GLYPH.FULL_BLOCK, GLYPH.DARK_SHADE, GLYPH.MEDIUM_SHADE, GLYPH.LIGHT_SHADE,
      '#', '@', '%', '&', '$', '!', '?', '/', '\\', '|',
      GLYPH.BOX_H, GLYPH.BOX_V, GLYPH.BOX_TR, GLYPH.BOX_TL,
      '0', '1', 'X', 'x', '>', '<', '^', 'v'
    ];
    
    return glitchChars[Math.floor(Math.random() * glitchChars.length)];
  },
  
  // Check if a row should have scanline noise
  isNoiseRow: function(row: number): boolean {
    for (var i = 0; i < this.noiseRows.length; i++) {
      if (this.noiseRows[i] === row) return true;
    }
    return false;
  },
  
  // Get noise character for scanline
  getNoiseChar: function(): string {
    var noiseChars = [GLYPH.LIGHT_SHADE, GLYPH.MEDIUM_SHADE, GLYPH.DARK_SHADE, ' ', '#', '%'];
    return noiseChars[Math.floor(Math.random() * noiseChars.length)];
  },
  
  // Get horizontal tear offset for a row
  getTearOffset: function(row: number): number {
    // Only tear middle section of screen
    if (row > 8 && row < 18 && Math.abs(this.tearOffset) > 0) {
      return this.tearOffset;
    }
    return 0;
  },
  
  // Get glitched road colors based on current road glitch state
  getGlitchedRoadColor: function(baseFg: number, baseBg: number, distance: number): { fg: number, bg: number } {
    if (this.roadColorGlitch === 0) {
      return { fg: baseFg, bg: baseBg };
    }
    
    switch (this.roadColorGlitch) {
      case 1:  // Inverted - swap dark/light
        return { fg: 15 - baseFg, bg: ((baseBg >> 4) ^ 0x07) << 4 };
      case 2:  // Rainbow - color cycles based on distance
        var rainbowColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
        var colorIndex = Math.floor(distance + (this.wavePhase * 3)) % rainbowColors.length;
        return { fg: rainbowColors[colorIndex], bg: BG_BLACK };
      case 3:  // Flash - alternates rapidly
        var flash = Math.floor(Date.now() / 50) % 2;
        return flash === 0 ? { fg: WHITE, bg: BG_BLACK } : { fg: BLACK, bg: BG_LIGHTGRAY };
      case 4:  // Corrupt - random per-pixel corruption
        if (Math.random() < 0.3) {
          return { fg: Math.floor(Math.random() * 16), bg: [BG_BLACK, BG_GREEN, BG_CYAN, BG_RED][Math.floor(Math.random() * 4)] };
        }
        return { fg: baseFg, bg: baseBg };
      default:
        return { fg: baseFg, bg: baseBg };
    }
  },
  
  // Get glitched roadside sprite color
  getGlitchedSpriteColor: function(baseFg: number, baseBg: number): { fg: number, bg: number } {
    if (this.roadsideColorShift === 0 || Math.random() > this.intensity * 0.7) {
      return { fg: baseFg, bg: baseBg };
    }
    
    // Shift the foreground color by the offset
    var shiftedFg = (baseFg + this.roadsideColorShift) % 16;
    // Occasionally corrupt the background too
    var shiftedBg = baseBg;
    if (Math.random() < 0.2) {
      shiftedBg = [BG_BLACK, BG_GREEN, BG_BLUE, BG_CYAN][Math.floor(Math.random() * 4)];
    }
    return { fg: shiftedFg, bg: shiftedBg };
  }
};

var GlitchTheme: Theme = {
  name: 'glitch_circuit',
  description: 'Reality is corrupted. The simulation is breaking down.',
  
  colors: {
    // Base colors - digital/matrix feel that gets corrupted
    skyTop: { fg: BLACK, bg: BG_BLACK },
    skyMid: { fg: DARKGRAY, bg: BG_BLACK },
    skyHorizon: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // Matrix-style grid
    skyGrid: { fg: GREEN, bg: BG_BLACK },
    skyGridGlow: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // Glitchy sun (fluctuates)
    celestialCore: { fg: WHITE, bg: BG_GREEN },
    celestialGlow: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // Flickering stars
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: GREEN, bg: BG_BLACK },
    
    // Digital mountains/scenery
    sceneryPrimary: { fg: GREEN, bg: BG_BLACK },
    scenerySecondary: { fg: LIGHTGREEN, bg: BG_BLACK },
    sceneryTertiary: { fg: WHITE, bg: BG_BLACK },
    
    // Neon road
    roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
    roadSurfaceAlt: { fg: GREEN, bg: BG_BLACK },
    roadStripe: { fg: LIGHTGREEN, bg: BG_BLACK },
    roadEdge: { fg: LIGHTCYAN, bg: BG_BLACK },
    roadGrid: { fg: GREEN, bg: BG_BLACK },
    
    // Digital void shoulders
    shoulderPrimary: { fg: GREEN, bg: BG_BLACK },
    shoulderSecondary: { fg: DARKGRAY, bg: BG_BLACK }
  },
  
  // Grid sky (matrix style)
  sky: {
    type: 'grid',
    gridDensity: 8,
    converging: true,
    horizontal: true
  },
  
  // Abstract digital mountains
  background: {
    type: 'mountains',
    config: {
      count: 5,
      minHeight: 3,
      maxHeight: 7,
      parallaxSpeed: 0.08
    }
  },
  
  // Glitchy sun
  celestial: {
    type: 'sun',
    size: 3,
    positionX: 0.5,
    positionY: 0.4
  },
  
  // Flickering digital stars
  stars: {
    enabled: true,
    density: 0.3,
    twinkle: true
  },
  
  // Void/grid ground
  ground: {
    type: 'void',
    primary: { fg: GREEN, bg: BG_BLACK },
    secondary: { fg: LIGHTGREEN, bg: BG_BLACK },
    pattern: {
      gridSpacing: 4,
      gridChar: '+'
    }
  },
  
  // Mixed chaotic roadside - objects from ALL themes!
  roadside: {
    pool: [
      // City
      { sprite: 'building', weight: 1, side: 'both' },
      { sprite: 'lamppost', weight: 1, side: 'both' },
      // Beach
      { sprite: 'palm', weight: 1, side: 'both' },
      { sprite: 'umbrella', weight: 1, side: 'both' },
      // Forest
      { sprite: 'tree', weight: 1, side: 'both' },
      // Horror
      { sprite: 'gravestone', weight: 1, side: 'both' },
      { sprite: 'deadtree', weight: 1, side: 'both' },
      { sprite: 'pumpkin', weight: 1, side: 'both' },
      // Winter
      { sprite: 'snowpine', weight: 1, side: 'both' },
      { sprite: 'snowman', weight: 1, side: 'both' },
      // Desert
      { sprite: 'saguaro', weight: 1, side: 'both' },
      { sprite: 'cowskull', weight: 1, side: 'both' },
      // Jungle
      { sprite: 'jungle_tree', weight: 1, side: 'both' },
      { sprite: 'fern', weight: 1, side: 'both' },
      // Candy
      { sprite: 'lollipop', weight: 1, side: 'both' },
      { sprite: 'candy_cane', weight: 1, side: 'both' },
      { sprite: 'gummy_bear', weight: 1, side: 'both' },
      // Space
      { sprite: 'planet', weight: 1, side: 'both' },
      { sprite: 'satellite', weight: 1, side: 'both' },
      // Castle
      { sprite: 'torch', weight: 1, side: 'both' },
      { sprite: 'tower', weight: 1, side: 'both' },
      // Villain
      { sprite: 'flame', weight: 1, side: 'both' },
      { sprite: 'skull_pile', weight: 1, side: 'both' },
      // Ruins
      { sprite: 'column', weight: 1, side: 'both' },
      { sprite: 'obelisk', weight: 1, side: 'both' },
      { sprite: 'sphinx', weight: 1, side: 'both' },
      // Stadium
      { sprite: 'tire_stack', weight: 1, side: 'both' },
      { sprite: 'hay_bale', weight: 1, side: 'both' }
    ],
    spacing: 35,
    density: 1.2
  }
};

registerTheme(GlitchTheme);
