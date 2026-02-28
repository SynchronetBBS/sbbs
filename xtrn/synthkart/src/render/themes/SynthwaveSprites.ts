/**
 * SynthwaveSprites - Roadside sprites for synthwave/neon/holodeck themes.
 * 
 * Includes: neon_pillar, grid_pylon, holo_billboard, neon_palm
 * 
 * These fit the futuristic synthwave aesthetic much better than
 * natural trees/rocks. The old natural objects are preserved in
 * RoadRenderer.ts for themes that need them.
 */

var SynthwaveSprites = {
  /**
   * Create a neon pillar - glowing vertical structure.
   * Cyan/magenta neon tubes.
   */
  createNeonPillar: function(): SpriteDefinition {
    var neonCyan = makeAttr(LIGHTCYAN, BG_BLACK);
    var neonMagenta = makeAttr(LIGHTMAGENTA, BG_BLACK);
    var glowCyan = makeAttr(CYAN, BG_BLACK);
    var glowMagenta = makeAttr(MAGENTA, BG_BLACK);
    var base = makeAttr(BLUE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'neon_pillar',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: GLYPH.BOX_V, attr: neonCyan }]
        ],
        // Scale 1: 1x2
        [
          [{ char: GLYPH.BOX_V, attr: neonCyan }],
          [{ char: GLYPH.BOX_V, attr: neonMagenta }]
        ],
        // Scale 2: 2x3
        [
          [{ char: GLYPH.UPPER_HALF, attr: neonCyan }, { char: GLYPH.UPPER_HALF, attr: glowCyan }],
          [{ char: GLYPH.FULL_BLOCK, attr: neonMagenta }, { char: GLYPH.DARK_SHADE, attr: glowMagenta }],
          [{ char: GLYPH.LOWER_HALF, attr: base }, { char: GLYPH.LOWER_HALF, attr: base }]
        ],
        // Scale 3: 3x4
        [
          [U, { char: GLYPH.UPPER_HALF, attr: neonCyan }, U],
          [{ char: GLYPH.DARK_SHADE, attr: glowCyan }, { char: GLYPH.FULL_BLOCK, attr: neonCyan }, { char: GLYPH.DARK_SHADE, attr: glowCyan }],
          [{ char: GLYPH.DARK_SHADE, attr: glowMagenta }, { char: GLYPH.FULL_BLOCK, attr: neonMagenta }, { char: GLYPH.DARK_SHADE, attr: glowMagenta }],
          [U, { char: GLYPH.LOWER_HALF, attr: base }, U]
        ],
        // Scale 4: 3x5
        [
          [U, { char: GLYPH.UPPER_HALF, attr: neonCyan }, U],
          [{ char: GLYPH.DARK_SHADE, attr: glowCyan }, { char: GLYPH.FULL_BLOCK, attr: neonCyan }, { char: GLYPH.DARK_SHADE, attr: glowCyan }],
          [{ char: GLYPH.DARK_SHADE, attr: glowCyan }, { char: GLYPH.FULL_BLOCK, attr: neonCyan }, { char: GLYPH.DARK_SHADE, attr: glowCyan }],
          [{ char: GLYPH.DARK_SHADE, attr: glowMagenta }, { char: GLYPH.FULL_BLOCK, attr: neonMagenta }, { char: GLYPH.DARK_SHADE, attr: glowMagenta }],
          [U, { char: GLYPH.LOWER_HALF, attr: base }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a grid pylon - tall angular structure for the holodeck grid.
   * Magenta with cyan accents.
   */
  createGridPylon: function(): SpriteDefinition {
    var frame = makeAttr(MAGENTA, BG_BLACK);
    var frameBright = makeAttr(LIGHTMAGENTA, BG_BLACK);
    var accent = makeAttr(CYAN, BG_BLACK);
    var accentBright = makeAttr(LIGHTCYAN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'grid_pylon',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: GLYPH.TRIANGLE_UP, attr: frame }]
        ],
        // Scale 1: 1x2
        [
          [{ char: GLYPH.TRIANGLE_UP, attr: frameBright }],
          [{ char: GLYPH.BOX_V, attr: frame }]
        ],
        // Scale 2: 3x3
        [
          [U, { char: GLYPH.TRIANGLE_UP, attr: accentBright }, U],
          [{ char: '/', attr: frame }, { char: GLYPH.BOX_V, attr: frameBright }, { char: '\\', attr: frame }],
          [{ char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_CROSS, attr: accentBright }, { char: GLYPH.BOX_H, attr: accent }]
        ],
        // Scale 3: 3x4
        [
          [U, { char: GLYPH.TRIANGLE_UP, attr: accentBright }, U],
          [{ char: '/', attr: frame }, { char: GLYPH.BOX_V, attr: frameBright }, { char: '\\', attr: frame }],
          [{ char: GLYPH.BOX_V, attr: frame }, { char: GLYPH.BOX_V, attr: frameBright }, { char: GLYPH.BOX_V, attr: frame }],
          [{ char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_CROSS, attr: accentBright }, { char: GLYPH.BOX_H, attr: accent }]
        ],
        // Scale 4: 5x5
        [
          [U, U, { char: GLYPH.TRIANGLE_UP, attr: accentBright }, U, U],
          [U, { char: '/', attr: frameBright }, { char: GLYPH.BOX_V, attr: frameBright }, { char: '\\', attr: frameBright }, U],
          [{ char: '/', attr: frame }, { char: ' ', attr: frame }, { char: GLYPH.BOX_V, attr: frameBright }, { char: ' ', attr: frame }, { char: '\\', attr: frame }],
          [{ char: GLYPH.BOX_V, attr: frame }, { char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_CROSS, attr: accentBright }, { char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_V, attr: frame }],
          [{ char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_CROSS, attr: accentBright }, { char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_H, attr: accent }]
        ]
      ]
    };
  },
  
  /**
   * Create a holographic billboard - floating neon sign.
   */
  createHoloBillboard: function(): SpriteDefinition {
    var border = makeAttr(LIGHTMAGENTA, BG_BLACK);
    var borderDim = makeAttr(MAGENTA, BG_BLACK);
    var text = makeAttr(LIGHTCYAN, BG_BLACK);
    var support = makeAttr(BLUE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'holo_billboard',
      variants: [
        // Scale 0: 2x1
        [
          [{ char: GLYPH.FULL_BLOCK, attr: border }, { char: GLYPH.FULL_BLOCK, attr: borderDim }]
        ],
        // Scale 1: 3x2
        [
          [{ char: GLYPH.FULL_BLOCK, attr: border }, { char: '-', attr: text }, { char: GLYPH.FULL_BLOCK, attr: borderDim }],
          [U, { char: GLYPH.BOX_V, attr: support }, U]
        ],
        // Scale 2: 5x3
        [
          [{ char: GLYPH.UPPER_HALF, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.UPPER_HALF, attr: borderDim }],
          [{ char: GLYPH.BOX_V, attr: border }, { char: 'N', attr: text }, { char: 'E', attr: text }, { char: 'O', attr: text }, { char: GLYPH.BOX_V, attr: borderDim }],
          [U, U, { char: GLYPH.BOX_V, attr: support }, U, U]
        ],
        // Scale 3: 6x4
        [
          [{ char: GLYPH.UPPER_HALF, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.UPPER_HALF, attr: borderDim }],
          [{ char: GLYPH.BOX_V, attr: border }, { char: 'N', attr: text }, { char: 'E', attr: text }, { char: 'O', attr: text }, { char: 'N', attr: text }, { char: GLYPH.BOX_V, attr: borderDim }],
          [{ char: GLYPH.LOWER_HALF, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.LOWER_HALF, attr: borderDim }],
          [U, U, { char: GLYPH.BOX_V, attr: support }, { char: GLYPH.BOX_V, attr: support }, U, U]
        ],
        // Scale 4: 7x5
        [
          [{ char: GLYPH.UPPER_HALF, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.UPPER_HALF, attr: borderDim }],
          [{ char: GLYPH.BOX_V, attr: border }, { char: ' ', attr: text }, { char: 'N', attr: text }, { char: 'E', attr: text }, { char: 'O', attr: text }, { char: ' ', attr: text }, { char: GLYPH.BOX_V, attr: borderDim }],
          [{ char: GLYPH.BOX_V, attr: border }, { char: ' ', attr: text }, { char: '8', attr: text }, { char: '0', attr: text }, { char: 's', attr: text }, { char: ' ', attr: text }, { char: GLYPH.BOX_V, attr: borderDim }],
          [{ char: GLYPH.LOWER_HALF, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.LOWER_HALF, attr: borderDim }],
          [U, U, { char: GLYPH.BOX_V, attr: support }, U, { char: GLYPH.BOX_V, attr: support }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a neon palm tree - stylized palm with neon outline.
   * Magenta trunk, cyan fronds with glow.
   */
  createNeonPalm: function(): SpriteDefinition {
    var trunk = makeAttr(MAGENTA, BG_BLACK);
    var trunkBright = makeAttr(LIGHTMAGENTA, BG_BLACK);
    var frond = makeAttr(CYAN, BG_BLACK);
    var frondBright = makeAttr(LIGHTCYAN, BG_BLACK);
    var glow = makeAttr(BLUE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'neon_palm',
      variants: [
        // Scale 0: 1x1 - simple glow
        [
          [{ char: GLYPH.UPPER_HALF, attr: frondBright }]
        ],
        // Scale 1: 2x2 - tiny palm
        [
          [{ char: '/', attr: frond }, { char: '\\', attr: frond }],
          [U, { char: GLYPH.BOX_V, attr: trunk }]
        ],
        // Scale 2: 4x3 - small palm
        [
          [{ char: '/', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '\\', attr: frondBright }],
          [U, { char: '\\', attr: frond }, { char: '/', attr: frond }, U],
          [U, { char: GLYPH.BOX_V, attr: trunk }, { char: GLYPH.BOX_V, attr: trunkBright }, U]
        ],
        // Scale 3: 5x4 - medium palm
        [
          [{ char: '-', attr: glow }, { char: '/', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '\\', attr: frondBright }, { char: '-', attr: glow }],
          [U, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '*', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, U],
          [U, { char: '/', attr: frond }, { char: GLYPH.BOX_V, attr: trunkBright }, { char: '\\', attr: frond }, U],
          [U, U, { char: GLYPH.FULL_BLOCK, attr: trunk }, U, U]
        ],
        // Scale 4: 6x5 - large palm
        [
          [{ char: '-', attr: glow }, { char: '/', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '\\', attr: frondBright }, { char: '-', attr: glow }],
          [{ char: '/', attr: frond }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: ' ', attr: frond }, { char: '*', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '\\', attr: frond }],
          [U, { char: '\\', attr: frond }, { char: '/', attr: frond }, { char: '\\', attr: frond }, { char: '/', attr: frond }, U],
          [U, U, { char: GLYPH.FULL_BLOCK, attr: trunk }, { char: GLYPH.FULL_BLOCK, attr: trunkBright }, U, U],
          [U, U, { char: GLYPH.LOWER_HALF, attr: trunk }, { char: GLYPH.LOWER_HALF, attr: trunk }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a laser beam - vertical beam of light.
   */
  createLaserBeam: function(): SpriteDefinition {
    var beamCore = makeAttr(WHITE, BG_BLACK);
    var beamMid = makeAttr(LIGHTCYAN, BG_BLACK);
    var beamOuter = makeAttr(CYAN, BG_BLACK);
    var glowDim = makeAttr(BLUE, BG_BLACK);
    
    return {
      name: 'laser_beam',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: GLYPH.BOX_V, attr: beamMid }]
        ],
        // Scale 1: 1x2
        [
          [{ char: GLYPH.BOX_V, attr: beamCore }],
          [{ char: GLYPH.BOX_V, attr: beamMid }]
        ],
        // Scale 2: 3x3
        [
          [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }],
          [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
          [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }]
        ],
        // Scale 3: 3x4
        [
          [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }],
          [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
          [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
          [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }]
        ],
        // Scale 4: 3x5
        [
          [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }],
          [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
          [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamCore }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
          [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
          [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }]
        ]
      ]
    };
  }
};

// Register synthwave sprites
registerRoadsideSprite('neon_pillar', SynthwaveSprites.createNeonPillar);
registerRoadsideSprite('grid_pylon', SynthwaveSprites.createGridPylon);
registerRoadsideSprite('holo_billboard', SynthwaveSprites.createHoloBillboard);
registerRoadsideSprite('neon_palm', SynthwaveSprites.createNeonPalm);
registerRoadsideSprite('laser_beam', SynthwaveSprites.createLaserBeam);
