/**
 * RuinsSprites.ts - Ancient Egyptian roadside objects.
 * Columns, statues, obelisks, sphinx, hieroglyphs, and scarabs.
 */

var RuinsSprites = {
  /**
   * Create a column sprite - broken ancient pillar.
   */
  createColumn: function(): SpriteDefinition {
    var stone = makeAttr(YELLOW, BG_BLACK);
    var stoneDark = makeAttr(BROWN, BG_BLACK);
    var stoneLight = makeAttr(WHITE, BG_BLACK);
    
    return {
      name: 'column',
      variants: [
        // Scale 0: 2x4
        [
          [{ char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }],
          [{ char: '|', attr: stone }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: '|', attr: stoneDark }],
          [{ char: '=', attr: stone }, { char: '=', attr: stone }]
        ],
        // Scale 1: 3x5
        [
          [{ char: '/', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '\\', attr: stoneLight }],
          [{ char: '|', attr: stone }, { char: '|', attr: stone }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }],
          [{ char: '\\', attr: stoneDark }, { char: '=', attr: stone }, { char: '/', attr: stoneDark }]
        ],
        // Scale 2: 4x6
        [
          [{ char: '/', attr: stoneLight }, { char: '-', attr: stoneLight }, { char: '-', attr: stoneLight }, { char: '\\', attr: stoneLight }],
          [{ char: '|', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stoneDark }],
          [{ char: '\\', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stone }, { char: '/', attr: stoneDark }]
        ],
        // Scale 3: 5x7
        [
          [{ char: '/', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '\\', attr: stoneLight }],
          [{ char: '|', attr: stone }, { char: '(', attr: stone }, { char: ')', attr: stone }, { char: '(', attr: stone }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stone }, { char: ')', attr: stoneDark }, { char: '(', attr: stoneDark }, { char: ')', attr: stoneDark }, { char: '|', attr: stone }],
          [{ char: '\\', attr: stoneDark }, { char: '=', attr: stone }, { char: '=', attr: stone }, { char: '=', attr: stone }, { char: '/', attr: stoneDark }]
        ],
        // Scale 4: 5x7 (same)
        [
          [{ char: '/', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '\\', attr: stoneLight }],
          [{ char: '|', attr: stone }, { char: '(', attr: stone }, { char: ')', attr: stone }, { char: '(', attr: stone }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stone }, { char: ')', attr: stoneDark }, { char: '(', attr: stoneDark }, { char: ')', attr: stoneDark }, { char: '|', attr: stone }],
          [{ char: '\\', attr: stoneDark }, { char: '=', attr: stone }, { char: '=', attr: stone }, { char: '=', attr: stone }, { char: '/', attr: stoneDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a statue sprite - ancient pharaoh statue.
   */
  createStatue: function(): SpriteDefinition {
    var stone = makeAttr(YELLOW, BG_BLACK);
    var stoneDark = makeAttr(BROWN, BG_BLACK);
    var gold = makeAttr(YELLOW, BG_BROWN);
    var face = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'statue',
      variants: [
        // Scale 0: 2x3
        [
          [{ char: '/', attr: gold }, { char: '\\', attr: gold }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: '\\', attr: stoneDark }]
        ],
        // Scale 1: 3x4
        [
          [U, { char: '^', attr: gold }, U],
          [{ char: '/', attr: gold }, { char: 'O', attr: face }, { char: '\\', attr: gold }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stoneDark }]
        ],
        // Scale 2: 5x5
        [
          [U, { char: '/', attr: gold }, { char: '^', attr: gold }, { char: '\\', attr: gold }, U],
          [{ char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: 'O', attr: face }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }],
          [{ char: '|', attr: stone }, { char: '.', attr: face }, { char: 'v', attr: face }, { char: '.', attr: face }, { char: '|', attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stoneDark }]
        ],
        // Scale 3: 7x6
        [
          [U, U, { char: '/', attr: gold }, { char: '^', attr: gold }, { char: '\\', attr: gold }, U, U],
          [U, { char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }, U],
          [{ char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '.', attr: face }, { char: 'v', attr: face }, { char: '.', attr: face }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }],
          [{ char: '|', attr: stone }, { char: '|', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stone }, { char: '|', attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
        ],
        // Scale 4: 7x6 (same)
        [
          [U, U, { char: '/', attr: gold }, { char: '^', attr: gold }, { char: '\\', attr: gold }, U, U],
          [U, { char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }, U],
          [{ char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '.', attr: face }, { char: 'v', attr: face }, { char: '.', attr: face }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }],
          [{ char: '|', attr: stone }, { char: '|', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stone }, { char: '|', attr: stone }],
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
        ]
      ]
    };
  },
  
  /**
   * Create an obelisk sprite - tall ancient monument.
   */
  createObelisk: function(): SpriteDefinition {
    var stone = makeAttr(YELLOW, BG_BLACK);
    var stoneDark = makeAttr(BROWN, BG_BLACK);
    var top = makeAttr(WHITE, BG_BLACK);
    var glyph = makeAttr(RED, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'obelisk',
      variants: [
        // Scale 0: 1x4
        [
          [{ char: '^', attr: top }],
          [{ char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }],
          [{ char: '|', attr: stone }]
        ],
        // Scale 1: 2x5
        [
          [{ char: '/', attr: top }, { char: '\\', attr: top }],
          [{ char: '|', attr: stone }, { char: '|', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stone }, { char: '|', attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: '\\', attr: stoneDark }]
        ],
        // Scale 2: 3x6
        [
          [U, { char: '^', attr: top }, U],
          [{ char: '/', attr: stone }, { char: '\\', attr: stone }, U],
          [{ char: '|', attr: stone }, { char: '*', attr: glyph }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stone }],
          [{ char: '|', attr: stone }, { char: '*', attr: glyph }, { char: '|', attr: stoneDark }],
          [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
        ],
        // Scale 3: 4x7
        [
          [U, { char: '/', attr: top }, { char: '\\', attr: top }, U],
          [{ char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stone }],
          [{ char: '|', attr: stone }, { char: '@', attr: glyph }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '*', attr: glyph }, { char: '|', attr: stone }],
          [{ char: '|', attr: stone }, { char: '*', attr: glyph }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '@', attr: glyph }, { char: '|', attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
        ],
        // Scale 4: 4x7 (same)
        [
          [U, { char: '/', attr: top }, { char: '\\', attr: top }, U],
          [{ char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stone }],
          [{ char: '|', attr: stone }, { char: '@', attr: glyph }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '*', attr: glyph }, { char: '|', attr: stone }],
          [{ char: '|', attr: stone }, { char: '*', attr: glyph }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stoneDark }],
          [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '@', attr: glyph }, { char: '|', attr: stone }],
          [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a sphinx sprite - mythical guardian.
   */
  createSphinx: function(): SpriteDefinition {
    var stone = makeAttr(YELLOW, BG_BLACK);
    var stoneDark = makeAttr(BROWN, BG_BLACK);
    var face = makeAttr(BROWN, BG_BLACK);
    var eye = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'sphinx',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: '/', attr: stone }, { char: 'O', attr: face }, { char: '\\', attr: stone }],
          [{ char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }]
        ],
        // Scale 1: 4x3
        [
          [U, { char: '/', attr: stone }, { char: '\\', attr: stone }, U],
          [{ char: '/', attr: stone }, { char: 'O', attr: face }, { char: 'v', attr: face }, { char: '\\', attr: stone }],
          [{ char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }]
        ],
        // Scale 2: 6x4
        [
          [U, { char: '/', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '\\', attr: stone }, U],
          [{ char: '/', attr: stone }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: face }, { char: GLYPH.FULL_BLOCK, attr: face }, { char: '.', attr: eye }, { char: '\\', attr: stone }],
          [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '>', attr: face }, { char: '<', attr: face }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stoneDark }],
          [{ char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }]
        ],
        // Scale 3: 9x5
        [
          [U, U, { char: '/', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '\\', attr: stone }, U, U],
          [U, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: face }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stone }, U],
          [{ char: '/', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '>', attr: face }, { char: 'w', attr: face }, { char: '<', attr: face }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stoneDark }],
          [{ char: '-', attr: stone }, { char: '-', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '-', attr: stone }, { char: '-', attr: stone }],
          [{ char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }]
        ],
        // Scale 4: 9x5 (same)
        [
          [U, U, { char: '/', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '\\', attr: stone }, U, U],
          [U, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: face }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stone }, U],
          [{ char: '/', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '>', attr: face }, { char: 'w', attr: face }, { char: '<', attr: face }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stoneDark }],
          [{ char: '-', attr: stone }, { char: '-', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '-', attr: stone }, { char: '-', attr: stone }],
          [{ char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a hieroglyph wall sprite - ancient writings.
   */
  createHieroglyph: function(): SpriteDefinition {
    var stone = makeAttr(YELLOW, BG_BLACK);
    var stoneDark = makeAttr(BROWN, BG_BLACK);
    var glyph1 = makeAttr(RED, BG_BLACK);
    var glyph2 = makeAttr(BLUE, BG_BLACK);
    var glyph3 = makeAttr(GREEN, BG_BLACK);
    
    return {
      name: 'hieroglyph',
      variants: [
        // Scale 0: 2x3
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '*', attr: glyph1 }, { char: '+', attr: glyph2 }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 1: 3x4
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '@', attr: glyph1 }, { char: '+', attr: glyph2 }, { char: '~', attr: glyph3 }],
          [{ char: '|', attr: glyph3 }, { char: '*', attr: glyph1 }, { char: 'o', attr: glyph2 }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 2: 4x5
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '@', attr: glyph1 }, { char: '~', attr: glyph2 }, { char: '+', attr: glyph3 }, { char: '|', attr: glyph1 }],
          [{ char: '|', attr: glyph2 }, { char: 'o', attr: glyph3 }, { char: '*', attr: glyph1 }, { char: '~', attr: glyph2 }],
          [{ char: '^', attr: glyph3 }, { char: '*', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: 'v', attr: glyph3 }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 3: 5x6
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '/', attr: glyph1 }, { char: 'O', attr: glyph2 }, { char: '\\', attr: glyph1 }, { char: '~', attr: glyph3 }, { char: '|', attr: glyph2 }],
          [{ char: '|', attr: glyph2 }, { char: '*', attr: glyph3 }, { char: '+', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: '~', attr: glyph1 }],
          [{ char: '^', attr: glyph3 }, { char: '~', attr: glyph1 }, { char: 'o', attr: glyph2 }, { char: '*', attr: glyph3 }, { char: 'v', attr: glyph1 }],
          [{ char: '|', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: '|', attr: glyph3 }, { char: '+', attr: glyph1 }, { char: '|', attr: glyph2 }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ],
        // Scale 4: 5x6 (same)
        [
          [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
          [{ char: '/', attr: glyph1 }, { char: 'O', attr: glyph2 }, { char: '\\', attr: glyph1 }, { char: '~', attr: glyph3 }, { char: '|', attr: glyph2 }],
          [{ char: '|', attr: glyph2 }, { char: '*', attr: glyph3 }, { char: '+', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: '~', attr: glyph1 }],
          [{ char: '^', attr: glyph3 }, { char: '~', attr: glyph1 }, { char: 'o', attr: glyph2 }, { char: '*', attr: glyph3 }, { char: 'v', attr: glyph1 }],
          [{ char: '|', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: '|', attr: glyph3 }, { char: '+', attr: glyph1 }, { char: '|', attr: glyph2 }],
          [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a scarab sprite - sacred beetle.
   */
  createScarab: function(): SpriteDefinition {
    var shell = makeAttr(LIGHTBLUE, BG_BLACK);
    var shellDark = makeAttr(BLUE, BG_BLACK);
    var gold = makeAttr(YELLOW, BG_BLACK);
    var leg = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'scarab',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '(', attr: shell }, { char: ')', attr: shell }],
          [{ char: '<', attr: leg }, { char: '>', attr: leg }]
        ],
        // Scale 1: 3x3
        [
          [{ char: '/', attr: shell }, { char: '*', attr: gold }, { char: '\\', attr: shell }],
          [{ char: '(', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: ')', attr: shellDark }],
          [{ char: '<', attr: leg }, { char: '|', attr: leg }, { char: '>', attr: leg }]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '/', attr: shell }, { char: '*', attr: gold }, { char: '\\', attr: shell }, U],
          [{ char: '/', attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shell }],
          [{ char: '(', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: ')', attr: shellDark }],
          [{ char: '<', attr: leg }, { char: '/', attr: leg }, { char: '|', attr: leg }, { char: '\\', attr: leg }, { char: '>', attr: leg }]
        ],
        // Scale 3: 7x5
        [
          [U, U, { char: '/', attr: shell }, { char: '@', attr: gold }, { char: '\\', attr: shell }, U, U],
          [U, { char: '/', attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shell }, U],
          [{ char: '/', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shellDark }],
          [{ char: '(', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: ')', attr: shellDark }],
          [{ char: '<', attr: leg }, { char: '<', attr: leg }, { char: '/', attr: leg }, { char: '|', attr: leg }, { char: '\\', attr: leg }, { char: '>', attr: leg }, { char: '>', attr: leg }]
        ],
        // Scale 4: 7x5 (same)
        [
          [U, U, { char: '/', attr: shell }, { char: '@', attr: gold }, { char: '\\', attr: shell }, U, U],
          [U, { char: '/', attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shell }, U],
          [{ char: '/', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shellDark }],
          [{ char: '(', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: ')', attr: shellDark }],
          [{ char: '<', attr: leg }, { char: '<', attr: leg }, { char: '/', attr: leg }, { char: '|', attr: leg }, { char: '\\', attr: leg }, { char: '>', attr: leg }, { char: '>', attr: leg }]
        ]
      ]
    };
  }
};

// Register ruins sprites
registerRoadsideSprite('column', RuinsSprites.createColumn);
registerRoadsideSprite('statue', RuinsSprites.createStatue);
registerRoadsideSprite('obelisk', RuinsSprites.createObelisk);
registerRoadsideSprite('sphinx', RuinsSprites.createSphinx);
registerRoadsideSprite('hieroglyph', RuinsSprites.createHieroglyph);
registerRoadsideSprite('scarab', RuinsSprites.createScarab);
