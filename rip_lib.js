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

// RIP button style flags
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

// Returns a string for RIP kill mouse fields
function RIPKillMouseFields()
{
	return "|1K";
}

// Returns a string for RIP to reset windows (RIP_RESET_WINDOWS)
function RIPResetWindows()
{
	return "|*";
}

// Returns a string for RIP to clear the text window to the current
// background color (RIP_ERASE_WINDOW for text window)
function RIPEraseTextWindow()
{
	return "|e";
}

// Returns a string for RIP to clear the graphics window to the current
// background color (RIP_ERASE_WINDOW for graphics window)
function RIPEraseGraphicsWindow()
{
	return "|E";
}

// Returns a RIP string for RIP_HOME, to move the cursor to the upper-left
// corner of the Text Window
function RIPHome()
{
	return "|H";
}

// Returns a RIP string for RIP_ERASE_EOL, to erase the current line from the
// cursor to the end of the line
function RIPEraseEOL()
{
	return "|>";
}

// Returns a RIP command string to move the text cursor to a row & column
// in Text Window
function RIPGotoXY(pX, pY)
{
	var rip = "|g";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	return rip;
}
// Same as RIPGotoXY(), but with numeric parameters rather than MegaNum strings
function RIPGotoXYNumeric(pX, pY)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	return RIPGotoXY(x, y);
}

// Returns a RIP comand string to set RIP write mode
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

// Returns a string to set RIP color
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
// Same as RIPColor() but numeric instead of a MegaNum string
function RIPColorNumeric(pColor)
{
	var colorNum = 0;
	if (typeof(pColor) === "number" && pColor >= 0 && pColor <= 15)
		colorNum = toMegaNum(pColor, 2);
	return RIPColor(colorNum);
}

// Returns a string that defines a RIP font style
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
// Same as RIPFontStyle, but all numeric parameters (rather than MegaNum strings)
function RIPFontStyleNumeric(pFont, pDirection, pSize, pRes)
{
	var font = (typeof(pFont) === "number" ? toMegaNum(pFont, 2) : "00");
	var direction = (typeof(pDirection) === "number" ? toMegaNum(pDirection, 2) : "00");
	var size = (typeof(pSize) === "number" ? toMegaNum(pSize, 2) : "01");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 2) : "00");
	return RIPFontStyle(font, direction, size, res);
}

// Returns a RIP command string for RIP_TEXT: Draw text in current font/color at current spot
//
// Parameters:
//  pText: The text string to Draw
//
// Return value: RIP command string for drawing the text
function RIPText(pText)
{
	return "|T" + pText;
}

// Returns a string for the RIP_TEXT_XY function, to draw text
// in the current font/color at a specific spot
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
// Like RIPText() but numeric instead of MegaNum strings
function RIPTextXYNumeric(pX, pY, pText)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	return RIPTextXY(x, y, pText);
}

// Gets a string that defines RIP button style.
// This RIPscrip command is probably one of the most complex in the
// entire protocol.  It defines how subsequent RIP_BUTTON commands will
// be interpreted.  The purpose of this command is to define what a
// Button is and how they operate.  Buttons can have many different
// configurations, flags, and styles.  With the diversity of modes that
// the Button can take on, complexity is a necessary evil.
//
// The <pWidth> and <pHeight> parameters represent the fixed width and
// height of the Button (applies only to Plain Buttons).  If both values
// are greater than zero, then this will represent the actual size of the
// Button (its dimensions are not specified by the RIP_BUTTON command).
// If both of these are set to zero, then the actual RIP_BUTTON command
// will specify the size of the particular Button (dynamic sizing).
//
// Parameters:
//  pWidth: The width for the button style (RIP MegaNum); applies only to plain buttons
//  pHeight: The height for the button style (RIP MegaNum); applies only to plain buttons
//  pLabelOrientation: Numeric; defines the location of the optional text label. The
//                     actual text of the label is not specified with this command, it
//                     is specified when you actually create a Button (see RIP_BUTTON
//                     in the RIP spec).  The value that this parameter can be is as
//                     follows:
//                     Value  Description of Orientation
//                     -------------------------------------------------
//                     00     Display label above button
//                     01     Display label to the left of button
//                     02     Display label in the center of the button
//                     03     Display label to the right of button
//                     04     Display label beneath the button
//  pFlags: BigNum (4); Flags for the buton style. Each of these values may be combined to
//          achieve a "composite" group of flags.  See the preceding paragraphs for a
//          more detailed explanation of this method.
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
//  pBevelSize: BigNum; only used if the BEVEL FLAG (flag 512) is  > v1.54
//              specified.  When active, this parameter will determine how many
//              pixels thick the bevel should be.  This may be any value greater or
//              equal to zero.
//  pTxtLblDFore: BigNum; Used with the text label.
//  pTxtLblDBack: BigNum; Used with the text label.
//  pBright: BigNum; Used with Plain Buttons & with Special Effects styles (defined
//           by flags)
//  pDark: BigNum; Used with Plain Buttons & with Special Effects styles (defined
//         by flags)
//  pSurface: BigNum; Used with Plain Buttons & with Special Effects styles (defined by flags)
//  pGrpNum: BigNum; determines which button group subsequent RIP_BUTTON commands will be
//           associated with
//  pFlags2: BigNum (2); additional flags
//  pULineCol:
//  pCornerCol:
//  pRes: Reserved for future use
//
// Return value: A string that defines RIP button style
function RIPButtonStyle(pWidth, pHeight, pLabelOrientation, pFlags, pBevelSize, pTxtLblDFore,
						pTxtLblDBack, pBright, pDark, pSurface, pGrpNum, pFlags2, pULineCol,
						pCornerCol, pRes)
{
	/* ---------------------------------------------------------------------
	 * RIP_BUTTON_STYLE
	 * ---------------------------------------------------------------------
	 *          Function:  Button style definition
	 *             Level:  1
	 *           Command:  B
	 *         Arguments:  wid:2 hgt:2 orient:2 flags:4 size:2
	 *                     dfore:2 dback:2 bright:2 dark:2 surface:2
	 *                     grp_no:2 flags2:2 uline_col:2 corner_col:2
	 *                     res:6
	 *            Format:  !|1B <wid> <hgt> <orient> <flags>
	 *                     <bevsize> <dfore> <dback> <bright> <dark>
	 *                     <surface> <grp_no> <flags2> <uline_col>
	 *                     <corner_col> <res>
	 *           Example:  !|1B0A0A010274030F080F080700010E07000000
	 *   Uses Draw Color:  NO
	 * Uses Line Pattern:  NO
	 *   Uses Line Thick:  NO
	 *   Uses Fill Color:  NO
	 * Uses Fill Pattern:  NO
	 *   Uses Write Mode:  NO
	 *   Uses Font Sizes:  NO
	 *     Uses Viewport:  NO
	 */

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
// Same as RIPButtonStyle(), but where all the parameters are numbers rather than MegaNum strings
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

// Returns text to define a RIP window.
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
	/*
	 *         Arguments:  x0:2, y0:2, x1:2, y1:2, wrap:1, size:1
	 *            Format:  !|w <x0> <y0> <x1> <y1> <wrap> <size>
	 *           Example:  !|w00001B0M10
	 *   Uses Draw Color:  NO
	 * Uses Line Pattern:  NO
	 *   Uses Line Thick:  NO
	 *   Uses Fill Color:  NO
	 * Uses Fill Pattern:  NO
	 *   Uses Write Mode:  NO
	 *   Uses Font Sizes:  NO
	 *     Uses Viewport:  NO
	 */

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
// Like RIPWindow(), but with numeric parameters (rather than MegaNum strings)
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

// Gets a string that defines a RIP button.
// The (pX0,pY0) and (pX1,pY1) parameters will be modified by the following
// values for the different special effects:
// Effect Type   X0 Modifier   Y0 Modifier   X1 Modifier   Y1 Modifier
// -------------------------------------------------------------------
// Bevel         -bevel size   -bevel size   +bevel size   +bevel size
// Recess            -2            -2            +2            +2
// Sunken            0             0             0             0
// Chisel            0             0             0             0
//
// The <pFlags> parameter provides several different functions for each
// button.  The possible "combinatorial" flags for this parameter are
// listed in the following table.  Note that these values may be
// combined together (by adding their values) to arrive at the final
// flag parameter's value.
// Value  Description
// --------------------------------------------------
// 1    Draw button as already selected
// 2    Button is "default" when <ENTER> is pressed
//
// Parameters:
//  pX0: A 2-digit MegaNum (string) for the X value of the top-left coordinate. Defaults to 0.
//  pY0: A 2-digit MegaNum (string) for the Y value of the top-left coordinate. Defaults to 0.
//  pX1: A 2-digit MegaNum (string) for the X value of the lower-right coordinate. Defaults to 0.
//  pY1: A 2-digit MegaNum (string) for the Y value of the lower-right coordinate. Defaults to 0.
//  pHotkey: The hotkey to use. Only used with Mouse Buttons.  It is the ASCII
//           code for the keystroke that will activate this Button.  It is
//           represented as a two-digit MegaNum.  If this character exists in the
//           text label, and the Underline flag or hilight hotkey flag is enabled
//           in the RIP_BUTTON_STYLE, then the character will be underlined in the
//           label.  Control codes are allowable, and a value of 255 (decimal)
//           corresponds to "any" key.
//  pFlags: BigNum; the flags to use
//  pRes: Reserved
//  pTextStrs: An array of text strings to be used/displayed
//  pHostCmd: An optional string containing a command to send to the host
function RIPButton(pX0, pY0, pX1, pY1, pHotkey, pFlags, pRes, pTextStrs, pHostCmd)
{
	/*
	        Arguments:  x0:2 y0:2 x1:2 y1:2 hotkey:2 flags:1 res:1
	                    ...text
	           Format:  !|1U <x0> <y0> <x1> <y1> <hotkey> <flags>
	                    <res> <text>
	          Example:  !|1U010100003200iconfile<>Label<>HostCmd^m
	  Uses Draw Color:  NO
	Uses Line Pattern:  NO
	  Uses Line Thick:  NO
	  Uses Fill Color:  NO
	Uses Fill Pattern:  NO
	  Uses Write Mode:  NO
	  Uses Font Sizes:  YES
	    Uses Viewport:  YES

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
// Same as RIPButton, but with numeric parameters rather than MegaNum strings
function RIPButtonNumeric(pX0, pY0, pX1, pY1, pHotkey, pFlags, pRes, pTextStrs, pHostCmd)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var hotkey = (typeof(pHotkey) === "number" ? toMegaNum(pHotkey, 2) : "0W"); // Space
	var flags = (typeof(pFlags) === "number" ? toMegaNum(pFlags, 1) : "0");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 1) : "0");
	return RIPButton(x0, y0, x1, y1, hotkey, flags, res, pTextStrs, pHostCmd);
}

// Returns a string for RIP_VIEWPORT, defining the size & location of
// the graphics window
function RIPViewport(pX0, pY0, pX1, pY1)
{
	var rip = "|B";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "2E");
	rip += verifyMegaNumStr(pY1, 2, "1M");
	return rip;
}
// Same as RIPViewport() but all numeric (rather than MegaNum strings)
function RIPViewportNumeric(pX0, pY0, pX1, pY1)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "2E");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "1M");
	return RIPViewport(x0, y0, x1, y1);
}

// Returns a RIP command string for RIP_SET_PALETTE, to set the 16-color
// palette from master 64-color palette.
// This command modifies the 16-color RIP palette by choosing from the
// 64 colors in the master palette.  This allows you to alter the colors
// in your RIPscrip graphics scenes.  Once a Set Palette command is
// processed, any colors on the screen that had their corresponding
// palette entries changed will instantly switch to the new color set.
// You may obtain color cycling effects by using this command.  The
// default 16-color RIP palette is restored by the RIP_RESET_WINDOWS
// command.  The default 16-color RIP palette is:
//
//    16-Color RIP Palette     Master 64-Color EGA
//        Color Code           Palette Color Code       Color
//    ---------------------------------------------------------------
//    00                     0  (00)                    Black
//    01                     1  (01)                    Blue
//    02                     2  (02)                    Green
//    03                     3  (03)                    Cyan
//    04                     4  (04)                    Red
//    05                     5  (05)                    Magenta
//    06                     7  (06)                    Brown
//    07                     20 (0K)                    Light Gray
//    08                     56 (1K)                    Dark Gray
//    09                     57 (1L)                    Light Blue
//    0A                     58 (1M)                    Light Green
//    0B                     59 (1N)                    Light Cyan
//    0C                     60 (1O)                    Light Red
//    0D                     61 (1P)                    Light Magenta
//    0E                     62 (1Q)                    Yellow
//    0F                     63 (1R)                    White
//
// Color 00 of the 16-color RIP palette is always the background color
// (which is typically Black, or color 00 of the 64-color EGA palette).
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
// Same as RIPSetPallette(), but with all numeric parameters rather than MegaNum strings
function RIPSetPaletteNumeric(pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pC9, pC10, pC11, pC12, pC13, pC14, pC15, pC16)
{
	var vals = [pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pC9, pC10, pC11, pC12, pC13, pC14, pC15, pC16];
	for (var i = 0; i < vals.length; ++i)
		vals[i] = (typeof(vals[i]) === "number" ? toMegaNum(vals[i], 2) : "00");
	return RIPSetPalette(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7], vals[8], vals[9],
	                     vals[10], vals[11], vals[12], vals[13], vals[14], vals[15]);
}

// Returns a RIP command string for RIP_ONE_PALETTE, to set a color of the 16-color palette from
// Master Palette
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
// Same as RIPOnePalette(), but with numeric parameters rather than MegaNum strings
function RIPOnePaletteNumeric(pColor, pValue)
{
	var color = (typeof(pColor) === "number" ? toMegaNum(pColor, 2) : "00");
	var value = (typeof(pColor) === "number" ? toMegaNum(pValue, 2) : "00");
	return RIPOnePalette(color, value);
}

// Returns a RIP command string for RIP_MOVE: Move the current drawing position to (pX, pY)
function RIPMove(pX, pY)
{
	var rip = "|m";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	return rip;
}
function RIPMoveNumeric(pX, pY)
{
	var x = (typeof(pColor) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pColor) === "number" ? toMegaNum(pY, 2) : "00");
	return RIPMove(x, y);
}

// Returns a RIP scommand tring for RIP_PIXEL: Draws a pixel using current drawing color
//
// Parameters:
//  pX0: A 2-digit MegaNum (string) for the X value of the text location (2 digits)
//  pY0: A 2-digit MegaNum (string) for the Y value of the text location (2 digits)
//
// Return value: RIP command string
function RIPPixel(pX, pY)
{
	var rip = "|X";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	return rip;
}
function RIPPixelNumeric(pX, pY)
{
	var x = (typeof(pColor) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pColor) === "number" ? toMegaNum(pY, 2) : "00");
	RIPPixel(x, y);
}

// Returns a RIP command string for RIP_LINE: Draw a line in the current color/line style
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
function RIPLineNumeric(pX0, pY0, pX1, pY1)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	return RIPLine(x0, y0, x1, y1);
}

// Returns a RIP command string for RIP_RECTANGLE: Draw a rectangle in the current color/line style
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
function RIPRectangleNumeric(pX0, pY0, pX1, pY1)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	return RIPRectangle(x0, y0, x1, y1);
}

// Returns a RIP command string for RIP_BAR: Draw a filled rectangle with fill color/pattern
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
	/*
	 *        Arguments:  x0:2, y0:2, x1:2, y1:2
	 *           Format:  !|B <x0> <y0> <x1> <y1>
	 *          Example:  !|B00010A0E
	 *   Uses Draw Color:  NO
	 * Uses Line Pattern:  NO
	 *   Uses Line Thick:  NO
	 *   Uses Fill Color:  YES
	 * Uses Fill Pattern:  YES
	 *   Uses Write Mode:  NO
	 *   Uses Font Sizes:  NO
	 *     Uses Viewport:  YES
	 */

	var rip = "|B";
	rip += verifyMegaNumStr(pX0, 2, "00");
	rip += verifyMegaNumStr(pY0, 2, "00");
	rip += verifyMegaNumStr(pX1, 2, "00");
	rip += verifyMegaNumStr(pY1, 2, "00");
	return rip;
}
// Like RIPBar(), but all numeric (rather than MegaNum strings)
function RIPBarNumeric(pX0, pY0, pX1, pY1)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	return RIPBar(x0, y0, x1, y1);
}

// Returns a RIP command string for RIP_CIRCLE: Draw circle in current color & line thickness
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
function RIPCircleNumeric(pXCenter, pYCenter, pRadius)
{
	var XCenter = (typeof(pXCenter) === "number" ? toMegaNum(pXCenter, 2) : "00");
	var YCenter = (typeof(pYCenter) === "number" ? toMegaNum(pYCenter, 2) : "00");
	var radius = (typeof(pRadius) === "number" ? toMegaNum(pX0, 2) : "00");
	return RIPCircle(XCenter, YCenter, radius);
}

// Returns a RIP command string for RIP_OVAL: Draw elliptical arc in current color/line style.
// This command draws an elliptical arc similar to the circular RIP_ARC
// command.  The center of the ellipse is (x,y) and the arc is drawn
// starting from <pStartAng> and proceeding counterclockwise to <pEndAng>
// (see RIP_ARC above for details).
//
// The X radius is half the full width of the ellipse, the Y radius is
// half the full height.  The ellipse is drawn according to the current
// line thickness, but the current line pattern has no effect.
//
// Parameters:
//  pXCenter: X coordinate for the center of the oval (as a 2-digit MegaNum string)
//  pYCenter: Y coordinate for the center of the oval (as a 2-digit MegaNum string)
//  pStartAng:
//  pEndAng:
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

// Returns a RIP command string for RIP_FILLED_OVAL: Draw filled ellipse using current
// color/pattern.
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
function RIPFilledOvalNumeric(pXCenter, pYCenter, pXRad, pYRad)
{
	var xCenter = (typeof(pXCenter) === "number" ? toMegaNum(pXCenter, 2) : "00");
	var yCenter = (typeof(pYCenter) === "number" ? toMegaNum(pYCenter, 2) : "00");
	var xRad = (typeof(pXRad) === "number" ? toMegaNum(pXRad, 2) : "00");
	var yRad = (typeof(pYRad) === "number" ? toMegaNum(pYRad, 2) : "00");
	return RIPFilledOval(xCenter, yCenter, xRad, yRad);
}

// Returns a RIP command string for RIP_ARC: Draw circular arc in current color/line
// thickness.
// This command draws a circular arc, or a segment of a circle.  Drawing
// begins at <start_ang> and terminates at <end_ang>.  The angles are
// represented starting at zero for the 3 o'clock position and
// increasing counterclockwise through a full circle to 360:
//
//                   90
//                    |
//              180---|--- 0
//                    |
//                   270
//
// The arc drawing begins at the <start_angle> and continues counter-
// clockwise to the <end_angle>.  A full circle will be displayed if
// <start_ang>=0 and <end_ang>=360.  This command recognizes aspect
// ratios like the circle command does.  It does not take advantage of
// line patterns but does comply with line thickness.
//
// If both angles are equal, nothing is drawn.
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
function RIPArcNumeric(pX, pY, pStartAng, pEndAng, pRadius)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var startAng = (typeof(pStartAng) === "number" ? toMegaNum(pStartAng, 2) : "00");
	var endAng = (typeof(pEndAng) === "number" ? toMegaNum(pEndAng, 2) : "00");
	var radius = (typeof(pRadius) === "number" ? toMegaNum(pRadius, 2) : "00");
	return RIPArc(x, y, startAng, endAng, radius);
}

// Returns a RIP command string for RIP_OVAL_ARC: Draw an elliptical arc.
// This command draws an elliptical arc, or a segment of an ellipse.
// Drawing begins at <pStartAng> and terminates at <pEndAng>.  The angles are
// represented starting at zero for 3 o'clock position and increasing
// counterclockwise through a full ellipse at 360 degrees:
//
//                   90
//                    |
//              180---|--- 0
//                    |
//                   270
//
// The arc drawing begins at the <pStartAng> and continues counterclockwise
// to the <pEndAng>.  A complete ellipse will be displayed if <pStartAng>=0
// and <pEndAng>=360.  This command does not utilize "aspect ratios"
// because of the nature of an Ellipse.  It does not take advantage of
// line patterns but does comply with line thickness.
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

// Returns a RIP command string for RIP_PIE_SLICE: Draws a riculr pie slice
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
function RIPPieSliceNumeric(pX, pY, pStartAng, pEndAng, pRadius)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var startAng = (typeof(pStartAng) === "number" ? toMegaNum(pStartAng, 2) : "00");
	var endAng = (typeof(pEndAng) === "number" ? toMegaNum(pEndAng, 2) : "00");
	var radius = (typeof(pRadius) === "number" ? toMegaNum(pRadius, 2) : "00");
	return RIPPieSlice(x, y, startAng, endAng, radius);
}

// Returns a RIP command string for RIP_OVAL_PIE_SLICE: Draws an elliptical pie slice
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

// Returns a RIP command string for RIP_BEZIER: Draw a bezier curve
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

// Returns a RIP command string for RIP_POLYGON: Draw polygon in current
// color/line style.
// This function uses the "..Numeric" naming convention, as it takes
// values that are numbers rather than MegaNum strings; this will
// convert the numbers to MegaNum strings for the RIP instruction.
//
// This command will draw a multi-sided closed polygon.  The polygon is
// drawn using the current drawing color, line pattern and thickness.
// The <npoints> parameter is between 2 and 512 and indicates how many
// (x,y) coordinate pairs will follow, which is also the number of sides
// of the polygon.  The polygon interior is not filled by RIP_POLYGON.
//
// The polygon is enclosed by the last vertex between xn,yn and x1,y1.
// In other words, you do not have to connect the end to the beginning -
// it is automatically done for you.
//
// Parameters:
//  pXYPoints: An array of {x, y} objects containing MegaNum strings
//             representing the coordinates of the points
//
// Return value: RIP command string
function RIPPolygonNumeric(pXYPoints)
{
	// Arguments: npoints:2, x1:2, y1:2, ... xn:2, yn:2
	return getRIPCmdWithArrayOfXYPts("P", pXYPoints, 2, 2);
}

// Returnss a RIP command for RIP_FILL_POLYGON: Draw filled polygon in current
// color/fill pattern.
// This function uses the "..Numeric" naming convention, as it takes
// values that are numbers rather than MegaNum strings; this will
// convert the numbers to MegaNum strings for the RIP instruction.
//
// This command is identical to RIP_POLYGON, except that the interior of
// the polygon is filled with the current fill color and fill pattern.
// The actual outline of the polygon is drawn using the current drawing
// color, line pattern and thickness.
//
// NOTE:  You will get unusual effects if the lines of the polygon
//        overlap, creating a polygon with internal "gaps".  (The rule
//        is this: regions that are "inside" the polygon an even number
//        of times due to overlap are NOT filled.)  The interior fill
//        does not utilize Write Mode, but the outline of the polygon
//        does.
//
// Parameters:
//  pXYPoints: An array of {x, y} objects containing MegaNum strings
//             representing the coordinates of the points
//
// Return value: RIP command string
function RIPFillPolygonNumeric(pXYPoints)
{
	// Arguments: npoints:2, x1:2, y1:2, ... xn:2, yn:2
	return getRIPCmdWithArrayOfXYPts("p", pXYPoints, 2, 2);
}

// Returns a RIP command for RIP_POLYLINE: Draw a poly-line (multi-faceted line)
function RIPPolyLineNumeric(pXYPoints)
{
	// Arguments: npoints:2, x1:2, y1:2, ... xn:2, yn:2
	return getRIPCmdWithArrayOfXYPts("l", pXYPoints, 2, 2);
}

// Returns a RIP command string for RIP_FILL: Flood fill screen area with current
// fill settings.
//
// Parameters:
//  pX: 2-digit MegaNum: X value for the starting coordinate for the flood fill
//  pY: 2-digit MegaNum: Y value for the starting coordinate for the flood fill
//  pBorder:
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
function RIPFillNumeric(pX, pY, pBorder)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var border = (typeof(pBorder) === "number" ? toMegaNum(pBorder, 2) : "00");
	return RIPFill(x, y, border);
}

// Returns a RIP command string for RIP_LINE_STYLE: Defines a line style
// and thickness.
// There are four built-in line styles plus provisions for
// custom line patterns.
//
// Style   Description           Binary            Hex
// ----------------------------------------------------
// 00      Normal, Solid Line    1111111111111111  FFFF
// 01      Dotted Line           0011001100110011  3333
// 02      Centered Line         0001111000111111  1E3F
// 03      Dashed Line           0001111100011111  1F1F
// 04      Custom Defined line (see about <pUserPattern> below)
//
// Thick   Description
// -------------------------------
// 01      Lines are 1 pixel wide
// 03      Lines are 3 pixels wide
//
// If the <pStyle> is set to a value of 4 (custom pattern), then the
// <pUserPattern> parameter is used as a 16-bit representation of the pixels
// in the line pattern.  For example:
//
// Repeating Pattern     Binary Coding     Hex     Decimal   MegaNum
// -------------------------------------------------------------------
// - - - - - - - -    1010101010101010    AAAA     43690     0XPM
// ----    ----       1111000011110000    F0F0     61680     1BLC
//
// So, the most-significant-bit of <pUserPattern> is toward the starting
// point of the line or border that uses this fill pattern.  If the
// <pStyle> parameter is not 4, then the <pUserPattern> parameter is ignored.
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
function RIPLineStyleNumeric(pStyle, pUserPattern, pThickness)
{
	var style = (typeof(pStyle) === "number" ? toMegaNum(pStyle, 2) : "00");
	var pattern = (typeof(pUserPattern) === "number" ? toMegaNum(pUserPattern, 4) : "0000");
	var thickness = (typeof(pThickness) === "number" ? toMegaNum(pThickness, 2) : "00");
	return RIPLineStyle(style, pattern, thickness);
}

// Returns a RIP command string for RIP_FILL_STYLE: Set current fill style (predefined)
// & fill color.
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
function RIPFillStyleNumeric(pPattern, pColor)
{
	var pattern = (typeof(pPattern) === "number" ? toMegaNum(pPattern, 2) : "00");
	var color = (typeof(pColor) === "number" ? toMegaNum(pColor, 2) : "00");
	return RIPFillStyle(pattern, color);
}

// Returns a RIP command string for RIP_FILL_PATTERN: Set user-definable (custom)
// fill pattern/color
function RIPFilPattern(pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pcCol)
{
	var vals = [pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pcCol];
	var rip = "|s";
	for (var i = 0; i < vals.length; ++i)
		rip += verifyMegaNumStr(vals[i], 2, "00");
	return rip;
}
function RIPFilPatternNumeric(pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pcCol)
{
	var vals = [pC1, pC2, pC3, pC4, pC5, pC6, pC7, pC8, pcCol];
	for (var i = 0; i < vals.length; ++i)
		vals[i] = (typeof(vals[i]) === "number" ? toMegaNum(vals[i], 2) : "00");
	return RIPFilPattern(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7], vals[8]);
}

// Returns a RIP command string for RIP_Mouse: Defines a rectangular hot mouse region.
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

// Returns a RIP command string for RIP_BEGIN_TEXT: Define a rectangular text region.
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
function RIPBeginTextNumeric(pX1, pY1, pX2, pY2, pRes)
{
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var x2 = (typeof(pX2) === "number" ? toMegaNum(pX2, 2) : "00");
	var y2 = (typeof(pY2) === "number" ? toMegaNum(pY2, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 2) : "00");
	return RIPBeginText(x1, y1, x2, y2, res);
}

// Returns a RIP command string for RIP_REGION_TEXT: Display a line of text in
// rectangular text region.
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

// Returns a RIP command string for RIP_END_TEXT: End a rectangular text region.
function RIPEndText()
{
	return "|1E";
}

// Returns a RIP command string for RIP_GET_IMAGE: Copy rectangular image to clipboard
// (an icon).
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
function RIPGetImageNumeric(pX0, pY0, pX1, pY1, pRes)
{
	var x0 = (typeof(pX0) === "number" ? toMegaNum(pX0, 2) : "00");
	var y0 = (typeof(pY0) === "number" ? toMegaNum(pY0, 2) : "00");
	var x1 = (typeof(pX1) === "number" ? toMegaNum(pX1, 2) : "00");
	var y1 = (typeof(pY1) === "number" ? toMegaNum(pY1, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 1) : "0");
	return RIPGetImage(x0, y0, x1, y1, res);
}

// Returns a RIP command string for RIP_PUT_IMAGE: Pastes the clipboard contents
// onto the screen.
function RIPPutImage(pX, pY, pMode, pRes)
{
	var rip = "|1P";
	rip += verifyMegaNumStr(pX, 2, "00");
	rip += verifyMegaNumStr(pY, 2, "00");
	rip += verifyMegaNumStr(pMode, 2, "00");
	rip += verifyMegaNumStr(pRes, 1, "0");
	return rip;
}
function RIPPutImageNumeric(pX, pY, pMode, pRes)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var mode = (typeof(pMode) === "number" ? toMegaNum(pMode, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 1) : "0");
	return RIPPutImage(x, y, mode, res);
}

// Returns a RIP function for RIP_WRITE_ICON: Write contents of the clipboard
// (icon) to disk.
function RIPWriteIcon(pRes, pFilename)
{
	var rip = "|1W";
	rip += verifyMegaNumStr(pRes, 1, "0");
	rip += pFilename;
	return rip;
}
function RIPWriteIconNumeric(pRes, pFilename)
{
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 1) : "0");
	return RIPWriteIcon(res, pFilename);
}

// Returns a RIP command string for RIP_LOAD_ICON: Loads and displays a disk-based
// icon to screen.
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
function RIPLoadIconNumeric(pX, pY, pMode, pClipboard, pRes, pFilename)
{
	var x = (typeof(pX) === "number" ? toMegaNum(pX, 2) : "00");
	var y = (typeof(pY) === "number" ? toMegaNum(pY, 2) : "00");
	var mode = (typeof(pMode) === "number" ? toMegaNum(pMode, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 2) : "00");
	return RIPLoadIcon(x, y, mode, pClipboard, res, pFilename);
}

// Returns a RIP command string for RIP_QUERY: Query the contents of a text
// variable.
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
//  pText: Text of the query
function RIPQuery(pMode, pRes, pText)
{
	var rip = "|1" + ascii(27);
	rip += verifyMegaNumStr(pMode, 1, "0");
	rip += verifyMegaNumStr(pRes, 3, "000");
	rip += pText;
	return rip;
}
function RIPQueryNumeric(pMode, pRes, pText)
{
	var mode = (typeof(pMode) === "number" ? toMegaNum(pMode, 1) : "0");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 3) : "000");
	return RIPQuery(mode, res, pText);
}

// Returns a RIP command string for RIP_COPY_REGION: Copy screen region up/down.
// The pX0 and pX1 parameters must be evenly divisible by 8; if they're not,
// then the pX0 parameter will be reduced to the next most 8-pixel boundary,
// and the pX1 parameter will be increased to the next 8-pixel boundary.
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

// Returns a RIP command string for RIP_READ_SCENE: Playback local .RIP file
function RIPReadScene(pRes, pFilename)
{
	var rip = "|1R";
	rip += verifyMegaNumStr(pRes, 8, "00000000");
	rip += pFilename;
	return rip;
}
function RIPReadSceneNumeric(pRes, pFilename)
{
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 8) : "00000000");
	return RIPReadScene(res, pFilename);
}

// Returns a RIP command string for RIP_FILE_QUERY: Query existing information on
// a particular file.
function RIPFileQuery(pMode, pRes, pFilename)
{
	var rip = "|1F";
	rip += verifyMegaNumStr(pMode, 2, "00");
	rip += verifyMegaNumStr(pRes, 4, "0000");
	rip += pFilename;
	return rip;
}
function RIPFileQueryNumeric(pMode, pRes, pFilename)
{
	var mode = (typeof(pMode) === "number" ? toMegaNum(pMode, 2) : "00");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 4) : "0000");
	return RIPFileQuery(mode, res, pFilename);
}

// Returns a RIP command string for RIP_ENTER_BLOCK_MODE: Enter block transfer mode
// with host.
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
function RIPEnterBlockModeNumeric(pMode, pProtocol, pFileType, pRes)
{
	var protocol = (typeof(pProtocol) === "number" ? toMegaNum(pProtocol, 1) : "7");
	var fileType = (typeof(pFileType) === "number" ? toMegaNum(pFileType, 1) : "7");
	var res = (typeof(pRes) === "number" ? toMegaNum(pRes, 4) : "0000");
	return RIPEnterBlockMode(pMode, protocol, fileType, res);
}

// Returns a RIP command string for RIP_NO_MORE: End of RIPScrip scene.
function RIPNoMore()
{
	return "|#";
}


// Leave this as the last line; maybe this is used as a lib
this;
