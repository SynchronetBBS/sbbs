/**
 * KaijuRampageTheme.ts - A city under kaiju attack!
 * Destroyed buildings, fires, rubble, and chaos as monsters rampage.
 */

var KaijuRampageTheme: Theme = {
  name: 'kaiju_rampage',
  description: 'Destroyed city with fires, rubble, and giant monster silhouette',
  
  colors: {
    // Sky - smoky orange/red apocalyptic
    skyTop: { fg: DARKGRAY, bg: BG_BLACK },
    skyMid: { fg: BROWN, bg: BG_BLACK },
    skyHorizon: { fg: LIGHTRED, bg: BG_BLACK },
    
    // No grid for kaiju - smoky sky
    skyGrid: { fg: DARKGRAY, bg: BG_BLACK },
    skyGridGlow: { fg: BROWN, bg: BG_BLACK },
    
    // Fire glow (no sun/moon - obscured by smoke)
    celestialCore: { fg: LIGHTRED, bg: BG_BLACK },
    celestialGlow: { fg: YELLOW, bg: BG_BLACK },
    
    // Stars barely visible through smoke
    starBright: { fg: DARKGRAY, bg: BG_BLACK },
    starDim: { fg: BLACK, bg: BG_BLACK },
    
    // Destroyed buildings - dark with fire highlights
    sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
    scenerySecondary: { fg: BROWN, bg: BG_BLACK },
    sceneryTertiary: { fg: LIGHTRED, bg: BG_BLACK },  // Fire glow
    
    // Road - cracked asphalt with debris
    roadSurface: { fg: LIGHTGRAY, bg: BG_BLACK },
    roadSurfaceAlt: { fg: DARKGRAY, bg: BG_BLACK },
    roadStripe: { fg: WHITE, bg: BG_BLACK },
    roadEdge: { fg: LIGHTRED, bg: BG_BLACK },  // Warning markers
    roadGrid: { fg: BROWN, bg: BG_BLACK },
    
    // Shoulders - rubble and debris
    shoulderPrimary: { fg: BROWN, bg: BG_BLACK },
    shoulderSecondary: { fg: DARKGRAY, bg: BG_BLACK },
    
    // Roadside object colors - destruction themed
    roadsideColors: {
      'rubble': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK }
      },
      'fire': {
        primary: { fg: LIGHTRED, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK }
      },
      'wrecked_car': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK },
        tertiary: { fg: LIGHTRED, bg: BG_BLACK }  // Fire
      },
      'fallen_building': {
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK }
      },
      'tank': {
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK }
      },
      'monster_footprint': {
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK }
      },
      'danger_sign': {
        primary: { fg: YELLOW, bg: BG_BLACK },
        secondary: { fg: LIGHTRED, bg: BG_BLACK }
      }
    }
  },
  
  // Sky background: smoky gradient (no grid)
  sky: {
    type: 'gradient',
    converging: false,
    horizontal: false
  },
  
  background: {
    type: 'destroyed_city',
    config: {
      parallaxSpeed: 0.15
    }
  },
  
  celestial: {
    type: 'monster',  // Giant kaiju silhouette
    size: 5,
    positionX: 0.5,   // Centered like the sun
    positionY: 0.5    // Mid-sky, prominent
  },
  
  stars: {
    enabled: false,
    density: 0,
    twinkle: false
  },
  
  // Ground - cracked roads with debris
  ground: {
    type: 'solid',
    primary: { fg: BROWN, bg: BG_BLACK },
    secondary: { fg: DARKGRAY, bg: BG_BLACK },
    pattern: {
      gridSpacing: 20,
      radialLines: 0
    }
  },
  
  roadside: {
    pool: [
      { sprite: 'fleeing_person', weight: 4, side: 'both' },
      { sprite: 'wrecked_car', weight: 3, side: 'both' },
      { sprite: 'fire', weight: 2, side: 'both' },
      { sprite: 'rubble', weight: 2, side: 'both' },
      { sprite: 'danger_sign', weight: 1, side: 'both' }
    ],
    spacing: 12,
    density: 0.9
  }
};

// Register the theme
registerTheme(KaijuRampageTheme);

/**
 * Get scenery sprites for the Kaiju theme.
 */
function getKaijuSprite(type: string): SpriteDefinition | null {
  switch (type) {
    case 'rubble': return KaijuSprites.createRubble();
    case 'fire': return KaijuSprites.createFire();
    case 'wrecked_car': return KaijuSprites.createWreckedCar();
    case 'fallen_building': return KaijuSprites.createFallenBuilding();
    case 'tank': return KaijuSprites.createTank();
    case 'monster_footprint': return KaijuSprites.createFootprint();
    case 'danger_sign': return KaijuSprites.createDangerSign();
    default: return null;
  }
}
