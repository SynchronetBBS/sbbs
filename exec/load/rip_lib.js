// This file defines functions & things to help programmatically
// output RIP commands and UI elements, for terminals that support
// RIP.
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

// Returns a string for RIP kill mouse fields
function RIPKillMouseFields()
{
	return "|1K";
}

// Returns a string to set RIP write mode
//
// Parameters:
//  pMode: The RIP write mode:
//         Mode   Description
//         ------------------------------------------
//         00     Normal drawing mode (overwrite)
//         01     XOR (complimentary) mode
//
// Return value: A string to set RIP write mode
function RIPWriteModeStr(pMode)
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

// Returns a string for the RIP_TEXT_XY function, to draw text
// in the current font/color at a specific spot
//
// Parameters:
//  pX0: A MegaNum (string) for the X value of the text location
//  pY0: A MegaNum (string) fhe Y value of the text location
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

////////////////////////////////////////////////////
// RIP GUI elements

// Returns text to define a RIP window.
//
// Parameters:
//  pX0: A MegaNum (string) for the X value of the top-left coordinate. Defaults to 0.
//  pY0: A MegaNum (string) fhe Y value of the top-left coordinate. Defaults to 0.
//  pX1: A MegaNum (string) fhe X value of the lower-right coordinate. Defaults to 27.
//  pY1: A MegaNum (string) fhe Y value of the lower-right coordinate. Defaults to 16.
//  pWrap: Boolean/number/MegaNum; Whether or not to wrap text (true/false or 1/0). Defaults to true/1.
//  pSize: A MegaNum (string) to set the font size (defaults to 0):
//         Size   Font Size   X Range  Y Range
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
	 *        Arguments:  x0:2, y0:2, x1:2, y1:2, wrap:1, size:1
	 *           Format:  !|w <x0> <y0> <x1> <y1> <wrap> <size>
	 *          Example:  !|w00001B0M10
	 *  Uses Draw Color:  NO
	 * Uses Line Pattern:  NO
	 *  Uses Line Thick:  NO
	 *  Uses Fill Color:  NO
	 * Uses Fill Pattern:  NO
	 *  Uses Write Mode:  NO
	 *  Uses Font Sizes:  NO
	 *    Uses Viewport:  NO
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
//  pX0: A MegaNum (string) for the X value of the top-left coordinate. Defaults to 0.
//  pY0: A MegaNum (string) fhe Y value of the top-left coordinate. Defaults to 0.
//  pX1: A MegaNum (string) fhe X value of the lower-right coordinate. Defaults to 0.
//  pY1: A MegaNum (string) fhe Y value of the lower-right coordinate. Defaults to 0.
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

// Returns a string that defines a RIP bar (filled rectangle with fill color/pattern)
function RIPBar(pX0, pY0, pX1, pY1)
{
	/*
	       Arguments:  x0:2, y0:2, x1:2, y1:2
	           Format:  !|B <x0> <y0> <x1> <y1>
	          Example:  !|B00010A0E
	  Uses Draw Color:  NO
	Uses Line Pattern:  NO
	  Uses Line Thick:  NO
	  Uses Fill Color:  YES
	Uses Fill Pattern:  YES
	  Uses Write Mode:  NO
	  Uses Font Sizes:  NO
	    Uses Viewport:  YES
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


// Leave this as the last line; maybe this is used as a lib
this;
