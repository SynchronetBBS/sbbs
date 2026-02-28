/**
 * SynthwaveTheme - The classic OutRun synthwave aesthetic.
 * 
 * Features:
 * - Magenta/cyan sky grid
 * - Purple mountains
 * - Red/yellow sun
 * - Cyan/blue holodeck road
 * - Neon pillars, grid pylons, holo billboards, neon palms
 */

var SynthwaveTheme: Theme = {
  name: 'synthwave',
  description: 'Classic 80s synthwave with magenta sky, purple mountains, and setting sun',
  
  colors: {
    // Sky - magenta grid on black
    skyTop: { fg: MAGENTA, bg: BG_BLACK },
    skyMid: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    skyHorizon: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    
    // Sky grid (synthwave signature look)
    skyGrid: { fg: MAGENTA, bg: BG_BLACK },
    skyGridGlow: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    
    // Sun - orange/red sunset
    celestialCore: { fg: YELLOW, bg: BG_RED },
    celestialGlow: { fg: LIGHTRED, bg: BG_BLACK },
    
    // Stars (not prominent in synthwave but available)
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: LIGHTGRAY, bg: BG_BLACK },
    
    // Mountains - purple silhouettes
    sceneryPrimary: { fg: MAGENTA, bg: BG_BLACK },
    scenerySecondary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    sceneryTertiary: { fg: WHITE, bg: BG_BLACK },  // Snow caps
    
    // Road - cyan holodeck surface with blue edges
    roadSurface: { fg: LIGHTCYAN, bg: BG_CYAN },
    roadSurfaceAlt: { fg: CYAN, bg: BG_CYAN },
    roadStripe: { fg: WHITE, bg: BG_CYAN },
    roadEdge: { fg: LIGHTMAGENTA, bg: BG_BLUE },
    roadGrid: { fg: WHITE, bg: BG_CYAN },
    
    // Shoulders - blue holodeck grid extending outward
    shoulderPrimary: { fg: BLUE, bg: BG_BLACK },
    shoulderSecondary: { fg: MAGENTA, bg: BG_BLACK },
    
    // Item box colors - neon magenta/cyan
    itemBox: {
      border: { fg: LIGHTMAGENTA, bg: BG_BLACK },
      fill: { fg: MAGENTA, bg: BG_BLACK },
      symbol: { fg: WHITE, bg: BG_BLACK }
    },
    
    // Roadside object colors - neon palette
    roadsideColors: {
      'neon_pillar': {
        primary: { fg: LIGHTCYAN, bg: BG_BLACK },
        secondary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        tertiary: { fg: BLUE, bg: BG_BLACK }
      },
      'grid_pylon': {
        primary: { fg: MAGENTA, bg: BG_BLACK },
        secondary: { fg: CYAN, bg: BG_BLACK }
      },
      'holo_billboard': {
        primary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        secondary: { fg: LIGHTCYAN, bg: BG_BLACK },
        tertiary: { fg: BLUE, bg: BG_BLACK }
      },
      'neon_palm': {
        primary: { fg: MAGENTA, bg: BG_BLACK },
        secondary: { fg: CYAN, bg: BG_BLACK },
        tertiary: { fg: LIGHTCYAN, bg: BG_BLACK }
      },
      'laser_beam': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: LIGHTCYAN, bg: BG_BLACK }
      }
    }
  },
  
  // Sky background: synthwave grid (converging lines + horizontal scanlines)
  sky: {
    type: 'grid',
    converging: true,
    horizontal: true
  },
  
  background: {
    type: 'mountains',
    config: {
      count: 5,
      minHeight: 3,
      maxHeight: 6,
      parallaxSpeed: 0.1
    }
  },
  
  celestial: {
    type: 'sun',
    size: 3,
    positionX: 0.5,  // centered
    positionY: 0.6   // low in sky, near horizon
  },
  
  stars: {
    enabled: false,
    density: 0,
    twinkle: false
  },
  
  // Holodeck grid extending from road edges to horizon
  ground: {
    type: 'grid',
    primary: { fg: MAGENTA, bg: BG_BLACK },    // Grid line color
    secondary: { fg: CYAN, bg: BG_BLACK },     // Alternate/glow
    pattern: {
      gridSpacing: 15,      // World units between horizontal lines
      radialLines: 10       // Radial lines per side (evenly spaced by angle)
    }
  },
  
  roadside: {
    pool: [
      { sprite: 'neon_pillar', weight: 3, side: 'both' },
      { sprite: 'grid_pylon', weight: 3, side: 'both' },
      { sprite: 'holo_billboard', weight: 2, side: 'both' },
      { sprite: 'neon_palm', weight: 2, side: 'both' },
      { sprite: 'laser_beam', weight: 1, side: 'both' }
    ],
    spacing: 10,
    density: 1.0
  }
};

// Register the theme
registerTheme(SynthwaveTheme);
