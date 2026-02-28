/**
 * CitySprites - Roadside sprites for urban/city themes.
 * 
 * Includes: buildings, lampposts, signs
 */

var CitySprites = {
  /**
   * Create a building sprite at various scales.
   * Buildings are tall rectangles with lit windows.
   */
  createBuilding: function(): SpriteDefinition {
    var wall = makeAttr(DARKGRAY, BG_BLACK);
    var window = makeAttr(YELLOW, BG_BLACK);
    var windowDark = makeAttr(DARKGRAY, BG_BLACK);
    var roof = makeAttr(LIGHTGRAY, BG_BLACK);
    var U: SpriteCell | null = null;  // Transparent
    
    return {
      name: 'building',
      variants: [
        // Scale 0: 1x2 - tiny distant building
        [
          [{ char: GLYPH.UPPER_HALF, attr: roof }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }]
        ],
        // Scale 1: 2x3
        [
          [{ char: GLYPH.UPPER_HALF, attr: roof }, { char: GLYPH.UPPER_HALF, attr: roof }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }],
          [{ char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }]
        ],
        // Scale 2: 3x4
        [
          [{ char: GLYPH.UPPER_HALF, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.UPPER_HALF, attr: roof }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }],
          [{ char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: windowDark }, { char: GLYPH.FULL_BLOCK, attr: wall }]
        ],
        // Scale 3: 4x5
        [
          [U, { char: GLYPH.UPPER_HALF, attr: roof }, { char: GLYPH.UPPER_HALF, attr: roof }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }],
          [{ char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: windowDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }]
        ],
        // Scale 4: 5x6 (very close)
        [
          [U, { char: GLYPH.UPPER_HALF, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.UPPER_HALF, attr: roof }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }],
          [{ char: '.', attr: windowDark }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: windowDark }, { char: GLYPH.FULL_BLOCK, attr: wall }],
          [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }]
        ]
      ]
    };
  },
  
  /**
   * Create a lamppost sprite - tall thin pole with light at top.
   */
  createLamppost: function(): SpriteDefinition {
    var pole = makeAttr(DARKGRAY, BG_BLACK);
    var light = makeAttr(YELLOW, BG_BLACK);
    var lightBright = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;  // Transparent
    
    return {
      name: 'lamppost',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: GLYPH.FULL_BLOCK, attr: light }]
        ],
        // Scale 1: 1x2
        [
          [{ char: '*', attr: light }],
          [{ char: GLYPH.BOX_V, attr: pole }]
        ],
        // Scale 2: 2x3
        [
          [{ char: '*', attr: lightBright }, { char: '*', attr: light }],
          [U, { char: GLYPH.BOX_V, attr: pole }],
          [U, { char: GLYPH.BOX_V, attr: pole }]
        ],
        // Scale 3: 2x4
        [
          [{ char: GLYPH.DARK_SHADE, attr: light }, { char: '*', attr: lightBright }],
          [U, { char: GLYPH.BOX_V, attr: pole }],
          [U, { char: GLYPH.BOX_V, attr: pole }],
          [U, { char: GLYPH.BOX_V, attr: pole }]
        ],
        // Scale 4: 3x5
        [
          [{ char: GLYPH.DARK_SHADE, attr: light }, { char: '*', attr: lightBright }, { char: GLYPH.DARK_SHADE, attr: light }],
          [U, { char: GLYPH.BOX_V, attr: pole }, U],
          [U, { char: GLYPH.BOX_V, attr: pole }, U],
          [U, { char: GLYPH.BOX_V, attr: pole }, U],
          [U, { char: GLYPH.BOX_V, attr: pole }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a highway sign sprite - green rectangle with text.
   */
  createSign: function(): SpriteDefinition {
    var sign = makeAttr(GREEN, BG_BLACK);
    var signBright = makeAttr(LIGHTGREEN, BG_BLACK);
    var text = makeAttr(WHITE, BG_GREEN);
    var pole = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;  // Transparent
    
    return {
      name: 'sign',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: GLYPH.FULL_BLOCK, attr: sign }]
        ],
        // Scale 1: 2x2
        [
          [{ char: GLYPH.FULL_BLOCK, attr: sign }, { char: GLYPH.FULL_BLOCK, attr: sign }],
          [U, { char: GLYPH.BOX_V, attr: pole }]
        ],
        // Scale 2: 3x2
        [
          [{ char: GLYPH.FULL_BLOCK, attr: sign }, { char: '-', attr: text }, { char: GLYPH.FULL_BLOCK, attr: sign }],
          [U, { char: GLYPH.BOX_V, attr: pole }, U]
        ],
        // Scale 3: 4x3
        [
          [{ char: GLYPH.FULL_BLOCK, attr: sign }, { char: 'E', attr: text }, { char: 'X', attr: text }, { char: GLYPH.FULL_BLOCK, attr: sign }],
          [{ char: GLYPH.FULL_BLOCK, attr: signBright }, { char: 'I', attr: text }, { char: 'T', attr: text }, { char: GLYPH.FULL_BLOCK, attr: signBright }],
          [U, { char: GLYPH.BOX_V, attr: pole }, { char: GLYPH.BOX_V, attr: pole }, U]
        ],
        // Scale 4: 5x3
        [
          [{ char: GLYPH.FULL_BLOCK, attr: sign }, { char: 'E', attr: text }, { char: 'X', attr: text }, { char: 'I', attr: text }, { char: GLYPH.FULL_BLOCK, attr: sign }],
          [{ char: GLYPH.FULL_BLOCK, attr: signBright }, { char: ' ', attr: text }, { char: '1', attr: text }, { char: ' ', attr: text }, { char: GLYPH.FULL_BLOCK, attr: signBright }],
          [U, U, { char: GLYPH.BOX_V, attr: pole }, U, U]
        ]
      ]
    };
  }
};

// Register city sprites
registerRoadsideSprite('building', CitySprites.createBuilding);
registerRoadsideSprite('lamppost', CitySprites.createLamppost);
registerRoadsideSprite('sign', CitySprites.createSign);
