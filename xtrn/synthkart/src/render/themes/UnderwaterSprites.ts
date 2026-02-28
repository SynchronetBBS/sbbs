/**
 * UnderwaterSprites.ts - Underwater/aquarium themed roadside sprites.
 * Colorful fish, coral, seaweed, anemones, and marine life.
 */

var UnderwaterSprites = {
  /**
   * Create a tropical fish sprite - clearly fish-shaped with fins.
   */
  createFish: function(): SpriteDefinition {
    var body1 = makeAttr(YELLOW, BG_BLUE);
    var body2 = makeAttr(LIGHTCYAN, BG_BLUE);
    var eye = makeAttr(WHITE, BG_BLUE);
    var fin = makeAttr(LIGHTRED, BG_BLUE);
    var stripe = makeAttr(WHITE, BG_BLUE);
    var U: SpriteCell | null = null;
    
    return {
      name: 'underwater_fish',
      variants: [
        // Scale 0: 2x1 - tiny fish
        [
          [{ char: '<', attr: body1 }, { char: '>', attr: body1 }]
        ],
        // Scale 1: 3x2 - small fish with fin
        [
          [U, { char: GLYPH.UPPER_HALF, attr: fin }, U],
          [{ char: '<', attr: body2 }, { char: GLYPH.FULL_BLOCK, attr: body2 }, { char: '>', attr: body2 }]
        ],
        // Scale 2: 5x3 - medium fish with eye and stripes
        [
          [U, { char: '/', attr: fin }, { char: GLYPH.UPPER_HALF, attr: fin }, { char: '\\', attr: fin }, U],
          [{ char: '<', attr: body1 }, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: '|', attr: stripe }, { char: 'O', attr: eye }, { char: '>', attr: body1 }],
          [U, { char: '\\', attr: fin }, { char: GLYPH.LOWER_HALF, attr: fin }, { char: '/', attr: fin }, U]
        ],
        // Scale 3: 6x3 - larger fish
        [
          [U, U, { char: '/', attr: fin }, { char: GLYPH.FULL_BLOCK, attr: fin }, { char: '\\', attr: fin }, U],
          [{ char: '<', attr: body2 }, { char: '<', attr: body2 }, { char: GLYPH.FULL_BLOCK, attr: body2 }, { char: '|', attr: stripe }, { char: 'O', attr: eye }, { char: '>', attr: body2 }],
          [U, U, { char: '\\', attr: fin }, { char: GLYPH.FULL_BLOCK, attr: fin }, { char: '/', attr: fin }, U]
        ],
        // Scale 4: 7x4 - large fish with detail
        [
          [U, U, { char: '/', attr: fin }, { char: '_', attr: fin }, { char: GLYPH.FULL_BLOCK, attr: fin }, { char: '\\', attr: fin }, U],
          [{ char: '<', attr: body1 }, { char: '<', attr: body1 }, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: '|', attr: stripe }, { char: 'O', attr: eye }, { char: '>', attr: body1 }],
          [U, U, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: '|', attr: stripe }, { char: GLYPH.FULL_BLOCK, attr: body1 }, U],
          [U, U, { char: '\\', attr: fin }, { char: GLYPH.LOWER_HALF, attr: fin }, { char: GLYPH.FULL_BLOCK, attr: fin }, { char: '/', attr: fin }, U]
        ]
      ]
    };
  },
  
  /**
   * Create coral formation sprite - branching colorful coral.
   */
  createCoral: function(): SpriteDefinition {
    var coral1 = makeAttr(LIGHTRED, BG_BLUE);
    var coral2 = makeAttr(LIGHTMAGENTA, BG_BLUE);
    var coral3 = makeAttr(YELLOW, BG_BLUE);
    var base = makeAttr(DARKGRAY, BG_BLUE);
    var U: SpriteCell | null = null;
    
    return {
      name: 'underwater_coral',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: '*', attr: coral1 }]
        ],
        // Scale 1: 2x2
        [
          [{ char: 'Y', attr: coral1 }, { char: 'Y', attr: coral2 }],
          [{ char: '|', attr: base }, { char: '|', attr: base }]
        ],
        // Scale 2: 3x3
        [
          [{ char: '*', attr: coral1 }, { char: '*', attr: coral2 }, { char: '*', attr: coral1 }],
          [{ char: 'Y', attr: coral1 }, { char: '|', attr: coral2 }, { char: 'Y', attr: coral1 }],
          [U, { char: '|', attr: base }, U]
        ],
        // Scale 3: 4x4
        [
          [U, { char: '*', attr: coral1 }, { char: '*', attr: coral3 }, U],
          [{ char: 'Y', attr: coral1 }, { char: '|', attr: coral1 }, { char: '|', attr: coral3 }, { char: 'Y', attr: coral3 }],
          [{ char: '|', attr: coral1 }, { char: '/', attr: coral1 }, { char: '\\', attr: coral3 }, { char: '|', attr: coral3 }],
          [U, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, U]
        ],
        // Scale 4: 5x5
        [
          [U, { char: '*', attr: coral1 }, { char: '*', attr: coral2 }, { char: '*', attr: coral3 }, U],
          [{ char: '*', attr: coral1 }, { char: 'Y', attr: coral1 }, { char: '|', attr: coral2 }, { char: 'Y', attr: coral3 }, { char: '*', attr: coral3 }],
          [{ char: '|', attr: coral1 }, { char: '/', attr: coral1 }, { char: '|', attr: coral2 }, { char: '\\', attr: coral3 }, { char: '|', attr: coral3 }],
          [U, { char: '|', attr: base }, { char: '|', attr: base }, { char: '|', attr: base }, U],
          [U, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, U]
        ]
      ]
    };
  },
  
  /**
   * Create seaweed sprite - swaying plants with leaves.
   */
  createSeaweed: function(): SpriteDefinition {
    var leaf1 = makeAttr(LIGHTGREEN, BG_BLUE);
    var leaf2 = makeAttr(GREEN, BG_BLUE);
    var U: SpriteCell | null = null;
    
    return {
      name: 'underwater_seaweed',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: '|', attr: leaf2 }]
        ],
        // Scale 1: 1x2
        [
          [{ char: ')', attr: leaf1 }],
          [{ char: '|', attr: leaf2 }]
        ],
        // Scale 2: 2x3
        [
          [{ char: '(', attr: leaf1 }, { char: ')', attr: leaf1 }],
          [{ char: ')', attr: leaf2 }, { char: '(', attr: leaf2 }],
          [{ char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }]
        ],
        // Scale 3: 3x4
        [
          [U, { char: '~', attr: leaf1 }, U],
          [{ char: '(', attr: leaf1 }, { char: ')', attr: leaf1 }, { char: '(', attr: leaf1 }],
          [{ char: ')', attr: leaf2 }, { char: '(', attr: leaf2 }, { char: ')', attr: leaf2 }],
          [{ char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }]
        ],
        // Scale 4: 4x5
        [
          [U, { char: '~', attr: leaf1 }, { char: '~', attr: leaf1 }, U],
          [{ char: '(', attr: leaf1 }, { char: ')', attr: leaf1 }, { char: '(', attr: leaf1 }, { char: ')', attr: leaf1 }],
          [{ char: ')', attr: leaf2 }, { char: '(', attr: leaf2 }, { char: ')', attr: leaf2 }, { char: '(', attr: leaf2 }],
          [{ char: '(', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: ')', attr: leaf2 }],
          [{ char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }]
        ]
      ]
    };
  },
  
  /**
   * Create sea anemone sprite - waving tentacles with clownfish hiding.
   */
  createAnemone: function(): SpriteDefinition {
    var tentacle1 = makeAttr(LIGHTMAGENTA, BG_BLUE);
    var tentacle2 = makeAttr(MAGENTA, BG_BLUE);
    var center = makeAttr(YELLOW, BG_BLUE);
    var fish = makeAttr(LIGHTRED, BG_BLUE);  // Clownfish
    var base = makeAttr(BROWN, BG_BLUE);
    var U: SpriteCell | null = null;
    
    return {
      name: 'underwater_anemone',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: '*', attr: tentacle1 }]
        ],
        // Scale 1: 2x2 - simple anemone
        [
          [{ char: ')', attr: tentacle1 }, { char: '(', attr: tentacle1 }],
          [{ char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }]
        ],
        // Scale 2: 3x3 - with center
        [
          [{ char: ')', attr: tentacle1 }, { char: '|', attr: tentacle2 }, { char: '(', attr: tentacle1 }],
          [{ char: '(', attr: tentacle2 }, { char: 'O', attr: center }, { char: ')', attr: tentacle2 }],
          [U, { char: GLYPH.FULL_BLOCK, attr: base }, U]
        ],
        // Scale 3: 4x4 - waving tentacles
        [
          [{ char: '~', attr: tentacle1 }, { char: ')', attr: tentacle1 }, { char: '(', attr: tentacle1 }, { char: '~', attr: tentacle1 }],
          [{ char: ')', attr: tentacle2 }, { char: '|', attr: tentacle2 }, { char: '|', attr: tentacle2 }, { char: '(', attr: tentacle2 }],
          [{ char: '(', attr: tentacle2 }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: ')', attr: tentacle2 }],
          [U, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, U]
        ],
        // Scale 4: 5x5 - with clownfish hiding in tentacles!
        [
          [{ char: '~', attr: tentacle1 }, { char: ')', attr: tentacle1 }, { char: '<', attr: fish }, { char: '(', attr: tentacle1 }, { char: '~', attr: tentacle1 }],
          [{ char: ')', attr: tentacle1 }, { char: '|', attr: tentacle2 }, { char: '>', attr: fish }, { char: '|', attr: tentacle2 }, { char: '(', attr: tentacle1 }],
          [{ char: '(', attr: tentacle2 }, { char: ')', attr: tentacle2 }, { char: 'O', attr: center }, { char: '(', attr: tentacle2 }, { char: ')', attr: tentacle2 }],
          [{ char: '|', attr: tentacle2 }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: '|', attr: tentacle2 }],
          [U, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, U]
        ]
      ]
    };
  },
  
  /**
   * Create jellyfish sprite - glowing translucent floater.
   */
  createJellyfish: function(): SpriteDefinition {
    var body = makeAttr(LIGHTMAGENTA, BG_BLUE);
    var glow = makeAttr(LIGHTCYAN, BG_BLUE);
    var tent = makeAttr(MAGENTA, BG_BLUE);
    var U: SpriteCell | null = null;
    
    return {
      name: 'underwater_jellyfish',
      variants: [
        // Scale 0: 1x2
        [
          [{ char: 'n', attr: body }],
          [{ char: '|', attr: tent }]
        ],
        // Scale 1: 2x3
        [
          [{ char: '/', attr: glow }, { char: '\\', attr: glow }],
          [{ char: '(', attr: body }, { char: ')', attr: body }],
          [{ char: '|', attr: tent }, { char: '|', attr: tent }]
        ],
        // Scale 2: 3x4
        [
          [U, { char: GLYPH.UPPER_HALF, attr: glow }, U],
          [{ char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }],
          [{ char: '(', attr: tent }, { char: '~', attr: tent }, { char: ')', attr: tent }],
          [{ char: '|', attr: tent }, U, { char: '|', attr: tent }]
        ],
        // Scale 3: 4x5
        [
          [U, { char: '_', attr: glow }, { char: '_', attr: glow }, U],
          [{ char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }],
          [{ char: '|', attr: body }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: '|', attr: body }],
          [{ char: '(', attr: tent }, { char: '~', attr: tent }, { char: '~', attr: tent }, { char: ')', attr: tent }],
          [{ char: '|', attr: tent }, { char: '|', attr: tent }, { char: '|', attr: tent }, { char: '|', attr: tent }]
        ],
        // Scale 4: 5x6
        [
          [U, { char: '_', attr: glow }, { char: '_', attr: glow }, { char: '_', attr: glow }, U],
          [{ char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }],
          [{ char: '|', attr: body }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: '|', attr: body }],
          [{ char: '\\', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '/', attr: body }],
          [{ char: '(', attr: tent }, { char: '~', attr: tent }, { char: '|', attr: tent }, { char: '~', attr: tent }, { char: ')', attr: tent }],
          [{ char: '|', attr: tent }, U, { char: '|', attr: tent }, U, { char: '|', attr: tent }]
        ]
      ]
    };
  },
  
  /**
   * Create treasure chest sprite - pirate loot!
   */
  createTreasure: function(): SpriteDefinition {
    var chest = makeAttr(BROWN, BG_BLUE);
    var gold = makeAttr(YELLOW, BG_BLUE);
    var shine = makeAttr(WHITE, BG_BLUE);
    var U: SpriteCell | null = null;
    
    return {
      name: 'underwater_treasure',
      variants: [
        // Scale 0: 1x1
        [
          [{ char: '#', attr: chest }]
        ],
        // Scale 1: 2x1
        [
          [{ char: '[', attr: chest }, { char: ']', attr: chest }]
        ],
        // Scale 2: 3x2
        [
          [{ char: '$', attr: gold }, { char: '*', attr: shine }, { char: '$', attr: gold }],
          [{ char: '[', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: ']', attr: chest }]
        ],
        // Scale 3: 4x3
        [
          [U, { char: '$', attr: gold }, { char: '$', attr: gold }, U],
          [{ char: '/', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: chest }],
          [{ char: '[', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: ']', attr: chest }]
        ],
        // Scale 4: 5x4
        [
          [U, { char: '$', attr: gold }, { char: '*', attr: shine }, { char: '$', attr: gold }, U],
          [{ char: '/', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: chest }],
          [{ char: '[', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: 'O', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: ']', attr: chest }],
          [{ char: '[', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: ']', attr: chest }]
        ]
      ]
    };
  }
};

// Register all underwater sprites
registerRoadsideSprite('underwater_fish', function() { return UnderwaterSprites.createFish(); });
registerRoadsideSprite('underwater_coral', function() { return UnderwaterSprites.createCoral(); });
registerRoadsideSprite('underwater_seaweed', function() { return UnderwaterSprites.createSeaweed(); });
registerRoadsideSprite('underwater_anemone', function() { return UnderwaterSprites.createAnemone(); });
registerRoadsideSprite('underwater_jellyfish', function() { return UnderwaterSprites.createJellyfish(); });
registerRoadsideSprite('underwater_treasure', function() { return UnderwaterSprites.createTreasure(); });
