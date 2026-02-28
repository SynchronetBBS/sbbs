/**
 * TrackCatalog - Modular track definition system.
 *
 * Tracks are defined with:
 * - Geometry (segments, curves, length)
 * - Aesthetics (theme, colors, scenery types)
 * - Metadata (name, difficulty, lap count)
 */

// ============================================================
// TRACK THEME DEFINITIONS
// ============================================================

/**
 * Visual theme for a track - defines colors and scenery types.
 */
interface TrackTheme {
  /** Theme identifier */
  id: string;
  
  /** Display name */
  name: string;
  
  /** Sky colors */
  sky: {
    top: ColorDef;
    horizon: ColorDef;
    gridColor: ColorDef;
  };
  
  /** Sun/moon appearance */
  sun: {
    color: ColorDef;
    glowColor: ColorDef;
    position: number;  // 0-1, position on horizon
  };
  
  /** Road colors */
  road: {
    surface: ColorDef;
    stripe: ColorDef;
    edge: ColorDef;
    grid: ColorDef;
  };
  
  /** Off-road scenery */
  offroad: {
    groundColor: ColorDef;
    sceneryTypes: string[];  // e.g., ['palm_tree', 'rock', 'cactus']
    sceneryDensity: number;  // 0-1
  };
  
  /** Background elements */
  background: {
    type: string;  // 'mountains', 'city', 'ocean', 'forest'
    color: ColorDef;
    highlightColor: ColorDef;
  };
}

/**
 * Color definition - uses Synchronet color constants.
 */
interface ColorDef {
  fg: number;
  bg: number;
}

// ============================================================
// TRACK DEFINITION
// ============================================================

/**
 * Complete track definition.
 */
interface TrackDefinition {
  /** Unique identifier */
  id: string;
  
  /** Display name */
  name: string;
  
  /** Description shown in selector */
  description: string;
  
  /** Difficulty rating (1-5 stars) */
  difficulty: number;
  
  /** Number of laps */
  laps: number;
  
  /** Theme ID to use */
  themeId: string;
  
  /** Road geometry - array of section definitions */
  sections: TrackSection[];
  
  /** Estimated time to complete one lap (seconds) - for display */
  estimatedLapTime: number;
  
  /** Number of NPC vehicles (commuters/traffic) on track */
  npcCount?: number;
  
  /** If true, hidden from normal selection (accessible via hotkey) */
  hidden?: boolean;
}

/**
 * A section of track geometry.
 */
interface TrackSection {
  /** Section type */
  type: 'straight' | 'curve' | 'ease_in' | 'ease_out' | 's_curve';
  
  /** Number of segments */
  length: number;
  
  /** Curvature for curves (-1 to 1) */
  curve?: number;
  
  /** Target curve for ease_in */
  targetCurve?: number;
}

// ============================================================
// PREDEFINED THEMES
// ============================================================

var TRACK_THEMES: { [id: string]: TrackTheme } = {
  'synthwave': {
    id: 'synthwave',
    name: 'Synthwave Sunset',
    sky: {
      top: { fg: MAGENTA, bg: BG_BLACK },
      horizon: { fg: LIGHTMAGENTA, bg: BG_BLACK },
      gridColor: { fg: MAGENTA, bg: BG_BLACK }
    },
    sun: {
      color: { fg: YELLOW, bg: BG_RED },
      glowColor: { fg: LIGHTRED, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: CYAN, bg: BG_BLACK },
      stripe: { fg: WHITE, bg: BG_BLACK },
      edge: { fg: LIGHTRED, bg: BG_BLACK },
      grid: { fg: CYAN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: BROWN, bg: BG_BLACK },
      sceneryTypes: ['palm_tree', 'rock', 'grass'],
      sceneryDensity: 0.15
    },
    background: {
      type: 'mountains',
      color: { fg: MAGENTA, bg: BG_BLACK },
      highlightColor: { fg: LIGHTMAGENTA, bg: BG_BLACK }
    }
  },

  'midnight_city': {
    id: 'midnight_city',
    name: 'Midnight City',
    sky: {
      top: { fg: BLUE, bg: BG_BLACK },
      horizon: { fg: LIGHTBLUE, bg: BG_BLACK },
      gridColor: { fg: BLUE, bg: BG_BLACK }
    },
    sun: {
      color: { fg: WHITE, bg: BG_BLUE },
      glowColor: { fg: LIGHTCYAN, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: DARKGRAY, bg: BG_BLACK },
      stripe: { fg: YELLOW, bg: BG_BLACK },
      edge: { fg: WHITE, bg: BG_BLACK },
      grid: { fg: DARKGRAY, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: DARKGRAY, bg: BG_BLACK },
      sceneryTypes: ['building', 'streetlight', 'sign'],
      sceneryDensity: 0.2
    },
    background: {
      type: 'city',
      color: { fg: BLUE, bg: BG_BLACK },
      highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
    }
  },

  'beach_paradise': {
    id: 'beach_paradise',
    name: 'Beach Paradise',
    sky: {
      top: { fg: LIGHTCYAN, bg: BG_BLACK },
      horizon: { fg: CYAN, bg: BG_BLACK },
      gridColor: { fg: CYAN, bg: BG_BLACK }
    },
    sun: {
      color: { fg: YELLOW, bg: BG_BROWN },
      glowColor: { fg: YELLOW, bg: BG_BLACK },
      position: 0.3
    },
    road: {
      surface: { fg: LIGHTGRAY, bg: BG_BLACK },
      stripe: { fg: WHITE, bg: BG_BLACK },
      edge: { fg: YELLOW, bg: BG_BLACK },
      grid: { fg: DARKGRAY, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: YELLOW, bg: BG_BLACK },
      sceneryTypes: ['palm_tree', 'beach_umbrella', 'wave'],
      sceneryDensity: 0.12
    },
    background: {
      type: 'ocean',
      color: { fg: CYAN, bg: BG_BLACK },
      highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
    }
  },

  'forest_night': {
    id: 'forest_night',
    name: 'Forest Night',
    sky: {
      top: { fg: BLACK, bg: BG_BLACK },
      horizon: { fg: DARKGRAY, bg: BG_BLACK },
      gridColor: { fg: DARKGRAY, bg: BG_BLACK }
    },
    sun: {
      color: { fg: WHITE, bg: BG_BLACK },
      glowColor: { fg: LIGHTGRAY, bg: BG_BLACK },
      position: 0.7
    },
    road: {
      surface: { fg: DARKGRAY, bg: BG_BLACK },
      stripe: { fg: WHITE, bg: BG_BLACK },
      edge: { fg: BROWN, bg: BG_BLACK },
      grid: { fg: DARKGRAY, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: GREEN, bg: BG_BLACK },
      sceneryTypes: ['pine_tree', 'bush', 'rock'],
      sceneryDensity: 0.25
    },
    background: {
      type: 'forest',
      color: { fg: GREEN, bg: BG_BLACK },
      highlightColor: { fg: LIGHTGREEN, bg: BG_BLACK }
    }
  },

  'haunted_hollow': {
    id: 'haunted_hollow',
    name: 'Haunted Hollow',
    sky: {
      top: { fg: BLACK, bg: BG_BLACK },
      horizon: { fg: MAGENTA, bg: BG_BLACK },
      gridColor: { fg: DARKGRAY, bg: BG_BLACK }
    },
    sun: {
      color: { fg: RED, bg: BG_BLACK },       // Blood moon
      glowColor: { fg: LIGHTRED, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: DARKGRAY, bg: BG_BLACK },
      stripe: { fg: LIGHTRED, bg: BG_BLACK },
      edge: { fg: DARKGRAY, bg: BG_BLACK },
      grid: { fg: DARKGRAY, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: DARKGRAY, bg: BG_BLACK },
      sceneryTypes: ['deadtree', 'gravestone', 'pumpkin', 'skull', 'fence', 'candle'],
      sceneryDensity: 0.3
    },
    background: {
      type: 'cemetery',
      color: { fg: BLACK, bg: BG_BLACK },
      highlightColor: { fg: MAGENTA, bg: BG_BLACK }
    }
  },

  'winter_wonderland': {
    id: 'winter_wonderland',
    name: 'Winter Wonderland',
    sky: {
      top: { fg: BLUE, bg: BG_BLACK },
      horizon: { fg: WHITE, bg: BG_BLACK },
      gridColor: { fg: LIGHTCYAN, bg: BG_BLACK }
    },
    sun: {
      color: { fg: WHITE, bg: BG_LIGHTGRAY },
      glowColor: { fg: YELLOW, bg: BG_BLACK },
      position: 0.3
    },
    road: {
      surface: { fg: LIGHTGRAY, bg: BG_BLACK },
      stripe: { fg: LIGHTRED, bg: BG_BLACK },
      edge: { fg: WHITE, bg: BG_BLACK },
      grid: { fg: LIGHTCYAN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: WHITE, bg: BG_BLACK },
      sceneryTypes: ['snowpine', 'snowman', 'icecrystal', 'candycane', 'snowdrift', 'signpost'],
      sceneryDensity: 0.25
    },
    background: {
      type: 'mountains',
      color: { fg: WHITE, bg: BG_BLACK },
      highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
    }
  },

  'cactus_canyon': {
    id: 'cactus_canyon',
    name: 'Cactus Canyon',
    sky: {
      top: { fg: BLUE, bg: BG_BLACK },
      horizon: { fg: YELLOW, bg: BG_BLACK },
      gridColor: { fg: BROWN, bg: BG_BLACK }
    },
    sun: {
      color: { fg: YELLOW, bg: BG_BROWN },
      glowColor: { fg: YELLOW, bg: BG_BLACK },
      position: 0.6
    },
    road: {
      surface: { fg: BROWN, bg: BG_BLACK },
      stripe: { fg: YELLOW, bg: BG_BLACK },
      edge: { fg: BROWN, bg: BG_BLACK },
      grid: { fg: BROWN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: YELLOW, bg: BG_BLACK },
      sceneryTypes: ['saguaro', 'barrel', 'tumbleweed', 'cowskull', 'desertrock', 'westernsign'],
      sceneryDensity: 0.2
    },
    background: {
      type: 'dunes',
      color: { fg: BROWN, bg: BG_BLACK },
      highlightColor: { fg: YELLOW, bg: BG_BLACK }
    }
  },

  'tropical_jungle': {
    id: 'tropical_jungle',
    name: 'Tropical Jungle',
    sky: {
      top: { fg: GREEN, bg: BG_BLACK },
      horizon: { fg: LIGHTGREEN, bg: BG_BLACK },
      gridColor: { fg: GREEN, bg: BG_BLACK }
    },
    sun: {
      color: { fg: YELLOW, bg: BG_GREEN },
      glowColor: { fg: LIGHTGREEN, bg: BG_BLACK },
      position: 0.4
    },
    road: {
      surface: { fg: BROWN, bg: BG_BLACK },
      stripe: { fg: YELLOW, bg: BG_BLACK },
      edge: { fg: GREEN, bg: BG_BLACK },
      grid: { fg: BROWN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: GREEN, bg: BG_BLACK },
      sceneryTypes: ['jungle_tree', 'fern', 'vine', 'flower', 'parrot', 'banana'],
      sceneryDensity: 0.35
    },
    background: {
      type: 'forest',
      color: { fg: GREEN, bg: BG_BLACK },
      highlightColor: { fg: LIGHTGREEN, bg: BG_BLACK }
    }
  },

  'candy_land': {
    id: 'candy_land',
    name: 'Candy Land',
    sky: {
      top: { fg: LIGHTMAGENTA, bg: BG_BLACK },
      horizon: { fg: LIGHTCYAN, bg: BG_BLACK },
      gridColor: { fg: LIGHTMAGENTA, bg: BG_BLACK }
    },
    sun: {
      color: { fg: YELLOW, bg: BG_MAGENTA },
      glowColor: { fg: LIGHTMAGENTA, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: LIGHTMAGENTA, bg: BG_BLACK },
      stripe: { fg: WHITE, bg: BG_BLACK },
      edge: { fg: LIGHTCYAN, bg: BG_BLACK },
      grid: { fg: MAGENTA, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: LIGHTGREEN, bg: BG_BLACK },
      sceneryTypes: ['lollipop', 'candy_cane', 'gummy_bear', 'cupcake', 'ice_cream', 'cotton_candy'],
      sceneryDensity: 0.3
    },
    background: {
      type: 'mountains',
      color: { fg: LIGHTMAGENTA, bg: BG_BLACK },
      highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
    }
  },

  'rainbow_road': {
    id: 'rainbow_road',
    name: 'Rainbow Road',
    sky: {
      top: { fg: BLUE, bg: BG_BLACK },
      horizon: { fg: LIGHTBLUE, bg: BG_BLACK },
      gridColor: { fg: LIGHTMAGENTA, bg: BG_BLACK }
    },
    sun: {
      color: { fg: WHITE, bg: BG_BLUE },
      glowColor: { fg: LIGHTCYAN, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: LIGHTRED, bg: BG_BLACK },
      stripe: { fg: YELLOW, bg: BG_BLACK },
      edge: { fg: LIGHTMAGENTA, bg: BG_BLACK },
      grid: { fg: LIGHTCYAN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: BLUE, bg: BG_BLACK },
      sceneryTypes: ['star', 'moon', 'planet', 'comet', 'nebula', 'satellite'],
      sceneryDensity: 0.2
    },
    background: {
      type: 'space',
      color: { fg: BLUE, bg: BG_BLACK },
      highlightColor: { fg: LIGHTMAGENTA, bg: BG_BLACK }
    }
  },

  'dark_castle': {
    id: 'dark_castle',
    name: 'Dark Castle',
    sky: {
      top: { fg: BLACK, bg: BG_BLACK },
      horizon: { fg: DARKGRAY, bg: BG_BLACK },
      gridColor: { fg: DARKGRAY, bg: BG_BLACK }
    },
    sun: {
      color: { fg: LIGHTGRAY, bg: BG_BLACK },
      glowColor: { fg: DARKGRAY, bg: BG_BLACK },
      position: 0.7
    },
    road: {
      surface: { fg: DARKGRAY, bg: BG_BLACK },
      stripe: { fg: LIGHTGRAY, bg: BG_BLACK },
      edge: { fg: BROWN, bg: BG_BLACK },
      grid: { fg: DARKGRAY, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: DARKGRAY, bg: BG_BLACK },
      sceneryTypes: ['tower', 'battlement', 'torch', 'banner', 'gargoyle', 'portcullis'],
      sceneryDensity: 0.25
    },
    background: {
      type: 'castle',
      color: { fg: DARKGRAY, bg: BG_BLACK },
      highlightColor: { fg: LIGHTGRAY, bg: BG_BLACK }
    }
  },

  'villains_lair': {
    id: 'villains_lair',
    name: "Villain's Lair",
    sky: {
      top: { fg: RED, bg: BG_BLACK },
      horizon: { fg: LIGHTRED, bg: BG_BLACK },
      gridColor: { fg: RED, bg: BG_BLACK }
    },
    sun: {
      color: { fg: LIGHTRED, bg: BG_RED },
      glowColor: { fg: YELLOW, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: DARKGRAY, bg: BG_BLACK },
      stripe: { fg: LIGHTRED, bg: BG_BLACK },
      edge: { fg: RED, bg: BG_BLACK },
      grid: { fg: RED, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: RED, bg: BG_BLACK },
      sceneryTypes: ['lava_rock', 'flame', 'skull_pile', 'chain', 'spike', 'cauldron'],
      sceneryDensity: 0.28
    },
    background: {
      type: 'volcano',
      color: { fg: RED, bg: BG_BLACK },
      highlightColor: { fg: YELLOW, bg: BG_BLACK }
    }
  },

  'ancient_ruins': {
    id: 'ancient_ruins',
    name: 'Ancient Ruins',
    sky: {
      top: { fg: CYAN, bg: BG_BLACK },
      horizon: { fg: YELLOW, bg: BG_BLACK },
      gridColor: { fg: BROWN, bg: BG_BLACK }
    },
    sun: {
      color: { fg: YELLOW, bg: BG_BROWN },
      glowColor: { fg: YELLOW, bg: BG_BLACK },
      position: 0.4
    },
    road: {
      surface: { fg: BROWN, bg: BG_BLACK },
      stripe: { fg: YELLOW, bg: BG_BLACK },
      edge: { fg: LIGHTGRAY, bg: BG_BLACK },
      grid: { fg: BROWN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: YELLOW, bg: BG_BLACK },
      sceneryTypes: ['column', 'statue', 'obelisk', 'sphinx', 'hieroglyph', 'scarab'],
      sceneryDensity: 0.22
    },
    background: {
      type: 'pyramids',
      color: { fg: YELLOW, bg: BG_BLACK },
      highlightColor: { fg: BROWN, bg: BG_BLACK }
    }
  },

  'thunder_stadium': {
    id: 'thunder_stadium',
    name: 'Thunder Stadium',
    sky: {
      top: { fg: BLACK, bg: BG_BLACK },
      horizon: { fg: DARKGRAY, bg: BG_BLACK },
      gridColor: { fg: YELLOW, bg: BG_BLACK }
    },
    sun: {
      color: { fg: WHITE, bg: BG_BLACK },
      glowColor: { fg: YELLOW, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: BROWN, bg: BG_BLACK },
      stripe: { fg: WHITE, bg: BG_BLACK },
      edge: { fg: YELLOW, bg: BG_BLACK },
      grid: { fg: BROWN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: BROWN, bg: BG_BLACK },
      sceneryTypes: ['grandstand', 'tire_stack', 'hay_bale', 'flag_marshal', 'pit_crew', 'banner'],
      sceneryDensity: 0.35
    },
    background: {
      type: 'stadium',
      color: { fg: DARKGRAY, bg: BG_BLACK },
      highlightColor: { fg: YELLOW, bg: BG_BLACK }
    }
  },

  'glitch_circuit': {
    id: 'glitch_circuit',
    name: 'Glitch Circuit',
    sky: {
      top: { fg: BLACK, bg: BG_BLACK },
      horizon: { fg: LIGHTGREEN, bg: BG_BLACK },
      gridColor: { fg: GREEN, bg: BG_BLACK }
    },
    sun: {
      color: { fg: WHITE, bg: BG_GREEN },
      glowColor: { fg: LIGHTGREEN, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: DARKGRAY, bg: BG_BLACK },
      stripe: { fg: LIGHTGREEN, bg: BG_BLACK },
      edge: { fg: LIGHTCYAN, bg: BG_BLACK },
      grid: { fg: GREEN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: GREEN, bg: BG_BLACK },
      sceneryTypes: ['building', 'palm_tree', 'tree', 'gravestone', 'cactus', 'lollipop', 'crystal', 'torch', 'column'],
      sceneryDensity: 0.30
    },
    background: {
      type: 'mountains',
      color: { fg: GREEN, bg: BG_BLACK },
      highlightColor: { fg: LIGHTGREEN, bg: BG_BLACK }
    }
  },

  'kaiju_rampage': {
    id: 'kaiju_rampage',
    name: 'Kaiju Rampage',
    sky: {
      top: { fg: DARKGRAY, bg: BG_BLACK },
      horizon: { fg: RED, bg: BG_BLACK },
      gridColor: { fg: BROWN, bg: BG_BLACK }
    },
    sun: {
      color: { fg: LIGHTRED, bg: BG_RED },
      glowColor: { fg: YELLOW, bg: BG_BLACK },
      position: 0.3
    },
    road: {
      surface: { fg: DARKGRAY, bg: BG_BLACK },
      stripe: { fg: YELLOW, bg: BG_BLACK },
      edge: { fg: LIGHTRED, bg: BG_BLACK },
      grid: { fg: BROWN, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: BROWN, bg: BG_BLACK },
      sceneryTypes: ['rubble', 'wrecked_car', 'fire', 'monster_footprint', 'fallen_building', 'tank'],
      sceneryDensity: 0.35
    },
    background: {
      type: 'destroyed_city',
      color: { fg: DARKGRAY, bg: BG_BLACK },
      highlightColor: { fg: LIGHTRED, bg: BG_BLACK }
    }
  },

  'ansi_tunnel': {
    id: 'ansi_tunnel',
    name: 'ANSI Tunnel',
    sky: {
      top: { fg: BLACK, bg: BG_BLACK },
      horizon: { fg: CYAN, bg: BG_BLACK },
      gridColor: { fg: LIGHTCYAN, bg: BG_BLACK }
    },
    sun: {
      color: { fg: WHITE, bg: BG_BLACK },
      glowColor: { fg: CYAN, bg: BG_BLACK },
      position: 0.5
    },
    road: {
      surface: { fg: DARKGRAY, bg: BG_BLACK },
      stripe: { fg: LIGHTCYAN, bg: BG_BLACK },
      edge: { fg: CYAN, bg: BG_BLACK },
      grid: { fg: DARKGRAY, bg: BG_BLACK }
    },
    offroad: {
      groundColor: { fg: BLACK, bg: BG_BLACK },
      sceneryTypes: ['data_beacon', 'data_node', 'signal_pole', 'binary_pillar'],
      sceneryDensity: 0.15
    },
    background: {
      type: 'ansi',
      color: { fg: CYAN, bg: BG_BLACK },
      highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
    }
  }
};

// ============================================================
// TRACK CATALOG
// ============================================================

var TRACK_CATALOG: TrackDefinition[] = [
  // ============================================================
  // SPRINT TRACKS (~1:00-1:30 total race time, 20-30 seg laps)
  // ============================================================

  // ---- SUNSET BEACH (easy intro track) ----
  {
    id: 'sunset_beach',
    name: 'Sunset Beach',
    description: 'Gentle coastal cruise - perfect for beginners',
    difficulty: 1,
    laps: 3,
    themeId: 'beach_paradise',
    estimatedLapTime: 25,
    npcCount: 4,
    sections: [
      { type: 'straight', length: 8 },
      { type: 'ease_in', length: 3, targetCurve: 0.3 },
      { type: 'curve', length: 6, curve: 0.3 },
      { type: 'ease_out', length: 3 },
      { type: 'straight', length: 5 }
    ]
  },

  // ---- SUGAR RUSH (fun candy sprint) ----
  {
    id: 'sugar_rush',
    name: 'Sugar Rush',
    description: 'Sweet sprint through candy land!',
    difficulty: 2,
    laps: 3,
    themeId: 'candy_land',
    estimatedLapTime: 28,
    npcCount: 4,
    sections: [
      { type: 'straight', length: 6 },
      { type: 'ease_in', length: 2, targetCurve: 0.4 },
      { type: 'curve', length: 5, curve: 0.4 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.35 },
      { type: 'curve', length: 5, curve: -0.35 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- THUNDER STADIUM (oval-style) - SECRET TRACK ----
  {
    id: 'thunder_stadium',
    name: 'Thunder Stadium',
    description: 'Oval speedway under the lights',
    difficulty: 1,
    laps: 4,
    themeId: 'thunder_stadium',
    estimatedLapTime: 22,
    npcCount: 6,
    hidden: true,  // Secret track - access via ? key
    sections: [
      { type: 'straight', length: 6 },
      { type: 'ease_in', length: 2, targetCurve: 0.5 },
      { type: 'curve', length: 6, curve: 0.5 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 6 }
    ]
  },

  // ---- DATA HIGHWAY (ANSI Tunnel) ----
  {
    id: 'data_highway',
    name: 'Data Highway',
    description: 'Race through scrolling ANSI art at 28.8 Kbps!',
    difficulty: 2,
    laps: 3,
    themeId: 'ansi_tunnel',
    estimatedLapTime: 30,
    npcCount: 5,
    sections: [
      { type: 'straight', length: 8 },
      { type: 'ease_in', length: 3, targetCurve: 0.35 },
      { type: 'curve', length: 6, curve: 0.35 },
      { type: 'ease_out', length: 3 },
      { type: 'straight', length: 6 },
      { type: 'ease_in', length: 2, targetCurve: -0.4 },
      { type: 'curve', length: 5, curve: -0.4 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ============================================================
  // STANDARD TRACKS (~1:45-2:30 total race time, 35-50 seg laps)
  // ============================================================

  // ---- NEON COAST (signature track) ----
  {
    id: 'neon_coast',
    name: 'Neon Coast',
    description: 'Synthwave sunset along the coast',
    difficulty: 2,
    laps: 3,
    themeId: 'synthwave',
    estimatedLapTime: 38,
    npcCount: 6,
    sections: [
      { type: 'straight', length: 8 },
      { type: 'ease_in', length: 3, targetCurve: 0.4 },
      { type: 'curve', length: 8, curve: 0.4 },
      { type: 'ease_out', length: 3 },
      { type: 'straight', length: 6 },
      { type: 'ease_in', length: 2, targetCurve: -0.5 },
      { type: 'curve', length: 6, curve: -0.5 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- DOWNTOWN DASH (city streets) ----
  {
    id: 'downtown_dash',
    name: 'Downtown Dash',
    description: 'Tight corners through city streets',
    difficulty: 3,
    laps: 3,
    themeId: 'midnight_city',
    estimatedLapTime: 42,
    npcCount: 7,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.7 },
      { type: 'curve', length: 5, curve: 0.7 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.75 },
      { type: 'curve', length: 5, curve: -0.75 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.5 },
      { type: 'curve', length: 6, curve: 0.5 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- WINTER WONDERLAND (snowy) ----
  {
    id: 'winter_wonderland',
    name: 'Winter Wonderland',
    description: 'Icy roads through a frosty forest',
    difficulty: 2,
    laps: 3,
    themeId: 'winter_wonderland',
    estimatedLapTime: 35,
    npcCount: 4,
    sections: [
      { type: 'straight', length: 6 },
      { type: 'ease_in', length: 3, targetCurve: 0.35 },
      { type: 'curve', length: 7, curve: 0.35 },
      { type: 'ease_out', length: 3 },
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: -0.4 },
      { type: 'curve', length: 6, curve: -0.4 },
      { type: 'ease_out', length: 3 }
    ]
  },

  // ---- CACTUS CANYON (desert) ----
  {
    id: 'cactus_canyon',
    name: 'Cactus Canyon',
    description: 'Blazing trails through desert canyons',
    difficulty: 3,
    laps: 3,
    themeId: 'cactus_canyon',
    estimatedLapTime: 40,
    npcCount: 5,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.5 },
      { type: 'curve', length: 7, curve: 0.5 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.65 },
      { type: 'curve', length: 6, curve: -0.65 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: 0.4 },
      { type: 'curve', length: 4, curve: 0.4 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- TWILIGHT GROVE (forest) ----
  {
    id: 'twilight_grove',
    name: 'Twilight Grove',
    description: 'Winding paths under dual moons',
    difficulty: 3,
    laps: 3,
    themeId: 'forest_night',
    estimatedLapTime: 38,
    npcCount: 5,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: -0.4 },
      { type: 'curve', length: 5, curve: -0.4 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: 0.55 },
      { type: 'curve', length: 6, curve: 0.55 },
      { type: 'ease_out', length: 2 },
      { type: 'ease_in', length: 2, targetCurve: -0.45 },
      { type: 'curve', length: 5, curve: -0.45 },
      { type: 'ease_out', length: 3 }
    ]
  },

  // ---- JUNGLE RUN (tropical) ----
  {
    id: 'jungle_run',
    name: 'Jungle Run',
    description: 'Dense rainforest with tight turns',
    difficulty: 3,
    laps: 3,
    themeId: 'tropical_jungle',
    estimatedLapTime: 36,
    npcCount: 5,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.5 },
      { type: 'curve', length: 6, curve: 0.5 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.55 },
      { type: 'curve', length: 5, curve: -0.55 },
      { type: 'ease_out', length: 2 },
      { type: 'ease_in', length: 2, targetCurve: 0.4 },
      { type: 'curve', length: 4, curve: 0.4 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- HAUNTED HOLLOW (horror) ----
  {
    id: 'haunted_hollow',
    name: 'Haunted Hollow',
    description: 'Spooky cemetery with sharp turns',
    difficulty: 4,
    laps: 3,
    themeId: 'haunted_hollow',
    estimatedLapTime: 44,
    npcCount: 3,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: -0.5 },
      { type: 'curve', length: 5, curve: -0.5 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 3 },
      { type: 'ease_in', length: 2, targetCurve: 0.7 },
      { type: 'curve', length: 6, curve: 0.7 },
      { type: 'ease_out', length: 2 },
      { type: 'ease_in', length: 2, targetCurve: -0.6 },
      { type: 'curve', length: 4, curve: -0.6 },
      { type: 'ease_in', length: 2, targetCurve: 0.5 },
      { type: 'curve', length: 4, curve: 0.5 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 3 }
    ]
  },

  // ---- FORTRESS RALLY (castle) ----
  {
    id: 'fortress_rally',
    name: 'Fortress Rally',
    description: 'Medieval castle walls and courtyards',
    difficulty: 4,
    laps: 3,
    themeId: 'dark_castle',
    estimatedLapTime: 45,
    npcCount: 5,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.45 },
      { type: 'curve', length: 6, curve: 0.45 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.7 },
      { type: 'curve', length: 5, curve: -0.7 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 3 },
      { type: 'ease_in', length: 2, targetCurve: 0.5 },
      { type: 'curve', length: 5, curve: 0.5 },
      { type: 'ease_in', length: 2, targetCurve: -0.45 },
      { type: 'curve', length: 5, curve: -0.45 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- PHARAOH'S TOMB (ruins) ----
  {
    id: 'pharaohs_tomb',
    name: "Pharaoh's Tomb",
    description: 'Ancient pyramid mysteries',
    difficulty: 3,
    laps: 3,
    themeId: 'ancient_ruins',
    estimatedLapTime: 40,
    npcCount: 5,
    hidden: true,  // Hidden from normal selection, accessible via hotkey
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.45 },
      { type: 'curve', length: 6, curve: 0.45 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.55 },
      { type: 'curve', length: 5, curve: -0.55 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: 0.4 },
      { type: 'curve', length: 4, curve: 0.4 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- MERMAID LAGOON (underwater grotto) ----
  {
    id: 'mermaid_lagoon',
    name: 'Mermaid Lagoon',
    description: 'Race through a magical underwater grotto',
    difficulty: 3,
    laps: 3,
    themeId: 'underwater_grotto',
    estimatedLapTime: 42,
    npcCount: 5,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.4 },
      { type: 'curve', length: 5, curve: 0.4 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.5 },
      { type: 'curve', length: 6, curve: -0.5 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 3 },
      { type: 'ease_in', length: 2, targetCurve: 0.45 },
      { type: 'curve', length: 5, curve: 0.45 },
      { type: 'ease_in', length: 2, targetCurve: -0.35 },
      { type: 'curve', length: 4, curve: -0.35 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- INFERNO SPEEDWAY (volcano) ----
  {
    id: 'inferno_speedway',
    name: 'Inferno Speedway',
    description: 'Volcanic villain lair at your peril',
    difficulty: 5,
    laps: 3,
    themeId: 'villains_lair',
    estimatedLapTime: 48,
    npcCount: 6,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.6 },
      { type: 'curve', length: 7, curve: 0.6 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.7 },
      { type: 'curve', length: 6, curve: -0.7 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 3 },
      { type: 'ease_in', length: 2, targetCurve: 0.55 },
      { type: 'curve', length: 5, curve: 0.55 },
      { type: 'ease_in', length: 2, targetCurve: -0.5 },
      { type: 'curve', length: 5, curve: -0.5 },
      { type: 'ease_out', length: 2 }
    ]
  },

  // ---- KAIJU RAMPAGE (monster city) ----
  {
    id: 'kaiju_rampage',
    name: 'Kaiju Rampage',
    description: 'Flee the monster through a crumbling city!',
    difficulty: 5,
    laps: 3,
    themeId: 'kaiju_rampage',
    estimatedLapTime: 50,
    npcCount: 5,
    sections: [
      { type: 'straight', length: 6 },
      { type: 'ease_in', length: 2, targetCurve: 0.75 },
      { type: 'curve', length: 5, curve: 0.75 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 },
      { type: 'ease_in', length: 2, targetCurve: -0.6 },
      { type: 'curve', length: 4, curve: -0.6 },
      { type: 'ease_in', length: 2, targetCurve: 0.55 },
      { type: 'curve', length: 4, curve: 0.55 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: -0.65 },
      { type: 'curve', length: 6, curve: -0.65 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 4 }
    ]
  },

  // ============================================================
  // ENDURANCE TRACKS (~3:00-4:00 total race time, 60-80 seg laps)
  // ============================================================

  // ---- CELESTIAL CIRCUIT (longest rainbow road) ----
  {
    id: 'celestial_circuit',
    name: 'Celestial Circuit',
    description: 'Epic cosmic journey through the stars',
    difficulty: 4,
    laps: 3,
    themeId: 'rainbow_road',
    estimatedLapTime: 70,
    npcCount: 7,
    sections: [
      // Launch sequence
      { type: 'straight', length: 8 },
      // Around the moon
      { type: 'ease_in', length: 3, targetCurve: 0.5 },
      { type: 'curve', length: 10, curve: 0.5 },
      { type: 'ease_out', length: 3 },
      // Asteroid field straight
      { type: 'straight', length: 6 },
      // Nebula hairpin
      { type: 'ease_in', length: 2, targetCurve: -0.65 },
      { type: 'curve', length: 6, curve: -0.65 },
      { type: 'ease_out', length: 2 },
      // Comet chase
      { type: 'straight', length: 5 },
      // Binary star helix
      { type: 'ease_in', length: 2, targetCurve: 0.55 },
      { type: 'curve', length: 6, curve: 0.55 },
      { type: 'ease_in', length: 2, targetCurve: -0.55 },
      { type: 'curve', length: 6, curve: -0.55 },
      { type: 'ease_out', length: 2 },
      // Return warp
      { type: 'straight', length: 7 }
    ]
  },

  // ---- GLITCH CIRCUIT (longest corrupted reality) ----
  {
    id: 'glitch_circuit',
    name: 'Glitch Circuit',
    description: 'Reality is corrupted. The simulation breaks.',
    difficulty: 5,
    laps: 3,
    themeId: 'glitch_circuit',
    estimatedLapTime: 75,
    npcCount: 6,
    sections: [
      // Stable zone
      { type: 'straight', length: 6 },
      // Reality warps
      { type: 'ease_in', length: 2, targetCurve: 0.6 },
      { type: 'curve', length: 5, curve: 0.6 },
      { type: 'ease_in', length: 2, targetCurve: -0.7 },
      { type: 'curve', length: 6, curve: -0.7 },
      { type: 'ease_out', length: 2 },
      // Brief stability
      { type: 'straight', length: 4 },
      // Rapid glitches
      { type: 'ease_in', length: 2, targetCurve: 0.55 },
      { type: 'curve', length: 4, curve: 0.55 },
      { type: 'ease_in', length: 2, targetCurve: -0.5 },
      { type: 'curve', length: 4, curve: -0.5 },
      { type: 'ease_in', length: 2, targetCurve: 0.45 },
      { type: 'curve', length: 4, curve: 0.45 },
      { type: 'ease_out', length: 2 },
      // System recovery
      { type: 'straight', length: 6 },
      // Major corruption
      { type: 'ease_in', length: 3, targetCurve: -0.6 },
      { type: 'curve', length: 10, curve: -0.6 },
      { type: 'ease_out', length: 3 },
      // Stabilizing
      { type: 'straight', length: 6 }
    ]
  },

  // ============================================================
  // TEST TRACKS (for development)
  // ============================================================

  {
    id: 'test_oval',
    name: 'Test Oval',
    description: 'Simple oval for testing',
    difficulty: 1,
    laps: 2,
    themeId: 'synthwave',
    estimatedLapTime: 20,
    npcCount: 3,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.5 },
      { type: 'curve', length: 6, curve: 0.5 },
      { type: 'ease_out', length: 2 },
      { type: 'straight', length: 5 }
    ]
  },

  {
    id: 'quick_test',
    name: 'Quick Test',
    description: 'Ultra-short for quick tests',
    difficulty: 1,
    laps: 2,
    themeId: 'synthwave',
    estimatedLapTime: 12,
    npcCount: 2,
    sections: [
      { type: 'straight', length: 5 },
      { type: 'ease_in', length: 2, targetCurve: 0.4 },
      { type: 'curve', length: 4, curve: 0.4 },
      { type: 'ease_out', length: 2 }
    ]
  }
];

// ============================================================
// TRACK BUILDER FROM DEFINITION
// ============================================================

/**
 * Build a Road from a TrackDefinition.
 */
function buildRoadFromDefinition(def: TrackDefinition): Road {
  var builder = new RoadBuilder()
    .name(def.name)
    .laps(def.laps);

  for (var i = 0; i < def.sections.length; i++) {
    var section = def.sections[i];
    
    switch (section.type) {
      case 'straight':
        builder.straight(section.length);
        break;
        
      case 'curve':
        builder.curve(section.length, section.curve || 0);
        break;
        
      case 'ease_in':
        builder.easeIn(section.length, section.targetCurve || 0);
        break;
        
      case 'ease_out':
        builder.easeOut(section.length);
        break;
        
      case 's_curve':
        // S-curve: right then left (or use length to split evenly)
        var halfLen = Math.floor(section.length / 6);
        builder
          .easeIn(halfLen, 0.5)
          .curve(halfLen * 2, 0.5)
          .easeOut(halfLen)
          .easeIn(halfLen, -0.5)
          .curve(halfLen * 2, -0.5)
          .easeOut(halfLen);
        break;
    }
  }

  return builder.build();
}

/**
 * Get a track definition by ID.
 */
function getTrackDefinition(id: string): TrackDefinition | null {
  for (var i = 0; i < TRACK_CATALOG.length; i++) {
    if (TRACK_CATALOG[i].id === id) {
      return TRACK_CATALOG[i];
    }
  }
  return null;
}

/**
 * Get the theme for a track.
 */
function getTrackTheme(trackDef: TrackDefinition): TrackTheme {
  return TRACK_THEMES[trackDef.themeId] || TRACK_THEMES['synthwave'];
}

/**
 * Get all available tracks.
 */
function getAllTracks(): TrackDefinition[] {
  return TRACK_CATALOG;
}

/**
 * Render difficulty as stars.
 */
function renderDifficultyStars(difficulty: number): string {
  var stars = '';
  for (var i = 0; i < 5; i++) {
    stars += i < difficulty ? '*' : '.';
  }
  return stars;
}
