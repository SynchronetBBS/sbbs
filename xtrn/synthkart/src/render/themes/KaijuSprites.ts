/**
 * KaijuSprites - Scenery sprites for the Kaiju Rampage theme.
 * A city under monster attack: rubble, fires, wrecked vehicles, and destruction.
 */

var KaijuSprites = {
  /**
   * Create a pile of building rubble.
   */
  createRubble: function(): SpriteDefinition {
    var concrete = makeAttr(DARKGRAY, BG_BLACK);
    var rebar = makeAttr(BROWN, BG_BLACK);
    var dust = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'rubble',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: '/', attr: rebar }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }],
          [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
        ],
        // Scale 1: 5x3
        [
          [U, { char: '.', attr: dust }, { char: '/', attr: rebar }, { char: '.', attr: dust }, U],
          [{ char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '/', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }],
          [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
        ],
        // Scale 2: 7x4
        [
          [U, U, { char: '.', attr: dust }, { char: '_', attr: rebar }, { char: '.', attr: dust }, U, U],
          [U, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: '/', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: rebar }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, U],
          [{ char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }],
          [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
        ],
        // Scale 3: 9x5
        [
          [U, U, U, { char: '.', attr: dust }, { char: '/', attr: rebar }, { char: '.', attr: dust }, U, U, U],
          [U, U, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: '/', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: rebar }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, U, U],
          [U, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, U],
          [{ char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '[', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: ']', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }],
          [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
        ]
      ]
    };
  },

  /**
   * Create flames/fire.
   */
  createFire: function(): SpriteDefinition {
    var flame1 = makeAttr(YELLOW, BG_BLACK);
    var flame2 = makeAttr(LIGHTRED, BG_BLACK);
    var flame3 = makeAttr(RED, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'fire',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '^', attr: flame1 }, { char: '^', attr: flame2 }],
          [{ char: GLYPH.MEDIUM_SHADE, attr: flame3 }, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '^', attr: flame1 }, U],
          [{ char: '(', attr: flame2 }, { char: '*', attr: flame1 }, { char: ')', attr: flame2 }],
          [{ char: GLYPH.MEDIUM_SHADE, attr: flame3 }, { char: GLYPH.FULL_BLOCK, attr: flame3 }, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '^', attr: flame1 }, { char: '^', attr: flame1 }, { char: '^', attr: flame1 }, U],
          [{ char: '(', attr: flame2 }, { char: '*', attr: flame1 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: '*', attr: flame1 }, { char: ')', attr: flame2 }],
          [{ char: GLYPH.MEDIUM_SHADE, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.MEDIUM_SHADE, attr: flame2 }],
          [U, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }, { char: GLYPH.FULL_BLOCK, attr: flame3 }, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }, U]
        ],
        // Scale 3: 6x5
        [
          [U, U, { char: '^', attr: flame1 }, { char: '^', attr: flame1 }, U, U],
          [U, { char: '(', attr: flame1 }, { char: '*', attr: flame1 }, { char: '*', attr: flame1 }, { char: ')', attr: flame1 }, U],
          [{ char: '(', attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: ')', attr: flame2 }],
          [{ char: GLYPH.MEDIUM_SHADE, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.MEDIUM_SHADE, attr: flame2 }],
          [U, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }, { char: GLYPH.FULL_BLOCK, attr: flame3 }, { char: GLYPH.FULL_BLOCK, attr: flame3 }, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }, U]
        ]
      ]
    };
  },

  /**
   * Create a wrecked/crushed car.
   */
  createWreckedCar: function(): SpriteDefinition {
    var body = makeAttr(DARKGRAY, BG_BLACK);
    var rust = makeAttr(BROWN, BG_BLACK);
    var glass = makeAttr(LIGHTCYAN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'wrecked_car',
      variants: [
        // Scale 0: 4x2
        [
          [{ char: '_', attr: body }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: '_', attr: body }],
          [{ char: 'o', attr: rust }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: 'o', attr: rust }]
        ],
        // Scale 1: 6x3
        [
          [U, { char: ',', attr: body }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: '.', attr: body }, U],
          [{ char: '[', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ']', attr: body }],
          [{ char: 'O', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: 'O', attr: rust }]
        ],
        // Scale 2: 8x4
        [
          [U, U, { char: '_', attr: body }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: '_', attr: body }, U, U],
          [U, { char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '|', attr: glass }, { char: '|', attr: glass }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }, U],
          [{ char: '[', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ']', attr: body }],
          [{ char: '(', attr: rust }, { char: 'O', attr: rust }, { char: ')', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '(', attr: rust }, { char: 'O', attr: rust }, { char: ')', attr: rust }]
        ],
        // Scale 3: 10x5
        [
          [U, U, U, { char: '_', attr: body }, { char: '/', attr: glass }, { char: '\\', attr: glass }, { char: '_', attr: body }, U, U, U],
          [U, U, { char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '|', attr: glass }, { char: '|', attr: glass }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }, U, U],
          [U, { char: '[', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ']', attr: body }, U],
          [{ char: '[', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ']', attr: body }],
          [{ char: '(', attr: rust }, { char: 'O', attr: rust }, { char: ')', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '(', attr: rust }, { char: 'O', attr: rust }, { char: ')', attr: rust }]
        ]
      ]
    };
  },

  /**
   * Create a fallen/collapsed building.
   */
  createFallenBuilding: function(): SpriteDefinition {
    var concrete = makeAttr(DARKGRAY, BG_BLACK);
    var window = makeAttr(BLACK, BG_BLACK);
    var rebar = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'fallen_building',
      variants: [
        // Scale 0: 5x3
        [
          [U, { char: '/', attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: '\\', attr: concrete }, U],
          [{ char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }],
          [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
        ],
        // Scale 1: 7x4
        [
          [U, U, { char: '/', attr: concrete }, { char: '_', attr: rebar }, { char: '\\', attr: concrete }, U, U],
          [U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U],
          [{ char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }],
          [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
        ],
        // Scale 2: 9x5
        [
          [U, U, U, { char: '/', attr: concrete }, { char: '=', attr: rebar }, { char: '\\', attr: concrete }, U, U, U],
          [U, U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U, U],
          [U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U],
          [{ char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }],
          [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
        ],
        // Scale 3: 11x6
        [
          [U, U, U, U, { char: '/', attr: concrete }, { char: '_', attr: rebar }, { char: '\\', attr: concrete }, U, U, U, U],
          [U, U, U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U, U, U],
          [U, U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U, U],
          [U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U],
          [{ char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }],
          [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
        ]
      ]
    };
  },

  /**
   * Create a military tank.
   */
  createTank: function(): SpriteDefinition {
    var armor = makeAttr(GREEN, BG_BLACK);
    var turret = makeAttr(LIGHTGREEN, BG_BLACK);
    var track = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'tank',
      variants: [
        // Scale 0: 4x2
        [
          [{ char: '-', attr: turret }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, U],
          [{ char: 'O', attr: track }, { char: GLYPH.FULL_BLOCK, attr: track }, { char: GLYPH.FULL_BLOCK, attr: track }, { char: 'O', attr: track }]
        ],
        // Scale 1: 6x3
        [
          [U, { char: '-', attr: turret }, { char: '-', attr: turret }, { char: GLYPH.FULL_BLOCK, attr: armor }, U, U],
          [{ char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }],
          [{ char: 'O', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'O', attr: track }]
        ],
        // Scale 2: 8x4
        [
          [U, U, { char: '=', attr: turret }, { char: '=', attr: turret }, { char: '=', attr: turret }, U, U, U],
          [U, { char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: turret }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }, U],
          [{ char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }],
          [{ char: 'O', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'O', attr: track }]
        ],
        // Scale 3: 10x5
        [
          [U, U, U, { char: '=', attr: turret }, { char: '=', attr: turret }, { char: '=', attr: turret }, { char: '=', attr: turret }, U, U, U],
          [U, U, { char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: turret }, { char: GLYPH.FULL_BLOCK, attr: turret }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }, U, U],
          [U, { char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }, U],
          [{ char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }],
          [{ char: 'O', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'O', attr: track }]
        ]
      ]
    };
  },

  /**
   * Create a monster footprint crater.
   */
  createFootprint: function(): SpriteDefinition {
    var dirt = makeAttr(BROWN, BG_BLACK);
    var crater = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'monster_footprint',
      variants: [
        // Scale 0: 4x2
        [
          [{ char: '.', attr: dirt }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '.', attr: dirt }],
          [{ char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }]
        ],
        // Scale 1: 6x3
        [
          [U, { char: '.', attr: dirt }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '.', attr: dirt }, U],
          [{ char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }],
          [U, { char: '\\', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '/', attr: crater }, U]
        ],
        // Scale 2: 8x4
        [
          [U, U, { char: '.', attr: dirt }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '.', attr: dirt }, U, U],
          [U, { char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }, U],
          [{ char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }],
          [U, { char: '\\', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '/', attr: crater }, U]
        ],
        // Scale 3: 10x5
        [
          [U, U, U, { char: '.', attr: dirt }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '.', attr: dirt }, U, U, U],
          [U, U, { char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }, U, U],
          [U, { char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }, U],
          [{ char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }],
          [U, { char: '\\', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '/', attr: crater }, U]
        ]
      ]
    };
  },

  /**
   * Create a danger/warning sign.
   */
  createDangerSign: function(): SpriteDefinition {
    var sign = makeAttr(YELLOW, BG_BLACK);
    var border = makeAttr(RED, BG_BLACK);
    var pole = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'danger_sign',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: '/', attr: border }, { char: '!', attr: sign }, { char: '\\', attr: border }],
          [U, { char: '|', attr: pole }, U]
        ],
        // Scale 1: 4x3
        [
          [U, { char: '/', attr: border }, { char: '\\', attr: border }, U],
          [{ char: '/', attr: border }, { char: '!', attr: sign }, { char: '!', attr: sign }, { char: '\\', attr: border }],
          [U, { char: '|', attr: pole }, { char: '|', attr: pole }, U]
        ],
        // Scale 2: 5x4
        [
          [U, U, { char: '/\\', attr: border }, U, U],
          [U, { char: '/', attr: border }, { char: '!', attr: sign }, { char: '\\', attr: border }, U],
          [{ char: '/', attr: border }, { char: '-', attr: border }, { char: '!', attr: sign }, { char: '-', attr: border }, { char: '\\', attr: border }],
          [U, U, { char: '|', attr: pole }, U, U]
        ],
        // Scale 3: 6x5
        [
          [U, U, { char: '/', attr: border }, { char: '\\', attr: border }, U, U],
          [U, { char: '/', attr: border }, { char: GLYPH.FULL_BLOCK, attr: sign }, { char: GLYPH.FULL_BLOCK, attr: sign }, { char: '\\', attr: border }, U],
          [{ char: '/', attr: border }, { char: GLYPH.FULL_BLOCK, attr: sign }, { char: '!', attr: border }, { char: '!', attr: border }, { char: GLYPH.FULL_BLOCK, attr: sign }, { char: '\\', attr: border }],
          [{ char: '-', attr: border }, { char: '-', attr: border }, { char: '-', attr: border }, { char: '-', attr: border }, { char: '-', attr: border }, { char: '-', attr: border }],
          [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U]
        ]
      ]
    };
  },

  /**
   * Create a fleeing person (civilian running with arms up in panic).
   */
  createFleeingPerson: function(): SpriteDefinition {
    var skin = makeAttr(YELLOW, BG_BLACK);
    var clothes = makeAttr(CYAN, BG_BLACK);
    var hair = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'fleeing_person',
      variants: [
        // Scale 0: 3x2 - simple running figure with arms up
        [
          [{ char: '\\', attr: skin }, { char: 'o', attr: skin }, { char: '/', attr: skin }],
          [U, { char: 'A', attr: clothes }, U]
        ],
        // Scale 1: 3x3 - arms clearly up
        [
          [{ char: '\\', attr: skin }, { char: 'O', attr: skin }, { char: '/', attr: skin }],
          [U, { char: '|', attr: clothes }, U],
          [{ char: '/', attr: clothes }, U, { char: '\\', attr: clothes }]
        ],
        // Scale 2: 5x4 - more detail, panic visible
        [
          [{ char: '\\', attr: skin }, U, { char: 'O', attr: skin }, U, { char: '/', attr: skin }],
          [U, { char: '\\', attr: skin }, { char: '|', attr: clothes }, { char: '/', attr: skin }, U],
          [U, U, { char: 'Y', attr: clothes }, U, U],
          [U, { char: '/', attr: clothes }, U, { char: '\\', attr: clothes }, U]
        ],
        // Scale 3: 5x5 - full detail with panicked face (o_O)
        [
          [{ char: '\\', attr: skin }, U, { char: 'n', attr: hair }, U, { char: '/', attr: skin }],
          [U, { char: '|', attr: skin }, { char: '@', attr: skin }, { char: '|', attr: skin }, U],
          [U, U, { char: 'Y', attr: clothes }, U, U],
          [U, U, { char: '|', attr: clothes }, U, U],
          [U, { char: '/', attr: clothes }, U, { char: '\\', attr: clothes }, U]
        ]
      ]
    };
  },

  /**
   * Create a burning barrel (fire in a barrel).
   */
  createBurningBarrel: function(): SpriteDefinition {
    var barrel = makeAttr(BROWN, BG_BLACK);
    var flame = makeAttr(YELLOW, BG_BLACK);
    var hotFlame = makeAttr(LIGHTRED, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'burning_barrel',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '^', attr: flame }, { char: '^', attr: hotFlame }],
          [{ char: '[', attr: barrel }, { char: ']', attr: barrel }]
        ],
        // Scale 1: 3x3
        [
          [{ char: '^', attr: flame }, { char: '*', attr: hotFlame }, { char: '^', attr: flame }],
          [{ char: '(', attr: flame }, { char: GLYPH.FULL_BLOCK, attr: hotFlame }, { char: ')', attr: flame }],
          [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }]
        ],
        // Scale 2: 4x4
        [
          [U, { char: '^', attr: flame }, { char: '^', attr: hotFlame }, U],
          [{ char: '(', attr: flame }, { char: '*', attr: hotFlame }, { char: '*', attr: hotFlame }, { char: ')', attr: flame }],
          [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }],
          [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }]
        ],
        // Scale 3: 5x5
        [
          [U, { char: '^', attr: flame }, { char: '*', attr: hotFlame }, { char: '^', attr: flame }, U],
          [{ char: '(', attr: flame }, { char: GLYPH.FULL_BLOCK, attr: hotFlame }, { char: GLYPH.FULL_BLOCK, attr: hotFlame }, { char: GLYPH.FULL_BLOCK, attr: hotFlame }, { char: ')', attr: flame }],
          [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }],
          [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }],
          [U, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, U]
        ]
      ]
    };
  },

  /**
   * Create a broken street lamp.
   */
  createBrokenStreetLamp: function(): SpriteDefinition {
    var pole = makeAttr(DARKGRAY, BG_BLACK);
    var spark = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'broken_streetlamp',
      variants: [
        // Scale 0: 2x3
        [
          [{ char: '*', attr: spark }, U],
          [{ char: '/', attr: pole }, U],
          [{ char: '|', attr: pole }, U]
        ],
        // Scale 1: 3x4
        [
          [{ char: '*', attr: spark }, { char: '_', attr: pole }, U],
          [U, { char: '/', attr: pole }, U],
          [U, { char: '|', attr: pole }, U],
          [U, { char: '|', attr: pole }, U]
        ],
        // Scale 2: 4x5
        [
          [{ char: '*', attr: spark }, { char: '*', attr: spark }, { char: '_', attr: pole }, U],
          [U, U, { char: '/', attr: pole }, U],
          [U, U, { char: '|', attr: pole }, U],
          [U, U, { char: '|', attr: pole }, U],
          [U, { char: '=', attr: pole }, { char: '|', attr: pole }, { char: '=', attr: pole }]
        ],
        // Scale 3: 5x6
        [
          [{ char: '*', attr: spark }, { char: '*', attr: spark }, { char: '_', attr: pole }, { char: '_', attr: pole }, U],
          [U, U, U, { char: '/', attr: pole }, U],
          [U, U, U, { char: '|', attr: pole }, U],
          [U, U, U, { char: '|', attr: pole }, U],
          [U, U, U, { char: '|', attr: pole }, U],
          [U, { char: '=', attr: pole }, { char: '=', attr: pole }, { char: '|', attr: pole }, { char: '=', attr: pole }]
        ]
      ]
    };
  }
};

// Register all kaiju roadside sprites
registerRoadsideSprite('rubble', KaijuSprites.createRubble);
registerRoadsideSprite('fire', KaijuSprites.createFire);
registerRoadsideSprite('wrecked_car', KaijuSprites.createWreckedCar);
registerRoadsideSprite('fallen_building', KaijuSprites.createFallenBuilding);
registerRoadsideSprite('tank', KaijuSprites.createTank);
registerRoadsideSprite('monster_footprint', KaijuSprites.createFootprint);
registerRoadsideSprite('danger_sign', KaijuSprites.createDangerSign);
registerRoadsideSprite('fleeing_person', KaijuSprites.createFleeingPerson);
registerRoadsideSprite('burning_barrel', KaijuSprites.createBurningBarrel);
registerRoadsideSprite('broken_streetlamp', KaijuSprites.createBrokenStreetLamp);
