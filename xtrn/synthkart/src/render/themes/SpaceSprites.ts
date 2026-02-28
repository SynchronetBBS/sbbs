/**
 * SpaceSprites.ts - Cosmic roadside objects for Rainbow Road theme.
 * Stars, moons, planets, comets, nebulae, and satellites.
 */

var SpaceSprites = {
  /**
   * Create a star sprite - twinkling celestial body.
   */
  createStar: function(): SpriteDefinition {
    var core = makeAttr(WHITE, BG_BLACK);
    var glow = makeAttr(YELLOW, BG_BLACK);
    var glowDim = makeAttr(LIGHTCYAN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'star',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: '*', attr: core }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '.', attr: glow }, U],
          [{ char: '-', attr: glow }, { char: '*', attr: core }, { char: '-', attr: glow }],
          [U, { char: "'", attr: glow }, U]
        ],
        // Scale 2: 5x5
        [
          [U, U, { char: '.', attr: glowDim }, U, U],
          [U, { char: '\\', attr: glow }, { char: '|', attr: core }, { char: '/', attr: glow }, U],
          [{ char: '-', attr: glow }, { char: '-', attr: core }, { char: '*', attr: core }, { char: '-', attr: core }, { char: '-', attr: glow }],
          [U, { char: '/', attr: glow }, { char: '|', attr: core }, { char: '\\', attr: glow }, U],
          [U, U, { char: "'", attr: glowDim }, U, U]
        ],
        // Scale 3: 5x5 (same)
        [
          [U, U, { char: '.', attr: glowDim }, U, U],
          [U, { char: '\\', attr: glow }, { char: '|', attr: core }, { char: '/', attr: glow }, U],
          [{ char: '-', attr: glow }, { char: '-', attr: core }, { char: '*', attr: core }, { char: '-', attr: core }, { char: '-', attr: glow }],
          [U, { char: '/', attr: glow }, { char: '|', attr: core }, { char: '\\', attr: glow }, U],
          [U, U, { char: "'", attr: glowDim }, U, U]
        ],
        // Scale 4: 5x5 (same)
        [
          [U, U, { char: '.', attr: glowDim }, U, U],
          [U, { char: '\\', attr: glow }, { char: '|', attr: core }, { char: '/', attr: glow }, U],
          [{ char: '-', attr: glow }, { char: '-', attr: core }, { char: '*', attr: core }, { char: '-', attr: core }, { char: '-', attr: glow }],
          [U, { char: '/', attr: glow }, { char: '|', attr: core }, { char: '\\', attr: glow }, U],
          [U, U, { char: "'", attr: glowDim }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a moon sprite - crescent or full moon.
   */
  createMoon: function(): SpriteDefinition {
    var moonLight = makeAttr(WHITE, BG_BLACK);
    var moonMid = makeAttr(LIGHTGRAY, BG_BLACK);
    var moonDark = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'moon',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '(', attr: moonLight }, { char: ')', attr: moonMid }],
          [{ char: '(', attr: moonMid }, { char: ')', attr: moonDark }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '_', attr: moonLight }, U],
          [{ char: '(', attr: moonLight }, { char: ' ', attr: moonMid }, { char: ')', attr: moonMid }],
          [U, { char: '-', attr: moonDark }, U]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonMid }, U],
          [{ char: '/', attr: moonLight }, { char: ' ', attr: moonLight }, { char: 'o', attr: moonMid }, { char: ' ', attr: moonMid }, { char: '\\', attr: moonMid }],
          [{ char: '\\', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonDark }, { char: 'o', attr: moonDark }, { char: '/', attr: moonDark }],
          [U, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, U]
        ],
        // Scale 3: 6x5
        [
          [U, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonMid }, U],
          [{ char: '/', attr: moonLight }, { char: ' ', attr: moonLight }, { char: ' ', attr: moonLight }, { char: 'o', attr: moonMid }, { char: ' ', attr: moonMid }, { char: '\\', attr: moonMid }],
          [{ char: '|', attr: moonLight }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonDark }, { char: '|', attr: moonDark }],
          [{ char: '\\', attr: moonMid }, { char: ' ', attr: moonDark }, { char: 'o', attr: moonDark }, { char: ' ', attr: moonDark }, { char: ' ', attr: moonDark }, { char: '/', attr: moonDark }],
          [U, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, U]
        ],
        // Scale 4: 6x5 (same)
        [
          [U, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonMid }, U],
          [{ char: '/', attr: moonLight }, { char: ' ', attr: moonLight }, { char: ' ', attr: moonLight }, { char: 'o', attr: moonMid }, { char: ' ', attr: moonMid }, { char: '\\', attr: moonMid }],
          [{ char: '|', attr: moonLight }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonDark }, { char: '|', attr: moonDark }],
          [{ char: '\\', attr: moonMid }, { char: ' ', attr: moonDark }, { char: 'o', attr: moonDark }, { char: ' ', attr: moonDark }, { char: ' ', attr: moonDark }, { char: '/', attr: moonDark }],
          [U, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a planet sprite - ringed gas giant.
   */
  createPlanet: function(): SpriteDefinition {
    var planet = makeAttr(LIGHTRED, BG_BLACK);
    var planetDark = makeAttr(RED, BG_BLACK);
    var ring = makeAttr(YELLOW, BG_BLACK);
    var ringDark = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'planet',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '(', attr: planet }, { char: ')', attr: planetDark }],
          [{ char: '-', attr: ring }, { char: '-', attr: ringDark }]
        ],
        // Scale 1: 4x3
        [
          [U, { char: '(', attr: planet }, { char: ')', attr: planetDark }, U],
          [{ char: '-', attr: ring }, { char: '-', attr: ring }, { char: '-', attr: ringDark }, { char: '-', attr: ringDark }],
          [U, { char: '(', attr: planetDark }, { char: ')', attr: planetDark }, U]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: '\\', attr: planetDark }, U],
          [{ char: '-', attr: ring }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '-', attr: ringDark }],
          [{ char: '-', attr: ringDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '-', attr: ringDark }],
          [U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U]
        ],
        // Scale 3: 7x5
        [
          [U, U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: '\\', attr: planetDark }, U, U],
          [U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '\\', attr: planetDark }, U],
          [{ char: '-', attr: ring }, { char: '-', attr: ring }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '-', attr: ringDark }, { char: '-', attr: ringDark }],
          [U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U],
          [U, U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U, U]
        ],
        // Scale 4: 7x5 (same)
        [
          [U, U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: '\\', attr: planetDark }, U, U],
          [U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '\\', attr: planetDark }, U],
          [{ char: '-', attr: ring }, { char: '-', attr: ring }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '-', attr: ringDark }, { char: '-', attr: ringDark }],
          [U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U],
          [U, U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a comet sprite - blazing through space.
   */
  createComet: function(): SpriteDefinition {
    var core = makeAttr(WHITE, BG_BLACK);
    var coreGlow = makeAttr(LIGHTCYAN, BG_BLACK);
    var tail1 = makeAttr(LIGHTBLUE, BG_BLACK);
    var tail2 = makeAttr(BLUE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'comet',
      variants: [
        // Scale 0: 3x1
        [
          [{ char: '-', attr: tail2 }, { char: '=', attr: tail1 }, { char: '*', attr: core }]
        ],
        // Scale 1: 4x2
        [
          [U, U, { char: '/', attr: coreGlow }, { char: '*', attr: core }],
          [{ char: '-', attr: tail2 }, { char: '~', attr: tail1 }, { char: '\\', attr: coreGlow }, U]
        ],
        // Scale 2: 6x3
        [
          [U, U, U, { char: '/', attr: coreGlow }, { char: 'O', attr: core }, { char: ')', attr: coreGlow }],
          [{ char: '-', attr: tail2 }, { char: '~', attr: tail2 }, { char: '=', attr: tail1 }, { char: '=', attr: coreGlow }, { char: '\\', attr: coreGlow }, U],
          [U, U, U, { char: '~', attr: tail1 }, U, U]
        ],
        // Scale 3: 8x4
        [
          [U, U, U, U, { char: '.', attr: coreGlow }, { char: '/', attr: coreGlow }, { char: '@', attr: core }, { char: ')', attr: coreGlow }],
          [{ char: '-', attr: tail2 }, { char: '-', attr: tail2 }, { char: '~', attr: tail1 }, { char: '=', attr: tail1 }, { char: '=', attr: coreGlow }, { char: '@', attr: core }, { char: ')', attr: coreGlow }, U],
          [U, U, { char: '~', attr: tail2 }, { char: '~', attr: tail1 }, { char: '=', attr: tail1 }, { char: '\\', attr: coreGlow }, U, U],
          [U, U, U, U, { char: '~', attr: tail1 }, U, U, U]
        ],
        // Scale 4: 8x4 (same)
        [
          [U, U, U, U, { char: '.', attr: coreGlow }, { char: '/', attr: coreGlow }, { char: '@', attr: core }, { char: ')', attr: coreGlow }],
          [{ char: '-', attr: tail2 }, { char: '-', attr: tail2 }, { char: '~', attr: tail1 }, { char: '=', attr: tail1 }, { char: '=', attr: coreGlow }, { char: '@', attr: core }, { char: ')', attr: coreGlow }, U],
          [U, U, { char: '~', attr: tail2 }, { char: '~', attr: tail1 }, { char: '=', attr: tail1 }, { char: '\\', attr: coreGlow }, U, U],
          [U, U, U, U, { char: '~', attr: tail1 }, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a nebula sprite - colorful gas cloud.
   */
  createNebula: function(): SpriteDefinition {
    var gas1 = makeAttr(LIGHTMAGENTA, BG_BLACK);
    var gas2 = makeAttr(MAGENTA, BG_BLACK);
    var gas3 = makeAttr(LIGHTBLUE, BG_BLACK);
    var gas4 = makeAttr(BLUE, BG_BLACK);
    var star = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'nebula',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }],
          [{ char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }]
        ],
        // Scale 1: 4x3
        [
          [U, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.LIGHT_SHADE, attr: gas2 }, U],
          [{ char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }],
          [U, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, U]
        ],
        // Scale 2: 6x4
        [
          [U, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
          [{ char: GLYPH.LIGHT_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }],
          [{ char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }],
          [U, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U]
        ],
        // Scale 3: 7x5
        [
          [U, U, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, U, U],
          [U, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
          [{ char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }],
          [U, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
          [U, U, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, U, U]
        ],
        // Scale 4: 7x5 (same)
        [
          [U, U, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, U, U],
          [U, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
          [{ char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }],
          [U, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
          [U, U, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a satellite sprite - orbiting spacecraft.
   */
  createSatellite: function(): SpriteDefinition {
    var body = makeAttr(LIGHTGRAY, BG_BLACK);
    var bodyDark = makeAttr(DARKGRAY, BG_BLACK);
    var panel = makeAttr(LIGHTBLUE, BG_BLACK);
    var panelDark = makeAttr(BLUE, BG_BLACK);
    var antenna = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'satellite',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: '=', attr: panel }, { char: '[', attr: body }, { char: '=', attr: panel }],
          [U, { char: '|', attr: antenna }, U]
        ],
        // Scale 1: 5x3
        [
          [U, U, { char: '/', attr: antenna }, U, U],
          [{ char: '=', attr: panel }, { char: '=', attr: panelDark }, { char: '[', attr: body }, { char: '=', attr: panelDark }, { char: '=', attr: panel }],
          [U, U, { char: GLYPH.LOWER_HALF, attr: bodyDark }, U, U]
        ],
        // Scale 2: 7x4
        [
          [U, U, U, { char: '/', attr: antenna }, { char: ')', attr: antenna }, U, U],
          [{ char: '/', attr: panel }, { char: '#', attr: panel }, { char: '\\', attr: panelDark }, { char: '[', attr: body }, { char: '/', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }],
          [{ char: '\\', attr: panelDark }, { char: '#', attr: panelDark }, { char: '/', attr: panel }, { char: ']', attr: bodyDark }, { char: '\\', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }],
          [U, U, U, { char: GLYPH.LOWER_HALF, attr: bodyDark }, U, U, U]
        ],
        // Scale 3: 9x5
        [
          [U, U, U, U, { char: '/', attr: antenna }, { char: ')', attr: antenna }, U, U, U],
          [U, U, U, U, { char: '[', attr: body }, { char: ']', attr: body }, U, U, U],
          [{ char: '/', attr: panel }, { char: '#', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }, { char: '[', attr: body }, { char: ']', attr: bodyDark }, { char: '\\', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }],
          [{ char: '\\', attr: panelDark }, { char: '#', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }, { char: '[', attr: bodyDark }, { char: ']', attr: bodyDark }, { char: '/', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }],
          [U, U, U, U, { char: GLYPH.LOWER_HALF, attr: bodyDark }, { char: GLYPH.LOWER_HALF, attr: bodyDark }, U, U, U]
        ],
        // Scale 4: 9x5 (same)
        [
          [U, U, U, U, { char: '/', attr: antenna }, { char: ')', attr: antenna }, U, U, U],
          [U, U, U, U, { char: '[', attr: body }, { char: ']', attr: body }, U, U, U],
          [{ char: '/', attr: panel }, { char: '#', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }, { char: '[', attr: body }, { char: ']', attr: bodyDark }, { char: '\\', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }],
          [{ char: '\\', attr: panelDark }, { char: '#', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }, { char: '[', attr: bodyDark }, { char: ']', attr: bodyDark }, { char: '/', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }],
          [U, U, U, U, { char: GLYPH.LOWER_HALF, attr: bodyDark }, { char: GLYPH.LOWER_HALF, attr: bodyDark }, U, U, U]
        ]
      ]
    };
  }
};

// Register space sprites
registerRoadsideSprite('star', SpaceSprites.createStar);
registerRoadsideSprite('moon', SpaceSprites.createMoon);
registerRoadsideSprite('planet', SpaceSprites.createPlanet);
registerRoadsideSprite('comet', SpaceSprites.createComet);
registerRoadsideSprite('nebula', SpaceSprites.createNebula);
registerRoadsideSprite('satellite', SpaceSprites.createSatellite);
