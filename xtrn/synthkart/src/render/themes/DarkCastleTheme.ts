/**
 * DarkCastleTheme.ts - Medieval fortress racing at night.
 * Stone towers, flickering torches, and ancient battlements.
 */

var DarkCastleTheme: Theme = {
  name: 'dark_castle',
  description: 'Race through the moonlit grounds of an ancient fortress',
  
  colors: {
    // Dark stormy sky
    skyTop: { fg: BLACK, bg: BG_BLACK },
    skyMid: { fg: DARKGRAY, bg: BG_BLACK },
    skyHorizon: { fg: BLUE, bg: BG_BLACK },
    
    // Misty grid
    skyGrid: { fg: DARKGRAY, bg: BG_BLACK },
    skyGridGlow: { fg: LIGHTGRAY, bg: BG_BLACK },
    
    // Full moon
    celestialCore: { fg: WHITE, bg: BG_BLACK },
    celestialGlow: { fg: LIGHTGRAY, bg: BG_BLACK },
    
    // Pale stars through clouds
    starBright: { fg: LIGHTGRAY, bg: BG_BLACK },
    starDim: { fg: DARKGRAY, bg: BG_BLACK },
    
    // Castle silhouettes in background
    sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
    scenerySecondary: { fg: BLACK, bg: BG_BLACK },
    sceneryTertiary: { fg: BLUE, bg: BG_BLACK },
    
    // Cobblestone road
    roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
    roadSurfaceAlt: { fg: BLACK, bg: BG_BLACK },
    roadStripe: { fg: LIGHTGRAY, bg: BG_BLACK },
    roadEdge: { fg: BROWN, bg: BG_BLACK },
    roadGrid: { fg: DARKGRAY, bg: BG_BLACK },
    
    // Stone-bordered shoulders
    shoulderPrimary: { fg: DARKGRAY, bg: BG_BLACK },
    shoulderSecondary: { fg: BROWN, bg: BG_BLACK },
    
    // Roadside colors - medieval stone palette
    roadsideColors: {
      'tower': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }  // Lit windows
      },
      'battlement': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: BLACK, bg: BG_BLACK }
      },
      'torch': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }  // Flame
      },
      'banner': {
        primary: { fg: LIGHTRED, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }  // Crest
      },
      'gargoyle': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
      },
      'portcullis': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK }
      }
    }
  },
  
  // Cloudy night sky
  sky: {
    type: 'gradient',
    converging: false,
    horizontal: false
  },
  
  // Castle fortress silhouette with towers and walls
  background: {
    type: 'castle_fortress',
    config: {
      towerCount: 5,
      hasTorches: true,
      parallaxSpeed: 0.08
    }
  },
  
  // Full moon - positioned in gap between castle towers
  celestial: {
    type: 'moon',
    size: 3,
    positionX: 0.45,
    positionY: 0.25
  },
  
  // Few stars visible through clouds
  stars: {
    enabled: true,
    density: 0.25,
    twinkle: true
  },
  
  // Medieval cobblestone ground
  ground: {
    type: 'cobblestone',
    primary: { fg: DARKGRAY, bg: BG_BLACK },
    secondary: { fg: BLACK, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.6,
      ditherChars: ['#', '+', GLYPH.DARK_SHADE]
    }
  },
  
  // Castle structures along the road
  roadside: {
    pool: [
      { sprite: 'tower', weight: 3, side: 'both' },
      { sprite: 'battlement', weight: 4, side: 'both' },
      { sprite: 'torch', weight: 5, side: 'both' },  // Frequent for lighting
      { sprite: 'banner', weight: 3, side: 'both' },
      { sprite: 'gargoyle', weight: 2, side: 'both' },
      { sprite: 'portcullis', weight: 2, side: 'both' }
    ],
    spacing: 35,
    density: 1.0
  }
};

registerTheme(DarkCastleTheme);
