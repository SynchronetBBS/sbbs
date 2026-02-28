/**
 * PlayerCarSprites - Parameterized player car sprites with multiple body styles.
 * 
 * Unlike NPC vehicles which have scaling variants (far to near), player cars
 * are always rendered at full size at the bottom of the screen. However,
 * we define multiple body styles and support dynamic color/brake light states.
 * 
 * Body Styles:
 * - sports:  Balanced sporty look (default)
 * - muscle:  Wide, aggressive stance
 * - compact: Small, nimble shape
 * - super:   Low, sleek supercar
 * - classic: Vintage car silhouette
 * 
 * All sprites are 5 columns x 3 rows to maintain consistent positioning.
 */

/**
 * Player car sprite configuration - passed to sprite creation.
 */
interface PlayerCarConfig {
  /** Body color (ANSI foreground) */
  bodyColor: number;
  /** Trim/window color (ANSI foreground) */
  trimColor: number;
  /** Whether brake lights are active */
  brakeLightsOn: boolean;
}

/**
 * Extended sprite definition that includes brake light positions.
 */
interface PlayerCarSpriteDefinition extends SpriteDefinition {
  /** Brake light cell positions [row][col] indices */
  brakeLightCells: { row: number; col: number }[];
}

/**
 * Cache for player car sprites.
 */
var PLAYER_CAR_SPRITE_CACHE: { [key: string]: PlayerCarSpriteDefinition } = {};

/**
 * Create a sports car sprite - the default balanced look.
 * 
 *    ▄█▄      (row 0: roof/window)
 *   █████     (row 1: body)  
 *   ▀▄▀▄▀     (row 2: wheels)
 */
function createPlayerSportsCarSprite(config: PlayerCarConfig): PlayerCarSpriteDefinition {
  var body = makeAttr(config.bodyColor, BG_BLACK);
  var trim = makeAttr(config.trimColor, BG_BLACK);
  var wheel = makeAttr(DARKGRAY, BG_BLACK);
  // Brake light: body color on top, red on bottom when ON; solid body when OFF
  var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
  var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
  var U: SpriteCell | null = null;
  
  return {
    name: 'player_sports',
    brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
    variants: [
      [
        [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
        [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
        [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
      ]
    ]
  };
}

/**
 * Create a muscle car sprite - wide and aggressive.
 * 
 *    ▄▄▄      (row 0: flat roof)
 *   █████     (row 1: wide body with lights)
 *   ▀█▀█▀     (row 2: big wheels)
 */
function createMuscleCarSprite(config: PlayerCarConfig): PlayerCarSpriteDefinition {
  var body = makeAttr(config.bodyColor, BG_BLACK);
  var trim = makeAttr(config.trimColor, BG_BLACK);
  var wheel = makeAttr(DARKGRAY, BG_BLACK);
  // Brake light: body color on top, red on bottom when ON; solid body when OFF
  var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
  var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
  var U: SpriteCell | null = null;
  
  return {
    name: 'player_muscle',
    brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
    variants: [
      [
        [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
        [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
        [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
      ]
    ]
  };
}

/**
 * Create a compact car sprite - small and nimble.
 * 
 *     ▄█▄     (row 0: bubble roof)
 *    ████     (row 1: small body)
 *    ▀▀▀▀     (row 2: small wheels)
 */
function createCompactCarSprite(config: PlayerCarConfig): PlayerCarSpriteDefinition {
  var body = makeAttr(config.bodyColor, BG_BLACK);
  var trim = makeAttr(config.trimColor, BG_BLACK);
  var wheel = makeAttr(DARKGRAY, BG_BLACK);
  // Brake light: body color on top, red on bottom when ON; solid body when OFF
  var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
  var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
  var U: SpriteCell | null = null;
  
  return {
    name: 'player_compact',
    brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
    variants: [
      [
        [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
        [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
        [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: wheel }]
      ]
    ]
  };
}

/**
 * Create a supercar sprite - low and sleek.
 * 
 *     ▄▀▄     (row 0: aero windshield)
 *   █████     (row 1: wide low body)
 *   ▀▀▀▀▀     (row 2: flush wheels)
 */
function createSuperCarSprite(config: PlayerCarConfig): PlayerCarSpriteDefinition {
  var body = makeAttr(config.bodyColor, BG_BLACK);
  var trim = makeAttr(config.trimColor, BG_BLACK);
  var wheelWell = makeAttr(DARKGRAY, BG_BLACK);  // Sleek supercars show wheel wells
  // Brake light: body color on top, red on bottom when ON; solid body when OFF
  var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
  var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
  var U: SpriteCell | null = null;
  
  return {
    name: 'player_super',
    brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
    variants: [
      [
        [U, { char: GLYPH.UPPER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: body }, U],
        [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
        [{ char: GLYPH.LOWER_HALF, attr: wheelWell }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheelWell }]
      ]
    ]
  };
}

/**
 * Create a classic car sprite - vintage look.
 * 
 *    ▄██▄     (row 0: rounded roof)
 *   █████     (row 1: chrome trim body)
 *   ▀▄▀▄▀     (row 2: chrome bumper wheels)
 */
function createClassicCarSprite(config: PlayerCarConfig): PlayerCarSpriteDefinition {
  var body = makeAttr(config.bodyColor, BG_BLACK);
  var trim = makeAttr(config.trimColor, BG_BLACK);
  var chrome = makeAttr(LIGHTGRAY, BG_BLACK);  // Chrome wheels and trim
  // Brake light: body color on top, red on bottom when ON; solid body when OFF
  var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
  var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
  var U: SpriteCell | null = null;
  
  return {
    name: 'player_classic',
    brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
    variants: [
      [
        [U, { char: GLYPH.UPPER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: body }, U],
        [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: chrome }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
        [{ char: GLYPH.LOWER_HALF, attr: chrome }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: chrome }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: chrome }]
      ]
    ]
  };
}

/**
 * Get a player car sprite by body style and configuration.
 * Caches sprites to avoid recreation each frame.
 */
function getPlayerCarSprite(bodyStyle: CarBodyStyle, colorId: string, brakeLightsOn: boolean): PlayerCarSpriteDefinition {
  // Create cache key
  var key = bodyStyle + '_' + colorId + '_' + (brakeLightsOn ? 'brake' : 'normal');
  
  if (!PLAYER_CAR_SPRITE_CACHE[key]) {
    var color = getCarColor(colorId);
    if (!color) {
      // Fallback to yellow
      color = CAR_COLORS['yellow'];
    }
    
    var config: PlayerCarConfig = {
      bodyColor: color.body,
      trimColor: color.trim,
      brakeLightsOn: brakeLightsOn
    };
    
    switch (bodyStyle) {
      case 'muscle':
        PLAYER_CAR_SPRITE_CACHE[key] = createMuscleCarSprite(config);
        break;
      case 'compact':
        PLAYER_CAR_SPRITE_CACHE[key] = createCompactCarSprite(config);
        break;
      case 'super':
        PLAYER_CAR_SPRITE_CACHE[key] = createSuperCarSprite(config);
        break;
      case 'classic':
        PLAYER_CAR_SPRITE_CACHE[key] = createClassicCarSprite(config);
        break;
      case 'sports':
      default:
        PLAYER_CAR_SPRITE_CACHE[key] = createPlayerSportsCarSprite(config);
        break;
    }
  }
  
  return PLAYER_CAR_SPRITE_CACHE[key];
}

/**
 * Clear the sprite cache (e.g., when changing color settings).
 */
function clearPlayerCarSpriteCache(): void {
  PLAYER_CAR_SPRITE_CACHE = {};
}

/**
 * Create a preview sprite for car selection screen.
 * Returns a larger sprite with more detail for display purposes.
 * Uses the same style as the in-game sprite but could be enhanced.
 */
function createCarPreviewSprite(bodyStyle: CarBodyStyle, colorId: string): PlayerCarSpriteDefinition {
  // For now, just return the normal sprite - we can enhance this later
  // with a larger detailed version if needed
  return getPlayerCarSprite(bodyStyle, colorId, false);
}
