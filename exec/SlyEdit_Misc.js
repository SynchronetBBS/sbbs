// $Id$

/* This file declares some general helper functions and variables
 * that are used by SlyEdit.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 * Date       User              Description
 * 2009-06-06 Eric Oulashin     Started development
 * 2009-06-11 Eric Oulashin     Taking a break from development
 * 2009-08-09 Eric Oulashin     Started more development & testing
 * 2009-08-22 Eric Oulashin     Version 1.00
 *                              Initial public release
 * ....Removed some comments...
 * 2017-12-16 Eric Oulashin     Updated ReadSlyEditConfigFile() to include the
 *                              allowEditQuoteLines option.
 * 2017-12-18 Eric Oulashin     Update the KEY_PAGE_UP and KEY_PAGE_DOWN keys to
 *                              ensure they mat what's in sbbsdef.js
 * 2017-12-24 Eric Oulashin     Updated firstNonQuoteTxtIndex() to better handle
 *                              lines with 3 non-space characters before a >, to
 *                              not consider those sequences a quote when using
 *                              author initials.  When using author initials,
 *                              SlyEdit considers a quote sequence to only have 2
 *                              non-space characters (such as "EO>").
 * 2017-12-25 Eric Oulashin     Updated wrapTextLines() - Added an optional
 *                              parameter for the lineInfo object array so it
 *                              can be updated when lines are split (for quoting
 *                              with author initials).  That should fix an
 *                              issue where some wrapped/split quote lines
 *                              were missing the quote line prefix.
 * 2017-12-26 Eric Oulashin     Updated wrapTextLines() to (hopefully) better
 *                              handle situations when it wraps text into the
 *                              next line when that next line is blank - Ensuring
 *                              it adds a blank line below that.
 * 2018-01-27 Eric Oulashin     Added removeStrayANSIOneChars()
 */
 
 load("text.js");
 
// Note: These variables are declared with "var" instead of "const" to avoid
// multiple declaration errors when this file is loaded more than once.

// Values for attribute types (for text attribute substitution)
var FORE_ATTR = 1; // Foreground color attribute
var BKG_ATTR = 2;  // Background color attribute
var SPECIAL_ATTR = 3; // Special attribute

// Box-drawing/border characters: Single-line
var UPPER_LEFT_SINGLE = "Ú";
var HORIZONTAL_SINGLE = "Ä";
var UPPER_RIGHT_SINGLE = "¿";
var VERTICAL_SINGLE = "³";
var LOWER_LEFT_SINGLE = "À";
var LOWER_RIGHT_SINGLE = "Ù";
var T_SINGLE = "Â";
var LEFT_T_SINGLE = "Ã";
var RIGHT_T_SINGLE = "´";
var BOTTOM_T_SINGLE = "Á";
var CROSS_SINGLE = "Å";
// Box-drawing/border characters: Double-line
var UPPER_LEFT_DOUBLE = "É";
var HORIZONTAL_DOUBLE = "Í";
var UPPER_RIGHT_DOUBLE = "»";
var VERTICAL_DOUBLE = "º";
var LOWER_LEFT_DOUBLE = "È";
var LOWER_RIGHT_DOUBLE = "¼";
var T_DOUBLE = "Ë";
var LEFT_T_DOUBLE = "Ì";
var RIGHT_T_DOUBLE = "¹";
var BOTTOM_T_DOUBLE = "Ê";
var CROSS_DOUBLE = "Î";
// Box-drawing/border characters: Vertical single-line with horizontal double-line
var UPPER_LEFT_VSINGLE_HDOUBLE = "Õ";
var UPPER_RIGHT_VSINGLE_HDOUBLE = "¸";
var LOWER_LEFT_VSINGLE_HDOUBLE = "Ô";
var LOWER_RIGHT_VSINGLE_HDOUBLE = "¾";
// Other special characters
var DOT_CHAR = "ú";
var CHECK_CHAR = "û";
var THIN_RECTANGLE_LEFT = "Ý";
var THIN_RECTANGLE_RIGHT = "Þ";
var BLOCK1 = "°"; // Dimmest block
var BLOCK2 = "±";
var BLOCK3 = "²";
var BLOCK4 = "Û"; // Brightest block

// Navigational keys
var UP_ARROW = "";
var DOWN_ARROW = "";
// CTRL keys
var CTRL_A = "\x01";
var CTRL_B = "\x02";
//var KEY_HOME = CTRL_B;
var CTRL_C = "\x03";
var CTRL_D = "\x04";
var CTRL_E = "\x05";
//var KEY_END = CTRL_E;
var CTRL_F = "\x06";
//var KEY_RIGHT = CTRL_F;
var CTRL_G = "\x07";
var BEEP = CTRL_G;
var CTRL_H = "\x08";
var BACKSPACE = CTRL_H;
var CTRL_I = "\x09";
var TAB = CTRL_I;
var CTRL_J = "\x0a";
//var KEY_DOWN = CTRL_J;
var CTRL_K = "\x0b";
var CTRL_L = "\x0c";
var INSERT_LINE = CTRL_L;
var CTRL_M = "\x0d";
var CR = CTRL_M;
var KEY_ENTER = CTRL_M;
var CTRL_N = "\x0e";
var CTRL_O = "\x0f";
var CTRL_P = "\x10";
var CTRL_Q = "\x11";
var XOFF = CTRL_Q;
var CTRL_R = "\x12";
var CTRL_S = "\x13";
var XON = CTRL_S;
var CTRL_T = "\x14";
var CTRL_U = "\x15";
var CTRL_V = "\x16";
var KEY_INSERT = CTRL_V;
var CTRL_W = "\x17";
var CTRL_X = "\x18";
var CTRL_Y = "\x19";
var CTRL_Z = "\x1a";
var KEY_ESC = "\x1b";
var KEY_F1 = "\1F1";
var KEY_F2 = "\1F2";
var KEY_F3 = "\1F3";
var KEY_F4 = "\1F4";
var KEY_F5 = "\1F5";
// PageUp & PageDown keys - Synchronet 3.17 as of about December 18, 2017
// use CTRL-P and CTRL-N for PageUp and PageDown, respectively.  sbbsdefs.js
// defines them as KEY_PAGEUP and KEY_PAGEDN; I've used slightly different names
// in this script so that this script will work with Synchronet systems before
// and after the update containing those key definitions.
var KEY_PAGE_UP = CTRL_P;
var KEY_PAGE_DOWN = CTRL_N;
// Ensure KEY_PAGE_UP and KEY_PAGE_DOWN are set to what's defined in sbbs.js
// for KEY_PAGEUP and KEY_PAGEDN in case they change.  Note that this relies
// on sbbsdefs.js being loaded; SlyEdit.js loads sbbsdefs.js before this file,
// so this should work.
if (typeof(KEY_PAGEUP) === "string")
	KEY_PAGE_UP = KEY_PAGEUP;
if (typeof(KEY_PAGEDN) === "string")
	KEY_PAGE_DOWN = KEY_PAGEDN;

// Store the full path & filename of the Digital Distortion Message
// Lister, since it will be used more than once.
var gDDML_DROP_FILE_NAME = system.node_dir + "DDML_SyncSMBInfo.txt";

var gUserSettingsFilename = backslash(system.data_dir + "user") + format("%04d", user.number) + ".SlyEdit_Settings";

// A regular expression for matching a Synchronet color/attribute code (case-insensitive)
var gSyncAttrRegex = /[krgybmcw01234567hinpq,;\.dtl<>\[\]asz]/i;
// A regular expression to match all only Synchronet attribute codes in a string
var gOnlySyncAttrsInStrRegex = /^([krgybmcw01234567hinpq,;\.dtl<>\[\]asz])+$/i;

///////////////////////////////////////////////////////////////////////////////////
// Object/class stuff

//////
// TextLine stuff

// TextLine object constructor: This is used to keep track of a text line,
// and whether it has a hard newline at the end (i.e., if the user pressed
// enter to break the line).
//
// Parameters (all optional):
//  pText: The text for the line
//  pHardNewlineEnd: Whether or not the line has a "hard newline" - What
//                   this means is that text below it won't be wrapped up
//                   to this line when re-adjusting the text lines.
//  pIsQuoteLine: Whether or not the line is a quote line.
function TextLine(pText, pHardNewlineEnd, pIsQuoteLine)
{
	this.text = "";               // The line text
	this.hardNewlineEnd = false; // Whether or not the line has a hard newline at the end
	this.isQuoteLine = false;    // Whether or not this is a quote line
	// Copy the parameters if they are valid.
	if ((pText != null) && (typeof(pText) == "string"))
		this.text = pText;
	if ((pHardNewlineEnd != null) && (typeof(pHardNewlineEnd) == "boolean"))
		this.hardNewlineEnd = pHardNewlineEnd;
	if ((pIsQuoteLine != null) && (typeof(pIsQuoteLine) == "boolean"))
		this.isQuoteLine = pIsQuoteLine;

	// Functions
	this.displayLength = TextLine_DisplayLength;
	this.print = TextLine_Print;
	this.doMacroTxtReplacement = TextLine_DoMacroTxtReplacement;
	this.getWord = TextLine_GetWord;
	this.substrWithSyncColorCodes = TextLine_SubstrWithSyncColorCodes;
	this.getLastAttrCodes = TextLine_GetLastAttrCodes;
	this.getAttrsAndIndexesBeforeIdx = TextLine_GetAttrsAndIndexesBeforeIdx;
	this.displayIdxToActualIdx = TextLine_DisplayIdxToActualIdx;
}
// For the TextLine class: Returns the length of the text.
function TextLine_DisplayLength()
{
	//return this.text.length;
	return strip_ctrl(this.text).length;
}
// For  the TextLine class: Prints the text line, using its text attributes.
//
// Parameters:
//  pClearToEOL: Boolean - Whether or not to clear to the end of the line
function TextLine_Print(pClearToEOL)
{
	console.print(this.text);

	if (pClearToEOL)
		console.cleartoeol();
}
// Performs text replacement (AKA macro replacement) in the text line.
//
// Parameters:
//  pTxtReplacements: An associative array of text to be replaced (i.e.,
//                    gTxtReplacements)
//  pCharIndex: The current character index in the text line
//  pUseRegex: Whether or not to treat the text replacement search string as a
//             regular expression.
//  pAllowAttrCodes: Boolean - Whether or not to allow attribute codes in
//                   the replacement strings
//  pTextAttrs: The text attribute(s) for the message text.  If there are
//              any color/attribute codes in a replacement text, this will
//              be appended to the end so the rest of the message will be
//              the color it should be.
//
// Return value: An object containing the following properties:
//               textLineIndex: The updated text line index (integer)
//               wordLenDiff: The change in length of the word that
//                            was replaced (integer)
//               displayableWordLenDiff: The change in display length of the
//                                       word that was replaced (integer)
//               wordStartIdx: The index of the first character in the word.
//                             Only valid if a word was found.  Otherwise, this
//                             will be 0.
//               newTextEndIdx: The index of the last character in the new
//                              text.  Only valid if a word was replaced.
//                              Otherwise, this will be 0.
//               newTextLen: The length of the new text in the string.  Will be
//                           the length of the existing word if the word wasn't
//                           replaced or 0 if no word was found.
//               madeTxtReplacement: Whether or not a text replacement was made
//                                   (boolean)
function TextLine_DoMacroTxtReplacement(pTxtReplacements, pCharIndex, pUseRegex, pAllowAttrCodes, pTextAttrs)
{
	var retObj = {
		textLineIndex: pCharIndex,
		wordLenDiff: 0,
		displayableWordLenDiff: 0,
		wordStartIdx: 0,
		newTextEndIdx: 0,
		newTextLen: 0,
		madeTxtReplacement: false
	};

	var allowAttrCodes = (typeof(pAllowAttrCodes) == "boolean" ? pAllowAttrCodes : false);

	var wordObj = this.getWord(retObj.textLineIndex);
	if (wordObj.foundWord)
	{
		retObj.wordStartIdx = wordObj.startIdx;
		retObj.newTextLen = wordObj.word.length;

		// See if the word starts with a capital letter; if so, we'll capitalize
		// the replacement word.
		var firstCharUpper = false;
		var txtReplacement = "";
		if (pUseRegex)
		{
			// Since a regular expression might have more characters in addition
			// to the actual word, we need to go through all the replacement strings
			// in pTxtReplacements and use the first one that changes the text.
			for (var prop in pTxtReplacements)
			{
				if (pTxtReplacements.hasOwnProperty(prop))
				{
					var regex = new RegExp(prop);
					if (allowAttrCodes)
					{
						txtReplacement = wordObj.word.replace(regex, pTxtReplacements[prop]);
						// If the replacement has any Synchronet attribute codes, append the
						// given text attributes to it so the rest of the message is back to
						// normal colors.  // TODO: Dealing with high vs. normal?
						if (gSyncAttrRegex.test(txtReplacement))
							txtReplacement += "\1n" + pTextAttrs;
					}
					else
						txtReplacement = wordObj.word.replace(regex, strip_ctrl(pTxtReplacements[prop]));
					retObj.madeTxtReplacement = (txtReplacement != wordObj.word);
					// If a text replacement was made, then check and see if the first
					// letter in the original text was uppercase, and if so, make the
					// first letter in the new text (txtReplacement) uppercase.
					if (retObj.madeTxtReplacement)
					{
						if (firstLetterIsUppercase(wordObj.word))
						{
							var letterInfo = getFirstLetterFromStr(txtReplacement);
							if (letterInfo.idx > -1)
							{
								txtReplacement = txtReplacement.substr(0, letterInfo.idx)
								               + letterInfo.letter.toUpperCase()
								               + txtReplacement.substr(letterInfo.idx+1);
							}
						}
						// Now that we've made a text replacement, stop going through
						// pTxtReplacements looking for a matching regex.
						break;
					}
				}
			}
		}
		else
		{
			// Not using a regular expression.
			firstCharUpper = (wordObj.word.charAt(0) == wordObj.word.charAt(0).toUpperCase());
			// Convert the word to all uppercase to do the case-insensitive lookup
			// in pTxtReplacements.
			wordObj.word = wordObj.word.toUpperCase();
			if (pTxtReplacements.hasOwnProperty(wordObj.word))
			{
				if (allowAttrCodes)
				{
					txtReplacement = pTxtReplacements[wordObj.word];
					// If the replacement has any Synchronet attribute codes, append the
					// given text attributes to it so the rest of the message is back to
					// normal colors.  // TODO: Dealing with high vs. normal?
					if (gSyncAttrRegex.test(txtReplacement))
						txtReplacement += "\1n" + pTextAttrs;
				}
				else
					txtReplacement = strip_ctrl(pTxtReplacements[wordObj.word]);
				retObj.madeTxtReplacement = true;
			}
		}
		if (retObj.madeTxtReplacement)
		{
			if (firstCharUpper)
				txtReplacement = txtReplacement.charAt(0).toUpperCase() + txtReplacement.substr(1);
			this.text = this.text.substr(0, wordObj.startIdx) + txtReplacement + this.text.substr(wordObj.endIndex+1);
			// Based on the difference in word length, update the data that
			// matters (retObj.textLineIndex, which keeps track of the index of the current line).
			// Note: The horizontal cursor position variable should be replaced after calling this
			// function.
			retObj.wordLenDiff = txtReplacement.length - wordObj.word.length;
			retObj.displayableWordLenDiff = strip_ctrl(txtReplacement).length - strip_ctrl(wordObj.word).length;
			retObj.textLineIndex += retObj.wordLenDiff;
			retObj.newTextEndIdx = wordObj.endIndex + retObj.wordLenDiff;
			retObj.newTextLen = txtReplacement.length;
		}
	}

	return retObj;
}
// Returns the word in a text line at a given index.  If the index
// is at a space, then this function will return the word before
// (to the left of) the space.
//
// Parameters:
//  pEditLinesIndex: The index of the line to look at (0-based)
//  pCharIndex: The character index in the text line (0-based)
//
// Return value: An object containing the following properties:
//               foundWord: Whether or not a word was found (boolean)
//               word: The word in the edit line at the given indexes (text).
//                     This might include control/color codes, etc..
//               plainWord: The word in the edit line without any control
//                          or color codes, etc.  This may or may not be
//                          the same as word.
//               startIdx: The index of the first character of the word (integer)
//               endIndex: The index of the last character of the word (integer)
//                         This includes any control/color codes, etc.
function TextLine_GetWord(pCharIndex)
{
	var retObj = {
		foundWord: false,
		word: "",
		plainWord: "",
		startIdx: 0,
		endIndex: 0
	};

	// Parameter checking
	if ((pCharIndex < 0) || (pCharIndex >= this.text.length))
		return retObj;

	// If pCharIndex specifies the index of a space, then look for a non-space
	// character before it.
	var charIndex = pCharIndex;
	while (this.text.charAt(charIndex) == " ")
		--charIndex;
	// Look for the start & end of the word based on the indexes of a space
	// before and at/after the given character index.
	var wordStartIdx = charIndex;
	var wordEndIdx = charIndex;
	while ((this.text.charAt(wordStartIdx) != " ") && (wordStartIdx >= 0))
		--wordStartIdx;
	++wordStartIdx;
	while ((this.text.charAt(wordEndIdx) != " ") && (wordEndIdx < this.text.length))
		++wordEndIdx;
	--wordEndIdx;

	retObj.foundWord = true;
	retObj.startIdx = wordStartIdx;
	retObj.endIndex = wordEndIdx;
	retObj.word = this.text.substring(wordStartIdx, wordEndIdx+1);
	retObj.plainWord = strip_ctrl(retObj.word);
	return retObj;
}
// For the TextLine class: Gets a substring, handling Synchronet color codes.
// This calls substrWithSyncColorCodes() with the object's text line and
// returns the same value as that function.
//
// Parameters:
//  pStartIdx: The index of where to start in the string, as it appears
//             on the screen
//  pLen: The length of the substring to get.  Optional - If not specified,
//        the rest of the string will be used.
//
// Return value: An object with the following properties:
//               strSub: The substring from the string, including any Synchronet
//                       color codes that may exist preceding the word at pStartIdx
//               startIdx: The actual index in the string where the substring starts
//                         (-1 if an error occurred)
//               endIdx: The actual index in the string where the substring ends.
//                       This is the index of the last character in the substring.
//                       Will be -1 if an error occurred.
//               len: The actual length of the substring
//               printableLen: The length of the string as it would appear on the screen
//                             (i.e., its length without Synchronet attribute codes)
//               syncAttrStartIdx: The starting index of Synchronet attributes
//                                 (-1 if not found or an error occurred)
//               syncAttrEndIdx: The ending index of Synchronet attributes, including
//                               the last character (-1 if not found or an error occurred)
function TextLine_SubstrWithSyncColorCodes(pStartIdx, pLen)
{
	return substrWithSyncColorCodes(this.text, pStartIdx, pLen);
}
// For the TextLine class: Returns the last attribute codes appearing in the line
// before a given index, as a string.  If there are none, it will just be a
// blank string.  The index is optional - If not provided, this method will use
// the end index of the string.
//
// Parameters:
//  pIdx: Optional - The end index of the string to use.  If not provided, this will
//        use the last index of the string.
//
// Return value: Any attribute codes appearing before the given index (or last index)
//               of the string, as a string.
function TextLine_GetLastAttrCodes(pIdx)
{
	var endIdx = (typeof(pIdx) == "number" ? pIdx : this.text.length-1);
	if (endIdx < 0)
		return "";
	if (endIdx >= this.text.length)
		endIdx =  this.text.length - 1;
	return getAttrsBeforeStrIdx(this.text, endIdx);
}
// For the TextLine class: Returns an object with information about any Synchronet
// color/attribute codes found in the text line before a given index.
//
// Parameters:
//  pIdx: The index in the text line to search before
//
// Return value: An object containing the following properties:
//               attrStr: A string containing any Synchronet attribute codes
//                        found before the given index in the given string.  If
//                        none are found, this string will be empty.
//               syncAttrStartIdx: The index of where the attribute codes start, or
//                                 -1 if none were found
//               syncAttrEndIdx: The index of the last character of the attribute
//                               codes, or -1 if none were found
function TextLine_GetAttrsAndIndexesBeforeIdx(pIdx)
{
	return getAttrsAndIndexesBeforeStrIdx(this.text, pIdx);
}

// For the TextLine class: Converts a printable display index to the actual index
// of the text line, ignoring Synchronet attribute codes in the string.
//
// Parameters:
//  pDisplayIdx: The string index as displayed on the screen
//
// Return value: The index in the actual string.  0 on error
function TextLine_DisplayIdxToActualIdx(pDisplayIdx)
{
	return strDisplayIdxToActualIdx(this.text, pDisplayIdx);
}

// AbortConfirmFuncParams constructor: This object contains parameters used by
// the abort confirmation function (actually, there are separate ones for
// IceEdit and DCT Edit styles).
function AbortConfirmFuncParams()
{
	this.editTop = gEditTop;
	this.editBottom = gEditBottom;
	this.editWidth = gEditWidth;
	this.editHeight = gEditHeight;
	this.editLinesIndex = gEditLinesIndex;
	this.displayMessageRectangle = displayMessageRectangle;
}

//////
// ChoiceScrollbox stuff

// Returns the minimum width for a ChoiceScrollbox
function ChoiceScrollbox_MinWidth()
{
	return 73; // To leave room for the navigation text in the bottom border
}

// ChoiceScrollbox constructor
//
// Parameters:
//  pLeftX: The horizontal component (column) of the upper-left coordinate
//  pTopY: The vertical component (row) of the upper-left coordinate
//  pWidth: The width of the box (including the borders)
//  pHeight: The height of the box (including the borders)
//  pTopBorderText: The text to include in the top border
//  pSlyEdCfgObj: The SlyEdit configuration object (color settings are used)
//  pAddTCharsAroundTopText: Optional, boolean - Whether or not to use left & right T characters
//                           around the top border text.  Defaults to true.
// pReplaceTopTextSpacesWithBorderChars: Optional, boolean - Whether or not to replace
//                           spaces in the top border text with border characters.
//                           Defaults to false.
function ChoiceScrollbox(pLeftX, pTopY, pWidth, pHeight, pTopBorderText, pSlyEdCfgObj,
                         pAddTCharsAroundTopText, pReplaceTopTextSpacesWithBorderChars)
{
	// The default is to add left & right T characters around the top border
	// text.  But also use pAddTCharsAroundTopText if it's a boolean.
	var addTopTCharsAroundText = true;
	if (typeof(pAddTCharsAroundTopText) == "boolean")
		addTopTCharsAroundText = pAddTCharsAroundTopText;
	// If pReplaceTopTextSpacesWithBorderChars is true, then replace the spaces
	// in pTopBorderText with border characters.
	if (pReplaceTopTextSpacesWithBorderChars)
	{
		var startIdx = 0;
		var firstSpcIdx = pTopBorderText.indexOf(" ", 0);
		// Look for the first non-space after firstSpaceIdx
		var nonSpcIdx = -1;
		for (var i = firstSpcIdx; (i < pTopBorderText.length) && (nonSpcIdx == -1); ++i)
		{
			if (pTopBorderText.charAt(i) != " ")
				nonSpcIdx = i;
		}
		var firstStrPart = "";
		var lastStrPart = "";
		var numSpaces = 0;
		while ((firstSpcIdx > -1) && (nonSpcIdx > -1))
		{
			firstStrPart = pTopBorderText.substr(startIdx, (firstSpcIdx-startIdx));
			lastStrPart = pTopBorderText.substr(nonSpcIdx);
			numSpaces = nonSpcIdx - firstSpcIdx;
			if (numSpaces > 0)
			{
				pTopBorderText = firstStrPart + "\1n" + pSlyEdCfgObj.genColors.listBoxBorder;
				for (var i = 0; i < numSpaces; ++i)
					pTopBorderText += HORIZONTAL_SINGLE;
				pTopBorderText += "\1n" + pSlyEdCfgObj.genColors.listBoxBorderText + lastStrPart;
			}

			// Look for the next space and non-space character after that.
			firstSpcIdx = pTopBorderText.indexOf(" ", nonSpcIdx);
			// Look for the first non-space after firstSpaceIdx
			nonSpcIdx = -1;
			for (var i = firstSpcIdx; (i < pTopBorderText.length) && (nonSpcIdx == -1); ++i)
			{
				if (pTopBorderText.charAt(i) != " ")
					nonSpcIdx = i;
			}
		}
	}

	this.SlyEdCfgObj = pSlyEdCfgObj;

	var minWidth = ChoiceScrollbox_MinWidth();

	this.dimensions = new Object();
	this.dimensions.topLeftX = pLeftX;
	this.dimensions.topLeftY = pTopY;
	// Make sure the width is the minimum width
	if ((pWidth < 0) || (pWidth < minWidth))
		this.dimensions.width = minWidth;
	else
		this.dimensions.width = pWidth;
	this.dimensions.height = pHeight;
	this.dimensions.bottomRightX = this.dimensions.topLeftX + this.dimensions.width - 1;
	this.dimensions.bottomRightY = this.dimensions.topLeftY + this.dimensions.height - 1;

	// The text item array and member variables relating to it and the items
	// displayed on the screen during the input loop
	this.txtItemList = new Array();
	this.chosenTextItemIndex = -1;
	this.topItemIndex = 0;
	this.bottomItemIndex = 0;

	// Top border string
	var innerBorderWidth = this.dimensions.width - 2;
	// Calculate the maximum top border text length to account for the left/right
	// T chars and "Page #### of ####" text
	var maxTopBorderTextLen = innerBorderWidth - (pAddTCharsAroundTopText ? 21 : 19);
	if (strip_ctrl(pTopBorderText).length > maxTopBorderTextLen)
	pTopBorderText = pTopBorderText.substr(0, maxTopBorderTextLen);
		this.topBorder = "\1n" + pSlyEdCfgObj.genColors.listBoxBorder + UPPER_LEFT_SINGLE;
	if (addTopTCharsAroundText)
		this.topBorder += RIGHT_T_SINGLE;
	this.topBorder += "\1n" + pSlyEdCfgObj.genColors.listBoxBorderText
	               + pTopBorderText + "\1n" + pSlyEdCfgObj.genColors.listBoxBorder;
	if (addTopTCharsAroundText)
		this.topBorder += LEFT_T_SINGLE;
	const topBorderTextLen = strip_ctrl(pTopBorderText).length;
	var numHorizBorderChars = innerBorderWidth - topBorderTextLen - 20;
	if (addTopTCharsAroundText)
		numHorizBorderChars -= 2;
	for (var i = 0; i <= numHorizBorderChars; ++i)
		this.topBorder += HORIZONTAL_SINGLE;
	this.topBorder += RIGHT_T_SINGLE + "\1n" + pSlyEdCfgObj.genColors.listBoxBorderText
	               + "Page    1 of    1" + "\1n" + pSlyEdCfgObj.genColors.listBoxBorder + LEFT_T_SINGLE
	               + UPPER_RIGHT_SINGLE;

	// Bottom border string
	this.btmBorderNavText = "nhcb, cb, cNy)bext, cPy)brev, "
	                      + "cFy)birst, cLy)bast, cHOMEb, cENDb, cEntery=bSelect, "
	                      + "cESCnc/hcQy=bEnd";
	this.bottomBorder = "n" + pSlyEdCfgObj.genColors.listBoxBorder + LOWER_LEFT_SINGLE
	                  + RIGHT_T_SINGLE + this.btmBorderNavText + "n" + pSlyEdCfgObj.genColors.listBoxBorder
	                  + LEFT_T_SINGLE;
	var numCharsRemaining = this.dimensions.width - strip_ctrl(this.btmBorderNavText).length - 6;
	for (var i = 0; i < numCharsRemaining; ++i)
		this.bottomBorder += HORIZONTAL_SINGLE;
	this.bottomBorder += LOWER_RIGHT_SINGLE;

	// Item format strings
	this.listIemFormatStr = "n" + pSlyEdCfgObj.genColors.listBoxItemText + "%-" + +(this.dimensions.width-2) + "s";
	this.listIemHighlightFormatStr = "n" + pSlyEdCfgObj.genColors.listBoxItemHighlight + "%-" + +(this.dimensions.width-2) + "s";

	// Key functionality override function pointers
	this.enterKeyOverrideFn = null;

	// inputLoopeExitKeys is an object containing additional keypresses that will
	// exit the input loop.
	this.inputLoopExitKeys = new Object();

	// "Class" functions
	this.addTextItem = ChoiceScrollbox_AddTextItem; // Returns the index of the item
	this.getTextItem = ChoiceScrollbox_GetTextIem;
	this.replaceTextItem = ChoiceScrollbox_ReplaceTextItem;
	this.delTextItem = ChoiceScrollbox_DelTextItem;
	this.chgCharInTextItem = ChoiceScrollbox_ChgCharInTextItem;
	this.getChosenTextItemIndex = ChoiceScrollbox_GetChosenTextItemIndex;
	this.setItemArray = ChoiceScrollbox_SetItemArray; // Sets the item array; returns whether or not it was set.
	this.clearItems = ChoiceScrollbox_ClearItems; // Empties the array of items
	this.setEnterKeyOverrideFn = ChoiceScrollbox_SetEnterKeyOverrideFn;
	this.clearEnterKeyOverrideFn = ChoiceScrollbox_ClearEnterKeyOverrideFn;
	this.addInputLoopExitKey = ChoiceScrollbox_AddInputLoopExitKey;
	this.setBottomBorderText = ChoiceScrollbox_SetBottomBorderText;
	this.drawBorder = ChoiceScrollbox_DrawBorder;
	this.refreshItemCharOnScreen = ChoiceScrollbox_RefreshItemCharOnScreen;
	// Does the input loop.  Returns an object with the following properties:
	//  itemWasSelected: Boolean - Whether or not an item was selected
	//  selectedIndex: The index of the selected item
	//  selectedItem: The text of the selected item
	//  lastKeypress: The last key pressed by the user
	this.doInputLoop = ChoiceScrollbox_DoInputLoop;
}
function ChoiceScrollbox_AddTextItem(pTextLine, pStripCtrl)
{
	var stripCtrl = true;
	if (typeof(pStripCtrl) == "boolean")
		stripCtrl = pStripCtrl;

	if (stripCtrl)
		this.txtItemList.push(strip_ctrl(pTextLine));
	else
		this.txtItemList.push(pTextLine);
	// Return the index of the added item
	return this.txtItemList.length-1;
}
function ChoiceScrollbox_GetTextIem(pItemIndex)
{
	if (typeof(pItemIndex) != "number")
		return "";
	if ((pItemIndex < 0) || (pItemIndex >= this.txtItemList.length))
		return "";

	return this.txtItemList[pItemIndex];
}
function ChoiceScrollbox_ReplaceTextItem(pItemIndexOrStr, pNewItem)
{
	if (typeof(pNewItem) != "string")
		return false;

	// Find the item index
	var itemIndex = -1;
	if (typeof(pItemIndexOrStr) == "number")
	{
		if ((pItemIndexOrStr < 0) || (pItemIndexOrStr >= this.txtItemList.length))
			return false;
		else
			itemIndex = pItemIndexOrStr;
	}
	else if (typeof(pItemIndexOrStr) == "string")
	{
		itemIndex = -1;
		for (var i = 0; (i < this.txtItemList.length) && (itemIndex == -1); ++i)
		{
			if (this.txtItemList[i] == pItemIndexOrStr)
				itemIndex = i;
		}
	}
	else
		return false;

	// Replace the item
	var replacedIt = false;
	if ((itemIndex > -1) && (itemIndex < this.txtItemList.length))
	{
		this.txtItemList[itemIndex] = pNewItem;
			replacedIt = true;
	}
	return replacedIt;
}
function ChoiceScrollbox_DelTextItem(pItemIndexOrStr)
{
   // Find the item index
   var itemIndex = -1;
   if (typeof(pItemIndexOrStr) == "number")
   {
      if ((pItemIndexOrStr < 0) || (pItemIndexOrStr >= this.txtItemList.length))
         return false;
      else
         itemIndex = pItemIndexOrStr;
   }
   else if (typeof(pItemIndexOrStr) == "string")
   {
      itemIndex = -1;
      for (var i = 0; (i < this.txtItemList.length) && (itemIndex == -1); ++i)
      {
         if (this.txtItemList[i] == pItemIndexOrStr)
            itemIndex = i;
      }
   }
   else
      return false;

   // Remove the item
   var removedIt = false;
   if ((itemIndex > -1) && (itemIndex < this.txtItemList.length))
   {
      this.txtItemList = this.txtItemList.splice(itemIndex, 1);
      removedIt = true;
   }
   return removedIt;
}
function ChoiceScrollbox_ChgCharInTextItem(pItemIndexOrStr, pStrIndex, pNewText)
{
   // Find the item index
   var itemIndex = -1;
   if (typeof(pItemIndexOrStr) == "number")
   {
      if ((pItemIndexOrStr < 0) || (pItemIndexOrStr >= this.txtItemList.length))
         return false;
      else
         itemIndex = pItemIndexOrStr;
   }
   else if (typeof(pItemIndexOrStr) == "string")
   {
      itemIndex = -1;
      for (var i = 0; (i < this.txtItemList.length) && (itemIndex == -1); ++i)
      {
         if (this.txtItemList[i] == pItemIndexOrStr)
            itemIndex = i;
      }
   }
   else
      return false;

   // Change the character in the item
   var changedIt = false;
   if ((itemIndex > -1) && (itemIndex < this.txtItemList.length))
   {
      this.txtItemList[itemIndex] = chgCharInStr(this.txtItemList[itemIndex], pStrIndex, pNewText);
      changedIt = true;
   }
   return changedIt;
}
function ChoiceScrollbox_GetChosenTextItemIndex()
{
   return this.chosenTextItemIndex;
}
function ChoiceScrollbox_SetItemArray(pArray, pStripCtrl)
{
   var safeToSet = false;
   if (Object.prototype.toString.call(pArray) === "[object Array]")
   {
      if (pArray.length > 0)
         safeToSet = (typeof(pArray[0]) == "string");
      else
         safeToSet = true; // It's safe to set an empty array
   }

   if (safeToSet)
   {
      delete this.txtItemList;
      this.txtItemList = pArray;

      var stripCtrl = true;
      if (typeof(pStripCtrl) == "boolean")
         stripCtrl = pStripCtrl;
      if (stripCtrl)
      {
         // Remove attribute/color characters from the text lines in the array
         for (var i = 0; i < this.txtItemList.length; ++i)
            this.txtItemList[i] = strip_ctrl(this.txtItemList[i]);
      }
   }

   return safeToSet;
}
function ChoiceScrollbox_ClearItems()
{
   this.txtItemList.length = 0;
}
function ChoiceScrollbox_SetEnterKeyOverrideFn(pOverrideFn)
{
   if (Object.prototype.toString.call(pOverrideFn) == "[object Function]")
      this.enterKeyOverrideFn = pOverrideFn;
}
function ChoiceScrollbox_ClearEnterKeyOverrideFn()
{
   this.enterKeyOverrideFn = null;
}
function ChoiceScrollbox_AddInputLoopExitKey(pKeypress)
{
   this.inputLoopExitKeys[pKeypress] = true;
}
function ChoiceScrollbox_SetBottomBorderText(pText, pAddTChars, pAutoStripIfTooLong)
{
	if (typeof(pText) != "string")
		return;

	const innerWidth = (pAddTChars ? this.dimensions.width-4 : this.dimensions.width-2);

	if (pAutoStripIfTooLong)
	{
		if (strip_ctrl(pText).length > innerWidth)
			pText = pText.substr(0, innerWidth);
	}

	// Re-build the bottom border string based on the new text
	this.bottomBorder = "n" + this.SlyEdCfgObj.genColors.listBoxBorder + LOWER_LEFT_SINGLE;
	if (pAddTChars)
		this.bottomBorder += RIGHT_T_SINGLE;
	if (pText.indexOf("n") != 0)
		this.bottomBorder += "n";
	this.bottomBorder += pText + "n" + this.SlyEdCfgObj.genColors.listBoxBorder;
		if (pAddTChars)
	this.bottomBorder += LEFT_T_SINGLE;
	var numCharsRemaining = this.dimensions.width - strip_ctrl(this.bottomBorder).length - 3;
	for (var i = 0; i < numCharsRemaining; ++i)
		this.bottomBorder += HORIZONTAL_SINGLE;
	this.bottomBorder += LOWER_RIGHT_SINGLE;
}
function ChoiceScrollbox_DrawBorder()
{
	console.gotoxy(this.dimensions.topLeftX, this.dimensions.topLeftY);
	console.print(this.topBorder);
	// Draw the side border characters
	var screenRow = this.dimensions.topLeftY + 1;
	for (var screenRow = this.dimensions.topLeftY+1; screenRow <= this.dimensions.bottomRightY-1; ++screenRow)
	{
		console.gotoxy(this.dimensions.topLeftX, screenRow);
		console.print(VERTICAL_SINGLE);
		console.gotoxy(this.dimensions.bottomRightX, screenRow);
		console.print(VERTICAL_SINGLE);
	}
	// Draw the bottom border
	console.gotoxy(this.dimensions.topLeftX, this.dimensions.bottomRightY);
	console.print(this.bottomBorder);
}
function ChoiceScrollbox_RefreshItemCharOnScreen(pItemIndex, pCharIndex)
{
	if ((typeof(pItemIndex) != "number") || (typeof(pCharIndex) != "number"))
		return;
	if ((pItemIndex < 0) || (pItemIndex >= this.txtItemList.length) ||
	    (pItemIndex < this.topItemIndex) || (pItemIndex > this.bottomItemIndex))
	{
		return;
	}
	if ((pCharIndex < 0) || (pCharIndex >= this.txtItemList[pItemIndex].length))
		return;

	// Save the current cursor position so that we can restore it later
	const originalCurpos = console.getxy();
	// Go to the character's position on the screen and set the highlight or
	// normal color, depending on whether the item is the currently selected item,
	// then print the character on the screen.
	const charScreenX = this.dimensions.topLeftX + 1 + pCharIndex;
	const itemScreenY = this.dimensions.topLeftY + 1 + (pItemIndex - this.topItemIndex);
	console.gotoxy(charScreenX, itemScreenY);
	if (pItemIndex == this.chosenTextItemIndex)
		console.print(this.SlyEdCfgObj.genColors.listBoxItemHighlight);
	else
		console.print(this.SlyEdCfgObj.genColors.listBoxItemText);
	console.print(this.txtItemList[pItemIndex].charAt(pCharIndex));
	// Move the cursor back to where it was originally
	console.gotoxy(originalCurpos);
}
function ChoiceScrollbox_DoInputLoop(pDrawBorder)
{
	var retObj = {
		itemWasSelected: false,
		selectedIndex: -1,
		selectedItem: "",
		lastKeypress: ""
	};

	// Don't do anything if the item list doesn't contain any items
	if (this.txtItemList.length == 0)
		return retObj;

	//////////////////////////////////
	// Locally-defined functions

	// This function returns the index of the bottommost item that
	// can be displayed in the box.
	//
	// Parameters:
	//  pArray: The array containing the items
	//  pTopindex: The index of the topmost item displayed in the box
	//  pNumItemsPerPage: The number of items per page
	function getBottommostItemIndex(pArray, pTopIndex, pNumItemsPerPage)
	{
		var bottomIndex = pTopIndex + pNumItemsPerPage - 1;
		// If bottomIndex is beyond the last index, then adjust it.
		if (bottomIndex >= pArray.length)
			bottomIndex = pArray.length - 1;
		return bottomIndex;
	}



	//////////////////////////////////
	// Code

	// Variables for keeping track of the item list
	const numItemsPerPage = this.dimensions.height - 2;
	this.topItemIndex = 0;    // The index of the message group at the top of the list
	// Figure out the index of the last message group to appear on the screen.
	this.bottomItemIndex = getBottommostItemIndex(this.txtItemList, this.topItemIndex, numItemsPerPage);
	const numPages = Math.ceil(this.txtItemList.length / numItemsPerPage);
	const topIndexForLastPage = (numItemsPerPage * numPages) - numItemsPerPage;

	if (pDrawBorder)
		this.drawBorder();

	// User input loop
	// For the horizontal location of the page number text for the box border:
	// Based on the fact that there can be up to 9999 text replacements and 10
	// per page, there will be up to 1000 pages of replacements.  To write the
	// text, we'll want to be 20 characters to the left of the end of the border
	// of the box.
	const pageNumTxtStartX = this.dimensions.topLeftX + this.dimensions.width - 19;
	const maxItemWidth = this.dimensions.width - 2;
	var pageNum = 0;
	var startArrIndex = 0;
	this.chosenTextItemIndex = retObj.selectedIndex = 0;
	var endArrIndex = 0; // One past the last array item
	var screenY = 0;
	var curpos = new Object(); // For keeping track of the current cursor position
	curpos.x = 0;
	curpos.y = 0;
	var refreshList = true; // For screen redraw optimizations
	var continueOn = true;
	while (continueOn)
	{
		if (refreshList)
		{
			this.bottomItemIndex = getBottommostItemIndex(this.txtItemList, this.topItemIndex, numItemsPerPage);

			// Write the list of items for the current page
			startArrIndex = pageNum * numItemsPerPage;
			endArrIndex = startArrIndex + numItemsPerPage;
			if (endArrIndex > this.txtItemList.length)
				endArrIndex = this.txtItemList.length;
			var selectedItemRow = this.dimensions.topLeftY+1;
			screenY = this.dimensions.topLeftY + 1;
			for (var i = startArrIndex; i < endArrIndex; ++i)
			{
				console.gotoxy(this.dimensions.topLeftX+1, screenY);
				if (i == retObj.selectedIndex)
				{
					printf(this.listIemHighlightFormatStr, this.txtItemList[i].substr(0, maxItemWidth));
					selectedItemRow = screenY;
				}
				else
					printf(this.listIemFormatStr, this.txtItemList[i].substr(0, maxItemWidth));
				++screenY;
			}
			// If the current screen row is below the bottom row inside the box,
			// continue and write blank lines to the bottom of the inside of the box
			// to blank out any text that might still be there.
			while (screenY < this.dimensions.topLeftY+this.dimensions.height-1)
			{
				console.gotoxy(this.dimensions.topLeftX+1, screenY);
				printf(this.listIemFormatStr, "");
				++screenY;
			}

			// Update the page number in the top border of the box.
			console.gotoxy(pageNumTxtStartX, this.dimensions.topLeftY);
			printf("\1n" + this.SlyEdCfgObj.genColors.listBoxBorderText + "Page %4d of %4d", pageNum+1, numPages);

			// Just for sane appearance: Move the cursor to the first character of
			// the currently-selected row and set the appropriate color.
			curpos.x = this.dimensions.topLeftX+1;
			curpos.y = selectedItemRow;
			console.gotoxy(curpos.x, curpos.y);
			console.print(this.SlyEdCfgObj.genColors.listBoxItemHighlight);

			refreshList = false;
		}

		// Get a key from the user (upper-case) and take action based upon it.
		retObj.lastKeypress = getKeyWithESCChars(K_UPPER|K_NOCRLF|K_NOSPIN, this.SlyEdCfgObj);
		switch (retObj.lastKeypress)
		{
			case 'N': // Next page
			case KEY_PAGE_DOWN:
				refreshList = (pageNum < numPages-1);
				if (refreshList)
				{
					++pageNum;
					this.topItemIndex += numItemsPerPage;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case 'P': // Previous page
			case KEY_PAGE_UP:
				refreshList = (pageNum > 0);
				if (refreshList)
				{
					--pageNum;
					this.topItemIndex -= numItemsPerPage;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case 'F': // First page
				refreshList = (pageNum > 0);
				if (refreshList)
				{
					pageNum = 0;
					this.topItemIndex = 0;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case 'L': // Last page
				refreshList = (pageNum < numPages-1);
				if (refreshList)
				{
					pageNum = numPages-1;
					this.topItemIndex = topIndexForLastPage;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case KEY_UP:
				// Move the cursor up one item
				if (retObj.selectedIndex > 0)
				{
					// If the previous item index is on the previous page, then we'll
					// want to display the previous page.
					var previousItemIndex = retObj.selectedIndex - 1;
					if (previousItemIndex < this.topItemIndex)
					{
						--pageNum;
						this.topItemIndex -= numItemsPerPage;
						// Note: this.bottomItemIndex is refreshed at the top of the loop
						refreshList = true;
					}
					else
					{
						// Display the current line un-highlighted
						console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
						printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, maxItemWidth));
						// Display the previous line highlighted
						curpos.x = this.dimensions.topLeftX+1;
						--curpos.y;
						console.gotoxy(curpos);
						printf(this.listIemHighlightFormatStr, this.txtItemList[previousItemIndex].substr(0, maxItemWidth));
						console.gotoxy(curpos); // Move the cursor into place where it should be
						refreshList = false;
					}
					this.chosenTextItemIndex = retObj.selectedIndex = previousItemIndex;
				}
				break;
			case KEY_DOWN:
				// Move the cursor down one item
				if (retObj.selectedIndex < this.txtItemList.length - 1)
				{
					// If the next item index is on the next page, then we'll want to
					// display the next page.
					var nextItemIndex = retObj.selectedIndex + 1;
					if (nextItemIndex > this.bottomItemIndex)
					{
						++pageNum;
						this.topItemIndex += numItemsPerPage;
						// Note: this.bottomItemIndex is refreshed at the top of the loop
						refreshList = true;
					}
					else
					{
						// Display the current line un-highlighted
						console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
						printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, maxItemWidth));
						// Display the previous line highlighted
						curpos.x = this.dimensions.topLeftX+1;
						++curpos.y;
						console.gotoxy(curpos);
						printf(this.listIemHighlightFormatStr, this.txtItemList[nextItemIndex].substr(0, maxItemWidth));
						console.gotoxy(curpos); // Move the cursor into place where it should be
						refreshList = false;
					}
					this.chosenTextItemIndex = retObj.selectedIndex = nextItemIndex;
				}
				break;
			case KEY_HOME: // Go to the first row in the box
				if (retObj.selectedIndex > this.topItemIndex)
				{
					// Display the current line un-highlighted
					console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
					printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, maxItemWidth));
					// Select the top item, and display it highlighted.
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					curpos.x = this.dimensions.topLeftX+1;
					curpos.y = this.dimensions.topLeftY+1;
					console.gotoxy(curpos);
					printf(this.listIemHighlightFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, maxItemWidth));
					console.gotoxy(curpos); // Move the cursor into place where it should be
					refreshList = false;
				}
				break;
			case KEY_END: // Go to the last row in the box
				if (retObj.selectedIndex < this.bottomItemIndex)
				{
					// Display the current line un-highlighted
					console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
					printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, maxItemWidth));
					// Select the bottommost item, and display it highlighted.
					this.chosenTextItemIndex = retObj.selectedIndex = this.bottomItemIndex;
					curpos.x = this.dimensions.topLeftX+1;
					curpos.y = this.dimensions.bottomRightY-1;
					console.gotoxy(curpos);
					printf(this.listIemHighlightFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, maxItemWidth));
					console.gotoxy(curpos); // Move the cursor into place where it should be
					refreshList = false;
				}
				break;
			case KEY_ENTER:
				// If the enter key override function is set, then call it and pass
				// this object into it.  Otherwise, just select the item and quit.
				if (this.enterKeyOverrideFn !== null)
				this.enterKeyOverrideFn(this);
				else
				{
					retObj.itemWasSelected = true;
					// Note: retObj.selectedIndex is already set.
					retObj.selectedItem = this.txtItemList[retObj.selectedIndex];
					refreshList = false;
					continueOn = false;
				}
				break;
			case KEY_ESC: // Quit
			case CTRL_A:  // Quit
			case 'Q':     // Quit
				this.chosenTextItemIndex = retObj.selectedIndex = -1;
				refreshList = false;
				continueOn = false;
				break;
			default:
				// If the keypress is an additional key to exit the input loop, then
				// do so.
				if (this.inputLoopExitKeys.hasOwnProperty(retObj.lastKeypress))
				{
					this.chosenTextItemIndex = retObj.selectedIndex = -1;
					refreshList = false;
					continueOn = false;
				}
				else
				{
					// Unrecognized command.  Don't refresh the list of the screen.
					refreshList = false;
				}
				break;
		}
	}

	console.print("\1n"); // To prevent outputting highlight colors, etc..
	return retObj;
}


///////////////////////////////////////////////////////////////////////////////////
// Functions

// This function takes a string and returns a copy of the string
// with a color randomly alternating between dim & bright versions.
//
// Parameters:
//  pString: The string to convert
//  pColor: The color to use (Synchronet color code)
function randomDimBrightString(pString, pColor)
{
	// Return if an invalid string is passed in.
	if (typeof(pString) != "string")
		return "";

	// Set the color.  Default to green.
	var color = (typeof(pColor) == "string" ? pColor : "\1g");
	return(randomTwoColorString(pString, "\1n" + color, "\1n\1h" + color));
}

// This function takes a string and returns a copy of the string
// with colors randomly alternating between two given colors.
//
// Parameters:
//  pString: The string to convert
//  pColor11: The first color to use (Synchronet color code)
//  pColor12: The second color to use (Synchronet color code)
//
// Return value: The string passed in, with the 2 colors used randomly for
//               each character
function randomTwoColorString(pString, pColor1, pColor2)
{
	// Return if an invalid string is passed in.
	if (typeof(pString) != "string")
		return "";

	// Set the colors.  Default to green.
	var color1 = (typeof(pColor1) == "string" ? pColor1 : "\1n\1g");
	var color2 = (typeof(pColor2) == "string" ? pColor2 : "\1n\1g\1h");

	// Create a copy of the string without any control characters,
	// and then add our coloring to it.
	pString = strip_ctrl(pString);
	var returnString = color1;
	var useColor1 = false;     // Whether or not to use the useColor1 version of the color1
	var oldUseColor1 = useColor1; // The value of useColor1 from the last pass
	for (var i = 0; i < pString.length; ++i)
	{
		// Determine if this character should be useColor1
		useColor1 = (Math.floor(Math.random()*2) == 1);
		if (useColor1 != oldUseColor1)
			returnString += (useColor1 ? color1 : color2);

		// Append the character from pString.
		returnString += pString.charAt(i);

		oldUseColor1 = useColor1;
	}

	return returnString;
}

// Returns the current time as a string, to be displayed on the screen.
function getCurrentTimeStr()
{
	return strftime("%I:%M%p", time()).replace("AM", "a").replace("PM", "p");
}

// Returns whether or not a character is printable.
function isPrintableChar(pText)
{
	// Make sure pText is valid and is a string.
	if (typeof(pText) != "string")
		return false;
	if (pText.length == 0)
		return false;

	// Make sure the character is a printable ASCII character in the range of 32 to 254,
	// except for 127 (delete).
	var charCode = pText.charCodeAt(0);
	return ((charCode > 31) && (charCode < 255) && (charCode != 127));
}

// Removes multiple, leading, and/or trailing spaces
// The search & replace regular expressions used in this
// function came from the following URL:
// http://qodo.co.uk/blog/javascript-trim-leading-and-trailing-spaces
//
// Parameters:
//  pString: The string to trim
//  pLeading: Whether or not to trim leading spaces (optional, defaults to true)
//  pMultiple: Whether or not to trim multiple spaces (optional, defaults to true)
//  pTrailing: Whether or not to trim trailing spaces (optional, defaults to true)
//
// Return value: The trimmed string
function trimSpaces(pString, pLeading, pMultiple, pTrailing)
{
	// Make sure pString is a string.
	if (typeof(pString) == "string")
	{
		var leading = true;
		var multiple = true;
		var trailing = true;
		if(typeof(pLeading) != "undefined")
			leading = pLeading;
		if(typeof(pMultiple) != "undefined")
			multiple = pMultiple;
		if(typeof(pTrailing) != "undefined")
			trailing = pTrailing;

		// To remove both leading & trailing spaces:
		//pString = pString.replace(/(^\s*)|(\s*$)/gi,"");

		if (leading)
			pString = pString.replace(/(^\s*)/gi,"");
		if (multiple)
			pString = pString.replace(/[ ]{2,}/gi," ");
		if (trailing)
			pString = pString.replace(/(\s*$)/gi,"");
	}

	return pString;
}

// Displays the text to display above help screens.
function displayHelpHeader()
{
   // Construct the header text lines only once.
   if (typeof(displayHelpHeader.headerLines) == "undefined")
   {
      displayHelpHeader.headerLines = new Array();

      var headerText = EDITOR_PROGRAM_NAME + " Help w(y"
                      + (EDITOR_STYLE == "DCT" ? "DCT" : "Ice")
                      + " modew)";
      var headerTextLen = strip_ctrl(headerText).length;

      // Top border
      var headerTextStr = "nhc" + UPPER_LEFT_SINGLE;
      for (var i = 0; i < headerTextLen + 2; ++i)
         headerTextStr += HORIZONTAL_SINGLE;
      headerTextStr += UPPER_RIGHT_SINGLE;
      displayHelpHeader.headerLines.push(headerTextStr);

      // Middle line: Header text string
      headerTextStr = VERTICAL_SINGLE + "4y " + headerText + " nhc"
                    + VERTICAL_SINGLE;
      displayHelpHeader.headerLines.push(headerTextStr);

      // Lower border
      headerTextStr = LOWER_LEFT_SINGLE;
      for (var i = 0; i < headerTextLen + 2; ++i)
         headerTextStr += HORIZONTAL_SINGLE;
      headerTextStr += LOWER_RIGHT_SINGLE;
      displayHelpHeader.headerLines.push(headerTextStr);
   }

   // Print the header strings
   for (var index in displayHelpHeader.headerLines)
      console.center(displayHelpHeader.headerLines[index]);
}

// Displays the command help.
//
// Parameters:
//  pDisplayHeader: Whether or not to display the help header.
//  pClear: Whether or not to clear the screen first
//  pPause: Whether or not to pause at the end
//  pCanCrossPost: Whether or not cross-posting is enabled
//  pConfigSettings: The SlyEdit configuration settings object
function displayCommandList(pDisplayHeader, pClear, pPause, pCanCrossPost, pConfigSettings)
{
	if (pClear)
		console.clear("\1n");
	if (pDisplayHeader)
	{
		displayHelpHeader();
		console.crlf();
	}

	var isSysop = (pConfigSettings.userIsSysop != null ? pConfigSettings.userIsSysop : user.compare_ars("SYSOP"));

	// This function displays a key and its description with formatting & colors.
	//
	// Parameters:
	//  pKey: The key description
	//  pDesc: The description of the key's function
	//  pCR: Whether or not to display a carriage return (boolean).  Optional;
	//       if not specified, this function won't display a CR.
	function displayCmdKeyFormatted(pKey, pDesc, pCR)
	{
		printf("\1c\1h%-13s\1g: \1n\1c%s", pKey, pDesc);
		if (pCR)
			console.crlf();
	}
	// This function does the same, but outputs 2 on the same line.
	function displayCmdKeyFormattedDouble(pKey, pDesc, pKey2, pDesc2, pCR)
	{
		var sepChar1 = ":";
		var sepChar2 = ":";
		if ((pKey.length == 0) && (pDesc.length == 0))
			sepChar1 = " ";
		if ((pKey2.length == 0) && (pDesc2.length == 0))
			sepChar2 = " ";
		printf("\1c\1h%-13s\1g" + sepChar1 + " \1n\1c%-28s \1k\1h" + VERTICAL_SINGLE +
		       " \1c\1h%-8s\1g" + sepChar2 + " \1n\1c%s", pKey, pDesc, pKey2, pDesc2);
		if (pCR)
			console.crlf();
	}

	// Help keys and slash commands
	printf("\1n\1g%-44s  %-33s\r\n", "Help keys", "Slash commands (on blank line)");
	printf("\1k\1h%-44s  %-33s\r\n", "ÄÄÄÄÄÄÄÄÄ", "ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
	displayCmdKeyFormattedDouble("Ctrl-G", "General help", "/A", "Abort", true);
	displayCmdKeyFormattedDouble("Ctrl-L", "Command key list (this list)", "/S", "Save", true);
	displayCmdKeyFormattedDouble("Ctrl-R", "Program information", "/Q", "Quote message", true);
	if (pConfigSettings.enableTextReplacements)
		displayCmdKeyFormattedDouble("Ctrl-T", "List text replacements", "/T", "List text replacements", true);
	if (pConfigSettings.allowUserSettings)
		displayCmdKeyFormattedDouble("", "", "/U", "Your settings", true);
	if (pCanCrossPost)
		displayCmdKeyFormattedDouble("", "", "/C", "Cross-post selection", true);
	printf(" \1c\1h%-7s\1g  \1n\1c%s", "", "", "/?", "Show help");
	console.crlf();
	// Command/edit keys
	console.print("\1n\1gCommand/edit keys\r\n\1k\1hÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\r\n");
	displayCmdKeyFormattedDouble("Ctrl-A", "Abort message", "PageUp", "Page up", true);
	displayCmdKeyFormattedDouble("Ctrl-Z", "Save message", "PageDown", "Page down", true);
	displayCmdKeyFormattedDouble("Ctrl-Q", "Quote message", "Ctrl-S", "Search for text", true);
	displayCmdKeyFormattedDouble("Insert/Ctrl-I", "Toggle insert/overwrite mode",
	                             "Ctrl-D", "Delete line", true);
	if (pCanCrossPost)
		displayCmdKeyFormattedDouble("ESC", "Command menu", "Ctrl-C", "Cross-post selection", true);
	else
		displayCmdKeyFormatted("ESC", "Command menu", true);
	var nextKeys = [];
	if (pConfigSettings.allowUserSettings)
		nextKeys.push({keyStr: "Ctrl-U", desc: "Your settings"});
	if (gConfigSettings.allowColorSelection)
		nextKeys.push({keyStr: "Ctrl-K", desc: "Choose text color"});
	if (nextKeys.length == 1)
		displayCmdKeyFormatted(nextKeys[0].keyStr, nextKeys[0].desc, true);
	else if (nextKeys.length == 2)
		displayCmdKeyFormattedDouble(nextKeys[0].keyStr, nextKeys[0].desc, nextKeys[1].keyStr, nextKeys[1].desc, true);
	if (isSysop)
		displayCmdKeyFormattedDouble("Ctrl-O", "Import a file", "Ctrl-X", "Export to file", true);

	if (pPause)
	{
		// TODO: I doubt this needs consolePauseWithESCChars() anymore..
		// Should be able to use console.pause(), which easily supports
		// custom pause scripts being loaded.
		//consolePauseWithESCChars();
		console.pause();
	}
}

// Displays the general help screen.
//
// Parameters:
//  pDisplayHeader: Whether or not to display the help header.
//  pClear: Whether or not to clear the screen first
//  pPause: Whether or not to pause at the end
function displayGeneralHelp(pDisplayHeader, pClear, pPause)
{
   if (pClear)
      console.clear("\1n");
   if (pDisplayHeader)
      displayHelpHeader();

   console.print("\1n\1cSlyEdit is a full-screen message editor that mimics the look & feel of\r\n");
   console.print("IceEdit or DCT Edit, two popular editors.  The editor is currently in " +
                 (EDITOR_STYLE == "DCT" ? "DCT" : "Ice") + "\r\nmode.\r\n");
   console.print("At the top of the screen, information about the message being written (or\r\n");
   console.print("file being edited) is displayed.  The middle section is the edit area,\r\n");
   console.print("where the message/file is edited.  Finally, the bottom section displays\r\n");
   console.print("some of the most common keys and/or status.");
   console.crlf();
   if (pPause)
      console.pause();
}

// Displays program information.
//
// Parameters:
//  pClear: Whether or not to clear the screen first
//  pPause: Whether or not to pause at the end
function displayProgramInfo(pClear, pPause)
{
   if (pClear)
      console.clear("n");

   // Print the program information
   console.center("nhc" + EDITOR_PROGRAM_NAME + "n cVersion g" +
                  EDITOR_VERSION + " wh(b" + EDITOR_VER_DATE + "w)");
   console.center("ncby Eric Oulashin");
   console.crlf();
   console.print("ncSlyEdit is a full-screen message editor for Synchronet that mimics the look &\r\n");
   console.print("feel of IceEdit or DCT Edit.");
   console.crlf();
   if (pPause)
      console.pause();
}

// Displays the informational screen for the program exit.
//
// Parameters:
//  pClearScreen: Whether or not to clear the screen.
function displayProgramExitInfo(pClearScreen)
{
	if (pClearScreen)
		console.clear("n");

	/*console.print("ncYou have been using:\r\n");
	console.print("hkÛ7ßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßß0Û\r\n");
	console.print("Û7 nb7Üßßßß Û       Ûßßßß    Û Ü       hk0Û\r\n");
	console.print("Û7 nb7ßÜÜÜ  Û Ü   Ü ÛÜÜÜ   ÜÜÛ Ü ÜÜÛÜÜ hk0Û\r\n");
	console.print("Û7     nb7Û Û Û   Û Û     Û  Û Û   Û   hk0Û\r\n");
	console.print("Û7 nb7ßßßß  ß  ßÜß  ßßßßß  ßßß ß   ßßß hk0Û\r\n");
	console.print("Û7         nb7Üß                       hk0Û\r\n");
	console.print("Û7        nb7ß                         hk0Û\r\n");
	console.print("ßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßß\r\n");
	console.print("ngVersion hy" + EDITOR_VERSION + " nm(" +
	              EDITOR_VER_DATE + ")");*/
	console.print("ncYou have been using hSlyEdit ncversion g" + EDITOR_VERSION +
	              " nm(" + EDITOR_VER_DATE + ")");
	console.crlf();
	console.print("ncby Eric Oulashin of chDncigital hDncistortion hBncBS");
	console.crlf();
	console.crlf();
	console.print("ncAcknowledgements for look & feel go to the following people:");
	console.crlf();
	console.print("Dan Traczynski: Creator of DCT Edit");
	console.crlf();
	console.print("Jeremy Landvoigt: Original creator of IceEdit");
	console.crlf();
}

// Writes some text on the screen at a given location with a given pause.
//
// Parameters:
//  pX: The column number on the screen at which to write the message
//  pY: The row number on the screen at which to write the message
//  pText: The text to write
//  pPauseMS: The pause time, in milliseconds
//  pClearLineAttrib: Optional - The color/attribute to clear the line with.
//                    If not specified, defaults to normal attribute.
function writeWithPause(pX, pY, pText, pPauseMS, pClearLineAttrib)
{
   var clearLineAttrib = "n";
   if ((pClearLineAttrib != null) && (typeof(pClearLineAttrib) == "string"))
      clearLineAttrib = pClearLineAttrib;
   console.gotoxy(pX, pY);
   console.cleartoeol(clearLineAttrib);
   console.print(pText);
   mswait(pPauseMS);
}

// Prompts the user for a yes/no question.
//
// Parameters:
//  pQuestion: The question to ask the user
//  pDefaultYes: Boolean - Whether or not the default should be Yes.
//               For false, the default will be No.
//  pBoxTitle: For DCT mode, this specifies the title to use for the
//             prompt box.  This is optional; if this is left out,
//             the prompt box title will default to "Prompt".
// pIceRefreshForBothAnswers: In Ice mode, whether or not to refresh the bottom
//                            line on the screen for a "yes" as well as "no"
//                            answer.  This is optional.  By default, only
//                            refreshes for a "no" answer.
//
// Return value: Boolean - true for a "Yes" answer, false for "No"
function promptYesNo(pQuestion, pDefaultYes, pBoxTitle, pIceRefreshForBothAnswers)
{
   var userResponse = pDefaultYes;

   if (EDITOR_STYLE == "DCT")
   {
      // We need to create an object of parameters to pass to the DCT-style
      // Yes/No function.
      var paramObj = new AbortConfirmFuncParams();
      paramObj.editLinesIndex = gEditLinesIndex;
      if (typeof(pBoxTitle) == "string")
         userResponse = promptYesNo_DCTStyle(pQuestion, pBoxTitle, pDefaultYes, paramObj);
      else
         userResponse = promptYesNo_DCTStyle(pQuestion, "Prompt", pDefaultYes, paramObj);
   }
   else if (EDITOR_STYLE == "ICE")
   {
      const originalCurpos = console.getxy();
      // Go to the bottom line on the screen and prompt the user
      console.gotoxy(1, console.screen_rows);
      console.cleartoeol();
      console.gotoxy(1, console.screen_rows);
      userResponse = promptYesNo_IceStyle(pQuestion, pDefaultYes);
      // If the user chose "No", or if we are to refresh the bottom line
      // regardless, then re-display the bottom help line and move the
      // cursor back to its original position.
      if (pIceRefreshForBothAnswers || !userResponse)
      {
         fpDisplayBottomHelpLine(console.screen_rows, gUseQuotes);
         console.gotoxy(originalCurpos);
      }
   }

   return userResponse;
}

// Reads the SlyEdit configuration settings from SlyEdit.cfg.
//
// Return value: An object containing the settings as properties.
function ReadSlyEditConfigFile()
{
	var cfgObj = new Object(); // Configuration object
	cfgObj.userIsSysop = user.compare_ars("SYSOP"); // Whether or not the user is a sysop
	// Default settings
	cfgObj.thirdPartyLoadOnStart = new Array();
	cfgObj.runJSOnStart = new Array();
	cfgObj.thirdPartyLoadOnExit = new Array();
	cfgObj.runJSOnExit = new Array();
	cfgObj.displayEndInfoScreen = true;
	cfgObj.userInputTimeout = true;
	cfgObj.inputTimeoutMS = 300000;
	cfgObj.reWrapQuoteLines = true;
	cfgObj.allowColorSelection = true;
	cfgObj.noColorSelectionGrpNames = [];
	cfgObj.noColorSelectionSubBoardCodes = [];
	cfgObj.cvtColorToANSIGrpNames = [];
	cfgObj.cvtColorToANSISubBoardCodes = [];
	cfgObj.useQuoteLineInitials = true;
	cfgObj.indentQuoteLinesWithInitials = true;
	cfgObj.allowCrossPosting = true;
	cfgObj.enableTextReplacements = false;
	cfgObj.textReplacementsUseRegex = false;
	cfgObj.enableTaglines = false;
	cfgObj.tagLineFilename = genFullPathCfgFilename("SlyEdit_Taglines.txt", gStartupPath);
	cfgObj.taglinePrefix = "... ";
	cfgObj.quoteTaglines = false;
	cfgObj.shuffleTaglines = false;
	cfgObj.allowUserSettings = true;
	cfgObj.allowEditQuoteLines = true;

	// General SlyEdit color settings
	cfgObj.genColors = new Object();
	// Cross-posting UI element colors
	cfgObj.genColors.listBoxBorder = "\1n\1g";
	cfgObj.genColors.listBoxBorderText = "\1n\1b\1h";
	cfgObj.genColors.crossPostMsgAreaNum = "\1n\1h\1w";
	cfgObj.genColors.crossPostMsgAreaNumHighlight = "\1n\1" + "4\1h\1w";
	cfgObj.genColors.crossPostMsgAreaDesc = "\1n\1c";
	cfgObj.genColors.crossPostMsgAreaDescHighlight = "\1n\1" + "4\1c";
	cfgObj.genColors.crossPostChk = "\1n\1h\1y";
	cfgObj.genColors.crossPostChkHighlight = "\1n\1" + "4\1h\1y";
	cfgObj.genColors.crossPostMsgGrpMark = "\1n\1h\1g";
	cfgObj.genColors.crossPostMsgGrpMarkHighlight = "\1n\1" + "4\1h\1g";
	// Colors for certain output strings
	cfgObj.genColors.msgWillBePostedHdr = "\1n\1c";
	cfgObj.genColors.msgPostedGrpHdr = "\1n\1h\1b";
	cfgObj.genColors.msgPostedSubBoardName = "\1n\1g";
	cfgObj.genColors.msgPostedOriginalAreaText = "\1n\1c";
	cfgObj.genColors.msgHasBeenSavedText = "\1n\1h\1c";
	cfgObj.genColors.msgAbortedText = "\1n\1m\1h";
	cfgObj.genColors.emptyMsgNotSentText = "\1n\1m\1h";
	cfgObj.genColors.genMsgErrorText = "\1n\1m\1h";
	cfgObj.genColors.listBoxItemText = "\1n\1c";
	cfgObj.genColors.listBoxItemHighlight = "\1n\1" + "4\1w\1h";

	// Default Ice-style colors
	cfgObj.iceColors = new Object();
	cfgObj.iceColors.menuOptClassicColors = true;
	// Ice color theme file
	cfgObj.iceColors.ThemeFilename = genFullPathCfgFilename("SlyIceColors_BlueIce.cfg", gStartupPath);
	// Quote line color
	cfgObj.iceColors.QuoteLineColor = "\1n\1c";
	// Ice colors for the quote window
	cfgObj.iceColors.QuoteWinText = "\1n\1h\1w";            // White
	cfgObj.iceColors.QuoteLineHighlightColor = "\1" + "4\1h\1c"; // High cyan on blue background
	cfgObj.iceColors.QuoteWinBorderTextColor = "\1n\1c\1h"; // Bright cyan
	cfgObj.iceColors.BorderColor1 = "\1n\1b";              // Blue
	cfgObj.iceColors.BorderColor2 = "\1n\1b\1h";          // Bright blue
	// Ice colors for multi-choice prompts
	cfgObj.iceColors.SelectedOptionBorderColor = "\1n\1b\1h\1" + "4";
	cfgObj.iceColors.SelectedOptionTextColor = "\1n\1c\1h\1" + "4"
	cfgObj.iceColors.UnselectedOptionBorderColor = "\1n\1b";
	cfgObj.iceColors.UnselectedOptionTextColor = "\1n\1w";
	// Ice colors for the top info area
	cfgObj.iceColors.TopInfoBkgColor = "\1" + "4";
	cfgObj.iceColors.TopLabelColor = "\1c\1h";
	cfgObj.iceColors.TopLabelColonColor = "\1b\1h";
	cfgObj.iceColors.TopToColor = "\1w\1h";
	cfgObj.iceColors.TopFromColor = "\1w\1h";
	cfgObj.iceColors.TopSubjectColor = "\1w\1h";
	cfgObj.iceColors.TopTimeColor = "\1g\1h";
	cfgObj.iceColors.TopTimeLeftColor = "\1g\1h";
	cfgObj.iceColors.EditMode = "\1c\1h";
	cfgObj.iceColors.KeyInfoLabelColor = "\1c\1h";

	// Default DCT-style colors
	cfgObj.DCTColors = new Object();
	// DCT color theme file
	cfgObj.DCTColors.ThemeFilename = genFullPathCfgFilename("SlyDCTColors_Default.cfg", gStartupPath);
	// Quote line color
	cfgObj.DCTColors.QuoteLineColor = "\1n\1c";
	// DCT colors for the border stuff
	cfgObj.DCTColors.TopBorderColor1 = "\1n\1r";
	cfgObj.DCTColors.TopBorderColor2 = "\1n\1r\1h";
	cfgObj.DCTColors.EditAreaBorderColor1 = "\1n\1g";
	cfgObj.DCTColors.EditAreaBorderColor2 = "\1n\1g\1h";
	cfgObj.DCTColors.EditModeBrackets = "\1n\1k\1h";
	cfgObj.DCTColors.EditMode = "\1n\1w";
	// DCT colors for the top informational area
	cfgObj.DCTColors.TopLabelColor = "\1n\1b\1h";
	cfgObj.DCTColors.TopLabelColonColor = "\1n\1b";
	cfgObj.DCTColors.TopFromColor = "\1n\1c\1h";
	cfgObj.DCTColors.TopFromFillColor = "\1n\1c";
	cfgObj.DCTColors.TopToColor = "\1n\1c\1h";
	cfgObj.DCTColors.TopToFillColor = "\1n\1c";
	cfgObj.DCTColors.TopSubjColor = "\1n\1w\1h";
	cfgObj.DCTColors.TopSubjFillColor = "\1n\1w";
	cfgObj.DCTColors.TopAreaColor = "\1n\1g\1h";
	cfgObj.DCTColors.TopAreaFillColor = "\1n\1g";
	cfgObj.DCTColors.TopTimeColor = "\1n\1y\1h";
	cfgObj.DCTColors.TopTimeFillColor = "\1n\1r";
	cfgObj.DCTColors.TopTimeLeftColor = "\1n\1y\1h";
	cfgObj.DCTColors.TopTimeLeftFillColor = "\1n\1r";
	cfgObj.DCTColors.TopInfoBracketColor = "\1n\1m";
	// DCT colors for the quote window
	cfgObj.DCTColors.QuoteWinText = "\1n\1" + "7\1k";
	cfgObj.DCTColors.QuoteLineHighlightColor = "\1n\1w";
	cfgObj.DCTColors.QuoteWinBorderTextColor = "\1n\1" + "7\1r";
	cfgObj.DCTColors.QuoteWinBorderColor = "\1n\1k\1" + "7";
	// DCT colors for the quote window
	cfgObj.DCTColors.QuoteWinText = "\1n\1" + "7\1b";
	cfgObj.DCTColors.QuoteLineHighlightColor = "\1n\1w";
	cfgObj.DCTColors.QuoteWinBorderTextColor = "\1n\1" + "7\1r";
	cfgObj.DCTColors.QuoteWinBorderColor = "\1n\1k\1" + "7";
	// DCT colors for the bottom row help text
	cfgObj.DCTColors.BottomHelpBrackets = "\1n\1k\1h";
	cfgObj.DCTColors.BottomHelpKeys = "\1n\1r\1h";
	cfgObj.DCTColors.BottomHelpFill = "\1n\1r";
	cfgObj.DCTColors.BottomHelpKeyDesc = "\1n\1c";
	// DCT colors for text boxes
	cfgObj.DCTColors.TextBoxBorder = "\1n\1k\1" + "7";
	cfgObj.DCTColors.TextBoxBorderText = "\1n\1r\1" + "7";
	cfgObj.DCTColors.TextBoxInnerText = "\1n\1b\1" + "7";
	cfgObj.DCTColors.YesNoBoxBrackets = "\1n\1k\1" + "7";
	cfgObj.DCTColors.YesNoBoxYesNoText = "\1n\1w\1h\1" + "7";
	// DCT colors for the menus
	cfgObj.DCTColors.SelectedMenuLabelBorders = "\1n\1w";
	cfgObj.DCTColors.SelectedMenuLabelText = "\1n\1k\1" + "7";
	cfgObj.DCTColors.UnselectedMenuLabelText = "\1n\1w\1h";
	cfgObj.DCTColors.MenuBorders = "\1n\1k\1" + "7";
	cfgObj.DCTColors.MenuSelectedItems = "\1n\1w";
	cfgObj.DCTColors.MenuUnselectedItems = "\1n\1k\1" + "7";
	cfgObj.DCTColors.MenuHotkeys = "\1n\1w\1h\1" + "7";

	// Open the SlyEdit configuration file
	var slyEdCfgFileName = genFullPathCfgFilename("SlyEdit.cfg", gStartupPath);
	var cfgFile = new File(slyEdCfgFileName);
	if (cfgFile.open("r"))
	{
		var settingsMode = "behavior";
		var fileLine = null;     // A line read from the file
		var equalsPos = 0;       // Position of a = in the line
		var commentPos = 0;      // Position of the start of a comment
		var setting = null;      // A setting name (string)
		var settingUpper = null; // Upper-case setting name
		var value = null;        // A value for a setting (string), with spaces trimmed
		var valueLiteral = null; // The value as it is in the config file, no processing
		var valueUpper = null;   // Upper-cased value
		while (!cfgFile.eof)
		{
			// Read the next line from the config file.
			fileLine = cfgFile.readln(2048);

			// fileLine should be a string, but I've seen some cases
			// where for some reason it isn't.  If it's not a string,
			// then continue onto the next line.
			if (typeof(fileLine) != "string")
				continue;

			// If the line starts with with a semicolon (the comment
			// character) or is blank, then skip it.
			if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
				continue;

			// If in the "behavior" section, then set the behavior-related variables.
			if (fileLine.toUpperCase() == "[BEHAVIOR]")
			{
				settingsMode = "behavior";
				continue;
			}
			else if (fileLine.toUpperCase() == "[ICE_COLORS]")
			{
				settingsMode = "ICEColors";
				continue;
			}
			else if (fileLine.toUpperCase() == "[DCT_COLORS]")
			{
				settingsMode = "DCTColors";
				continue;
			}

			// If the line has a semicolon anywhere in it, then remove
			// everything from the semicolon onward.
			commentPos = fileLine.indexOf(";");
			if (commentPos > -1)
				fileLine = fileLine.substr(0, commentPos);

			// Look for an equals sign, and if found, separate the line
			// into the setting name (before the =) and the value (after the
			// equals sign).
			equalsPos = fileLine.indexOf("=");
			if (equalsPos > 0)
			{
				// Read the setting & value, and trim leading & trailing spaces.
				setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
				settingUpper = setting.toUpperCase();
				valueLiteral = fileLine.substr(equalsPos+1);
				value = trimSpaces(valueLiteral, true, false, true);
				valueUpper = value.toUpperCase();

				if (settingsMode == "behavior")
				{
					if (settingUpper == "DISPLAYENDINFOSCREEN")
						cfgObj.displayEndInfoScreen = (valueUpper == "TRUE");
					else if (settingUpper == "USERINPUTTIMEOUT")
						cfgObj.userInputTimeout = (valueUpper == "TRUE");
					else if (settingUpper == "INPUTTIMEOUTMS")
						cfgObj.inputTimeoutMS = +value;
					else if (settingUpper == "REWRAPQUOTELINES")
						cfgObj.reWrapQuoteLines = (valueUpper == "TRUE");
					else if (settingUpper == "ALLOWCOLORSELECTION")
						cfgObj.allowColorSelection = (valueUpper == "TRUE");
					else if (settingUpper == "NOCOLORSELECTIONGRPNAMES")
					{
						// Message group names for message groups where text
						// color selection isn't allowed.  Split on commas.
						// I was originally going to have this be a list of
						// numbers for the group numbers/indexes and check
						// against msg_area.grp_list, but that group list could
						// be different for different users, depending on access
						// requirements.
						cfgObj.noColorSelectionGrpNames = valueUpper.split(",");
					}
					else if (settingUpper == "NOCOLORSELECTIONSUBBOARDCODES")
					{
						// Sub-board codes for sub-boards where text color selection
						// isn't allowed.  Split on commas, and convert all to
						// lowercase, since sub-board codes need to be lowercase.
						var values = value.toLowerCase().split(",");
						for (var i = 0; i < values.length; ++i)
						{
							if (msg_area.sub.hasOwnProperty(values[i]))
								cfgObj.noColorSelectionSubBoardCodes.push(values[i]);
						}
					}
					else if (settingUpper == "CVTCOLORTOANSIGRPNAMES")
					{
						if ((value == "*") || (valueUpper == "ALL"))
						{
							for (var i = 0; i < msg_area.grp_list.length; ++i)
								cfgObj.cvtColorToANSIGrpNames.push(msg_area.grp_list[i].name.toUpperCase());
							cfgObj.cvtColorToANSIGrpNames.push("ELECTRONIC MAIL");
						}
						else
							cfgObj.cvtColorToANSIGrpNames = valueUpper.split(",");
					}
					else if (settingUpper == "CVTCOLORTOANSISUBBOARDCODES")
					{
						var values = value.toLowerCase().split(",");
						for (var i = 0; i < values.length; ++i)
						{
							if (msg_area.sub.hasOwnProperty(values[i]))
								cfgObj.cvtColorToANSISubBoardCodes.push(values[i]);
						}
					}
					else if (settingUpper == "USEQUOTELINEINITIALS")
						cfgObj.useQuoteLineInitials = (valueUpper == "TRUE");
					else if (settingUpper == "INDENTQUOTELINESWITHINITIALS")
						cfgObj.indentQuoteLinesWithInitials = (valueUpper == "TRUE");
					else if (settingUpper == "ADD3RDPARTYSTARTUPSCRIPT")
						cfgObj.thirdPartyLoadOnStart.push(value);
					else if (settingUpper == "ADD3RDPARTYEXITSCRIPT")
						cfgObj.thirdPartyLoadOnExit.push(value);
					else if (settingUpper == "ADDJSONSTART")
						cfgObj.runJSOnStart.push(value);
					else if (settingUpper == "ADDJSONEXIT")
						cfgObj.runJSOnExit.push(value);
					else if (settingUpper == "ALLOWCROSSPOSTING")
						cfgObj.allowCrossPosting = (valueUpper == "TRUE");
					else if (settingUpper == "ENABLETEXTREPLACEMENTS")
					{
						// The enableTxtReplacements setting in the config file can
						// be regex, true, or false:
						//  - regex: Text replacement enabled using regular expressions
						//  - true: Text replacement enabled using exact match
						//  - false: Text replacement disabled
						cfgObj.textReplacementsUseRegex = (valueUpper == "REGEX");
						if (cfgObj.textReplacementsUseRegex)
							cfgObj.enableTextReplacements = true;
						else
							cfgObj.enableTextReplacements = (valueUpper == "TRUE");
					}
					else if (settingUpper == "ENABLETAGLINES")
						cfgObj.enableTaglines = (valueUpper == "TRUE");
					else if (settingUpper == "TAGLINEFILENAME")
						cfgObj.tagLineFilename = genFullPathCfgFilename(value, gStartupPath);
					else if (settingUpper == "TAGLINEPREFIX")
						cfgObj.taglinePrefix = valueLiteral;
					else if (settingUpper == "QUOTETAGLINES")
						cfgObj.quoteTaglines = (valueUpper == "TRUE");
					else if (settingUpper == "SHUFFLETAGLINES")
						cfgObj.shuffleTaglines = (valueUpper == "TRUE");
					else if (settingUpper == "ALLOWUSERSETTINGS")
						cfgObj.allowUserSettings = (valueUpper == "TRUE");
					else if (settingUpper == "ALLOWEDITQUOTELINES")
						cfgObj.allowEditQuoteLines = (valueUpper == "TRUE");
				}
				else if (settingsMode == "ICEColors")
				{
					if (settingUpper == "THEMEFILENAME")
						cfgObj.iceColors.ThemeFilename = genFullPathCfgFilename(value, gStartupPath);
					else if (settingUpper == "MENUOPTCLASSICCOLORS")
						cfgObj.iceColors.menuOptClassicColors = (valueUpper == "TRUE");
				}
				else if (settingsMode == "DCTColors")
				{
					if (settingUpper == "THEMEFILENAME")
						cfgObj.DCTColors.ThemeFilename = genFullPathCfgFilename(value, gStartupPath);
				}
			}
		}

		cfgFile.close();

		// Validate the settings
		if (cfgObj.inputTimeoutMS < 1000)
			cfgObj.inputTimeoutMS = 300000;
	}

	return cfgObj;
}

// This function reads a configuration file containing
// setting=value pairs and returns the settings in
// an Object.
//
// Parameters:
//  pFilename: The name of the configuration file.
//  pLineReadLen: The maximum number of characters to read from each
//                line.  This is optional; if not specified, then up
//                to 2048 characters will be read from each line.
//  pCvtSyncColorAttrChar: Optional boolean - Whether or not to convert
//                         \1 in color values to ASCII char 1 in color values
//
// Return value: An Object containing the value=setting pairs.  If the
//               file can't be opened or no settings can be read, then
//               this function will return null.
function readValueSettingConfigFile(pFilename, pLineReadLen, pCvtSyncColorAttrChar)
{
	var retObj = null;

	var cfgFile = new File(pFilename);
	if (cfgFile.open("r"))
	{
		var cvtSyncColorAttrChar = (typeof(pCvtSyncColorAttrChar) == "boolean" ? pCvtSyncColorAttrChar : false);
		var numCharsPerLine = (typeof(pLineReadLen) == "number" ? pLineReadLen : 2048);

		var fileLine = null;     // A line read from the file
		var equalsPos = 0;       // Position of a = in the line
		var commentPos = 0;      // Position of the start of a comment
		var setting = null;      // A setting name (string)
		var settingUpper = null; // Upper-case setting name
		var value = null;        // A value for a setting (string)
		var valueUpper = null;   // Upper-cased value
		while (!cfgFile.eof)
		{
			// Read the next line from the config file.
			fileLine = cfgFile.readln(numCharsPerLine);

			// fileLine should be a string, but I've seen some cases
			// where it isn't, so check its type.
			if (typeof(fileLine) != "string")
				continue;

			// If the line starts with with a semicolon (the comment
			// character) or is blank, then skip it.
			if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
				continue;

			// If the line has a semicolon anywhere in it, then remove
			// everything from the semicolon onward.
			commentPos = fileLine.indexOf(";");
			if (commentPos > -1)
				fileLine = fileLine.substr(0, commentPos);

			// Look for an equals sign, and if found, separate the line
			// into the setting name (before the =) and the value (after the
			// equals sign).
			equalsPos = fileLine.indexOf("=");
			if (equalsPos > 0)
			{
				// If retObj hasn't been created yet, then create it.
				if (retObj == null)
					retObj = new Object();

				// Read the setting & value, and trim leading & trailing spaces.  Then
				// set the value in retObj.
				setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
				value = trimSpaces(fileLine.substr(equalsPos+1), true, false, true);
				if (cvtSyncColorAttrChar) // Replace \1 with the SOH character, for Sync color codes
					value = value.replace(/\\1/g, "\1");
				retObj[setting] = value;
			}
		}

		cfgFile.close();
	}

	return retObj;
}

// Splits a string up by a maximum length, preserving whole words.
//
// Parameters:
//  pStr: The string to split
//  pMaxLen: The maximum length for the strings (strings longer than this
//           will be split)
//
// Return value: An array of strings resulting from the string split
function splitStrStable(pStr, pMaxLen)
{
   var strings = new Array();

   // Error checking
   if (typeof(pStr) != "string")
   {
      console.print("1 - pStr not a string!\r\n");
      return strings;
   }

   // If the string's length is less than or equal to pMaxLen, then
   // just insert it into the strings array.  Otherwise, we'll
   // need to split it.
   if (pStr.length <= pMaxLen)
      strings.push(pStr);
   else
   {
      // Make a copy of pStr so that we don't modify it.
      var theStr = pStr;

      var tempStr = "";
      var splitIndex = 0; // Index of a space in a string
      while (theStr.length > pMaxLen)
      {
         // If there isn't a space at the pMaxLen location in theStr,
         // then assume there's a word there and look for a space
         // before it.
         splitIndex = pMaxLen;
         if (theStr.charAt(splitIndex) != " ")
         {
            splitIndex = theStr.lastIndexOf(" ", splitIndex);
            // If a space was not found, then we should split at
            // pMaxLen.
            if (splitIndex == -1)
               splitIndex = pMaxLen;
         }

         // Extract the first part of theStr up to splitIndex into
         // tempStr, and then remove that part from theStr.
         tempStr = theStr.substr(0, splitIndex);
         theStr = theStr.substr(splitIndex+1);

         // If tempStr is not blank, then insert it into the strings
         // array.
         if (tempStr.length > 0)
            strings.push(tempStr);
      }
      // Edge case: If theStr is not blank, then insert it into the
      // strings array.
      if (theStr.length > 0)
         strings.push(theStr);
   }

   return strings;
}

// Inserts a string inside another string.
//
// Parameters:
//  pStr: The string inside which to insert the other string
//  pIndex: The index of pStr at which to insert the other string
//  pStr2: The string to insert into the first string
//
// Return value: The spliced string
function spliceIntoStr(pStr, pIndex, pStr2)
{
	// Error checking
	var typeofPStr = typeof(pStr);
	var typeofPStr2 = typeof(pStr2);
	if ((typeofPStr != "string") && (typeofPStr2 != "string"))
		return "";
	else if ((typeofPStr == "string") && (typeofPStr2 != "string"))
		return pStr;
	else if ((typeofPStr != "string") && (typeofPStr2 == "string"))
		return pStr2;
	// If pIndex is beyond the last index of pStr, then just return the
	// two strings concatenated.
	if (pIndex >= pStr.length)
		return (pStr + pStr2);
	// If pIndex is below 0, then just return pStr2 + pStr.
	else if (pIndex < 0)
		return (pStr2 + pStr);

	//return (pStr.substr(0, pIndex) + pStr2 + pStr.substr(pIndex));
	return (pStr.substr(0, pIndex) + pStr2 + pStr.substr(pIndex));
}

// Fixes the text lines in the gEditLines array so that they all
// have a maximum width to fit within the edit area.
//
// Parameters:
//  pTextLineArray: An array of TextLine objects to adjust
//  pStartIndex: The index of the line in the array to start at.
//  pEndIndex: One past the last index of the line in the array to end at.
//  pEditWidth: The width of the edit area (AKA the maximum line length + 1)
//
// Return value: Boolean - Whether or not any text was changed.
function reAdjustTextLines(pTextLineArray, pStartIndex, pEndIndex, pEditWidth)
{
	// Returns without doing anything if any of the parameters are not
	// what they should be. (Note: Not checking pTextLineArray for now..)
	if ((typeof(pStartIndex) != "number") || (typeof(pEndIndex) != "number") || (typeof(pEditWidth) != "number"))
		return false;
	// Range checking
	if ((pStartIndex < 0) || (pStartIndex >= pTextLineArray.length))
		return false;
	if ((pEndIndex <= pStartIndex) || (pEndIndex < 0))
		return false;
	if (pEndIndex > pTextLineArray.length)
		pEndIndex = pTextLineArray.length;
	if (pEditWidth <= 5)
		return false;

	var textChanged = false; // We'll return this at the end of the function

	var nextLineIndex = 0;
	var numCharsToRemove = 0;
	var splitIndex = 0;
	var spaceFound = false;      // Whether or not a space was found in a text line
	var splitIndexOriginal = 0;
	var tempText = null;
	var appendedNewLine = false; // If we appended another line
	for (var i = pStartIndex; i < pEndIndex; ++i)
	{
		// As an extra precaution, check to make sure this array element is defined.
		if (pTextLineArray[i] == undefined)
			continue;

		nextLineIndex = i + 1;
		// If the line's text is longer or equal to the edit width, then if
		// possible, move the last word to the beginning of the next line.
		if (pTextLineArray[i].displayLength() >= pEditWidth)
		{
			numCharsToRemove = pTextLineArray[i].displayLength() - pEditWidth + 1;
			splitIndex = pTextLineArray[i].displayIdxToActualIdx(pTextLineArray[i].displayLength() - numCharsToRemove);
			splitIndexOriginal = splitIndex;
			// If the character in the text line at splitIndex is not a space,
			// then look for a space before splitIndex.
			spaceFound = (pTextLineArray[i].text.charAt(splitIndex) == " ");
			if (!spaceFound)
			{
				splitIndex = pTextLineArray[i].text.lastIndexOf(" ", splitIndex-1);
				spaceFound = (splitIndex > -1);
				if (!spaceFound)
					splitIndex = splitIndexOriginal;
				// If the character at splitIndex is the 2nd character in a Synchronet
				// attribute code, then adjust splitIndex accordingly.
				if ((splitIndex > 0) && gSyncAttrRegex.test(pTextLineArray[i].text.substr(splitIndex-1, 2)))
					--splitIndex;
			}

			var tempText = pTextLineArray[i].text.substr(splitIndex);
			// If the substring contains a space at the beginning or the end, then
			// remove the space.
			if (tempText.search(/^ /) == 0)
				tempText = tempText.substr(1);
			else if (tempText.search(/ $/) == tempText.length-1)
				tempText = tempText.substr(0, tempText.length-1);
			pTextLineArray[i].text = pTextLineArray[i].text.substr(0, splitIndex);

			textChanged = true;
			// If we're on the last line, or if the current line has a hard
			// newline or is a quote line, then append a new line below.
			appendedNewLine = false;
			if ((nextLineIndex == pTextLineArray.length) || pTextLineArray[i].hardNewlineEnd || isQuoteLine(pTextLineArray, i))
			{
				pTextLineArray.splice(nextLineIndex, 0, new TextLine());
				pTextLineArray[nextLineIndex].hardNewlineEnd = pTextLineArray[i].hardNewlineEnd;
				pTextLineArray[i].hardNewlineEnd = false;
				pTextLineArray[nextLineIndex].isQuoteLine = pTextLineArray[i].isQuoteLine;
				appendedNewLine = true;
			}

			// Move the text around and adjust the line properties.
			if (appendedNewLine)
				pTextLineArray[nextLineIndex].text = tempText;
			else
			{
				// If we're in insert mode, then insert the text at the beginning of
				// the next line.  Otherwise, overwrite the text in the next line.
				if (inInsertMode())
					pTextLineArray[nextLineIndex].text = tempText + " " + pTextLineArray[nextLineIndex].text;
				else
				{
					// We're in overwrite mode, so overwrite the first part of the next
					// line with tempText.
					if (pTextLineArray[nextLineIndex].text.length < tempText.length)
						pTextLineArray[nextLineIndex].text = tempText;
					else
						pTextLineArray[nextLineIndex].text = tempText + pTextLineArray[nextLineIndex].text.substr(tempText.length);
				}
			}
		}
		else
		{
			// pTextLineArray[i].text.length is < pEditWidth, so try to bring up text
			// from the next line.

			// Only do it if the line doesn't have a hard newline and it's not a
			// quote line and there is a next line.
			if (!pTextLineArray[i].hardNewlineEnd && !isQuoteLine(pTextLineArray, i) && (i < pTextLineArray.length-1))
			{
				if (pTextLineArray[nextLineIndex].text.length > 0)
				{
					splitIndex = pEditWidth - pTextLineArray[i].text.length - 2;
					// If splitIndex is negative, that means the entire next line
					// can fit on the current line.
					if ((splitIndex < 0) || (splitIndex > pTextLineArray[nextLineIndex].text.length))
						splitIndex = pTextLineArray[nextLineIndex].text.length;
					else
					{
						// If the character in the next line at splitIndex is not a
						// space, then look for a space before it.
						if (pTextLineArray[nextLineIndex].text.charAt(splitIndex) != " ")
							splitIndex = pTextLineArray[nextLineIndex].text.lastIndexOf(" ", splitIndex);
						// If no space was found, then skip to the next line (we don't
						// want to break up words from the next line).
						if (splitIndex == -1)
							continue;
					}
					// If the character at splitIndex is the 2nd character in a Synchronet
					// attribute code, then adjust splitIndex accordingly.
					if ((splitIndex > 0) && gSyncAttrRegex.test(pTextLineArray[nextLineIndex].text.substr(splitIndex-1, 2)))
						--splitIndex;

					// Get the text to bring up to the current line.
					// If the current line does not end with a space and the next line
					// does not start with a space, then add a space between this line
					// and the next line's text.  This is done to avoid joining words
					// accidentally.
					var tempText = "";
					if ((pTextLineArray[i].text.charAt(pTextLineArray[i].text.length-1) != " ") &&
					    (pTextLineArray[nextLineIndex].text.substr(0, 1) != " "))
					{
						tempText = " ";
					}
					tempText += pTextLineArray[nextLineIndex].text.substr(0, splitIndex);
					// Move the text from the next line to the current line, if the current
					// line has room for it.
					if (pTextLineArray[i].text.length + tempText.length < pEditWidth)
					{
						pTextLineArray[i].text += tempText;
						pTextLineArray[nextLineIndex].text = pTextLineArray[nextLineIndex].text.substr(splitIndex+1);
						textChanged = true;

						// If the next line is now blank, then remove it.
						if (pTextLineArray[nextLineIndex].text.length == 0)
						{
							// The current line should take on the next line's
							// hardnewlineEnd property before removing the next line.
							pTextLineArray[i].hardNewlineEnd = pTextLineArray[nextLineIndex].hardNewlineEnd;
							pTextLineArray.splice(nextLineIndex, 1);
						}
					}
				}
				else
				{
					// The next line's text string is blank.  If its hardNewlineEnd
					// property is false, then remove the line.
					if (!pTextLineArray[nextLineIndex].hardNewlineEnd)
					{
						pTextLineArray.splice(nextLineIndex, 1);
						textChanged = true;
					}
				}
			}
		}
	}

	return textChanged;
}

// Returns indexes of the first unquoted text line and the next
// quoted text line in an array of text lines.
//
// Parameters:
//  pTextLineArray: An array of TextLine objects
//  pStartIndex: The index of where to start looking in the array
//  pQuotePrefix: The quote line prefix (string)
//
// Return value: An object containing the following properties:
//               noQuoteLineIndex: The index of the next non-quoted line.
//                                 Will be -1 if none are found.
//               nextQuoteLineIndex: The index of the next quoted line.
//                                   Will be -1 if none are found.
function quotedLineIndexes(pTextLineArray, pStartIndex, pQuotePrefix)
{
   var retObj = new Object();
   retObj.noQuoteLineIndex = -1;
   retObj.nextQuoteLineIndex = -1;

   if (pTextLineArray.length == 0)
      return retObj;
   if (typeof(pStartIndex) != "number")
      return retObj;
   if (pStartIndex >= pTextLineArray.length)
      return retObj;

   var startIndex = (pStartIndex > -1 ? pStartIndex : 0);

   // Look for the first non-quoted line in the array.
   retObj.noQuoteLineIndex = startIndex;
   for (; retObj.noQuoteLineIndex < pTextLineArray.length; ++retObj.noQuoteLineIndex)
   {
      if (pTextLineArray[retObj.noQuoteLineIndex].text.indexOf(pQuotePrefix) == -1)
         break;
   }
   // If the index is pTextLineArray.length, then what we're looking for wasn't
   // found, so set the index to -1.
   if (retObj.noQuoteLineIndex == pTextLineArray.length)
      retObj.noQuoteLineIndex = -1;

   // Look for the next quoted line in the array.
   // If we found a non-quoted line, then use that index; otherwise,
   // start at the first line.
   if (retObj.noQuoteLineIndex > -1)
      retObj.nextQuoteLineIndex = retObj.noQuoteLineIndex;
   else
      retObj.nextQuoteLineIndex = 0;
   for (; retObj.nextQuoteLineIndex < pTextLineArray.length; ++retObj.nextQuoteLineIndex)
   {
      if (pTextLineArray[retObj.nextQuoteLineIndex].text.indexOf(pQuotePrefix) == 0)
         break;
   }
   // If the index is pTextLineArray.length, then what we're looking for wasn't
   // found, so set the index to -1.
   if (retObj.nextQuoteLineIndex == pTextLineArray.length)
      retObj.nextQuoteLineIndex = -1;

   return retObj;
}

// Returns whether a line in an array of TextLine objects is a quote line.
// This is true if the line's isQuoteLine property is true or the line's text
// starts with > (preceded by any # of spaces).
//
// Parameters:
//  pLineArray: An array of TextLine objects
//  pLineIndex: The index of the line in gEditLines
function isQuoteLine(pLineArray, pLineIndex)
{
   if (typeof(pLineArray) == "undefined")
      return false;
   if (typeof(pLineIndex) != "number")
      return false;

   var lineIsQuoteLine = false;
   if (typeof(pLineArray[pLineIndex]) != "undefined")
      lineIsQuoteLine = (pLineArray[pLineIndex].isQuoteLine);
   return lineIsQuoteLine;
}

// Replaces an attribute in a text attribute string.
//
// Parameters:
//  pAttrType: Numeric:
//             FORE_ATTR: Foreground attribute
//             BKG_ATTR: Background attribute
//             3: Special attribute
//  pAttrs: The attribute string to change
//  pNewAttr: The new attribute to put into the attribute string (without the
//            control character)
function toggleAttr(pAttrType, pAttrs, pNewAttr)
{
	// Removes an attribute from an attribute string, if it
	// exists.  Returns the new attribute string.
	function removeAttrIfExists(pAttrs, pNewAttr)
	{
		var index = pAttrs.search(pNewAttr);
		if (index > -1)
			pAttrs = pAttrs.replace(pNewAttr, "");
		return pAttrs;
	}

	// Convert pAttrs and pNewAttr to all uppercase for ease of searching
	pAttrs = pAttrs.toUpperCase();
	pNewAttr = pNewAttr.toUpperCase();

	// If pAttrs starts with the normal attribute, then
	// remove it (we'll put it back on later).
	var normalAtStart = false;
	if (pAttrs.search(/^N/) == 0)
	{
		normalAtStart = true;
		pAttrs = pAttrs.substr(2);
	}

	// Prepend the attribute control character to the new attribute
	var newAttr = "" + pNewAttr;

	// Set a regex for searching & replacing
	var regex = "";
	switch (pAttrType)
	{
		case FORE_ATTR: // Foreground attribute
			regex = /K|R|G|Y|B|M|C|W/g;
			break;
		case BKG_ATTR: // Background attribute
			regex = /0|1|2|3|4|5|6|7/g;
			break;
		case SPECIAL_ATTR: // Special attribute
			//regex = /H|I|N/g;
			index = pAttrs.search(newAttr);
			if (index > -1)
			pAttrs = pAttrs.replace(newAttr, "");
			else
			pAttrs += newAttr;
			break;
		default:
			break;
	}

	// If regex is not blank, then search & replace on it in
	// pAttrs.
	if (regex != "")
	{
		pAttrs = removeAttrIfExists(pAttrs, newAttr);
		// If the regex is found, then replace it.  Otherwise,
		// add pNewAttr to the attribute string.
		if (pAttrs.search(regex) > -1)
			pAttrs = pAttrs.replace(regex, "" + pNewAttr);
		else
			pAttrs += "" + pNewAttr;
	}

	// If pAttrs started with the normal attribute, then
	// put it back on.
	if (normalAtStart)
		pAttrs = "N" + pAttrs;

	return pAttrs;
}

// This function wraps an array of strings based on a line width.
//
// Parameters:
//  pLineArr: An array of strings
//  pStartLineIndex: The index of the text line in the array to start at
//  pEndIndex: The index of where to stop in the array.  This is one past
//             the last line in the array.  For example, to end at the
//             last line in the array, use the array's .length property
//             for this parameter.
//  pLineWidth: The maximum width of each line
//  pIdxesRequiringNL (OUT): Optional - An array to contain the indexes of original
//                           wrapped lines that required a new line to be added.
//  pLineInfos (IN/OUT): Optional - An array of lineInfo objects previously generated
//                       for the unwrapped lines - This will be updated if lines are
//                       wrapped.
//
// Return value: The number of new lines added
function wrapTextLines(pLineArr, pStartLineIndex, pEndIndex, pLineWidth, pIdxesRequiringNL, pLineInfos)
{
	// Validate parameters
	if (pLineArr == null)
		return 0;
	if (typeof(pLineWidth) != "number")
		return 0;
	if (pLineWidth < 0)
		return 0;
	if ((pStartLineIndex == null) || (typeof(pStartLineIndex) != "number") || (pStartLineIndex < 0))
		pStartLineIndex = 0;
	if (pStartLineIndex >= pLineArr.length)
		return pLineArr.length;
	if ((typeof(pEndIndex) != "number") || (pEndIndex == null) || (pEndIndex > pLineArr.length))
		pEndIndex = pLineArr.length;

	// Determine whether pIdxesRequiringNL is an array (actually, the most we can
	// do is check whether it's an object).
	var pNewLineIndexesIsArray = (typeof(pIdxesRequiringNL) == "object");
	if (pNewLineIndexesIsArray)
		pIdxesRequiringNL.length = 0;

	// Wrap the text lines
	var origNumLines = pLineArr.length; // So we can return the # of lines added
	var trimLen = 0;   // The number of characters to trim from the end of a string
	var trimIndex = 0; // The index of where to start trimming
	for (var i = pStartLineIndex; i < pEndIndex; ++i)
	{
		// If the object in pLineArr is not a string for some reason, then skip it.
		if (typeof(pLineArr[i]) != "string")
			continue;

		if (pLineArr[i].length > pLineWidth)
		{
			trimLen = pLineArr[i].length - pLineWidth;
			trimIndex = pLineArr[i].lastIndexOf(" ", pLineArr[i].length - trimLen);
			if (trimIndex == -1)
				trimIndex = pLineArr[i].length - trimLen;
			// Trim the text, and remove leading spaces from it too.
			trimmedText = pLineArr[i].substr(trimIndex).replace(/^ +/, "");
			pLineArr[i] = pLineArr[i].substr(0, trimIndex);
			if (i < pLineArr.length - 1)
			{
				// Append a space to the end of the trimmed text if it doesn't have one.
				if ((trimmedText.length > 0) && (trimmedText.charAt(trimmedText.length-1) != " "))
					trimmedText += " "
				// Prepend the trimmed text to the next line.  If the next line's index
				// is within the paragraph we're wrapping, then go ahead and prepend the
				// text to the next line.  Otherwise, add a new line to the array and
				// add the text to the new line.
				if (i+1 < pEndIndex)
				{
					var nextLineWasBlank = (pLineArr[i+1].length == 0);
					pLineArr[i+1] = trimmedText + pLineArr[i+1];
					if (nextLineWasBlank)
						pLineArr.splice(i+2, 0, "");
					// Copy the current line's lineInfo object to the next
					// one in the array
					if (typeof(pLineInfos) == "object")
					{
						if (pLineInfos.length > i+1)
						{
							pLineInfos[i+1].startIndex = pLineInfos[i].startIndex;
							pLineInfos[i+1].quoteLevel = pLineInfos[i].quoteLevel;
							pLineInfos[i+1].begOfLine = pLineInfos[i].begOfLine;
						}
						else
						{
							// pLineInfos doesn't have enough objects..  This probably
							// shouldn't happen, as the caller should fill it up to
							// the correct number of objects.
							var numToAdd = (i+1) - pLineInfos.length + 1;
							for (var idx = 0; idx < numToAdd; ++idx)
								pLineInfos.push(getDefaultQuoteStrObj());
						}
						// If the next line was blank before adding text to it,
						// then splice a new lineInfo object into pLineInfos as
						// a copy of the lineInfo object before it.
						if (nextLineWasBlank)
						{
							pLineInfos.splice(i+2, 0, getDefaultQuoteStrObj());
							pLineInfos[i+2].startIndex = pLineInfos[i+1].startIndex;
							pLineInfos[i+2].quoteLevel = pLineInfos[i+1].quoteLevel;
							pLineInfos[i+2].begOfLine = pLineInfos[i+1].begOfLine;
						}
					}
				}
				else
				{
					// Add the trimmed text on a new line in the array.  Then, if the
					// trimmed text's length is longer then the allowed line width, then
					// we'll want to extend the end index so we can continue wrapping the
					// lines in the current paragraph.  Otherwise, add the current line's
					// index to the array of lines requiring a newline.
					pLineArr.splice(i+1, 0, trimmedText);
					if (trimmedText.length > pLineWidth)
						++pEndIndex;
					else
					{
						if (pNewLineIndexesIsArray)
							pIdxesRequiringNL.push(i);
					}
					// Append a lineInfo object to pLineInfos as a copy of the
					// last one in the array.
					if (typeof(pLineInfos) == "object")
					{
						// Save the last lineInfo object's values
						var lastLineInfoObj = {
							startIndex: pLineInfos[pLineInfos.length-1].startIndex,
							quoteLevel: pLineInfos[pLineInfos.length-1].quoteLevel,
							begOfLine: pLineInfos[pLineInfos.length-1].begOfLine
						};
						// Append a new lineInfo object to pLineInfos and copy
						// the last one's values into it
						pLineInfos.push(getDefaultQuoteStrObj());
						pLineInfos[pLineInfos.length-1].startIndex = lastLineInfoObj.startIndex;
						pLineInfos[pLineInfos.length-1].quoteLevel = lastLineInfoObj.quoteLevel;
						pLineInfos[pLineInfos.length-1].begOfLine = lastLineInfoObj.begOfLine;
					}
				}
			}
			else
			{
				// Remove any leading spaces 
				pLineArr.push(trimmedText);
				// If the current line index is before the specified end index, then
				// increment the end index since we've added a line in order to continue
				// wrapping the lines.
				if (i < pEndIndex-1)
					++pEndIndex;

				if (pNewLineIndexesIsArray)
					pIdxesRequiringNL.push(i);
			}
		}
	}

	return(pLineArr.length - origNumLines);
}

// Returns an object containing default quote string information.
//
// Return value: An object containing the following properties:
//               startIndex: The index of the first non-quote character in the string.
//                           Defaults to -1.
//               quoteLevel: The number of > characters at the start of the string
//               begOfLine: Normally, the quote text at the beginng of the line.
//                          This defaults to a blank string.
function getDefaultQuoteStrObj()
{
	var retObj = {
		startIndex: -1,
		quoteLevel: 0,
		begOfLine: "", // Will store the beginning of the line, before the >
		copy: function(pThatQuoteStrObj) {
			this.startIndex = pThatQuoteStrObj.startIndex;
			this.quoteLevel = pThatQuoteStrObj.quoteLevel;
			this.begOfLine = pThatQuoteStrObj.begOfLine;
		}
	};
	return retObj;
}

// Searches a string for the index of the first non-quote character; also finds
// the quote level (number of times quoted) and the beginning-of-line text (the
// text before the quote characters).
//
// Parameters:
//  pStr: A string to check
//  pUseAuthorInitials: Whether or not SlyEdit is configured to prefix
//                      quote lines with author's initials
//  pIndentQuoteLinesWithInitials: Whether or not indenting is enabled for
//                                 quote lines with initials
//
// Return value: An object containing the following properties:
//               startIndex: The index of the first non-quote character in the string.
//                           If pStr is an invalid string, or if a non-quote character
//                           is not found, this will be -1.
//               quoteLevel: The number of > characters at the start of the string
//               begOfLine: The quote text at the beginning of the line
function firstNonQuoteTxtIndex(pStr, pUseAuthorInitials, pIndentQuoteLinesWithInitials)
{
	// Create the return object with initial values.
	var retObj = getDefaultQuoteStrObj();  

	// If pStr is not a valid positive-length string, then just return.
	if ((pStr == null) || (typeof(pStr) != "string") || (pStr.length == 0))
		return retObj;

	// If using author initials, then do some special checking: If the first >
	// character is preceded by something other than spaces or 3 non-space characters,
	// then this string is probably not quoted, so return an object that signifies
	// such.
	if (pUseAuthorInitials)
	{
		var firstGTCharIdx = pStr.indexOf(">");
		if (firstGTCharIdx > -1)
		{
			// double-quoted text: If there are only spaces, > characters, or
			// up to 3 characters directly before the >> (without spaces), then
			// take this as a valid instance of double-quoted text.
			var upToThreeNonSpacesBefore = false;
			var onlySpaces = true;
			var currentChar;
			for (var srchIdx = 0; (srchIdx < pStr.length) && onlySpaces; ++srchIdx)
				onlySpaces = (pStr.charAt(srchIdx) == " ");
			if (!onlySpaces)
			{
				var startIdxBeforeGT = firstGTCharIdx - 4;
				if (startIdxBeforeGT < 0)
					startIdxBeforeGT = 0;
				// If the string don't contain a non > followed by a space before the >, then
				// go ahead and check the first 3 characters before the >.  Otherwise, it's
				// already disqualified.
				if (!/[^>] /.test(pStr.substr(startIdxBeforeGT, firstGTCharIdx-startIdxBeforeGT)))
				{
					upToThreeNonSpacesBefore = true;
					var numNonSpaceChars = 0;
					for (var srchIdx = firstGTCharIdx-1; srchIdx >= startIdxBeforeGT; --srchIdx)
					{
						if (pStr.charAt(srchIdx) != " ")
							++numNonSpaceChars;
					}
					upToThreeNonSpacesBefore = (numNonSpaceChars < 4);
				}
			}

			// If there aren't just spaces or up to 3 non-space characters just before
			// the first >, then return an object that signifies this situation properly.
			if (!onlySpaces && !upToThreeNonSpacesBefore)
			{
				retObj.startIndex = 0;
				retObj.quoteLevel = 0;
				retObj.begOfLine = "";
				return retObj;
			}
		}
	}

	// Look for quote lines that begin with 1 or 2 initials followed by a > (i.e.,
	// "EO>" or "E>" at the start of the line.  If found, set an index to look for
	// & count the > characters from the >.
	var searchStartIndex = 0;
	// Regex notes:
	//  \w: Matches any alphanumeric character (word characters) including underscore (short for [a-zA-Z0-9_])
	//  ?: Supposed to match 0 or 1 occurance, but seems to match 1 or 2
	// First, look for spaces then 1 or 2 initials followed by a non-space followed
	// by a >.  If not found, then look for ">>".  If that isn't found, then look
	// for just 2 characters followed by a >.
	var lineStartsWithQuoteText = /^ *\w?[^ ]>/.test(pStr);
	if (pUseAuthorInitials)
	{
		if (!lineStartsWithQuoteText)
			lineStartsWithQuoteText = (pStr.lastIndexOf(">>") > -1);
		if (!lineStartsWithQuoteText)
			lineStartsWithQuoteText = /\w{2}>/.test(pStr);
	}
	if (lineStartsWithQuoteText)
	{
		if (pUseAuthorInitials)
		{
			// If the string is an origin line (starting with " * Origin:"), then don't
			// do much with this line..  Just set the first non-space character in retObj.
			if (/^ \* Origin:/.test(pStr))
				retObj.startIndex = 1;
			else
			{
				// First, look for the last instance of ">> " (signifying a multi-quoted line).
				// If found, increment searchStartIndex by 2 to get past the ">>".
				var validDoubleQuoteChars = false;
				searchStartIndex = pStr.lastIndexOf(">> ");
				if (searchStartIndex > -1)
					searchStartIndex += 2;
				else
				{
					// If pStr is at least 3 characters long, then starting with the
					// last 3 characters in pStr, look for an instance of 2 letters
					// or numbers or underscores followed by a >.  Keep moving back
					// 1 character at a time until found or until the beginning of
					// the string is reached.
					if (pStr.length >= 3)
					{
						// Regex notes:
						//  \w: Matches any alphanumeric character (word characters) including underscore (short for [a-zA-Z0-9_])
						var substrStartIndex = pStr.length - 3;
						for (; (substrStartIndex >= 0) && (searchStartIndex < 0); --substrStartIndex)
							searchStartIndex = pStr.substr(substrStartIndex, 3).search(/^\w{2}>$/);
						++substrStartIndex; // To fix off-by-one
						if (searchStartIndex > -1)
						{
							searchStartIndex += substrStartIndex + 3; // To get past the "..>"
							// New (2017-12-24):
							// If the instance(s) of a > has 3 non-space characters
							// before it, then assume the > is not part of a quote
							// prefix, and look for another > earlier in the text string.
							// When using author initials, SlyEdit assumes a quote prefix
							// has up to 2 characters before the >.
							while ((searchStartIndex >= 4) && (pStr.substr(searchStartIndex-4, 4).search(/^[^\s]{3}>$/) >= 0))
							{
								searchStartIndex = pStr.lastIndexOf(">", searchStartIndex-2);
								if (searchStartIndex == -1)
									searchStartIndex = 0;
								else
									++searchStartIndex; // To fix off-by-one
							}
						}
						// Note: I originally had + 4 here..
						if (searchStartIndex < 0)
						{
							searchStartIndex = pStr.indexOf(">");
							if (searchStartIndex < 0)
								searchStartIndex = 0;
						}
					}
					else
					{
						searchStartIndex = pStr.indexOf(">");
						if (searchStartIndex < 0)
							searchStartIndex = 0;
					}
				}
			}
		}
		else
		{
			// SlyEdit is not prefixing quote lines with author's initials.
			searchStartIndex = pStr.indexOf(">");
			if (searchStartIndex < 0)
				searchStartIndex = 0;
		}
	}

	// Find the quote level and the beginning of the line.
	// Look for the first non-quote text and quote level in the string.
	var strChar = "";
	var j = 0;
	for (var i = searchStartIndex; i < pStr.length; ++i)
	{
		strChar = pStr.charAt(i);
		if ((strChar != " ") && (strChar != ">"))
		{
			// New (2017-12-24):
			// If using author initials and there are 3 non-space characters
			// before the >, then continue to the next character.
			if (i >= 3)
			{
				if (pUseAuthorInitials && (pStr.substr(i-3, 4).search(/^[^\s]{3}>$/) >= 0))
					continue;
			}

			// We've found the first non-quote character.
			retObj.startIndex = i;
			// Count the number of times the > character appears at the start of
			// the line, and set quoteLevel to that.
			if (i >= 0)
			{
				for (j = 0; j < i; ++j)
				{
					if (pStr.charAt(j) == ">")
					{
						// New (2017-12-24):
						// If using author initials, then increment the quote level
						// only if there are not 3 non-space characters before the >
						if (pUseAuthorInitials && (j >= 3))
						{
							if (pStr.substr(j-3, 4).search(/^[^\s]{3}>$/) < 0)
								++retObj.quoteLevel;
						}
						else
							++retObj.quoteLevel;
					}
				}
			}
			// Store the beginning of the line in retObj.begOfLine.  And if
			// SlyEdit is configured to indent quote lines with author initials,
			// and if the beginning of the line doesn't begin with a space,
			// then add a space to the beginning of it.
			retObj.begOfLine = pStr.substr(0, retObj.startIndex);
			if (pUseAuthorInitials && pIndentQuoteLinesWithInitials && (retObj.begOfLine.length > 0) && (retObj.begOfLine.charAt(0) != " "))
				retObj.begOfLine = " " + retObj.begOfLine;
			break;
		}
	}

	// If we haven't found non-quote text but the line starts with quote text,
	// then set the starting index & quote level in retObj.
	if (lineStartsWithQuoteText && ((retObj.startIndex == -1) || (retObj.quoteLevel == 0)))
	{
		retObj.startIndex = pStr.indexOf(">") + 1;
		// New (2017-12-24):
		var setQuoteLevel = true;
		// When using author initials in quote lines: If there are 3 non-space
		// characters before the >, then it's not an actual quote (SlyEdit
		// considers quote lines with initials to have only 2 characters before
		// the >).
		if (pUseAuthorInitials && retObj.startIndex >= 4)
		{
			if (pStr.substr(retObj.startIndex-4, 4).search(/^[^\s]{3}>$/) >= 0)
			{
				retObj.startIndex = 0;
				setQuoteLevel = false;
			}
		}
		if (setQuoteLevel)
			retObj.quoteLevel = 1;
	}

	return retObj;
}

// Performs text wrapping on the quote lines.
//
// Parameters:
//  pUseAuthorInitials: Whether or not to prefix quote lines with the last author's
//                      initials
// pIndentQuoteLinesWithInitials: If prefixing the quote lines with the
//                                last author's initials, this parameter specifies
//                                whether or not to also prefix the quote lines with
//                                a space.
//  pTrimSpacesFromQuoteLines: Whether or not to trim spaces from quote lines (for when people
//                             indent the first line of their reply, etc.).  Defaults to true.
function wrapQuoteLines(pUseAuthorInitials, pIndentQuoteLinesWithInitials, pTrimSpacesFromQuoteLines)
{
	var useAuthorInitials = true;
	var indentQuoteLinesWithInitials = false;
	if (typeof(pUseAuthorInitials) != "undefined")
		useAuthorInitials = pUseAuthorInitials;
	if (typeof(pIndentQuoteLinesWithInitials) != "undefined")
		indentQuoteLinesWithInitials = pIndentQuoteLinesWithInitials;

	var trimSpacesFromQuoteLines = (typeof(pTrimSpacesFromQuoteLines) == "boolean" ? pTrimSpacesFromQuoteLines : true);
	if (useAuthorInitials)
		wrapQuoteLinesUsingAuthorInitials(pIndentQuoteLinesWithInitials, trimSpacesFromQuoteLines);
	else
		wrapQuoteLines_NoAuthorInitials(trimSpacesFromQuoteLines);
}

// For wrapping quote lines: This function checks if a string has only > characters
// separated by whitespace and returns a version where the > characters are only
// separated by one space each, and if the line starts with " >", the leading space
// will be removed.
function normalizeGTChars(pStr)
{
	if (/^\s*>\s*$/.test(pStr))
		pStr = ">";
	else
	{
		pStr = pStr.replace(/>\s*>/g, "> >")
		           .replace(/^\s>/, ">")
		           .replace(/^\s*$/, "");
	}
	return pStr;
}

// Wraps quote lines and prefixes them with the original author's initials.
// Assumes gQuotePrefix contains the author's initials.
//
// Parameters:
//  pIndentQuoteLines: Whether or not to indent the quote lines
//  pTrimSpacesFromQuoteLines: Whether or not to trim spaces from quote lines (for when people
//                             indent the first line of their reply, etc.).  Defaults to true.
function wrapQuoteLinesUsingAuthorInitials(pIndentQuoteLines, pTrimSpacesFromQuoteLines)
{
	if (gQuoteLines.length == 0)
		return;

	// Steps for wrapping quote lines:
	// 1. Get information for each line (quote level, beginning of line, etc.)
	// 2. Based on the line info, find the different sections of the quote lines
	// 3. Go through each section of the quote lines and quote appropriately

	// Note: gQuotePrefix is declared in SlyEdit.js.
	// Make another copy of it without its leading space for searching the
	// quote lines later.
	var quotePrefixWithoutLeadingSpace = gQuotePrefix.replace(/^ /, "");

	// 1. Get information for each line (quote level, beginning of line, etc.)
	var lineInfos = new Array();
	for (var quoteLineIndex = 0; quoteLineIndex < gQuoteLines.length; ++quoteLineIndex)
		lineInfos.push(firstNonQuoteTxtIndex(gQuoteLines[quoteLineIndex], true, pIndentQuoteLines));

	// 2. Based on the line info, find the different sections of the quote lines
	var quoteSections = new Array();
	var startArrIndex = 0;
	var endArrIndex = -1;
	var lastQuoteLevel = lineInfos[0].quoteLevel;
	for (var quoteLineIndex = 1; quoteLineIndex < gQuoteLines.length; ++quoteLineIndex)
	{
		endArrIndex = -1; // Resetting to help ensure that we get the last section sometimes

		if (gQuoteLines[quoteLineIndex].length == 0)
			continue;

		// If this line has a different quote level than the previous line, then
		// it marks a new section.
		if (lineInfos[quoteLineIndex].quoteLevel != lastQuoteLevel)
		{
			endArrIndex = quoteLineIndex;
			var sectionInfo = new Object();
			sectionInfo.startArrIndex = startArrIndex;
			sectionInfo.endArrIndex = endArrIndex;
			sectionInfo.quoteLevel = lastQuoteLevel;
			// If the end array index is for a blank quote line, then
			// adjust it to the first non-blank quote line before it.
			while ((sectionInfo.endArrIndex-1 >= 0) &&
			       (typeof(gQuoteLines[sectionInfo.endArrIndex-1]) == "string") &&
			       gQuoteLines[sectionInfo.endArrIndex-1].length == 0)
			{
				--sectionInfo.endArrIndex;
			}
			// If we moved sectionInfo.endArrIndex back too far, then increment it.
			while (typeof(gQuoteLines[sectionInfo.endArrIndex]) != "string")
				++sectionInfo.endArrIndex;

			quoteSections.push(sectionInfo);
			startArrIndex = quoteLineIndex;
			lastQuoteLevel = lineInfos[quoteLineIndex].quoteLevel;
		}
		// For lines with a quote level of 0, if this line's indentation differs from
		// the previous line's indentation, then that marks a new section.
		else if ((lineInfos[quoteLineIndex].quoteLevel == 0) && (lastQuoteLevel == 0) &&
		         (lineInfos[quoteLineIndex].startIndex > lineInfos[quoteLineIndex-1].startIndex))
		{
			endArrIndex = quoteLineIndex; // One past the last index of the current paragraph
			var sectionInfo = new Object();
			sectionInfo.startArrIndex = startArrIndex;
			sectionInfo.endArrIndex = endArrIndex;
			sectionInfo.quoteLevel = 0;
			// If the end array index is for a blank quote line, then
			// adjust it to the first non-blank quote line before it.
			while ((sectionInfo.endArrIndex-1 >= 0) &&
			       (typeof(gQuoteLines[sectionInfo.endArrIndex-1]) == "string") &&
			gQuoteLines[sectionInfo.endArrIndex-1].length == 0)
			{
				--sectionInfo.endArrIndex;
			}
			// If we moved sectionInfo.endArrIndex back too far, then increment it.
			while (typeof(gQuoteLines[sectionInfo.endArrIndex]) != "string")
				++sectionInfo.endArrIndex;

			quoteSections.push(sectionInfo);
			startArrIndex = quoteLineIndex;
		}
	}
	// If we only found one section or we're at the last section, then add it to
	// quoteSections.
	if ((endArrIndex == -1) || (endArrIndex == gQuoteLines.length-1))
	{
		var sectionInfo = new Object();
		sectionInfo.startArrIndex = startArrIndex;
		sectionInfo.endArrIndex = gQuoteLines.length;
		sectionInfo.quoteLevel = lastQuoteLevel;
		// If the end array index is for a blank quote line, then
		// adjust it to the first non-blank quote line before it.
		while ((sectionInfo.endArrIndex > 0) && (gQuoteLines[sectionInfo.endArrIndex-1].length == 0))
			--sectionInfo.endArrIndex;
		quoteSections.push(sectionInfo);
	}

	// 3. Go through each section of the quote lines and wrap & quote appropriately
	var trimSpacesFromQuoteLines = (typeof(pTrimSpacesFromQuoteLines) == "boolean" ? pTrimSpacesFromQuoteLines : true);
	for (var sIndex = 0; sIndex < quoteSections.length; ++sIndex)
	{
		// If the section is not quoted text (in other words, it was written by
		// author of the message), then remove leading whitespace from the text
		// lines in this section to leave more room for wrapping and so that we
		// don't end up with a section of quote lines that all start with several
		// spaces.
		if (quoteSections[sIndex].quoteLevel == 0)
		{
			for (var i = quoteSections[sIndex].startArrIndex; i < quoteSections[sIndex].endArrIndex; ++i)
			{
				if (trimSpacesFromQuoteLines)
					gQuoteLines[i] = trimSpaces(gQuoteLines[i], true, true, false);
				lineInfos[i].startIndex = 0;
				lineInfos[i].begOfLine = "";
			}
		}

		// Remove the quote strings from the lines we're about to wrap
		var maxBegOfLineLen = 0;
		for (var i = quoteSections[sIndex].startArrIndex; i < quoteSections[sIndex].endArrIndex; ++i)
		{
			if (lineInfos[i] != null)
			{
				if (lineInfos[i].startIndex > -1)
					gQuoteLines[i] = gQuoteLines[i].substr(lineInfos[i].startIndex);
				else
					gQuoteLines[i] = normalizeGTChars(gQuoteLines[i]);

				// If the quote line now only consists of spaces after removing the quote
				// characters, then make it blank.
				if (/^ +$/.test(gQuoteLines[i]))
					gQuoteLines[i] = "";
				// Change multiple spaces to single spaces in the beginning-of-line
				// string.  Also, if not prefixing quote lines w/ initials with a
				// space, then also trim leading spaces.
				if (pIndentQuoteLines)
					lineInfos[i].begOfLine = trimSpaces(lineInfos[i].begOfLine, false, true, false);
				else
					lineInfos[i].begOfLine = trimSpaces(lineInfos[i].begOfLine, true, true, false);

				// See if we need to update maxBegOfLineLen, and if so, do it.
				if (lineInfos[i].begOfLine.length > maxBegOfLineLen)
					maxBegOfLineLen = lineInfos[i].begOfLine.length;
			}
		}
		// If maxBegOfLineLen is positive, then add 1 more to it because
		// we'll be adding a > character to the quote lines to signify one
		// more level of quoting.
		if (maxBegOfLineLen > 0)
			++maxBegOfLineLen;
		// Add gQuotePrefix's length to maxBegOfLineLen to account for that
		// for wrapping the text. Note: In future versions, if we don't want
		// to add the previous author's initials to all lines, then we might
		// not automatically want to add this to every line.
		maxBegOfLineLen += gQuotePrefix.length;

		// Wrap the current section of quote lines
		var maxLineWidth = 79 - maxBegOfLineLen;
		if (maxLineWidth < 0)
			maxLineWidth = 0;
		var idxesAddedNL = new Array();
		var numLinesAdded = 0;
		if (maxLineWidth > 0)
		{
			numLinesAdded = wrapTextLines(gQuoteLines, quoteSections[sIndex].startArrIndex,
			                              quoteSections[sIndex].endArrIndex, maxLineWidth,
			                              idxesAddedNL, lineInfos);
		}

		// If quote lines were added as a result of wrapping, then determine the
		// number of lines added and update the end index of this object in
		// quoteSections and the start & end indexes of the subsequent objects in
		// quoteSections.
		if (numLinesAdded > 0)
		{
			// Splice new lineInfo objects into the lineInfos array at the end of this
			// section for each new line added in this section.
			for (var counter = 0; counter < numLinesAdded; ++counter)
				lineInfos.splice(quoteSections[sIndex].endArrIndex, 0, getDefaultQuoteStrObj());
			// Now we can update this section's end index.  Then, after each index that
			// required a new line to be added, move the lineInfo information down one line.
			quoteSections[sIndex].endArrIndex += numLinesAdded;
			for (var NLArrIdx = 0; NLArrIdx < idxesAddedNL.length; ++NLArrIdx)
			{
				for (var lnInfoMoveIdx = quoteSections[sIndex].endArrIndex-1;
				     lnInfoMoveIdx > idxesAddedNL[NLArrIdx]; --lnInfoMoveIdx)
				{
					lineInfos[lnInfoMoveIdx].copy(lineInfos[lnInfoMoveIdx-1]);
				}
			}

			// Update the start & end indexes of the following sections.
			for (var sIndex2 = sIndex+1; sIndex2 < quoteSections.length; ++sIndex2)
			{
				quoteSections[sIndex2].startArrIndex += numLinesAdded;
				quoteSections[sIndex2].endArrIndex += numLinesAdded;
			}

			// Go through this section's quote lines, and for each quote line that is
			// non-blank and has a lineInfo object with a blank beginning-of-line text,
			// set its beginning-of-line text to the first non-blank beginning-of-line
			// text from a line preceding it.
			for (var lnIdx = quoteSections[sIndex].startArrIndex+1; lnIdx < quoteSections[sIndex].endArrIndex; ++lnIdx)
			{
				if ((gQuoteLines[lnIdx].length > 0) && (lineInfos[lnIdx].begOfLine.length == 0))
				{
					var nonBlankIdx = lnIdx - 1;
					while ((lineInfos[nonBlankIdx].begOfLine.length == 0) && (nonBlankIdx > quoteSections[sIndex].startArrIndex))
						--nonBlankIdx;
					if (lineInfos[nonBlankIdx].begOfLine.length > 0)
						lineInfos[lnIdx].begOfLine = lineInfos[nonBlankIdx].begOfLine;
				}
			}
		}

		// Go through this section's (now wrapped) quote lines and quote them
		for (var quoteLnIdx = quoteSections[sIndex].startArrIndex; quoteLnIdx < quoteSections[sIndex].endArrIndex; ++quoteLnIdx)
		{
			if (lineInfos[quoteLnIdx] != null)
			{
				if (gQuoteLines[quoteLnIdx].length == 0)
					continue;

				// If the quote level in this section is positive and the beginning
				// of the line has a non-zero length, then add a > at the end to
				// signify that this line is being quoted again.
				var begOfLineLen = lineInfos[quoteLnIdx].begOfLine.length;
				if ((begOfLineLen > 0) && (quoteSections[sIndex].quoteLevel > 0))
				{
					if (lineInfos[quoteLnIdx].begOfLine.charAt(begOfLineLen-1) == " ")
						lineInfos[quoteLnIdx].begOfLine = lineInfos[quoteLnIdx].begOfLine.substr(0, begOfLineLen-1) + "> ";
					else
						lineInfos[quoteLnIdx].begOfLine += ">";
				}
				// Re-assemble the quote line
				gQuoteLines[quoteLnIdx] = lineInfos[quoteLnIdx].begOfLine + gQuoteLines[quoteLnIdx];
				if (quoteSections[sIndex].quoteLevel == 0)
					gQuoteLines[quoteLnIdx] = gQuotePrefix + gQuoteLines[quoteLnIdx];
			}
			else
			{
				// Old style: Put quote strings ("> ") back into the lines we just wrapped.
				var quotePrefix = "";
				for (var counter = 0; counter < lastQuoteLevel; ++counter)
					quotePrefix += "> ";
				gQuoteLines[quoteLnIdx] = quotePrefix + gQuoteLines[quoteLnIdx].replace(/^\s*>/, ">");
			}
		}
	}
}

// Wraps the quote lines without using the originals author's initials
// (classic quoting).
// Assumes gQuotePrefix does not contains the author's initials.
//
// Parameters:
//  pTrimSpacesFromQuoteLines: Whether or not to trim spaces from quote lines (for when people
//                             indent the first line of their reply, etc.).  Defaults to true.
function wrapQuoteLines_NoAuthorInitials(pTrimSpacesFromQuoteLines)
{
	if (gQuoteLines.length == 0)
		return;

	// Create an array for line information objects.
	var lineInfos = new Array();
	for (var quoteLineIndex = 0; quoteLineIndex < gQuoteLines.length; ++quoteLineIndex)
		lineInfos.push(firstNonQuoteTxtIndex(gQuoteLines[quoteLineIndex], false, false));

	// Set an initial value for lastQuoteLevel, which will be used to compare the
	// quote levels of each line.
	var lastQuoteLevel = lineInfos[0].quoteLevel;

	// Loop through the array starting at the 2nd line and wrap the lines
	var startArrIndex = 0;
	var endArrIndex = 0;
	var quoteStr = "";
	var quoteLevel = 0;
	var i = 0; // Index variable
	for (var quoteLineIndex = 1; quoteLineIndex < gQuoteLines.length; ++quoteLineIndex)
	{
		if (lineInfos[quoteLineIndex].quoteLevel != lastQuoteLevel)
		{
			endArrIndex = quoteLineIndex;
			// Remove the quote strings from the lines we're about to wrap
			for (i = startArrIndex; i < endArrIndex; ++i)
			{
				if (lineInfos[i] != null)
				{
					if (lineInfos[i].startIndex > -1)
						gQuoteLines[i] = gQuoteLines[i].substr(lineInfos[i].startIndex);
					else
						gQuoteLines[i] = normalizeGTChars(gQuoteLines[i]);
					// If the quote line now only consists of spaces after removing the quote
					// characters, then make it blank.
					if (/^ +$/.test(gQuoteLines[i]))
						gQuoteLines[i] = "";
				}
			}
			// Wrap the text lines in the range we've seen.
			// Note: 79 is assumed as the maximum line length because
			// that seems to be a commonly-accepted message width for
			// BBSes.  Also, the following length is subtracted from it:
			// (2*(lastQuoteLevel+1) + gQuotePrefix.length)
			// That is because we'll be prepending "> " to the quote lines,
			// and then SlyEdit will prepend gQuotePrefix to them during quoting.
			var numLinesAdded =  wrapTextLines(gQuoteLines, startArrIndex, endArrIndex,
			                                   79 - (2*(lastQuoteLevel+1) + gQuotePrefix.length));
			// If quote lines were added as a result of wrapping, then
			// determine the number of lines added, and update endArrIndex
			// and quoteLineIndex accordingly.
			if (numLinesAdded > 0)
			{
				endArrIndex += numLinesAdded;
				quoteLineIndex += (numLinesAdded-1); // - 1 because quoteLineIndex will be incremented by the for loop
				// Splice new lineInfo objects into the lineInfos array at the end of this
				// section for each new line added in this section.
				for (var counter = 0; counter < numLinesAdded; ++counter)
					lineInfos.splice(endArrIndex, 0, getDefaultQuoteStrObj());
			}
			// Put quote strings ("> ") back into the lines we just wrapped
			if ((quoteLineIndex > 0) && (lastQuoteLevel > 0))
			{
				quoteStr = "";
				for (i = 0; i < lastQuoteLevel; ++i)
					quoteStr += "> ";
				for (i = startArrIndex; i < endArrIndex; ++i)
					gQuoteLines[i] = quoteStr + gQuoteLines[i].replace(/^\s*>/, ">");
			}
			lastQuoteLevel = lineInfos[quoteLineIndex].quoteLevel;
			startArrIndex = quoteLineIndex;
		}
		// For lines with a quote level of 0, if this line's indentation differs from
		// the previous line's indentation, then that marks a new section.
		else if ((lineInfos[quoteLineIndex].quoteLevel == 0) && (lastQuoteLevel == 0) &&
		         (lineInfos[quoteLineIndex].startIndex > lineInfos[quoteLineIndex-1].startIndex))
		{
			endArrIndex = quoteLineIndex;

			// Remove leading whitespace from the text lines in this section to leave
			// more room for wrapping and so that we don't end up with a section of
			// quote lines that all start with several spaces.
			var trimSpacesFromQuoteLines = (typeof(pTrimSpacesFromQuoteLines) == "boolean" ? pTrimSpacesFromQuoteLines : true);
			for (var i = startArrIndex; i < endArrIndex; ++i)
			{
				if (trimSpacesFromQuoteLines)
					gQuoteLines[i] = trimSpaces(gQuoteLines[i], true, true, false);
				lineInfos[i].startIndex = 0;
				lineInfos[i].begOfLine = "";
			}

			// Wrap the text lines in the range we've seen.
			// Note: 79 is assumed as the maximum line length because
			// that seems to be a commonly-accepted message width for
			// BBSes.
			var numLinesAdded = wrapTextLines(gQuoteLines, startArrIndex, endArrIndex, 79);
			// If quote lines were added as a result of wrapping, then
			// determine the number of lines added, and update endArrIndex
			// and quoteLineIndex accordingly.
			if (numLinesAdded > 0)
			{
				endArrIndex += numLinesAdded;
				quoteLineIndex += (numLinesAdded-1); // - 1 because quoteLineIndex will be incremented by the for loop
				// Splice new lineInfo objects into the lineInfos array at the end of this
				// section for each new line added in this section.
				for (var counter = 0; counter < numLinesAdded; ++counter)
					lineInfos.splice(endArrIndex, 0, getDefaultQuoteStrObj());
			}
			startArrIndex = quoteLineIndex;
		}
	}
	// Wrap the last block of lines
	wrapTextLines(gQuoteLines, startArrIndex, gQuoteLines.length, 79 - (2*(lastQuoteLevel+1) + gQuotePrefix.length));

	// Go through the quote lines again, and for ones that start with " >", remove
	// the leading whitespace.  This is because the quote string is " > ", so it
	// would insert an extra space before the first > in the quote line.
	for (i = 0; i < gQuoteLines.length; ++i)
		gQuoteLines[i] = gQuoteLines[i].replace(/^\s*>/, ">");
}

// Returns an object containing the following properties:
//  lastMsg: The last message in the sub-board (i.e., bbs.smb_last_msg)
//  totalNumMsgs: The total number of messages in the sub-board (i.e., bbs.smb_total_msgs)
//  curMsgNum: The number/index of the current message being read.  Starting
//             with Synchronet 3.16 on May 12, 2013, this is the absolute
//             message number (bbs.msg_number).  For Synchronet builds before
//             May 12, 2013, this is bbs.smb_curmsg.  Starting on May 12, 2013,
//             bbs.msg_number is preferred because it works properly in all
//             situations, whereas in earlier builds, bbs.msg_number was
//             always given to JavaScript scripts as 0.
//  msgNumIsOffset: Boolean - Whether or not the message number is an offset.
//                  If not, then it is the absolute message number (i.e.,
//                  bbs.msg_number).
//  subBoardCode: The current sub-board code (i.e., bbs.smb_sub_code)
//  grpIndex: The message group index for the sub-board
//
// This function First tries to read the values from the file
// DDML_SyncSMBInfo.txt in the node directory (written by the Digital
// Distortion Message Lister v1.31 and higher).  If that file can't be read,
// the values will default to the values of bbs.smb_last_msg,
// bbs.smb_total_msgs, and bbs.msg_number/bbs.smb_curmsg.
//
// Parameters:
//  pMsgAreaName: The name of the message area being posted to
function getCurMsgInfo(pMsgAreaName)
{
  var retObj = new Object();
  retObj.msgNumIsOffset = false;
  if (bbs.smb_sub_code.length > 0)
  {
    retObj.lastMsg = bbs.smb_last_msg;
    retObj.totalNumMsgs = bbs.smb_total_msgs;
    // If bbs.msg_number is valid (greater than 0), then use it.  Otherwise,
    // use the older behavior of using bbs.smb_curmsg (the offset) instead.
    // bbs.msg_number was correct in Synchronet 3.16 builds starting on
    // May 12, 2013.
    //retObj.curMsgNum = (bbs.msg_number > 0 ? bbs.msg_number : bbs.smb_curmsg);
    if (bbs.msg_number > 0)
      retObj.curMsgNum = bbs.msg_number;
    else
    {
      retObj.curMsgNum = bbs.smb_curmsg;
      retObj.msgNumIsOffset = true;
    }
    retObj.subBoardCode = bbs.smb_sub_code;
    retObj.grpIndex = msg_area.sub[bbs.smb_sub_code].grp_index;
  }
  else
  {
    retObj.lastMsg = -1;
    retObj.curMsgNum = -1;
    // If the user has a valid current sub-board code, then use it;
    // otherwise, find the first sub-board the user is able to post
    // in and use that.
    if (typeof(msg_area.sub[bbs.cursub_code]) != "undefined")
    {
      retObj.subBoardCode = bbs.cursub_code;
      retObj.grpIndex = msg_area.sub[bbs.cursub_code].grp_index;
    }
    else
    {
      var firstPostableSubInfo = getFirstPostableSubInfo();
      retObj.subBoardCode = firstPostableSubInfo.subCode;
      retObj.grpIndex = firstPostableSubInfo.grpIndex;
    }

    // If we got a valid sub-board code, then open that sub-board
    // and get the total number of messages from it.
    if (retObj.subBoardCode.length > 0)
    {
      var tmpMsgBaseObj = new MsgBase(retObj.subBoardCode);
      if (tmpMsgBaseObj.open())
      {
        retObj.totalNumMsgs = tmpMsgBaseObj.total_msgs;
        tmpMsgBaseObj.close();
      }
      else
        retObj.totalNumMsgs = 0;
    }
    else
       retObj.totalNumMsgs = 0;
  }
  // If pMsgAreaName is valid, then if it specifies a message area name that is
  // different from what's in retObj, then we probably want to use bbs.cursub_code
  // instead of bbs.smb_sub_code, etc.
  // Note: As of the May 8, 2013 build of Synchronet (3.16), the bbs.smb_sub*
  // properties reflect the current sub-board being posted to, always.
  // Digital Man committed a change in CVS for this on May 7, 2013.
  if ((typeof(pMsgAreaName) == "string") && (pMsgAreaName.length > 0))
  {
    if (msg_area.sub[retObj.subBoardCode].name.indexOf(pMsgAreaName) == -1)
    {
      retObj.lastMsg = -1;
      retObj.curMsgNum = -1;
      // If the user has a valid current sub-board code, then use it;
      // otherwise, find the first sub-board the user is able to post
      // in and use that.
      if (typeof(msg_area.sub[bbs.cursub_code]) != "undefined")
      {
        retObj.subBoardCode = bbs.cursub_code;
        retObj.grpIndex = msg_area.sub[bbs.cursub_code].grp_index;
      }
      else
      {
        var firstPostableSubInfo = getFirstPostableSubInfo();
        retObj.subBoardCode = firstPostableSubInfo.subCode;
        retObj.grpIndex = firstPostableSubInfo.grpIndex;
      }

      // If we got a valid sub-board code, then open that sub-board
      // and get the total number of messages from it.
      if (retObj.subBoardCode.length > 0)
      {
        var tmpMsgBaseObj = new MsgBase(retObj.subBoardCode);
        if (tmpMsgBaseObj.open())
        {
          retObj.totalNumMsgs = tmpMsgBaseObj.total_msgs;
          tmpMsgBaseObj.close();
        }
        else
          retObj.totalNumMsgs = 0;
      }
      else
       retObj.totalNumMsgs = 0;
    }
  }

  // If the Digital Distortion Message Lister drop file exists,
  // then use the message information from that file instead.
  if (file_exists(gDDML_DROP_FILE_NAME))
  {
    var SMBInfoFile = new File(gDDML_DROP_FILE_NAME);
    if (SMBInfoFile.open("r"))
    {
      var fileLine = null; // A line read from the file
      var lineNum = 0; // Will be incremented at the start of the while loop, to start at 1.
      while (!SMBInfoFile.eof)
      {
         ++lineNum;

         // Read the next line from the config file.
         fileLine = SMBInfoFile.readln(2048);

         // fileLine should be a string, but I've seen some cases
         // where for some reason it isn't.  If it's not a string,
         // then continue onto the next line.
         if (typeof(fileLine) != "string")
            continue;

          // Depending on the line number, set the appropriate value
          // in retObj.
          switch (lineNum)
          {
            case 1:
              retObj.lastMsg = +fileLine;
              break;
            case 2:
              retObj.totalNumMsgs = +fileLine;
              break;
            case 3:
              retObj.curMsgNum = +fileLine;
              retObj.msgNumIsOffset = false; // For Message Lister 1.36 and newer
              break;
            case 4:
              retObj.subBoardCode = fileLine;
              retObj.grpIndex = msg_area.sub[retObj.subBoardCode].grp_index;
              break;
            default:
              break;
          }
       }
       SMBInfoFile.close();
    }
  }

  return retObj;
}

// Gets the "From" name of the current message being replied to.
// Only for use when replying to a message in a public sub-board.
// The message information is retrieved from DDML_SyncSMBInfo.txt
// in the node dir if it exists or from the bbs object's properties.
// On error, the string returned will be blank.
//
// Parameters:
//  pMsgInfo: Optional: An object returned by getCurMsgInfo().  If this
//            parameter is not specified, this function will call
//            getCurMsgInfo() to get it.
//
// Return value: The from name of the current message being replied
//               to (a string).
function getFromNameForCurMsg(pMsgInfo)
{
  var fromName = "";

  // Get the information about the current message from
  // DDML_SyncSMBInfo.txt in the node dir if it exists or from
  // the bbs object's properties.  Then open the message header
  // and get the 'from' name from it.
  var msgInfo = null;
  if ((pMsgInfo != null) && (typeof(pMsgInfo) != "undefined"))
    msgInfo = pMsgInfo;
  else
    msgInfo = getCurMsgInfo();

  if (msgInfo.subBoardCode.length > 0)
  {
    var msgBase = new MsgBase(msgInfo.subBoardCode);
    if (msgBase != null)
    {
      msgBase.open();
      var hdr = msgBase.get_msg_header(msgInfo.msgNumIsOffset, msgInfo.curMsgNum, true);
      if (hdr != null)
        fromName = hdr.from;
      msgBase.close();
    }
  }

  return fromName;
}

// Calculates & returns a page number.
//
// Parameters:
//  pTopIndex: The index (0-based) of the topmost item on the page
//  pNumPerPage: The number of items per page
//
// Return value: The page number
function calcPageNum(pTopIndex, pNumPerPage)
{
  return ((pTopIndex / pNumPerPage) + 1);
}

// Returns whether or not the user is posting in a message sub-board.
// If false, that means the user is sending private email or netmail.
// This function determines whether the user is posting in a message
// sub-board if the message area name is not "Electronic Mail" and
// is not "NetMail".
//
// Parameters:
//  pMsgAreaName: The name of the message area.
function postingInMsgSubBoard(pMsgAreaName)
{
	if (typeof(pMsgAreaName) != "string")
		return false;
	if (pMsgAreaName.length == 0)
		return false;

	return ((pMsgAreaName != "Electronic Mail") && (pMsgAreaName != "NetMail"));
}

// Returns the number of properties of an object.
//
// Parameters:
//  pObject: The object for which to count properties
//
// Return value: The number of properties of the object
function numObjProperties(pObject)
{
  if (pObject == null)
    return 0;

  var numProps = 0;
  for (var prop in pObject) ++numProps;
  return numProps;
}

//
// Paramters:
//  pSubBoardCode: Synchronet's internal code for the sub-board to post in
//  pTo: The name of the person to send the message to
//  pSubj: The subject of the email
//  pMessage: The email message
//  pFromUserNum: The number of the user to use as the message sender.
//                This is optional; if not specified, the current user
//                will be used.
//
// Return value: String - Blank on success, or message on failure.
function postMsgToSubBoard(pSubBoardCode, pTo, pSubj, pMessage, pFromUserNum)
{
	// Return if the parameters are invalid.
	if (typeof(pSubBoardCode) != "string")
		return ("Sub-board code is not a string");
	if (typeof(pTo) != "string")
		return ("To name is not a string");
	if (pTo.length == 0)
		return ("The 'to' user name is blank");
	if (typeof(pSubj) != "string")
		return ("Subject is not a string");
	if (pSubj.length == 0)
		return ("The subject is blank");
	if (typeof(pMessage) != "string")
		return ("Message is not a string");
	if (pMessage.length == 0)
		return ("Not sending an empty message");
	if (typeof(pFromUserNum) != "number")
		return ("From user number is not a number");
	if ((pFromUserNum <= 0) || (pFromUserNum > system.lastuser))
		return ("Invalid user number");

	// LOad the user record specified by pFromUserNum.  If it's a deleted user,
	// then return an error.
	var fromUser = new User(pFromUserNum);
	if (fromUser.settings & USER_DELETED)
		return ("The 'from' user is marked as deleted");

	// Open the sub-board so that the message can be posted there.
	var msgbase = new MsgBase(pSubBoardCode);
	if (!msgbase.open())
		return ("Error opening the message area: " + msgbase.last_error);

	// Create the message header, and send the message.
	var header = new Object();
	header.to = pTo;
	header.from_net_type = NET_NONE;
	header.to_net_type = NET_NONE;
	// For the 'From' name, use the user's real name if the sub-board is set
	// up to post using real names; otherwise, use the user's alias.
	if ((msgbase.cfg.settings & SUB_NAME) == SUB_NAME)
		header.from = fromUser.name;
	else
		header.from = fromUser.alias;
	header.from_ext = fromUser.number;
	header.from_net_addr = fromUser.netmail;
	header.subject = pSubj;
	var saveRetval = msgbase.save_msg(header, pMessage);
	msgbase.close();

	if (!saveRetval)
		return ("Error saving the message: " + msgbase.last_error);

	return "";
}

// Reads the current user's message signature file (if it exists)
// and returns its contents.
//
// Return value: An object containing the following properties:
//               sigFileExists: Boolean - Whether or not the user's signature file exists
//               sigContents: String - The user's message signature
function readUserSigFile()
{
	var retObj = {
		sigFileExists: false,
		sigContents: ""
	};

	// The user signature files are located in sbbs/data/user, and the filename
	// is the user number (zero-padded up to 4 digits) + .sig
	var userSigFilename = backslash(system.data_dir + "user") + format("%04d.sig", user.number);
	retObj.sigFileExists = file_exists(userSigFilename);
	if (retObj.sigFileExists)
	{
		var msgSigFile = new File(userSigFilename);
		if (msgSigFile.open("r"))
		{
			var fileLine = ""; // A line read from the file
			while (!msgSigFile.eof)
			{
				fileLine = msgSigFile.readln(2048);
				// fileLine should be a string, but I've seen some cases
				// where for some reason it isn't.  If it's not a string,
				// then continue onto the next line.
				if (typeof(fileLine) != "string")
					continue;

				retObj.sigContents += fileLine + "\r\n";
			}

			msgSigFile.close();
		}
	}

	return retObj;
}

// Returns the sub-board code and group index for the first sub-board
// the user is allowed to post in.  If none are found, the sub-board
// code will be a blank string and the group index will be 0.
//
// Return value: An object with the following properties:
//               subCode: The sub-board code
//               grpIndex: The group index of the sub-board
function getFirstPostableSubInfo()
{
  var retObj = new Object();
  retObj.subCode = "";
  retObj.grpIndex = 0;

  var continueOn = true;
  for (var groupIdx = 0; (groupIdx < msg_area.grp_list.length) && continueOn; ++groupIdx)
  {
     for (var subIdx = 0; (subIdx < msg_area.grp_list[groupIdx].sub_list.length) && continueOn; ++subIdx)
     {
        if (user.compare_ars(msg_area.grp_list[groupIdx].sub_list[subIdx].ars) &&
            user.compare_ars(msg_area.grp_list[groupIdx].sub_list[subIdx].post_ars))
        {
           retObj.subCode = msg_area.grp_list[groupIdx].sub_list[subIdx].code;
           retObj.grpIndex = groupIdx;
           continueOn = false;
           break;
        }
     }
  }

  return retObj;
}

// Reads SlyEdit_TextReplacements.cfg (from sbbs/mods, sbbs/ctrl, or the
// script's directory) and populates an associative array with the WORD=text
// pairs.  When not using regular expressions, the key will be in all uppercase
// and the value in lowercase.  This function will read up to 9999 replacements.
//
// Parameters:
//  pArray: The array to populate.  Must be created as "new Array()".
//  pRegex: Whether or not the text replace feature is configured to use regular
//          expressions.  If so, then the search words in the array will not
//          be converted to uppercase and the replacement text will not be
//          converted to lowercase.
//
// Return value: The number of text replacements added to the array.
function populateTxtReplacements(pArray, pRegex)
{
	var numTxtReplacements = 0;

	// Note: Limited to words without spaces.
	// Open the word replacements configuration file
	var wordReplacementsFilename = genFullPathCfgFilename("SlyEdit_TextReplacements.cfg", gStartupPath);
	var arrayPopulated = false;
	var wordFile = new File(wordReplacementsFilename);
	if (wordFile.open("r"))
	{
		var fileLine = null;      // A line read from the file
		var equalsPos = 0;        // Position of a = in the line
		var wordToSearch = null; // A word to be replaced
		var wordToSearchUpper = null;
		var substWord = null;    // The word to substitute
		// This tests numTxtReplacements < 9999 so that the 9999th one is the last
		// one read.
		while (!wordFile.eof && (numTxtReplacements < 9999))
		{
			// Read the next line from the file.
			fileLine = wordFile.readln(2048);

			// fileLine should be a string, but I've seen some cases
			// where for some reason it isn't.  If it's not a string,
			// then continue onto the next line.
			if (typeof(fileLine) != "string")
				continue;
			// If the line starts with with a semicolon (the comment
			// character) or is blank, then skip it.
			if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
				continue;

			// Look for an equals sign, and if found, separate the line
			// into the setting name (before the =) and the value (after the
			// equals sign).
			equalsPos = fileLine.indexOf("=");
			if (equalsPos <= 0)
				continue; // = not found or is at the beginning, so go on to the next line

			// Replace \1 with the SOH character so it can be used for
			// Synchronet color/attribute codes
			fileLine = fileLine.replace(/\\1/g, "\1");

			// Extract the word to search and substitution word from the line.  If
			// not using regular expressions, then convert the word to search to
			// all uppercase for case-insensitive searching.
			wordToSearch = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
			wordToSearchUpper = wordToSearch.toUpperCase();
			substWord = trimSpaces(fileLine.substr(equalsPos+1), true, false, true);
			// Make sure substWord only contains printable characters.  If not, then
			// skip this one.
			var substIsPrintable = true;
			for (var i = 0; (i < substWord.length) && substIsPrintable; ++i)
			{
				var currentChar = substWord.charAt(i);
				substIsPrintable = (isPrintableChar(currentChar) || (currentChar == "\1"));
			}
			if (!substIsPrintable)
				continue;

			// And add the search word and replacement text to pArray.
			if (wordToSearchUpper != substWord.toUpperCase())
			{
				if (pRegex)
					pArray[wordToSearch] = substWord;
				else
					pArray[wordToSearchUpper] = substWord;
				++numTxtReplacements;
			}
		}

		wordFile.close();
	}

	return numTxtReplacements;
}

function moveGenColorsToGenSettings(pColorsArray, pCfgObj)
{
   // Set up an array of color setting names
   var colorSettingStrings = new Array();
   colorSettingStrings.push("crossPostBorder"); // Deprecated
   colorSettingStrings.push("crossPostBorderText"); // Deprecated
   colorSettingStrings.push("listBoxBorder");
   colorSettingStrings.push("listBoxBorderText");
   colorSettingStrings.push("crossPostMsgAreaNum");
   colorSettingStrings.push("crossPostMsgAreaNumHighlight");
   colorSettingStrings.push("crossPostMsgAreaDesc");
   colorSettingStrings.push("crossPostMsgAreaDescHighlight");
   colorSettingStrings.push("crossPostChk");
   colorSettingStrings.push("crossPostChkHighlight");
   colorSettingStrings.push("crossPostMsgGrpMark");
   colorSettingStrings.push("crossPostMsgGrpMarkHighlight");
   colorSettingStrings.push("msgWillBePostedHdr");
   colorSettingStrings.push("msgPostedGrpHdr");
   colorSettingStrings.push("msgPostedSubBoardName");
   colorSettingStrings.push("msgPostedOriginalAreaText");
   colorSettingStrings.push("msgHasBeenSavedText");
   colorSettingStrings.push("msgAbortedText");
   colorSettingStrings.push("emptyMsgNotSentText");
   colorSettingStrings.push("genMsgErrorText");
   colorSettingStrings.push("listBoxItemText");
   colorSettingStrings.push("listBoxItemHighlight");

   var colorName = "";
   for (var i = 0; i < colorSettingStrings.length; ++i)
   {
      colorName = colorSettingStrings[i];
      if (pColorsArray.hasOwnProperty(colorName))
      {
         pCfgObj.genColors[colorName] = pColorsArray[colorName];
         delete pColorsArray[colorName];
      }
   }
   // If listBoxBorder and listBoxBorderText exist in the general colors settings,
   // then remove crossPostBorder and crossPostBorderText if they exist.
   if (pCfgObj.genColors.hasOwnProperty["listBoxBorder"] && pCfgObj.genColors.hasOwnProperty["crossPostBorder"])
   {
      // Favor crossPostBorder to preserve backwards compatibility.
      pCfgObj.genColors["listBoxBorder"] = pCfgObj.genColors["crossPostBorder"];
      delete pCfgObj.genColors["crossPostBorder"];
   }
   if (pCfgObj.genColors.hasOwnProperty["listBoxBorderText"] && pCfgObj.genColors.hasOwnProperty["crossPostBorderText"])
   {
      // Favor crossPostBorderText to preserve backwards compatibility.
      pCfgObj.genColors["listBoxBorderText"] = pCfgObj.genColors["crossPostBorderText"];
      delete pCfgObj.genColors["crossPostBorderText"];
   }
}

// Returns whether or not a character is a letter.
//
// Parameters:
//  pChar: The character to test
//
// Return value: Boolean - Whether or not the character is a letter
function charIsLetter(pChar)
{
   return /^[ABCDEFGHIJKLMNOPQRSTUVWXYZÀÈÌÒÙàèìòùÁÉÍÓÚÝáéíóúýÂÊÎÔÛâêîôûÃÑÕãñõÄËÏÖÜäëïöüçÇßØøÅåÆæÞþÐð]$/.test(pChar.toUpperCase());
}

// For configuration files, this function returns a fully-pathed filename.
// This function first checks to see if the file exists in the sbbs/mods
// directory, then the sbbs/ctrl directory, and if the file is not found there,
// this function defaults to the given default path.
//
// Parameters:
//  pFilename: The name of the file to look for
//  pDefaultPath: The default directory (must have a trailing separator character)
function genFullPathCfgFilename(pFilename, pDefaultPath)
{
   var fullyPathedFilename = system.mods_dir + pFilename;
   if (!file_exists(fullyPathedFilename))
      fullyPathedFilename = system.ctrl_dir + pFilename;
   if (!file_exists(fullyPathedFilename))
   {
      if (typeof(pDefaultPath) == "string")
      {
         // Make sure the default path has a trailing path separator
         var defaultPath = backslash(pDefaultPath);
         fullyPathedFilename = defaultPath + pFilename;
      }
      else
         fullyPathedFilename = pFilename;
   }
   return fullyPathedFilename;
}

// Returns the first letter found in a string and its index.  If a letter is
// not found, the string returned will be blank, and the index will be -1.
//
// Parameters:
//  pString: The string to search
//
// Return value: An object with the following properties:
//               letter: The first letter found in the string, or a blank string if none was found
//               idx: The index of the first letter found, or -1 if none was found
function getFirstLetterFromStr(pString)
{
   var retObj = new Object;
   retObj.letter = "";
   retObj.idx = -1;

   var theChar = "";
   for (var i = 0; (i < pString.length) && (retObj.idx == -1); ++i)
   {
      theChar = pString.charAt(i);
      if (charIsLetter(theChar))
      {
         retObj.idx = i;
         retObj.letter = theChar;
      }
   }

   return retObj;
}

// Returns whether or not the first letter in a string is uppercase.  If the
// string doesn't contain any letters, then this function will return false.
//
// Parameters:
//  pString: The string to search
//
// Return value: Boolean - Whether or not the first letter in the string is uppercase
function firstLetterIsUppercase(pString)
{
   var firstIsUpper = false;
   var letterObj = getFirstLetterFromStr(pString);
   if (letterObj.idx > -1)
   {
      var theLetter = pString.charAt(letterObj.idx);
      firstIsUpper = (theLetter == theLetter.toUpperCase());
   }
   return firstIsUpper;
}

// Gets a keypress from the user.  Uses the configured timeout if configured to
// do so and the user is not a sysop; otherwise (no timeout configured or the
// user is a sysop), the configured input timeout will be used.
//
// Parameters:
//  pMode: The input mode flag(s)
//  pCfgObj: The configuration object (stores the input timeout setting)
//
// Return value: The user's keypress (the return value of console.getkey()
//               or console.inkey()).
function getUserKey(pMode, pCfgObj)
{
   var defaultTimeoutMS = 300000;
   var userKey = "";

   if (typeof(pCfgObj) == "object")
   {
      // If the user is a sysop, don't use an input timeout.
      if ((typeof(pCfgObj.userIsSysop) == "boolean") && pCfgObj.userIsSysop)
         userKey = console.getkey(pMode);
      else if (typeof(pCfgObj.userInputTimeout) == "number")
         userKey = console.inkey(pMode, pCfgObj.inputTimeoutMS);
      else
         userKey = console.inkey(pMode, defaultTimeoutMS);
   }
   else if (typeof(pCfgObj) == "boolean")
   {
      // pCfgObj is a boolean that specifies whether or not the user is a sysop.
      // If so, then use console.getkey().  If the user isn't a sysop, use a
      // timeout of 5 minutes.
      if (pCfgObj)
         userKey = console.getkey(pMode);
      else
         userKey = console.inkey(pMode, defaultTimeoutMS);
   }
   else // pCfgObj is not a known type, so use the default input timeout.
      userKey = console.inkey(pMode, defaultTimeoutMS);

   return userKey;
}

// Gets a string of user input such that each character is validated against a
// set of strings.  Each string contains a list of valid characters, and each
// set of potential valid characters can only appear once in the user input.
// This function was written to be used in color selection in lieu of
// console.getkeys(), which outputs a carriage return at the end, which was not
// desirable.  For instance, color selection involves prompting the user for
// 3 characters (foreground, background, and special).
//
// Parameters:
//  pMode: The input mode bit(s).  See K_* in sbbsdefs.js for mode bits.
//  pValidKeyStrs: An array of strings containing valid keys
//  pCfgObj: The SlyEdit configuration settings object
//  pCurPos: Optional.  Contains x and y coordinates specifying the current
//           cursor position.  If this is not specified, this function will
//           get the cursor position by calling console.getxy().
//
// Return value: The user's input, as a string.  If nothing is input, this
//               function will return an empty string.
function getUserInputWithSetOfInputStrs(pMode, pValidKeyStrs, pCfgObj, pCurPos)
{
   if (pValidKeyStrs == null)
      return "";
   if (pValidKeyStrs.length == 0)
      return "";

   // Get the current cursor position, either from pCurPos or console.getxy().
   var curPos = (pCurPos != null ? pCurPos : console.getxy());

   // Build one string containing all the valid keys.
   var allValidKeys = "";
   for (var i = 0; i < pValidKeyStrs.length; ++i)
      allValidKeys += pValidKeyStrs[i];

   // User input loop
   var displayChars = !((pMode & K_NOECHO) == K_NOECHO);
   var userInput = "";
   var inputKey = "";
   var lastKey = "";
   var validKey = false;
   var idx = 0;
   var continueOn = true;
   while (continueOn)
   {
      inputKey = getUserKey(pMode|K_NOECHO, pCfgObj);
      // If userInput is blank, then the timeout was probably reached, so don't
      // continue inputting characters.
      if (inputKey.length == 0)
         break;

      switch (inputKey)
      {
         case BACKSPACE:
            // See if lastKey is in any of the strings in pValidKeyStrs.  If so,
            // then append that string back onto allValidKeys.
            for (var i = 0; i < pValidKeyStrs.length; ++i)
            {
               if (pValidKeyStrs[i].indexOf(lastKey) > -1)
               {
                  allValidKeys += pValidKeyStrs[i];
                  break;
               }
            }

            // If userInput has some characters in it, then remove the last
            // one and move the cursor back one space on the screen.
            if (userInput.length > 0)
            {
               userInput = userInput.substr(0, userInput.length-1);
               // If we are to display the input characters, then also blank out
               // the character on the screen and make sure the cursor is placed
               // properly on the screen.
               if (displayChars)
               {
                  console.gotoxy(--curPos.x, curPos.y);
                  console.print(" ");
                  console.gotoxy(curPos);
               }
            }
            break;
         // ESC and Ctrl-K: Cancel out of color selection, whereas
         // ENTER will save the user's input before returning.
         case KEY_ESC:
         case CTRL_K:
            userInput = "";
         case KEY_ENTER:
            continueOn = false;
            break;
         default:
            validKey = (allValidKeys.indexOf(inputKey) > -1);
            if (validKey)
            {
               // Find the key in one of the strings in pValidKeyStrs.  When
               // found, remove that string from allValidKeys.
               for (var i = 0; i < pValidKeyStrs.length; ++i)
               {
                  validKey = (pValidKeyStrs[i].indexOf(inputKey) > -1);
                  if (validKey)
                  {
                     // Remove the current string from allValidKeys
                     idx = allValidKeys.indexOf(pValidKeyStrs[i]);
                     if (idx > -1)
                     {
                        allValidKeys = allValidKeys.substr(0, idx)
                                     + allValidKeys.substr(idx + pValidKeyStrs[i].length);
                     }

                     break;
                  }
               }
            }

            // If the user pressed a valid key (found in the input strings), then
            // append it to userInput.
            if (validKey)
            {
               // If K_NOECHO wasn't passed in pMode, then output the keypress
               if (displayChars)
               {
                  console.print(inputKey);
                  ++curPos.x;
               }
               userInput += inputKey;
            }
            break;
      }

      // Update lastKey.  Default to the last keypress, but if there is anything
      // in userInput, then set lastKey to the last character in userInput.
      lastKey = inputKey;
      if (userInput.length > 0)
         lastKey = userInput.substr(userInput.length-1);
   }
   return userInput;
}

// Reads a text file and returns an array of strings containing the lines from
// the text file.
//
// Parameters:
//  pFilename: The name of the file to read
//  pStripCtrl: Boolean - Whether or not to strip control characters from the lines
//  pIgnoreBlankLines: Boolean - Whether or not to ignore blank lines
//  pMaxNumLines: Optional - The maximum number of lines to read from the file
//
// Return value: An array of strings read from the file
function readTxtFileIntoArray(pFilename, pStripCtrl, pIgnoreBlankLines, pMaxNumLines)
{
   var fileStrs = new Array();

   var maxNumLines = -1;
   if (typeof(pMaxNumLines) == "number")
   {
      if (pMaxNumLines > -1)
         maxNumLines = pMaxNumLines;
   }
   if (maxNumLines == 0)
      return fileStrs;

   var txtFile = new File(pFilename);
   if (txtFile.open("r"))
   {
      var fileLine = null;  // A line read from the file
      var numLinesRead = 0;
      while (!txtFile.eof)
      {
         // Read the next line from the config file.
         fileLine = txtFile.readln(2048);

         // fileLine should be a string, but I've seen some cases
         // where for some reason it isn't.  If it's not a string,
         // then continue onto the next line.
         if (typeof(fileLine) != "string")
            continue;

         if (pStripCtrl)
            fileLine = strip_ctrl(fileLine);

         if (pIgnoreBlankLines && (fileLine.length == 0))
            continue;

         fileStrs.push(fileLine);

         ++numLinesRead;
         if ((maxNumLines > 0) && (numLinesRead >= maxNumLines))
            break;
      }

      txtFile.close();
   }

   return fileStrs;
}

// Returns whether or not a text file contains lines in it.
//
// Parameters:
//  pFilename: The name of the file to test
//
// Return value: Boolean - Whether or not the file contains lines
function txtFileContainsLines(pFilename)
{
   var fileContainsLines = false;

   var txtFile = new File(pFilename);
   if (txtFile.open("r"))
   {
      var fileLine = null;  // A line read from the file
      var numLinesRead = 0;
      while (!txtFile.eof && (numLinesRead == 0))
      {
         // Read the next line from the config file.
         fileLine = txtFile.readln(2048);

         // fileLine should be a string, but I've seen some cases
         // where for some reason it isn't.  If it's not a string,
         // then continue onto the next line.
         if (typeof(fileLine) != "string")
            continue;
         ++numLinesRead;
      }
      fileContainsLines = (numLinesRead > 0);

      txtFile.close();
   }

   return fileContainsLines;
}

// Reads the user settings file and returns an object containing the user's
// settings.
//
// Parameters:
//  pSlyEdCfgObj: The SlyEdit configuration object, which contains defaults
//                for some of the user settings.  This settings object should
//                be populated before this function is called.
//
// Return value: An object containing the user's settings as properties.
function ReadUserSettingsFile(pSlyEdCfgObj)
{
	// Initialize the settings object with the default settings
	var userSettingsObj = {
		enableTaglines: pSlyEdCfgObj.enableTaglines,
		useQuoteLineInitials: pSlyEdCfgObj.useQuoteLineInitials,
		// The next setting specifies whether or not quote lines should be
		// prefixed with a space when using author initials.
		indentQuoteLinesWithInitials: pSlyEdCfgObj.indentQuoteLinesWithInitials,
		// Whether or not to trim spaces from quoted lines
		trimSpacesFromQuoteLines: true,
		autoSignMessages: false,
		autoSignRealNameOnlyFirst: true,
		autoSignEmailsRealName: true
	};

	// Open the user settings file
	var userSettingsFile = new File(gUserSettingsFilename);
	if (userSettingsFile.open("r"))
	{
		var settingsMode = "behavior";
		var fileLine = null;     // A line read from the file
		var equalsPos = 0;       // Position of a = in the line
		var commentPos = 0;      // Position of the start of a comment
		var setting = null;      // A setting name (string)
		var settingUpper = null; // Upper-case setting name
		var value = null;        // A value for a setting (string)
		var valueUpper = null;   // Upper-cased value
		while (!userSettingsFile.eof)
		{
			// Read the next line from the config file.
			fileLine = userSettingsFile.readln(2048);

			// fileLine should be a string, but I've seen some cases
			// where for some reason it isn't.  If it's not a string,
			// then continue onto the next line.
			if (typeof(fileLine) != "string")
				continue;

			// If the line starts with with a semicolon (the comment
			// character) or is blank, then skip it.
			if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
				continue;

			// If in the "behavior" section, then set the behavior-related variables.
			if (fileLine.toUpperCase() == "[BEHAVIOR]")
			{
				settingsMode = "behavior";
				continue;
			}

			// If the line has a semicolon anywhere in it, then remove
			// everything from the semicolon onward.
			commentPos = fileLine.indexOf(";");
			if (commentPos > -1)
				fileLine = fileLine.substr(0, commentPos);

			// Look for an equals sign, and if found, separate the line
			// into the setting name (before the =) and the value (after the
			// equals sign).
			equalsPos = fileLine.indexOf("=");
			if (equalsPos > 0)
			{
				// Read the setting & value, and trim leading & trailing spaces.
				setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
				settingUpper = setting.toUpperCase();
				value = trimSpaces(fileLine.substr(equalsPos+1), true, false, true);
				valueUpper = value.toUpperCase();

				if (settingsMode == "behavior")
				{
					if (settingUpper == "ENABLETAGLINES")
						userSettingsObj.enableTaglines = (valueUpper == "TRUE");
					else if (settingUpper == "USEQUOTELINEINITIALS")
						userSettingsObj.useQuoteLineInitials = (valueUpper == "TRUE");
					else if (settingUpper == "INDENTQUOTELINESWITHINITIALS")
						userSettingsObj.indentQuoteLinesWithInitials = (valueUpper == "TRUE");
					else if (settingUpper == "TRIMSPACESFROMQUOTELINES")
						userSettingsObj.trimSpacesFromQuoteLines = (valueUpper == "TRUE");
					else if (settingUpper == "AUTOSIGNMESSAGES")
						userSettingsObj.autoSignMessages = (valueUpper == "TRUE");
					else if (settingUpper == "AUTOSIGNREALNAMEONLYFIRST")
						userSettingsObj.autoSignRealNameOnlyFirst = (valueUpper == "TRUE");
					else if (settingUpper == "AUTOSIGNEMAILSREALNAME")
						userSettingsObj.autoSignEmailsRealName = (valueUpper == "TRUE");
				}
			}
		}
		userSettingsFile.close();
	}
	else
	{
		// We couldn't read the user settings file - So this is probably the
		// first time the user has run SlyEdit.  So, save the settings to the
		// file.
		var saveSucceeded = WriteUserSettingsFile(userSettingsObj);
	}

	return userSettingsObj;
}

// Writes the user settings to the user settings file, overwriting the
// existing file.
//
// Parameters:
//  pUserSettingsObj: The object containing the user's settings
//
// Return value: Boolean - Whether or not this function succeeded in writing the file.
function WriteUserSettingsFile(pUserSettingsObj)
{
	var writeSucceeded = false;

	var userSettingsFile = new File(gUserSettingsFilename);
	if (userSettingsFile.open("w"))
	{
		const behaviorBoolSettingNames = ["enableTaglines",
		                                  "useQuoteLineInitials",
		                                  "indentQuoteLinesWithInitials",
		                                  "trimSpacesFromQuoteLines",
		                                  "autoSignMessages",
		                                  "autoSignRealNameOnlyFirst",
		                                  "autoSignEmailsRealName"];
		userSettingsFile.writeln("[BEHAVIOR]");
		for (var i = 0; i < behaviorBoolSettingNames.length; ++i)
		{
			if (pUserSettingsObj.hasOwnProperty(behaviorBoolSettingNames[i]))
				userSettingsFile.writeln(behaviorBoolSettingNames[i] + "=" + (pUserSettingsObj[behaviorBoolSettingNames[i]] ? "true" : "false"));
		}

		userSettingsFile.close();
		writeSucceeded = true;
	}

	return writeSucceeded;
}

// Changes a character in a string, and returns the new string.  If any of the
// parameters are invalid, then the original string will be returned.
//
// Parameters:
//  pStr: The original string
//  pCharIndex: The index of the character to replace
//  pNewText: The new character or text to place at that position in the string
//
// Return value: The new string
function chgCharInStr(pStr, pCharIndex, pNewText)
{
   if (typeof(pStr) != "string")
      return "";
   if ((pCharIndex < 0) || (pCharIndex >= pStr.length))
      return pStr;
   if (typeof(pNewText) != "string")
      return pStr;

   return (pStr.substr(0, pCharIndex) + pNewText + pStr.substr(pCharIndex+1));
}

// Shuffles (randomizes) the contents of an array and returns the new
// array.  This function came from the following web page:
// http://stackoverflow.com/questions/6274339/how-can-i-shuffle-an-array-in-javascript
//
// Parameters:
//  pArray: The array to shuffle
//
// Return value: The new array
function shuffleArray(pArray)
{
    var counter = pArray.length, temp, index;

    // While there are elements in the pArray
    while (counter--)
    {
        // Pick a random index
        index = (Math.random() * (counter + 1)) | 0;

        // And swap the last element with it
        temp = pArray[counter];
        pArray[counter] = pArray[index];
        pArray[index] = temp;
    }

    return pArray;
}

// Performs the same function as console.pause(), but also allows input of multi-key
// sequences such as PageUp, PageDown, F1, etc. without writing extra characters on
// the screen.
//
// Parameters:
//  pCfgObj: Optional - The configuration object, which specifies the input timeout.
function consolePauseWithESCChars(pCfgObj)
{
	// Get the pause prompt text from text.dat.  In case that text contains
	// "@EXEC:" (to execute a script), default to a "press a key" message.
	var pausePromptText = bbs.text(Pause); // 563: The "Press a key" text in text.dat
	if (pausePromptText.toUpperCase().indexOf("@EXEC:") > -1)
		pausePromptText = "\1n\1c[ Press a key ] ";
	console.print("\1n" + pausePromptText);
	getKeyWithESCChars(K_NOSPIN|K_NOCRLF|K_NOECHO, pCfgObj);
}

// Inputs a keypress from the user and handles some ESC-based
// characters such as PageUp, PageDown, and ESC.  If PageUp
// or PageDown are pressed, this function will return the
// string defined by KEY_PAGE_UP or EY_PAGE_DOWN,
// respectively.  Also, F1-F5 will be returned as "\1F1"
// through "\1F5", respectively.
// Thanks goes to Psi-Jack for the original impementation
// of this function.
//
// Parameters:
//  pGetKeyMode: Optional - The mode bits for console.getkey().
//               If not specified, K_NONE will be used.
//  pCfgObj: The configuration object (stores the input timeout setting)
//
// Return value: The user's keypress
function getKeyWithESCChars(pGetKeyMode, pCfgObj)
{
	var getKeyMode = (typeof(pGetKeyMode) == "number" ? pGetKeyMode : K_NONE);
	var userInput = getUserKey(getKeyMode, pCfgObj);
	if (userInput == KEY_ESC)
	{
		switch (console.inkey(K_NOECHO|K_NOSPIN, 2))
		{
			case '[':
				switch (console.inkey(K_NOECHO|K_NOSPIN, 2))
				{
					case 'V':
						userInput = KEY_PAGE_UP;
						break;
					case 'U':
						userInput = KEY_PAGE_DOWN;
						break;
				}
				break;
			case 'O':
				switch (console.inkey(K_NOECHO|K_NOSPIN, 2))
				{
					case 'P':
						userInput = KEY_F1;
						break;
					case 'Q':
						userInput = KEY_F2;
						break;
					case 'R':
						userInput = KEY_F3;
						break;
					case 'S':
						userInput = KEY_F4;
						break;
					case 't':
						userInput = KEY_F5;
						break;
				}
			default:
				break;
		}
	}

	return userInput;
}

// Returns the index of the first Synchronet attribute code before a given index
// in a string.
//
// Parameters:
//  pStr: The string to search in
//  pIdx: The index to search back from
//  pSeriesOfAttrs: Optional boolean - Whether or not to look for a series of
//                  attributes.  Defaults to false (look for just one attribute).
//  pOnlyInWord: Optional boolean - Whether or not to look only in the current word
//               (with words separated by whitespace).  Defaults to false.
//
// Return value: The index of the first Synchronet attribute code before the given
//               index in the string, or -1 if there is none or if the parameters
//               are invalid
function strIdxOfSyncAttrBefore(pStr, pIdx, pSeriesOfAttrs, pOnlyInWord)
{
	if (typeof(pStr) != "string")
		return -1;
	if (typeof(pIdx) != "number")
		return -1;
	if ((pIdx < 0) || (pIdx >= pStr.length))
		return -1;

	var seriesOfAttrs = (typeof(pSeriesOfAttrs) == "boolean" ? pSeriesOfAttrs : false);
	var onlyInWord = (typeof(pOnlyInWord) == "boolean" ? pOnlyInWord : false);

	var attrCodeIdx = pStr.lastIndexOf("\1", pIdx-1);
	if (attrCodeIdx > -1)
	{
		// If we are to only check the current word, then continue only if
		// there isn't a space between the attribute code and the given index.
		if (onlyInWord)
		{
			if (pStr.lastIndexOf(" ", pIdx-1) >= attrCodeIdx)
				attrCodeIdx = -1;
		}
	}
	if (attrCodeIdx > -1)
	{
		var syncAttrRegexWholeWord = /^\1[krgybmcw01234567hinpq,;\.dtl<>\[\]asz]$/i;
		if (syncAttrRegexWholeWord.test(pStr.substr(attrCodeIdx, 2)))
		{
			if (seriesOfAttrs)
			{
				for (var i = attrCodeIdx - 2; i >= 0; i -= 2)
				{
					if (syncAttrRegexWholeWord.test(pStr.substr(i, 2)))
						attrCodeIdx = i;
					else
						break;
				}
			}
		}
		else
			attrCodeIdx = -1;
	}
	return attrCodeIdx;
}

// Gets a substring for a string, including preceding Synchronet color codes
// for the word at the given start index.
//
// Parameters:
//  pStr: The string to get the substring from
//  pStartIdx: The index in the string where the substring should start
//  pLen: The length of the substring to get.  Optional - If not specified,
//        the rest of the string will be used.
//
// Return value: An object with the following properties:
//               strSub: The substring from the string, including any Synchronet
//                       color codes that may exist preceding the word at pStartIdx
//               startIdx: The actual index in the string where the substring starts
//                         (-1 if an error occurred)
//               endIdx: The actual index in the string where the substring ends.
//                       This is the index of the last character in the substring.
//                       Will be -1 if an error occurred.
//               len: The actual length of the substring
//               printableLen: The length of the string as it would appear on the screen
//                             (i.e., its length without Synchronet attribute codes)
//               syncAttrStartIdx: The starting index of Synchronet attributes
//                                 (-1 if not found or an error occurred)
//               syncAttrEndIdx: The ending index of Synchronet attributes, including
//                               the last character (-1 if not found or an error occurred)
function substrWithSyncColorCodes(pStr, pStartIdx, pLen)
{
	var retObj = {
		strSub: "",
		startIdx: -1,
		endIdx: -1,
		len: 0,
		printableLen: 0,
		syncAttrStartIdx: -1,
		syncAttrEndIdx: -1,
		clear: function() {
			this.strSub = "";
			this.startIdx = -1;
			this.endIdx = -1;
			this.len = 0;
			this.printableLen = 0;
			this.syncAttrStartIdx = -1;
			this.syncAttrEndIdx = -1;
		}
	};

	if (typeof(pStr) != "string")
		return retObj;
	if (typeof(pStartIdx) != "number")
		return retObj;
	if ((pStartIdx < 0) || (pStartIdx >= pStr.length))
		return retObj;

	var substrLen = (typeof(pLen) == "number" ? pLen : pStr.length-pStartIdx);
	if (substrLen > pStr.length)
		substrLen = pStr.length - pStartIdx + 1;

	// If there are Synchronet attribute codes at the given index, then find out
	// where the attribute codes end and extend the length to include those with
	// the printable text.
	retObj.startIdx = pStartIdx;
	retObj.len = substrLen;
	if (pStr.substr(pStartIdx).search(gSyncAttrRegex) == 0)
	{
		retObj.syncAttrStartIdx = pStartIdx;
		retObj.syncAttrEndIdx = regexLastIndexOf(pStr, gSyncAttrRegex, pStartIdx, pStartIdx+substrLen);
		if (retObj.syncAttrEndIdx > -1)
		{
			++retObj.syncAttrEndIdx; // This index should be the last character in the Sync attributes
			retObj.len += (retObj.syncAttrEndIdx - pStartIdx + 1);
		}
	}
	else
	{
		var attrInfo = getAttrsAndIndexesBeforeStrIdx(pStr, pStartIdx);
		retObj.syncAttrStartIdx = attrInfo.syncAttrStartIdx;
		retObj.syncAttrEndIdx = attrInfo.syncAttrEndIdx;
		if ((retObj.syncAttrStartIdx > -1) && (retObj.syncAttrEndIdx > -1))
		{
			// Map the given start index to the actual index in the string
			retObj.startIdx = strDisplayIdxToActualIdx(pStr, pStartIdx);
			// Older code which has a bug - Doesn't map a 'on-screen' string
			// index to an actual string index correctly if an attribute code
			// happens to appear in the string right before pStartIdx:
			/*
			if (retObj.syncAttrEndIdx == pStartIdx-1)
			{
				var lenDiff = retObj.syncAttrEndIdx - retObj.syncAttrStartIdx + 1;
				retObj.startIdx -= lenDiff;
				retObj.len += lenDiff;
			}
			else
				retObj.startIdx = strDisplayIdxToActualIdx(pStr, pStartIdx);
			*/
		}
		else
		{
			// Found no attribute codes before the start index.
			// See if there are any Synchronet attribute codes between the
			// start index & within the length.  If so, then adjust the length
			// accordingly: Add Synchronet attribute code lengths to the substring
			// length to account for them in the string.
			var endIdx = pStartIdx + substrLen;
			var i = pStartIdx;
			while (i < endIdx)
			{
				if (gSyncAttrRegex.test(pStr.substr(i, 2)))
				{
					i += 2;
					retObj.len += 2;
				}
				else
					++i;
			}
		}
	}
	retObj.endIdx = retObj.startIdx + retObj.len - 1;
	retObj.strSub = pStr.substr(retObj.startIdx, retObj.len);
	retObj.printableLen = strip_ctrl(retObj.strSub).length;
	return retObj;
}

// Returns the index of the last-occurring instance of a given regular expression
// in a string.
//
// Parameters:
//  pStr: The string to look in
//  pRegex: The regular expression to look for
//  pStartIdx: Optional - The index of the starting character in the string
//  pEndIdx: Optional - The index of the last character in the string to check
//
// Return value: The index of the last-occuring instance of the given regular
//               expression, or -1 if not found.
function regexLastIndexOf(pStr, pRegex, pStartIdx, pEndIdx)
{
	if (typeof(pStr) != "string")
		return -1;
	var startIdx = (typeof(pEndIdx) == "number" ? pStartIdx : 0);
	if ((startIdx < 0) || (startIdx >= pStr.length))
		return -1;
	var endIdx = (typeof(pEndIdx) == "number" ? pEndIdx : pStr.length-1);
	if ((endIdx < 0) || (endIdx >= pStr.length))
		return -1;

	var lastIdx = -1;
	for (var i = endIdx; (i >= startIdx) && (lastIdx == -1); --i)
	{
		if (pStr.substr(i).search(pRegex) == 0)
			lastIdx = i;
	}
	return lastIdx;
}

// Finds the index of the next occurrance of a regular expression in
// a string.
//
// Parameters:
//  pStr: The string to search
//  pRegex: The regular expression to search for
//  pStartIdx: Optional - The index of where to start in the string.
//             If not specified, this value will be 0.
//
// Return value: The index of the next occurance of the regular expression, or
//               -1 if not found
function strSearchNext(pStr, pRegex, pStartIdx)
{
	if (typeof(pStr) != "string")
		return -1;
	var startIdx = (typeof(pStartIdx) == "number" ? pStartIdx : 0);
	if ((startIdx < 0) || (startIdx >= pStr.length))
		return -1;

	var subStr = (startIdx == 0 ? pStr : pStr.substr(startIdx));
	var searchIdx = subStr.search(pRegex);
	if (searchIdx > -1)
		searchIdx += startIdx;
	return searchIdx;
}

// Returns a string with any Synchronet color/attribute codes found in a string
// before a given index.
//
// Parameters:
//  pStr: The string to search in
//  pIdx: The index in the string to search before
//
// Return value: A string containing any Synchronet attribute codes found before
//               the given index in the given string
function getAttrsBeforeStrIdx(pStr, pIdx)
{
	if (typeof(pStr) != "string")
		return "";
	if (typeof(pIdx) != "number")
		return "";
	if (pIdx < 0)
		return "";

	var idx = (pIdx < pStr.length ? pIdx : pStr.length-1);
	var attrStartIdx = strIdxOfSyncAttrBefore(pStr, idx, true, false);
	var attrEndIdx = strIdxOfSyncAttrBefore(pStr, idx, false, false); // Start of 2-character code
	var attrsStr = "";
	if ((attrStartIdx > -1) && (attrEndIdx > -1))
		attrsStr = pStr.substring(attrStartIdx, attrEndIdx+2);
	return attrsStr;
}

// Returns an object with information about any group of Synchronet color/attribute codes
// found in a string before a given index.
//
// Parameters:
//  pStr: The string to search in
//  pIdx: The index in the string to search before
//
// Return value: An object containing the following properties:
//               attrStr: A string containing any Synchronet attribute codes
//                        found before the given index in the given string.  If
//                        none are found, this string will be empty.
//               syncAttrStartIdx: The index of where the attribute codes start, or
//                                 -1 if none were found
//               syncAttrEndIdx: The index of the last character of the attribute
//                               codes, or -1 if none were found
function getAttrsAndIndexesBeforeStrIdx(pStr, pIdx)
{
	var retObj = {
		attrStr: "",
		syncAttrStartIdx: -1,
		syncAttrEndIdx: -1
	};

	if ((typeof(pStr) != "string") || (typeof(pIdx) != "number"))
		return retObj;
	if (pIdx < 0)
		return retObj;

	var idx = (pIdx < pStr.length ? pIdx : pStr.length-1);

	// Look for indexes of any Synchronet attribute codes before the given
	// index.  If there are some, then set the start index after the attribute
	// codes end.
	retObj.syncAttrStartIdx = strIdxOfSyncAttrBefore(pStr, idx, /*false*/true, false);
	if ((retObj.syncAttrStartIdx > -1) && (retObj.syncAttrStartIdx < idx))
	{
		retObj.syncAttrEndIdx = regexLastIndexOf(pStr, gSyncAttrRegex, retObj.syncAttrStartIdx, idx);
		if (retObj.syncAttrEndIdx == -1)
			retObj.syncAttrEndIdx = regexLastIndexOf(pStr, gSyncAttrRegex, retObj.syncAttrStartIdx, retObj.syncAttrStartIdx+substrLen);
		if (retObj.syncAttrEndIdx > -1)
		{
			++retObj.syncAttrEndIdx; // This index should be the last character in the Sync attributes
			retObj.attrStr = pStr.substring(retObj.syncAttrStartIdx, retObj.syncAttrEndIdx+1);
		}
	}

	if (retObj.attrStr.length == 0)
	{
		retObj.syncAttrStartIdx = -1;
		retObj.syncAttrEndIdx = -1;
	}

	return retObj;
}

// Converts a printable display index to an actual index of a string,
// ignoring Synchronet attribute codes in the string.
//
// Parameters:
//  pStr: The string to check
//  pDisplayIdx: The string index as displayed on the screen
//
// Return value: The index in the actual string.  0 on error
function strDisplayIdxToActualIdx(pStr, pDisplayIdx)
{
	if (typeof(pStr) != "string")
		return 0;
	if (typeof(pDisplayIdx) != "number")
		return 0;

	var actualIdx = 0;
	var strWithoutCtrlChars = strip_ctrl(pStr);
	// If the given index is one past the last actual index in the string
	// without control characters, then put actualIndex one past the last
	// index of the string.
	if (pDisplayIdx == strWithoutCtrlChars.length)
		actualIdx = pStr.length;
	else
	{
		var syncAttrIdx = pStr.search(gSyncAttrRegex);
		if (syncAttrIdx > -1)
		{
			// Count the printable characters, ignoring Synchronet attribute codes
			// I suspect there may be a more efficient way to do this..
			var numDisplayChars = pDisplayIdx + 1;
			var numDisplayableCharsSeen = 0;
			var i = 0;
			while ((i < pStr.length) && (numDisplayableCharsSeen < numDisplayChars))
			{
				if (gSyncAttrRegex.test(pStr.substr(i, 2)))
					i += 2;
				else
				{
					actualIdx = i;
					++numDisplayableCharsSeen;
					++i;
				}
			}
			// Edge case: If pDisplayIdx was one past the last index of the string
			// displayed on the screen, then increment actualIdx by 1 to fix off-by-one
			if ((actualIdx == pStr.length-1) && (pDisplayIdx == strWithoutCtrlChars.length))
				++actualIdx;
			else if (pDisplayIdx == strWithoutCtrlChars.length - 1)
				++actualIdx;
		}
		else
			actualIdx = pDisplayIdx;
	}

	return actualIdx;
}

///////////////////////////////////////////////////////////////////////////////////////
// Color/attribute code conversion functions

// Converts Synchronet attribute codes to ANSI
//
// Parameters:
//  pText: A string containing the text to convert
//
// Return value: The text with the color codes converted
function SyncAttrsToANSI(pText)
{
	// Attributes
	var txt = pText.replace(/n/gi, "[0m"); // All attributes off
	txt = txt.replace(/h/gi, "[1m"); // Bold on (use high intensity)
	txt = txt.replace(/i/gi, "[5m"); // Blink on
	// Foreground colors
	txt = txt.replace(/k/gi, "[30m"); // Black foreground
	txt = txt.replace(/r/gi, "[31m"); // Red foreground
	txt = txt.replace(/g/gi, "[32m"); // Green foreground
	txt = txt.replace(/y/gi, "[33m"); // Yellow foreground
	txt = txt.replace(/b/gi, "[34m"); // Blue foreground
	txt = txt.replace(/m/gi, "[35m"); // Magenta foreground
	txt = txt.replace(/c/gi, "[36m"); // Cyan foreground
	txt = txt.replace(/w/gi, "[37m"); // White foreground
	// Background colors
	txt = txt.replace(/0/g, "[40m"); // Black background
	txt = txt.replace(/1/g, "[41m"); // Red background
	txt = txt.replace(/2/g, "[42m"); // Green background
	txt = txt.replace(/3/g, "[43m"); // Yellow background
	txt = txt.replace(/4/g, "[44m"); // Blue background
	txt = txt.replace(/5/g, "[45m"); // Magenta background
	txt = txt.replace(/6/g, "[46m"); // Cyan background
	txt = txt.replace(/7/g, "[47m"); // White background
	return txt;
}

// Given some text, this converts ANSI color codes to Synchronet codes and
// removes unwanted ANSI codes (such as cursor movement codes, etc.).
//
// Parameters:
//  pText: A string to process
//
// Return value: A version of the string with Synchronet color codes converted to
//               Synchronet attribute codes and unwanted ANSI codes removed
function cvtANSIToSyncAndRemoveUnwantedANSI(pText)
{
	// Attributes
	var txt = pText.replace(/\[0[mM]/g, "\1n"); // All attributes off
	txt = txt.replace(/\[1m/gi, "\1h"); // Bold on (use high intensity)
	txt = txt.replace(/\[5m/gi, "\1i"); // Blink on
	// Foreground colors
	txt = txt.replace(/\[30m/gi, "\1k"); // Black foreground
	txt = txt.replace(/\[31m/gi, "\1r"); // Red foreground
	txt = txt.replace(/\[32m/gi, "\1g"); // Green foreground
	txt = txt.replace(/\[33m/gi, "\1y"); // Yellow foreground
	txt = txt.replace(/\[34m/gi, "\1b"); // Blue foreground
	txt = txt.replace(/\[35m/gi, "\1m"); // Magenta foreground
	txt = txt.replace(/\[36m/gi, "\1c"); // Cyan foreground
	txt = txt.replace(/\[37m/gi, "\1w"); // White foreground
	// Background colors
	txt = txt.replace(/\[40m/gi, "\1" + "0"); // Black background
	txt = txt.replace(/\[41m/gi, "\1" + "1"); // Red background
	txt = txt.replace(/\[42m/gi, "\1" + "2"); // Green background
	txt = txt.replace(/\[43m/gi, "\1" + "3"); // Yellow background
	txt = txt.replace(/\[44m/gi, "\1" + "4"); // Blue background
	txt = txt.replace(/\[45m/gi, "\1" + "5"); // Magenta background
	txt = txt.replace(/\[46m/gi, "\1" + "6"); // Cyan background
	txt = txt.replace(/\[47m/gi, "\1" + "7"); // White background
	// Convert ;-delimited modes (such as [Value;...;Valuem)
	txt = ANSIMultiConvertToSyncCodes(txt);
	// Remove ANSI codes that are not wanted (such as moving the cursor, etc.)
	txt = txt.replace(/\[[0-9]+a/gi, ""); // Cursor up
	txt = txt.replace(/\[[0-9]+b/gi, ""); // Cursor down
	txt = txt.replace(/\[[0-9]+c/gi, ""); // Cursor forward
	txt = txt.replace(/\[[0-9]+d/gi, ""); // Cursor backward
	txt = txt.replace(/\[[0-9]+;[0-9]+h/gi, ""); // Cursor position
	txt = txt.replace(/\[[0-9]+;[0-9]+f/gi, ""); // Cursor position
	txt = txt.replace(/\[s/gi, ""); // Restore cursor position
	txt = txt.replace(/\[2j/gi, ""); // Erase display
	txt = txt.replace(/\[k/gi, ""); // Erase line
	txt = txt.replace(/\[=[0-9]+h/gi, ""); // Set various screen modes
	txt = txt.replace(/\[=[0-9]+l/gi, ""); // Reset various screen modes
	return txt;
}

// Converts ANSI ;-delimited modes (such as [Value;...;Valuem) to Synchronet
// attribute codes
//
// Parameters:
//  pText: The text with ANSI ;-delimited modes to convert
//
// Return value: The text with ANSI ;-delimited modes converted to Synchronet attributes
function ANSIMultiConvertToSyncCodes(pText)
{
	var multiMatches = pText.match(/\[[0-9]+(;[0-9]+)+m/g);
	if (multiMatches == null)
		return pText;
	var updatedText = pText;
	for (var i = 0; i < multiMatches.length; ++i)
	{
		// Copy the string, with the [ removed from the beginning and the
		// trailing 'm' removed
		var text = multiMatches[i].substr(2);
		text = text.substr(0, text.length-1);
		var codes = text.split(";");
		var syncCodes = "";
		for (var idx = 0; idx < codes.length; ++idx)
		{
			if (codes[idx] == "0") // All attributes off
				syncCodes += "\1n";
			else if (codes[idx] == "1") // Bold on (high intensity)
				syncCodes += "\1h";
			else if (codes[idx] == "5") // Blink on
				syncCodes += "\1i";
			else if (codes[idx] == "30") // Black foreground
				syncCodes += "\1k";
			else if (codes[idx] == "31") // Red foreground
				syncCodes += "\1r";
			else if (codes[idx] == "32") // Green foreground
				syncCodes += "\1g";
			else if (codes[idx] == "33") // Yellow foreground
				syncCodes += "\1y";
			else if (codes[idx] == "34") // Blue foreground
				syncCodes += "\1b";
			else if (codes[idx] == "35") // Magenta foreground
				syncCodes += "\1m";
			else if (codes[idx] == "36") // Cyan foreground
				syncCodes += "\1c";
			else if (codes[idx] == "37") // White foreground
				syncCodes += "\1w";
			else if (codes[idx] == "40") // Black background
				syncCodes += "\1" + "0";
			else if (codes[idx] == "41") // Red background
				syncCodes += "\1" + "1";
			else if (codes[idx] == "42") // Green background
				syncCodes += "\1" + "2";
			else if (codes[idx] == "43") // Yellow background
				syncCodes += "\1" + "3";
			else if (codes[idx] == "44") // Blue background
				syncCodes += "\1" + "4";
			else if (codes[idx] == "45") // Magenta background
				syncCodes += "\1" + "5";
			else if (codes[idx] == "46") // Cyan background
				syncCodes += "\1" + "6";
			else if (codes[idx] == "47") // White background
				syncCodes += "\1" + "7";
		}
		updatedText = updatedText.replace(multiMatches[i], syncCodes);
	}
	return updatedText;
}

// Color/attribute code conversion functions end
///////////////////////////////////////////////////////////////////////////////////////

// Finds any Synchronet attributes in an array of TextLine objects before a given
// text line index and edit line index in the array of lines.
//
// Parameters:
//  pEditLines: An array of TextLine objects
//  pEditLineIdx: The starting edit line index in the array
//  pTextLineIdx: The ending text line index in the starting line
//
// Return value: A string containing any Synchronet attribute codes found in the
//               array of text lines before the given indexes
function findAttrCodesInLinesBeforeIdx(pEditLines, pEditLineIdx, pTextLineIdx)
{
	if ((pEditLineIdx < 0) || (pEditLineIdx >= pEditLines.length))
		return "";
	if (pTextLineIdx < 0)
		return "";
	// Let the text line index be one past the last character but no more than that
	var textLineIdx = (pTextLineIdx > pEditLines[pEditLineIdx].text.length ? pEditLines[pEditLineIdx].text.length : pTextLineIdx);

	var attrCodes = "";
	var attrCodeObj = getAttrsAndIndexesBeforeStrIdx(pEditLines[pEditLineIdx].text, textLineIdx-1);
	var startIdx = attrCodeObj.syncAttrStartIdx;
	while (startIdx > -1)
	{
		attrCodes = attrCodeObj.attrStr + attrCodes;
		attrCodeObj = getAttrsAndIndexesBeforeStrIdx(pEditLines[pEditLineIdx].text, startIdx);
		startIdx = attrCodeObj.syncAttrStartIdx;
	}
	for (var i = pEditLineIdx - 1; i >= 0; --i)
	{
		attrCodeObj = getAttrsAndIndexesBeforeStrIdx(pEditLines[i].text, pEditLines[i].text.length-1);
		startIdx = attrCodeObj.syncAttrStartIdx;
		while (startIdx > -1)
		{
			attrCodes = attrCodeObj.attrStr + attrCodes;
			attrCodeObj = getAttrsAndIndexesBeforeStrIdx(pEditLines[i].text, startIdx);
			startIdx = attrCodeObj.syncAttrStartIdx;
		}
	}
	// TODO: Remove redundancies in attrCodes?
	return attrCodes;
}

// Removes stray ASCII-1 characters from a string that aren't part of a Synchronet
// color code.
//
// Parameters:
//  pStr: The string to remove stray ASCII-1 codes from
//
// Return value: The string with the stray characters removed
function removeStrayANSIOneChars(pStr)
{
	var str = pStr;
	var idx = -1;
	var startIdx = 0;
	while ((idx = str.indexOf("\1", startIdx)) > -1)
	{
		if (!gSyncAttrRegex.test(str.substr(idx, 2)))
		{
			str = str.substr(0, idx) + str.substr(idx+1);
			startIdx = idx;
		}
		else
			startIdx = idx + 2;
	}
	return str;
}

///////////////////////////////////////////////////////////////////////////////////////
// Debugging

// This function displays debug text at a given location on the screen, then
// moves the cursor back to a given location.
//
// Parameters:
//  pDebugX: The X location of where to write the debug text
//  pDebugY: The Y location of where to write the debug text
//  pText: The text to write at the debug location
//  pOriginalPos: An object with x and y properties containing the original cursor position
//  pClearDebugLineFirst: Whether or not to clear the debug line before writing the text
//  pPauseAfter: Whether or not to pause after displaying the text
//  pClearLineAfter: Optional - Whether or not to clear the line after displaying the text
function displayDebugText(pDebugX, pDebugY, pText, pOriginalPos, pClearDebugLineFirst, pPauseAfter, pClearLineAfter)
{
	console.gotoxy(pDebugX, pDebugY);
	if (pClearDebugLineFirst)
	{
		//console.clearline("\1n");
		console.cleartoeol("\1n");
	}
	// Output the text
	console.print(pText);
	if (pPauseAfter)
		console.pause();
	var clearLineAfter = (typeof(pClearLineAfter) == "boolean" ? pClearLineAfter : false);
	if (clearLineAfter)
	{
		console.gotoxy(pDebugX, pDebugY);
		console.cleartoeol("\1n");
	}
	if ((typeof(pOriginalPos) != "undefined") && (pOriginalPos != null))
		console.gotoxy(pOriginalPos);
}
