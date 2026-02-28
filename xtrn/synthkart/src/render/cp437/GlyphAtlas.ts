/**
 * GlyphAtlas - CP437 character constants.
 * Using actual CP437 codes for Synchronet compatibility.
 */

var GLYPH = {
  // Block elements (CP437)
  FULL_BLOCK: String.fromCharCode(219),     // █
  LOWER_HALF: String.fromCharCode(220),     // ▄
  UPPER_HALF: String.fromCharCode(223),     // ▀
  LEFT_HALF: String.fromCharCode(221),      // ▌
  RIGHT_HALF: String.fromCharCode(222),     // ▐
  LIGHT_SHADE: String.fromCharCode(176),    // ░
  MEDIUM_SHADE: String.fromCharCode(177),   // ▒
  DARK_SHADE: String.fromCharCode(178),     // ▓

  // Box drawing - single (CP437)
  BOX_H: String.fromCharCode(196),          // ─
  BOX_V: String.fromCharCode(179),          // │
  BOX_TL: String.fromCharCode(218),         // ┌
  BOX_TR: String.fromCharCode(191),         // ┐
  BOX_BL: String.fromCharCode(192),         // └
  BOX_BR: String.fromCharCode(217),         // ┘
  BOX_VR: String.fromCharCode(195),         // ├
  BOX_VL: String.fromCharCode(180),         // ┤
  BOX_HD: String.fromCharCode(194),         // ┬
  BOX_HU: String.fromCharCode(193),         // ┴
  BOX_CROSS: String.fromCharCode(197),      // ┼

  // Box drawing - double (CP437)
  DBOX_H: String.fromCharCode(205),         // ═
  DBOX_V: String.fromCharCode(186),         // ║
  DBOX_TL: String.fromCharCode(201),        // ╔
  DBOX_TR: String.fromCharCode(187),        // ╗
  DBOX_BL: String.fromCharCode(200),        // ╚
  DBOX_BR: String.fromCharCode(188),        // ╝
  DBOX_VR: String.fromCharCode(204),        // ╠
  DBOX_VL: String.fromCharCode(185),        // ╣
  DBOX_HD: String.fromCharCode(203),        // ╦
  DBOX_HU: String.fromCharCode(202),        // ╩
  DBOX_CROSS: String.fromCharCode(206),     // ╬
  
  // Box drawing - mixed single/double (CP437)
  BOX_VD_HD: String.fromCharCode(209),      // ╤ (single vertical, double horizontal down)
  BOX_VD_HU: String.fromCharCode(207),      // ╧ (single vertical, double horizontal up)

  // Geometric shapes - using safe ASCII alternatives
  // (CP437 chars 0-31 are control codes in most modern terminals)
  TRIANGLE_UP: '^',                         // Was char 30 (▲) - control char
  TRIANGLE_DOWN: 'v',                       // Was char 31 (▼) - control char
  TRIANGLE_LEFT: '<',                       // Was char 17 (◄) - DC1 control char
  TRIANGLE_RIGHT: '>',                      // Was char 16 (►) - DLE control char
  DIAMOND: '*',                             // Was char 4 (♦) - EOT control char
  BULLET: 'o',                              // Was char 7 (●) - BELL control char
  CIRCLE: 'O',                              // Was char 9 (○) - TAB control char!
  INVERSE_BULLET: '@',                      // Was char 8 (◘) - BACKSPACE control char!

  // Other useful characters
  SPACE: ' ',
  DOT: String.fromCharCode(250),            // ·
  SLASH: '/',
  BACKSLASH: '\\',
  EQUALS: '=',
  ASTERISK: '*',

  // Scenery characters - using safe alternatives
  TREE_TOP: '^',                            // Was char 6 (♠) - ACK control char
  TREE_TRUNK: String.fromCharCode(179),     // │
  ROCK: String.fromCharCode(178),           // ▓
  GRASS: String.fromCharCode(176),          // ░
  CACTUS: '|',                              // Was char 157 - may be problematic
  MOUNTAIN_PEAK: '/',
  MOUNTAIN_SLOPE: '\\',
  
  // Racing elements
  CHECKER: String.fromCharCode(177),        // ▒ - checkered flag pattern
  FLAG: '>'                                 // Was char 16 (►) - DLE control char
};

/**
 * Get shade character based on intensity (0-1).
 */
function getShadeGlyph(intensity: number): string {
  if (intensity >= 0.75) return GLYPH.FULL_BLOCK;
  if (intensity >= 0.5) return GLYPH.DARK_SHADE;
  if (intensity >= 0.25) return GLYPH.MEDIUM_SHADE;
  return GLYPH.LIGHT_SHADE;
}

/**
 * Get horizontal bar segment for a given fill amount (0-1).
 */
function getBarGlyph(fill: number): string {
  if (fill >= 0.875) return GLYPH.FULL_BLOCK;
  if (fill >= 0.625) return GLYPH.DARK_SHADE;
  if (fill >= 0.375) return GLYPH.MEDIUM_SHADE;
  if (fill >= 0.125) return GLYPH.LIGHT_SHADE;
  return GLYPH.SPACE;
}
