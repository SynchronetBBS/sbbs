/**
 * StadiumSprites.ts - Motorsport stadium roadside objects.
 * Grandstands, tire stacks, hay bales, flag marshals, pit crew, banners.
 */

var StadiumSprites = {
  /**
   * Create a grandstand section filled with fans.
   */
  createGrandstand: function(): SpriteDefinition {
    var structure = makeAttr(DARKGRAY, BG_BLACK);
    var fans1 = makeAttr(LIGHTRED, BG_BLACK);
    var fans2 = makeAttr(YELLOW, BG_BLACK);
    var fans3 = makeAttr(LIGHTCYAN, BG_BLACK);
    var rail = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'grandstand',
      variants: [
        // Scale 0: 4x3 - distant stands
        [
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.MEDIUM_SHADE, attr: fans1 }, { char: GLYPH.MEDIUM_SHADE, attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.DARK_SHADE, attr: fans3 }, { char: GLYPH.DARK_SHADE, attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: '-', attr: rail }, { char: '-', attr: rail }, { char: '-', attr: rail }, { char: '-', attr: rail }]
        ],
        // Scale 1: 5x4
        [
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }]
        ],
        // Scale 2: 7x5
        [
          [{ char: '/', attr: structure }, { char: GLYPH.MEDIUM_SHADE, attr: fans1 }, { char: GLYPH.MEDIUM_SHADE, attr: fans2 }, { char: GLYPH.MEDIUM_SHADE, attr: fans3 }, { char: GLYPH.MEDIUM_SHADE, attr: fans1 }, { char: GLYPH.MEDIUM_SHADE, attr: fans2 }, { char: '\\', attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: '[', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: ']', attr: rail }]
        ],
        // Scale 3: 9x6
        [
          [U, { char: '/', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '\\', attr: structure }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: '[', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: ']', attr: rail }]
        ],
        // Scale 4: 9x6 (same)
        [
          [U, { char: '/', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '\\', attr: structure }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }],
          [{ char: '[', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: ']', attr: rail }]
        ]
      ]
    };
  },
  
  /**
   * Create a stack of tires - classic track barrier.
   */
  createTireStack: function(): SpriteDefinition {
    var tire = makeAttr(DARKGRAY, BG_BLACK);
    var tireInner = makeAttr(BLACK, BG_BLACK);
    var paint = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'tire_stack',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: 'O', attr: tire }, { char: 'O', attr: paint }],
          [{ char: 'O', attr: paint }, { char: 'O', attr: tire }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: 'O', attr: paint }, U],
          [{ char: 'O', attr: tire }, { char: 'O', attr: paint }, { char: 'O', attr: tire }],
          [{ char: 'O', attr: paint }, { char: 'O', attr: tire }, { char: 'O', attr: paint }]
        ],
        // Scale 2: 4x4
        [
          [U, { char: 'O', attr: paint }, { char: 'O', attr: tire }, U],
          [{ char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: '0', attr: tireInner }, { char: ')', attr: tire }],
          [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: '0', attr: tireInner }, { char: ')', attr: paint }],
          [{ char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: '0', attr: tireInner }, { char: ')', attr: tire }]
        ],
        // Scale 3: 5x5
        [
          [U, U, { char: 'O', attr: paint }, U, U],
          [U, { char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: tire }, U],
          [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: paint }],
          [{ char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: paint }, { char: '0', attr: tireInner }, { char: ')', attr: tire }],
          [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: paint }]
        ],
        // Scale 4: 5x5 (same)
        [
          [U, U, { char: 'O', attr: paint }, U, U],
          [U, { char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: tire }, U],
          [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: paint }],
          [{ char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: paint }, { char: '0', attr: tireInner }, { char: ')', attr: tire }],
          [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: paint }]
        ]
      ]
    };
  },
  
  /**
   * Create a hay bale - soft barrier for dirt tracks.
   */
  createHayBale: function(): SpriteDefinition {
    var hay = makeAttr(YELLOW, BG_BLACK);
    var hayDark = makeAttr(BROWN, BG_BLACK);
    var twine = makeAttr(BROWN, BG_BLACK);
    
    return {
      name: 'hay_bale',
      variants: [
        // Scale 0: 2x1
        [
          [{ char: '[', attr: hay }, { char: ']', attr: hay }]
        ],
        // Scale 1: 3x2
        [
          [{ char: '/', attr: hay }, { char: '~', attr: hayDark }, { char: '\\', attr: hay }],
          [{ char: '[', attr: hay }, { char: '=', attr: twine }, { char: ']', attr: hay }]
        ],
        // Scale 2: 4x2
        [
          [{ char: '/', attr: hay }, { char: GLYPH.MEDIUM_SHADE, attr: hayDark }, { char: GLYPH.MEDIUM_SHADE, attr: hay }, { char: '\\', attr: hay }],
          [{ char: '[', attr: hay }, { char: '=', attr: twine }, { char: '=', attr: twine }, { char: ']', attr: hay }]
        ],
        // Scale 3: 5x3
        [
          [{ char: '/', attr: hay }, { char: '~', attr: hayDark }, { char: '~', attr: hay }, { char: '~', attr: hayDark }, { char: '\\', attr: hay }],
          [{ char: '[', attr: hay }, { char: GLYPH.MEDIUM_SHADE, attr: hayDark }, { char: '|', attr: twine }, { char: GLYPH.MEDIUM_SHADE, attr: hay }, { char: ']', attr: hay }],
          [{ char: '\\', attr: hay }, { char: '_', attr: hayDark }, { char: '=', attr: twine }, { char: '_', attr: hay }, { char: '/', attr: hay }]
        ],
        // Scale 4: 5x3 (same)
        [
          [{ char: '/', attr: hay }, { char: '~', attr: hayDark }, { char: '~', attr: hay }, { char: '~', attr: hayDark }, { char: '\\', attr: hay }],
          [{ char: '[', attr: hay }, { char: GLYPH.MEDIUM_SHADE, attr: hayDark }, { char: '|', attr: twine }, { char: GLYPH.MEDIUM_SHADE, attr: hay }, { char: ']', attr: hay }],
          [{ char: '\\', attr: hay }, { char: '_', attr: hayDark }, { char: '=', attr: twine }, { char: '_', attr: hay }, { char: '/', attr: hay }]
        ]
      ]
    };
  },
  
  /**
   * Create a flag marshal with waving flag.
   */
  createFlagMarshal: function(): SpriteDefinition {
    var uniform = makeAttr(WHITE, BG_BLACK);
    var pants = makeAttr(DARKGRAY, BG_BLACK);
    var flag = makeAttr(YELLOW, BG_BLACK);
    var flagAlt = makeAttr(LIGHTGREEN, BG_BLACK);  // Green flag
    var U: SpriteCell | null = null;
    
    return {
      name: 'flag_marshal',
      variants: [
        // Scale 0: 1x3
        [
          [{ char: '\\', attr: flag }],
          [{ char: 'o', attr: uniform }],
          [{ char: '|', attr: pants }]
        ],
        // Scale 1: 2x4
        [
          [{ char: '~', attr: flag }, { char: '>', attr: flag }],
          [U, { char: 'O', attr: uniform }],
          [{ char: '/', attr: uniform }, { char: '\\', attr: uniform }],
          [{ char: '/', attr: pants }, { char: '\\', attr: pants }]
        ],
        // Scale 2: 3x5
        [
          [{ char: '~', attr: flagAlt }, { char: '~', attr: flagAlt }, { char: '>', attr: flagAlt }],
          [U, { char: '|', attr: uniform }, U],
          [U, { char: 'O', attr: uniform }, U],
          [{ char: '/', attr: uniform }, { char: '|', attr: uniform }, { char: '\\', attr: uniform }],
          [{ char: '/', attr: pants }, U, { char: '\\', attr: pants }]
        ],
        // Scale 3: 4x6
        [
          [U, { char: '~', attr: flag }, { char: '~', attr: flag }, { char: '>', attr: flag }],
          [U, U, { char: '|', attr: uniform }, U],
          [U, { char: '(', attr: uniform }, { char: ')', attr: uniform }, U],
          [U, { char: '/', attr: uniform }, { char: '\\', attr: uniform }, U],
          [U, { char: '|', attr: pants }, { char: '|', attr: pants }, U],
          [{ char: '/', attr: pants }, U, U, { char: '\\', attr: pants }]
        ],
        // Scale 4: 4x6 (same)
        [
          [U, { char: '~', attr: flag }, { char: '~', attr: flag }, { char: '>', attr: flag }],
          [U, U, { char: '|', attr: uniform }, U],
          [U, { char: '(', attr: uniform }, { char: ')', attr: uniform }, U],
          [U, { char: '/', attr: uniform }, { char: '\\', attr: uniform }, U],
          [U, { char: '|', attr: pants }, { char: '|', attr: pants }, U],
          [{ char: '/', attr: pants }, U, U, { char: '\\', attr: pants }]
        ]
      ]
    };
  },
  
  /**
   * Create a pit crew member with equipment.
   */
  createPitCrew: function(): SpriteDefinition {
    var helmet = makeAttr(LIGHTRED, BG_BLACK);
    var suit = makeAttr(RED, BG_BLACK);
    var tool = makeAttr(LIGHTGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'pit_crew',
      variants: [
        // Scale 0: 1x3
        [
          [{ char: 'o', attr: helmet }],
          [{ char: '#', attr: suit }],
          [{ char: 'A', attr: suit }]
        ],
        // Scale 1: 2x4
        [
          [{ char: '(', attr: helmet }, { char: ')', attr: helmet }],
          [{ char: '[', attr: suit }, { char: ']', attr: suit }],
          [{ char: '/', attr: suit }, { char: '\\', attr: suit }],
          [{ char: '|', attr: suit }, { char: '|', attr: suit }]
        ],
        // Scale 2: 3x5
        [
          [U, { char: '@', attr: helmet }, U],
          [{ char: '/', attr: tool }, { char: '#', attr: suit }, { char: '\\', attr: suit }],
          [{ char: '|', attr: tool }, { char: '#', attr: suit }, { char: '|', attr: suit }],
          [U, { char: '/', attr: suit }, { char: '\\', attr: suit }],
          [U, { char: '|', attr: suit }, { char: '|', attr: suit }]
        ],
        // Scale 3: 4x6
        [
          [U, { char: '(', attr: helmet }, { char: ')', attr: helmet }, U],
          [{ char: '/', attr: tool }, { char: '[', attr: suit }, { char: ']', attr: suit }, { char: '\\', attr: suit }],
          [{ char: '|', attr: tool }, { char: '[', attr: suit }, { char: ']', attr: suit }, U],
          [U, { char: '[', attr: suit }, { char: ']', attr: suit }, U],
          [U, { char: '/', attr: suit }, { char: '\\', attr: suit }, U],
          [U, { char: '|', attr: suit }, { char: '|', attr: suit }, U]
        ],
        // Scale 4: 4x6 (same)
        [
          [U, { char: '(', attr: helmet }, { char: ')', attr: helmet }, U],
          [{ char: '/', attr: tool }, { char: '[', attr: suit }, { char: ']', attr: suit }, { char: '\\', attr: suit }],
          [{ char: '|', attr: tool }, { char: '[', attr: suit }, { char: ']', attr: suit }, U],
          [U, { char: '[', attr: suit }, { char: ']', attr: suit }, U],
          [U, { char: '/', attr: suit }, { char: '\\', attr: suit }, U],
          [U, { char: '|', attr: suit }, { char: '|', attr: suit }, U]
        ]
      ]
    };
  },
  
  /**
   * Create an advertising banner/sign.
   */
  createBanner: function(): SpriteDefinition {
    var frame = makeAttr(DARKGRAY, BG_BLACK);
    var banner1 = makeAttr(LIGHTRED, BG_RED);
    var banner2 = makeAttr(YELLOW, BG_BROWN);
    var banner3 = makeAttr(LIGHTCYAN, BG_CYAN);
    var pole = makeAttr(LIGHTGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'banner',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: '[', attr: frame }, { char: '#', attr: banner1 }, { char: ']', attr: frame }],
          [U, { char: '|', attr: pole }, U]
        ],
        // Scale 1: 4x3
        [
          [{ char: '[', attr: frame }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: ']', attr: frame }],
          [U, { char: '|', attr: pole }, { char: '|', attr: pole }, U],
          [U, { char: '|', attr: pole }, { char: '|', attr: pole }, U]
        ],
        // Scale 2: 5x4
        [
          [{ char: '/', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '\\', attr: frame }],
          [{ char: '|', attr: frame }, { char: GLYPH.FULL_BLOCK, attr: banner1 }, { char: GLYPH.FULL_BLOCK, attr: banner1 }, { char: GLYPH.FULL_BLOCK, attr: banner1 }, { char: '|', attr: frame }],
          [U, U, { char: '|', attr: pole }, U, U],
          [U, U, { char: '|', attr: pole }, U, U]
        ],
        // Scale 3: 6x5
        [
          [{ char: '/', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '\\', attr: frame }],
          [{ char: '|', attr: frame }, { char: GLYPH.FULL_BLOCK, attr: banner3 }, { char: GLYPH.FULL_BLOCK, attr: banner3 }, { char: GLYPH.FULL_BLOCK, attr: banner3 }, { char: GLYPH.FULL_BLOCK, attr: banner3 }, { char: '|', attr: frame }],
          [{ char: '\\', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '/', attr: frame }],
          [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U],
          [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U]
        ],
        // Scale 4: 6x5 (same)
        [
          [{ char: '/', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '\\', attr: frame }],
          [{ char: '|', attr: frame }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: '|', attr: frame }],
          [{ char: '\\', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '/', attr: frame }],
          [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U],
          [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U]
        ]
      ]
    };
  }
};

// Register stadium sprites
registerRoadsideSprite('grandstand', StadiumSprites.createGrandstand);
registerRoadsideSprite('tire_stack', StadiumSprites.createTireStack);
registerRoadsideSprite('hay_bale', StadiumSprites.createHayBale);
registerRoadsideSprite('flag_marshal', StadiumSprites.createFlagMarshal);
registerRoadsideSprite('pit_crew', StadiumSprites.createPitCrew);
registerRoadsideSprite('banner', StadiumSprites.createBanner);
