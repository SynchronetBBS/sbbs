/**
 * Theme - Interface defining a complete visual aesthetic.
 * 
 * Themes are composable: you can mix and match elements.
 * Each theme defines colors, patterns, and sprites for all scene elements.
 * 
 * Color system: Themes define a color palette that can recolor shared
 * background renderers and sprites. This allows mountains rendered in
 * purple in one theme to be green in another.
 */

interface ColorPair {
  fg: number;
  bg: number;
}

interface ThemeColors {
  // Sky background
  skyTop: ColorPair;
  skyMid: ColorPair;
  skyHorizon: ColorPair;
  
  // Sky grid (synthwave aesthetic)
  skyGrid: ColorPair;
  skyGridGlow: ColorPair;
  
  // Celestial body (sun/moon)
  celestialCore: ColorPair;
  celestialGlow: ColorPair;
  
  // Stars (if enabled)
  starBright: ColorPair;
  starDim: ColorPair;
  
  // Background scenery (mountains/buildings/etc)
  sceneryPrimary: ColorPair;      // Main color (mountain body, building walls)
  scenerySecondary: ColorPair;    // Accent (highlights, windows)
  sceneryTertiary: ColorPair;     // Additional detail (snow caps, antennas)
  
  // Road surface
  roadSurface: ColorPair;         // Main road color
  roadSurfaceAlt: ColorPair;      // Alternating shade for depth
  roadStripe: ColorPair;          // Center line
  roadEdge: ColorPair;            // Road edge/curb
  roadGrid: ColorPair;            // Grid lines on road (if any)
  
  // Road shoulders/off-road
  shoulderPrimary: ColorPair;     // Immediate road edge (dirt, curb)
  shoulderSecondary: ColorPair;   // Further out
  
  // Item box colors (optional - uses defaults if not set)
  itemBox?: {
    border: ColorPair;    // Box border color
    fill: ColorPair;      // Box fill/background
    symbol: ColorPair;    // The "?" symbol
  };
  
  // Roadside object color overrides (optional - uses sprite defaults if not set)
  roadsideColors?: {
    [spriteName: string]: {
      primary: ColorPair;
      secondary?: ColorPair;
      tertiary?: ColorPair;
    };
  };
}

interface ThemeBackgroundElement {
  type: 'mountains' | 'skyscrapers' | 'dunes' | 'forest' | 'hills' | 'ocean' | 
        'jungle_canopy' | 'candy_hills' | 'nebula' | 'castle_fortress' | 
        'volcanic' | 'pyramids' | 'stadium' | 'destroyed_city' | 'underwater' | 'aquarium' | 'ansi';
  // Configuration varies by type
  config: {
    // Mountains/hills
    count?: number;
    minHeight?: number;
    maxHeight?: number;
    // Skyscrapers
    density?: number;
    hasWindows?: boolean;
    hasAntennas?: boolean;
    // Jungle canopy
    vineCount?: number;
    hangingVines?: boolean;
    // Candy hills
    swirls?: boolean;
    // Nebula
    cloudCount?: number;
    // Castle
    towerCount?: number;
    hasTorches?: boolean;
    // Volcanic
    lavaGlow?: boolean;
    smokeLevel?: number;
    // Pyramids
    pyramidCount?: number;
    hasSphinx?: boolean;
    // Underwater
    kelp?: boolean;
    bubbles?: boolean;
    // General
    parallaxSpeed?: number;
  };
}

interface ThemeCelestialBody {
  type: 'sun' | 'moon' | 'dual_moons' | 'monster' | 'mermaid' | 'none';
  size: number;           // 1-5 scale
  positionX: number;      // 0=left, 0.5=center, 1=right
  positionY: number;      // 0=top of sky, 1=at horizon
}

interface ThemeStars {
  enabled: boolean;
  density: number;        // 0-1, how many stars
  twinkle: boolean;       // Animated twinkle effect
}

interface ThemeSkyBackground {
  type: 'grid' | 'stars' | 'gradient' | 'plain' | 'water' | 'ansi';  // What to render in sky background layer
  // Grid-specific config
  gridDensity?: number;   // For grid spacing
  gridChar?: string;      // Character to use for grid lines
  converging?: boolean;   // Converging lines to vanishing point
  horizontal?: boolean;   // Horizontal scan lines
  // Stars use the ThemeStars config
}

/**
 * A weighted roadside object entry.
 * Weight determines relative frequency (higher = more common).
 */
interface RoadsidePoolEntry {
  sprite: string;         // Sprite name from ROADSIDE_SPRITES registry
  weight: number;         // Relative weight (e.g., 3 = 3x as common as weight 1)
  side?: 'left' | 'right' | 'both';  // Which side(s) it can appear on
}

interface ThemeRoadsideConfig {
  // Object pool with weights for controlled distribution
  // e.g., [{ sprite: 'building', weight: 3 }, { sprite: 'lamppost', weight: 2 }, { sprite: 'tree', weight: 1 }]
  pool: RoadsidePoolEntry[];
  // Spacing between objects (world units)
  spacing: number;
  // Density multiplier (1 = normal, 2 = double, 0.5 = half)
  density: number;
}

/**
 * Ground/terrain configuration for off-road areas.
 * Defines how the terrain looks on either side of the road.
 */
interface ThemeGroundConfig {
  // Type of ground pattern
  type: 'solid' | 'grid' | 'dither' | 'grass' | 'sand' | 'lava' | 'candy' | 'void' | 'cobblestone' | 'jungle' | 'dirt' | 'water';
  
  // Colors for ground (primary = main, secondary = pattern/accent)
  primary: ColorPair;
  secondary: ColorPair;
  
  // Pattern-specific config
  pattern?: {
    // Grid pattern: converging lines like synthwave holodeck
    gridSpacing?: number;      // Spacing between horizontal grid lines
    gridChar?: string;         // Character for grid intersections (default '+')
    converging?: boolean;      // Lines converge to horizon vanishing point
    radialLines?: number;      // Number of radial lines per side (default 8)
    
    // Dither pattern: checkerboard/noise for dirt/gravel
    ditherDensity?: number;    // 0-1, how dense the pattern is
    ditherChars?: string[];    // Characters to use ('.', ',', "'", etc)
    
    // Grass pattern: tufts of grass
    grassDensity?: number;     // 0-1
    grassChars?: string[];     // Characters like '"', 'v', ',', etc
  };
}

interface Theme {
  name: string;
  description: string;
  
  // Complete color palette
  colors: ThemeColors;
  
  // Sky background layer (grid, stars, or plain)
  sky: ThemeSkyBackground;
  
  // Background scenery renderer (mountains, buildings, etc)
  background: ThemeBackgroundElement;
  
  // Celestial body (sun/moon)
  celestial: ThemeCelestialBody;
  
  // Star field config (used when sky.type === 'stars')
  stars: ThemeStars;
  
  // Ground/terrain for off-road areas (optional - defaults to solid shoulder colors)
  ground?: ThemeGroundConfig;
  
  // Roadside objects configuration
  roadside: ThemeRoadsideConfig;
  
  // Road rendering options (optional)
  road?: {
    rainbow?: boolean;        // Cycles through rainbow colors
    hideEdgeMarkers?: boolean; // Hide edge markers (for rainbow road blend)
  };
  
  // HUD customization (optional) - for themed HUD labels
  hud?: {
    speedLabel?: string;      // Label for speed (default: none, just number)
    speedMultiplier?: number; // Multiply speed by this (e.g., 0.24 for "Kbps")
    positionPrefix?: string;  // Prefix for position (e.g., "Node " instead of "")
    lapLabel?: string;        // Label for lap (e.g., "SECTOR" instead of "LAP")
    timeLabel?: string;       // Label for time (e.g., "CONNECT" instead of "TIME")
  };
}

/**
 * Global registry of available themes
 */
var ThemeRegistry: { [key: string]: Theme } = {};

/**
 * Global registry of roadside sprite creators
 */
var ROADSIDE_SPRITES: { [key: string]: () => SpriteDefinition } = {};

/**
 * Register a theme
 */
function registerTheme(theme: Theme): void {
  ThemeRegistry[theme.name] = theme;
}

/**
 * Register a roadside sprite
 */
function registerRoadsideSprite(name: string, creator: () => SpriteDefinition): void {
  ROADSIDE_SPRITES[name] = creator;
}

/**
 * Get a theme by name
 */
function getTheme(name: string): Theme | null {
  return ThemeRegistry[name] || null;
}

/**
 * Get all theme names
 */
function getThemeNames(): string[] {
  var names: string[] = [];
  for (var key in ThemeRegistry) {
    if (ThemeRegistry.hasOwnProperty(key)) {
      names.push(key);
    }
  }
  return names;
}
