/**
 * WinterWonderlandTheme.ts - Magical snowy race through a frosty forest.
 * Crisp blue sky, snow-covered pines, snowmen, and sparkling ice.
 */

var WinterWonderlandTheme: Theme = {
  name: 'winter_wonderland',
  description: 'Race through a magical snowy forest with sparkling ice',
  
  colors: {
    // Crisp winter sky - pale blue fading to white at horizon
    skyTop: { fg: BLUE, bg: BG_BLACK },
    skyMid: { fg: LIGHTCYAN, bg: BG_BLACK },
    skyHorizon: { fg: WHITE, bg: BG_BLACK },
    
    // No grid - clean winter sky
    skyGrid: { fg: LIGHTCYAN, bg: BG_BLACK },
    skyGridGlow: { fg: WHITE, bg: BG_BLACK },
    
    // Pale winter sun
    celestialCore: { fg: WHITE, bg: BG_LIGHTGRAY },
    celestialGlow: { fg: YELLOW, bg: BG_BLACK },
    
    // Sparse daytime stars (snowflakes!)
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: LIGHTCYAN, bg: BG_BLACK },
    
    // Snow-capped mountain background
    sceneryPrimary: { fg: WHITE, bg: BG_BLACK },        // Snow caps
    scenerySecondary: { fg: LIGHTCYAN, bg: BG_BLACK },  // Ice blue shadows
    sceneryTertiary: { fg: LIGHTGRAY, bg: BG_BLACK },   // Rock showing through
    
    // Snowy/icy road
    roadSurface: { fg: LIGHTGRAY, bg: BG_BLACK },       // Packed snow
    roadSurfaceAlt: { fg: WHITE, bg: BG_BLACK },        // Fresh snow
    roadStripe: { fg: LIGHTRED, bg: BG_BLACK },         // Marker poles
    roadEdge: { fg: WHITE, bg: BG_BLACK },
    roadGrid: { fg: LIGHTCYAN, bg: BG_BLACK },
    
    // Snow banks
    shoulderPrimary: { fg: WHITE, bg: BG_BLACK },       // Pure snow
    shoulderSecondary: { fg: LIGHTCYAN, bg: BG_BLACK }, // Ice blue shadows
    
    // Roadside colors - winter palette
    roadsideColors: {
      'snowpine': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: WHITE, bg: BG_BLACK }         // Snow on branches
      },
      'snowman': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: LIGHTRED, bg: BG_BLACK }      // Scarf
      },
      'icecrystal': {
        primary: { fg: LIGHTCYAN, bg: BG_BLACK },
        secondary: { fg: WHITE, bg: BG_BLACK }         // Sparkle
      },
      'candycane': {
        primary: { fg: LIGHTRED, bg: BG_BLACK },
        secondary: { fg: WHITE, bg: BG_BLACK }
      },
      'snowdrift': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
      },
      'signpost': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: WHITE, bg: BG_BLACK }
      }
    }
  },
  
  // Falling snow effect (using stars renderer)
  sky: {
    type: 'stars',
    converging: false,
    horizontal: false
  },
  
  // Snow-capped mountains
  background: {
    type: 'mountains',
    config: {
      count: 5,
      minHeight: 4,
      maxHeight: 8,
      parallaxSpeed: 0.12
    }
  },
  
  // Pale winter sun
  celestial: {
    type: 'sun',
    size: 2,              // Smaller winter sun
    positionX: 0.3,       // Off to the side
    positionY: 0.35       // Lower in winter sky
  },
  
  // "Stars" are actually falling snowflakes
  stars: {
    enabled: true,
    density: 0.5,         // Moderate snowfall
    twinkle: true         // Sparkling snow
  },
  
  // Snowy ground with drifts
  ground: {
    type: 'dither',
    primary: { fg: WHITE, bg: BG_BLACK },
    secondary: { fg: LIGHTCYAN, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.3,
      ditherChars: ['.', '*', GLYPH.LIGHT_SHADE, "'"]
    }
  },
  
  // Winter roadside scenery
  roadside: {
    pool: [
      { sprite: 'snowpine', weight: 5, side: 'both' },    // Snow-covered pines
      { sprite: 'snowman', weight: 2, side: 'both' },     // Friendly snowmen
      { sprite: 'icecrystal', weight: 2, side: 'both' },  // Sparkling ice
      { sprite: 'candycane', weight: 1, side: 'both' },   // Festive candy canes
      { sprite: 'snowdrift', weight: 3, side: 'both' },   // Snow piles
      { sprite: 'signpost', weight: 1, side: 'both' }     // Ski signs
    ],
    spacing: 45,
    density: 1.2          // Nice winter forest density
  }
};

registerTheme(WinterWonderlandTheme);
