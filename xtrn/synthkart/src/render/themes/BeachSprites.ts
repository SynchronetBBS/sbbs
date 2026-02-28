/**
 * BeachSprites - Roadside sprites for tropical beach themes.
 * 
 * Includes: palm trees, umbrellas, lifeguard towers, surfboards, tiki torches, beach huts
 */

var BeachSprites = {
  /**
   * Create a palm tree sprite at various scales.
   */
  createPalm: function(): SpriteDefinition {
    var frond = makeAttr(LIGHTGREEN, BG_BLACK);
    var frondDark = makeAttr(GREEN, BG_BLACK);
    var trunk = makeAttr(BROWN, BG_BLACK);
    var coconut = makeAttr(BROWN, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'palm',
      variants: [
        // Scale 0: 2x3 - tiny distant palm
        [
          [{ char: 'Y', attr: frond }],
          [{ char: '|', attr: trunk }],
          [{ char: '|', attr: trunk }]
        ],
        // Scale 1: 3x4
        [
          [{ char: '\\', attr: frondDark }, { char: '|', attr: frond }, { char: '/', attr: frondDark }],
          [U, { char: '|', attr: trunk }, U],
          [U, { char: '|', attr: trunk }, U],
          [U, { char: '|', attr: trunk }, U]
        ],
        // Scale 2: 5x5
        [
          [{ char: '~', attr: frondDark }, { char: '\\', attr: frond }, { char: '|', attr: frond }, { char: '/', attr: frond }, { char: '~', attr: frondDark }],
          [U, { char: '\\', attr: frond }, { char: 'o', attr: coconut }, { char: '/', attr: frond }, U],
          [U, U, { char: '|', attr: trunk }, U, U],
          [U, U, { char: ')', attr: trunk }, U, U],
          [U, U, { char: '|', attr: trunk }, U, U]
        ],
        // Scale 3: 7x7
        [
          [{ char: '/', attr: frondDark }, { char: '~', attr: frond }, U, U, U, { char: '~', attr: frond }, { char: '\\', attr: frondDark }],
          [U, { char: '\\', attr: frond }, { char: '_', attr: frond }, { char: 'Y', attr: frond }, { char: '_', attr: frond }, { char: '/', attr: frond }, U],
          [U, U, { char: '\\', attr: frondDark }, { char: '|', attr: trunk }, { char: '/', attr: frondDark }, U, U],
          [U, U, U, { char: '(', attr: trunk }, U, U, U],
          [U, U, U, { char: '|', attr: trunk }, U, U, U],
          [U, U, U, { char: ')', attr: trunk }, U, U, U],
          [U, U, U, { char: '|', attr: trunk }, U, U, U]
        ],
        // Scale 4: 9x9 (very close)
        [
          [{ char: '/', attr: frondDark }, { char: '~', attr: frond }, { char: '~', attr: frondDark }, U, U, U, { char: '~', attr: frondDark }, { char: '~', attr: frond }, { char: '\\', attr: frondDark }],
          [U, { char: '\\', attr: frond }, { char: '_', attr: frond }, { char: '|', attr: frond }, { char: 'o', attr: coconut }, { char: '|', attr: frond }, { char: '_', attr: frond }, { char: '/', attr: frond }, U],
          [U, U, { char: '\\', attr: frondDark }, { char: '\\', attr: frond }, { char: '|', attr: trunk }, { char: '/', attr: frond }, { char: '/', attr: frondDark }, U, U],
          [U, U, U, U, { char: '|', attr: trunk }, { char: ')', attr: trunk }, U, U, U],
          [U, U, U, U, { char: '(', attr: trunk }, { char: '|', attr: trunk }, U, U, U],
          [U, U, U, U, { char: '|', attr: trunk }, { char: ')', attr: trunk }, U, U, U],
          [U, U, U, U, { char: '(', attr: trunk }, { char: '|', attr: trunk }, U, U, U],
          [U, U, U, U, { char: '|', attr: trunk }, { char: ')', attr: trunk }, U, U, U],
          [U, U, U, U, { char: '|', attr: trunk }, { char: '|', attr: trunk }, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a beach umbrella sprite at various scales.
   */
  createUmbrella: function(): SpriteDefinition {
    var stripe1 = makeAttr(LIGHTRED, BG_BLACK);
    var stripe2 = makeAttr(YELLOW, BG_BLACK);
    var pole = makeAttr(WHITE, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'umbrella',
      variants: [
        // Scale 0: 3x2 - tiny distant
        [
          [{ char: '/', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '\\', attr: stripe1 }],
          [U, { char: '|', attr: pole }, U]
        ],
        // Scale 1: 5x3
        [
          [U, { char: '/', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '\\', attr: stripe1 }, U],
          [U, U, { char: '|', attr: pole }, U, U],
          [U, U, { char: '+', attr: pole }, U, U]
        ],
        // Scale 2: 7x4
        [
          [U, { char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }, U],
          [U, U, U, { char: '|', attr: pole }, U, U, U],
          [U, U, U, { char: '|', attr: pole }, U, U, U],
          [U, U, U, { char: '+', attr: pole }, U, U, U]
        ],
        // Scale 3: 9x5
        [
          [U, { char: '_', attr: stripe1 }, { char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }, { char: '_', attr: stripe1 }, U],
          [{ char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '=', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '|', attr: pole }, { char: '=', attr: stripe1 }, { char: '=', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }],
          [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
          [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
          [U, U, U, U, { char: '+', attr: pole }, U, U, U, U]
        ],
        // Scale 4: 9x6
        [
          [U, { char: '_', attr: stripe1 }, { char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }, { char: '_', attr: stripe1 }, U],
          [{ char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '=', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '|', attr: pole }, { char: '=', attr: stripe1 }, { char: '=', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }],
          [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
          [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
          [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
          [U, U, U, U, { char: '+', attr: pole }, U, U, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a lifeguard tower sprite.
   */
  createLifeguard: function(): SpriteDefinition {
    var red = makeAttr(LIGHTRED, BG_BLACK);
    var white = makeAttr(WHITE, BG_BLACK);
    var wood = makeAttr(BROWN, BG_BLACK);
    var window = makeAttr(LIGHTCYAN, BG_CYAN);
    var U: SpriteCell | null = null;
    
    return {
      name: 'lifeguard',
      variants: [
        // Scale 0: 2x3
        [
          [{ char: '[', attr: red }, { char: ']', attr: red }],
          [{ char: '/', attr: wood }, { char: '\\', attr: wood }],
          [{ char: '/', attr: wood }, { char: '\\', attr: wood }]
        ],
        // Scale 1: 3x4
        [
          [{ char: '[', attr: red }, { char: '#', attr: white }, { char: ']', attr: red }],
          [{ char: '/', attr: wood }, { char: '_', attr: wood }, { char: '\\', attr: wood }],
          [{ char: '/', attr: wood }, U, { char: '\\', attr: wood }],
          [{ char: '/', attr: wood }, U, { char: '\\', attr: wood }]
        ],
        // Scale 2: 5x5
        [
          [U, { char: '[', attr: red }, { char: '=', attr: white }, { char: ']', attr: red }, U],
          [U, { char: '|', attr: white }, { char: ' ', attr: window }, { char: '|', attr: white }, U],
          [{ char: '/', attr: wood }, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: wood }],
          [{ char: '/', attr: wood }, U, U, U, { char: '\\', attr: wood }],
          [{ char: '/', attr: wood }, U, U, U, { char: '\\', attr: wood }]
        ],
        // Scale 3: 7x6
        [
          [U, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, U],
          [{ char: '/', attr: white }, { char: '[', attr: red }, { char: '=', attr: white }, { char: '=', attr: white }, { char: '=', attr: white }, { char: ']', attr: red }, { char: '\\', attr: white }],
          [U, { char: '|', attr: white }, { char: ' ', attr: window }, { char: ' ', attr: window }, { char: ' ', attr: red }, { char: '|', attr: white }, U],
          [{ char: '/', attr: wood }, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: wood }],
          [{ char: '/', attr: wood }, U, U, U, U, U, { char: '\\', attr: wood }],
          [{ char: '/', attr: wood }, U, U, U, U, U, { char: '\\', attr: wood }]
        ],
        // Scale 4: 9x8
        [
          [U, U, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, U, U],
          [U, { char: '/', attr: white }, { char: '[', attr: red }, { char: '=', attr: white }, { char: '=', attr: white }, { char: '=', attr: white }, { char: ']', attr: red }, { char: '\\', attr: white }, U],
          [U, { char: '|', attr: red }, { char: '_', attr: white }, { char: ' ', attr: window }, { char: ' ', attr: window }, { char: '_', attr: white }, { char: '|', attr: red }, U, U],
          [U, { char: '|', attr: white }, { char: ' ', attr: window }, { char: ' ', attr: window }, { char: ' ', attr: red }, { char: ' ', attr: red }, { char: '|', attr: white }, U, U],
          [{ char: '/', attr: wood }, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: wood }, U],
          [{ char: '/', attr: wood }, U, U, U, U, U, U, { char: '\\', attr: wood }, U],
          [{ char: '/', attr: wood }, U, U, U, U, U, U, { char: '\\', attr: wood }, U],
          [{ char: '/', attr: wood }, U, U, U, U, U, U, { char: '\\', attr: wood }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a surfboard sprite.
   */
  createSurfboard: function(): SpriteDefinition {
    var board = makeAttr(LIGHTCYAN, BG_BLACK);
    var accent = makeAttr(CYAN, BG_BLACK);
    var star = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'surfboard',
      variants: [
        // Scale 0: 1x3
        [
          [{ char: '^', attr: board }],
          [{ char: '|', attr: board }],
          [{ char: 'v', attr: accent }]
        ],
        // Scale 1: 1x4
        [
          [{ char: '^', attr: board }],
          [{ char: '|', attr: board }],
          [{ char: '|', attr: accent }],
          [{ char: 'v', attr: board }]
        ],
        // Scale 2: 2x5
        [
          [{ char: '/', attr: board }, { char: '\\', attr: board }],
          [{ char: '|', attr: board }, { char: '|', attr: accent }],
          [{ char: '|', attr: accent }, { char: '|', attr: board }],
          [{ char: '|', attr: board }, { char: '|', attr: accent }],
          [{ char: '\\', attr: accent }, { char: '/', attr: accent }]
        ],
        // Scale 3: 3x6
        [
          [U, { char: '^', attr: board }, U],
          [{ char: '/', attr: board }, { char: '~', attr: accent }, { char: '\\', attr: board }],
          [{ char: '|', attr: board }, { char: '*', attr: star }, { char: '|', attr: accent }],
          [{ char: '|', attr: accent }, { char: ' ', attr: board }, { char: '|', attr: board }],
          [{ char: '|', attr: board }, { char: ' ', attr: board }, { char: '|', attr: accent }],
          [{ char: '\\', attr: accent }, { char: '_', attr: board }, { char: '/', attr: accent }]
        ],
        // Scale 4: 4x8
        [
          [U, { char: '/', attr: board }, { char: '\\', attr: board }, U],
          [{ char: '/', attr: board }, { char: '~', attr: accent }, { char: '~', attr: board }, { char: '\\', attr: board }],
          [{ char: '|', attr: board }, { char: ' ', attr: accent }, { char: ' ', attr: accent }, { char: '|', attr: accent }],
          [{ char: '|', attr: accent }, { char: '*', attr: star }, { char: ' ', attr: board }, { char: '|', attr: board }],
          [{ char: '|', attr: board }, { char: ' ', attr: board }, { char: ' ', attr: board }, { char: '|', attr: accent }],
          [{ char: '|', attr: accent }, { char: ' ', attr: board }, { char: ' ', attr: board }, { char: '|', attr: board }],
          [{ char: '\\', attr: board }, { char: '_', attr: accent }, { char: '_', attr: accent }, { char: '/', attr: board }],
          [U, { char: '\\', attr: accent }, { char: '/', attr: accent }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a tiki torch sprite.
   */
  createTiki: function(): SpriteDefinition {
    var flame = makeAttr(YELLOW, BG_BLACK);
    var flameOrange = makeAttr(LIGHTRED, BG_BLACK);
    var bamboo = makeAttr(BROWN, BG_BLACK);
    var band = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'tiki',
      variants: [
        // Scale 0: 1x3
        [
          [{ char: '*', attr: flame }],
          [{ char: '#', attr: bamboo }],
          [{ char: '|', attr: bamboo }]
        ],
        // Scale 1: 3x4
        [
          [U, { char: '*', attr: flame }, U],
          [U, { char: '#', attr: bamboo }, U],
          [U, { char: '|', attr: bamboo }, U],
          [U, { char: '|', attr: bamboo }, U]
        ],
        // Scale 2: 3x5
        [
          [{ char: '\\', attr: flame }, { char: '*', attr: flameOrange }, { char: '/', attr: flame }],
          [U, { char: '#', attr: bamboo }, U],
          [U, { char: '|', attr: bamboo }, U],
          [U, { char: '=', attr: band }, U],
          [U, { char: '|', attr: bamboo }, U]
        ],
        // Scale 3: 5x7
        [
          [U, { char: '(', attr: flame }, { char: '*', attr: flameOrange }, { char: ')', attr: flame }, U],
          [U, { char: '\\', attr: flameOrange }, { char: '|', attr: flame }, { char: '/', attr: flameOrange }, U],
          [U, { char: '[', attr: bamboo }, { char: '#', attr: bamboo }, { char: ']', attr: bamboo }, U],
          [U, U, { char: '|', attr: bamboo }, U, U],
          [U, U, { char: '=', attr: band }, U, U],
          [U, U, { char: '|', attr: bamboo }, U, U],
          [U, U, { char: '|', attr: bamboo }, U, U]
        ],
        // Scale 4: 5x8
        [
          [U, { char: '(', attr: flame }, { char: '*', attr: flameOrange }, { char: ')', attr: flame }, U],
          [U, { char: '\\', attr: flameOrange }, { char: '|', attr: flame }, { char: '/', attr: flameOrange }, U],
          [U, { char: '[', attr: bamboo }, { char: '#', attr: bamboo }, { char: ']', attr: bamboo }, U],
          [U, U, { char: '|', attr: bamboo }, U, U],
          [U, U, { char: '=', attr: band }, U, U],
          [U, U, { char: '|', attr: bamboo }, U, U],
          [U, U, { char: '=', attr: band }, U, U],
          [U, U, { char: '|', attr: bamboo }, U, U]
        ]
      ]
    };
  },
  
  /**
   * Create a beach hut sprite.
   */
  createBeachhut: function(): SpriteDefinition {
    var thatch = makeAttr(YELLOW, BG_BLACK);
    var thatchDark = makeAttr(BROWN, BG_BLACK);
    var wood = makeAttr(BROWN, BG_BLACK);
    var window = makeAttr(LIGHTCYAN, BG_CYAN);
    var U: SpriteCell | null = null;
    
    return {
      name: 'beachhut',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '\\', attr: thatch }],
          [{ char: '[', attr: wood }, { char: '#', attr: wood }, { char: ']', attr: wood }]
        ],
        // Scale 1: 4x3
        [
          [U, { char: '/', attr: thatch }, { char: '\\', attr: thatch }, U],
          [{ char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '\\', attr: thatch }],
          [{ char: '|', attr: wood }, { char: '#', attr: wood }, { char: '#', attr: wood }, { char: '|', attr: wood }]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '\\', attr: thatch }, U],
          [{ char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '~', attr: thatchDark }, { char: '\\', attr: thatch }],
          [{ char: '|', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '|', attr: wood }],
          [{ char: '|', attr: wood }, { char: ' ', attr: window }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: '|', attr: wood }]
        ],
        // Scale 3: 7x5
        [
          [U, U, { char: '_', attr: thatch }, { char: '/', attr: thatch }, { char: '\\', attr: thatch }, { char: '_', attr: thatch }, U],
          [U, { char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '\\', attr: thatch }],
          [{ char: '/', attr: thatch }, { char: '|', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: thatch }],
          [U, { char: '|', attr: wood }, { char: ' ', attr: window }, { char: '|', attr: wood }, { char: '/', attr: wood }, { char: '\\', attr: wood }, { char: '|', attr: wood }],
          [U, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: '|', attr: wood }, { char: '|', attr: wood }]
        ],
        // Scale 4: 10x7
        [
          [U, U, { char: '_', attr: thatch }, { char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '\\', attr: thatch }, { char: '_', attr: thatch }, U, U],
          [U, { char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '\\', attr: thatch }, U],
          [{ char: '/', attr: thatch }, { char: '|', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: thatch }],
          [U, { char: '|', attr: wood }, { char: ' ', attr: window }, { char: ' ', attr: window }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: '/', attr: wood }, { char: '\\', attr: wood }, { char: '|', attr: wood }, U],
          [U, { char: '|', attr: wood }, { char: '_', attr: window }, { char: '_', attr: window }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: ' ', attr: wood }, { char: '|', attr: wood }, { char: '|', attr: wood }, U],
          [U, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: '|', attr: wood }, { char: '|', attr: wood }, U],
          [U, { char: '|', attr: wood }, U, U, U, U, U, U, { char: '|', attr: wood }, U]
        ]
      ]
    };
  }
};

// Register beach sprites
registerRoadsideSprite('palm', BeachSprites.createPalm);
registerRoadsideSprite('umbrella', BeachSprites.createUmbrella);
registerRoadsideSprite('lifeguard', BeachSprites.createLifeguard);
registerRoadsideSprite('surfboard', BeachSprites.createSurfboard);
registerRoadsideSprite('tiki', BeachSprites.createTiki);
registerRoadsideSprite('beachhut', BeachSprites.createBeachhut);
