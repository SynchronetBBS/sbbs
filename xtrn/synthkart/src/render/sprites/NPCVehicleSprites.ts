/**
 * NPCVehicleSprites - Sprites for NPC vehicles (commuters and racers).
 * 
 * Provides variety of vehicle types:
 * - Sedan: Standard car (similar to player but different shape)
 * - Truck: Larger, boxier vehicle
 * - SportsCar: Sleek, low profile
 * 
 * Each sprite type has color variants.
 */

/**
 * NPC vehicle types for spawn variety.
 */
var NPC_VEHICLE_TYPES = ['sedan', 'truck', 'sportscar'];

/**
 * Color palettes for NPC vehicles.
 * Each is { body: color, trim: color }
 */
var NPC_VEHICLE_COLORS = [
  { body: RED, trim: LIGHTRED, name: 'red' },
  { body: BLUE, trim: LIGHTBLUE, name: 'blue' },
  { body: GREEN, trim: LIGHTGREEN, name: 'green' },
  { body: CYAN, trim: LIGHTCYAN, name: 'cyan' },
  { body: MAGENTA, trim: LIGHTMAGENTA, name: 'magenta' },
  { body: WHITE, trim: LIGHTGRAY, name: 'white' },
  { body: BROWN, trim: YELLOW, name: 'orange' }
];

/**
 * Create a sedan sprite - standard passenger car.
 * Rounded roof, medium size - looks like a car from behind.
 */
function createSedanSprite(bodyColor: number, trimColor: number): SpriteDefinition {
  var body = makeAttr(bodyColor, BG_BLACK);
  var trim = makeAttr(trimColor, BG_BLACK);
  var wheel = makeAttr(DARKGRAY, BG_BLACK);
  var window = makeAttr(CYAN, BG_BLACK);       // Darker window
  var taillight = makeAttr(LIGHTRED, BG_BLACK); // Taillights
  var U: SpriteCell | null = null;
  
  return {
    name: 'sedan',
    variants: [
      // Scale 0: dot (far horizon) - 1x1 single pixel
      [
        [{ char: GLYPH.LOWER_HALF, attr: taillight }]
      ],
      // Scale 1: tiny - 3x1 with taillights visible
      [
        [{ char: GLYPH.LOWER_HALF, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: taillight }]
      ],
      // Scale 2: small - 3x2, car shape with window and taillights
      [
        [{ char: GLYPH.UPPER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: body }],
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
      ],
      // Scale 3: medium - 5x2, clear car silhouette
      [
        [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
      ],
      // Scale 4: large (close) - 5x3, detailed car from behind
      [
        [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }],
        [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
      ]
    ]
  };
}

/**
 * Create a truck sprite - larger, boxier vehicle.
 * Taller profile - like a pickup or SUV from behind.
 */
function createTruckSprite(bodyColor: number, trimColor: number): SpriteDefinition {
  var body = makeAttr(bodyColor, BG_BLACK);
  var trim = makeAttr(trimColor, BG_BLACK);
  var wheel = makeAttr(DARKGRAY, BG_BLACK);
  var window = makeAttr(CYAN, BG_BLACK);
  var taillight = makeAttr(LIGHTRED, BG_BLACK);
  var U: SpriteCell | null = null;
  
  return {
    name: 'truck',
    variants: [
      // Scale 0: dot (far horizon) - 1x1 single pixel
      [
        [{ char: GLYPH.LOWER_HALF, attr: taillight }]
      ],
      // Scale 1: tiny - 3x1 with taillights
      [
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
      ],
      // Scale 2: small - 3x2 with window row
      [
        [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: body }],
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
      ],
      // Scale 3: medium - 5x3
      [
        [U, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, U],
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }],
        [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
      ],
      // Scale 4: large (close) - 5x4
      [
        [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
        [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: body }],
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }],
        [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
      ]
    ]
  };
}

/**
 * Create a sports car sprite - sleek, low profile.
 * Wide and low - like a sports car from behind.
 */
function createSportsCarSprite(bodyColor: number, trimColor: number): SpriteDefinition {
  var body = makeAttr(bodyColor, BG_BLACK);
  var trim = makeAttr(trimColor, BG_BLACK);
  var window = makeAttr(CYAN, BG_BLACK);
  var taillight = makeAttr(LIGHTRED, BG_BLACK);
  var U: SpriteCell | null = null;
  
  return {
    name: 'sportscar',
    variants: [
      // Scale 0: dot (far horizon) - 1x1 single pixel
      [
        [{ char: GLYPH.LOWER_HALF, attr: taillight }]
      ],
      // Scale 1: tiny - 3x1 with taillights (sports cars are wide)
      [
        [{ char: GLYPH.LOWER_HALF, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: taillight }]
      ],
      // Scale 2: small - 4x1 (low profile with taillights)
      [
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
      ],
      // Scale 3: medium - 5x2 (wide and low)
      [
        [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
      ],
      // Scale 4: large (close) - 5x2 (sports cars are low but wide)
      [
        [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
        [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
      ]
    ]
  };
}

/**
 * NPC sprite registry - caches sprites by type and color.
 */
var NPC_SPRITE_CACHE: { [key: string]: SpriteDefinition } = {};

/**
 * Get an NPC vehicle sprite by type and color index.
 */
function getNPCSprite(vehicleType: string, colorIndex: number): SpriteDefinition {
  var color = NPC_VEHICLE_COLORS[colorIndex % NPC_VEHICLE_COLORS.length];
  var key = vehicleType + '_' + color.name;
  
  if (!NPC_SPRITE_CACHE[key]) {
    switch (vehicleType) {
      case 'sedan':
        NPC_SPRITE_CACHE[key] = createSedanSprite(color.body, color.trim);
        break;
      case 'truck':
        NPC_SPRITE_CACHE[key] = createTruckSprite(color.body, color.trim);
        break;
      case 'sportscar':
        NPC_SPRITE_CACHE[key] = createSportsCarSprite(color.body, color.trim);
        break;
      default:
        // Default to sedan
        NPC_SPRITE_CACHE[key] = createSedanSprite(color.body, color.trim);
    }
  }
  
  return NPC_SPRITE_CACHE[key];
}

/**
 * Get a random NPC sprite (random type and color).
 */
function getRandomNPCSprite(): { sprite: SpriteDefinition; type: string; colorIndex: number } {
  var typeIndex = Math.floor(Math.random() * NPC_VEHICLE_TYPES.length);
  var colorIndex = Math.floor(Math.random() * NPC_VEHICLE_COLORS.length);
  var vehicleType = NPC_VEHICLE_TYPES[typeIndex];
  
  return {
    sprite: getNPCSprite(vehicleType, colorIndex),
    type: vehicleType,
    colorIndex: colorIndex
  };
}

