/**
 * HorrorSprites.ts - Spooky roadside objects for Haunted Hollow theme.
 * Gravestones, dead trees, jack-o-lanterns, skulls, and creepy fences.
 */

var HorrorSprites = {
  /**
   * Create a dead tree sprite - gnarled, leafless branches.
   */
  createDeadTree: function(): SpriteDefinition {
    var branch = makeAttr(BROWN, BG_BLACK);
    var branchDark = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'deadtree',
      variants: [
        // Scale 0: 3x2 - tiny distant tree
        [
          [U, { char: 'Y', attr: branch }, U],
          [U, { char: '|', attr: branch }, U]
        ],
        // Scale 1: 3x3
        [
          [{ char: '\\', attr: branchDark }, { char: 'Y', attr: branch }, { char: '/', attr: branchDark }],
          [U, { char: '|', attr: branch }, U],
          [U, { char: '|', attr: branch }, U]
        ],
        // Scale 2: 5x4
        [
          [{ char: '\\', attr: branchDark }, U, { char: 'Y', attr: branch }, U, { char: '/', attr: branchDark }],
          [U, { char: '\\', attr: branch }, { char: '|', attr: branch }, { char: '/', attr: branch }, U],
          [U, U, { char: '|', attr: branch }, U, U],
          [U, U, { char: '|', attr: branch }, U, U]
        ],
        // Scale 3: 7x5
        [
          [{ char: '\\', attr: branchDark }, U, { char: 'Y', attr: branch }, U, { char: 'Y', attr: branch }, U, { char: '/', attr: branchDark }],
          [U, { char: '\\', attr: branch }, { char: '\\', attr: branch }, { char: '|', attr: branch }, { char: '/', attr: branch }, { char: '/', attr: branch }, U],
          [U, U, { char: '\\', attr: branch }, { char: '|', attr: branch }, { char: '/', attr: branch }, U, U],
          [U, U, U, { char: '|', attr: branch }, U, U, U],
          [U, U, U, { char: '|', attr: branch }, U, U, U]
        ],
        // Scale 4: 9x6 - gnarly close-up
        [
          [{ char: '_', attr: branchDark }, U, U, U, { char: 'V', attr: branch }, U, U, U, { char: '_', attr: branchDark }],
          [U, { char: '\\', attr: branch }, { char: 'Y', attr: branch }, U, { char: '|', attr: branch }, U, { char: 'Y', attr: branch }, { char: '/', attr: branch }, U],
          [U, U, { char: '\\', attr: branch }, { char: '\\', attr: branch }, { char: '|', attr: branch }, { char: '/', attr: branch }, { char: '/', attr: branch }, U, U],
          [U, U, U, U, { char: '|', attr: branch }, U, U, U, U],
          [U, U, U, U, { char: '|', attr: branch }, U, U, U, U],
          [U, U, U, { char: '-', attr: branch }, { char: '|', attr: branch }, { char: '-', attr: branch }, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a gravestone sprite - classic tombstone shape.
   */
  createGravestone: function(): SpriteDefinition {
    var stone = makeAttr(LIGHTGRAY, BG_BLACK);
    var stoneDark = makeAttr(DARKGRAY, BG_BLACK);
    var stoneText = makeAttr(DARKGRAY, BG_LIGHTGRAY);
    var U: SpriteCell | null = null;
    
    return {
      name: 'gravestone',
      variants: [
        // Scale 0: 2x2 - tiny
        [
          [{ char: GLYPH.UPPER_HALF, attr: stone }, { char: GLYPH.UPPER_HALF, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: GLYPH.UPPER_HALF, attr: stone }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: 'R', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'I', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 2: 4x4
        [
          [U, { char: GLYPH.UPPER_HALF, attr: stone }, { char: GLYPH.UPPER_HALF, attr: stone }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: 'R', attr: stoneText }, { char: 'I', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'P', attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.LOWER_HALF, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.LOWER_HALF, attr: stoneDark }]
        ],
        // Scale 3: 5x5
        [
          [U, { char: '_', attr: stone }, { char: GLYPH.UPPER_HALF, attr: stone }, { char: '_', attr: stone }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: stoneText }, { char: '+', attr: stoneText }, { char: ' ', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'R', attr: stoneText }, { char: '.', attr: stoneText }, { char: 'I', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'P', attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.LOWER_HALF, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.LOWER_HALF, attr: stoneDark }]
        ],
        // Scale 4: 5x5 (same as scale 3)
        [
          [U, { char: '_', attr: stone }, { char: GLYPH.UPPER_HALF, attr: stone }, { char: '_', attr: stone }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: stoneText }, { char: '+', attr: stoneText }, { char: ' ', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'R', attr: stoneText }, { char: '.', attr: stoneText }, { char: 'I', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'P', attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.LOWER_HALF, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.LOWER_HALF, attr: stoneDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a jack-o-lantern sprite - glowing pumpkin.
   */
  createPumpkin: function(): SpriteDefinition {
    var pumpkin = makeAttr(BROWN, BG_BLACK);
    var glow = makeAttr(YELLOW, BG_BROWN);
    var stem = makeAttr(GREEN, BG_BLACK);
    var face = makeAttr(YELLOW, BG_BROWN);
    var U: SpriteCell | null = null;
    
    return {
      name: 'pumpkin',
      variants: [
        // Scale 0: 2x2 - tiny distant
        [
          [{ char: '(', attr: glow }, { char: ')', attr: glow }],
          [{ char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '|', attr: stem }, U],
          [{ char: '(', attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: glow }],
          [{ char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }]
        ],
        // Scale 2: 5x4
        [
          [U, U, { char: '|', attr: stem }, U, U],
          [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
          [{ char: '(', attr: pumpkin }, { char: '^', attr: face }, { char: 'v', attr: face }, { char: '^', attr: face }, { char: ')', attr: pumpkin }],
          [U, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, U]
        ],
        // Scale 3: 6x5
        [
          [U, U, { char: '\\', attr: stem }, { char: '/', attr: stem }, U, U],
          [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
          [{ char: '(', attr: pumpkin }, { char: '^', attr: face }, { char: GLYPH.FULL_BLOCK, attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: pumpkin }, { char: '^', attr: face }, { char: ')', attr: pumpkin }],
          [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: 'v', attr: face }, { char: 'v', attr: face }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
          [U, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, U]
        ],
        // Scale 4: 6x5 (same as scale 3)
        [
          [U, U, { char: '\\', attr: stem }, { char: '/', attr: stem }, U, U],
          [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
          [{ char: '(', attr: pumpkin }, { char: '^', attr: face }, { char: GLYPH.FULL_BLOCK, attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: pumpkin }, { char: '^', attr: face }, { char: ')', attr: pumpkin }],
          [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: 'v', attr: face }, { char: 'v', attr: face }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
          [U, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a skull on a stake sprite.
   */
  createSkull: function(): SpriteDefinition {
    var bone = makeAttr(WHITE, BG_BLACK);
    var eye = makeAttr(BLACK, BG_LIGHTGRAY);
    var stake = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'skull',
      variants: [
        // Scale 0: 2x2 - tiny
        [
          [{ char: '0', attr: bone }, { char: '0', attr: bone }],
          [U, { char: '|', attr: stake }]
        ],
        // Scale 1: 3x3
        [
          [{ char: '(', attr: bone }, { char: GLYPH.UPPER_HALF, attr: bone }, { char: ')', attr: bone }],
          [{ char: 'o', attr: eye }, { char: 'v', attr: bone }, { char: 'o', attr: eye }],
          [U, { char: '|', attr: stake }, U]
        ],
        // Scale 2: 4x4
        [
          [U, { char: GLYPH.UPPER_HALF, attr: bone }, { char: GLYPH.UPPER_HALF, attr: bone }, U],
          [{ char: '(', attr: bone }, { char: 'o', attr: eye }, { char: 'o', attr: eye }, { char: ')', attr: bone }],
          [U, { char: GLYPH.LOWER_HALF, attr: bone }, { char: GLYPH.LOWER_HALF, attr: bone }, U],
          [U, { char: '|', attr: stake }, { char: '|', attr: stake }, U]
        ],
        // Scale 3: 5x5
        [
          [U, { char: '_', attr: bone }, { char: GLYPH.UPPER_HALF, attr: bone }, { char: '_', attr: bone }, U],
          [{ char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }],
          [{ char: '(', attr: bone }, { char: ' ', attr: bone }, { char: 'v', attr: bone }, { char: ' ', attr: bone }, { char: ')', attr: bone }],
          [U, { char: GLYPH.LOWER_HALF, attr: bone }, { char: 'w', attr: bone }, { char: GLYPH.LOWER_HALF, attr: bone }, U],
          [U, U, { char: '|', attr: stake }, U, U]
        ],
        // Scale 4: 5x5 (same as scale 3)
        [
          [U, { char: '_', attr: bone }, { char: GLYPH.UPPER_HALF, attr: bone }, { char: '_', attr: bone }, U],
          [{ char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }],
          [{ char: '(', attr: bone }, { char: ' ', attr: bone }, { char: 'v', attr: bone }, { char: ' ', attr: bone }, { char: ')', attr: bone }],
          [U, { char: GLYPH.LOWER_HALF, attr: bone }, { char: 'w', attr: bone }, { char: GLYPH.LOWER_HALF, attr: bone }, U],
          [U, U, { char: '|', attr: stake }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create an iron cemetery fence sprite.
   */
  createFence: function(): SpriteDefinition {
    var iron = makeAttr(DARKGRAY, BG_BLACK);
    var ironPoint = makeAttr(LIGHTGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'fence',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: '|', attr: iron }, { char: '|', attr: iron }, { char: '|', attr: iron }],
          [{ char: '-', attr: iron }, { char: '-', attr: iron }, { char: '-', attr: iron }]
        ],
        // Scale 1: 5x3
        [
          [{ char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }],
          [{ char: '|', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }],
          [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }]
        ],
        // Scale 2: 7x4
        [
          [{ char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }],
          [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }],
          [{ char: '|', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }],
          [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }]
        ],
        // Scale 3: 7x4 (same as scale 2)
        [
          [{ char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }],
          [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }],
          [{ char: '|', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }],
          [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }]
        ],
        // Scale 4: 7x4 (same as scale 2)
        [
          [{ char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }],
          [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }],
          [{ char: '|', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }],
          [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }]
        ]
      ]
    };
  },
  
  /**
   * Create a candelabra sprite - eerie light source.
   */
  createCandle: function(): SpriteDefinition {
    var flame = makeAttr(YELLOW, BG_BLACK);
    var candle = makeAttr(WHITE, BG_BLACK);
    var holder = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'candle',
      variants: [
        // Scale 0: 1x2
        [
          [{ char: '*', attr: flame }],
          [{ char: '|', attr: holder }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '*', attr: flame }, U],
          [{ char: '(', attr: holder }, { char: GLYPH.FULL_BLOCK, attr: candle }, { char: ')', attr: holder }],
          [U, { char: '|', attr: holder }, U]
        ],
        // Scale 2: 5x4
        [
          [{ char: '*', attr: flame }, U, { char: '*', attr: flame }, U, { char: '*', attr: flame }],
          [{ char: '|', attr: candle }, U, { char: '|', attr: candle }, U, { char: '|', attr: candle }],
          [{ char: '\\', attr: holder }, { char: '-', attr: holder }, { char: '+', attr: holder }, { char: '-', attr: holder }, { char: '/', attr: holder }],
          [U, U, { char: '|', attr: holder }, U, U]
        ],
        // Scale 3: 5x4 (same as scale 2)
        [
          [{ char: '*', attr: flame }, U, { char: '*', attr: flame }, U, { char: '*', attr: flame }],
          [{ char: '|', attr: candle }, U, { char: '|', attr: candle }, U, { char: '|', attr: candle }],
          [{ char: '\\', attr: holder }, { char: '-', attr: holder }, { char: '+', attr: holder }, { char: '-', attr: holder }, { char: '/', attr: holder }],
          [U, U, { char: '|', attr: holder }, U, U]
        ],
        // Scale 4: 5x4 (same as scale 2)
        [
          [{ char: '*', attr: flame }, U, { char: '*', attr: flame }, U, { char: '*', attr: flame }],
          [{ char: '|', attr: candle }, U, { char: '|', attr: candle }, U, { char: '|', attr: candle }],
          [{ char: '\\', attr: holder }, { char: '-', attr: holder }, { char: '+', attr: holder }, { char: '-', attr: holder }, { char: '/', attr: holder }],
          [U, U, { char: '|', attr: holder }, U, U]
        ]
      ]
    };
  }
};

// Register horror sprites
registerRoadsideSprite('deadtree', HorrorSprites.createDeadTree);
registerRoadsideSprite('gravestone', HorrorSprites.createGravestone);
registerRoadsideSprite('pumpkin', HorrorSprites.createPumpkin);
registerRoadsideSprite('skull', HorrorSprites.createSkull);
registerRoadsideSprite('fence', HorrorSprites.createFence);
registerRoadsideSprite('candle', HorrorSprites.createCandle);
