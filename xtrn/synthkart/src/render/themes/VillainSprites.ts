/**
 * VillainSprites.ts - Evil lair roadside objects.
 * Lava rocks, flames, skull piles, chains, spikes, and cauldrons.
 */

var VillainSprites = {
  /**
   * Create a lava rock sprite - volcanic boulder.
   */
  createLavaRock: function(): SpriteDefinition {
    var rock = makeAttr(DARKGRAY, BG_BLACK);
    var rockDark = makeAttr(BLACK, BG_BLACK);
    var lava = makeAttr(LIGHTRED, BG_BLACK);
    var lavaGlow = makeAttr(YELLOW, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'lava_rock',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '(', attr: rock }, { char: ')', attr: rock }],
          [{ char: GLYPH.LOWER_HALF, attr: lava }, { char: GLYPH.LOWER_HALF, attr: lava }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '^', attr: rock }, U],
          [{ char: '(', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: ')', attr: rockDark }],
          [{ char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '/', attr: rock }, { char: '^', attr: rockDark }, { char: '\\', attr: rock }, U],
          [{ char: '/', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '\\', attr: rockDark }],
          [{ char: '(', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '*', attr: lavaGlow }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: ')', attr: rock }],
          [{ char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }]
        ],
        // Scale 3: 7x5
        [
          [U, U, { char: '/', attr: rock }, { char: '^', attr: rockDark }, { char: '\\', attr: rock }, U, U],
          [U, { char: '/', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '\\', attr: rockDark }, U],
          [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '*', attr: lavaGlow }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '\\', attr: rock }],
          [{ char: '(', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '*', attr: lava }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: ')', attr: rockDark }],
          [{ char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }]
        ],
        // Scale 4: 7x5 (same)
        [
          [U, U, { char: '/', attr: rock }, { char: '^', attr: rockDark }, { char: '\\', attr: rock }, U, U],
          [U, { char: '/', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '\\', attr: rockDark }, U],
          [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '*', attr: lavaGlow }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '\\', attr: rock }],
          [{ char: '(', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '*', attr: lava }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: ')', attr: rockDark }],
          [{ char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }]
        ]
      ]
    };
  },
  
  /**
   * Create a flame sprite - fire pillar.
   */
  createFlame: function(): SpriteDefinition {
    var core = makeAttr(YELLOW, BG_BLACK);
    var mid = makeAttr(LIGHTRED, BG_BLACK);
    var outer = makeAttr(RED, BG_BLACK);
    var smoke = makeAttr(DARKGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'flame',
      variants: [
        // Scale 0: 1x3
        [
          [{ char: '*', attr: core }],
          [{ char: '^', attr: mid }],
          [{ char: '~', attr: outer }]
        ],
        // Scale 1: 3x4
        [
          [U, { char: '.', attr: smoke }, U],
          [U, { char: '*', attr: core }, U],
          [{ char: '(', attr: mid }, { char: '^', attr: core }, { char: ')', attr: mid }],
          [{ char: '~', attr: outer }, { char: '^', attr: mid }, { char: '~', attr: outer }]
        ],
        // Scale 2: 5x5
        [
          [U, U, { char: '.', attr: smoke }, U, U],
          [U, { char: '(', attr: core }, { char: '*', attr: core }, { char: ')', attr: core }, U],
          [{ char: '(', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: ')', attr: mid }],
          [{ char: '(', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: ')', attr: outer }],
          [U, { char: '~', attr: outer }, { char: '^', attr: mid }, { char: '~', attr: outer }, U]
        ],
        // Scale 3: 7x6
        [
          [U, U, { char: '.', attr: smoke }, { char: '.', attr: smoke }, { char: '.', attr: smoke }, U, U],
          [U, U, { char: '(', attr: core }, { char: '*', attr: core }, { char: ')', attr: core }, U, U],
          [U, { char: '(', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: ')', attr: mid }, U],
          [{ char: '(', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: ')', attr: outer }],
          [{ char: '(', attr: outer }, { char: '^', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: '^', attr: outer }, { char: ')', attr: outer }],
          [U, { char: '~', attr: outer }, { char: '~', attr: mid }, { char: '^', attr: mid }, { char: '~', attr: mid }, { char: '~', attr: outer }, U]
        ],
        // Scale 4: 7x6 (same)
        [
          [U, U, { char: '.', attr: smoke }, { char: '.', attr: smoke }, { char: '.', attr: smoke }, U, U],
          [U, U, { char: '(', attr: core }, { char: '*', attr: core }, { char: ')', attr: core }, U, U],
          [U, { char: '(', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: ')', attr: mid }, U],
          [{ char: '(', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: ')', attr: outer }],
          [{ char: '(', attr: outer }, { char: '^', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: '^', attr: outer }, { char: ')', attr: outer }],
          [U, { char: '~', attr: outer }, { char: '~', attr: mid }, { char: '^', attr: mid }, { char: '~', attr: mid }, { char: '~', attr: outer }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a skull pile sprite - pile of bones.
   */
  createSkullPile: function(): SpriteDefinition {
    var skull = makeAttr(WHITE, BG_BLACK);
    var skullDark = makeAttr(LIGHTGRAY, BG_BLACK);
    var bone = makeAttr(LIGHTGRAY, BG_BLACK);
    var eye = makeAttr(RED, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'skull_pile',
      variants: [
        // Scale 0: 2x2
        [
          [{ char: '(', attr: skull }, { char: ')', attr: skull }],
          [{ char: 'o', attr: skullDark }, { char: 'o', attr: skullDark }]
        ],
        // Scale 1: 3x3
        [
          [U, { char: '(', attr: skull }, { char: ')', attr: skull }],
          [{ char: '(', attr: skullDark }, { char: 'o', attr: eye }, { char: ')', attr: skullDark }],
          [{ char: '-', attr: bone }, { char: '-', attr: bone }, { char: '-', attr: bone }]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '/', attr: skull }, { char: 'O', attr: skull }, { char: '\\', attr: skull }, U],
          [{ char: '/', attr: skullDark }, { char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }, { char: '\\', attr: skullDark }],
          [{ char: '(', attr: skull }, { char: 'o', attr: skullDark }, { char: '(', attr: skull }, { char: 'o', attr: skullDark }, { char: ')', attr: skull }],
          [{ char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }]
        ],
        // Scale 3: 7x5
        [
          [U, U, { char: '/', attr: skull }, { char: 'O', attr: skull }, { char: '\\', attr: skull }, U, U],
          [U, { char: '/', attr: skull }, { char: '*', attr: eye }, { char: 'v', attr: skull }, { char: '*', attr: eye }, { char: '\\', attr: skull }, U],
          [{ char: '/', attr: skullDark }, { char: 'O', attr: skull }, { char: '\\', attr: skullDark }, U, { char: '/', attr: skullDark }, { char: 'O', attr: skull }, { char: '\\', attr: skullDark }],
          [{ char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }, { char: '-', attr: bone }, { char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }],
          [{ char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }]
        ],
        // Scale 4: 7x5 (same)
        [
          [U, U, { char: '/', attr: skull }, { char: 'O', attr: skull }, { char: '\\', attr: skull }, U, U],
          [U, { char: '/', attr: skull }, { char: '*', attr: eye }, { char: 'v', attr: skull }, { char: '*', attr: eye }, { char: '\\', attr: skull }, U],
          [{ char: '/', attr: skullDark }, { char: 'O', attr: skull }, { char: '\\', attr: skullDark }, U, { char: '/', attr: skullDark }, { char: 'O', attr: skull }, { char: '\\', attr: skullDark }],
          [{ char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }, { char: '-', attr: bone }, { char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }],
          [{ char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }]
        ]
      ]
    };
  },
  
  /**
   * Create a chain sprite - hanging chains.
   */
  createChain: function(): SpriteDefinition {
    var chain = makeAttr(DARKGRAY, BG_BLACK);
    var chainLight = makeAttr(LIGHTGRAY, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'chain',
      variants: [
        // Scale 0: 1x4
        [
          [{ char: 'o', attr: chain }],
          [{ char: 'O', attr: chainLight }],
          [{ char: 'o', attr: chain }],
          [{ char: 'O', attr: chainLight }]
        ],
        // Scale 1: 2x5
        [
          [{ char: '(', attr: chain }, { char: ')', attr: chain }],
          [U, { char: 'O', attr: chainLight }],
          [{ char: '(', attr: chain }, { char: ')', attr: chain }],
          [U, { char: 'O', attr: chainLight }],
          [{ char: '(', attr: chain }, { char: ')', attr: chain }]
        ],
        // Scale 2: 3x6
        [
          [{ char: '/', attr: chain }, { char: 'o', attr: chainLight }, { char: '\\', attr: chain }],
          [U, { char: 'O', attr: chain }, U],
          [{ char: '/', attr: chainLight }, { char: 'o', attr: chain }, { char: '\\', attr: chainLight }],
          [U, { char: 'O', attr: chainLight }, U],
          [{ char: '/', attr: chain }, { char: 'o', attr: chainLight }, { char: '\\', attr: chain }],
          [U, { char: 'O', attr: chain }, U]
        ],
        // Scale 3: 3x7
        [
          [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
          [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
          [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
          [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
          [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
          [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
          [U, { char: 'V', attr: chain }, U]
        ],
        // Scale 4: 3x7 (same)
        [
          [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
          [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
          [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
          [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
          [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
          [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
          [U, { char: 'V', attr: chain }, U]
        ]
      ]
    };
  },
  
  /**
   * Create a spike sprite - deadly metal spikes.
   */
  createSpike: function(): SpriteDefinition {
    var spike = makeAttr(LIGHTGRAY, BG_BLACK);
    var spikeDark = makeAttr(DARKGRAY, BG_BLACK);
    var blood = makeAttr(RED, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'spike',
      variants: [
        // Scale 0: 3x2
        [
          [{ char: '^', attr: spike }, { char: '^', attr: spike }, { char: '^', attr: spike }],
          [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }]
        ],
        // Scale 1: 4x3
        [
          [U, { char: '^', attr: spike }, { char: '^', attr: spike }, U],
          [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }],
          [{ char: '/', attr: spikeDark }, { char: '/', attr: spike }, { char: '\\', attr: spike }, { char: '\\', attr: spikeDark }]
        ],
        // Scale 2: 5x4
        [
          [U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U],
          [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '^', attr: spike }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }],
          [{ char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spike }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }],
          [{ char: '/', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spike }, { char: '-', attr: spike }, { char: '\\', attr: spikeDark }]
        ],
        // Scale 3: 7x5
        [
          [U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U],
          [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }],
          [{ char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }],
          [{ char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '.', attr: blood }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }],
          [{ char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }]
        ],
        // Scale 4: 7x5 (same)
        [
          [U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U],
          [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }],
          [{ char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }],
          [{ char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '.', attr: blood }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }],
          [{ char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }]
        ]
      ]
    };
  },
  
  /**
   * Create a cauldron sprite - bubbling witch pot.
   */
  createCauldron: function(): SpriteDefinition {
    var pot = makeAttr(DARKGRAY, BG_BLACK);
    var potDark = makeAttr(BLACK, BG_BLACK);
    var brew = makeAttr(LIGHTGREEN, BG_BLACK);
    var bubble = makeAttr(GREEN, BG_BLACK);
    var fire = makeAttr(LIGHTRED, BG_BLACK);
    var U: SpriteCell | null = null;
    
    return {
      name: 'cauldron',
      variants: [
        // Scale 0: 2x3
        [
          [{ char: 'o', attr: bubble }, { char: 'o', attr: bubble }],
          [{ char: '(', attr: pot }, { char: ')', attr: pot }],
          [{ char: '^', attr: fire }, { char: '^', attr: fire }]
        ],
        // Scale 1: 3x4
        [
          [U, { char: 'o', attr: bubble }, U],
          [{ char: '(', attr: pot }, { char: '~', attr: brew }, { char: ')', attr: pot }],
          [{ char: '(', attr: potDark }, { char: '_', attr: pot }, { char: ')', attr: potDark }],
          [{ char: '^', attr: fire }, { char: '^', attr: fire }, { char: '^', attr: fire }]
        ],
        // Scale 2: 5x5
        [
          [U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U],
          [{ char: '/', attr: pot }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '\\', attr: pot }],
          [{ char: '|', attr: pot }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '|', attr: pot }],
          [{ char: '\\', attr: potDark }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '/', attr: potDark }],
          [U, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, U]
        ],
        // Scale 3: 7x6
        [
          [U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U],
          [U, { char: '/', attr: pot }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '\\', attr: pot }, U],
          [{ char: '/', attr: pot }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: 'o', attr: bubble }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: 'o', attr: bubble }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '\\', attr: pot }],
          [{ char: '|', attr: potDark }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '|', attr: potDark }],
          [{ char: '\\', attr: potDark }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '/', attr: potDark }],
          [U, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, U]
        ],
        // Scale 4: 7x6 (same)
        [
          [U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U],
          [U, { char: '/', attr: pot }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '\\', attr: pot }, U],
          [{ char: '/', attr: pot }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: 'o', attr: bubble }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: 'o', attr: bubble }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '\\', attr: pot }],
          [{ char: '|', attr: potDark }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '|', attr: potDark }],
          [{ char: '\\', attr: potDark }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '/', attr: potDark }],
          [U, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, U]
        ]
      ]
    };
  }
};

// Register villain sprites
registerRoadsideSprite('lava_rock', VillainSprites.createLavaRock);
registerRoadsideSprite('flame', VillainSprites.createFlame);
registerRoadsideSprite('skull_pile', VillainSprites.createSkullPile);
registerRoadsideSprite('chain', VillainSprites.createChain);
registerRoadsideSprite('spike', VillainSprites.createSpike);
registerRoadsideSprite('cauldron', VillainSprites.createCauldron);
