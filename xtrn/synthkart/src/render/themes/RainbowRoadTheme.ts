/**
 * RainbowRoadTheme.ts - Celestial racing through the cosmos.
 * Stars, planets, and cosmic wonders light the way.
 */

var RainbowRoadTheme: Theme = {
  name: 'rainbow_road',
  description: 'Race across a cosmic highway through the stars and galaxies',
  
  colors: {
    // Deep space sky
    skyTop: { fg: BLACK, bg: BG_BLACK },
    skyMid: { fg: BLUE, bg: BG_BLACK },
    skyHorizon: { fg: MAGENTA, bg: BG_BLACK },
    
    // Cosmic grid effect
    skyGrid: { fg: MAGENTA, bg: BG_BLACK },
    skyGridGlow: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    
    // Distant galaxy center
    celestialCore: { fg: WHITE, bg: BG_BLACK },
    celestialGlow: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    
    // Brilliant stars
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: CYAN, bg: BG_BLACK },
    
    // Nebula clouds in background
    sceneryPrimary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    scenerySecondary: { fg: LIGHTCYAN, bg: BG_BLACK },
    sceneryTertiary: { fg: LIGHTBLUE, bg: BG_BLACK },
    
    // Rainbow road surface - cycles through colors
    roadSurface: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    roadSurfaceAlt: { fg: LIGHTCYAN, bg: BG_BLACK },
    roadStripe: { fg: WHITE, bg: BG_BLACK },
    roadEdge: { fg: YELLOW, bg: BG_BLACK },
    roadGrid: { fg: LIGHTBLUE, bg: BG_BLACK },
    
    // Void shoulders
    shoulderPrimary: { fg: BLUE, bg: BG_BLACK },
    shoulderSecondary: { fg: MAGENTA, bg: BG_BLACK },
    
    // Roadside colors - cosmic palette
    roadsideColors: {
      'star': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      },
      'moon': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
      },
      'planet': {
        primary: { fg: LIGHTBLUE, bg: BG_BLACK },
        secondary: { fg: LIGHTCYAN, bg: BG_BLACK }
      },
      'comet': {
        primary: { fg: LIGHTCYAN, bg: BG_BLACK },
        secondary: { fg: WHITE, bg: BG_BLACK }
      },
      'nebula': {
        primary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        secondary: { fg: MAGENTA, bg: BG_BLACK }
      },
      'satellite': {
        primary: { fg: LIGHTGRAY, bg: BG_BLACK },
        secondary: { fg: LIGHTBLUE, bg: BG_BLACK }
      }
    }
  },
  
  // Star-filled sky
  sky: {
    type: 'stars',
    converging: true,
    horizontal: false
  },
  
  // Cosmic nebula clouds
  background: {
    type: 'nebula',
    config: {
      cloudCount: 5,
      parallaxSpeed: 0.05
    }
  },
  
  // Galaxy center
  celestial: {
    type: 'sun',  // Glowing galaxy core
    size: 4,
    positionX: 0.5,
    positionY: 0.4
  },
  
  // Dense starfield
  stars: {
    enabled: true,
    density: 1.0,  // Maximum stars
    twinkle: true
  },
  
  // Cosmic void with rainbow guide lines
  ground: {
    type: 'void',
    primary: { fg: BLUE, bg: BG_BLACK },
    secondary: { fg: WHITE, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.1,
      ditherChars: ['.', '*']
    }
  },
  
  // Cosmic objects along the road
  roadside: {
    pool: [
      { sprite: 'star', weight: 5, side: 'both' },
      { sprite: 'moon', weight: 2, side: 'both' },
      { sprite: 'planet', weight: 3, side: 'both' },
      { sprite: 'comet', weight: 2, side: 'both' },
      { sprite: 'nebula', weight: 3, side: 'both' },
      { sprite: 'satellite', weight: 2, side: 'both' }
    ],
    spacing: 45,
    density: 1.0
  },
  
  // Rainbow road effect - cycles through colors, no edge markers
  road: {
    rainbow: true,
    hideEdgeMarkers: true
  }
};

registerTheme(RainbowRoadTheme);
