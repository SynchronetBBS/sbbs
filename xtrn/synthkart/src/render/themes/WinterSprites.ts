/**
 * WinterSprites.ts - Snowy roadside objects for Winter Wonderland theme.
 * Snow-covered pines, snowmen, ice crystals, candy canes, and ski poles.
 */

var WinterSprites = {
  /**
   * Create a snow-covered pine tree sprite.
   */
  createSnowPine: function(): SpriteDefinition {
    var snow = makeAttr(WHITE, BG_BLACK);
    var pine = makeAttr(GREEN, BG_BLACK);
    var pineDark = makeAttr(CYAN, BG_BLACK);
    var trunk = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'snowpine',
      variants: [
        // Scale 0: 3x2 - tiny distant
        [
          [U, { char: '^', attr: snow }, U],
          [U, { char: '|', attr: trunk }, U]
        ],
        // Scale 1: 3x4
        [
          [U, { char: '*', attr: snow }, U],
          [{ char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }],
          [{ char: '/', attr: pineDark }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: '\\', attr: pineDark }],
          [U, { char: '|', attr: trunk }, U]
        ],
        // Scale 2: 5x5
        [
          [U, U, { char: '*', attr: snow }, U, U],
          [U, { char: '/', attr: pine }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: '\\', attr: pine }, U],
          [{ char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }],
          [{ char: '/', attr: pineDark }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: '\\', attr: pineDark }],
          [U, U, { char: '|', attr: trunk }, U, U]
        ],
        // Scale 3: 7x6
        [
          [U, U, U, { char: '*', attr: snow }, U, U, U],
          [U, U, { char: '/', attr: pine }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: '\\', attr: pine }, U, U],
          [U, { char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }, U],
          [{ char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }],
          [{ char: '/', attr: pineDark }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: '\\', attr: pineDark }],
          [U, U, U, { char: '|', attr: trunk }, U, U, U]
        ],
        // Scale 4: 7x7
        [
          [U, U, U, { char: '*', attr: snow }, U, U, U],
          [U, U, { char: '/', attr: pine }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: '\\', attr: pine }, U, U],
          [U, { char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }, U],
          [{ char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }],
          [{ char: '/', attr: pineDark }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: '\\', attr: pineDark }],
          [U, U, U, { char: '|', attr: trunk }, U, U, U],
          [U, U, U, { char: '|', attr: trunk }, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a snowman sprite.
   */
  createSnowman: function(): SpriteDefinition {
    var snow = makeAttr(WHITE, BG_BLACK);
    var hat = makeAttr(BLACK, BG_BLACK);
    var face = makeAttr(BLACK, BG_LIGHTGRAY);
    var carrot = makeAttr(BROWN, BG_LIGHTGRAY);
    var scarf = makeAttr(LIGHTRED, BG_BLACK);
    var button = makeAttr(BLACK, BG_LIGHTGRAY);
    var arm = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'snowman',
      variants: [
        // Scale 0: 2x3 - tiny
        [
          [{ char: 'o', attr: snow }, { char: 'o', attr: snow }],
          [{ char: 'O', attr: snow }, { char: 'O', attr: snow }],
          [{ char: 'O', attr: snow }, { char: 'O', attr: snow }]
        ],
        // Scale 1: 3x4
        [
          [U, { char: GLYPH.FULL_BLOCK, attr: hat }, U],
          [{ char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }],
          [{ char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }],
          [{ char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }]
        ],
        // Scale 2: 5x5
        [
          [U, { char: '_', attr: hat }, { char: GLYPH.FULL_BLOCK, attr: hat }, { char: '_', attr: hat }, U],
          [U, { char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }, U],
          [{ char: '-', attr: arm }, { char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }, { char: '-', attr: arm }],
          [U, { char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }, U],
          [U, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, U]
        ],
        // Scale 3: 5x6
        [
          [U, { char: '_', attr: hat }, { char: GLYPH.FULL_BLOCK, attr: hat }, { char: '_', attr: hat }, U],
          [U, { char: '.', attr: face }, { char: 'v', attr: carrot }, { char: '.', attr: face }, U],
          [U, { char: '~', attr: scarf }, { char: '~', attr: scarf }, { char: '~', attr: scarf }, U],
          [{ char: '-', attr: arm }, { char: '(', attr: snow }, { char: '*', attr: button }, { char: ')', attr: snow }, { char: '-', attr: arm }],
          [U, { char: '(', attr: snow }, { char: '*', attr: button }, { char: ')', attr: snow }, U],
          [U, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, U]
        ],
        // Scale 4: 7x7
        [
          [U, U, { char: '_', attr: hat }, { char: GLYPH.FULL_BLOCK, attr: hat }, { char: '_', attr: hat }, U, U],
          [U, { char: '(', attr: snow }, { char: '.', attr: face }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '.', attr: face }, { char: ')', attr: snow }, U],
          [U, { char: '(', attr: snow }, { char: ' ', attr: snow }, { char: '>', attr: carrot }, { char: ' ', attr: snow }, { char: ')', attr: snow }, U],
          [U, { char: '~', attr: scarf }, { char: '~', attr: scarf }, { char: '~', attr: scarf }, { char: '~', attr: scarf }, { char: '~', attr: scarf }, U],
          [{ char: '/', attr: arm }, { char: '(', attr: snow }, { char: ' ', attr: snow }, { char: '*', attr: button }, { char: ' ', attr: snow }, { char: ')', attr: snow }, { char: '\\', attr: arm }],
          [U, { char: '(', attr: snow }, { char: ' ', attr: snow }, { char: '*', attr: button }, { char: ' ', attr: snow }, { char: ')', attr: snow }, U],
          [U, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, U]
        ]
      ]
    };
  },
  
  /**
   * Create an ice crystal / icicle sprite.
   */
  createIceCrystal: function(): SpriteDefinition {
    var ice = makeAttr(LIGHTCYAN, BG_BLACK);
    var iceShine = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'icecrystal',
      variants: [
        // Scale 0: 1x2
        [
          [{ char: '*', attr: iceShine }],
          [{ char: 'V', attr: ice }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '*', attr: iceShine }, U],
          [{ char: '/', attr: ice }, { char: '|', attr: ice }, { char: '\\', attr: ice }],
          [U, { char: 'V', attr: ice }, U]
        ],
        // Scale 2: 5x4
        [
          [U, U, { char: '*', attr: iceShine }, U, U],
          [{ char: '/', attr: ice }, { char: '/', attr: ice }, { char: '|', attr: ice }, { char: '\\', attr: ice }, { char: '\\', attr: ice }],
          [U, { char: '/', attr: ice }, { char: '|', attr: iceShine }, { char: '\\', attr: ice }, U],
          [U, U, { char: 'V', attr: ice }, U, U]
        ],
        // Scale 3: 5x5
        [
          [U, U, { char: '*', attr: iceShine }, U, U],
          [{ char: '\\', attr: ice }, { char: '/', attr: ice }, { char: '|', attr: ice }, { char: '\\', attr: ice }, { char: '/', attr: ice }],
          [U, { char: '<', attr: ice }, { char: '+', attr: iceShine }, { char: '>', attr: ice }, U],
          [{ char: '/', attr: ice }, { char: '\\', attr: ice }, { char: '|', attr: ice }, { char: '/', attr: ice }, { char: '\\', attr: ice }],
          [U, U, { char: 'V', attr: ice }, U, U]
        ],
        // Scale 4: 5x5 (same)
        [
          [U, U, { char: '*', attr: iceShine }, U, U],
          [{ char: '\\', attr: ice }, { char: '/', attr: ice }, { char: '|', attr: ice }, { char: '\\', attr: ice }, { char: '/', attr: ice }],
          [U, { char: '<', attr: ice }, { char: '+', attr: iceShine }, { char: '>', attr: ice }, U],
          [{ char: '/', attr: ice }, { char: '\\', attr: ice }, { char: '|', attr: ice }, { char: '/', attr: ice }, { char: '\\', attr: ice }],
          [U, U, { char: 'V', attr: ice }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a candy cane sprite.
   */
  createCandyCane: function(): SpriteDefinition {
    var red = makeAttr(LIGHTRED, BG_BLACK);
    var white = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'candycane',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '?', attr: red }],
          [{ char: '|', attr: white }]
        ],
        // Scale 1: 2x3
        [
          [{ char: '_', attr: red }, { char: ')', attr: red }],
          [{ char: '|', attr: white }, U],
          [{ char: '|', attr: red }, U]
        ],
        // Scale 2: 3x4
        [
          [{ char: '_', attr: red }, { char: '_', attr: white }, { char: ')', attr: red }],
          [{ char: '|', attr: white }, U, U],
          [{ char: '|', attr: red }, U, U],
          [{ char: '|', attr: white }, U, U]
        ],
        // Scale 3: 3x5
        [
          [{ char: '_', attr: red }, { char: '_', attr: white }, { char: ')', attr: red }],
          [{ char: '|', attr: white }, U, { char: '/', attr: white }],
          [{ char: '|', attr: red }, U, U],
          [{ char: '|', attr: white }, U, U],
          [{ char: '|', attr: red }, U, U]
        ],
        // Scale 4: 3x5 (same)
        [
          [{ char: '_', attr: red }, { char: '_', attr: white }, { char: ')', attr: red }],
          [{ char: '|', attr: white }, U, { char: '/', attr: white }],
          [{ char: '|', attr: red }, U, U],
          [{ char: '|', attr: white }, U, U],
          [{ char: '|', attr: red }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a snow drift / pile sprite.
   */
  createSnowDrift: function(): SpriteDefinition {
    var snow = makeAttr(WHITE, BG_BLACK);
    var snowShade = makeAttr(LIGHTGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'snowdrift',
      variants: [
        // Scale 0: 3x1
        [
          [{ char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }]
        ],
        // Scale 1: 4x2
        [
          [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
          [{ char: GLYPH.LOWER_HALF, attr: snowShade }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snowShade }]
        ],
        // Scale 2: 5x2
        [
          [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
          [{ char: GLYPH.LOWER_HALF, attr: snowShade }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snowShade }]
        ],
        // Scale 3: 6x3
        [
          [U, U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U, U],
          [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
          [{ char: GLYPH.LOWER_HALF, attr: snowShade }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snowShade }]
        ],
        // Scale 4: 6x3 (same)
        [
          [U, U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U, U],
          [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
          [{ char: GLYPH.LOWER_HALF, attr: snowShade }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snowShade }]
        ]
      ]
    };
  },
  
  /**
   * Create a wooden signpost sprite.
   */
  createSignpost: function(): SpriteDefinition {
    var wood = makeAttr(BROWN, BG_BLACK);
    var snow = makeAttr(WHITE, BG_BLACK);
    var text = makeAttr(WHITE, BG_BROWN);
    var U: SpriteCell | null = null;
    
    return {
      name: 'signpost',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '[', attr: wood }, { char: ']', attr: wood }],
          [U, { char: '|', attr: wood }]
        ],
        // Scale 1: 3x3
        [
          [{ char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }],
          [{ char: '[', attr: wood }, { char: '>', attr: text }, { char: ']', attr: wood }],
          [U, { char: '|', attr: wood }, U]
        ],
        // Scale 2: 4x4
        [
          [{ char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }],
          [{ char: '[', attr: wood }, { char: '>', attr: text }, { char: '>', attr: text }, { char: ']', attr: wood }],
          [U, { char: '|', attr: wood }, { char: '|', attr: wood }, U],
          [U, { char: '|', attr: wood }, { char: '|', attr: wood }, U]
        ],
        // Scale 3: 5x4
        [
          [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
          [{ char: '[', attr: wood }, { char: 'S', attr: text }, { char: 'K', attr: text }, { char: 'I', attr: text }, { char: '>', attr: wood }],
          [U, U, { char: '|', attr: wood }, U, U],
          [U, U, { char: '|', attr: wood }, U, U]
        ],
        // Scale 4: 5x4 (same)
        [
          [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
          [{ char: '[', attr: wood }, { char: 'S', attr: text }, { char: 'K', attr: text }, { char: 'I', attr: text }, { char: '>', attr: wood }],
          [U, U, { char: '|', attr: wood }, U, U],
          [U, U, { char: '|', attr: wood }, U, U]
        ]
      ]
    };
  }
};

// Register winter sprites
registerRoadsideSprite('snowpine', WinterSprites.createSnowPine);
registerRoadsideSprite('snowman', WinterSprites.createSnowman);
registerRoadsideSprite('icecrystal', WinterSprites.createIceCrystal);
registerRoadsideSprite('candycane', WinterSprites.createCandyCane);
registerRoadsideSprite('snowdrift', WinterSprites.createSnowDrift);
registerRoadsideSprite('signpost', WinterSprites.createSignpost);
