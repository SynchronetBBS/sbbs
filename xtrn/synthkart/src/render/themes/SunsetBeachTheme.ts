// Sunset Beach Theme - Warm tropical sunset vibes
// Golden hour colors with palm trees and ocean backdrop

var SunsetBeachTheme: Theme = {
  name: 'sunset_beach',
  description: 'Tropical sunset with palm trees, warm orange skies, and beach vibes',
  
  colors: {
    // Warm sunset sky gradient
    skyTop: { fg: MAGENTA, bg: BG_BLACK },      // Deep purple at top
    skyMid: { fg: LIGHTMAGENTA, bg: BG_BLACK },  // Pink/magenta middle
    skyHorizon: { fg: LIGHTRED, bg: BG_BLACK },  // Orange near horizon
    
    // Synthwave grid colors (not used much in gradient sky)
    skyGrid: { fg: LIGHTRED, bg: BG_BLACK },
    skyGridGlow: { fg: YELLOW, bg: BG_BLACK },
    
    // Large golden sun
    celestialCore: { fg: YELLOW, bg: BG_BROWN },
    celestialGlow: { fg: BROWN, bg: BG_RED },
    
    // Warm stars (barely visible in sunset)
    starBright: { fg: YELLOW, bg: BG_BLACK },
    starDim: { fg: BROWN, bg: BG_BLACK },
    
    // Ocean/island on horizon
    sceneryPrimary: { fg: MAGENTA, bg: BG_BLACK },      // Distant purple island
    scenerySecondary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    sceneryTertiary: { fg: CYAN, bg: BG_BLACK },        // Ocean waves
    
    // Sandy beach road
    roadSurface: { fg: BROWN, bg: BG_BLACK },
    roadSurfaceAlt: { fg: YELLOW, bg: BG_BLACK },
    roadStripe: { fg: WHITE, bg: BG_BLACK },
    roadEdge: { fg: YELLOW, bg: BG_BLACK },
    roadGrid: { fg: BROWN, bg: BG_BLACK },
    
    // Sandy shoulders
    shoulderPrimary: { fg: YELLOW, bg: BG_BLACK },
    shoulderSecondary: { fg: BROWN, bg: BG_BLACK },
    
    // Roadside object colors (optional overrides)
    roadsideColors: {
      'palm': {
        primary: { fg: LIGHTGREEN, bg: BG_BLACK },  // Fronds
        secondary: { fg: GREEN, bg: BG_BLACK },
        tertiary: { fg: BROWN, bg: BG_BLACK }       // Trunk
      },
      'umbrella': {
        primary: { fg: LIGHTRED, bg: BG_BLACK },    // Striped
        secondary: { fg: YELLOW, bg: BG_BLACK }
      },
      'lifeguard': {
        primary: { fg: LIGHTRED, bg: BG_BLACK },    // Tower
        secondary: { fg: WHITE, bg: BG_BLACK },
        tertiary: { fg: BROWN, bg: BG_BLACK }
      },
      'surfboard': {
        primary: { fg: LIGHTCYAN, bg: BG_BLACK },
        secondary: { fg: CYAN, bg: BG_BLACK }
      },
      'tiki': {
        primary: { fg: YELLOW, bg: BG_BLACK },      // Flame
        secondary: { fg: LIGHTRED, bg: BG_BLACK },
        tertiary: { fg: BROWN, bg: BG_BLACK }       // Pole
      },
      'beachhut': {
        primary: { fg: YELLOW, bg: BG_BLACK },      // Thatch
        secondary: { fg: BROWN, bg: BG_BLACK }
      }
    }
  },
  
  sky: {
    type: 'gradient',
    converging: false,
    horizontal: false
  },
  
  background: {
    type: 'ocean',
    config: {
      count: 3,
      minHeight: 2,
      maxHeight: 4,
      parallaxSpeed: 0.2
    }
  },
  
  celestial: {
    type: 'sun',
    size: 4,            // Large setting sun
    positionX: 0.5,     // Centered
    positionY: 0.75     // Low in sky (setting)
  },
  
  stars: {
    enabled: false,     // No stars during sunset
    density: 0,
    twinkle: false
  },
  
  // Beach sand - warm golden with occasional shells/ripples
  ground: {
    type: 'sand',
    primary: { fg: YELLOW, bg: BG_BROWN },
    secondary: { fg: WHITE, bg: BG_BROWN }
  },
  
  roadside: {
    pool: [
      { sprite: 'palm', weight: 4, side: 'both' },
      { sprite: 'umbrella', weight: 3, side: 'both' },
      { sprite: 'tiki', weight: 2, side: 'both' },
      { sprite: 'surfboard', weight: 2, side: 'both' },
      { sprite: 'lifeguard', weight: 1, side: 'right' },
      { sprite: 'beachhut', weight: 1, side: 'left' }
    ],
    spacing: 55,
    density: 1.3
  }
};

registerTheme(SunsetBeachTheme);
