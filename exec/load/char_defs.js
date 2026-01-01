// This file defines some variables for various characters to be
// displayed to the user.
//
// Author: Eric Oulashin (AKA Nightfox)
// BBS: Digital Distortion
// BBS address: digitaldistortionbbs.com (or digdist.synchro.net)

"use strict";

// Box-drawing/border characters: Single-line
var UPPER_LEFT_SINGLE = "\xDA"; // ASCII 218
var HORIZONTAL_SINGLE = "\xC4"; // ASCII 196
var UPPER_RIGHT_SINGLE = "\xBF"; // ASCII 191 // or 170?
var VERTICAL_SINGLE = "\xB3"; // ASCII 179
var LOWER_LEFT_SINGLE = "\xC0"; // ASCII 192
var LOWER_RIGHT_SINGLE = "\xD9"; // ASCII 217
var T_SINGLE = "\xC2"; // ASCII 194
var LEFT_T_SINGLE = "\xC3"; // ASCII 195
var RIGHT_T_SINGLE = "\xB4"; // ASCII 180
var BOTTOM_T_SINGLE = "\xC1"; // ASCII 193
var CROSS_SINGLE = "\xC5"; // ASCII 197
// Box-drawing/border characters: Double-line
var UPPER_LEFT_DOUBLE = "\xC9"; // ASCII 201
var HORIZONTAL_DOUBLE = "\xCD"; // ASCII 205
var UPPER_RIGHT_DOUBLE = "\xBB"; // ASCII 187
var VERTICAL_DOUBLE = "\xBA"; // ASCII 186
//var LOWER_LEFT_DOUBLE = ascii(200); // "\xCB" is the hex value but wasn't making the correct character
var LOWER_LEFT_DOUBLE = "\xC8"; // ASCII 200
var LOWER_RIGHT_DOUBLE = "\xBC"; // ASCII 188
var T_DOUBLE = "\xCB"; // ASCII 203
var LEFT_T_DOUBLE = "\xCC"; // ASCII 204
var RIGHT_T_DOUBLE = "\xB9"; // ASCII 185
var BOTTOM_T_DOUBLE = "\xCA"; // ASCII 202
var CROSS_DOUBLE = "\xCE"; // ASCII 206
// Box-drawing/border characters: Vertical single-line with horizontal double-line
var UPPER_LEFT_VSINGLE_HDOUBLE = "\xD5"; // ASCII 213
var UPPER_RIGHT_VSINGLE_HDOUBLE = "\xB8"; // ASCII 184
var LOWER_LEFT_VSINGLE_HDOUBLE = "\xD4"; // ASCII 212
var LOWER_RIGHT_VSINGLE_HDOUBLE = "\xBE"; // ASCII 190
// Other special characters
var DOT_CHAR = "\xF9"; // ASCII 249
var CHECK_CHAR = "\xFB"; // ASCII 251
var THIN_RECTANGLE_LEFT = "\xDD"; // ASCII 221
var THIN_RECTANGLE_RIGHT = "\xDE"; // ASCII 222
var CENTERED_SQUARE = "\xFE"; // ASCII 254
var BLOCK1 = "\xB0"; // ASCII 176 // Dimmest block
var BLOCK2 = "\xB1"; // ASCII 177
var BLOCK3 = "\xB2"; // ASCII 178
var BLOCK4 = "\xDB"; // ASCII 219 // Brightest block