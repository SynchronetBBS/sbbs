/**
 * TropicalJungleTheme.ts - Dense rainforest racing through exotic wilderness.
 * Lush canopy overhead, exotic birds, vines, and vibrant tropical flowers.
 */

var TropicalJungleTheme: Theme = {
  name: 'tropical_jungle',
  description: 'Race through a dense tropical rainforest teeming with life',
  
  colors: {
    // Jungle sky - patches of blue through canopy
    skyTop: { fg: LIGHTCYAN, bg: BG_BLACK },
    skyMid: { fg: GREEN, bg: BG_BLACK },
    skyHorizon: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // Dappled sunlight through leaves
    skyGrid: { fg: GREEN, bg: BG_BLACK },
    skyGridGlow: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // Bright tropical sun
    celestialCore: { fg: YELLOW, bg: BG_GREEN },
    celestialGlow: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // No stars - daytime jungle
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // Dense forest background
    sceneryPrimary: { fg: GREEN, bg: BG_BLACK },        // Canopy
    scenerySecondary: { fg: LIGHTGREEN, bg: BG_BLACK }, // Highlights
    sceneryTertiary: { fg: BROWN, bg: BG_BLACK },       // Tree trunks
    
    // Dirt jungle road
    roadSurface: { fg: BROWN, bg: BG_BLACK },
    roadSurfaceAlt: { fg: YELLOW, bg: BG_BLACK },
    roadStripe: { fg: YELLOW, bg: BG_BLACK },
    roadEdge: { fg: GREEN, bg: BG_BLACK },
    roadGrid: { fg: BROWN, bg: BG_BLACK },
    
    // Jungle floor - ferns and moss
    shoulderPrimary: { fg: GREEN, bg: BG_BLACK },
    shoulderSecondary: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // Roadside colors - jungle palette
    roadsideColors: {
      'jungle_tree': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK }
      },
      'fern': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: LIGHTGREEN, bg: BG_BLACK }
      },
      'vine': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: LIGHTMAGENTA, bg: BG_BLACK }
      },
      'flower': {
        primary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      },
      'parrot': {
        primary: { fg: LIGHTRED, bg: BG_BLACK },
        secondary: { fg: LIGHTBLUE, bg: BG_BLACK }
      },
      'banana': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      }
    }
  },
  
  // Gradient sky with canopy overlay
  sky: {
    type: 'gradient',
    converging: false,
    horizontal: false
  },
  
  // Dense jungle canopy overhead
  background: {
    type: 'jungle_canopy',
    config: {
      vineCount: 8,
      hangingVines: true,
      parallaxSpeed: 0.08
    }
  },
  
  // Sun peeking through canopy
  celestial: {
    type: 'sun',
    size: 2,
    positionX: 0.4,
    positionY: 0.2
  },
  
  // No stars in daytime
  stars: {
    enabled: false,
    density: 0,
    twinkle: false
  },
  
  // Lush jungle floor with ferns and undergrowth
  ground: {
    type: 'jungle',
    primary: { fg: GREEN, bg: BG_BLACK },
    secondary: { fg: BROWN, bg: BG_BLACK },
    pattern: {
      grassDensity: 0.7,
      grassChars: ['"', 'v', 'V', 'Y', ',']
    }
  },
  
  // Dense jungle scenery
  roadside: {
    pool: [
      { sprite: 'jungle_tree', weight: 5, side: 'both' },
      { sprite: 'fern', weight: 4, side: 'both' },
      { sprite: 'vine', weight: 3, side: 'both' },
      { sprite: 'flower', weight: 3, side: 'both' },
      { sprite: 'parrot', weight: 1, side: 'both' },
      { sprite: 'banana', weight: 2, side: 'both' }
    ],
    spacing: 35,
    density: 1.4
  }
};

registerTheme(TropicalJungleTheme);
