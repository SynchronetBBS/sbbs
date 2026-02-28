/**
 * CastleSprites.ts - Medieval fortress roadside objects.
 * Towers, battlements, torches, banners, gargoyles, and portcullises.
 */

var CastleSprites = {
  /**
   * Create a tower sprite - stone castle tower.
   */
  createTower: function(): SpriteDefinition {
    var stone = makeAttr(DARKGRAY, BG_BLACK);
    var stoneDark = makeAttr(BLACK, BG_BLACK);
    var stoneLight = makeAttr(LIGHTGRAY, BG_BLACK);
    var roof = makeAttr(BROWN, BG_BLACK);
    var window = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'tower',
      variants: [
        // Scale 0: 2x4
        [
          [{ char: '/', attr: roof }, { char: '\\', attr: roof }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }]
        ],
        // Scale 1: 3x5
        [
          [U, { char: '^', attr: roof }, U],
          [{ char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }]
        ],
        // Scale 2: 5x6
        [
          [U, U, { char: '^', attr: roof }, U, U],
          [U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U],
          [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }]
        ],
        // Scale 3: 7x8
        [
          [U, U, U, { char: '^', attr: roof }, U, U, U],
          [U, U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U, U],
          [U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U],
          [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 4: 7x8 (same)
        [
          [U, U, U, { char: '^', attr: roof }, U, U, U],
          [U, U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U, U],
          [U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U],
          [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a battlement sprite - castle wall crenellations.
   */
  createBattlement: function(): SpriteDefinition {
    var stone = makeAttr(DARKGRAY, BG_BLACK);
    var stoneDark = makeAttr(BLACK, BG_BLACK);
    var stoneLight = makeAttr(LIGHTGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'battlement',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: 'n', attr: stone }, U, { char: 'n', attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 1: 4x3
        [
          [{ char: 'n', attr: stoneLight }, U, { char: 'n', attr: stoneLight }, U],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 2: 6x4
        [
          [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, U, U, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 3: 8x5
        [
          [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, U, U, U, U, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }]
        ],
        // Scale 4: 8x5 (same)
        [
          [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, U, U, U, U, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }]
        ]
      ]
    };
  },
  
  /**
   * Create a torch sprite - wall-mounted flame.
   */
  createTorch: function(): SpriteDefinition {
    var flame = makeAttr(YELLOW, BG_BLACK);
    var flameOuter = makeAttr(LIGHTRED, BG_BLACK);
    var handle = makeAttr(BROWN, BG_BLACK);
    var bracket = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'torch',
      variants: [
        // Scale 0: 1x3
        [
          [{ char: '*', attr: flame }],
          [{ char: '|', attr: handle }],
          [{ char: '+', attr: bracket }]
        ],
        // Scale 1: 3x4
        [
          [U, { char: '*', attr: flame }, U],
          [{ char: '(', attr: flameOuter }, { char: '*', attr: flame }, { char: ')', attr: flameOuter }],
          [U, { char: '|', attr: handle }, U],
          [{ char: '-', attr: bracket }, { char: '+', attr: bracket }, { char: '-', attr: bracket }]
        ],
        // Scale 2: 3x5
        [
          [U, { char: '^', attr: flameOuter }, U],
          [{ char: '(', attr: flameOuter }, { char: '*', attr: flame }, { char: ')', attr: flameOuter }],
          [U, { char: '|', attr: handle }, U],
          [U, { char: '|', attr: handle }, U],
          [{ char: '-', attr: bracket }, { char: '+', attr: bracket }, { char: '-', attr: bracket }]
        ],
        // Scale 3: 5x6
        [
          [U, U, { char: '^', attr: flameOuter }, U, U],
          [U, { char: '(', attr: flameOuter }, { char: '*', attr: flame }, { char: ')', attr: flameOuter }, U],
          [{ char: '~', attr: flame }, { char: '(', attr: flame }, { char: GLYPH.FULL_BLOCK, attr: flame }, { char: ')', attr: flame }, { char: '~', attr: flame }],
          [U, U, { char: '|', attr: handle }, U, U],
          [U, U, { char: '|', attr: handle }, U, U],
          [U, { char: '-', attr: bracket }, { char: '+', attr: bracket }, { char: '-', attr: bracket }, U]
        ],
        // Scale 4: 5x6 (same)
        [
          [U, U, { char: '^', attr: flameOuter }, U, U],
          [U, { char: '(', attr: flameOuter }, { char: '*', attr: flame }, { char: ')', attr: flameOuter }, U],
          [{ char: '~', attr: flame }, { char: '(', attr: flame }, { char: GLYPH.FULL_BLOCK, attr: flame }, { char: ')', attr: flame }, { char: '~', attr: flame }],
          [U, U, { char: '|', attr: handle }, U, U],
          [U, U, { char: '|', attr: handle }, U, U],
          [U, { char: '-', attr: bracket }, { char: '+', attr: bracket }, { char: '-', attr: bracket }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a banner sprite - hanging flag.
   */
  createBanner: function(): SpriteDefinition {
    var banner = makeAttr(LIGHTRED, BG_BLACK);
    var bannerDark = makeAttr(RED, BG_BLACK);
    var pole = makeAttr(BROWN, BG_BLACK);
    var emblem = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'banner',
      variants: [
        // Scale 0: 2x3
        [
          [{ char: '-', attr: pole }, { char: '-', attr: pole }],
          [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [{ char: '\\', attr: banner }, { char: '/', attr: bannerDark }]
        ],
        // Scale 1: 3x4
        [
          [{ char: '-', attr: pole }, { char: 'o', attr: pole }, { char: '-', attr: pole }],
          [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: '*', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [U, { char: 'V', attr: banner }, U]
        ],
        // Scale 2: 4x5
        [
          [{ char: '-', attr: pole }, { char: '-', attr: pole }, { char: 'o', attr: pole }, { char: '-', attr: pole }],
          [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: '*', attr: emblem }, { char: '*', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: banner }],
          [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [U, { char: '\\', attr: banner }, { char: '/', attr: bannerDark }, U]
        ],
        // Scale 3: 5x6
        [
          [{ char: '-', attr: pole }, { char: '-', attr: pole }, { char: 'O', attr: pole }, { char: '-', attr: pole }, { char: '-', attr: pole }],
          [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }],
          [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: '/', attr: emblem }, { char: '*', attr: emblem }, { char: '\\', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: '\\', attr: emblem }, { char: '*', attr: emblem }, { char: '/', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: banner }],
          [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [U, { char: '\\', attr: banner }, { char: 'V', attr: bannerDark }, { char: '/', attr: banner }, U]
        ],
        // Scale 4: 5x6 (same)
        [
          [{ char: '-', attr: pole }, { char: '-', attr: pole }, { char: 'O', attr: pole }, { char: '-', attr: pole }, { char: '-', attr: pole }],
          [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }],
          [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: '/', attr: emblem }, { char: '*', attr: emblem }, { char: '\\', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: '\\', attr: emblem }, { char: '*', attr: emblem }, { char: '/', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: banner }],
          [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
          [U, { char: '\\', attr: banner }, { char: 'V', attr: bannerDark }, { char: '/', attr: banner }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a gargoyle sprite - stone guardian.
   */
  createGargoyle: function(): SpriteDefinition {
    var stone = makeAttr(DARKGRAY, BG_BLACK);
    var stoneDark = makeAttr(BLACK, BG_BLACK);
    var eye = makeAttr(RED, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'gargoyle',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '(', attr: stone }, { char: ')', attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: '\\', attr: stoneDark }]
        ],
        // Scale 1: 3x3
        [
          [{ char: '/', attr: stone }, { char: '^', attr: stone }, { char: '\\', attr: stone }],
          [{ char: '(', attr: stoneDark }, { char: 'v', attr: stone }, { char: ')', attr: stoneDark }],
          [{ char: '/', attr: stone }, U, { char: '\\', attr: stone }]
        ],
        // Scale 2: 5x4
        [
          [{ char: '/', attr: stone }, { char: '^', attr: stone }, U, { char: '^', attr: stone }, { char: '\\', attr: stone }],
          [{ char: '(', attr: stoneDark }, { char: '.', attr: eye }, { char: 'v', attr: stone }, { char: '.', attr: eye }, { char: ')', attr: stoneDark }],
          [U, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '\\', attr: stone }, U],
          [{ char: '/', attr: stoneDark }, U, { char: '|', attr: stone }, U, { char: '\\', attr: stoneDark }]
        ],
        // Scale 3: 7x5
        [
          [U, { char: '/', attr: stone }, { char: '^', attr: stone }, U, { char: '^', attr: stone }, { char: '\\', attr: stone }, U],
          [{ char: '/', attr: stone }, { char: '(', attr: stoneDark }, { char: '*', attr: eye }, { char: 'w', attr: stone }, { char: '*', attr: eye }, { char: ')', attr: stoneDark }, { char: '\\', attr: stone }],
          [U, { char: '/', attr: stoneDark }, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '\\', attr: stone }, { char: '\\', attr: stoneDark }, U],
          [{ char: '/', attr: stone }, U, { char: '(', attr: stoneDark }, { char: '|', attr: stone }, { char: ')', attr: stoneDark }, U, { char: '\\', attr: stone }],
          [{ char: '/', attr: stoneDark }, U, U, { char: '|', attr: stoneDark }, U, U, { char: '\\', attr: stoneDark }]
        ],
        // Scale 4: 7x5 (same)
        [
          [U, { char: '/', attr: stone }, { char: '^', attr: stone }, U, { char: '^', attr: stone }, { char: '\\', attr: stone }, U],
          [{ char: '/', attr: stone }, { char: '(', attr: stoneDark }, { char: '*', attr: eye }, { char: 'w', attr: stone }, { char: '*', attr: eye }, { char: ')', attr: stoneDark }, { char: '\\', attr: stone }],
          [U, { char: '/', attr: stoneDark }, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '\\', attr: stone }, { char: '\\', attr: stoneDark }, U],
          [{ char: '/', attr: stone }, U, { char: '(', attr: stoneDark }, { char: '|', attr: stone }, { char: ')', attr: stoneDark }, U, { char: '\\', attr: stone }],
          [{ char: '/', attr: stoneDark }, U, U, { char: '|', attr: stoneDark }, U, U, { char: '\\', attr: stoneDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a portcullis sprite - castle gate.
   */
  createPortcullis: function(): SpriteDefinition {
    var iron = makeAttr(DARKGRAY, BG_BLACK);
    var ironLight = makeAttr(LIGHTGRAY, BG_BLACK);
    var stone = makeAttr(BROWN, BG_BLACK);
    
    return {
      name: 'portcullis',
      variants: [
        // Scale 0: 3x3
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: '^', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '#', attr: iron }, { char: '#', attr: iron }, { char: '#', attr: iron }],
          [{ char: '#', attr: ironLight }, { char: '#', attr: ironLight }, { char: '#', attr: ironLight }]
        ],
        // Scale 1: 4x4
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: '/', attr: stone }, { char: '\\', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
          [{ char: '|', attr: ironLight }, { char: '#', attr: ironLight }, { char: '#', attr: ironLight }, { char: '|', attr: ironLight }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }]
        ],
        // Scale 2: 6x5
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '/', attr: stone }, { char: '\\', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
          [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
          [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }]
        ],
        // Scale 3: 7x6
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '/', attr: stone }, { char: '^', attr: stone }, { char: '\\', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
          [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
          [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }]
        ],
        // Scale 4: 7x6 (same)
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '/', attr: stone }, { char: '^', attr: stone }, { char: '\\', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
          [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
          [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
          [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }]
        ]
      ]
    };
  }
};

// Register castle sprites
registerRoadsideSprite('tower', CastleSprites.createTower);
registerRoadsideSprite('battlement', CastleSprites.createBattlement);
registerRoadsideSprite('torch', CastleSprites.createTorch);
registerRoadsideSprite('banner', CastleSprites.createBanner);
registerRoadsideSprite('gargoyle', CastleSprites.createGargoyle);
registerRoadsideSprite('portcullis', CastleSprites.createPortcullis);
