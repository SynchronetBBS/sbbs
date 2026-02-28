/**
 * Palette - ANSI color constants and utilities.
 * Synthwave aesthetic: magenta sky grid, cyan road grid, black backgrounds.
 */

var PALETTE = {
  // Sky - magenta grid on black (synthwave aesthetic)
  SKY_TOP: { fg: MAGENTA, bg: BG_BLACK },
  SKY_MID: { fg: LIGHTMAGENTA, bg: BG_BLACK },
  SKY_HORIZON: { fg: LIGHTMAGENTA, bg: BG_BLACK },

  // Sky grid lines (magenta)
  SKY_GRID: { fg: MAGENTA, bg: BG_BLACK },
  SKY_GRID_GLOW: { fg: LIGHTMAGENTA, bg: BG_BLACK },

  // Sun/moon
  SUN_CORE: { fg: YELLOW, bg: BG_RED },
  SUN_GLOW: { fg: LIGHTRED, bg: BG_BLACK },

  // Mountains (background scenery)
  MOUNTAIN: { fg: MAGENTA, bg: BG_BLACK },
  MOUNTAIN_HIGHLIGHT: { fg: LIGHTMAGENTA, bg: BG_BLACK },

  // Horizon grid (cyan - road side)
  GRID_LINE: { fg: CYAN, bg: BG_BLACK },
  GRID_GLOW: { fg: LIGHTCYAN, bg: BG_BLACK },

  // Road (cyan grid aesthetic)
  ROAD_LIGHT: { fg: LIGHTCYAN, bg: BG_BLACK },
  ROAD_DARK: { fg: CYAN, bg: BG_BLACK },
  ROAD_STRIPE: { fg: WHITE, bg: BG_BLACK },
  ROAD_EDGE: { fg: LIGHTRED, bg: BG_BLACK },
  ROAD_GRID: { fg: CYAN, bg: BG_BLACK },

  // Off-road scenery (greens, browns, grays on black)
  OFFROAD_GRASS: { fg: GREEN, bg: BG_BLACK },
  OFFROAD_DIRT: { fg: BROWN, bg: BG_BLACK },
  OFFROAD_ROCK: { fg: DARKGRAY, bg: BG_BLACK },
  OFFROAD_TREE: { fg: LIGHTGREEN, bg: BG_BLACK },
  OFFROAD_TREE_TRUNK: { fg: BROWN, bg: BG_BLACK },
  OFFROAD_CACTUS: { fg: GREEN, bg: BG_BLACK },

  // Vehicles
  PLAYER_BODY: { fg: YELLOW, bg: BG_BLACK },
  PLAYER_TRIM: { fg: WHITE, bg: BG_BLACK },
  ENEMY_BODY: { fg: LIGHTCYAN, bg: BG_BLACK },

  // HUD
  HUD_FRAME: { fg: LIGHTCYAN, bg: BG_BLACK },
  HUD_TEXT: { fg: WHITE, bg: BG_BLACK },
  HUD_VALUE: { fg: YELLOW, bg: BG_BLACK },
  HUD_LABEL: { fg: LIGHTGRAY, bg: BG_BLACK },

  // Items
  ITEM_BOX: { fg: YELLOW, bg: BG_BLACK },
  ITEM_MUSHROOM: { fg: LIGHTRED, bg: BG_BLACK },
  ITEM_SHELL: { fg: LIGHTGREEN, bg: BG_BLACK }
};

/**
 * Create attribute byte from foreground and background.
 */
function makeAttr(fg: number, bg: number): number {
  return fg | bg;
}

/**
 * Get foreground color from attribute.
 */
function getFg(attr: number): number {
  return attr & 0x0F;
}

/**
 * Get background color from attribute.
 */
function getBg(attr: number): number {
  return attr & 0xF0;
}

/**
 * Color pair to attribute byte.
 */
function colorToAttr(color: { fg: number; bg: number }): number {
  return color.fg | color.bg;
}
