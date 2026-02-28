/**
 * ThunderStadiumTheme.ts - Motorsport dirt track stadium theme.
 * Packed dirt racing under stadium lights with roaring crowds.
 */

var ThunderStadiumTheme: Theme = {
  name: 'thunder_stadium',
  description: 'Race on packed dirt under stadium lights with roaring crowds',
  
  colors: {
    // Night sky
    skyTop: { fg: BLACK, bg: BG_BLACK },
    skyMid: { fg: DARKGRAY, bg: BG_BLACK },
    skyHorizon: { fg: DARKGRAY, bg: BG_BLACK },
    
    // Stadium lights - no grid
    skyGrid: { fg: YELLOW, bg: BG_BLACK },
    skyGridGlow: { fg: WHITE, bg: BG_BLACK },
    
    // No celestial (indoors)
    celestialCore: { fg: WHITE, bg: BG_BLACK },
    celestialGlow: { fg: YELLOW, bg: BG_BLACK },
    
    // Stadium lights
    starBright: { fg: WHITE, bg: BG_BLACK },
    starDim: { fg: YELLOW, bg: BG_BLACK },
    
    // Stadium grandstands
    sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
    scenerySecondary: { fg: WHITE, bg: BG_BLACK },
    sceneryTertiary: { fg: YELLOW, bg: BG_BLACK },
    
    // Brown dirt road
    roadSurface: { fg: YELLOW, bg: BG_BROWN },
    roadSurfaceAlt: { fg: BROWN, bg: BG_BROWN },
    roadStripe: { fg: WHITE, bg: BG_BROWN },
    roadEdge: { fg: YELLOW, bg: BG_BLACK },
    roadGrid: { fg: DARKGRAY, bg: BG_BROWN },
    
    // Dirt shoulders
    shoulderPrimary: { fg: YELLOW, bg: BG_BROWN },
    shoulderSecondary: { fg: BROWN, bg: BG_BROWN },
    
    // Roadside object colors
    roadsideColors: {
      'grandstand': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: WHITE, bg: BG_BLACK }
      },
      'tire_stack': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: WHITE, bg: BG_BLACK }
      },
      'hay_bale': {
        primary: { fg: YELLOW, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK }
      },
      'flag_marshal': {
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      },
      'pit_crew': {
        primary: { fg: LIGHTRED, bg: BG_BLACK },
        secondary: { fg: RED, bg: BG_BLACK }
      },
      'banner': {
        primary: { fg: LIGHTRED, bg: BG_RED },
        secondary: { fg: YELLOW, bg: BG_BROWN }
      }
    }
  },
  
  // Dark stadium sky - no grid
  sky: {
    type: 'plain',
    converging: false,
    horizontal: false
  },
  
  // Stadium background with grandstands and lights
  background: {
    type: 'stadium',
    config: {
      parallaxSpeed: 0.05
    }
  },
  
  // No celestial body (indoor stadium)
  celestial: {
    type: 'none',
    size: 0,
    positionX: 0.5,
    positionY: 0.5
  },
  
  // Stadium lights instead of stars
  stars: {
    enabled: false,
    density: 0,
    twinkle: false
  },
  
  // Packed dirt ground with tire tracks
  ground: {
    type: 'dirt',
    primary: { fg: BROWN, bg: BG_BLACK },
    secondary: { fg: BROWN, bg: BG_BLACK },
    pattern: {
      ditherDensity: 0.4,
      ditherChars: ['.', "'", '`', GLYPH.LIGHT_SHADE]
    }
  },
  
  // Stadium/motorsport roadside objects
  roadside: {
    pool: [
      { sprite: 'grandstand', weight: 3, side: 'both' },
      { sprite: 'tire_stack', weight: 4, side: 'both' },
      { sprite: 'hay_bale', weight: 4, side: 'both' },
      { sprite: 'flag_marshal', weight: 2, side: 'both' },
      { sprite: 'pit_crew', weight: 2, side: 'both' },
      { sprite: 'banner', weight: 3, side: 'both' }
    ],
    spacing: 35,
    density: 1.2  // Lots of track objects
  }
};

registerTheme(ThunderStadiumTheme);
