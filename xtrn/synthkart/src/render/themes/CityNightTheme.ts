/**
 * CityNightTheme - Urban nighttime aesthetic.
 * 
 * Features:
 * - Dark blue/black sky with yellow stars
 * - Crescent moon
 * - Skyscraper silhouettes with lit windows
 * - Light gray road with blue curbs
 * - Buildings, lampposts, signs on roadside
 */

var CityNightTheme: Theme = {
  name: 'city_night',
  description: 'Midnight city drive with skyscrapers, stars, and neon lights',
  
  colors: {
    // Sky - deep blue/black night sky
    skyTop: { fg: BLUE, bg: BG_BLACK },
    skyMid: { fg: BLUE, bg: BG_BLACK },
    skyHorizon: { fg: LIGHTBLUE, bg: BG_BLACK },
    
    // Sky grid (subtle or none for city)
    skyGrid: { fg: BLUE, bg: BG_BLACK },
    skyGridGlow: { fg: LIGHTBLUE, bg: BG_BLACK },
    
    // Moon - glowing cyan/white crescent
    celestialCore: { fg: WHITE, bg: BG_CYAN },           // Bright white on cyan
    celestialGlow: { fg: LIGHTCYAN, bg: BG_BLACK },      // Cyan glow
    
    // Stars - white/cyan twinkle
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: CYAN, bg: BG_BLACK },
    
    // Skyscrapers - dark with lit windows
    sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },      // Building walls
    scenerySecondary: { fg: YELLOW, bg: BG_BLACK },      // Lit windows
    sceneryTertiary: { fg: LIGHTRED, bg: BG_BLACK },     // Antenna lights
    
    // Road - light gray asphalt with blue curbs
    roadSurface: { fg: DARKGRAY, bg: BG_LIGHTGRAY },
    roadSurfaceAlt: { fg: BLACK, bg: BG_LIGHTGRAY },
    roadStripe: { fg: YELLOW, bg: BG_LIGHTGRAY },        // Yellow lane markers
    roadEdge: { fg: LIGHTBLUE, bg: BG_BLUE },            // Blue curbs
    roadGrid: { fg: DARKGRAY, bg: BG_LIGHTGRAY },
    
    // Shoulders - concrete sidewalk
    shoulderPrimary: { fg: LIGHTGRAY, bg: BG_BLUE },     // Blue curb
    shoulderSecondary: { fg: DARKGRAY, bg: BG_BLACK },   // Dark beyond
    
    // Roadside object colors
    roadsideColors: {
      'building': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK },   // Windows
        tertiary: { fg: LIGHTCYAN, bg: BG_BLACK }  // Neon signs
      },
      'lamppost': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },   // Pole
        secondary: { fg: YELLOW, bg: BG_BLACK }    // Light
      },
      'sign': {
        primary: { fg: GREEN, bg: BG_BLACK },      // Green highway sign
        secondary: { fg: WHITE, bg: BG_BLACK }     // Text
      },
      'tree': {
        // City trees are darker, more shadowy
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: GREEN, bg: BG_BLACK },
        tertiary: { fg: DARKGRAY, bg: BG_BLACK }   // Dark trunk
      }
    }
  },
  
  // Sky background: stars instead of grid
  sky: {
    type: 'stars',
    converging: false,
    horizontal: false
  },
  
  background: {
    type: 'skyscrapers',
    config: {
      density: 0.8,
      hasWindows: true,
      hasAntennas: true,
      parallaxSpeed: 0.15
    }
  },
  
  celestial: {
    type: 'moon',
    size: 2,
    positionX: 0.85,  // Upper right
    positionY: 0.3    // High in sky (but not too high - horizonY is only 8)
  },
  
  stars: {
    enabled: true,
    density: 0.4,     // More stars
    twinkle: true
  },
  
  roadside: {
    pool: [
      { sprite: 'building', weight: 4, side: 'both' },
      { sprite: 'lamppost', weight: 3, side: 'both' },
      { sprite: 'sign', weight: 2, side: 'right' },   // Signs only on right
      { sprite: 'tree', weight: 1, side: 'both' }     // Occasional city trees
    ],
    spacing: 12,
    density: 1.2
  }
};

// Register the theme
registerTheme(CityNightTheme);