// This file defines functions & things to help programmatically
// output RIP commands and UI elements, for terminals that support
// RIP.
//
// A RIP command string must be sent to the client starting on a new
// line and must start with an exclamation point (!). A single RIP
// command starts with the pipe symbol (|) and a character, and multiple
// RIP commands can be chained together in a single string (but that
// string only needs to have one ! at the start).
// The functions in this file that return a string with a RIP command
// don't include the ! so that you can easily chain them together as
// desired.
// In the RIPScrip 1.54 spec, see the section
// "RIPscrip PROTOCOL - GENERAL STRUCTURE" near the top of the file
// for more details on the structure of RIP commands.
//
// Some useful URLs:
// RIP-related downloads on Break Into Chat:
// https://breakintochat.com/wiki/Remote_Imaging_Protocol_(RIP)#Downloads
//
// RIPScrip 1.54 specification (which this is based on):
// https://carl.gorringe.org/pub/code/javascript/RIPtermJS/docs/RIPscrip154.txt
//
// RIPScrip 2.00 Alpha 4 specification:
// https://carl.gorringe.org/pub/code/javascript/RIPtermJS/docs/RIPscrip200a4.txt
//
// Base-10 to base-36 ("MegaNum") converter:
// https://www.unitconverters.net/numbers/decimal-to-base-36.htm
//
// Base-36 (MegaNum) to base-10 converter:
// https://www.unitconverters.net/numbers/base-36-to-decimal.htm
//
// Author: Eric Oulashin (AKA Nightfox)
// BBS: Digital Distortion
// BBS address: digitaldistortionbbs.com (or digdist.synchro.net)

"use strict";

require("rip.js", "RIP");

// The maximum value that can fit in a 2-digit (base-36) MegaNum.
// This is stored in a variable because 2-digit MegaNum values
// are fairly common in a lot of the RIPScrip commands.
var MAX_2DIGIT_MEGANUM_VAL = getMaxMegaNumValue(2);

// RIP button style flags (numeric values, for RIPButtonStyleNumeric())
// Each of these values may be combined to achieve a "composite" group of flags.
// See the preceding paragraphs in the RIPScrip 1.54 spec for a more detailed
// explanation of this method.
//          Value Description of Flags Field #1   Icon Clip  Plain Mouse No-Mouse
//          ---------------------------------------------------------------------
//              1 Button is a "Clipboard Button"   N     Y     N     Y     Y
//              2 Button is "Invertable"           Y     Y     Y     Y     N
//              4 Reset screen after button click  Y     Y     Y     Y     N
//              8 Display Chisel special effect    Y     Y     Y     Y     Y
//             16 Display Recessed special effect  Y     Y     Y     Y     Y
//             32 Dropshadow the label (if any)    Y     Y     Y     Y     Y
//             64 Auto-stamp image onto Clipboard  Y     Y     Y     Y     Y
//            128 Button is an "Icon Button"       Y     N     N     Y     Y
//            256 Button is a "Plain Button"       N     N     Y     Y     Y
//            512 Display Bevel special effect     Y     Y     Y     Y     Y
//           1024 Button is a Mouse Button         Y     Y     Y     Y     N
//           2048 Underline hot-key in label       Y     Y     Y     Y     N
//           4096 Make Icon Button use Hot Icons   Y     N     N     Y     N
//           8192 Adj. vertical centering of label Y     Y     Y     Y     Y
//          16384 Button belongs to a Radio Group  Y     Y     Y     Y     N
//          32768 Display Sunken special effect    Y     Y     Y     Y     Y
var RIP_BTN_STYLE_CLIPBOARD             = (1<<0);  // Button is a "Clipboard Button"
var RIP_BTN_STYLE_INVERTABLE            = (1<<1);  // Button is "Invertable"
var RIP_BTN_STYLE_RESET_AFTER_CLICK     = (1<<2);  // Reset screen after button click
var RIP_BTN_STYLE_CHISEL_EFFECT         = (1<<3);  // Display Chisel special effect
var RIP_BTN_STYLE_RECESSED              = (1<<4);  // Display Recessed special effect
var RIP_BTN_STYLE_DROPSHADOW_LBL        = (1<<5);  // Dropshadow the label (if any)
var RIP_BTN_STYLE_AUTOSTAMP_IMG_CLIPBRD = (1<<6);  // Auto-stamp image onto clipboard
var RIP_BTN_STYLE_ICON                  = (1<<7);  // Button is an "Icon Button"
var RIP_BTN_STYLE_PLAIN                 = (1<<8);  // Button is a "Plain Button"
var RIP_BTN_STYLE_BEVEL                 = (1<<9);  // Display Bevel special effect
var RIP_BTN_STYLE_MOUSE                 = (1<<10); // Button is a Mouse Button
var RIP_BTN_STYLE_UNDERLINE_HOTKEY      = (1<<11); // Underline hot-key in label
var RIP_BTN_STYLE_ICON_HOT_ICONS        = (1<<12); // Make icon button use hot icons
var RIP_BTN_STYLE_ADJ_VERT_CENTERING    = (1<<13); // Adjust vertical centering of label
var RIP_BTN_STYLE_RADIO_GROUP           = (1<<14); // Button belongs to a radio group
var RIP_BTN_STYLE_SUNKEN                = (1<<15); // Display Sunken special effect

// RIP color definitions (numeric values)
// 16-Color RIP Palette     Master 64-Color EGA
//     Color Code           Palette Color Code       Color
// ---------------------------------------------------------------
//        00                     0  (00)             Black
//        01                     1  (01)             Blue
//        02                     2  (02)             Green
//        03                     3  (03)             Cyan
//        04                     4  (04)             Red
//        05                     5  (05)             Magenta
//        06                     7  (06)             Brown
//        07                     20 (0K)             Light Gray
//        08                     56 (1K)             Dark Gray
//        09                     57 (1L)             Light Blue
//        0A                     58 (1M)             Light Green
//        0B                     59 (1N)             Light Cyan
//        0C                     60 (1O)             Light Red
//        0D                     61 (1P)             Light Magenta
//        0E                     62 (1Q)             Yellow
//        0F                     63 (1R)             White
var RIP_COLOR_BLACK      = 0;
var RIP_COLOR_BLUE       = 1;
var RIP_COLOR_GREEN      = 2;
var RIP_COLOR_CYAN       = 3;
var RIP_COLOR_RED        = 4;
var RIP_COLOR_MAGENTA    = 5;
var RIP_COLOR_BROWN      = 6;
var RIP_COLOR_LT_GRAY    = 7; // Light gray
var RIP_COLOR_DK_GRAY    = 8; // Dark gray
var RIP_COLOR_LT_BLUE    = 9;
var RIP_COLOR_LT_GREEN   = 10;
var RIP_COLOR_LT_CYAN    = 11;
var RIP_COLOR_LT_RED     = 12;
var RIP_COLOR_LT_MAGENTA = 13;
var RIP_COLOR_YELLOW     = 14;
var RIP_COLOR_WHITE      = 15;

// RIP font definitions (numeric values, for RIPFontStyleNumeric())
// Font   Description of Font
// ---------------------------------------------
// 00     Default 8x8 font            Bit-Mapped
// 01     Triplex Font                Scalable
// 02     Small Font                  Scalable
// 03     Sans Serif Font             Scalable
// 04     Gothic [Old English] Font   Scalable
// 05     Script Font                 Scalable
// 06     Simplex Font                Scalable
// 07     Triplex Script Font         Scalable
// 08     Complex Font                Scalable
// 09     European Font               Scalable
// 0A     Bold Font                   Scalable
var RIP_FONT_DEFAULT    = 0;  // Default 8x8 font
var RIP_FONT_TRIPLEX    = 1;  // Scalable
var RIP_FONT_SMALL      = 2;  // Scalable
var RIP_FONT_SANS_SERIF = 3;  // Scalable
var RIP_FONT_GOTHIC     = 4;  // Scalable
var RIP_FONT_SCRIPT     = 5;  // Scalable
var RIP_FONT_SIMPLEX    = 6;  // Scalable
var RIP_FONT_TRIPLEX    = 7;  // Scalable
var RIP_FONT_COMPLEX    = 8;  // Scalable
var RIP_FONT_EUROPEAN   = 9;  // Scalable
var RIP_FONT_BOLD       = 10; // Scalable


////////////////////////////////////////////////////
// RIP MegaNum stuff, commands, styles, colors, etc.

// Converts a number to a MegaNum string of a specified width.
// RIPscrip uses base-36 (0-9, A-Z) for efficiency [1, 2].
function toMegaNum(val, width)
{
	const chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	var result = "";
	var tempVal = val;
	for (var i = 0; i < width; ++i)
	{
		result = chars[tempVal % 36] + result;
		tempVal = Math.floor(tempVal / 36);
	}
	return result;
}

/**
 * Converts a base-36 MegaNum string to an integer value.
 * MegaNums use 0-9 and A-Z (capital letters). [1, 3]
 */
function fromMegaNum(megaStr)
{
	const chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // Character set defined in [2, 3]
	var result = 0;

	for (var i = 0; i < megaStr.length; ++i)
	{
		var currentChar = megaStr[i];
		var charValue = chars.indexOf(currentChar);

		// Validates that the character is within the MegaNum set.
		// Lowercase letters are reserved for base-64 UltraNums [3].
		if (charValue === -1)
			throw new Error("Invalid MegaNum character: " + currentChar);

		// Multiply existing result by the base (36) and add the new digit value.
		result = (result * 36) + charValue;
	}

	return result;
}

// Returns whether a string is a valid RIP "MegaNum".
//
// Parameters:
//  pStr: A string to validate
//
// Return value: Boolean - Whether or not the string is a valid RIP "MegaNum".
//               A RIP MegaNum should only have digits 0-9 and letters A-Z (case-sensitive).
function isValidRIPMegaNumStr(pStr)
{
	return /^[0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ]+$/.test(pStr);
}

// Calculates the maximum numeric value for a base-36 MegaNum of
// a given number of digits.
//
// Parameters:
//  numDigits: The number of digits in the MegaNum
//
// Return value: The highest possible positive integer value
function getMaxMegaNumValue(pNumDigits)
{
	// The maximum value for a base-b number with n digits is (b^n) - 1.
	// MegaNums use base-36.
	return Math.pow(36, pNumDigits) - 1;
}

// Zero-pads a string to a specified width. Mainly for zero-padding
// RIP MegaNum strings.
//
// Parameters:
//  pStr: The string to pad
//  pPaddedStrLen: The length that the padded string should be. If this isn't a
//                 number, the original string will be returned.
//
// Return value: The padded string.  If the original string is longer than
//               pPaddedStrLen, it will simply be truncated.
function zeroPadStr(pStr, pPaddedStrLen)
{
	if (typeof(pStr) !== "string")
		return "";
	if (typeof(pPaddedStrLen) !== "number")
		return pStr;

	var padLen = (pPaddedStrLen >= 0 ? pPaddedStrLen : 0);
	var newStr = pStr;
	if (padLen > 0)
		newStr = newStr.substring(0, padLen);
	if (newStr.length < padLen)
		newStr = format("%2s", newStr).replace(/ /g, "0");
	return newStr;
}

// Verifies a RIP MegaNum string and returns a sanitized/verified version
//
// Parameters:
//  pNumStr: The RIP MegaNum string to verify/sanitize
//  pMaxLen: The maximum string length of the MegaNum string
//  pDefault: The default value (string) to use for the MegaNum in case pNumStr is invalid
//
// Return value: The verified & sanitized MegaNum string
function verifyMegaNumStr(pNumStr, pMaxLen, pDefault)
{
	if (typeof(pMaxLen) !== "number" || pMaxLen <= 0)
		return pNumStr;

	var numStr = (typeof(pDefault) === "string" ? pDefault : "0");
	if (typeof(pNumStr) === "string" && pNumStr.length <= pMaxLen)
	{
		var numStrUpper = pNumStr.toUpperCase();
		if (isValidRIPMegaNumStr(numStrUpper))
			numStr = zeroPadStr(numStrUpper, pMaxLen);
	}
	return numStr;
}

// Gets a filtered array of XY coordinates (objects with 'x' and 'y' properties)
// where the 'x' and 'y' properties are numbers whose values fit within MegaNum
// numbers of a specified number of digits.
//
// Parameters:
//  pXYPoints: An array of objects with 'x' and 'y' numeric values
//  pNumMegaNumDigits: The number of MegaNum digits that the values must fit in
//
// Return value: A filtered array containing only valid XY point objects
function filterArrayOfXYPoints(pXYPoints, pNumMegaNumDigits)
{
	if (!Array.isArray(pXYPoints) || typeof(pNumMegaNumDigits) !== "number")
		return [];

	// Filter the array
	const maxNumericVal = getMaxMegaNumValue(pNumMegaNumDigits);
	var points = pXYPoints.filter(function(item) {
		return (typeof(item) === "object" &&
		        item.hasOwnProperty("x") && item.hasOwnProperty("y") &&
		        typeof(item.x) === "number" && typeof(item.y) === "number" &&
		        item.x <= maxNumericVal && item.y <= maxNumericVal);
	});
	return points;
}

// Returns a RIP command string that's composed of a RIP command
// and a series of XY points. The values of the 'x' and 'y' propreties
// are expected to be numbers.
//
// Parameters:
//  pRIPCmd: The RIP command character
//  pXYPoints: An array of objects with 'x' and 'y' numeric values
//  pNumMegaNumDigitsForNPoints: For the number of points MegaNum value,
//                               the maximum number of MegaNum digits
//  pNumMegaNumDigitsForPoints: For the XY points, the number of MegaNum
//                              digits that the values must fit in
//
// Return value: The RIP command string
function getRIPCmdWithArrayOfXYPts(pRIPCmd, pXYPoints, pNumMegaNumDigitsForNPoints, pNumMegaNumDigitsForPoints)
{
	if (typeof(pRIPCmd) !== "string" || !Array.isArray(pXYPoints) || typeof(pNumMegaNumDigitsForNPoints) !== "number" || typeof(pNumMegaNumDigitsForPoints) !== "number")
		return "";

	// Sanity check the elements in pXYPoints and put together another array with elements that are objects
	// with 'x' and 'y' properties that are numbers that don't exceed
	// the maximum value of a 2-digit MegaNum
	var points = filterArrayOfXYPoints(pXYPoints, pNumMegaNumDigitsForPoints);
	// If the number of points exceeds the maximum value that can fit in
	// a 2-digit MegaNum string, then just return an empty string.
	if (points.length > getMaxMegaNumValue(pNumMegaNumDigitsForNPoints))
		return "";

	// Make a string for the default MegaNum string for each X & Y value
	const defaultXYVal = zeroPadStr("", pNumMegaNumDigitsForPoints);

	// Arguments:  npoints, x1, y1, ... xn, yn
	var rip = "|" + pRIPCmd;
	rip += toMegaNum(points.length, pNumMegaNumDigitsForPoints);
	for (var i = 0; i < points.length; ++i)
	{
		rip += verifyMegaNumStr(toMegaNum(points[i].x, pNumMegaNumDigitsForPoints), pNumMegaNumDigitsForPoints, defaultXYVal);
		rip += verifyMegaNumStr(toMegaNum(points[i].y, pNumMegaNumDigitsForPoints), pNumMegaNumDigitsForPoints, defaultXYVal);
	}
	return rip;
}

////////////////////////////////////////////////////
// RIP commands

// RIP_KILL_MOUSE_FIELDS
//          Function:  Destroys all previously defined hot mouse regions
//             Level:  1
//           Command:  K
//         Arguments:  <none>
//            Format:  !|1K
//           Example:  !|1K
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// This command will "forget" all Mouse Regions.  Use it at the beginning
// of each Scene, so that one scene's Mouse Regions don't get used in another.
function RIPKillMouseFields()
{
	return "|1K";
}

// RIP_RESET_WINDOWS
//          Function:  Clear Graphics/Text Windows & reset to full screen
//             Level:  0
//           Command:  *
//         Arguments:  <none>
//            Format:  !|*
//           Example:  !|*
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Sets the Text Window to a full 80x43 EGA hi-res text mode, places the cursor
// in the upper left corner, clears the screen, and zooms the Graphics Window to
// full 640x350 EGA screen.  Both windows are filled with the current graphics
// background color.  All Mouse Regions and Mouse Buttons are deleted and the
// Clipboard is erased.  Also restores the default 16-color RIP palette.
function RIPResetWindows()
{
	return "|*";
}

// RIP_ERASE_WINDOW
//          Function:  Clears Text Window to current background color
//             Level:  0
//           Command:  e
//         Arguments:  <none>
//            Format:  !|e
//           Example:  !|e
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Clears the TTY text window to the current graphics background color and
// positions the cursor in the upper-left corner of the window.  If the text
// window is inactive, this command is ignored.  If the text and graphics
// windows overlap, the overlapping portion is also cleared.
function RIPEraseTextWindow()
{
	return "|e";
}

// RIP_ERASE_VIEW
//          Function:  Clear Graphics Window to current background color
//             Level:  0
//           Command:  E
//         Arguments:  <none>
//            Format:  !|E
//           Example:  !|E
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Clears the Graphics Viewport to the current graphics background color.  If
// the graphics viewport is not active (boundaries are 0,0,0,0), this command
// is ignored.  If the text and graphics windows overlap, the overlapping
// portion is also cleared.
function RIPEraseGraphicsWindow()
{
	return "|E";
}

// RIP_HOME
//          Function:  Move cursor to upper-left corner of Text Window
//             Level:  0
//           Command:  H
//         Arguments:  <none>
//            Format:  !|H
//           Example:  !|H
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Positions the text cursor to the upper-left corner in the TTY Text Window,
// if it is active.
function RIPHome()
{
	return "|H";
}

// RIP_ERASE_EOL
//          Function:  Erase current line from cursor to end of line
//             Level:  0
//           Command:  >
//         Arguments:  <none>
//            Format:  !|>
//           Example:  !|>
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Erases the current text line in the TTY text window from the current cursor
// location (inclusive) to the end of the line.  The erased region is filled
// with the current graphics background color.
function RIPEraseEOL()
{
	return "|>";
}

// RIP_GOTOXY
//          Function:  Move text cursor to row & column in Text Window
//             Level:  0
//           Command:  g
//         Arguments:  x:2, y:2
//            Format:  !|g <x> <y>
//           Example:  !|g0509
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Sets the position of the text cursor in the TTY Text window, if it is active.
// If inactive (dimensions are 0,0,0,0), this command is ignored.  Coordinates
// are 0-based (unlike the ANSI/VT-100 goto which is 1-based).
//
// Parameters:
//  pX: 2-digit MegaNum (string) for the X column in the text window (0-based)
//  pY: 2-digit MegaNum (string) for the Y row in the text window (0-based)
//
// Return value: RIP command string
function RIPGotoXY(pX, pY)
{
	var rip = "|g";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	return rip;
}
// Same as RIPGotoXY(), but with numeric parameters rather than MegaNum strings.
function RIPGotoXYNumeric(pX, pY)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	return RIPGotoXY(x, y);
}

// RIP_WRITE_MODE
//          Function:  Set drawing mode for graphics primitives
//             Level:  0
//           Command:  W
//         Arguments:  mode:2
//            Format:  !|W <mode>
//           Example:  !|W00
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Sets the current drawing mode for most graphics primitives:
//   Mode   Description
//   ------------------------------------------
//   00     Normal drawing mode (overwrite)
//   01     XOR (complimentary) mode
//
// In XOR mode, each pixel is inverted rather than drawn in the drawing color.
// Drawing the same item a second time erases it completely.
//
// Parameters:
//  pMode: The RIP write mode:
//         Mode   Description
//         ------------------------------------------
//         00     Normal drawing mode (overwrite)
//         01     XOR (complimentary) mode
//
// Return value: A string to set RIP write mode
function RIPWriteMode(pMode)
{
	var rip = "|W";
	var mode = (typeof(pMode) === "number" && pMode >= 0 && pMode <= 1 ? pMode : 0);
	rip += format("%02d", mode);
	return rip;
}

// RIP_COLOR
//          Function:  Set current Drawing Color for graphics
//             Level:  0
//           Command:  c
//         Arguments:  color:2
//            Format:  !|c <color>
//           Example:  !|cA
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Sets the color for drawing lines, circles, arcs, rectangles, and other
// graphics primitives, as well as the color for drawing graphics-text.
// Does not affect Fill colors or Fill Patterns.
//
// Parameters:
//  pColor: A MegaNum string to specify the color:
//          Value   Color
//          -------------------------------------------------------
//          00      Black (00 is always the background color)
//          01      Blue
//          02      Green
//          03      Cyan
//          04      Red
//          05      Magenta
//          06      Brown
//          07      Light Gray
//          08      Dark Gray
//          09      Light Blue
//          0A      Light Green
//          0B      Light Cyan
//          0C      Light Red
//          0D      Light Magenta
//          0E      Yellow
//          0F      White
//
// Return value: A string to set the RIP color
function RIPColor(pColor)
{
	var rip = "|c";
	rip += verifyMegaNumStr(pColor, 2, "00");
	return rip;
}
// Same as RIPColor(), but with numeric parameters rather than MegaNum strings.
function RIPColorNumeric(pColor)
{
	var colorNum = 0;
	if (typeof(pColor) === "number" && pColor >= 0 && pColor <= 15)
		colorNum = toMegaNum(pColor, 2);
	return RIPColor(colorNum);
}

// RIP_FONT_STYLE
//          Function:  Select current font style, orientation and size
//             Level:  0
//           Command:  Y
//         Arguments:  font:2, direction:2, size:2, res:2
//            Format:  !|Y <font> <direction> <size> <res>
//           Example:  !|Y01000400
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  YES
//     Uses Viewport:  NO
//
// Sets the font, direction, and size for RIP_TEXT commands.  Direction: 00=horizontal,
// 01=vertical.  Size: 01=normal, 02=x2 magnification, ..., 0A=x10.  Font 00 is
// bit-mapped (best at size 1); fonts 01-0A are scalable vector fonts.
//
// Parameters:
//  pFont: A MegaNum string defining the font:
//         Font   Description of Font
//         ---------------------------------------------
//         00     Default 8x8 font            Bit-Mapped
//         01     Triplex Font                Scalable
//         02     Small Font                  Scalable
//         03     Sans Serif Font             Scalable
//         04     Gothic [Old English] Font   Scalable
//         05     Script Font                 Scalable
//         06     Simplex Font                Scalable
//         07     Triplex Script Font         Scalable
//         08     Complex Font                Scalable
//         09     European Font               Scalable
//         0A     Bold Font                   Scalable
//  pDirection: MegaNum string; Use 00 to indicate horizontal and 01 for vertical
//  pSize: MegaNum string; use 01 for the normal default size, 02 for x2
//         magnification, 03 for x3 magnification, ... , and 0A for x10
//         magnification.
//  pRes: Reserved
//
// Return value: A string that defines a RIP font style according to the parameters
function RIPFontStyle(pFont, pDirection, pSize, pRes)
{
	/*
	 *        Arguments:  font:2, direction:2, size:2, res:2
	 *           Format:  !|Y <font> <direction> <size> <res>
	 *          Example:  !|Y01000400
	 *  Uses Draw Color:  NO
	 * Uses Line Pattern:  NO
	 *  Uses Line Thick:  NO
	 *  Uses Fill Color:  NO
	 * Uses Fill Pattern:  NO
	 *  Uses Write Mode:  NO
	 *  Uses Font Sizes:  YES
	 *    Uses Viewport:  NO
	 */
	var rip = "|Y";
	rip += verifyMegaNumStr(pFont, 2, "00");
	rip += verifyMegaNumStr(pDirection, 2, "00");
	rip += verifyMegaNumStr(pSize, 2, "01");
	rip += verifyMegaNumStr(pRes, 2, "00");
	return rip;
}
// Same as RIPFontStyle(), but with numeric parameters rather than MegaNum strings.
function RIPFontStyleNumeric(pFont, pDirection, pSize, pRes)
{
	var font = (typeof(pFont) === "number" ? toMegaNum(pFont, 2) : "00");
	var direction = (typeof(pDirection) === "number" ? toMegaNum(pDirection, 2) : "00");
	var size = (typeof(pSize) === "number" ? toMegaNum(pSize, 2) : "01");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 2) : "00");
	return RIPFontStyle(font, direction, size, res);
}

// RIP_TEXT
//          Function:  Draw text in current font/color at current spot
//             Level:  0
//           Command:  T
//         Arguments:  text-string
//            Format:  !|T <text-string>
//           Example:  !|Thello world
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  YES
//     Uses Viewport:  YES
//
// Displays text at the current graphics drawing position (as set with RIP_MOVE).
// Drawing position is placed at the end of the last character drawn, enabling
// subsequent RIP_TEXT commands to continue where the previous one left off.
//
// Parameters:
//  pText: The text string to Draw
//
// Return value: RIP command string for drawing the text
function RIPText(pText)
{
	return "|T" + pText;
}

// RIP_TEXT_XY
//          Function:  Draw text in current font/color at specific spot
//             Level:  0
//           Command:  @
//         Arguments:  x:2, y:2 and text-string
//            Format:  !|@ <x> <y> <text-string>
//           Example:  !|@0011hello world
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  YES
//     Uses Viewport:  YES
//
// Efficient combination of RIP_MOVE and RIP_TEXT.  Draws text at the specified
// location using the current font, color, and write mode settings.
// The drawing position is set to the right of the last character drawn.
//
// Parameters:
//  pX0: A 2-digit MegaNum (string) for the X value of the text location (2 digits)
//  pY0: A 2-digit MegaNum (string) for the Y value of the text location (2 digits)
//  pText: The text to write
//
// Return value: A string defining a RIP_TEXT_XY command to draw the specified
//               text at the specified location
function RIPTextXY(pX, pY, pText)
{
	if (typeof(pText) !== "string")
		return "";

	var rip = "|@";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += pText;
	return rip;

}
// Same as RIPTextXY(), but with numeric parameters rather than MegaNum strings.
function RIPTextXYNumeric(pX, pY, pText)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	return RIPTextXY(x, y, pText);
}

// RIP_BUTTON_STYLE
//          Function:  Button style definition
//             Level:  1
//           Command:  B
//         Arguments:  wid:2 hgt:2 orient:2 flags:4 bevsize:2
//                     dfore:2 dback:2 bright:2 dark:2 surface:2
//                     grp_no:2 flags2:2 uline_col:2 corner_col:2
//                     res:6
//            Format:  !|1B <wid> <hgt> <orient> <flags> <bevsize>
//                     <dfore> <dback> <bright> <dark> <surface>
//                     <grp_no> <flags2> <uline_col> <corner_col> <res>
//           Example:  !|1B0A0A010274030F080F080700010E07000000
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Defines how subsequent RIP_BUTTON commands will be rendered.  This command
// does not draw anything on screen; it creates an internal Button style
// definition.  Three button types exist: Icon (flag 128), Clipboard (flag 1),
// and Plain (flag 256).  The <wid>/<hgt> parameters set fixed size (Plain Buttons
// only); if zero, the RIP_BUTTON command specifies size dynamically.  The
// <orient> parameter sets text label position (00=above, 01=left, 02=center,
// 03=right, 04=below).  The <grp_no> parameter (0-35) assigns subsequent buttons
// to a button group (for Radio/Checkbox behavior).
//
// Parameters:
//  pWidth: 2-digit MegaNum (string) for the button width (Plain Buttons only; 0=dynamic)
//  pHeight: 2-digit MegaNum (string) for the button height (Plain Buttons only; 0=dynamic)
//  pLabelOrientation: 2-digit MegaNum (string) for the text label position:
//                     00=above, 01=left, 02=center, 03=right, 04=below
//  pFlags: 4-digit MegaNum (string) for button style flags (see RIP_BTN_STYLE_* constants)
//  pBevelSize: 2-digit MegaNum (string) for the bevel size in pixels
//  pTxtLblDFore: 2-digit MegaNum (string) for the text label default foreground color
//  pTxtLblDBack: 2-digit MegaNum (string) for the text label default background color
//  pBright: 2-digit MegaNum (string) for the bright bevel color
//  pDark: 2-digit MegaNum (string) for the dark bevel color
//  pSurface: 2-digit MegaNum (string) for the button surface color
//  pGrpNum: 2-digit MegaNum (string) for the button group number (0-35) for Radio/Checkbox behavior
//  pFlags2: 2-digit MegaNum (string) for secondary flags
//  pULineCol: 2-digit MegaNum (string) for the underline color (for hot-key underlining)
//  pCornerCol: 2-digit MegaNum (string) for the corner color
//  pRes: Reserved (2-digit MegaNum string)
//
// Return value: RIP command string
function RIPButtonStyle(pWidth, pHeight, pLabelOrientation, pFlags, pBevelSize, pTxtLblDFore,
                        pTxtLblDBack, pBright, pDark, pSurface, pGrpNum, pFlags2, pULineCol,
                        pCornerCol, pRes)
{
	var lblOrientation = "00";
	if (typeof(pLabelOrientation) === "number" && pLabelOrientation >= 0 && pLabelOrientation <= 4)
		lblOrientation = verifyMegaNumStr(toMegaNum(pLabelOrientation, 2), 2, "00");
	else if (typeof(pLabelOrientation) === "string")
		lblOrientation = verifyMegaNumStr(pLabelOrientation, 2, "00");
	var rip = "|1B";
	rip += verifyMegaNumStr(pWidth, 2, "00");
	rip += verifyMegaNumStr(pHeight, 2, "00");
	rip += lblOrientation;
	rip += verifyMegaNumStr(pFlags, 4, "0PX4");
	rip += verifyMegaNumStr(pBevelSize, 2, "00");
	rip += verifyMegaNumStr(pTxtLblDFore, 2, "00");
	rip += verifyMegaNumStr(pTxtLblDBack, 2, "00");
	rip += verifyMegaNumStr(pBright, 2, "00");
	rip += verifyMegaNumStr(pDark, 2, "00");
	rip += verifyMegaNumStr(pSurface, 2, "00");
	rip += verifyMegaNumStr(pGrpNum, 2, "00");
	rip += verifyMegaNumStr(pFlags2, 2, "00");
	rip += verifyMegaNumStr(pULineCol, 2, "00");
	rip += verifyMegaNumStr(pCornerCol, 2, "00");
	rip += verifyMegaNumStr(pRes, 2, "00"); // Reserved for future use

	return rip;
}
// Same as RIPButtonStyle(), but with numeric parameters rather than MegaNum strings.
function RIPButtonStyleNumeric(pWidth, pHeight, pLabelOrientation, pFlags, pBevelSize, pTxtLblDFore,
                               pTxtLblDBack, pBright, pDark, pSurface, pGrpNum, pFlags2, pULineCol,
                               pCornerCol, pRes)
{
	var width = (typeof(pWidth) === "number" ? toMegaNum(pWidth, 2) : "00");
	var height = (typeof(pHeight) == "number" ? toMegaNum(pHeight, 2) : "00");
	var lblOrientation = 0;
	if (typeof(pLabelOrientation) === "number" && pLabelOrientation >= 0 && pLabelOrientation <= 4)
		lblOrientation = pLabelOrientation;
	var flags = (typeof(pFlags) === "number" ? toMegaNum(pFlags, 4) : "0000")
	var bevelSize = (typeof(pBevelSize) === "number" ? toMegaNum(pBevelSize, 2) : "00");
	var txtLblDFore = (typeof(pTxtLblDFore) === "number" ? toMegaNum(pTxtLblDFore, 2) : "00");
	var txtLblDBack = (typeof(pTxtLblDBack) === "number" ? toMegaNum(pTxtLblDBack, 2) : "00");
	var bright = (typeof(pBright) === "number" ? toMegaNum(pBright, 2) : "00");
	var dark = (typeof(pDark) === "number" ? toMegaNum(pDark, 2) : "00");
	var surface = (typeof(pSurface) === "number" ? toMegaNum(pSurface, 2) : "00");
	var grpNum = (typeof(pGrpNum) === "number" ? toMegaNum(pGrpNum, 2) : "01");
	var flags2 = (typeof(pFlags2) === "number" ? toMegaNum(pFlags2, 2) : "00");
	var ulineCol = (typeof(pULineCol) === "number" ? toMegaNum(pULineCol, 2) : "00");
	var cornerCol = (typeof(pCornerCol) === "number" ? toMegaNum(pCornerCol, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 6) : "000000");
	return RIPButtonStyle(width, height, lblOrientation, flags, bevelSize, txtLblDFore, txtLblDBack,
						  bright, dark, surface, grpNum, flags2, ulineCol, cornerCol, res);
}

// RIP_TEXT_WINDOW
//          Function:  Define the size and location of the Text Window
//             Level:  0
//           Command:  w
//         Arguments:  x0:2, y0:2, x1:2, y1:2, wrap:1, size:1
//            Format:  !|w <x0> <y0> <x1> <y1> <wrap> <size>
//           Example:  !|w00001B0M10
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Specifies the dimensions of the virtual TTY window for ASCII/ANSI (non-RIPscrip)
// data.  (x0,y0) is the upper-left and (x1,y1) is the lower-right corner in
// text-cell coordinates.  Setting all parameters to zero makes the window invisible
// (ignores non-RIPscrip bytes).  The <wrap> parameter (1=wrap/scroll, 0=truncate)
// applies to both dimensions.  The <size> parameter governs the font used for output.
//
// Size   Font Size   X Range  Y Range
// ------------------------------------------
//  0     8x8          0-79     0-42
//  1     7x8          0-90     0-42
//  2     8x14         0-79     0-24
//  3     7x14         0-90     0-24
//  4     16x14        0-39     0-24
//
// Parameters:
//  pX0: 2-digit A MegaNum (string) for the X value of the top-left coordinate. Defaults to 0.
//  pY0: 2-digit A MegaNum (string) for the Y value of the top-left coordinate. Defaults to 0.
//  pX1: 2-digit A MegaNum (string) for the X value of the lower-right coordinate. Defaults to 27.
//  pY1: 2-digit A MegaNum (string) for the Y value of the lower-right coordinate. Defaults to 16.
//  pWrap: Boolean/number/MegaNum; Whether or not to wrap text (true/false or 1/0). Defaults to true/1.
//  pSize: A MegaNum (string) to set the font size (defaults to 0):
//         Size  Font Size   X Range  Y Range
//         ------------------------------------------
//         0     8x8          0-79     0-42
//         1     7x8          0-90     0-42
//         2     8x14         0-79     0-24
//         3     7x14         0-90     0-24
//         4     16x14        0-39     0-24
//
// Return value: A string to define a RIP window
function RIPWindow(pX0, pY0, pX1, pY1, pWrap, pSize)
{
	var wrap = "1";
	if (typeof(pWrap) === "boolean")
		wrap = (pWrap ? "1" : "0");
	else if (typeof(pWrap) === "number")
		wrap = verifyMegaNumStr(toMegaNum(pWrap, 1), 1, "1");
	else if (typeof(pWrap) === "string")
		wrap = verifyMegaNumStr(pWrap, 1, "1");

	var rip = "|w";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	rip += wrap;
	rip += verifyMegaNumStr(pSize, 1, "0");
	return rip;
}
// Same as RIPWindow(), but with numeric parameters rather than MegaNum strings.
function RIPWindowNumeric(pX0, pY0, pX1, pY1, pWrap, pSize)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var wrap = (typeof(pWrap) === "boolean" ? pWrap : true);
	var size = (typeof(pSize) === "number" ? toMegaNum(pSize, 1) : "0");
	return RIPWindow(x0, y0, x1, y1, wrap, size);
}

// RIP_BUTTON
//          Function:  Define a Mouse Button
//             Level:  1
//           Command:  U
//         Arguments:  x0:2 y0:2 x1:2 y1:2 hotkey:2 flags:1 res:1
//                     ...text
//            Format:  !|1U <x0> <y0> <x1> <y1> <hotkey> <flags>
//                     <res> <text>
//           Example:  !|1U010100003200iconfile<>Label<>HostCmd^m
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  YES
//     Uses Viewport:  YES
//
// Creates a new Button using the style defined by the most recent
// RIP_BUTTON_STYLE command.  (x0,y0) is the upper-left corner; (x1,y1) is
// only used for dynamically-sized Plain Buttons.  The <hotkey> parameter is the
// ASCII code for the keystroke that activates the button (255="any key").
// The <flags> parameter: 1=draw as already selected, 2=default on Enter.
// The <text> parameter uses "<>" delimiters:
//   [icon-file][<>text-label][<>host-command]
//
// Parameters:
//  pX0: 2-digit MegaNum (string) for the X value of the upper-left corner
//  pY0: 2-digit MegaNum (string) for the Y value of the upper-left corner
//  pX1: 2-digit MegaNum (string) for the X value of the lower-right corner (used for dynamically-sized Plain Buttons)
//  pY1: 2-digit MegaNum (string) for the Y value of the lower-right corner (used for dynamically-sized Plain Buttons)
//  pHotkey: 2-digit MegaNum (string) for the ASCII code of the keystroke that activates the button (FF=any key)
//  pFlags: 1-digit MegaNum (string): 1=draw as already selected, 2=default on Enter
//  pRes: Reserved (1-digit MegaNum string)
//  pTextStrs: Array of strings for the button text blocks (icon filename, text label, host command).
//             Blocks are delimited by "<>" in the RIP command.
//  pHostCmd: String for the host command sent when the button is activated
//
// Return value: RIP command string
function RIPButton(pX0, pY0, pX1, pY1, pHotkey, pFlags, pRes, pTextStrs, pHostCmd)
{
	/*
	The <text> parameter for this command is somewhat different than
	those found in previously described RIPscrip commands.  All other
	RIPscrip commands only have one text parameter.  This command
	requires  anywhere from 0-3 text parameters.  The way RIPscrip
	accomplishes this is by separating each block in the <text> parameter
	with the delimiter "<>".  This text parameter delimiter is not needed
	before the first text block, but is necessary between the 1st and 2nd
	blocks, and the 2nd and 3rd blocks.  Here is an example of a typical
	text parameter for this command:

	ICONFILE.ICN<>TEXT LABEL<>HOST COMMAND

	The actual syntax of this text parameter is as follows:

	[icon-file][[<>text-label][<>host-command]]
	*/
	var rip = "|1U";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	rip += verifyMegaNumStr(pHotkey, 2, "00");
	rip += verifyMegaNumStr(pFlags, 1, "0");
	rip += verifyMegaNumStr(pRes, 1, "0");
	rip += "<>";
	var addedTextStrs = false;
	if (Array.isArray(pTextStrs))
	{
		for (var i = 0; i < pTextStrs.length; ++i)
		{
			if (typeof(pTextStrs[i] === "string"))
			{
				rip += pTextStrs[i] + "<>";
				addedTextStrs = true;
			}
		}
	}
	if (!addedTextStrs)
		rip += "<>";
	if (typeof(pHostCmd) === "string")
		rip += pHostCmd;
	return rip;
}
// Same as RIPButton(), but with numeric parameters rather than MegaNum strings.
function RIPButtonNumeric(pX0, pY0, pX1, pY1, pHotkey, pFlags, pRes, pTextStrs, pHostCmd)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var hotkey = (typeof(pHotkey) === "number" ? toMegaNum(pHotkey, 2) : "00");
	var flags = (typeof(pFlags) === "number" ? toMegaNum(pFlags, 1) : "0");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 1) : "0");
	return RIPButton(x0, y0, x1, y1, hotkey, flags, res, pTextStrs, pHostCmd);
}

// RIP_VIEWPORT
//          Function:  Define the size & location of the Graphics Window
//             Level:  0
//           Command:  v
//         Arguments:  x0:2, y0:2, x1:2, y1:2
//            Format:  !|v <x0> <y0> <x1> <y1>
//           Example:  !|v00002E1M
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Defines the (X,Y) pixel boundaries of the RIPscrip graphics window.
// (x0,y0) is the upper-left corner and (x1,y1) is the lower-right corner
// (inclusive).  Coordinates are based on a 640x350 pixel resolution.
// Setting all parameters to zero disables the graphics viewport.
// Graphics outside the viewport are clipped (truncated at the border).
//
// Parameters:
//  pX0: 2-digit MegaNum (string) for the X value of the upper-left corner. Defaults to 0.
//  pY0: 2-digit MegaNum (string) for the Y value of the upper-left corner. Defaults to 0.
//  pX1: 2-digit MegaNum (string) for the X value of the lower-right corner. Defaults to "2E" (630).
//  pY1: 2-digit MegaNum (string) for the Y value of the lower-right corner. Defaults to "1M" (58).
//
// Return value: RIP command string
function RIPViewport(pX0, pY0, pX1, pY1)
{
	var rip = "|B";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "2E");
	rip += verifyMegaNumStr(pY1, 2, "1M");
	return rip;
}
// Same as RIPViewport(), but with numeric parameters rather than MegaNum strings.
function RIPViewportNumeric(pX0, pY0, pX1, pY1)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "2E");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "1M");
	return RIPViewport(x0, y0, x1, y1);
}

// RIP_SET_PALETTE
//          Function:  Set 16-color Palette from master 64-color palette
//             Level:  0
//           Command:  Q
//         Arguments:  c1:2, c2:2, ... c16:2
//            Format:  !|Q <c1> <c2> ... <c16>
//           Example:  !|Q000102030405060708090A0B0C0D0E0F
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Modifies the 16-color RIP palette by choosing from the 64 colors in the
// master EGA palette.  Colors on screen change instantly when this command is
// processed.  Color 00 is always the background color.  The default palette
// is restored by RIP_RESET_WINDOWS.
//
// Parameters:
//  pC1, pC2 ..., pC16: MegaNum strings representing color codes
//
// Return value: RIP command string for setting the 16-color palette
function RIPSetPalette(pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pC9, pC10, pC11, pC12, pC13, pC14, pC15, pC16)
{
	var vals = [pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pC9, pC10, pC11, pC12, pC13, pC14, pC15, pC16];
	var rip = "|Q";
	for (var i = 0; i < vals.length; ++i)
		rip += verifyMegaNumStr(vals[i], 2, "00");
	return rip;
}
// Same as RIPSetPalette(), but with numeric parameters rather than MegaNum strings.
function RIPSetPaletteNumeric(pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pC9, pC10, pC11, pC12, pC13, pC14, pC15, pC16)
{
	var vals = [pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pC9, pC10, pC11, pC12, pC13, pC14, pC15, pC16];
	for (var i = 0; i < vals.length; ++i)
		vals[i] = (typeof(vals[i]) === "number" ? toMegaNum(vals[i], 2) : "00");
	return RIPSetPalette(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7], vals[8], vals[9],
	                     vals[10], vals[11], vals[12], vals[13], vals[14], vals[15]);
}

// RIP_ONE_PALETTE
//          Function:  Set color of 16-color Palette from Master Palette
//             Level:  0
//           Command:  a
//         Arguments:  color:2 value:2
//            Format:  !|a <color> <value>
//           Example:  !|a051B
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Changes one color in the 16-color palette by selecting from the 64-color
// master EGA palette.  The <color> number is the RIP palette entry to change
// (0-15); the <value> is the master palette color code (0-63).  The change
// takes effect instantly on-screen.  The default palette is restored by
// RIP_RESET_WINDOWS.
//
// Parameters:
//  pColor: Color number, as a MegaNum string
//  pValue: The color code value (see the table of color codes listed for RIPSetPallette())
//
// Return value: RIP command string for setting the palette color
function RIPOnePalette(pColor, pValue)
{
	var rip = "|a";
	rip += verifyMegaNumStr(pColor, 2, "00");
	rip += verifyMegaNumStr(pValue, 2, "00");
	return rip;
}
// Same as RIPOnePalette(), but with numeric parameters rather than MegaNum strings.
function RIPOnePaletteNumeric(pColor, pValue)
{
	var color = (typeof(pColor) === "number" ? toMegaNum(pColor, 2) : "00");
	var value = (typeof(pColor) === "number" ? toMegaNum(pValue, 2) : "00");
	return RIPOnePalette(color, value);
}

// RIP_MOVE
//          Function:  Move the current drawing position to (X,Y)
//             Level:  0
//           Command:  m
//         Arguments:  x:2, y:2
//            Format:  !|m <x> <y>
//           Example:  !|m0509
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Moves the current graphics drawing cursor to (x,y) without drawing anything.
// Primarily used to position the starting point for subsequent RIP_TEXT commands.
//
// Parameters:
//  pX: 2-digit MegaNum (string) for the X coordinate to move to
//  pY: 2-digit MegaNum (string) for the Y coordinate to move to
//
// Return value: RIP command string
function RIPMove(pX, pY)
{
	var rip = "|m";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	return rip;
}
// Same as RIPMove(), but with numeric parameters rather than MegaNum strings.
function RIPMoveNumeric(pX, pY)
{
	var x = (typeof(pColor) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pColor) === "number" ? toMegaNum(pY, 2) : "00");
	return RIPMove(x, y);
}

// RIP_PIXEL
//          Function:  Draws a one pixel using current drawing color
//             Level:  0
//           Command:  X
//         Arguments:  x:2, y:2
//            Format:  !|X <x> <y>
//           Example:  !|X1122
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a single pixel in the current drawing color at the given (x,y) graphics
// position.  Included for completeness; using it extensively would be inefficient.
//
// Parameters:
//  pX: A 2-digit MegaNum (string) for the X coordinate of the pixel
//  pY: A 2-digit MegaNum (string) for the Y coordinate of the pixel
//
// Return value: RIP command string
function RIPPixel(pX, pY)
{
	var rip = "|X";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	return rip;
}
// Same as RIPPixel(), but with numeric parameters rather than MegaNum strings.
function RIPPixelNumeric(pX, pY)
{
	var x = (typeof(pColor) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pColor) === "number" ? toMegaNum(pY, 2) : "00");
	RIPPixel(x, y);
}

// RIP_LINE
//          Function:  Draw a line in the current color/line style
//             Level:  0
//           Command:  L
//         Arguments:  x0:2, y0:2, x1:2, y1:2
//            Format:  !|L <x0> <y0> <x1> <y1>
//           Example:  !|L00010A0E
//   Uses Draw Color:  YES
// Uses Line Pattern:  YES
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a line in the current drawing color, using the current line style,
// pattern and thickness, from (x0,y0) to (x1,y1).
//
// Parameters:
//  pX0: A 2-digit MegaNum (string) for the X value of the start coordinate. Defaults to 0.
//  pY0: A 2-digit MegaNum (string) for the Y value of the start coordinate. Defaults to 0.
//  pX1: A 2-digit MegaNum (string) for the X value of the end coordinate. Defaults to 0.
//  pY1: A 2-digit MegaNum (string) for the Y value of the end coordinate. Defaults to 0.
//
// Return value: RIP command string
function RIPLine(pX0, pY0, pX1, pY1)
{
	var rip = "|L";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	return rip;
}
// Same as RIPLine(), but with numeric parameters rather than MegaNum strings.
function RIPLineNumeric(pX0, pY0, pX1, pY1)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	return RIPLine(x0, y0, x1, y1);
}

// RIP_RECTANGLE
//          Function:  Draw a rectangle in current color/line style
//             Level:  0
//           Command:  R
//         Arguments:  x0:2, y0:2, x1:2, y1:2
//            Format:  !|R <x0> <y0> <x1> <y1>
//           Example:  !|R00010A0E
//   Uses Draw Color:  YES
// Uses Line Pattern:  YES
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a rectangle in the current drawing color, using the current line style,
// pattern and thickness.  (x0,y0) and (x1,y1) are any two opposing corners.
// If x0=x1 or y0=y1, draws a single vertical or horizontal line.  Interior
// is not filled.
//
// Parameters:
//  pX0: A 2-digit MegaNum (string) for the X value of the top-left coordinate. Defaults to 0.
//  pY0: A 2-digit MegaNum (string) for the Y value of the top-left coordinate. Defaults to 0.
//  pX1: A 2-digit MegaNum (string) for the X value of the lower-right coordinate. Defaults to 0.
//  pY1: A 2-digit MegaNum (string) for the Y value of the lower-right coordinate. Defaults to 0.
//
// Return value: RIP command string
function RIPRectangle(pX0, pY0, pX1, pY1)
{
	var rip = "|R";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	return rip;
}
// Same as RIPRectangle(), but with numeric parameters rather than MegaNum strings.
function RIPRectangleNumeric(pX0, pY0, pX1, pY1)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	return RIPRectangle(x0, y0, x1, y1);
}

// RIP_BAR
//          Function:  Draw filled rectangle with fill color/pattern
//             Level:  0
//           Command:  B
//         Arguments:  x0:2, y0:2, x1:2, y1:2
//            Format:  !|B <x0> <y0> <x1> <y1>
//           Example:  !|B00010A0E
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Fills a rectangular region with the current fill color and pattern.
// No border is drawn.
//
// Parameters:
//  pX0: A 2-digit MegaNum (string) for the X value of the top-left coordinate. Defaults to 0.
//  pY0: A 2-digit MegaNum (string) for the Y value of the top-left coordinate. Defaults to 0.
//  pX1: A 2-digit MegaNum (string) for the X value of the lower-right coordinate. Defaults to 0.
//  pY1: A 2-digit MegaNum (string) for the Y value of the lower-right coordinate. Defaults to 0.
//
// Return value: RIP command string
function RIPBar(pX0, pY0, pX1, pY1)
{
	var rip = "|B";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	return rip;
}
// Same as RIPBar(), but with numeric parameters rather than MegaNum strings.
function RIPBarNumeric(pX0, pY0, pX1, pY1)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	return RIPBar(x0, y0, x1, y1);
}

// RIP_CIRCLE
//          Function:  Draw circle in current color and line thickness
//             Level:  0
//           Command:  C
//         Arguments:  x_center:2, y_center:2, radius:2
//            Format:  !|C <x_center> <y_center> <radius>
//           Example:  !|C1E180M
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a circle in the current drawing color and line thickness.  Understands
// aspect ratios (based on EGA 640x350 resolution) and will draw a truly circular
// circle.  Uses line thickness but not line patterns.
//
// Parameters:
//  pXCenter: X coordinate for the center of the circle (as a 2-digit MegaNum string)
//  pYCenter: Y coordinate for the center of the circle (as a 2-digit MegaNum string)
//  pRadius: Radius of the circle (as a MegaNum string)
//
// Return value: RIP command string
function RIPCircle(pXCenter, pYCenter, pRadius)
{
	var rip = "|C";
	rip += verifyMegaNumStr(pXCenter, 2, "00");
	rip += verifyMegaNumStr(pYCenter, 2, "00");
	rip += verifyMegaNumStr(pRadius, 2, "00");
	return rip;
}
// Same as RIPCircle(), but with numeric parameters rather than MegaNum strings.
function RIPCircleNumeric(pXCenter, pYCenter, pRadius)
{
	var XCenter = (typeof(pXCenter) === "number" ? toMegaNum(pXCenter, 2) : "00");
	var YCenter = (typeof(pYCenter) === "number" ? toMegaNum(pYCenter, 2) : "00");
	var radius = (typeof(pRadius) === "number" ? toMegaNum(pX0, 2) : "00");
	return RIPCircle(XCenter, YCenter, radius);
}

// RIP_OVAL
//          Function:  Draw elliptical arc in current color/line style
//             Level:  0
//           Command:  O
//         Arguments:  x:2, y:2, st_ang:2, end_ang:2, x_rad:2, y_rad:2
//            Format:  !|O <x> <y> <st_ang> <end_ang> <x_rad> <y_rad>
//           Example:  !|O1E1A18003G150Z
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws an elliptical arc.  The center is at (x,y); the arc is drawn from
// <st_ang> counterclockwise to <end_ang> (0=3 o'clock, 90=12 o'clock, etc.).
// X radius is half the full width; Y radius is half the full height.
// Uses line thickness but not line patterns.
//
// Parameters:
//  pXCenter: X coordinate for the center of the oval (as a 2-digit MegaNum string)
//  pYCenter: Y coordinate for the center of the oval (as a 2-digit MegaNum string)
//  pStartAng: Arc start angle (2-digit MegaNum string; 0=3 o'clock, 90=12 o'clock)
//  pEndAng: Arc end angle (2-digit MegaNum string; arc drawn counterclockwise from start to end)
//  pXRad: X radius (2-digit MegaNum string)
//  pYRad: Y radius (2-digit MegaNum string)
//
// Return value: RIP command string
function RIPOval(pX, pY, pStartAng, pEndAng, pXRad, pYRad)
{
	var rip = "|O";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pStartAng, 2, "00");
	rip += verifyMegaNumStr(pEndAng, 2, "00");
	rip += verifyMegaNumStr(pXRad, 2, "00");
	rip += verifyMegaNumStr(pYRad, 2, "00");
	return rip;
}
// Same as RIPOval(), but with numeric parameters rather than MegaNum strings.
function RIPOvalNumeric(pX, pY, pStartAng, pEndAng, pXRad, pYRad)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var startAng = (typeof(pStartAng) === "number" ? toMegaNum(pStartAng, 2) : "00");
	var endAng = (typeof(pEndAng) === "number" ? toMegaNum(pEndAng, 2) : "00");
	var xRad = (typeof(pXRad) === "number" ? toMegaNum(pXRad, 2) : "00");
	var yRad = (typeof(pYRad) === "number" ? toMegaNum(pYRad, 2) : "00");
	return RIPOval(x, y, startAng, endAng, xRad, yRad);
}

// RIP_FILLED_OVAL
//          Function:  Draw filled ellipse using current color/pattern
//             Level:  0
//           Command:  o
//         Arguments:  x_center:2, y_center:2, x_rad:2, y_rad:2
//            Format:  !|o <x_center> <y_center> <x_rad> <y_rad>
//           Example:  !|o1G2B0M0G
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  YES
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a complete filled ellipse.  The interior is drawn using the current
// fill pattern and fill color.  The outline is drawn using the current drawing
// color and line thickness.
//
// Parameters:
//  pXCenter: X coordinate for the center of the oval (as a 2-digit MegaNum string)
//  pYCenter: Y coordinate for the center of the oval (as a 2-digit MegaNum string)
//  pXRad: X radius (2-digit MegaNum string)
//  pYRad: Y radius (2-digit MegaNum string)
//
// Return value: RIP command string
function RIPFilledOval(pXCenter, pYCenter, pXRad, pYRad)
{
	var rip = "|o";
	rip += verifyMegaNumStr(pXCenter, 2, "00");
	rip += verifyMegaNumStr(pYCenter, 2, "00");
	rip += verifyMegaNumStr(pXRad, 2, "00");
	rip += verifyMegaNumStr(pYRad, 2, "00");
	return rip;
}
// Same as RIPFilledOval(), but with numeric parameters rather than MegaNum strings.
function RIPFilledOvalNumeric(pXCenter, pYCenter, pXRad, pYRad)
{
	var xCenter = (typeof(pXCenter) === "number" ? toMegaNum(pXCenter, 2) : "00");
	var yCenter = (typeof(pYCenter) === "number" ? toMegaNum(pYCenter, 2) : "00");
	var xRad = (typeof(pXRad) === "number" ? toMegaNum(pXRad, 2) : "00");
	var yRad = (typeof(pYRad) === "number" ? toMegaNum(pYRad, 2) : "00");
	return RIPFilledOval(xCenter, yCenter, xRad, yRad);
}

// RIP_ARC
//          Function:  Draw circular arc in current color/line thickness
//             Level:  0
//           Command:  A
//         Arguments:  x:2, y:2, start_ang:2, end_ang:2, radius:2
//            Format:  !|A <x> <y> <start_ang> <end_ang> <radius>
//           Example:  !|A1E18003G15
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a circular arc from <start_ang> counterclockwise to <end_ang>.
// Angles: 0=3 o'clock, 90=12 o'clock, 180=9 o'clock, 270=6 o'clock.
// A full circle: start_ang=0, end_ang=360.  Aspect-ratio aware.
// Uses line thickness but not line patterns.  If both angles are equal, nothing is drawn.
//
// Parameters:
//  pX: X coordinate for the start of the arc (as a 2-digit MegaNum string)
//  pY: Y coordinate for the start of the arc (as a 2-digit MegaNum string)
//  pStartAng: Arc start angle
//  pEndAng: Arc end angle
//  pRadius: The radius (2-digit MegaNum string)
//
// Return value: RIP command string
function RIPArc(pX, pY, pStartAng, pEndAng, pRadius)
{
	var rip = "|A";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pStartAng, 2, "00");
	rip += verifyMegaNumStr(pEndAng, 2, "00");
	rip += verifyMegaNumStr(pRadius, 2, "00");
	return rip;
}
// Same as RIPArc(), but with numeric parameters rather than MegaNum strings.
function RIPArcNumeric(pX, pY, pStartAng, pEndAng, pRadius)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var startAng = (typeof(pStartAng) === "number" ? toMegaNum(pStartAng, 2) : "00");
	var endAng = (typeof(pEndAng) === "number" ? toMegaNum(pEndAng, 2) : "00");
	var radius = (typeof(pRadius) === "number" ? toMegaNum(pRadius, 2) : "00");
	return RIPArc(x, y, startAng, endAng, radius);
}

// RIP_OVAL_ARC
//          Function:  Draw an elliptical arc
//             Level:  0
//           Command:  V
//         Arguments:  x:2, y:2, st_ang:2, e_ang:2, radx:2, rady:2
//            Format:  !|V <x> <y> <st_ang> <e_ang> <radx> <rady>
//           Example:  !|V1E18003G151Q
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws an elliptical arc from <st_ang> counterclockwise to <e_ang>.
// Angles: 0=3 o'clock, 90=12 o'clock, 180=9 o'clock, 270=6 o'clock.
// A complete ellipse: st_ang=0, e_ang=360.  Does not apply aspect ratio
// correction.  Uses line thickness but not line patterns.
//
// Parameters:
//  pX: X coordinate for the start of the arc (2-digit MegaNum string)
//  pY: Y coordinate for the start of the arc (2-digit MegaNum string)
//  pStartAng: Arc start angle (2-digit MegaNum string)
//  pEndAng: Arc end angle (2-digit MegaNum string)
//  pRadX: The X radius (2-digit MegaNum string)
//  pRadY: The Y radius (2-digit MegaNum string)
//
// Return value: RIP command string
function RIPOvalArc(pX, pY, pStartAng, pEndAng, pRadX, pRadY)
{
	var rip = "|V";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pStartAng, 2, "00");
	rip += verifyMegaNumStr(pEndAng, 2, "00");
	rip += verifyMegaNumStr(pRadX, 2, "00");
	rip += verifyMegaNumStr(pRadY, 2, "00");
	return rip;
}
// Same as RIPOvalArc(), but with numeric parameters rather than MegaNum strings.
function RIPOvalArcNumeric(pX, pY, pStartAng, pEndAng, pRadX, pRadY)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var startAng = (typeof(pStartAng) === "number" ? toMegaNum(pStartAng, 2) : "00");
	var endAng = (typeof(pEndAng) === "number" ? toMegaNum(pEndAng, 2) : "00");
	var radX = (typeof(pRadX) === "number" ? toMegaNum(pRadX, 2) : "00");
	var radY = (typeof(pRadY) === "number" ? toMegaNum(pRadY, 2) : "00");
	return RIPOvalArc(x, y, startAng, endAng, radX, radY);
}

// RIP_PIE_SLICE
//          Function:  Draws a circular pie slice
//             Level:  0
//           Command:  I
//         Arguments:  x:2, y:2, start_ang:2, end_ang:2, radius:2
//            Format:  !|I <x> <y> <start_ang> <end_ang> <radius>
//           Example:  !|I1E18003G15
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  YES
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a circular pie slice.  The arc ends are connected to the center point
// with two straight lines.  The interior is filled with the current fill color
// and pattern.  The outline is drawn in the current drawing color and line thickness.
// Line patterns do not apply.  Same angle conventions as RIP_ARC.
//
// Parameters:
//  pX: X coordinate for the start of the arc (as a 2-digit MegaNum string)
//  pY: Y coordinate for the start of the arc (as a 2-digit MegaNum string)
//  pStartAng: Arc start angle
//  pEndAng: Arc end angle
//  pRadius: The radius (2-digit MegaNum string)
//
// Return value: RIP command string
function RIPPieSlice(pX, pY, pStartAng, pEndAng, pRadius)
{
	var rip = "|I";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pStartAng, 2, "00");
	rip += verifyMegaNumStr(pEndAng, 2, "00");
	rip += verifyMegaNumStr(pRadius, 2, "00");
	return rip;
}
// Same as RIPPieSlice(), but with numeric parameters rather than MegaNum strings.
function RIPPieSliceNumeric(pX, pY, pStartAng, pEndAng, pRadius)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var startAng = (typeof(pStartAng) === "number" ? toMegaNum(pStartAng, 2) : "00");
	var endAng = (typeof(pEndAng) === "number" ? toMegaNum(pEndAng, 2) : "00");
	var radius = (typeof(pRadius) === "number" ? toMegaNum(pRadius, 2) : "00");
	return RIPPieSlice(x, y, startAng, endAng, radius);
}

// RIP_OVAL_PIE_SLICE
//          Function:  Draws an elliptical pie slice
//             Level:  0
//           Command:  i
//         Arguments:  x:2, y:2, st_ang:2, e_ang:2, radx:2, rady:2
//            Format:  !|i <x> <y> <st_ang> <e_ang> <radx> <rady>
//           Example:  !|i1E18003G151Q
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  YES
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws an elliptical pie slice.  The arc ends are connected to the center point
// with two straight lines.  The interior is filled with the current fill color
// and pattern.  The outline is drawn in the current drawing color and line thickness.
// Line patterns do not apply.  Same angle conventions as RIP_OVAL_ARC.
//
// Parameters:
//  pX: X coordinate for the start of the arc (as a 2-digit MegaNum string)
//  pY: Y coordinate for the start of the arc (as a 2-digit MegaNum string)
//  pStartAng: Arc start angle
//  pEndAng: Arc end angle
//  pRadX: The X radius (2-digit MegaNum string)
//  pRadY: The Y radius (2-digit MegaNum string)
//
// Return value: RIP command string
function RIPOvalPieSlice(pX, pY, pStartAng, pEndAng, pRadX, pRadY)
{
	var rip = "|i";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pStartAng, 2, "00");
	rip += verifyMegaNumStr(pEndAng, 2, "00");
	rip += verifyMegaNumStr(pRadX, 2, "00");
	rip += verifyMegaNumStr(pRadY, 2, "00");
	return rip;
}
// Same as RIPOvalPieSlice(), but with numeric parameters rather than MegaNum strings.
function RIPOvalPieSliceNumeric(pX, pY, pStartAng, pEndAng, pRadX, pRadY)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var startAng = (typeof(pStartAng) === "number" ? toMegaNum(pStartAng, 2) : "00");
	var endAng = (typeof(pEndAng) === "number" ? toMegaNum(pEndAng, 2) : "00");
	var radX = (typeof(pRadX) === "number" ? toMegaNum(pRadX, 2) : "00");
	var radY = (typeof(pRadY) === "number" ? toMegaNum(pRadY, 2) : "00");
	return RIPOvalPieSlice(x, y, startAng, endAng, radX, radY);
}

// RIP_BEZIER
//          Function:  Draw a bezier curve
//             Level:  0
//           Command:  Z
//         Arguments:  x1:2 y1:2 x2:2 y2:2 x3:2 y3:2 x4:2 y4:2 cnt:2
//            Format:  !|Z <x1> <y1> <x2> <y2> <x3> <y3> <x4> <y4> <cnt>
//           Example:  !|Z0A0B0C0D0E0F0G0H1G
//   Uses Draw Color:  YES
// Uses Line Pattern:  YES
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a customizable Bezier curve using four control points.  The curve begins
// at (x1,y1) and ends at (x4,y4).  Points (x2,y2) and (x3,y3) are control points
// that pull the curve in their direction but are not necessarily on the curve.
// The <cnt> parameter specifies the number of line segments; more segments produce
// a smoother curve.  Uses current line style, pattern, thickness, and write mode.
//
// Parameters:
//  pX1: 2-digit MegaNum (string) for the X value of the curve start point
//  pY1: 2-digit MegaNum (string) for the Y value of the curve start point
//  pX2: 2-digit MegaNum (string) for the X value of the first control point
//  pY2: 2-digit MegaNum (string) for the Y value of the first control point
//  pX3: 2-digit MegaNum (string) for the X value of the second control point
//  pY3: 2-digit MegaNum (string) for the Y value of the second control point
//  pX4: 2-digit MegaNum (string) for the X value of the curve end point
//  pY4: 2-digit MegaNum (string) for the Y value of the curve end point
//  pCnt: 2-digit MegaNum (string) for the number of line segments (more = smoother)
//
// Return value: RIP command string
function RIPBezier(pX1, pY1, pX2, pY2, pX3, pY3, pX4, pY4, pCnt)
{
	var rip = "|Z";
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	rip += verifyMegaNumStr(pX2, 2, "00");
	rip += verifyMegaNumStr(pY2, 2, "00");
	rip += verifyMegaNumStr(pX3, 2, "00");
	rip += verifyMegaNumStr(pY3, 2, "00");
	rip += verifyMegaNumStr(pX4, 2, "00");
	rip += verifyMegaNumStr(pY4, 2, "00");
	rip += verifyMegaNumStr(pCnt, 2, "00");
	return rip;
}
// Same as RIPBezier(), but with numeric parameters rather than MegaNum strings.
function RIPBezierNumeric(pX1, pY1, pX2, pY2, pX3, pY3, pX4, pY4, pCnt)
{
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var x2 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y2 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var x3 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y3 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var x4 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y4 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var cnt = (typeof(pCnt) === "number" ? toMegaNum(pCnt, 2) : "00");
	return RIPBezier(x1, y1, x2, y2, x3, y3, x4, y4, cnt);
}

// RIP_POLYGON
//          Function:  Draw polygon in current color/line-style
//             Level:  0
//           Command:  P
//         Arguments:  npoints:2, x1:2, y1:2, ... xn:2, yn:2
//            Format:  !|P <npoints> <x1> <y1> ... <xn> <yn>
//           Example:  !|P03010105090905
//   Uses Draw Color:  YES
// Uses Line Pattern:  YES
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a closed multi-sided polygon using the current drawing color, line pattern
// and thickness.  <npoints> is between 2 and 512.  The last vertex is automatically
// connected back to the first.  Interior is not filled.
// This function takes numeric values (not MegaNum strings) as parameters.
//
// Parameters:
//  pXYPoints: An array of objects with numeric 'x' and 'y' properties defining the polygon vertices
//
// Return value: RIP command string
function RIPPolygonNumeric(pXYPoints)
{
	// Arguments: npoints:2, x1:2, y1:2, ... xn:2, yn:2
	return getRIPCmdWithArrayOfXYPts("P", pXYPoints, 2, 2);
}

// RIP_FILL_POLYGON
//          Function:  Draw filled polygon in current color/fill pattern
//             Level:  0
//           Command:  p
//         Arguments:  npoints:2, x1:2, y1:2, ... xn:2, yn:2
//            Format:  !|p <npoints> <x1> <y1> ... <xn> <yn>
//           Example:  !|p03010105050909
//   Uses Draw Color:  YES
// Uses Line Pattern:  YES
//   Uses Line Thick:  YES
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  YES
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Identical to RIP_POLYGON, but the interior is filled with the current fill
// color and fill pattern.  The outline is drawn using the current drawing color,
// line pattern and thickness.  Note: overlapping lines create unfilled gaps
// (regions inside an even number of times are not filled).
// This function takes numeric values (not MegaNum strings) as parameters.
//
// Parameters:
//  pXYPoints: An array of objects with numeric 'x' and 'y' properties defining the polygon vertices
//
// Return value: RIP command string
function RIPFillPolygonNumeric(pXYPoints)
{
	// Arguments: npoints:2, x1:2, y1:2, ... xn:2, yn:2
	return getRIPCmdWithArrayOfXYPts("p", pXYPoints, 2, 2);
}

// RIP_POLYLINE
//          Function:  Draw a Poly-Line (multi-faceted line)
//             Level:  0
//           Command:  l
//         Arguments:  npoints:2, x1:2, y1:2, ... xn:2, yn:2
//            Format:  !|l <npoints> <x1> <y1> ... <xn> <yn>
//           Example:  !|l03010105090905
//   Uses Draw Color:  YES
// Uses Line Pattern:  YES
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Draws a multi-faceted open line (like RIP_POLYGON but the last point is NOT
// connected back to the first).  Segments are drawn using the current drawing
// color, line pattern, thickness and write mode.  <npoints> is between 2 and 512.
// This function takes numeric values (not MegaNum strings) as parameters.
//
// Parameters:
//  pXYPoints: An array of objects with numeric 'x' and 'y' properties defining the line vertices
//
// Return value: RIP command string
function RIPPolyLineNumeric(pXYPoints)
{
	// Arguments: npoints:2, x1:2, y1:2, ... xn:2, yn:2
	return getRIPCmdWithArrayOfXYPts("l", pXYPoints, 2, 2);
}

// RIP_FILL
//          Function:  Flood fill screen area with current fill settings
//             Level:  0
//           Command:  F
//         Arguments:  x:2, y:2, border:2
//            Format:  !|F <x> <y> <border>
//           Example:  !|F25090F
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Performs a flood fill emanating from (x,y), spreading in all directions until
// encountering the <border> color.  Whatever is enclosed by the border color that
// is not the border color itself is filled with the current fill color and pattern.
// If the border does not completely enclose the start point, the fill reaches the
// viewport edges.  If the start point is already the border color, nothing is drawn.
//
// Parameters:
//  pX: 2-digit MegaNum: X value for the starting coordinate for the flood fill
//  pY: 2-digit MegaNum: Y value for the starting coordinate for the flood fill
//  pBorder: 2-digit MegaNum (string) for the border color that stops the flood fill
//
// Return value: RIP command string
function RIPFill(pX, pY, pBorder)
{
	var rip = "|F";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pBorder, 2, "00");
	return rip;
}
// Same as RIPFill(), but with numeric parameters rather than MegaNum strings.
function RIPFillNumeric(pX, pY, pBorder)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var border = (typeof(pBorder) === "number" ? toMegaNum(pBorder, 2) : "00");
	return RIPFill(x, y, border);
}

// RIP_LINE_STYLE
//          Function:  Defines a line style and thickness
//             Level:  0
//           Command:  =
//         Arguments:  style:2, user_pat:4, thick:2
//            Format:  !|= <style> <user_pat> <thick>
//           Example:  !|=01000001
//   Uses Draw Color:  NO
// Uses Line Pattern:  YES
//   Uses Line Thick:  YES
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Establishes the current line pattern and thickness for subsequent graphics
// commands.  Built-in styles: 00=Solid, 01=Dotted, 02=Centered, 03=Dashed,
// 04=Custom (uses <user_pat> as a 16-bit binary pattern).  Thickness: 01=1 pixel,
// 03=3 pixels wide.  If <style> is not 4, <user_pat> is ignored.
//
// Parameters:
//  pStyle: 2-digit MegaNum:
//  pUserPattern: 4-digit MegaNum for a binary coding defining a line
//                pattern
//  pThickness: 2-digit MegaNum for thickness
//
// Return value: RIP command string
function RIPLineStyle(pStyle, pUserPattern, pThickness)
{
	var rip = "|=";
	rip += verifyMegaNumStr(pStyle, 2, "00");
	rip += verifyMegaNumStr(pUserPattern, 4, "0000");
	rip += verifyMegaNumStr(pThickness, 2, "00");
	return rip;
}
// Same as RIPLineStyle(), but with numeric parameters rather than MegaNum strings.
function RIPLineStyleNumeric(pStyle, pUserPattern, pThickness)
{
	var style = (typeof(pStyle) === "number" ? toMegaNum(pStyle, 2) : "00");
	var pattern = (typeof(pUserPattern) === "number" ? toMegaNum(pUserPattern, 4) : "0000");
	var thickness = (typeof(pThickness) === "number" ? toMegaNum(pThickness, 2) : "00");
	return RIPLineStyle(style, pattern, thickness);
}

// RIP_FILL_STYLE
//          Function:  Set current fill style (predefined) & fill color
//             Level:  0
//           Command:  S
//         Arguments:  pattern:2, color:2
//            Format:  !|S <pattern> <color>
//           Example:  !|S050F
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Defines the current fill pattern and fill color for subsequent fill operations.
// Predefined patterns (00-0B):
//   00  Background color fill  01  Solid fill        02  Line fill
//   03  Light slash fill       04  Normal slash fill  05  Normal backslash fill
//   06  Light backslash fill   07  Light hatch fill   08  Heavy cross hatch fill
//   09  Interleaving line fill 0A  Widely spaced dots 0B  Closely spaced dots
// The <color> parameter is the fill color; "active" pixels use this color,
// "inactive" pixels use the background color (color 00).
//
// Parameters:
//  pPattern: 2-digit MegaNum for a pattern ID (00 to 0B)
//  pColor: 2-digit MegaNum for a color ID
//
// Return value: RIP command string
function RIPFillStyle(pPattern, pColor)
{
	var rip = "|S";
	rip += verifyMegaNumStr(pPattern, 2, "00");
	rip += verifyMegaNumStr(pColor, 2, "00");
	return rip;
}
// Same as RIPFillStyle(), but with numeric parameters rather than MegaNum strings.
function RIPFillStyleNumeric(pPattern, pColor)
{
	var pattern = (typeof(pPattern) === "number" ? toMegaNum(pPattern, 2) : "00");
	var color = (typeof(pColor) === "number" ? toMegaNum(pColor, 2) : "00");
	return RIPFillStyle(pattern, color);
}

// RIP_FILL_PATTERN
//          Function:  Set user-definable (custom) fill pattern/color
//             Level:  0
//           Command:  s
//         Arguments:  c1:2 c2:2 c3:2 c4:2 c5:2 c6:2 c7:2 c8:2 col:2
//            Format:  !|s <c1> <c2> <c3> <c4> <c5> <c6> <c7> <c8> <col>
//           Example:  !|s11223344556677880F
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Specifies a user-defined custom fill pattern as an 8x8 pixel array.  Parameters
// c1-c8 are the bit-patterns for each row (c1=top row, most-significant bit=left).
// Active pixels are drawn in <col> color; inactive pixels in background color (00).
// This completely overrides the predefined patterns of RIP_FILL_STYLE, and vice versa.
//
// Parameters:
//  pC1: 2-digit MegaNum (string) for the bit pattern of row 1 (top row)
//  pC2: 2-digit MegaNum (string) for the bit pattern of row 2
//  pC3: 2-digit MegaNum (string) for the bit pattern of row 3
//  pC4: 2-digit MegaNum (string) for the bit pattern of row 4
//  pC5: 2-digit MegaNum (string) for the bit pattern of row 5
//  pC6: 2-digit MegaNum (string) for the bit pattern of row 6
//  pC7: 2-digit MegaNum (string) for the bit pattern of row 7
//  pC8: 2-digit MegaNum (string) for the bit pattern of row 8 (bottom row)
//  pcCol: 2-digit MegaNum (string) for the fill color
//
// Return value: RIP command string
function RIPFilPattern(pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pcCol)
{
	var vals = [pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pcCol];
	var rip = "|s";
	for (var i = 0; i < vals.length; ++i)
		rip += verifyMegaNumStr(vals[i], 2, "00");
	return rip;
}
// Same as RIPFilPattern(), but with numeric parameters rather than MegaNum strings.
function RIPFilPatternNumeric(pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pcCol)
{
	var vals = [pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pcCol];
	for (var i = 0; i < vals.length; ++i)
		vals[i] = (typeof(vals[i]) === "number" ? toMegaNum(vals[i], 2) : "00");
	return RIPFilPattern(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7], vals[8]);
}

// RIP_MOUSE
//          Function:  Defines a rectangular hot mouse region
//             Level:  1
//           Command:  M
//         Arguments:  num:2 x0:2 y0:2 x1:2 y1:2 clk:1 clr:1 res:5 text
//            Format:  !|1M <num> <x0><y0><x1><y1> <clk><clr><res><text>
//           Example:  !|1M00001122331100000host command^M
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Defines a rectangular hot mouse region.  When the user clicks in the region,
// the terminal transmits <text> to the host.  (x0,y0) must be the upper-left
// corner and (x1,y1) the lower-right.  The <num> parameter is obsolete (set to 00).
// <clk>=1: invert region while mouse button is down.  <clr>=1: zoom text window
// to full screen and clear screen on click.  Use '^' in <text> for control chars
// (e.g., ^M for carriage return).  Maximum of 128 Mouse Regions total.
// The <res> parameter is reserved and should be set to 00000.
//
// Parameters:
//  pNum: 2-digit MegaNum: Used to be used in older RIPScrip v1.0 spec but is now obsolete. For upwards
//        compatibility, it should be set to "00".
//  pX0: 2-digit MegaNum: X location of the upper-left corner
//  pY0: 2-digit MegaNum: Y location of the upper-left corner
//  pX1: 2-digit MegaNum: X location of the lower-right corner
//  pY1: 2-digit MegaNum: Y location of the lower-right corner
//  pClk: Boolean: Whether or not the region should be visibly inverted while the mouse
//        button is down
//  pClr: Boolean: If true, the command will physically zoom the tet window to
//        full screen size and clear the screen. This is useful if the <pText>
//        parameter instructs the host to enter an area of the system that
//        doesn't support RIPScript graphics.
//  pRes: 5-digit MegaNum: Reserved
//  pText: A host command that gets sent when the field is clicked. You may use a
//         caret (^) to represent control characters (i.e., ^M for carriage return,
//         ^G, ^C, etc.)
//
// Return value: RIP command string
function RIPMouse(pNum, pX0, pY0, pX1, pY1, pClk, pClr, pRes, pText)
{
	var clk = (typeof(pClk) === "boolean" ? pClk : true);
	var clr = (typeof(pClr) === "boolean" ? pClr : false);

	var rip = "|1M";
	rip += verifyMegaNumStr(pNum, 2, "00");
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	rip += (clk ? "1" : "0");
	rip += (clr ? "1" : "0");
	rip += verifyMegaNumStr(pRes, 5, "00000");
	rip += pText;
	return rip;
}
// Same as RIPMouse(), but with numeric parameters rather than MegaNum strings.
function RIPMouseNumeric(pNum, pX0, pY0, pX1, pY1, pClk, pClr, pRes, pText)
{
	var num = (typeof(pNum) === "number" ? toMegaNum(pNum, 2) : "00");
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 5) : "00000");
	return RIPMouse(num, x0, y0, x1, y1, pClk, pClr, res, pText);
}

// RIP_BEGIN_TEXT
//          Function:  Define a rectangular text region
//             Level:  1
//           Command:  T
//         Arguments:  x1:2, y1:2, x2:2, y2:2, res:2
//            Format:  !|1T <x1> <y1> <x2> <y2> <res>
//           Example:  !|1T00110011
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Defines a rectangular portion of the graphics viewport for displaying a stream
// of text.  Follow with one or more RIP_REGION_TEXT commands, then terminate with
// RIP_END_TEXT.  Font direction is ignored; all text is always horizontal.
// Text lines that fall off the bottom of the region are discarded (no scrolling).
// The <res> parameter is reserved for future use.
//
// Parameters:
//  pX1: 2-digit MegaNum (string) for the X value of the upper-left corner of the text region
//  pY1: 2-digit MegaNum (string) for the Y value of the upper-left corner of the text region
//  pX2: 2-digit MegaNum (string) for the X value of the lower-right corner of the text region
//  pY2: 2-digit MegaNum (string) for the Y value of the lower-right corner of the text region
//  pRes: Reserved (2-digit MegaNum string)
//
// Return value: RIP command string
function RIPBeginText(pX1, pY1, pX2, pY2, pRes)
{
	var rip = "|1T";
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	rip += verifyMegaNumStr(pX2, 2, "00");
	rip += verifyMegaNumStr(pY2, 2, "00");
	rip += verifyMegaNumStr(pRes, 2, "00");
	return rip;
}
// Same as RIPBeginText(), but with numeric parameters rather than MegaNum strings.
function RIPBeginTextNumeric(pX1, pY1, pX2, pY2, pRes)
{
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var x2 = (typeof(pX2) === "number" ? toMegaNum(pX2, 2) : "00");
	var y2 = (typeof(pY2) === "number" ? toMegaNum(pY2, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 2) : "00");
	return RIPBeginText(x1, y1, x2, y2, res);
}

// RIP_REGION_TEXT
//          Function:  Display a line of text in rectangular text region
//             Level:  1
//           Command:  t
//         Arguments:  justify:1 and text-string
//            Format:  !|1t <justify> <text-string>
//           Example:  !|1t1This is a text line to be justified
//   Uses Draw Color:  YES
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  YES
//   Uses Font Sizes:  YES
//     Uses Viewport:  YES
//
// Displays a pre-word-wrapped text line inside the region defined by RIP_BEGIN_TEXT.
// The <text-string> is already formatted to fit based on the current font and size.
// <justify>=0: left-justify only.  <justify>=1: full left/right margin justification
// (spaces are padded to align the right edge with the region boundary).
//
// Parameters:
//  pJustify: Boolean: Whether or not to perform right/left margin justification
//            of the line of text
//  pText: The text to display in the region
//
// Return value: RIP command string
function RIPRegionText(pJustify, pText)
{
	var justify = (typeof(pJustify) === "boolean" ? pJustify : false);
	var rip = "|1t";
	rip += (justify ? "1" : "0");
	rip += pText;
	return rip;
}

// RIP_END_TEXT
//          Function:  End a rectangular text region
//             Level:  1
//           Command:  E
//         Arguments:  <none>
//            Format:  !|1E
//           Example:  !|1E
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Terminates a formatted text block started by RIP_BEGIN_TEXT.  Only one of
// these "end" commands is needed per block.
function RIPEndText()
{
	return "|1E";
}

// RIP_GET_IMAGE
//          Function:  Copy rectangular image to clipboard (as icon)
//             Level:  1
//           Command:  C
//         Arguments:  x0:2, y0:2, x1:2, y1:2, res:1
//            Format:  !|1C <x0> <y0> <x1> <y1> <res>
//           Example:  !|1C001122330
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Copies the rectangular region from (x0,y0) to (x1,y1) onto the internal
// Clipboard for future use with RIP_PUT_IMAGE or RIP_WRITE_ICON.  (x0,y0) must
// be the upper-left corner and (x1,y1) the lower-right.  The Clipboard is
// completely overwritten.  The <res> parameter is reserved and should be zero.
//
// Parameters:
//  pX0: A 2-digit MegaNum (string) for the X value of the top-left coordinate
//  pY0: A 2-digit MegaNum (string) for the Y value of the top-left coordinate
//  pX1: A 2-digit MegaNum (string) for the X value of the lower-right coordinate
//  pY1: A 2-digit MegaNum (string) for the Y value of the lower-right coordinate
//  pRes: Reserved (1-digit MegaNum)
//
// Return value: RIP command string
function RIPGetImage(pX0, pY0, pX1, pY1, pRes)
{
	var rip = "|1C";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	rip += verifyMegaNumStr(pRes, 1, "0");
	return rip;
}
// Same as RIPGetImage(), but with numeric parameters rather than MegaNum strings.
function RIPGetImageNumeric(pX0, pY0, pX1, pY1, pRes)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 1) : "0");
	return RIPGetImage(x0, y0, x1, y1, res);
}

// RIP_PUT_IMAGE
//          Function:  Pastes the clipboard contents onto the screen
//             Level:  1
//           Command:  P
//         Arguments:  x:2, y:2, mode:2, res:1
//            Format:  !|1P <x> <y> <mode> <res>
//           Example:  !|1P0011010
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Pastes the Clipboard contents at (x,y) (upper-left corner of the image).
// Mode: 00=COPY (normal), 01=XOR, 02=OR, 03=AND, 04=NOT (inverse).
// If the right edge would go off-screen, the command is ignored.
// The <res> parameter is reserved and should be zero.
//
// Parameters:
//  pX: 2-digit MegaNum (string) for the X value of the upper-left corner where the image is pasted
//  pY: 2-digit MegaNum (string) for the Y value of the upper-left corner where the image is pasted
//  pMode: 2-digit MegaNum (string) for the paste mode:
//         00=COPY (normal), 01=XOR, 02=OR, 03=AND, 04=NOT (inverse)
//  pRes: Reserved (1-digit MegaNum string)
//
// Return value: RIP command string
function RIPPutImage(pX, pY, pMode, pRes)
{
	var rip = "|1P";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pMode, 2, "00");
	rip += verifyMegaNumStr(pRes, 1, "0");
	return rip;
}
// Same as RIPPutImage(), but with numeric parameters rather than MegaNum strings.
function RIPPutImageNumeric(pX, pY, pMode, pRes)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var mode = (typeof(pMode) === "number" ? toMegaNum(pMode, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 1) : "0");
	return RIPPutImage(x, y, mode, res);
}

// RIP_WRITE_ICON
//          Function:  Write contents of the clipboard (icon) to disk
//             Level:  1
//           Command:  W
//         Arguments:  res:1, filename
//            Format:  !|1W <res> <filename>
//           Example:  !|1W0filename.icn
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Writes the Clipboard contents to a disk icon file for later use with
// RIP_LOAD_ICON.  Path/sub-directory information is not allowed in the filename.
// If the Clipboard is empty, the command is ignored.  An existing file of the
// same name is overwritten.  The <res> parameter is reserved and should be zero.
//
// Parameters:
//  pRes: Reserved (1-digit MegaNum string)
//  pFilename: The filename to write the icon to (no path/sub-directory information allowed)
//
// Return value: RIP command string
function RIPWriteIcon(pRes, pFilename)
{
	var rip = "|1W";
	rip += verifyMegaNumStr(pRes, 1, "0");
	rip += pFilename;
	return rip;
}
// Same as RIPWriteIcon(), but with numeric parameters rather than MegaNum strings.
function RIPWriteIconNumeric(pRes, pFilename)
{
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 1) : "0");
	return RIPWriteIcon(res, pFilename);
}

// RIP_LOAD_ICON
//          Function:  Loads and displays a disk-based icon to screen
//             Level:  1
//           Command:  I
//         Arguments:  x:2, y:2, mode:2, clipboard:1, res:2, filename
//            Format:  !|1I <x> <y> <mode> <clipboard> <res> <filename>
//           Example:  !|1I001101010button.icn
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  YES
//
// Reads an icon from disk and displays it at upper-left (x,y).  The .ICN
// extension is automatically appended if omitted.  Mode: 00=COPY, 01=XOR,
// 02=OR, 03=AND, 04=NOT (same modes as RIP_PUT_IMAGE).  If <clipboard>=1,
// the image is also copied to the Clipboard after being stamped on screen.
// The <res> parameter is reserved and should be set to "10".
//
// Parameters:
//  pX: A 2-digit MegaNum (string) for the X value of the upper-left location
//  pY: A 2-digit MegaNum (string) for the Y value of the upper-left location
//  pMode: A 2-digit MegaNum (string):
//         Mode  Description                                          Logical
//         ------------------------------------------------------------------
//         00    Paste the image on-screen normally                   (COPY)
//         01    Exclusive-OR  image with the one already on screen   (XOR)
//         02    Logically OR  image with the one already on screen   (OR)
//         03    Logically AND image with the one already on screen   (AND)
//         04    Paste the inverse of the image on the screen         (NOT)
//  pClipboard: Boolean: Whether or not to paste the image on screen AND
//              also copy it to the clipboard. If false, it is simply pasted
//              on the screen.
//  pFilename: Icon filename. Must not contain any sub-directory or path
//             information and must specify a valid icon file name.
//
// Return value: RIP command string
function RIPLoadIcon(pX, pY, pMode, pClipboard, pRes, pFilename)
{
	var clipboard = (typeof(pClipboard) === "boolean" ? pClipboard : false);

	var rip = "|1I";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pMode, 2, "00");
	rip += (clipboard ? "1" : "0");
	rip += pFilename;
	return rip;
}
// Same as RIPLoadIcon(), but with numeric parameters rather than MegaNum strings.
function RIPLoadIconNumeric(pX, pY, pMode, pClipboard, pRes, pFilename)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var mode = (typeof(pMode) === "number" ? toMegaNum(pMode, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 2) : "00");
	return RIPLoadIcon(x, y, mode, pClipboard, res, pFilename);
}

// RIP_QUERY
//          Function:  Query the contents of a text variable
//             Level:  1
//           Command:  <escape> (ASCII 27)
//         Arguments:  mode:1 res:3 ...text
//            Format:  !|1<ESC> <mode> <res> <text>
//           Example:  !|1<ESC>0000this is a query $COMMAND$^m
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Queries the terminal for text variable values and instructs it to send data
// to the host.  Uses ESC as the command character (not a printable character)
// for security.  Mode: 0=process NOW, 1=process on Graphics Window mouse click,
// 2=process on Text Window mouse click.  The <text> parameter can contain Text
// Variables (e.g., $FULL_NAME$), control characters, and host command segments.
// The <res> parameter is reserved and should be set to 000.
//
// Parameters:
//  pMode: 1-digit MegaNum:
//         0: Process the query command NOW (upon receipt)
//         1: Process when mouse clicked in Graphics Window
//         2: Process when mouse clicked in Text Window (any text
//            variables that return X or Y mouse coordinates return TEXT
//            coordinates, not graphics coordinates in this mode.  These
//            coordinates are two-digit values instead of the graphical
//            values that are four digits).
//  pRes: Reserved (3-digit MegaNum)
//  pText: Text of the query (may contain Text Variables, control characters, host command segments)
//
// Return value: RIP command string
function RIPQuery(pMode, pRes, pText)
{
	var rip = "|1" + ascii(27);
	rip += verifyMegaNumStr(pMode, 1, "0");
	rip += verifyMegaNumStr(pRes, 3, "000");
	rip += pText;
	return rip;
}
// Same as RIPQuery(), but with numeric parameters rather than MegaNum strings.
function RIPQueryNumeric(pMode, pRes, pText)
{
	var mode = (typeof(pMode) === "number" ? toMegaNum(pMode, 1) : "0");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 3) : "000");
	return RIPQuery(mode, res, pText);
}

// RIP_COPY_REGION
//          Function:  Copy screen region up/down
//             Level:  1
//           Command:  G
//         Arguments:  x0:2 y0:2 x1:2 y1:2 res:2 dest_line:2
//            Format:  !|1G <x0> <y0> <x1> <y1> <res> <dest_line>
//           Example:  !|1G080G140M0005
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Copies a rectangular screen region vertically (up or down) for scrolling effects.
// <dest_line> is the Y destination scan line.  The original region is NOT cleared.
// X0 and X1 must be evenly divisible by 8; if not, they are adjusted to the
// nearest 8-pixel boundary (X0 down, X1 up).  This command ignores the current
// RIP_VIEWPORT.  The <res> parameter is reserved.
//
// Parameters:
//  pX0: 2-digit A MegaNum (string) for the X value of the top-left coordinate
//  pY0: 2-digit A MegaNum (string) for the Y value of the top-left coordinate
//  pX1: 2-digit A MegaNum (string) for the X value of the lower-right coordinate
//  pY1: 2-digit A MegaNum (string) for the Y value of the lower-right coordinate
//  pRes: Reserved (2-digit MegaNum string)
//  pDestLine: 2-digit MegaNum string: The Y position that is the destination scan
//             line to receive the region
//
// Return value: A string to define a RIP window
function RIPCopyRegion(pX0, pY0, pX1, pY1, pRes, pDestLine)
{
	var rip = "|1G";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	rip += verifyMegaNumStr(pRes, 2, "00");
	rip += verifyMegaNumStr(pDestLine, 2, "00");
	return rip;
}
// Same as RIPCopyRegion(), but with numeric parameters rather than MegaNum strings.
function RIPCopyRegionNumeric(pX0, pY0, pX1, pY1, pRes, pDestLine)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 2) : "00");
	var destLine = (typeof(pDestLine) === "number" ? toMegaNum(pDestLine, 2) : "00");
	return RIPCopyRegion(x0, y0, x1, y1, res, destLine);
}

// RIP_READ_SCENE
//          Function:  Playback local .RIP file
//             Level:  1
//           Command:  R
//         Arguments:  res:8 filename...
//            Format:  !|1R <res> <filename>
//           Example:  !|1R00000000testfile.rip
//   Uses Draw Color:  YES
// Uses Line Pattern:  YES
//   Uses Line Thick:  YES
//   Uses Fill Color:  YES
// Uses Fill Pattern:  YES
//   Uses Write Mode:  YES
//   Uses Font Sizes:  YES
//     Uses Viewport:  NO
//
// Suspends current RIPscrip execution and plays back the specified local .RIP
// file.  When the playback is complete, execution resumes.  Any state changes
// (colors, fonts, etc.) made in the playback file remain in effect.  This
// command must be the last command on its line, followed by a carriage return.
// The <res> parameter is reserved and should be set to "00000000".
//
// Parameters:
//  pRes: Reserved (8-digit MegaNum string; should be "00000000")
//  pFilename: The filename of the .RIP file to play back (no path/sub-directory information)
//
// Return value: RIP command string
function RIPReadScene(pRes, pFilename)
{
	var rip = "|1R";
	rip += verifyMegaNumStr(pRes, 8, "00000000");
	rip += pFilename;
	return rip;
}
// Same as RIPReadScene(), but with numeric parameters rather than MegaNum strings.
function RIPReadSceneNumeric(pRes, pFilename)
{
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 8) : "00000000");
	return RIPReadScene(res, pFilename);
}

// RIP_FILE_QUERY
//          Function:  Query existing information on a particular file
//             Level:  1
//           Command:  F
//         Arguments:  mode:2 res:4 filename...
//            Format:  !|1F <mode> <res> <filename>
//           Example:  !|1F010000testfile.icn
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Queries the existence and/or metadata of a file on the terminal's disk,
// sending a response to the host immediately.  Mode values:
//   00  "0" or "1" (exists) returned, no carriage return
//   01  Same as 00, with carriage return
//   02  "0<cr>" or "1.size<cr>" returned
//   03  "0<cr>" or "1.size.date.time<cr>" returned
//   04  "0<cr>" or "1.filename.size.date.time<cr>" returned
// The <res> parameter is reserved and should be set to 0000.
//
// Parameters:
//  pMode: 2-digit MegaNum (string) for the query mode:
//         00=exists? (no CR), 01=exists? (with CR), 02=size, 03=size+date+time, 04=filename+size+date+time
//  pRes: Reserved (4-digit MegaNum string; should be "0000")
//  pFilename: The filename to query (no path/sub-directory information)
//
// Return value: RIP command string
function RIPFileQuery(pMode, pRes, pFilename)
{
	var rip = "|1F";
	rip += verifyMegaNumStr(pMode, 2, "00");
	rip += verifyMegaNumStr(pRes, 4, "0000");
	rip += pFilename;
	return rip;
}
// Same as RIPFileQuery(), but with numeric parameters rather than MegaNum strings.
function RIPFileQueryNumeric(pMode, pRes, pFilename)
{
	var mode = (typeof(pMode) === "number" ? toMegaNum(pMode, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 4) : "0000");
	return RIPFileQuery(mode, res, pFilename);
}

// RIP_ENTER_BLOCK_MODE
//          Function:  Enter block transfer mode with host
//             Level:  9 (system command)
//           Command:  <escape> (ASCII 27)
//         Arguments:  mode:1 proto:1 file_type:2 res:4 [filename] <>
//            Format:  !|9<ESC> <mode> <proto> <file_type> <res> [filename] <>
//           Example:  !|9<ESC>00010000ICONFILE.ICN<>
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Auto-initiates a file transfer protocol.  Mode: 0=download, 1=upload.
// Protocol (<proto>): 0=Xmodem/checksum, 1=Xmodem/CRC, 2=Xmodem-1K,
// 3=Xmodem-1K(G), 4=Kermit, 5=Ymodem, 6=Ymodem-G, 7=Zmodem.
// File type (<file_type>): 0=RIP display, 1=RIP store, 2=ICN store,
// 3=HLP store, 4=Composite Dynamic, 5=Active Dynamic.
// Must be the last command on its line; the protocol begins on the next line.
// The command must always be terminated with a "<>" sequence.
//
// Parameters:
//  pMode: Boolean: True for upload or false for download mode
//  pProtocol: 1-digit MegaNum to specify the file transfer protocol:
//          Value   Protocol            Filename Required?
//          ----------------------------------------------
//          0       Xmodem (checksum)          Yes
//          1       Xmodem (CRC)               Yes
//          2       Xmodem-1K                  Yes
//          3       Xmodem-1K (G)              Yes
//          4       Kermit                     Yes
//          5       Ymodem (batch)             No
//          6       Ymodem-G                   No
//          7       Zmodem (crash recovery)    No
//  pFileType: 2-digit MegaNum string
//  pRes: Reserved (4-digit MegaNum string)
//
// Return value: A string to define a RIP window
function RIPEnterBlockMode(pMode, pProtocol, pFileType, pRes)
{
	var mode = (typeof(pMode) === "boolean" ? pMode : false);

	var rip = "|9" + ascii(27);
	rip += (mode ? "1" : "0");
	rip += verifyMegaNumStr(pProtocol, 1, "7");
	rip += verifyMegaNumStr(pFileType, 2, "00");
	rip += verifyMegaNumStr(pRes, 4, "0000");
	return rip;
}
// Same as RIPEnterBlockMode(), but with numeric parameters rather than MegaNum strings.
function RIPEnterBlockModeNumeric(pMode, pProtocol, pFileType, pRes)
{
	var protocol = (typeof(pProtocol) === "number" ? toMegaNum(pProtocol, 1) : "7");
	var fileType = (typeof(pFileType) === "number" ? toMegaNum(pFileType, 1) : "7");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 4) : "0000");
	return RIPEnterBlockMode(pMode, protocol, fileType, res);
}

// RIP_NO_MORE
//          Function:  End of RIPscrip Scene
//             Level:  0
//           Command:  #
//         Arguments:  <none>
//            Format:  !|#
//           Example:  !|#
//   Uses Draw Color:  NO
// Uses Line Pattern:  NO
//   Uses Line Thick:  NO
//   Uses Fill Color:  NO
// Uses Fill Pattern:  NO
//   Uses Write Mode:  NO
//   Uses Font Sizes:  NO
//     Uses Viewport:  NO
//
// Signals the end of RIPscrip commands, allowing the terminal to activate
// Mouse Regions and respond to queued mouse clicks.  For noise-immunity, the
// host should transmit three or more consecutive RIP_NO_MORE commands.
// The terminal may also time-out after no data is received to assume RIP_NO_MORE.
function RIPNoMore()
{
	return "|#";
}


// Leave this as the last line; maybe this is used as a lib
this;
