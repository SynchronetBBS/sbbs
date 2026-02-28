/**
 * CactusCanyonTheme.ts - Hot desert race through the American Southwest.
 * Blazing sun, saguaro cacti, tumbleweeds, and dusty canyon roads.
 */

var CactusCanyonTheme: Theme = {
  name: 'cactus_canyon',
  description: 'Race through the scorching desert canyons of the Southwest',
  
  colors: {
    // Hot desert sky - deep blue fading to orange/yellow at horizon
    skyTop: { fg: BLUE, bg: BG_BLACK },
    skyMid: { fg: CYAN, bg: BG_BLACK },
    skyHorizon: { fg: YELLOW, bg: BG_BLACK },
    
    // No grid - clear desert sky
    skyGrid: { fg: YELLOW, bg: BG_BLACK },
    skyGridGlow: { fg: BROWN, bg: BG_BLACK },
    
    // Blazing desert sun
    celestialCore: { fg: YELLOW, bg: BG_BROWN },
    celestialGlow: { fg: YELLOW, bg: BG_BLACK },
    
    // No stars during day
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: YELLOW, bg: BG_BLACK },
    
    // Canyon/mesa background
    sceneryPrimary: { fg: BROWN, bg: BG_BLACK },       // Canyon walls
    scenerySecondary: { fg: LIGHTRED, bg: BG_BLACK },  // Red rock highlights
    sceneryTertiary: { fg: YELLOW, bg: BG_BLACK },     // Sunlit tops
    
    // Dusty desert road
    roadSurface: { fg: BROWN, bg: BG_BLACK },          // Dirt road
    roadSurfaceAlt: { fg: YELLOW, bg: BG_BLACK },      // Sandy patches
    roadStripe: { fg: YELLOW, bg: BG_BLACK },          // Faded yellow line
    roadEdge: { fg: BROWN, bg: BG_BLACK },
    roadGrid: { fg: BROWN, bg: BG_BLACK },
    
    // Sandy desert floor
    shoulderPrimary: { fg: YELLOW, bg: BG_BLACK },     // Sand
    shoulderSecondary: { fg: BROWN, bg: BG_BLACK },    // Darker sand/dirt
    
    // Roadside colors - desert palette
    roadsideColors: {
      'saguaro': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: CYAN, bg: BG_BLACK }
      },
      'barrel': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: LIGHTRED, bg: BG_BLACK }      // Flower
      },
      'tumbleweed': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      },
      'cowskull': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
      },
      'desertrock': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      },
      'westernsign': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      }
    }
  },
  
  // Clear gradient sky
  sky: {
    type: 'gradient',
    converging: false,
    horizontal: false
  },
  
  // Desert dunes/mesas
  background: {
    type: 'dunes',
    config: {
      count: 6,
      minHeight: 3,
      maxHeight: 6,
      parallaxSpeed: 0.1
    }
  },
  
  // Blazing hot sun
  celestial: {
    type: 'sun',
    size: 3,              // Big hot sun
    positionX: 0.6,       // Slightly right
    positionY: 0.3        // High in sky
  },
  
  // No stars in daytime
  stars: {
    enabled: false,
    density: 0,
    twinkle: false
  },
  
  // Sandy desert ground
  ground: {
    type: 'sand',
    primary: { fg: YELLOW, bg: BG_BLACK },
    secondary: { fg: BROWN, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.35,
      ditherChars: ['.', ',', "'", GLYPH.LIGHT_SHADE, '~']
    }
  },
  
  // Desert roadside scenery
  roadside: {
    pool: [
      { sprite: 'saguaro', weight: 4, side: 'both' },     // Iconic saguaro cacti
      { sprite: 'barrel', weight: 3, side: 'both' },      // Barrel cacti
      { sprite: 'tumbleweed', weight: 2, side: 'both' },  // Rolling tumbleweeds
      { sprite: 'cowskull', weight: 1, side: 'both' },    // Bleached skulls
      { sprite: 'desertrock', weight: 3, side: 'both' },  // Desert boulders
      { sprite: 'westernsign', weight: 1, side: 'both' }  // Old west signs
    ],
    spacing: 50,
    density: 0.9          // Sparse desert landscape
  }
};

registerTheme(CactusCanyonTheme);
