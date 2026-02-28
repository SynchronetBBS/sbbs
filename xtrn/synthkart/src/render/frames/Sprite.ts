/**
 * Sprite - A drawable sprite that renders to a Frame.
 * 
 * Sprites are defined as 2D arrays of cells (char + attr).
 * They can be scaled by selecting different size variants.
 * Transparency is achieved by using null cells.
 */

interface SpriteCell {
  char: string;
  attr: number;
}

interface SpriteDefinition {
  name: string;
  // Multiple scale variants: index 0 = smallest (far), higher = larger (close)
  // null cells are transparent
  variants: (SpriteCell | null)[][][];  // [scaleIndex][row][col]
}

/**
 * SpriteSheet - Collection of pre-defined sprites.
 */
var SpriteSheet = {
  /**
   * Create a tree sprite at various scales.
   * Scale 0: 1x1 (far)
   * Scale 1: 3x2 (medium-far)
   * Scale 2: 4x3 (medium)
   * Scale 3: 5x4 (close)
   * Scale 4: 6x5 (very close)
   */
  createTree: function(): SpriteDefinition {
    var leafTop = makeAttr(LIGHTGREEN, BG_BLACK);
    var leafDark = makeAttr(GREEN, BG_BLACK);
    var trunk = makeAttr(BROWN, BG_BLACK);
    var leafTrunk = makeAttr(LIGHTGREEN, BG_BROWN);  // Two-tone cell
    var U: SpriteCell | null = null;  // Transparent
    
    return {
      name: 'tree',
      variants: [
        // Scale 0: 1x1 - two-tone half block
        [
          [{ char: GLYPH.UPPER_HALF, attr: leafTrunk }]
        ],
        // Scale 1: 3x2
        [
          [{ char: GLYPH.UPPER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.UPPER_HALF, attr: leafDark }],
          [U, { char: GLYPH.UPPER_HALF, attr: leafTrunk }, U]
        ],
        // Scale 2: 4x3
        [
          [{ char: GLYPH.UPPER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.UPPER_HALF, attr: leafDark }],
          [{ char: GLYPH.DARK_SHADE, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.DARK_SHADE, attr: leafDark }],
          [U, { char: GLYPH.UPPER_HALF, attr: leafTrunk }, { char: GLYPH.UPPER_HALF, attr: leafTrunk }, U]
        ],
        // Scale 3: 5x4
        [
          [{ char: GLYPH.UPPER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.UPPER_HALF, attr: leafDark }],
          [{ char: GLYPH.DARK_SHADE, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.DARK_SHADE, attr: leafDark }],
          [U, { char: GLYPH.LOWER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.LOWER_HALF, attr: leafDark }, U],
          [U, U, { char: GLYPH.FULL_BLOCK, attr: trunk }, U, U]
        ],
        // Scale 4: 6x5 (very close)
        [
          [{ char: GLYPH.UPPER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.UPPER_HALF, attr: leafDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafDark }],
          [U, { char: GLYPH.DARK_SHADE, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.DARK_SHADE, attr: leafDark }, U],
          [U, U, { char: GLYPH.FULL_BLOCK, attr: trunk }, { char: GLYPH.FULL_BLOCK, attr: trunk }, U, U],
          [U, U, { char: GLYPH.UPPER_HALF, attr: trunk }, { char: GLYPH.UPPER_HALF, attr: trunk }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a rock sprite - wide and flat.
   */
  createRock: function(): SpriteDefinition {
    var rock = makeAttr(DARKGRAY, BG_BLACK);
    var rockLight = makeAttr(LIGHTGRAY, BG_BLACK);
    
    return {
      name: 'rock',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: GLYPH.LOWER_HALF, attr: rock }]
        ],
        // Scale 1: 3x1
        [
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.LOWER_HALF, attr: rock }]
        ],
        // Scale 2: 4x1
        [
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rock }]
        ],
        // Scale 3: 5x2
        [
          [{ char: GLYPH.UPPER_HALF, attr: rock }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rock }, { char: GLYPH.UPPER_HALF, attr: rock }],
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rock }]
        ],
        // Scale 4: 6x2
        [
          [{ char: GLYPH.UPPER_HALF, attr: rock }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rock }, { char: GLYPH.UPPER_HALF, attr: rock }],
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rock }]
        ]
      ]
    };
  },
  
  /**
   * Create a bush sprite - round and low.
   */
  createBush: function(): SpriteDefinition {
    var bush = makeAttr(GREEN, BG_BLACK);
    var bushLight = makeAttr(LIGHTGREEN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'bush',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: GLYPH.LOWER_HALF, attr: bush }]
        ],
        // Scale 1: 3x1
        [
          [{ char: GLYPH.LOWER_HALF, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.LOWER_HALF, attr: bush }]
        ],
        // Scale 2: 4x1
        [
          [{ char: GLYPH.LOWER_HALF, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.LOWER_HALF, attr: bush }]
        ],
        // Scale 3: 5x2
        [
          [U, { char: GLYPH.UPPER_HALF, attr: bush }, { char: GLYPH.UPPER_HALF, attr: bushLight }, { char: GLYPH.UPPER_HALF, attr: bush }, U],
          [{ char: GLYPH.LOWER_HALF, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.LOWER_HALF, attr: bush }]
        ],
        // Scale 4: 6x2
        [
          [{ char: GLYPH.UPPER_HALF, attr: bush }, { char: GLYPH.UPPER_HALF, attr: bushLight }, { char: GLYPH.UPPER_HALF, attr: bushLight }, { char: GLYPH.UPPER_HALF, attr: bushLight }, { char: GLYPH.UPPER_HALF, attr: bush }, { char: GLYPH.UPPER_HALF, attr: bush }],
          [{ char: GLYPH.LOWER_HALF, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.LOWER_HALF, attr: bush }]
        ]
      ]
    };
  },
  
  /**
   * Create player vehicle sprite.
   */
  createPlayerCar: function(): SpriteDefinition {
    var body = makeAttr(YELLOW, BG_BLACK);
    var trim = makeAttr(WHITE, BG_BLACK);
    var wheel = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'player_car',
      variants: [
        // Only one size for player (always at bottom of screen)
        // 5x3 car viewed from behind
        [
          [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }],
          [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
        ]
      ]
    };
  },
  
  /**
   * Create AI vehicle sprite with color parameter.
   */
  createAICar: function(color: number): SpriteDefinition {
    var body = makeAttr(color, BG_BLACK);
    var trim = makeAttr(WHITE, BG_BLACK);
    var wheel = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'ai_car',
      variants: [
        // Scale 0: tiny (far) - 2x1
        [
          [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }]
        ],
        // Scale 1: small - 3x2
        [
          [{ char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }],
          [{ char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }]
        ],
        // Scale 2: medium - 4x2
        [
          [{ char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }],
          [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }]
        ],
        // Scale 3: large (close) - 5x3
        [
          [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }],
          [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
        ]
      ]
    };
  }
};

/**
 * Render a sprite variant to a Frame.
 */
function renderSpriteToFrame(frame: Frame, sprite: SpriteDefinition, scaleIndex: number): void {
  var variant = sprite.variants[scaleIndex];
  if (!variant) {
    // Clamp to max available scale
    variant = sprite.variants[sprite.variants.length - 1];
  }
  
  // Clear frame first
  frame.clear();
  
  // Draw sprite cells
  for (var row = 0; row < variant.length; row++) {
    for (var col = 0; col < variant[row].length; col++) {
      var cell = variant[row][col];
      if (cell !== null && cell !== undefined) {
        // frame.setData is 0-indexed
        frame.setData(col, row, cell.char, cell.attr);
      }
      // null cells stay transparent
    }
  }
}

/**
 * Get sprite dimensions for a specific scale.
 */
function getSpriteSize(sprite: SpriteDefinition, scaleIndex: number): { width: number; height: number } {
  var variant = sprite.variants[scaleIndex];
  if (!variant) {
    variant = sprite.variants[sprite.variants.length - 1];
  }
  
  var height = variant.length;
  var width = 0;
  for (var row = 0; row < variant.length; row++) {
    if (variant[row].length > width) {
      width = variant[row].length;
    }
  }
  
  return { width: width, height: height };
}

// Register nature sprites for theme system
registerRoadsideSprite('tree', SpriteSheet.createTree);
registerRoadsideSprite('rock', SpriteSheet.createRock);
registerRoadsideSprite('bush', SpriteSheet.createBush);
