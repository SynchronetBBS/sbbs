/**
 * AncientRuinsTheme.ts - Racing through Egyptian desert ruins.
 * Pyramids, temples, sphinxes, and hieroglyphics.
 */

var AncientRuinsTheme: Theme = {
  name: 'ancient_ruins',
  description: 'Discover the secrets of ancient Egypt at high speed',
  
  colors: {
    // Desert sunset sky
    skyTop: { fg: CYAN, bg: BG_BLACK },
    skyMid: { fg: YELLOW, bg: BG_BLACK },
    skyHorizon: { fg: WHITE, bg: BG_BLACK },
    
    // Heat shimmer
    skyGrid: { fg: YELLOW, bg: BG_BLACK },
    skyGridGlow: { fg: WHITE, bg: BG_BLACK },
    
    // Blazing sun
    celestialCore: { fg: WHITE, bg: BG_BROWN },
    celestialGlow: { fg: YELLOW, bg: BG_BLACK },
    
    // Faint daytime stars
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: YELLOW, bg: BG_BLACK },
    
    // Pyramid silhouettes
    sceneryPrimary: { fg: BROWN, bg: BG_BLACK },
    scenerySecondary: { fg: YELLOW, bg: BG_BLACK },
    sceneryTertiary: { fg: WHITE, bg: BG_BLACK },
    
    // Sandy road
    roadSurface: { fg: BROWN, bg: BG_BLACK },
    roadSurfaceAlt: { fg: YELLOW, bg: BG_BLACK },
    roadStripe: { fg: WHITE, bg: BG_BLACK },
    roadEdge: { fg: LIGHTGRAY, bg: BG_BLACK },  // Stone edge
    roadGrid: { fg: YELLOW, bg: BG_BLACK },
    
    // Sand shoulders
    shoulderPrimary: { fg: YELLOW, bg: BG_BLACK },
    shoulderSecondary: { fg: BROWN, bg: BG_BLACK },
    
    // Roadside colors - ancient Egyptian palette
    roadsideColors: {
      'column': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      },
      'statue': {
        primary: { fg: YELLOW, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK }
      },
      'obelisk': {
        primary: { fg: LIGHTGRAY, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }  // Gold cap
      },
      'sphinx': {
        primary: { fg: YELLOW, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK }
      },
      'hieroglyph': {
        primary: { fg: YELLOW, bg: BG_BLACK },
        secondary: { fg: LIGHTCYAN, bg: BG_BLACK }  // Painted colors
      },
      'scarab': {
        primary: { fg: LIGHTGREEN, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }  // Gold trim
      }
    }
  },
  
  // Clear desert sky
  sky: {
    type: 'gradient',
    converging: false,
    horizontal: false
  },
  
  // Egyptian pyramids with sphinx and obelisks
  background: {
    type: 'pyramids',
    config: {
      pyramidCount: 3,
      hasSphinx: true,
      parallaxSpeed: 0.06
    }
  },
  
  // Desert sun
  celestial: {
    type: 'sun',
    size: 4,
    positionX: 0.6,
    positionY: 0.3
  },
  
  // No visible stars during day
  stars: {
    enabled: false,
    density: 0,
    twinkle: false
  },
  
  // Rolling desert sand
  ground: {
    type: 'sand',
    primary: { fg: YELLOW, bg: BG_BLACK },
    secondary: { fg: BROWN, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.35,
      ditherChars: ['.', "'", '~', GLYPH.LIGHT_SHADE]
    }
  },
  
  // Egyptian ruins along the road
  roadside: {
    pool: [
      { sprite: 'column', weight: 4, side: 'both' },
      { sprite: 'statue', weight: 3, side: 'both' },
      { sprite: 'obelisk', weight: 3, side: 'both' },
      { sprite: 'sphinx', weight: 2, side: 'both' },  // Rare large
      { sprite: 'hieroglyph', weight: 3, side: 'both' },
      { sprite: 'scarab', weight: 2, side: 'both' }  // Small statues
    ],
    spacing: 45,
    density: 0.9  // Spread out ruins
  }
};

registerTheme(AncientRuinsTheme);
