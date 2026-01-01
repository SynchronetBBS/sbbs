// This file defines some variables for various characters to be
// displayed to the user.
//
// Author: Eric Oulashin (AKA Nightfox)
// BBS: Digital Distortion
// BBS address: digitaldistortionbbs.com (or digdist.synchro.net)

"use strict";

// The source file src/xpdev/cp437defs.h in the Synchronet repository
// has CP437 character definitions for the C++ source
// TODO: Add more from cp437defs.h to this file

// Box-drawing/border characters: Single-line
var CP437_BOX_DRAWING_UPPER_LEFT_SINGLE = "\xDA"; // 218
var CP437_BOX_DRAWING_HORIZONTAL_SINGLE = "\xC4"; // 196
var CP437_BOX_DRAWING_UPPER_RIGHT_SINGLE = "\xBF"; // 191 // or 170?
var CP437_BOX_DRAWINGS_LIGHT_VERTICAL = "\xB3"; // 179
var CP437_BOX_DRAWING_LOWER_LEFT_SINGLE = "\xC0"; // 192
var CP437_BOX_DRAWING_LOWER_RIGHT_SINGLE = "\xD9"; // 217
var CP437_BOX_DRAWING_LIGHT_T = "\xC2"; // 194
var CP437_BOX_DRAWING_LIGHT_LEFT_T = "\xC3"; // 195
var CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT = "\xB4"; // 180
var CP437_BOX_DRAWING_LIGHT_BOTTOM_T = "\xC1"; // 193
var CP437_BOX_DRAWINGS_VERTICAL_AND_HORIZONTAL = "\xC5"; // 197
// Box-drawing/border characters: Double-line
var CP437_BOX_DRAWING_UPPER_LEFT_DOUBLE = "\xC9"; // 201
var CP437_BOX_DRAWING_HORIZONTAL_DOUBLE = "\xCD"; // 205
var CP437_BOX_DRAWING_UPPER_RIGHT_DOUBLE = "\xBB"; // 187
var CP437_BOX_DRAWINGS_DOUBLE_VERTICAL = "\xBA"; // 186
//var CP437_BOX_DRAWING_LOWER_LEFT_DOUBLE = ascii(200); // "\xCB" is the hex value but wasn't making the correct character
var CP437_BOX_DRAWING_LOWER_LEFT_DOUBLE = "\xC8"; // 200
var CP437_BOX_DRAWING_LOWER_RIGHT_DOUBLE = "\xBC"; // 188
var CP437_BOX_DRAWING_DOUBLE_T = "\xCB"; // 203
var CP437_BOX_DRAWING_LEFT_DOUBLE_T = "\xCC"; // 204
var CP437_BOX_DRAWING_RIGHT_DOUBLE_T = "\xB9"; // 185
var CP437_BOX_DRAWING_BOTTOM_DOUBLE_T = "\xCA"; // 202
var CP437_BOX_DRAWING_DOUBLE_CROSS = "\xCE"; // 206
// Box-drawing/border characters: Vertical single-line with horizontal double-line
var CP437_BOX_DRAWING_UPPER_LEFT_VSINGLE_HDOUBLE = "\xD5"; // 213
var CP437_BOX_DRAWING_UPPER_RIGHT_VSINGLE_HDOUBLE = "\xB8"; // 184
var CP437_BOX_DRAWING_LOWER_LEFT_VSINGLE_HDOUBLE = "\xD4"; // 212
var CP437_BOX_DRAWING_LOWER_RIGHT_VSINGLE_HDOUBLE = "\xBE"; // 190
// Other special characters
var CP437_BULLET_OPERATOR = "\xF9"; // 249
var CP437_SQUARE_ROOT = "\xFB"; // 251
var CP437_CHECK_MARK = "\xFB"; // Duplicate; 251
var CP437_LEFT_HALF_BLOCK = "\xDD"; // 221
var CP437_RIGHT_HALF_BLOCK = "\xDE"; // 222
var CP437_BLACK_SQUARE = "\xFE"; // 254
var CP437_LIGHT_SHADE = "\xB0"; // 176 // Dimmest block
var CP437_MEDIUM_SHADE = "\xB1"; // 177
var CP437_DARK_SHADE = "\xB2"; // 178
var CP437_FULL_BLOCK = "\xDB"; // 219 // Brightest block
var CP437_LOWER_HALF_BLOCK = "\xDC";
var CP437_BLACK_SQUARE = "\xFE";
var CP437_UPPER_HALF_BLOCK = "\xDF";
var CP437_MIDDLE_DOT = "\xFA";
