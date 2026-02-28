/**
 * DesertSprites.ts - Arid roadside objects for Desert theme.
 * Saguaro cacti, tumbleweeds, cow skulls, desert rocks, and old west signs.
 */

var DesertSprites = {
  /**
   * Create a saguaro cactus sprite - classic desert icon.
   */
  createSaguaro: function(): SpriteDefinition {
    var cactus = makeAttr(GREEN, BG_BLACK);
    var cactusDark = makeAttr(CYAN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'saguaro',
      variants: [
        // Scale 0: 1x3 - tiny distant
        [
          [{ char: '|', attr: cactus }],
          [{ char: '|', attr: cactus }],
          [{ char: '|', attr: cactus }]
        ],
        // Scale 1: 3x4
        [
          [U, { char: '|', attr: cactus }, U],
          [{ char: '/', attr: cactus }, { char: '|', attr: cactus }, { char: '\\', attr: cactus }],
          [{ char: '|', attr: cactusDark }, { char: '|', attr: cactus }, { char: '|', attr: cactusDark }],
          [U, { char: '|', attr: cactus }, U]
        ],
        // Scale 2: 5x5
        [
          [U, U, { char: '(', attr: cactus }, U, U],
          [{ char: '_', attr: cactus }, U, { char: '|', attr: cactus }, U, { char: '_', attr: cactus }],
          [{ char: '|', attr: cactusDark }, { char: '/', attr: cactus }, { char: '|', attr: cactus }, { char: '\\', attr: cactus }, { char: '|', attr: cactusDark }],
          [{ char: '|', attr: cactusDark }, U, { char: '|', attr: cactus }, U, { char: '|', attr: cactusDark }],
          [U, U, { char: '|', attr: cactus }, U, U]
        ],
        // Scale 3: 7x6
        [
          [U, U, U, { char: '(', attr: cactus }, U, U, U],
          [{ char: '_', attr: cactus }, U, U, { char: '|', attr: cactus }, U, U, { char: '_', attr: cactus }],
          [{ char: '|', attr: cactusDark }, U, { char: '/', attr: cactus }, { char: '|', attr: cactus }, { char: '\\', attr: cactus }, U, { char: '|', attr: cactusDark }],
          [{ char: '|', attr: cactusDark }, { char: '/', attr: cactus }, U, { char: '|', attr: cactus }, U, { char: '\\', attr: cactus }, { char: '|', attr: cactusDark }],
          [U, { char: '|', attr: cactusDark }, U, { char: '|', attr: cactus }, U, { char: '|', attr: cactusDark }, U],
          [U, U, U, { char: '|', attr: cactus }, U, U, U]
        ],
        // Scale 4: 7x7
        [
          [U, U, U, { char: '(', attr: cactus }, U, U, U],
          [{ char: '_', attr: cactus }, U, U, { char: '|', attr: cactus }, U, U, { char: '_', attr: cactus }],
          [{ char: '|', attr: cactusDark }, U, { char: '/', attr: cactus }, { char: '|', attr: cactus }, { char: '\\', attr: cactus }, U, { char: '|', attr: cactusDark }],
          [{ char: '|', attr: cactusDark }, { char: '/', attr: cactus }, U, { char: '|', attr: cactus }, U, { char: '\\', attr: cactus }, { char: '|', attr: cactusDark }],
          [U, { char: '|', attr: cactusDark }, U, { char: '|', attr: cactus }, U, { char: '|', attr: cactusDark }, U],
          [U, { char: '|', attr: cactusDark }, U, { char: '|', attr: cactus }, U, { char: '|', attr: cactusDark }, U],
          [U, U, U, { char: '|', attr: cactus }, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a barrel cactus sprite - round and spiky.
   */
  createBarrelCactus: function(): SpriteDefinition {
    var cactus = makeAttr(GREEN, BG_BLACK);
    var spine = makeAttr(YELLOW, BG_BLACK);
    var flower = makeAttr(LIGHTRED, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'barrel',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '(', attr: cactus }, { char: ')', attr: cactus }],
          [{ char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }]
        ],
        // Scale 1: 3x2
        [
          [{ char: '(', attr: cactus }, { char: '*', attr: flower }, { char: ')', attr: cactus }],
          [{ char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }]
        ],
        // Scale 2: 4x3
        [
          [U, { char: '*', attr: flower }, { char: '*', attr: flower }, U],
          [{ char: '(', attr: cactus }, { char: '|', attr: spine }, { char: '|', attr: spine }, { char: ')', attr: cactus }],
          [{ char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }]
        ],
        // Scale 3: 5x4
        [
          [U, U, { char: '@', attr: flower }, U, U],
          [U, { char: '/', attr: spine }, { char: GLYPH.UPPER_HALF, attr: cactus }, { char: '\\', attr: spine }, U],
          [{ char: '(', attr: cactus }, { char: '|', attr: spine }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: '|', attr: spine }, { char: ')', attr: cactus }],
          [U, { char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }, U]
        ],
        // Scale 4: 5x4 (same)
        [
          [U, U, { char: '@', attr: flower }, U, U],
          [U, { char: '/', attr: spine }, { char: GLYPH.UPPER_HALF, attr: cactus }, { char: '\\', attr: spine }, U],
          [{ char: '(', attr: cactus }, { char: '|', attr: spine }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: '|', attr: spine }, { char: ')', attr: cactus }],
          [U, { char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a tumbleweed sprite - rolling desert plant.
   */
  createTumbleweed: function(): SpriteDefinition {
    var weed = makeAttr(BROWN, BG_BLACK);
    var weedLight = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'tumbleweed',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '@', attr: weed }, { char: '@', attr: weed }],
          [{ char: '@', attr: weed }, { char: '@', attr: weed }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '@', attr: weedLight }, U],
          [{ char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weed }],
          [U, { char: '@', attr: weed }, U]
        ],
        // Scale 2: 4x3
        [
          [U, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, U],
          [{ char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weed }],
          [U, { char: '@', attr: weed }, { char: '@', attr: weed }, U]
        ],
        // Scale 3: 5x4
        [
          [U, { char: '~', attr: weedLight }, { char: '@', attr: weedLight }, { char: '~', attr: weedLight }, U],
          [{ char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weed }],
          [{ char: '@', attr: weed }, { char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weed }, { char: '@', attr: weed }],
          [U, { char: '~', attr: weed }, { char: '@', attr: weed }, { char: '~', attr: weed }, U]
        ],
        // Scale 4: 5x4 (same)
        [
          [U, { char: '~', attr: weedLight }, { char: '@', attr: weedLight }, { char: '~', attr: weedLight }, U],
          [{ char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weed }],
          [{ char: '@', attr: weed }, { char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weed }, { char: '@', attr: weed }],
          [U, { char: '~', attr: weed }, { char: '@', attr: weed }, { char: '~', attr: weed }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a cow skull sprite - bleached bones in the desert.
   */
  createCowSkull: function(): SpriteDefinition {
    var bone = makeAttr(WHITE, BG_BLACK);
    var eye = makeAttr(BLACK, BG_LIGHTGRAY);
    var U: SpriteCell | null = null;
    
    return {
      name: 'cowskull',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: '\\', attr: bone }, { char: 'o', attr: bone }, { char: '/', attr: bone }],
          [U, { char: 'V', attr: bone }, U]
        ],
        // Scale 1: 4x3
        [
          [{ char: '~', attr: bone }, U, U, { char: '~', attr: bone }],
          [{ char: '(', attr: bone }, { char: 'o', attr: eye }, { char: 'o', attr: eye }, { char: ')', attr: bone }],
          [U, { char: '\\', attr: bone }, { char: '/', attr: bone }, U]
        ],
        // Scale 2: 5x4
        [
          [{ char: '~', attr: bone }, { char: '_', attr: bone }, U, { char: '_', attr: bone }, { char: '~', attr: bone }],
          [{ char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }],
          [U, { char: '\\', attr: bone }, { char: 'v', attr: bone }, { char: '/', attr: bone }, U],
          [U, U, { char: 'w', attr: bone }, U, U]
        ],
        // Scale 3: 7x5
        [
          [{ char: '/', attr: bone }, { char: '~', attr: bone }, { char: '_', attr: bone }, U, { char: '_', attr: bone }, { char: '~', attr: bone }, { char: '\\', attr: bone }],
          [U, { char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }, U],
          [U, { char: '(', attr: bone }, { char: ' ', attr: bone }, { char: 'v', attr: bone }, { char: ' ', attr: bone }, { char: ')', attr: bone }, U],
          [U, U, { char: '\\', attr: bone }, { char: 'w', attr: bone }, { char: '/', attr: bone }, U, U],
          [U, U, U, { char: '|', attr: bone }, U, U, U]
        ],
        // Scale 4: 7x5 (same)
        [
          [{ char: '/', attr: bone }, { char: '~', attr: bone }, { char: '_', attr: bone }, U, { char: '_', attr: bone }, { char: '~', attr: bone }, { char: '\\', attr: bone }],
          [U, { char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }, U],
          [U, { char: '(', attr: bone }, { char: ' ', attr: bone }, { char: 'v', attr: bone }, { char: ' ', attr: bone }, { char: ')', attr: bone }, U],
          [U, U, { char: '\\', attr: bone }, { char: 'w', attr: bone }, { char: '/', attr: bone }, U, U],
          [U, U, U, { char: '|', attr: bone }, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a desert rock/boulder sprite.
   */
  createDesertRock: function(): SpriteDefinition {
    var rock = makeAttr(BROWN, BG_BLACK);
    var rockDark = makeAttr(DARKGRAY, BG_BLACK);
    var rockLight = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'desertrock',
      variants: [
        // Scale 0: 2x1
        [
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rock }]
        ],
        // Scale 1: 3x2
        [
          [U, { char: GLYPH.UPPER_HALF, attr: rockLight }, U],
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rockDark }]
        ],
        // Scale 2: 4x3
        [
          [U, { char: '_', attr: rockLight }, { char: '_', attr: rockLight }, U],
          [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.MEDIUM_SHADE, attr: rockDark }, { char: '\\', attr: rockDark }],
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.LOWER_HALF, attr: rockDark }]
        ],
        // Scale 3: 5x3
        [
          [U, { char: '_', attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: '_', attr: rockLight }, U],
          [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.MEDIUM_SHADE, attr: rockDark }, { char: '\\', attr: rockDark }],
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.LOWER_HALF, attr: rockDark }]
        ],
        // Scale 4: 5x3 (same)
        [
          [U, { char: '_', attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: '_', attr: rockLight }, U],
          [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.MEDIUM_SHADE, attr: rockDark }, { char: '\\', attr: rockDark }],
          [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.LOWER_HALF, attr: rockDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a wooden signpost sprite - old west style.
   */
  createWesternSign: function(): SpriteDefinition {
    var wood = makeAttr(BROWN, BG_BLACK);
    var text = makeAttr(YELLOW, BG_BROWN);
    var U: SpriteCell | null = null;
    
    return {
      name: 'westernsign',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '[', attr: wood }, { char: ']', attr: wood }],
          [U, { char: '|', attr: wood }]
        ],
        // Scale 1: 3x3
        [
          [{ char: '[', attr: wood }, { char: '>', attr: text }, { char: ']', attr: wood }],
          [U, { char: '|', attr: wood }, U],
          [U, { char: '|', attr: wood }, U]
        ],
        // Scale 2: 5x4
        [
          [{ char: '[', attr: wood }, { char: 'G', attr: text }, { char: 'O', attr: text }, { char: '!', attr: text }, { char: '>', attr: wood }],
          [U, U, { char: '|', attr: wood }, U, U],
          [U, U, { char: '|', attr: wood }, U, U],
          [U, U, { char: '|', attr: wood }, U, U]
        ],
        // Scale 3: 6x5
        [
          [{ char: '<', attr: wood }, { char: 'W', attr: text }, { char: 'E', attr: text }, { char: 'S', attr: text }, { char: 'T', attr: text }, { char: '>', attr: wood }],
          [U, U, U, { char: '|', attr: wood }, U, U],
          [U, U, U, { char: '|', attr: wood }, U, U],
          [U, U, U, { char: '|', attr: wood }, U, U],
          [U, U, { char: '-', attr: wood }, { char: '+', attr: wood }, { char: '-', attr: wood }, U]
        ],
        // Scale 4: 6x5 (same)
        [
          [{ char: '<', attr: wood }, { char: 'W', attr: text }, { char: 'E', attr: text }, { char: 'S', attr: text }, { char: 'T', attr: text }, { char: '>', attr: wood }],
          [U, U, U, { char: '|', attr: wood }, U, U],
          [U, U, U, { char: '|', attr: wood }, U, U],
          [U, U, U, { char: '|', attr: wood }, U, U],
          [U, U, { char: '-', attr: wood }, { char: '+', attr: wood }, { char: '-', attr: wood }, U]
        ]
      ]
    };
  }
};

// Register desert sprites
registerRoadsideSprite('saguaro', DesertSprites.createSaguaro);
registerRoadsideSprite('barrel', DesertSprites.createBarrelCactus);
registerRoadsideSprite('tumbleweed', DesertSprites.createTumbleweed);
registerRoadsideSprite('cowskull', DesertSprites.createCowSkull);
registerRoadsideSprite('desertrock', DesertSprites.createDesertRock);
registerRoadsideSprite('westernsign', DesertSprites.createWesternSign);
