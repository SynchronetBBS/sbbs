/**
 * HauntedHollowTheme.ts - Creepy graveyard racing through the night.
 * Blood moon overhead, gravestones, dead trees, jack-o-lanterns, and eerie fog.
 */

var HauntedHollowTheme: Theme = {
  name: 'haunted_hollow',
  description: 'Race through a haunted cemetery under a blood moon',
  
  colors: {
    // Dark night sky with eerie purple-red glow at horizon
    skyTop: { fg: BLACK, bg: BG_BLACK },
    skyMid: { fg: DARKGRAY, bg: BG_BLACK },
    skyHorizon: { fg: MAGENTA, bg: BG_BLACK },
    
    // No grid in horror sky - just darkness
    skyGrid: { fg: DARKGRAY, bg: BG_BLACK },
    skyGridGlow: { fg: MAGENTA, bg: BG_BLACK },
    
    // Blood moon!
    celestialCore: { fg: RED, bg: BG_BLACK },
    celestialGlow: { fg: LIGHTRED, bg: BG_BLACK },
    
    // Sparse, dim stars
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: DARKGRAY, bg: BG_BLACK },
    
    // Cemetery silhouette background
    sceneryPrimary: { fg: BLACK, bg: BG_BLACK },        // Dark silhouettes
    scenerySecondary: { fg: DARKGRAY, bg: BG_BLACK },   // Slight detail
    sceneryTertiary: { fg: MAGENTA, bg: BG_BLACK },     // Eerie glow accents
    
    // Cracked, old road
    roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
    roadSurfaceAlt: { fg: BLACK, bg: BG_BLACK },
    roadStripe: { fg: LIGHTRED, bg: BG_BLACK },    // Blood-red center line
    roadEdge: { fg: DARKGRAY, bg: BG_BLACK },
    roadGrid: { fg: DARKGRAY, bg: BG_BLACK },
    
    // Graveyard dirt/grass
    shoulderPrimary: { fg: DARKGRAY, bg: BG_BLACK },    // Dark earth
    shoulderSecondary: { fg: GREEN, bg: BG_BLACK },     // Dead grass patches
    
    // Item box colors - spooky green/purple
    itemBox: {
      border: { fg: LIGHTGREEN, bg: BG_BLACK },
      fill: { fg: GREEN, bg: BG_BLACK },
      symbol: { fg: LIGHTMAGENTA, bg: BG_BLACK }
    },
    
    // Roadside colors - spooky palette
    roadsideColors: {
      'deadtree': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK }
      },
      'gravestone': {
        primary: { fg: LIGHTGRAY, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK }
      },
      'pumpkin': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }      // Glowing face
      },
      'skull': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK }       // Stake
      },
      'fence': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
      },
      'candle': {
        primary: { fg: YELLOW, bg: BG_BLACK },       // Flame
        secondary: { fg: BROWN, bg: BG_BLACK }       // Holder
      }
    }
  },
  
  // Dark starry sky (sparse stars)
  sky: {
    type: 'stars',
    converging: false,
    horizontal: false
  },
  
  // Cemetery hills/forest silhouette
  background: {
    type: 'hills',
    config: {
      count: 6,
      minHeight: 2,
      maxHeight: 5,
      parallaxSpeed: 0.1
    }
  },
  
  // Blood moon - large and ominous
  celestial: {
    type: 'moon',
    size: 4,              // Large blood moon
    positionX: 0.5,       // Centered for maximum creepy
    positionY: 0.25       // Higher in sky
  },
  
  // Sparse, dim stars through fog
  stars: {
    enabled: true,
    density: 0.3,         // Sparse - foggy night
    twinkle: true         // Eerie twinkling
  },
  
  // Graveyard dirt - dithered dark ground
  ground: {
    type: 'dither',
    primary: { fg: DARKGRAY, bg: BG_BLACK },
    secondary: { fg: GREEN, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.4,
      ditherChars: ['.', ',', "'", GLYPH.LIGHT_SHADE]
    }
  },
  
  // Dense graveyard scenery
  roadside: {
    pool: [
      { sprite: 'deadtree', weight: 4, side: 'both' },    // Gnarled trees
      { sprite: 'gravestone', weight: 5, side: 'both' },  // Tombstones everywhere
      { sprite: 'pumpkin', weight: 2, side: 'both' },     // Jack-o-lanterns
      { sprite: 'skull', weight: 1, side: 'both' },       // Occasional skulls
      { sprite: 'fence', weight: 3, side: 'both' },       // Cemetery fence
      { sprite: 'candle', weight: 2, side: 'both' }       // Eerie candles
    ],
    spacing: 40,
    density: 1.3          // Dense graveyard feel
  }
};

registerTheme(HauntedHollowTheme);
