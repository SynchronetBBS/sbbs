/**
 * VillainsLairTheme.ts - Racing through a volcanic villain's domain.
 * Lava flows, skull decorations, and ominous flames.
 */

var VillainsLairTheme: Theme = {
  name: 'villains_lair',
  description: 'Speed through the fiery domain of a supervillain',
  
  colors: {
    // Volcanic red/black sky
    skyTop: { fg: BLACK, bg: BG_BLACK },
    skyMid: { fg: RED, bg: BG_BLACK },
    skyHorizon: { fg: YELLOW, bg: BG_BLACK },
    
    // Heat shimmer grid
    skyGrid: { fg: RED, bg: BG_BLACK },
    skyGridGlow: { fg: YELLOW, bg: BG_BLACK },
    
    // Volcanic eruption glow
    celestialCore: { fg: YELLOW, bg: BG_RED },
    celestialGlow: { fg: RED, bg: BG_BLACK },
    
    // Ember stars
    starBright: { fg: YELLOW, bg: BG_BLACK },
    starDim: { fg: RED, bg: BG_BLACK },
    
    // Volcanic mountains
    sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
    scenerySecondary: { fg: RED, bg: BG_BLACK },  // Lava glow
    sceneryTertiary: { fg: YELLOW, bg: BG_BLACK },
    
    // Obsidian road
    roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
    roadSurfaceAlt: { fg: BLACK, bg: BG_BLACK },
    roadStripe: { fg: RED, bg: BG_BLACK },  // Lava stripe
    roadEdge: { fg: YELLOW, bg: BG_BLACK },
    roadGrid: { fg: RED, bg: BG_BLACK },
    
    // Scorched shoulders
    shoulderPrimary: { fg: DARKGRAY, bg: BG_BLACK },
    shoulderSecondary: { fg: RED, bg: BG_BLACK },
    
    // Roadside colors - volcanic/evil palette
    roadsideColors: {
      'lava_rock': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: RED, bg: BG_BLACK }  // Glowing cracks
      },
      'flame': {
        primary: { fg: YELLOW, bg: BG_BLACK },
        secondary: { fg: RED, bg: BG_BLACK }
      },
      'skull_pile': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
      },
      'chain': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
      },
      'spike': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: RED, bg: BG_BLACK }  // Blood stains
      },
      'cauldron': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: LIGHTGREEN, bg: BG_BLACK }  // Bubbling brew
      }
    }
  },
  
  // Smoky volcanic sky
  sky: {
    type: 'gradient',
    converging: false,
    horizontal: false
  },
  
  // Volcanic peaks with erupting lava and smoke
  background: {
    type: 'volcanic',
    config: {
      lavaGlow: true,
      smokeLevel: 2,
      parallaxSpeed: 0.1
    }
  },
  
  // Volcano glow
  celestial: {
    type: 'sun',  // Volcanic glow on horizon
    size: 5,
    positionX: 0.5,
    positionY: 0.6
  },
  
  // Embers in the sky
  stars: {
    enabled: true,
    density: 0.35,
    twinkle: true  // Floating embers
  },
  
  // Cracked obsidian ground with flowing lava veins
  ground: {
    type: 'lava',
    primary: { fg: DARKGRAY, bg: BG_BLACK },
    secondary: { fg: YELLOW, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.5,
      ditherChars: ['*', '~', GLYPH.MEDIUM_SHADE]
    }
  },
  
  // Villain lair decorations
  roadside: {
    pool: [
      { sprite: 'lava_rock', weight: 4, side: 'both' },
      { sprite: 'flame', weight: 5, side: 'both' },  // Lots of fire
      { sprite: 'skull_pile', weight: 3, side: 'both' },
      { sprite: 'chain', weight: 2, side: 'both' },
      { sprite: 'spike', weight: 3, side: 'both' },
      { sprite: 'cauldron', weight: 2, side: 'both' }
    ],
    spacing: 35,
    density: 1.1
  }
};

registerTheme(VillainsLairTheme);
