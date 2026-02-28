/**
 * JungleSprites.ts - Tropical rainforest roadside objects.
 * Jungle trees, ferns, vines, flowers, parrots, and banana plants.
 */

var JungleSprites = {
  /**
   * Create a jungle tree sprite - dense tropical canopy.
   */
  createJungleTree: function(): SpriteDefinition {
    var leaf = makeAttr(GREEN, BG_BLACK);
    var leafBright = makeAttr(LIGHTGREEN, BG_BLACK);
    var trunk = makeAttr(BROWN, BG_BLACK);
    var trunkDark = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'jungle_tree',
      variants: [
        // Scale 0: 2x3 - tiny distant
        [
          [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }],
          [U, { char: '|', attr: trunk }],
          [U, { char: '|', attr: trunk }]
        ],
        // Scale 1: 3x4
        [
          [{ char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }],
          [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }],
          [U, { char: '|', attr: trunk }, U],
          [U, { char: '|', attr: trunk }, U]
        ],
        // Scale 2: 5x5
        [
          [U, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, U],
          [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }],
          [{ char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '|', attr: trunk }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }],
          [U, U, { char: '|', attr: trunk }, U, U],
          [U, U, { char: '|', attr: trunkDark }, U, U]
        ],
        // Scale 3: 7x6
        [
          [U, U, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, U, U],
          [U, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, U],
          [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }],
          [{ char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '\\', attr: trunk }, { char: '|', attr: trunk }, { char: '/', attr: trunk }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }],
          [U, U, U, { char: '|', attr: trunk }, U, U, U],
          [U, U, U, { char: '|', attr: trunkDark }, U, U, U]
        ],
        // Scale 4: 9x7
        [
          [U, U, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, U, U],
          [U, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, U],
          [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }],
          [{ char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '|', attr: trunk }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }],
          [U, U, { char: '@', attr: leaf }, { char: '\\', attr: trunk }, { char: '|', attr: trunk }, { char: '/', attr: trunk }, { char: '@', attr: leaf }, U, U],
          [U, U, U, U, { char: '|', attr: trunk }, U, U, U, U],
          [U, U, U, U, { char: '|', attr: trunkDark }, U, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a fern sprite - lush ground foliage.
   */
  createFern: function(): SpriteDefinition {
    var frond = makeAttr(GREEN, BG_BLACK);
    var frondLight = makeAttr(LIGHTGREEN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'fern',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '/', attr: frond }, { char: '\\', attr: frond }],
          [{ char: '/', attr: frondLight }, { char: '\\', attr: frondLight }]
        ],
        // Scale 1: 3x2
        [
          [{ char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }],
          [{ char: '/', attr: frondLight }, { char: '|', attr: frond }, { char: '\\', attr: frondLight }]
        ],
        // Scale 2: 5x3
        [
          [{ char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }],
          [{ char: '/', attr: frond }, { char: '/', attr: frondLight }, { char: '|', attr: frond }, { char: '\\', attr: frondLight }, { char: '\\', attr: frond }],
          [U, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, U]
        ],
        // Scale 3: 7x4
        [
          [{ char: '/', attr: frond }, { char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }, { char: '\\', attr: frond }],
          [U, { char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }, U],
          [U, U, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, U, U],
          [U, U, U, { char: '|', attr: frond }, U, U, U]
        ],
        // Scale 4: 7x4 (same)
        [
          [{ char: '/', attr: frond }, { char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }, { char: '\\', attr: frond }],
          [U, { char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }, U],
          [U, U, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, U, U],
          [U, U, U, { char: '|', attr: frond }, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a hanging vine sprite.
   */
  createVine: function(): SpriteDefinition {
    var vine = makeAttr(GREEN, BG_BLACK);
    var vineLight = makeAttr(LIGHTGREEN, BG_BLACK);
    var flower = makeAttr(LIGHTMAGENTA, BG_BLACK);
    
    return {
      name: 'vine',
      variants: [
        // Scale 0: 1x3
        [
          [{ char: '|', attr: vine }],
          [{ char: ')', attr: vineLight }],
          [{ char: '|', attr: vine }]
        ],
        // Scale 1: 2x4
        [
          [{ char: '(', attr: vine }, { char: ')', attr: vineLight }],
          [{ char: ')', attr: vineLight }, { char: '(', attr: vine }],
          [{ char: '|', attr: vine }, { char: ')', attr: vineLight }],
          [{ char: ')', attr: vineLight }, { char: '|', attr: vine }]
        ],
        // Scale 2: 3x5
        [
          [{ char: '(', attr: vineLight }, { char: '|', attr: vine }, { char: ')', attr: vineLight }],
          [{ char: ')', attr: vine }, { char: '*', attr: flower }, { char: '(', attr: vine }],
          [{ char: '|', attr: vineLight }, { char: ')', attr: vine }, { char: '|', attr: vineLight }],
          [{ char: ')', attr: vine }, { char: '|', attr: vineLight }, { char: '(', attr: vine }],
          [{ char: '|', attr: vineLight }, { char: '(', attr: vine }, { char: '|', attr: vineLight }]
        ],
        // Scale 3: 4x6
        [
          [{ char: '(', attr: vine }, { char: '(', attr: vineLight }, { char: ')', attr: vineLight }, { char: ')', attr: vine }],
          [{ char: ')', attr: vineLight }, { char: '*', attr: flower }, { char: '*', attr: flower }, { char: '(', attr: vineLight }],
          [{ char: '|', attr: vine }, { char: ')', attr: vineLight }, { char: '(', attr: vineLight }, { char: '|', attr: vine }],
          [{ char: ')', attr: vineLight }, { char: '|', attr: vine }, { char: '|', attr: vine }, { char: '(', attr: vineLight }],
          [{ char: '(', attr: vine }, { char: ')', attr: vineLight }, { char: '(', attr: vineLight }, { char: ')', attr: vine }],
          [{ char: '|', attr: vineLight }, { char: '|', attr: vine }, { char: '|', attr: vine }, { char: '|', attr: vineLight }]
        ],
        // Scale 4: 4x6 (same)
        [
          [{ char: '(', attr: vine }, { char: '(', attr: vineLight }, { char: ')', attr: vineLight }, { char: ')', attr: vine }],
          [{ char: ')', attr: vineLight }, { char: '*', attr: flower }, { char: '*', attr: flower }, { char: '(', attr: vineLight }],
          [{ char: '|', attr: vine }, { char: ')', attr: vineLight }, { char: '(', attr: vineLight }, { char: '|', attr: vine }],
          [{ char: ')', attr: vineLight }, { char: '|', attr: vine }, { char: '|', attr: vine }, { char: '(', attr: vineLight }],
          [{ char: '(', attr: vine }, { char: ')', attr: vineLight }, { char: '(', attr: vineLight }, { char: ')', attr: vine }],
          [{ char: '|', attr: vineLight }, { char: '|', attr: vine }, { char: '|', attr: vine }, { char: '|', attr: vineLight }]
        ]
      ]
    };
  },
  
  /**
   * Create a tropical flower sprite.
   */
  createFlower: function(): SpriteDefinition {
    var petal = makeAttr(LIGHTMAGENTA, BG_BLACK);
    var petalDark = makeAttr(MAGENTA, BG_BLACK);
    var center = makeAttr(YELLOW, BG_BLACK);
    var stem = makeAttr(GREEN, BG_BLACK);
    var leaf = makeAttr(LIGHTGREEN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'flower',
      variants: [
        // Scale 0: 1x2
        [
          [{ char: '*', attr: petal }],
          [{ char: '|', attr: stem }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '*', attr: petal }, U],
          [{ char: '*', attr: petalDark }, { char: 'o', attr: center }, { char: '*', attr: petalDark }],
          [U, { char: '|', attr: stem }, U]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, U],
          [{ char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '@', attr: center }, { char: '*', attr: petal }, { char: '*', attr: petalDark }],
          [U, { char: '*', attr: petal }, { char: '|', attr: stem }, { char: '*', attr: petal }, U],
          [U, { char: '/', attr: leaf }, { char: '|', attr: stem }, { char: '\\', attr: leaf }, U]
        ],
        // Scale 3: 5x5
        [
          [U, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, U],
          [{ char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '@', attr: center }, { char: '*', attr: petal }, { char: '*', attr: petalDark }],
          [{ char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }],
          [U, { char: '/', attr: leaf }, { char: '|', attr: stem }, { char: '\\', attr: leaf }, U],
          [U, U, { char: '|', attr: stem }, U, U]
        ],
        // Scale 4: 5x5 (same)
        [
          [U, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, U],
          [{ char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '@', attr: center }, { char: '*', attr: petal }, { char: '*', attr: petalDark }],
          [{ char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }],
          [U, { char: '/', attr: leaf }, { char: '|', attr: stem }, { char: '\\', attr: leaf }, U],
          [U, U, { char: '|', attr: stem }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a parrot sprite - colorful tropical bird.
   */
  createParrot: function(): SpriteDefinition {
    var body = makeAttr(LIGHTRED, BG_BLACK);
    var wing = makeAttr(LIGHTBLUE, BG_BLACK);
    var beak = makeAttr(YELLOW, BG_BLACK);
    var tail = makeAttr(LIGHTGREEN, BG_BLACK);
    var eye = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'parrot',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '(', attr: body }, { char: '>', attr: beak }],
          [{ char: '~', attr: tail }, { char: ')', attr: body }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '(', attr: body }, { char: '<', attr: beak }],
          [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }],
          [U, { char: '~', attr: tail }, { char: '~', attr: tail }]
        ],
        // Scale 2: 4x4
        [
          [U, { char: '(', attr: body }, { char: 'o', attr: eye }, { char: '<', attr: beak }],
          [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }],
          [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }, U],
          [U, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }]
        ],
        // Scale 3: 5x5
        [
          [U, { char: '(', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: 'o', attr: eye }, { char: '<', attr: beak }],
          [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }],
          [{ char: '/', attr: wing }, { char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }, U],
          [U, U, { char: '\\', attr: body }, { char: '/', attr: body }, U],
          [U, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }]
        ],
        // Scale 4: 5x5 (same)
        [
          [U, { char: '(', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: 'o', attr: eye }, { char: '<', attr: beak }],
          [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }],
          [{ char: '/', attr: wing }, { char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }, U],
          [U, U, { char: '\\', attr: body }, { char: '/', attr: body }, U],
          [U, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }]
        ]
      ]
    };
  },
  
  /**
   * Create a banana plant sprite.
   */
  createBanana: function(): SpriteDefinition {
    var leaf = makeAttr(GREEN, BG_BLACK);
    var leafLight = makeAttr(LIGHTGREEN, BG_BLACK);
    var trunk = makeAttr(BROWN, BG_BLACK);
    var banana = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'banana',
      variants: [
        // Scale 0: 2x3
        [
          [{ char: '/', attr: leaf }, { char: '\\', attr: leaf }],
          [U, { char: ')', attr: banana }],
          [U, { char: '|', attr: trunk }]
        ],
        // Scale 1: 3x4
        [
          [{ char: '/', attr: leafLight }, { char: '|', attr: leaf }, { char: '\\', attr: leafLight }],
          [{ char: '/', attr: leaf }, { char: ')', attr: banana }, { char: '\\', attr: leaf }],
          [U, { char: '|', attr: trunk }, U],
          [U, { char: '|', attr: trunk }, U]
        ],
        // Scale 2: 5x5
        [
          [{ char: '/', attr: leaf }, { char: '/', attr: leafLight }, { char: '|', attr: leaf }, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }],
          [{ char: '/', attr: leafLight }, U, { char: '|', attr: trunk }, U, { char: '\\', attr: leafLight }],
          [U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U],
          [U, U, { char: '|', attr: trunk }, U, U],
          [U, U, { char: '|', attr: trunk }, U, U]
        ],
        // Scale 3: 7x6
        [
          [{ char: '/', attr: leaf }, { char: '/', attr: leafLight }, U, { char: '|', attr: leaf }, U, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }],
          [U, { char: '/', attr: leaf }, { char: '/', attr: leafLight }, { char: '|', attr: trunk }, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }, U],
          [U, U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U, U],
          [U, U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U, U],
          [U, U, U, { char: '|', attr: trunk }, U, U, U],
          [U, U, U, { char: '|', attr: trunk }, U, U, U]
        ],
        // Scale 4: 7x6 (same)
        [
          [{ char: '/', attr: leaf }, { char: '/', attr: leafLight }, U, { char: '|', attr: leaf }, U, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }],
          [U, { char: '/', attr: leaf }, { char: '/', attr: leafLight }, { char: '|', attr: trunk }, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }, U],
          [U, U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U, U],
          [U, U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U, U],
          [U, U, U, { char: '|', attr: trunk }, U, U, U],
          [U, U, U, { char: '|', attr: trunk }, U, U, U]
        ]
      ]
    };
  }
};

// Register jungle sprites
registerRoadsideSprite('jungle_tree', JungleSprites.createJungleTree);
registerRoadsideSprite('fern', JungleSprites.createFern);
registerRoadsideSprite('vine', JungleSprites.createVine);
registerRoadsideSprite('flower', JungleSprites.createFlower);
registerRoadsideSprite('parrot', JungleSprites.createParrot);
registerRoadsideSprite('banana', JungleSprites.createBanana);
