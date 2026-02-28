// Twilight Forest Theme - Enchanted forest at dusk
// Deep greens, earthy browns, dual moons rising, fireflies in the air

var TwilightForestTheme: Theme = {
  name: 'twilight_forest',
  description: 'Enchanted forest at twilight with dual moons and dancing fireflies',
  
  colors: {
    // Dusky twilight sky - deep blue fading to orange at horizon
    skyTop: { fg: BLUE, bg: BG_BLACK },
    skyMid: { fg: CYAN, bg: BG_BLACK },
    skyHorizon: { fg: BROWN, bg: BG_BLACK },  // Warm sunset glow through trees
    
    // No grid in forest sky
    skyGrid: { fg: BLUE, bg: BG_BLACK },
    skyGridGlow: { fg: CYAN, bg: BG_BLACK },
    
    // Dual moons - one pale white, accents in cyan
    celestialCore: { fg: WHITE, bg: BG_LIGHTGRAY },
    celestialGlow: { fg: LIGHTCYAN, bg: BG_BLACK },
    
    // Firefly stars - yellow/green dancing lights
    starBright: { fg: YELLOW, bg: BG_BLACK },
    starDim: { fg: LIGHTGREEN, bg: BG_BLACK },
    
    // Forest tree line on horizon
    sceneryPrimary: { fg: GREEN, bg: BG_BLACK },       // Dark forest silhouette
    scenerySecondary: { fg: LIGHTGREEN, bg: BG_BLACK }, // Highlighted treetops
    sceneryTertiary: { fg: BROWN, bg: BG_BLACK },       // Tree trunks
    
    // Dirt/gravel forest road
    roadSurface: { fg: BROWN, bg: BG_BLACK },
    roadSurfaceAlt: { fg: DARKGRAY, bg: BG_BLACK },
    roadStripe: { fg: YELLOW, bg: BG_BLACK },      // Faded yellow line
    roadEdge: { fg: BROWN, bg: BG_BLACK },
    roadGrid: { fg: DARKGRAY, bg: BG_BLACK },
    
    // Forest floor - mossy earth
    shoulderPrimary: { fg: GREEN, bg: BG_BLACK },     // Mossy edge
    shoulderSecondary: { fg: BROWN, bg: BG_BLACK },   // Dirt/leaves
    
    // Roadside colors - deep forest palette
    roadsideColors: {
      'tree': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: LIGHTGREEN, bg: BG_BLACK },
        tertiary: { fg: BROWN, bg: BG_BLACK }
      },
      'rock': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
      },
      'bush': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: LIGHTGREEN, bg: BG_BLACK }
      }
    }
  },
  
  // Firefly-filled sky (uses stars renderer with firefly colors)
  sky: {
    type: 'stars',
    converging: false,
    horizontal: false
  },
  
  // Forest tree line background
  background: {
    type: 'forest',
    config: {
      count: 8,
      minHeight: 3,
      maxHeight: 7,
      parallaxSpeed: 0.15
    }
  },
  
  // Dual moons - mystical/fantasy vibe
  celestial: {
    type: 'dual_moons',
    size: 3,
    positionX: 0.7,     // Main moon right of center
    positionY: 0.3      // Higher in sky
  },
  
  // Fireflies - more visible and animated than regular stars
  stars: {
    enabled: true,
    density: 0.6,       // Moderate density - not overwhelming
    twinkle: true       // Animated twinkling fireflies
  },
  
  // Forest floor - grass and dirt pattern
  ground: {
    type: 'grass',
    primary: { fg: GREEN, bg: BG_BLACK },
    secondary: { fg: BROWN, bg: BG_BLACK },
    pattern: {
      grassDensity: 0.4,
      grassChars: ['"', 'v', ',', "'", '.']
    }
  },
  
  // Dense forest - lots of trees, some rocks
  roadside: {
    pool: [
      { sprite: 'tree', weight: 5, side: 'both' },   // Heavy tree presence
      { sprite: 'bush', weight: 3, side: 'both' },   // Underbrush
      { sprite: 'rock', weight: 1, side: 'both' }    // Occasional boulders
    ],
    spacing: 45,
    density: 1.4        // Dense forest
  }
};

registerTheme(TwilightForestTheme);
