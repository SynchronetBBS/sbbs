/**
 * CandyLandTheme.ts - Sweet fantasy racing through a world of candy.
 * Pastel skies, candy cane forests, and sugary treats everywhere.
 * Cotton candy aesthetic with fluffy pastels, whites, and soft pinks.
 */

var CandyLandTheme: Theme = {
  name: 'candy_land',
  description: 'Race through a magical world made entirely of sweets and candy',
  
  colors: {
    // Cotton candy sky - fluffy pastels
    skyTop: { fg: LIGHTCYAN, bg: BG_MAGENTA },
    skyMid: { fg: WHITE, bg: BG_MAGENTA },
    skyHorizon: { fg: LIGHTMAGENTA, bg: BG_CYAN },
    
    // Sparkly candy atmosphere
    skyGrid: { fg: WHITE, bg: BG_MAGENTA },
    skyGridGlow: { fg: LIGHTCYAN, bg: BG_MAGENTA },
    
    // Gumdrop sun
    celestialCore: { fg: YELLOW, bg: BG_MAGENTA },
    celestialGlow: { fg: WHITE, bg: BG_MAGENTA },
    
    // Sparkle stars
    starBright: { fg: WHITE, bg: BG_MAGENTA },
    starDim: { fg: LIGHTCYAN, bg: BG_MAGENTA },
    
    // Candy mountain background - soft pastels
    sceneryPrimary: { fg: WHITE, bg: BG_MAGENTA },
    scenerySecondary: { fg: LIGHTCYAN, bg: BG_MAGENTA },
    sceneryTertiary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
    
    // Magenta frosting road
    roadSurface: { fg: WHITE, bg: BG_MAGENTA },
    roadSurfaceAlt: { fg: LIGHTMAGENTA, bg: BG_MAGENTA },
    roadStripe: { fg: LIGHTCYAN, bg: BG_MAGENTA },
    roadEdge: { fg: WHITE, bg: BG_CYAN },
    roadGrid: { fg: LIGHTMAGENTA, bg: BG_MAGENTA },
    
    // Cotton candy road shoulders - pink and white
    shoulderPrimary: { fg: WHITE, bg: BG_MAGENTA },
    shoulderSecondary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
    
    // Item box colors - rainbow candy colors
    itemBox: {
      border: { fg: LIGHTRED, bg: BG_CYAN },
      fill: { fg: LIGHTMAGENTA, bg: BG_CYAN },
      symbol: { fg: WHITE, bg: BG_CYAN }
    },
    
    // Roadside colors - candy palette with pastel backgrounds
    roadsideColors: {
      'lollipop': {
        primary: { fg: LIGHTRED, bg: BG_CYAN },
        secondary: { fg: WHITE, bg: BG_CYAN }
      },
      'candy_cane': {
        primary: { fg: LIGHTRED, bg: BG_CYAN },
        secondary: { fg: WHITE, bg: BG_CYAN }
      },
      'gummy_bear': {
        primary: { fg: LIGHTGREEN, bg: BG_CYAN },
        secondary: { fg: GREEN, bg: BG_CYAN }
      },
      'cupcake': {
        primary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
        secondary: { fg: LIGHTRED, bg: BG_CYAN }
      },
      'ice_cream': {
        primary: { fg: WHITE, bg: BG_CYAN },
        secondary: { fg: BROWN, bg: BG_CYAN }
      },
      'cotton_candy': {
        primary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
        secondary: { fg: WHITE, bg: BG_CYAN }
      }
    }
  },
  
  // Sparkly gradient sky
  sky: {
    type: 'stars',
    converging: false,
    horizontal: false
  },
  
  // Whimsical candy hills - rounded and colorful
  background: {
    type: 'candy_hills',
    config: {
      swirls: true,
      parallaxSpeed: 0.12
    }
  },
  
  // Bright candy sun
  celestial: {
    type: 'sun',
    size: 3,
    positionX: 0.5,
    positionY: 0.3
  },
  
  // Sparkle stars (even during day for magic)
  stars: {
    enabled: true,
    density: 0.4,
    twinkle: true
  },
  
  // Fluffy cotton candy ground - pastels with sprinkles
  ground: {
    type: 'candy',
    primary: { fg: WHITE, bg: BG_CYAN },
    secondary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
    pattern: {
      ditherDensity: 0.4,
      ditherChars: ['*', '@', '.', '~']
    }
  },
  
  // Candy-filled roadside
  roadside: {
    pool: [
      { sprite: 'lollipop', weight: 4, side: 'both' },
      { sprite: 'candy_cane', weight: 4, side: 'both' },
      { sprite: 'gummy_bear', weight: 2, side: 'both' },
      { sprite: 'cupcake', weight: 3, side: 'both' },
      { sprite: 'ice_cream', weight: 2, side: 'both' },
      { sprite: 'cotton_candy', weight: 3, side: 'both' }
    ],
    spacing: 40,
    density: 1.2
  }
};

registerTheme(CandyLandTheme);
