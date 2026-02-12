/* This file declares some general helper functions and variables
 * that are used by SlyEdit.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 */

"use strict";
 
if (typeof(require) === "function")
{
	require("text.js", "Pause");
	require("key_defs.js", "CTRL_A");
	require("userdefs.js", "USER_ANSI");
	require("dd_lightbar_menu.js", "DDLightbarMenu");
	require("cp437_defs.js", "CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE");
	require("frame.js", "Frame");
	require("scrollbar.js", "ScrollBar");
}
else
{
	load("text.js");
	load("key_defs.js");
	load("userdefs.js");
	load("dd_lightbar_menu.js");
	load("cp437_defs.js");
	load("frame.js");
	load("scrollbar.js");
}

var COPYRIGHT_YEAR = 2026;


// Note: These variables are declared with "var" instead of "const" to avoid
// multiple declaration errors when this file is loaded more than once.

// Values for attribute types (for text attribute substitution)
var FORE_ATTR = 1; // Foreground color attribute
var BKG_ATTR = 2;  // Background color attribute
var SPECIAL_ATTR = 3; // Special attribute

// Navigational keys
var UP_ARROW = "\x18"; //ASCII 24
var DOWN_ARROW = "\x19"; //ASCII 25
// Some functional keys
var BACKSPACE = CTRL_H;
var TAB = CTRL_I;
var INSERT_LINE = CTRL_L;
var CR = CTRL_M;
var KEY_ENTER = CTRL_M;
var XOFF = CTRL_Q;
var XON = CTRL_S;
var KEY_INSERT = CTRL_V;

// ESC menu action codes to be returned
var ESC_MENU_SAVE = 0;
var ESC_MENU_ABORT = 1;
var ESC_MENU_INS_OVR_TOGGLE = 2;
var ESC_MENU_SYSOP_IMPORT_FILE = 3;
var ESC_MENU_SYSOP_EXPORT_FILE = 4;
var ESC_MENU_FIND_TEXT = 5;
var ESC_MENU_HELP_COMMAND_LIST = 6;
var ESC_MENU_HELP_GRAPHIC_CHAR = 7;
var ESC_MENU_HELP_PROGRAM_INFO = 8;
var ESC_MENU_EDIT_MESSAGE = 9;
var ESC_MENU_CROSS_POST_MESSAGE = 10;
var ESC_MENU_LIST_TEXT_REPLACEMENTS = 11;
var ESC_MENU_USER_SETTINGS = 12;
var ESC_MENU_SPELL_CHECK = 13;
var ESC_MENU_INSERT_MEME = 14;

var RIP_START_SEQ = "!|R"; // RIP start sequence

// Store the full path & filename of the Digital Distortion Message
// Lister, since it will be used more than once.
var gDDML_DROP_FILE_NAME = system.node_dir + "DDML_SyncSMBInfo.txt";

var gUserSettingsFilename = system.data_dir + "user/" + format("%04d", user.number) + ".SlyEdit_Settings";

// See if the user's terminal supports UTF-8 (USER_UTF8 is defined in userdefs.js)
var gUserConsoleSupportsUTF8 = (typeof(USER_UTF8) != "undefined" ? console.term_supports(USER_UTF8) : false);
// Since K_UTF8 (in sbbsdefs.js) was added in Synchronet 3.20, see if it exists
var g_K_UTF8Exists = (typeof(K_UTF8) === "number");
var gPrintMode = (gUserConsoleSupportsUTF8 ? P_UTF8 : P_NONE);

// Characterset supported by the editor in the editor configuration; to be read from the drop file in SlyEdit.js
var gConfiguredCharset = "";

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
	this.text = "";              // The line text
	this.hardNewlineEnd = false; // Whether or not the line has a hard newline at the end
	this.isQuoteLine = false;    // Whether or not this is a quote line
	// Copy the parameters if they are valid.
	if (typeof(pHardNewlineEnd) === "boolean")
		this.hardNewlineEnd = pHardNewlineEnd;
	if (typeof(pIsQuoteLine) === "boolean")
		this.isQuoteLine = pIsQuoteLine;

	// For color support
	this.attrs = {}; // An object where its properties are the line indexes, and values are attributes at that index in the line

	// Functions
	this.isAQuoteLine = TextLine_isAQuoteLine;
	this.numAttrs = TextLine_numAttrs;
	this.hasAttrs = TextLine_hasAttrs;
	this.getSortedAttrKeysArray = TextLine_getSortedAttrKeysArray;
	this.length = TextLine_length;
	this.screenLength = TextLine_screenLength;
	this.print = TextLine_print;
	this.substr = TextLine_substr;
	this.substring = TextLine_substring;
	this.setText = TextLine_setText;
	this.getText = TextLine_getText;
	this.insertIntoText = TextLine_insertIntoText;
	this.trimFront = TextLine_trimFront;
	this.trimEnd = TextLine_trimEnd;
	this.moveAttrIdxes = TextLine_moveAttrIdxes;
	this.doMacroTxtReplacement = TextLine_doMacroTxtReplacement;
	this.getWord = TextLine_getWord;
	this.removeCharAt = TextLine_removeCharAt;
	this.getAttrs = TextLine_getAttrs;
	this.popAttrsFromFront = TextLine_popAttrsFromFront;
	this.popAttrsFromEnd = TextLine_popAttrsFromEnd;

	// If pText is a string, then set the text, accounting for attribute codes
	if (typeof(pText) === "string")
		this.setText(pText);
}
// For the TextLine class: Returns whether the line a quote line.  This is true if
// the line's isQuoteLine property is true.  If the line's text is empty, then this
// will return false and set the line's isQuoteLine property to false.
function TextLine_isAQuoteLine()
{
	var lineIsQuoteLine = false;
	//lineIsQuoteLine = (this.isQuoteLine || (/^ *>/.test(this.text)));
	//lineIsQuoteLine = this.isQuoteLine;
	if (this.isQuoteLine)
	{
		if (this.screenLength() > 0)
			lineIsQuoteLine = true;
		else
			this.isQuoteLine = false;
	}
	return lineIsQuoteLine;
}
// For the TextLine class: Returns the number of attribute codes in the line
function TextLine_numAttrs()
{
	return (Object.keys(this.attrs)).length;
}
// For the TextLine class: Returns whether or not the line has any attribute codes specified
function TextLine_hasAttrs()
{
	return ((Object.keys(this.attrs)).length > 0);
}
// For the TextLine class: Returns a sorted array of the keys (numeric line indexes) from the attrs object
function TextLine_getSortedAttrKeysArray()
{
	var attrKeys = Object.keys(this.attrs);
	if (attrKeys.length > 0)
	{
		attrKeys.sort(textLineAttrSortCompareFunc);
		// Ensure the values in the attrKeys array are numeric
		for (var keysIdx = 0; keysIdx < attrKeys.length; ++keysIdx)
			attrKeys[keysIdx] = +(attrKeys[keysIdx]);
	}
	return attrKeys;
}
// Compare function for sorting TextLine attribute objects
function textLineAttrSortCompareFunc(a, b)
{
	var numA = +a;
	var numB = +b;
	if (numA < numB)
		return -1;
	else if (numA > numB)
		return 1;
	else
		return 0;
}
// For the TextLine class: Returns the length of the text.
function TextLine_length()
{
	return this.text.length;
}
// For the TextLine class: Returns the printed length of the text (without any attribute codes, etc.)
function TextLine_screenLength()
{
	// We don't translate UTF-8 to CP437..
	//str_is_utf8(text)
	//utf8_get_width(text)
	// If the user's terminal is UTF-8 capable count the text as UTF-8
	//var textMode = (gUserConsoleSupportsUTF8 ? P_UTF8 : P_NONE);
	var textMode = (g_K_UTF8Exists && str_is_utf8(this.text) ? P_UTF8 : P_NONE);
	return console.strlen(this.text, textMode);
}
// For the TextLine class: Prints the text line, using its text attributes.
//
// Parameters:
//  pPrintNormalAttrFirst: Boolean - Whether or not to print a normal attribute first. Defaults to false.
//  pMaxLength: The maximum length to print.  Optional.  If not specified or <= 0, the whole text will be printed.
//  pClearToEOL: Boolean - Whether or not to clear to the end of the line.  Defaults to false.
function TextLine_print(pPrintNormalAttrFirst, pMaxLength, pClearToEOL)
{
	var maxLengthIsNumber = (typeof(pMaxLength) === "number");
	if (maxLengthIsNumber && pMaxLength == 0)
		return;

	var printNormalAttrFirst = (typeof(pPrintNormalAttrFirst) === "boolean" ? pPrintNormalAttrFirst : false);
	if (printNormalAttrFirst)
		console.attributes = "N";

	var attrKeys = this.getSortedAttrKeysArray(); // The attribute keys are indexes for this.text
	if (attrKeys.length > 0)
	{
		var continueOn = true;
		var keysArrayEndIdx = attrKeys.length - 1;
		var substrStartIdx = 0;
		for (var keysIdx = 0; keysIdx <= keysArrayEndIdx && continueOn; ++keysIdx)
		{
			var textIdx = +(attrKeys[keysIdx]);
			if (textIdx > 0)
			{
				if (maxLengthIsNumber && textIdx >= pMaxLength-1)
				{
					textIdx = pMaxLength-1;
					continueOn = false;
				}
				printStrConsideringUTF8(this.text.substring(substrStartIdx, textIdx), gPrintMode);
			}
			//console.attributes = this.attrs[textIdx].replace(ascii(0x01), "");
			console.print(this.attrs[textIdx]);

			if (keysIdx < keysArrayEndIdx)
			{
				var substringEndIdx = +(attrKeys[keysIdx+1]);
				// TODO: Is this correct with substringEndIdx and pMaxLength?
				if (maxLengthIsNumber && substringEndIdx >= pMaxLength-1)
				{
					substringEndIdx = pMaxLength-1;
					continueOn = false;
				}
				printStrConsideringUTF8(this.text.substring(textIdx, substringEndIdx), gPrintMode);
				substrStartIdx = substringEndIdx;
			}
			else
				printStrConsideringUTF8(this.text.substring(textIdx), gPrintMode);
		}
	}
	else
	{
		if (maxLengthIsNumber)
			printStrConsideringUTF8(this.text.substr(0, pMaxLength), gPrintMode);
		else
			printStrConsideringUTF8(this.text, gPrintMode);
	}

	var clearToEOL = (typeof(pClearToEOL) === "boolean" ? pClearToEOL : false);
	if (clearToEOL)
		console.cleartoeol("\x01n");
}
// For the TextLine class: Returns a substring of the text string, with or without attribute codes.
//
// Parameters:
//  pWithAttrs: Boolean - Whether or not to include the attribute codes in the substring
//  pStart: The index of where to start in the string
//  pLen: The printed screen length of the substring
//
// Return value: The substring from the text line. If pWithAttrs is true, the substring will contain
//               the attribute codes set for the text line.
function TextLine_substr(pWithAttrs, pStart, pLen)
{
	var startIdx = (typeof(pStart) === "number" && pStart >= 0 && pStart < this.text.length ? pStart : 0);
	var length = (typeof(pLen) === "number" && pLen > 0 && pLen <= this.text.length ? pLen : this.text.length);
	// Sanity check
	var maxLength = this.text.length - startIdx;
	if (length > maxLength)
		length = maxLength;

	// Note: substrWithAttrCodes() is defined in dd_lightbar_menu.js
	var theSubstr = "";
	if (pWithAttrs)
		theSubstr = substrWithAttrCodes(this.getText(pWithAttrs), startIdx, length);
	else
		theSubstr = this.text.substr(startIdx, length);
	return theSubstr;
}
// For the TextLine class: Returns a substring of the text string, with or without attribute codes.
//
// Parameters:
//  pWithAttrs: Boolean - Whether or not to include the attribute codes in the substring
//  pStart: The starting index of the string for the screen-printable text
//  pEnd: One past the last index to be included in the substring for the screen-printable text
//
// Return value: The substring from the text line. If pWithAttrs is true, the substring will contain
//               the attribute codes set for the text line.
function TextLine_substring(pWithAttrs, pStart, pEnd)
{
	if (typeof(pStart) !== "number" || typeof(pEnd) !== "number")
		return "";
	if (pStart < 1 || pStart >= this.text.length || pEnd <= pStart)
		return "";

	var strLength = pEnd - pStart;
	return this.substr(pWithAttrs, pStart, strLength);
}

// For the TextLine class: Sets the line's text. Accounts for Synchronet color/attribute codes.
//
// Parameters:
//  pText: The text to set in the text line
function TextLine_setText(pText)
{
	if (typeof(pText) !== "string")
	{
		this.attrs = {};
		this.text = "";
		return;
	}

	var attrSepObj = sepStringAndAttrCodes(pText);
	this.text = attrSepObj.textWithoutAttrs;
	this.attrs = attrSepObj.attrs;
}

// For the TextLine class: Gets the entire text string
//
// Parameters:
//  pWithAttrs: Boolean - Whether or not to include the attribute codes in the substring. Defaults to true.
//
// Return value: The object's text string, possibly with its attribute codes as specified
function TextLine_getText(pWithAttrs)
{
	var thisText = "";
	var withAttrs = (typeof(pWithAttrs) === "boolean" ? pWithAttrs : true);
	var attrKeys = this.getSortedAttrKeysArray(); // The attribute keys are indexes for this.text
	if (withAttrs && attrKeys.length > 0)
	{
		var textIdx = +(attrKeys[0]);
		if (textIdx > 0)
			thisText += this.text.substring(0, textIdx);
		var substringEndIdx = 0; // For substrings
		var keysArrayEndIdx = attrKeys.length - 1;
		for (var keysIdx = 0; keysIdx <= keysArrayEndIdx; ++keysIdx)
		{
			textIdx = +(attrKeys[keysIdx]);
			thisText += this.attrs[textIdx];
			if (keysIdx < keysArrayEndIdx)
			{
				substringEndIdx = +(attrKeys[keysIdx+1]);
				thisText += this.text.substring(textIdx, substringEndIdx);
			}
			else
				thisText += this.text.substring(textIdx);
		}
	}
	else
		thisText = this.text;
	return thisText;
}

// For the TextLine class: Inserts a string inside the line's text string. Accounts for Synchronet
// color/attribute codes.
//
// Parameters:
//  pIndex: The index of this TextLine's text at which to insert the other string
//  pAdditionalText: The string to insert into this TextTine's text string
//  pInsertMiddleShiftAttrsStartAtIndexPlusOne: Optional boolean - When inserting text in the middle of the
//                                              line, whether to start shifting attribute codes to the right
//                                              at pIndex+1.  Defaults to true (start at pIndex+1).
function TextLine_insertIntoText(pIndex, pAdditionalText, pInsertMiddleShiftAttrsStartAtIndexPlusOne)
{
	// Error checking
	if (typeof(pIndex) !== "number" || pIndex >= this.text.length)
		return;
	if (typeof(pAdditionalText) !== "string" || console.strlen(pAdditionalText) == 0)
		return;

	var txtIdx = (pIndex < 0 ? 0 : pIndex);

	// Check for attribute codes in pAdditionalText so we can move them to this.attrs
	var attrSepObj = sepStringAndAttrCodes(pAdditionalText);

	// If txtIdx is beyond the last index of this.text, then just return the
	// two strings concatenated.
	if (txtIdx >= this.text.length)
	{
		this.text = this.text + attrSepObj.textWithoutAttrs;
		// Copy any attributes into this.attrs
		for (var txtIdx in attrSepObj.attrs)
		{
			var newTxtIdx = this.text.length + (+txtIdx);
			this.attrs[newTxtIdx] = attrSepObj.attrs[txtIdx];
		}
	}
	// If txtIdx is 0, then just use the additional text + this.text.
	else if (txtIdx == 0)
	{
		this.text = attrSepObj.textWithoutAttrs + this.text;
		// Move any attribute indexes in the line to the right after the additional text
		this.moveAttrIdxes(0, attrSepObj.textWithoutAttrs.length);
		// Copy any attributes into this.attrs
		for (var txtIdx in attrSepObj.attrs)
			this.attrs[+txtIdx] = attrSepObj.attrs[txtIdx];
	}
	else
	{
		this.text = this.text.substr(0, txtIdx) + attrSepObj.textWithoutAttrs + this.text.substr(txtIdx);
		// Move any text attributes to the right according to the length of the text spliced in, starting
		// at the given index
		var shiftStartingAtIndexPlusOne = (typeof(pInsertMiddleShiftAttrsStartAtIndexPlusOne) === "boolean" ? pInsertMiddleShiftAttrsStartAtIndexPlusOne : true);
		this.moveAttrIdxes(shiftStartingAtIndexPlusOne ? txtIdx+1 : txtIdx, attrSepObj.textWithoutAttrs.length);
		// Copy any attributes into this.attrs
		for (var attrTxtIdx in attrSepObj.attrs)
		{
			var newTxtIdx = txtIdx + (+attrTxtIdx);
			this.attrs[newTxtIdx] = attrSepObj.attrs[txtIdx];
		}
	}
}
// For the TextLine class: Removes text from the front of the line, and adjusts any attribute codes accordingly
//
// Parameters:
//  pNewStartIdx: The index before which to trim off (this is basically the new starting index for the text)
//
// Return value: An object indexed by text index with any color/attribute codes before pNewStartIndex
function TextLine_trimFront(pNewStartIdx)
{
	if (typeof(pNewStartIdx) !== "number" || pNewStartIdx < 0)
		return {};

	var frontAttrs = {};
	if (pNewStartIdx > 0)
		frontAttrs = this.popAttrsFromFront(pNewStartIdx-1);

	if (pNewStartIdx >= this.text.length)
	{
		this.text = "";
		this.attrs = {};
	}
	else
	{
		this.text = this.text.substr(pNewStartIdx);
		/*
		// Adjust the indexes of any attribute codes this line might have: First delete any attributes
		// before pNewStartIdx
		var attrKeys = Object.keys(this.attrs);
		for (var attrKeysIdx = 0; attrKeysIdx < attrKeys.length; ++attrKeysIdx)
		{
			var textIdx = +(attrKeys[attrKeysIdx]);
			if (textIdx < pNewStartIdx)
				delete this.attrs[textIdx];
		}
		*/
		// Adjust the indexes of the remaining attribute codes to the left (the front ones were removed earlier, to be returned).
		this.moveAttrIdxes(pNewStartIdx, -pNewStartIdx);
	}

	return frontAttrs;
}
// For the TextLine class: Removes text from the end of the line, and removes and returns any attribute codes from the
// end starting at the given index.
//
// Parameters:
//  pTxtIdx: The text index to start trimming the line at
//
// Return value: An object indexed by text index with any color/attribute codes from pTxtIdx and afterward
function TextLine_trimEnd(pTxtIdx)
{
	if (typeof(pTxtIdx) !== "number" || pTxtIdx < 0 || pTxtIdx >= this.text.length)
		return {};
	var rearAttrs = this.popAttrsFromEnd(pTxtIdx);
	this.text = this.text.substring(0, pTxtIdx);
	return rearAttrs;
}
// For the TextLine class: Moves attribute indexes according to an offset.
//
// Parameters:
//  pStartIdx: The text index at which to start moving the attribute indexes
//  pOffset: The offset by which to move the attribute indexes
function TextLine_moveAttrIdxes(pStartIdx, pOffset)
{
	if (typeof(pStartIdx) !== "number" || pStartIdx < 0 || pStartIdx >= this.text.length || typeof(pOffset) !== "number" || pOffset == 0)
		return;

	var startIdx = (pStartIdx < 0 ? 0 : pStartIdx);
	if (pOffset > 0)
	{
		// pOffset is positive: Moving attributes to the right
		// Go through the sorted attribute keys from right to left, and for any text indexes >= pStartIdx,
		// adjust them rightward by pOffset
		var sortedAttrKeys = this.getSortedAttrKeysArray();
		for (var keysI = sortedAttrKeys.length - 1; keysI >= 0; --keysI)
		{
			var textIdx = +(sortedAttrKeys[keysI]);
			if (textIdx >= pStartIdx)
			{
				this.attrs[textIdx + pOffset] = this.attrs[textIdx];
				delete this.attrs[textIdx];
			}
			else
				break;
		}
	}
	else
	{
		// pOffset is negative: Moving attributes to the left
		// Go through the sorted attribute keys from left to right, and for any text indexes >= startIdx,
		// adjust them by pOffset
		var sortedAttrKeys = this.getSortedAttrKeysArray();
		for (var keysI = 0; keysI < sortedAttrKeys.length; ++keysI)
		{
			var textIdx = +(sortedAttrKeys[keysI]);
			if (textIdx >= startIdx)
			{
				// Note: pOffset is negative, so adding pOffset will subtract from textIdx.
				var newTextIdx = textIdx + pOffset;
				if (newTextIdx < 0) newTextIdx = 0;
				this.attrs[newTextIdx] = this.attrs[textIdx];
				delete this.attrs[textIdx];
			}
		}
	}
}
// For the TextLine class: Performs text replacement (AKA macro replacement) in the text line
// for the current word, based on an index in the text line.
//
// Parameters:
//  pTxtReplacements: An associative array of text to be replaced (i.e.,
//                    gTxtReplacements)
//  pCharIndex: The current character index in the text line
//  pUseRegex: Whether or not to treat the text replacement search string as a
//             regular expression.
//  pPriorTxtLineAttrs: In case colors are enabled, this includes the attribute codes from prior lines.
//
// Return value: An object containing the following properties:
//               textLineIndex: The updated text line index (integer)
//               wordLenDiff: The change in length of the word that
//                            was replaced (integer)
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
//               priorTxtAttrs: Any prior text attribute codes before the replacement was made. If none, this
//                              will be an empty string.
function TextLine_doMacroTxtReplacement(pTxtReplacements, pCharIndex, pUseRegex, pPriorTxtLineAttrs)
{
	var retObj = {
		textLineIndex: pCharIndex,
		wordLenDiff: 0,
		wordStartIdx: 0,
		newTextEndIdx: 0,
		newTextLen: 0,
		madeTxtReplacement: false,
		priorTxtAttrs: ""
	};

	// Try to find the word at the given index in the text line
	var wordObj = this.getWord(retObj.textLineIndex);
	if (!wordObj.foundWord)
		return retObj;

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
				txtReplacement = wordObj.word.replace(regex, pTxtReplacements[prop]);
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
			txtReplacement = pTxtReplacements[wordObj.word];
			retObj.madeTxtReplacement = true;
		}
	}
	if (retObj.madeTxtReplacement)
	{
		// Look for any attribute codes in the replacement word and separate them out (to put into this.attrs)
		var attrSepObj = sepStringAndAttrCodes(txtReplacement);
		// Uppercase the first character if desired
		if (firstCharUpper)
			attrSepObj.textWithoutAttrs = attrSepObj.textWithoutAttrs.charAt(0).toUpperCase() + attrSepObj.textWithoutAttrs.substr(1);
		var originalTextLen = this.text.length;
		this.text = this.text.substr(0, wordObj.startIdx) + attrSepObj.textWithoutAttrs + this.text.substr(wordObj.endIndex+1);
		// Based on the difference in word length, update the data that
		// matters (retObj.textLineIndex, which keeps track of the index of the current line).
		// Note: The horizontal cursor position variable should be replaced after calling this
		// function.
		retObj.wordLenDiff = attrSepObj.textWithoutAttrs.length - wordObj.word.length;
		retObj.textLineIndex += retObj.wordLenDiff;
		retObj.newTextEndIdx = wordObj.endIndex + retObj.wordLenDiff;
		retObj.newTextLen = attrSepObj.textWithoutAttrs.length;

		// Starting at the index of one past the end of the original word, adjust any text
		// indexes in this.attrs based on the change in length of the word after replacement.
		if (retObj.wordLenDiff > 0)
		{
			var endIdx = wordObj.endIndex + 1;
			for (var txtIdx = originalTextLen - 1; txtIdx >= endIdx; --txtIdx)
			{
				if (this.attrs.hasOwnProperty(txtIdx))
				{
					this.attrs[txtIdx+retObj.wordLenDiff] = this.attrs[txtIdx];
					delete this.attrs[txtIdx];
				}
			}
		}
		else if (retObj.wordLenDiff < 0)
		{
			for (var txtIdx = wordObj.endIndex + 1; txtIdx < originalTextLen; ++txtIdx)
			{
				if (this.attrs.hasOwnProperty(txtIdx))
				{
					this.attrs[txtIdx+retObj.wordLenDiff] = this.attrs[txtIdx];
					delete this.attrs[txtIdx];
				}
			}
		}
		// Copy any attribute codes from the replacement word into this.attrs (adjusting indexes to be correct)
		for (var idx in attrSepObj.attrs)
		{
			var newTextIdx = +idx + wordObj.startIdx;
			this.attrs[newTextIdx] = attrSepObj.attrs[idx];
		}
		// Apply the prior text attributes to ensure the text color after the replacement is back to what it was previously
		if (typeof(pPriorTxtLineAttrs) === "string")
		{
			var thisLinePriorAttrs = this.getAttrs(wordObj.startIdx, true);
			var endIdx = wordObj.endIndex + retObj.wordLenDiff + 1;
			if (this.attrs.hasOwnProperty(endIdx))
				this.attrs[endIdx] += ("\x01n" + pPriorTxtLineAttrs + thisLinePriorAttrs);
			else
				this.attrs[endIdx] = ("\x01n" + pPriorTxtLineAttrs + thisLinePriorAttrs);
			retObj.priorTxtAttrs = pPriorTxtLineAttrs + thisLinePriorAttrs;
		}
		else
			retObj.priorTxtAttrs = this.getAttrs(wordObj.startIdx, true);
	}

	return retObj;
}
// For the TextLine class: Returns the word in a text line at a given index.  If the index
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
function TextLine_getWord(pCharIndex)
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
// For the TextLine class: Removes a single character from the line.  Also adjust attribute code
// indexes if necessary.
//
// Return value: Whether or not the removed character was UTF-8 (boolean)
function TextLine_removeCharAt(pCharIdx)
{
	if (typeof(pCharIdx) !== "number" || pCharIdx < 0 || pCharIdx >= this.text.length)
		return false;

	this.text = this.text.substr(0, pCharIdx) + this.text.substr(pCharIdx+1);
	// For attribute indexes to the right of pCharIdx, move them left by 1.
	this.moveAttrIdxes(pCharIdx+1, -1);
}
// For the TextLine class: Returns a string containing all the attribute codes from the text line (if any).
// If there are none, this will return an empty string.
//
// Parameters:
//  pEndIdx: Optional (numeric) - One past the last index in the text string to get attributes for. If this is
//           not specified (default), all of the line's attributes will be returned.
//  pIncludeNormalAttr: Optional boolean: Whether or not to include the normal attribute, if it exists.  Defaults
//                      to false.
function TextLine_getAttrs(pEndIdx, pIncludeNormalAttr)
{
	var attrsStr = "";
	var attrKeys = this.getSortedAttrKeysArray();
	var onePastLastIdx = this.text.length;
	if (typeof(pEndIdx) === "number" && pEndIdx >= 0 && pEndIdx < this.text.length)
		onePastLastIdx = pEndIdx;
	var includeNormalAttr = (typeof(pIncludeNormalAttr) === "boolean" ? pIncludeNormalAttr : false);

	for (var keysIdx = 0; keysIdx < attrKeys.length; ++keysIdx)
	{
		var textIdx = +(attrKeys[keysIdx]);
		// If pEndIdx is unspecified, we want to ensure we get all remaining attributes, including after
		// the last index and after that
		if (typeof(pEndIdx) !== "number" || textIdx < onePastLastIdx)
			attrsStr += this.attrs[textIdx];
		else
			break;
	}
	// If there is a normal attribute code in the middle of the string, anything before it.
	var normalAttrIdx = attrsStr.lastIndexOf("\x01n");
	if (normalAttrIdx < 0)
		normalAttrIdx = attrsStr.lastIndexOf("\x01N");
	if (normalAttrIdx > -1)
	{
		if (includeNormalAttr)
			attrsStr = attrsStr.substr(normalAttrIdx);
		else
			attrsStr = attrsStr.substr(normalAttrIdx);
	}
	return attrsStr;
}
// For the TextLine class: Removes all line attributes from the beginning up to (and not including) an end
// index, and returns an object with those attributes, where the object properties are the text indexes
// where those attributes were to be applied.
//
// Parameters:
//  pEndIndex: The ending index (non-inclusive) to stop removing attribute codes
//
// Return value: An object with string indexes as properties, and attribute codes as values for those indexes
function TextLine_popAttrsFromFront(pEndIdx)
{
	var attrObj = {};
	if (typeof(pEndIdx) !== "number" || pEndIdx < 0)
		return attrObj;
	var endIdx = (pEndIdx <= this.text.length ? pEndIdx : this.text.length);
	var sortedAttrKeys = this.getSortedAttrKeysArray();
	for (var attrKeysIdx = 0; attrKeysIdx < sortedAttrKeys.length; ++attrKeysIdx)
	{
		var textIdx = +(sortedAttrKeys[attrKeysIdx]);
		if (textIdx <= endIdx)
		{
			attrObj[textIdx] = this.attrs[textIdx];
			delete this.attrs[textIdx];
		}
		else
			break;
	}
	return attrObj;
}
// For the TextLine class: Removes all line attributes from the end starting from a string index, and
// returns an object with those attributes, where the object properties are the text indexes where
// those attributes were to be applied.
//
// Parameters:
//  pStartIdx: The index of where to start removing attribute codes
//
// Return value: An object with string indexes as properties, and attribute codes as values for those indexes
function TextLine_popAttrsFromEnd(pStartIdx)
{
	var attrObj = {};
	if (typeof(pStartIdx) !== "number" || pStartIdx > this.text.length)
		return attrObj;
	var startIdx = (pStartIdx >= 0 ? pStartIdx : 0);
	var sortedAttrKeys = this.getSortedAttrKeysArray();
	for (var attrKeysIdx = 0; attrKeysIdx < sortedAttrKeys.length; ++attrKeysIdx)
	{
		var textIdx = sortedAttrKeys[attrKeysIdx];
		if (textIdx >= startIdx)
		{
			attrObj[textIdx] = this.attrs[textIdx];
			delete this.attrs[textIdx];
		}
	}
	return attrObj;
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

// This function takes a string and returns a copy of the string
// with a color randomly alternating between dim & bright versions.
//
// Parameters:
//  pString: The string to convert
//  pColor: The color to use (Synchronet color code)
function randomDimBrightString(pString, pColor)
{
	// Return if an invalid string is passed in.
	if (pString == null)
		return "";
	if (typeof(pString) != "string")
		return "";

   // Set the color.  Default to green.
	var color = "\x01g";
	if ((pColor != null) && (typeof(pColor) != "undefined"))
      color = pColor;

   return(randomTwoColorString(pString, "\x01n" + color, "\x01n\x01h" + color));
}

// This function takes a string and returns a copy of the string
// with colors randomly alternating between two given colors.
//
// Parameters:
//  pString: The string to convert
//  pColor11: The first color to use (Synchronet color code)
//  pColor12: The second color to use (Synchronet color code)
function randomTwoColorString(pString, pColor1, pColor2)
{
	// Return if an invalid string is passed in.
	if (pString == null)
		return "";
	if (typeof(pString) != "string")
		return "";

	// Set the colors.  Default to green.
	var color1 = "\x01n\x01g";
	if ((pColor1 != null) && (typeof(pColor1) != "undefined"))
      color1 = pColor1;
   var color2 = "\x01n\x01g\x01h";
	if ((pColor2 != null) && (typeof(pColor2) != "undefined"))
      color2 = pColor2;

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
	var timeStr = strftime("%I:%M%p", time());
	timeStr = timeStr.replace("AM", "a");
	timeStr = timeStr.replace("PM", "p");
	
	return timeStr;
}

// Returns whether or not a character is printable.  This function is fairly simplistic.
function isPrintableChar(pText)
{
	// Make sure pText is valid and is a string.
	if (typeof(pText) != "string")
		return false;
	if (pText.length == 0)
		return false;

	var charCode = pText.charCodeAt(0);
	// If K_UTF8 exists, then we use it for input and UTF-8 characters are possible.
	// Otherwise, just ASCII/CP437 characters are possible.
	if (g_K_UTF8Exists)
		return (charCode > 31 && charCode != 127);
	else // ASCII/CP437
	{
		// Make sure the character is a printable ASCII/CP437 character in the range of
		// 32 to 254, except for 127 (delete).
		return (charCode > 31 && charCode < 255 && charCode != 127);
	}
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
      displayHelpHeader.headerLines = [];

      var headerText = EDITOR_PROGRAM_NAME + " Help \x01w(\x01y"
                      + (EDITOR_STYLE == "DCT" ? "DCT" : "Ice")
                      + " mode\x01w)";
      var headerTextLen = console.strlen(headerText);

      // Top border
      var headerTextStr = "\x01n\x01h\x01c" + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE;
      for (var i = 0; i < headerTextLen + 2; ++i)
         headerTextStr += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
      headerTextStr += CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE;
      displayHelpHeader.headerLines.push(headerTextStr);

      // Middle line: Header text string
      headerTextStr = CP437_BOX_DRAWINGS_LIGHT_VERTICAL + "\x01" + "4\x01y " + headerText + " \x01n\x01h\x01c"
                    + CP437_BOX_DRAWINGS_LIGHT_VERTICAL;
      displayHelpHeader.headerLines.push(headerTextStr);

      // Lower border
      headerTextStr = CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE;
      for (var i = 0; i < headerTextLen + 2; ++i)
         headerTextStr += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
      headerTextStr += CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE;
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
//  pPause: Whether or not to pause at the end
//  pCanCrossPost: Whether or not cross-posting is enabled
//  pTxtReplacments: Whether or not the text replacements feature is enabled
//  pUserSettings: Whether or not the user settings feature is enabled
//  pSpellCheck: Whether or not spell check is allowed
//  pCanChangeColor: Whether or not changing text color is allowed
//  pCanChangeSubject: Whether or not changing the subject is allowed
//  pClearScrFirst: Optional - Whether or not to clear the screen first. Defaults to true.
//  pNumAdditionalLinesFromTop: Optional - The number of lines from the top row to
//                              start at (i.e., of some other information has already
//                              been displayed). Defaults to 0.
function displayCommandList(pDisplayHeader, pPause, pCanCrossPost, pTxtReplacments,
                            pUserSettings, pSpellCheck, pCanChangeColor, pCanChangeSubject,
                            pClearScrFirst, pNumAdditionalLinesFromTop)
{
	var clearScrFirst = (typeof(pClearScrFirst) === "boolean" ? pClearScrFirst : true);
	if (clearScrFirst)
	{
		console.line_counter = 0;
		console.clear("\x01n");
	}

	var numAdditionalLinesFromTop = (typeof(pNumAdditionalLinesFromTop) === "number" && pNumAdditionalLinesFromTop >= 0 ? pNumAdditionalLinesFromTop : 0);

	var scrollTopRow = 2 + numAdditionalLinesFromTop;
	if (pDisplayHeader)
	{
		if (console.term_supports(USER_RIP))
		{
			var headerText = format("%s Help (%s mode)", EDITOR_PROGRAM_NAME, EDITOR_STYLE == "DCT" ? "DCT" : "Ice");
			console.crlf(); // A new set of RIP commands needs to be on its own line
			// RIP text color values:
			/*
			 Value   Color
			 -------------------------------------------------------
			 00      Black (00 is always the background color)
			 01      Blue
			 02      Green
			 03      Cyan
			 04      Red
			 05      Magenta
			 06      Brown
			 07      Light Gray
			 08      Dark Gray
			 09      Light Blue
			 0A      Light Green
			 0B      Light Cyan
			 0C      Light Red
			 0D      Light Magenta
			 0E      Yellow
			 0F      White
			 */
			var rip = RIPTitleBar(
				1,      // attribute slot
				//7, 7,   // bar position
				1, 1,   // bar position
				1,      // line style
				"0000", // pen
				headerText,
				"01",   // Text color (blue)
				//7, 7,   // text position
				60, 9    // text position
			);
			rip = rip.replace(/~$/, "");
			console.print(RIP_START_SEQ);
			console.print(rip + "\r\n");
			scrollTopRow += 4;
		}
		else
		{
			scrollTopRow = 5 + numAdditionalLinesFromTop;
			displayHelpHeader();
		}
	}

	// Generate a text string with a horizontal line & help text specifying the list of
	// hotkeys for use while displaying the scrolling help text
	var keyHelpText = "\x01n\x01h\x01cUp\x01b, \x01cDn\x01b, \x01cPgUp\x01b, \x01cPgDn\x01b, \x01cHOME\x01b, \x01cEND\x01b, \x01cESC\x01n\x01c/\x01hQ\x01y=\x01bClose";
	var padLen = Math.floor((console.screen_columns / 2) - (console.strlen(keyHelpText) / 2)) - 1;
	var separatorLineColor = "\x01n\x01r";
	var separatorLine = separatorLineColor;
	for (var i = 0; i < padLen; ++i)
		separatorLine += CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE;
	separatorLine += CP437_BOX_DRAWINGS_VERTICAL_SINGLE_AND_LEFT_DOUBLE;
	separatorLine += keyHelpText + separatorLineColor + CP437_BOX_DRAWINGS_VERTICAL_SINGLE_AND_RIGHT_DOUBLE;
	var remainingLen = console.screen_columns - console.strlen(separatorLine);
	for (var i = 0; i < remainingLen; ++i)
		separatorLine += CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE;
	console.gotoxy(1, scrollTopRow-1);
	console.print(separatorLine);

	// Display a bottom border on the last line of the screen
	console.gotoxy(1, console.screen_rows);
	console.print(separatorLineColor);
	for (var i = 0; i < console.screen_columns-1; ++i)
		console.print(CP437_BOX_DRAWINGS_HORIZONTAL_DOUBLE);

	// Create the text string for the frame object
	var txtLines = getCommandListHelpText(pCanCrossPost, pTxtReplacments, pUserSettings, pSpellCheck, pCanChangeColor, pCanChangeSubject);
	var textStr = "";
	for (var i = 0; i < txtLines.length; ++i)
		textStr += format("%s\r\n", txtLines[i]);
	// Create the frame object
	var frameObj = new Frame(1, scrollTopRow, console.screen_columns, console.screen_rows-scrollTopRow, BG_BLACK);
	frameObj.attr &=~ HIGH;
	frameObj.v_scroll = true;
	frameObj.h_scroll = false;
	frameObj.scrollbars = true;
	var scrollbarObj = new ScrollBar(frameObj, {bg: BG_BLACK, fg: LIGHTGRAY, orientation: "vertical", autohide: false});
	// Add the help text, then start the user input loop for the frame
	frameObj.putmsg(textStr, "\x01n");

	// Display the scrolling frame with help text
	var lastUserInput = doFrameInputLoop(frameObj, scrollbarObj, textStr);
}
// Helper for displayCommandList(): Gets the command list help text.
//
// Parameters:
//  pCanCrossPost: Whether or not cross-posting is enabled
//  pTxtReplacments: Whether or not the text replacements feature is enabled
//  pUserSettings: Whether or not the user settings feature is enabled
//  pSpellCheck: Whether or not spell check is allowed
//  pCanChangeColor: Whether or not changing text color is allowed
//  pCanChangeSubject: Whether or not changing the subject is allowed
//
// Return value: An array of strings with the command list help
function getCommandListHelpText(pCanCrossPost, pTxtReplacments, pUserSettings, pSpellCheck, pCanChangeColor, pCanChangeSubject)
{
	var txtLines = [];
	// Help keys and slash commands
	txtLines.push(format("\x01n\x01g%-44s  %-33s", "Help keys", "Slash commands (on blank line)"));
	txtLines.push(format("\x01k\x01h%-44s  %-33s", charStr(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, 9), charStr(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, 30)));
	//txtLines.push(getCmdKeyFormattedDouble("Ctrl-G", "General help", "/A", "Abort", true));
	txtLines.push(getCmdKeyFormattedDouble("Ctrl-G", "Input graphic character", "/A", "Abort"));
	txtLines.push(getCmdKeyFormattedDouble("Ctrl-L", "Command key list (this list)", "/S", "Save"));
	if (pTxtReplacments)
		txtLines.push(getCmdKeyFormattedDouble("Ctrl-T", "List text replacements", "/T", "List text replacements"));
	txtLines.push(getCmdKeyFormattedDouble("", "", "/Q", "Quote message"));
	txtLines.push(getCmdKeyFormattedDouble("", "", "/M", "Add a meme"));
	if (pUserSettings)
		txtLines.push(getCmdKeyFormattedDouble("", "", "/U", "Your user settings"));
	if (pCanCrossPost)
		txtLines.push(getCmdKeyFormattedDouble("", "", "/C", "Cross-post selection"));
	txtLines.push(getCmdKeyFormattedDouble("", "", "/UPLOAD", "Upload a message"));
	txtLines.push(format(" \x01c\x01h%-7s\x01g  \x01n\x01c%s", "", "", "/?", "Show help"));
	txtLines.push("");
	// Command/edit keys
	txtLines.push("\x01n\x01gCommand/edit keys\r\n\x01k\x01h" + charStr(CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE, 17));
	txtLines.push("");
	txtLines.push(getCmdKeyFormattedDouble("Ctrl-A", "Abort message", "PageUp", "Page up"));
	txtLines.push(getCmdKeyFormattedDouble("Ctrl-Z", "Save message", "PageDown", "Page down"));
	const quoteHotkey = gUserSettings.ctrlQQuote ? "Ctrl-Q" : "Ctrl-Y";
	txtLines.push(getCmdKeyFormattedDouble(quoteHotkey, "Quote message", "Ctrl-W", "Word/text search"));
	txtLines.push(getCmdKeyFormattedDouble("Insert/Ctrl-I", "Toggle insert/overwrite mode", "Ctrl-D", "Delete line"));
	if (pCanChangeSubject)
		txtLines.push(getCmdKeyFormattedDouble("Ctrl-S", "Change subject", "ESC", "Command menu"));
	else
		txtLines.push(getCmdKeyFormattedDouble("", "", "ESC", "Command menu"));
	// For the remaining hotkeys, build an array of them based on whether they're allowed or not.
	// Then with the array, output each pair of hotkeys on the same line, and if there's only one
	// left, display it by itself.
	var remainingHotkeysAndDescriptions = [];
	if (pUserSettings)
		remainingHotkeysAndDescriptions.push(makeHotkeyAndDescObj("Ctrl-U", "Your user settings"));
	if (pCanCrossPost)
		remainingHotkeysAndDescriptions.push(makeHotkeyAndDescObj("Ctrl-C", "Cross-post selection"));
	if (pCanChangeColor)
		remainingHotkeysAndDescriptions.push(makeHotkeyAndDescObj("Ctrl-K", "Change text color"));
	if (pSpellCheck)
		remainingHotkeysAndDescriptions.push(makeHotkeyAndDescObj("Ctrl-R", "Spell checker"));
	if (user.is_sysop)
	{
		remainingHotkeysAndDescriptions.push(makeHotkeyAndDescObj("Ctrl-O", "Import a file"));
		remainingHotkeysAndDescriptions.push(makeHotkeyAndDescObj("Ctrl-X", "Export to file"));
	}
	var i = 0;
	while (i < remainingHotkeysAndDescriptions.length)
	{
		var numHotkeysRemaining = remainingHotkeysAndDescriptions.length - i;
		if (numHotkeysRemaining >= 2)
		{
			var nextI = i + 1;
			var hotkey1 = remainingHotkeysAndDescriptions[i].hotkey;
			var desc1 = remainingHotkeysAndDescriptions[i].desc;
			var hotkey2 = remainingHotkeysAndDescriptions[nextI].hotkey
			var desc2 = remainingHotkeysAndDescriptions[nextI].desc;
			txtLines.push(getCmdKeyFormattedDouble(hotkey1, desc1, hotkey2, desc2));
			i += 2;
		}
		else
		{
			var keyText = format("\x01c\x01h%-13s\x01g: \x01n\x01c%s", remainingHotkeysAndDescriptions[i].hotkey, remainingHotkeysAndDescriptions[i].desc);
			txtLines.push(keyText);
			++i;
		}
	}

	return txtLines;
}
// Helper for getCommandListHelpText(): Returns formatted text with 2 a keys
// and their descriptions with formatting & colors, on the same line.
function getCmdKeyFormattedDouble(pKey, pDesc, pKey2, pDesc2)
{
	var sepChar1 = ":";
	var sepChar2 = ":";
	if (pKey.length == 0 && pDesc.length == 0)
		sepChar1 = " ";
	if (pKey2.length == 0 && pDesc2.length == 0)
		sepChar2 = " ";
	var formatStr = "\x01c\x01h%-13s\x01g" + sepChar1 + " \x01n\x01c%-28s \x01k\x01h"
	              + CP437_BOX_DRAWINGS_LIGHT_VERTICAL + " \x01c\x01h%-8s\x01g" + sepChar2
	              + " \x01n\x01c%s";
	return format(formatStr, pKey, pDesc, pKey2, pDesc2);
}
// Helper for displayCommandList(): Returns an object with 'hotkey' and 'desc' propeties with the hotkey & its description
function makeHotkeyAndDescObj(pHotkey, pDescription)
{
	return {
		hotkey: pHotkey,
		desc: pDescription
	};
}
// Returns a string with a character repeated a given number of times
//
// Parameters:
//  pChar: The character to repeat in the string
//  pNumTimes: The number of times to repeat the character
//
// Return value: A string with the given character repeated the given number of times
function charStr(pChar, pNumTimes)
{
	if (typeof(pChar) !== "string" || pChar.length == 0 || typeof(pNumTimes) !== "number" || pNumTimes < 1)
		return "";

	var str = "";
	for (var i = 0; i < pNumTimes; ++i)
		str += pChar;
	return str;
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
      console.clear("\x01n");
   if (pDisplayHeader)
      displayHelpHeader();

   console.print("\x01n\x01cSlyEdit is a full-screen message editor that mimics the look & feel of\r\n");
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
		console.clear("N");

	// Print the program information
	var programInfoStrs = getProgramInfoText();
	for (var i = 0; i < programInfoStrs.length; ++i)
		printf("%s\r\n", programInfoStrs[i]);
	if (pPause)
		console.pause();
}
// Helper for displayProgramInfo(): Gets program information text
function getProgramInfoText()
{
	var programInfoStrs = [];
	var programNameAndDate = "\x01n\x01h\x01c" + EDITOR_PROGRAM_NAME + "\x01n \x01cVersion \x01g"
	                       + EDITOR_VERSION + " \x01w\x01h(\x01b" + EDITOR_VER_DATE + "\x01w)";
	programInfoStrs.push(getCenteredTextStr(programNameAndDate));
	programInfoStrs.push(getCenteredTextStr("\x01n\x01cby Eric Oulashin"));
	programInfoStrs.push("\x01n\x01cSlyEdit is a full-screen message editor for Synchronet that mimics the look &");
	programInfoStrs.push("feel of IceEdit or DCT Edit.");
	return programInfoStrs;
}

// Displays the program information followed by the command list in a scrollable box
//
// Parameters:
//  pPause: Whether or not to pause at the end
//  pCanCrossPost: Whether or not cross-posting is enabled
//  pTxtReplacments: Whether or not the text replacements feature is enabled
//  pUserSettings: Whether or not the user settings feature is enabled
//  pSpellCheck: Whether or not spell check is allowed
//  pCanChangeColor: Whether or not changing text color is allowed
//  pCanChangeSubject: Whether or not changing the subject is allowed
function displayProgramInfoAndCommandList(pPause, pCanCrossPost, pTxtReplacments,
                                          pUserSettings, pSpellCheck, pCanChangeColor, pCanChangeSubject)
{
	console.line_counter = 0;
	console.clear("\x01n");

	// Get the program information.
	// The first line will be the program name & date.
	// If the user's terminal supports RIP, then display the program
	// name & date as RIP instead of displaying the first line from
	// the program info array.
	var programInfoStrs = getProgramInfoText();
	var infoStartIdx = 0;
	var RIPHeightOffset = 0; // To get to the right place to display the 2nd+ info lines etc.
	if (console.term_supports(USER_RIP))
	{
		++infoStartIdx; // We'll start at the next index in programInfoStrs after the RIP header

		var headerText = format("%s Version %s (%s mode) (%s)", EDITOR_PROGRAM_NAME, EDITOR_VERSION,
		                        EDITOR_STYLE == "DCT" ? "DCT" : "Ice", EDITOR_VER_DATE);
		console.crlf(); // A new set of RIP commands needs to be on its own line
		// RIP text color values:
		/*
		Value   Color
		-------------------------------------------------------
		00      Black (00 is always the background color)
		01      Blue
		02      Green
		03      Cyan
		04      Red
		05      Magenta
		06      Brown
		07      Light Gray
		08      Dark Gray
		09      Light Blue
		0A      Light Green
		0B      Light Cyan
		0C      Light Red
		0D      Light Magenta
		0E      Yellow
		0F      White
		*/
		var rip = RIPTitleBar(
			1,      // attribute slot
			//7, 7,   // bar position
			1, 1,   // bar position
			1,      // line style
			"0000", // pen
			headerText,
			"01",   // Text color (blue)
			25, 9    // text position
		);
		rip = rip.replace(/~$/, "");
		console.print(RIP_START_SEQ);
		console.print(rip + "\r\n");
		RIPHeightOffset = 3;
	}
	//console.print("\r\n\r\n\r\n");
	for (var i = 0; i < RIPHeightOffset; ++i)
		console.crlf();
	for (var i = infoStartIdx; i < programInfoStrs.length; ++i)
		printf("%s\r\n", programInfoStrs[i]);

	// Display the command list
	displayCommandList(false, pPause, pCanCrossPost, pTxtReplacments, pUserSettings,
	                   pSpellCheck, pCanChangeColor, pCanChangeSubject, false, programInfoStrs.length+RIPHeightOffset);
}

// Displays the informational screen for the program exit.
//
// Parameters:
//  pClearScreen: Whether or not to clear the screen.
function displayProgramExitInfo(pClearScreen)
{
	if (pClearScreen)
		console.clear("\x01n");

	console.print("\x01n\x01cYou have been using \x01hSlyEdit \x01n\x01cversion \x01g" + EDITOR_VERSION + " \x01n\x01m(" + EDITOR_VER_DATE + ")");
	console.crlf();
	console.print("\x01n\x01cby Eric Oulashin of \x01c\x01hD\x01n\x01cigital \x01hD\x01n\x01cistortion \x01hB\x01n\x01cBS");
	console.crlf();
	console.crlf();
	console.print("\x01n\x01cAcknowledgements for look & feel go to the following people:");
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
	var clearLineAttrib = "\x01n";
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
// pAlwaysEraseBox: In DCT mode - Boolean: Whether to erase the box regardless of a Yes or No answer.
//
// Return value: Boolean - true for a "Yes" answer, false for "No"
function promptYesNo(pQuestion, pDefaultYes, pBoxTitle, pIceRefreshForBothAnswers, pAlwaysEraseBox)
{
   var userResponse = pDefaultYes;

   if (EDITOR_STYLE == "DCT")
   {
      // We need to create an object of parameters to pass to the DCT-style
      // Yes/No function.
      var paramObj = new AbortConfirmFuncParams();
      paramObj.editLinesIndex = gEditLinesIndex;
      if (typeof(pBoxTitle) == "string")
         userResponse = promptYesNo_DCTStyle(pQuestion, pBoxTitle, pDefaultYes, paramObj, pAlwaysEraseBox);
      else
         userResponse = promptYesNo_DCTStyle(pQuestion, "Prompt", pDefaultYes, paramObj, pAlwaysEraseBox);
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
	// Meme library for meme definitions
	var memeLib = load({}, "meme_lib.js");
	// Configuration settings
	var cfgObj = {
		// Default settings
		thirdPartyLoadOnStart: [],
		runJSOnStart: [],
		thirdPartyLoadOnExit: [],
		runJSOnExit: [],
		displayEndInfoScreen: true,
		reWrapQuoteLines: true,
		allowColorSelection: true,
		saveColorsAsANSI: false,
		useQuoteLineInitials: true,
		indentQuoteLinesWithInitials: true,
		allowCrossPosting: true,
		enableTextReplacements: false,
		textReplacementsUseRegex: false,
		enableTaglines: false,
		tagLineFilename: genFullPathCfgFilename("SlyEdit_Taglines.txt", js.exec_dir),
		taglinePrefix: "... ",
		quoteTaglines: false,
		shuffleTaglines: false,
		allowUserSettings: true,
		allowEditQuoteLines: true,
		allowSpellCheck: true,
		ctrlQQuote: true,
		dictionaryFilenames: [],
		memeSettings: {
			memeMaxTextLen: 500,
			memeDefaultWidth: 39,
			random: false,
			memeDefaultBorderIdx: 0,
			memeDefaultColorIdx: 0,
			justify: memeLib.JUSTIFY_CENTER
		},

		// General SlyEdit color settings
		genColors: {
			// Cross-posting UI element colors
			listBoxBorder: "\x01n\x01g",
			listBoxBorderText: "\x01n\x01b\x01h",
			crossPostMsgAreaNum: "\x01n\x01h\x01w",
			crossPostMsgAreaNumHighlight: "\x01n\x01" + "4\x01h\x01w",
			crossPostMsgAreaDesc: "\x01n\x01c",
			crossPostMsgAreaDescHighlight: "\x01n\x01" + "4\x01c",
			crossPostChk: "\x01n\x01h\x01y",
			crossPostChkHighlight: "\x01n\x01" + "4\x01h\x01y",
			crossPostMsgGrpMark: "\x01n\x01h\x01g",
			crossPostMsgGrpMarkHighlight: "\x01n\x01" + "4\x01h\x01g",
			// Colors for certain output strings
			msgWillBePostedHdr: "\x01n\x01c",
			msgPostedGrpHdr: "\x01n\x01h\x01b",
			msgPostedSubBoardName: "\x01n\x01g",
			msgPostedOriginalAreaText: "\x01n\x01c",
			msgHasBeenSavedText: "\x01n\x01h\x01c",
			msgAbortedText: "\x01n\x01m\x01h",
			emptyMsgNotSentText: "\x01n\x01m\x01h",
			genMsgErrorText: "\x01n\x01m\x01h",
			listBoxItemText: "\x01n\x01c",
			listBoxItemHighlight: "\x01n\x01" + "4\x01w\x01h"
		},

		// Default Ice-style colors
		iceColors: {
			menuOptClassicColors: true,
			// Ice color theme file
			ThemeFilename: genFullPathCfgFilename("SlyIceColors_BlueIce.cfg", js.exec_dir),
			// Quote line color
			QuoteLineColor: "\x01n\x01c",
			// Ice colors for the quote window
			QuoteWinText: "\x01n\x01h\x01w",            // White
			QuoteLineHighlightColor: "\x01" + "4\x01h\x01c", // High cyan on blue background
			QuoteWinBorderTextColor: "\x01n\x01c\x01h", // Bright cyan
			BorderColor1: "\x01n\x01b",              // Blue
			BorderColor2: "\x01n\x01b\x01h",          // Bright blue
			// Ice colors for multi-choice prompts
			SelectedOptionBorderColor: "\x01n\x01b\x01h\x01" + "4",
			SelectedOptionTextColor: "\x01n\x01c\x01h\x01" + "4",
			UnselectedOptionBorderColor: "\x01n\x01b",
			UnselectedOptionTextColor: "\x01n\x01w",
			// Ice colors for the top info area
			TopInfoBkgColor: "\x01" + "4",
			TopLabelColor: "\x01c\x01h",
			TopLabelColonColor: "\x01b\x01h",
			TopToColor: "\x01w\x01h",
			TopFromColor: "\x01w\x01h",
			TopSubjectColor: "\x01w\x01h",
			TopTimeColor: "\x01g\x01h",
			TopTimeLeftColor: "\x01g\x01h",
			EditMode: "\x01c\x01h",
			KeyInfoLabelColor: "\x01c\x01h"
		},

		// Default DCT-style colors
		DCTColors: {
			// DCT color theme file
			ThemeFilename: genFullPathCfgFilename("SlyDCTColors_Default.cfg", js.exec_dir),
			// Quote line color
			QuoteLineColor: "\x01n\x01c",
			// DCT colors for the border stuff
			TopBorderColor1: "\x01n\x01r",
			TopBorderColor2: "\x01n\x01r\x01h",
			EditAreaBorderColor1: "\x01n\x01g",
			EditAreaBorderColor2: "\x01n\x01g\x01h",
			EditModeBrackets: "\x01n\x01k\x01h",
			EditMode: "\x01n\x01w",
			// DCT colors for the top informational area
			TopLabelColor: "\x01n\x01b\x01h",
			TopLabelColonColor: "\x01n\x01b",
			TopFromColor: "\x01n\x01c\x01h",
			TopFromFillColor: "\x01n\x01c",
			TopToColor: "\x01n\x01c\x01h",
			TopToFillColor: "\x01n\x01c",
			TopSubjColor: "\x01n\x01w\x01h",
			TopSubjFillColor: "\x01n\x01w",
			TopAreaColor: "\x01n\x01g\x01h",
			TopAreaFillColor: "\x01n\x01g",
			TopTimeColor: "\x01n\x01y\x01h",
			TopTimeFillColor: "\x01n\x01r",
			TopTimeLeftColor: "\x01n\x01y\x01h",
			TopTimeLeftFillColor: "\x01n\x01r",
			TopInfoBracketColor: "\x01n\x01m",
			// DCT colors for the quote window
			QuoteWinText: "\x01n\x01" + "7\x01b", // or k
			QuoteLineHighlightColor: "\x01n\x01w",
			QuoteWinBorderTextColor: "\x01n\x01" + "7\x01r",
			QuoteWinBorderColor: "\x01n\x01k\x01" + "7",
			// DCT colors for the bottom row help text
			BottomHelpBrackets: "\x01n\x01k\x01h",
			BottomHelpKeys: "\x01n\x01r\x01h",
			BottomHelpFill: "\x01n\x01r",
			BottomHelpKeyDesc: "\x01n\x01c",
			// DCT colors for text boxes
			TextBoxBorder: "\x01n\x01k\x01" + "7",
			TextBoxBorderText: "\x01n\x01r\x01" + "7",
			TextBoxInnerText: "\x01n\x01b\x01" + "7",
			YesNoBoxBrackets: "\x01n\x01k\x01" + "7",
			YesNoBoxYesNoText: "\x01n\x01w\x01h\x01" + "7",
			// DCT colors for the menus
			SelectedMenuLabelBorders: "\x01n\x01w",
			SelectedMenuLabelText: "\x01n\x01k\x01" + "7",
			UnselectedMenuLabelText: "\x01n\x01w\x01h",
			MenuBorders: "\x01n\x01k\x01" + "7",
			MenuSelectedItems: "\x01n\x01w",
			MenuUnselectedItems: "\x01n\x01k\x01" + "7",
			MenuHotkeys: "\x01n\x01w\x01h\x01" + "7"
		},

		strings: {
			areYouThere: "\x01n\x01r\x01h\x01i@NAME@! \x01n\x01hAre you really there?\x01n"
		}
	};

	// Default strings configuration filename
	var stringsCfgFilename = genFullPathCfgFilename("SlyEditStrings_En.cfg", js.exec_dir);

	// Open the SlyEdit configuration file
	var slyEdCfgFileName = genFullPathCfgFilename("SlyEdit.cfg", js.exec_dir);
	if (!file_exists(slyEdCfgFileName))
		slyEdCfgFileName = genFullPathCfgFilename("SlyEdit.example.cfg", js.exec_dir);
	var cfgFile = new File(slyEdCfgFileName);
	if (cfgFile.open("r"))
	{
		// Behavior settings
		var behaviorSettings = cfgFile.iniGetObject("BEHAVIOR");
		// Text strings
		var stringsSettings = cfgFile.iniGetObject("STRINGS");
		// Color settings
		var iceColorSettings = cfgFile.iniGetObject("ICE_COLORS");
		var DCTColorSettings = cfgFile.iniGetObject("DCT_COLORS");
		cfgFile.close();

		// Checking/setting: Behavior
		if (behaviorSettings != null)
		{
			// The following are all boolean properties/settings:
			var boolPropNames = ["displayEndInfoScreen", "reWrapQuoteLines", "allowColorSelection", "saveColorsAsANSI",
			                     "useQuoteLineInitials", "indentQuoteLinesWithInitials", "allowCrossPosting", "enableTaglines",
			                     "quoteTaglines", "shuffleTaglines", "allowUserSettings", "allowEditQuoteLines", "allowSpellCheck",
			                     "ctrlQQuote"];
			for (var i = 0; i < boolPropNames.length; ++i)
			{
				var propName = boolPropNames[i];
				if (behaviorSettings.hasOwnProperty(propName) && typeof(behaviorSettings[propName]) === "boolean")
					cfgObj[propName] = behaviorSettings[propName];
			}
			// Other settings:
			if (behaviorSettings.hasOwnProperty("add3rdPartyStartupScript") && typeof(behaviorSettings.add3rdPartyStartupScript) === "string")
				cfgObj.thirdPartyLoadOnStart.push(behaviorSettings.add3rdPartyStartupScript);
			if (behaviorSettings.hasOwnProperty("addJSOnStart") && typeof(behaviorSettings.addJSOnStart) === "string")
				cfgObj.runJSOnStart.push(behaviorSettings.addJSOnStart);
			if (behaviorSettings.hasOwnProperty("addJSOnExit") && typeof(behaviorSettings.addJSOnExit) === "string")
				cfgObj.runJSOnExit.push(behaviorSettings.addJSOnExit);
			if (behaviorSettings.hasOwnProperty("enableTextReplacements"))
			{
				// The enableTxtReplacements setting in the config file can
				// be regex, true, or false:
				//  - regex: Text replacement enabled using regular expressions
				//  - true: Text replacement enabled using exact match
				//  - false: Text replacement disabled
				if (typeof(behaviorSettings.enableTextReplacements) === "string")
					cfgObj.textReplacementsUseRegex = (behaviorSettings.enableTextReplacements.toUpperCase() == "REGEX");
				else if (typeof(behaviorSettings.enableTextReplacements) === "boolean")
					cfgObj.enableTextReplacements = behaviorSettings.enableTextReplacements;
				if (cfgObj.textReplacementsUseRegex)
					cfgObj.enableTextReplacements = true;
			}
			if (behaviorSettings.hasOwnProperty("tagLineFilename") && typeof(behaviorSettings.tagLineFilename) === "string")
				cfgObj.tagLineFilename = genFullPathCfgFilename(behaviorSettings.tagLineFilename, js.exec_dir);
			if (behaviorSettings.hasOwnProperty("taglinePrefix") && typeof(behaviorSettings.taglinePrefix) === "string")
				cfgObj.taglinePrefix = behaviorSettings.taglinePrefix;
			if (behaviorSettings.hasOwnProperty("dictionaryFilenames") && typeof(behaviorSettings.dictionaryFilenames) === "string")
				cfgObj.dictionaryFilenames = parseDictionaryConfig(behaviorSettings.dictionaryFilenames, js.exec_dir);
			if (behaviorSettings.hasOwnProperty("memeMaxTextLen") && typeof(behaviorSettings.memeMaxTextLen) === "number")
			{
				if (behaviorSettings.memeMaxTextLen >= 1)
					cfgObj.memeSettings.memeMaxTextLen = behaviorSettings.memeMaxTextLen;
			}
			if (behaviorSettings.hasOwnProperty("memeDefaultWidth") && typeof(behaviorSettings.memeDefaultWidth) === "number")
			{
				if (behaviorSettings.memeDefaultWidth >= 1)
					cfgObj.memeSettings.memeDefaultWidth = behaviorSettings.memeDefaultWidth;
			}
			if (behaviorSettings.hasOwnProperty("memeStyleRandom") && typeof(behaviorSettings.memeStyleRandom) === "boolean")
				cfgObj.memeSettings.random = behaviorSettings.memeStyleRandom;
			if (behaviorSettings.hasOwnProperty("memeDefaultBorder"))
			{
				var borderSettingType = typeof(behaviorSettings.memeDefaultBorder);
				if (borderSettingType === "string")
				{
					var borderUpper = behaviorSettings.memeDefaultBorder.toUpperCase();
					if (borderUpper == "NONE" || borderUpper == "BORDER_NONE")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_NONE;
					else if (borderUpper == "SINGLE" || borderUpper == "BORDER_SINGLE")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_SINGLE;
					else if (borderUpper == "MIXED1" || borderUpper == "BORDER_MIXED1")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_MIXED1;
					else if (borderUpper == "MIXED2" || borderUpper == "BORDER_MIXED2")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_MIXED2;
					else if (borderUpper == "MIXED3" || borderUpper == "BORDER_MIXED3")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_MIXED3;
					else if (borderUpper == "DOUBLE" || borderUpper == "BORDER_DOUBLE")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_DOUBLE;
					else if (borderUpper == "ORNATE1" || borderUpper == "BORDER_ORNATE1")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_ORNATE1;
					else if (borderUpper == "ORNATE2" || borderUpper == "BORDER_ORNATE2")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_ORNATE2;
					else if (borderUpper == "ORNATE3" || borderUpper == "BORDER_ORNATE3")
						cfgObj.memeSettings.memeDefaultBorderIdx = memeLib.BORDER_ORNATE3;
				}
				else if (borderSettingType === "number")
				{
					if (behaviorSettings.memeDefaultBorder >= 1 && behaviorSettings.memeDefaultBorder < memeLib.BORDER_COUNT)
						cfgObj.memeSettings.memeDefaultBorderIdx = behaviorSettings.memeDefaultBorder - 1;
				}
			}
			if (behaviorSettings.hasOwnProperty("memeDefaultColor"))
			{
				var memeColorSettingType = typeof(behaviorSettings.memeDefaultColor);
				if (memeColorSettingType  === "number")
				{
					if (behaviorSettings.memeMaxTextLen >= 1)
						cfgObj.memeSettings.memeDefaultColorIdx = behaviorSettings.memeDefaultColor - 1;
				}
			}
			if (behaviorSettings.hasOwnProperty("memeJustify"))
			{
				var justifySettingType = typeof(behaviorSettings.memeJustify);
				if (justifySettingType === "string")
				{
					var justifyUpper = behaviorSettings.memeJustify.toUpperCase();
					if (justifyUpper == "CENTER" || justifyUpper == "JUSTIFY_CENTER")
						cfgObj.memeSettings.justify = memeLib.JUSTIFY_CENTER;
					else if (justifyUpper == "LEFT" || justifyUpper == "JUSTIFY_LEFT")
						cfgObj.memeSettings.justify = memeLib.JUSTIFY_LEFT;
					else if (justifyUpper == "RIGHT" || justifyUpper == "JUSTIFY_RIGHT")
						cfgObj.memeSettings.justify = memeLib.JUSTIFY_RIGHT;
				}
				else if (justifySettingType === "number")
				{
					if (behaviorSettings.memeJustify >= 0 && behaviorSettings.memeJustify < memeLib.JUSTIFY_COUNT)
						cfgObj.memeSettings.justify = behaviorSettings.memeJustify;
				}
			}
		}

		// Color settings
		if (iceColorSettings != null)
		{
			if (iceColorSettings.hasOwnProperty("ThemeFilename") && typeof(iceColorSettings.ThemeFilename) === "string")
				cfgObj.iceColors.ThemeFilename = genFullPathCfgFilename(iceColorSettings.ThemeFilename, js.exec_dir);
			if (iceColorSettings.hasOwnProperty("menuOptClassicColors") && typeof(iceColorSettings.menuOptClassicColors) === "boolean")
				cfgObj.iceColors.menuOptClassicColors = iceColorSettings.menuOptClassicColors; // This is a boolean
		}
		if (DCTColorSettings != null)
		{
			if (DCTColorSettings.hasOwnProperty("ThemeFilename") && typeof(DCTColorSettings.ThemeFilename) === "string")
				cfgObj.DCTColors.ThemeFilename = genFullPathCfgFilename(DCTColorSettings.ThemeFilename, js.exec_dir);
		}

		// If there is a strings filename, then set stringsCfgFilename
		if (stringsSettings != null && stringsSettings.hasOwnProperty("stringsFilename") && typeof(stringsSettings.stringsFilename) === "string")
			stringsCfgFilename = genFullPathCfgFilename(stringsSettings.stringsFilename, js.exec_dir);
	}

	// If the strings configuration file exists, then load and read it
	if (file_exists(stringsCfgFilename))
	{
		var stringsFile = new File(stringsCfgFilename);
		if (stringsFile.open("r"))
		{
			var stringsSettingsObj = stringsFile.iniGetObject();
			stringsFile.close();
			for (var prop in cfgObj.strings)
			{
				if (typeof(stringsSettingsObj[prop]) === "string" && stringsSettingsObj[prop].length > 0)
				{
					// Replace "\x01" with control character
					cfgObj.strings[prop] = stringsSettingsObj[prop].replace(/\\x01/g, "\x01");
				}
			}
		}
	}

	// If no dictionaries were specified in the configuration file, then
	// set all available dictionary files in the configuration.
	if (cfgObj.dictionaryFilenames.length == 0)
	{
		var dictFilenames = getDictionaryFilenames(js.exec_dir);
		for (var i = 0; i < dictFilenames.length; ++i)
			cfgObj.dictionaryFilenames.push(dictFilenames[i]);
	}

	return cfgObj;
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
   var strings = [];

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

// Fixes the text lines in the gEditLines array so that they all
// have a maximum width to fit within the edit area.
//
// Parameters:
//  pTextLineArray: An array of TextLine objects to adjust
//  pStartIndex: The index of the line in the array to start at.
//  pEndIndex: One past the last index of the line in the array to end at.
//  pEditWidth: The width of the edit area (AKA the maximum line length + 1)
//  pUsingColors: Boolean - Whether or not text color/attribute codes are being used
//  pEditingAFile: Boolean - Whether or not we're editing a file (rather than posting
//                 a message in email or a sub-board)
//
// Return value: An object with the following parameters:
//               textChanged: Boolean - Whether or not any text was changed.
//               addedSpaceAtSplitPoint: Boolean - Whether or not a space was added at the split point
//                                       (possible when editing a regular text file rather than a message)
function reAdjustTextLines(pTextLineArray, pStartIndex, pEndIndex, pEditWidth, pUsingColors, pEditingAFile)
{
	var retObj = {
		textChanged: false,
		addedSpaceAtSplitPoint: false
	};


	// Returns without doing anything if any of the parameters are not
	// what they should be. (Note: Not checking pTextLineArray for now..)
	if (typeof(pStartIndex) != "number")
		return retObj;
	if (typeof(pEndIndex) != "number")
		return retObj;
	if (typeof(pEditWidth) != "number")
		return retObj;
	// Range checking
	if ((pStartIndex < 0) || (pStartIndex >= pTextLineArray.length))
		return retObj;
	if ((pEndIndex <= pStartIndex) || (pEndIndex < 0))
		return retObj;
	if (pEndIndex > pTextLineArray.length)
		pEndIndex = pTextLineArray.length;
	if (pEditWidth <= 5)
		return retObj;

	var usingColors = (typeof(pUsingColors) === "boolean" ? pUsingColors : true);

	var nextLineIndex = 0;
	var numCharsToRemove = 0;
	var splitIndex = 0;
	var spaceFoundAtSplitIdx = false; // Whether or not a space was found in a text line at the split index
	var splitIndexOriginal = 0;
	var tempText = null;
	var appendedNewLine = false; // If we appended another line
	var onePastLastLineIdx = pEndIndex;
	for (var i = pStartIndex; i < onePastLastLineIdx; ++i)
	{
		// As an extra precaution, check to make sure this array element is defined.
		if (pTextLineArray[i] == undefined)
			continue;

		nextLineIndex = i + 1;
		// If the line's text is longer or equal to the edit width, then if
		// possible, move the last word to the beginning of the next line.
		if (pTextLineArray[i].text.length >= pEditWidth)
		{
			numCharsToRemove = pTextLineArray[i].text.length - pEditWidth + 1;
			splitIndex = pTextLineArray[i].text.length - numCharsToRemove;
			splitIndexOriginal = splitIndex;
			// If the character in the text line at splitIndex is not a space,
			// then look for a space before splitIndex.
			spaceFoundAtSplitIdx = (pTextLineArray[i].text.charAt(splitIndex) == " ");
			if (!spaceFoundAtSplitIdx)
			{
				splitIndex = pTextLineArray[i].text.lastIndexOf(" ", splitIndex-1);
				spaceFoundAtSplitIdx = (splitIndex > -1);
				if (!spaceFoundAtSplitIdx)
					splitIndex = splitIndexOriginal;
			}
			var originalLineLen = pTextLineArray[i].text.length; // For adjusting attribute indexes
			tempText = pTextLineArray[i].text.substr(spaceFoundAtSplitIdx ? splitIndex+1 : splitIndex);
			// Remove the attributes from the end of the line that was cut short, to be moved to the beginning of
			// the next line. Note: This must be done before shortening the text.
			// New (2025-04-23): Note: It seems using splitIndex+1 wouldn't be a good idea
			var lastAttrs = pTextLineArray[i].popAttrsFromEnd(splitIndex);
			// Remove the text from the line up to splitIndex
			//var charAfter = pTextLineArray[i].text.substr(splitIndex, 1); // Temporary (for debugging)
			pTextLineArray[i].text = pTextLineArray[i].text.substr(0, splitIndex);
			retObj.textChanged = true;
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
				//++onePastLastLineIdx; // End loop index // TODO: Is this needed?
			}

			// Move the text around and adjust the line properties.
			if (appendedNewLine)
			{
				pTextLineArray[nextLineIndex].text = tempText;
				// Add the attributes from the last of the line to the next line, adjusting the
				// text indexes as needed.  If a space was at the split index, then the attribute
				// indexes will need to be adjusted accordingly, since we got all attributes starting
				// from the split index but we removed the space there.
				if (spaceFoundAtSplitIdx)
				{
					// A space was found at the split index.
					// If we're editing a file, then add the space back to the end of
					// that text line. When we're editing a file, spaces aren't added
					// in between the text lines, so we need to ensure the space is
					// still there if there was originally a space.

					// 2025-05-04: The following check seems needed when editing a file, but
					// was causeing an issue where if a line is wrapped, the cursor is placed
					// directly under the last character rather than at the end of the
					// text line. I've fixed this issue but left this comment in for future
					// reference.
					// This is done below in the 'else' block too.
					if (pEditingAFile) // New (2025-04-24)
					{
						pTextLineArray[i].text += " "; // New (2025-04-24)
						retObj.addedSpaceAtSplitPoint = true;
					}

					var lastAttrKeys = Object.keys(lastAttrs);
					if (lastAttrKeys.length > 0)
					{
						lastAttrKeys.sort(textLineAttrSortCompareFunc);
						for (var lastAttrKeyI = 0; lastAttrKeyI < lastAttrKeys.length; ++lastAttrKeyI)
						{
							var originalTextIdx = +(lastAttrKeys[lastAttrKeyI]);
							lastAttrs[originalTextIdx - 1] = lastAttrs[originalTextIdx];
							delete lastAttrs[originalTextIdx];
						}
					}
				}
				for (var textIdx in lastAttrs)
				{
					var newTextIdx = (+textIdx) - splitIndex;
					pTextLineArray[nextLineIndex].attrs[newTextIdx] = lastAttrs[textIdx];
					// TODO: There might be an off-by-one issue here, related to whether a space was found (check spaceFoundAtSplitIdx?)
					//spaceFoundAtSplitIdx
				}
			}
			else
			{
				// Did not append a new line.
				// New (2025-04-23)
				// If we're editing a file and a space was found where the line
				// was split, then add the space back to the end of that text
				// line. When we're editing a file, spaces aren't added in between
				// the text lines, so we need to ensure the space is still there if
				// there was originally a space.
				if (pEditingAFile)
				{
					if (spaceFoundAtSplitIdx)
					{
						pTextLineArray[i].text += " ";
						retObj.addedSpaceAtSplitPoint = true;
					}
				}
				// End new (2025-04-23)
				// If we're in insert mode, then insert the text at the beginning of
				// the next line.  Otherwise, overwrite the text in the next line.
				if (inInsertMode())
				{
					if (pEditingAFile)
						pTextLineArray[nextLineIndex].text = tempText + pTextLineArray[nextLineIndex].text;
					else // Editing a message for email/sub-board
						pTextLineArray[nextLineIndex].text = tempText + " " + pTextLineArray[nextLineIndex].text;
					// Move the next line's current attributes to the right
					pTextLineArray[nextLineIndex].moveAttrIdxes(0, tempText.length + 1);
					// Add the attributes from the last of the line to the next line, adjusting the
					// text indexes as needed
					var actualSplitIndex = (spaceFoundAtSplitIdx ? splitIndex+1 : splitIndex);
					for (var textIdx in lastAttrs)
					{
						var newTextIdx = (+textIdx) - actualSplitIndex;
						if (newTextIdx < 0) newTextIdx = 0;
						pTextLineArray[nextLineIndex].attrs[newTextIdx] = lastAttrs[textIdx];
					}
				}
				else
				{
					// We're in overwrite mode, so overwite the first part of the next
					// line with tempText.
					if (pTextLineArray[nextLineIndex].text.length < tempText.length)
					{
						pTextLineArray[nextLineIndex].text = tempText;
						pTextLineArray[nextLineIndex].attrs = lastAttrs;
					}
					else
					{
						pTextLineArray[nextLineIndex].text = tempText + pTextLineArray[nextLineIndex].text.substr(tempText.length);
						// Adjust & insert attributes
						for (var textIdx in pTextLineArray[nextLineIndex].attrs)
						{
							var newTextIdx = (+textIdx) + tempText.length;
							pTextLineArray[nextLineIndex].attrs[newTextIdx] = pTextLineArray[nextLineIndex].attrs[textIdx];
							delete pTextLineArray[nextLineIndex].attrs[textIdx];
						}
						//for (var textIdx in lastAttrs)
						//	pTextLineArray[nextLineIndex].attrs[textIdx] = lastAttrs[textIdx];
						for (var textIdx in lastAttrs)
						{
							var newTextIdx = (+textIdx) - originalLineLen;
							pTextLineArray[nextLineIndex].attrs[newTextIdx] = lastAttrs[textIdx];
						}
					}
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

					// Get the text to bring up to the current line.
					// If the current line does not end with a space and the next line
					// does not start with a space, then add a space between this line
					// and the next line's text.  This is done to avoid joining words
					// accidentally.
					tempText = "";
					var prependedTextWithSpace = false;
					if ((pTextLineArray[i].text.charAt(pTextLineArray[i].text.length-1) != " ") && (pTextLineArray[nextLineIndex].text.substr(0, 1) != " "))
					{
						// TODO: Need to check pEditingAFile here?
						if (!pEditingAFile) // Editing a message for email or a sub-board
							tempText = " ";
						prependedTextWithSpace = true;
					}
					tempText += pTextLineArray[nextLineIndex].text.substr(0, splitIndex);
					// Move the text from the next line to the current line, if the current
					// line has room for it.
					if (pTextLineArray[i].text.length + console.strlen(tempText.length) < pEditWidth)
					{
						var currentLineOriginalLen = pTextLineArray[i].text.length;
						pTextLineArray[i].text += tempText;
						// TODO: Color issue when deleting text and wrapping text up
						// Set the next line's text: Trim off the front up to splitIndex+1.  Also, capture any attribute
						// codes removed from the front of the next line (to be moved up).
						var frontAttrs = pTextLineArray[nextLineIndex].trimFront(splitIndex+1);
						retObj.textChanged = true;
						if (prependedTextWithSpace)
							++currentLineOriginalLen; // To fix off-by-1 issue with color/attribute codes
						for (var textLineIdx in frontAttrs)
						{
							var newTextLineIdx = (+textLineIdx) + currentLineOriginalLen;
							pTextLineArray[i].attrs[newTextLineIdx] = frontAttrs[textLineIdx];
						}

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
						retObj.textChanged = true;
					}
				}
			}
		}
	}

	return retObj;
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
	var retObj = {
		noQuoteLineIndex: -1,
		nextQuoteLineIndex: -1
	};

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
// This checks whether the given TextLine object is a valid object before
// checking if it's a quote line.
//
// Parameters:
//  pLineArray: An array of TextLine objects
//  pLineIndex: The index of the line in gEditLines
function isQuoteLine(pLineArray, pLineIndex)
{
	//if (typeof(pLineArray) == "undefined")
	if (!Array.isArray(pLineArray))
		return false;
	if (typeof(pLineIndex) !== "number")
		return false;
	if (pLineIndex < 0 || pLineIndex >= pLineArray.length)
		return false;
	if (typeof(pLineArray[pLineIndex]) !== "object")
		return false;

	return pLineArray[pLineIndex].isAQuoteLine();
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
	if (pAttrs.search(/^\x01N/) == 0)
	{
		normalAtStart = true;
		pAttrs = pAttrs.substr(2);
	}

	// Prepend the attribute control character to the new attribute
	var newAttr = "\x01" + pNewAttr;

	// Set a regexStr for searching & replacing
	var regexStr = "";
	switch (pAttrType)
	{
		case FORE_ATTR: // Foreground attribute
			regexStr = "\x01K|\x01R|\x01G|\x01Y|\x01B|\x01M|\x01C|\x01W";
			break;
		case BKG_ATTR: // Background attribute
			regexStr = "x01" + "0|\x01" + "1|\x01" + "2|\x01" + "3|\x01" + "4|\x01" + "5|\x01" + "6|\x01" + "7";
			break;
		case SPECIAL_ATTR: // Special attribute
			//regexStr = /\x01H|\x01I|\x01N/g;
			index = pAttrs.search(newAttr);
			if (index > -1)
                pAttrs = pAttrs.replace(newAttr, "");
			else
                pAttrs += newAttr;
			break;
		default:
			break;
	}

	// If regexStr is not blank, then search & replace on it in
	// pAttrs.
	if (regexStr != "")
	{
        var regex = new RegExp(regexStr, 'g');
		pAttrs = removeAttrIfExists(pAttrs, newAttr);
		// If the regexStr is found, then replace it.  Otherwise,
		// add pNewAttr to the attribute string.
		if (pAttrs.search(regex) > -1)
			pAttrs = pAttrs.replace(regex, "\x01" + pNewAttr);
		else
			pAttrs += "\x01" + pNewAttr;
	}

	// If pAttrs started with the normal attribute, then
	// put it back on.
	if (normalAtStart)
		pAttrs = "\x01N" + pAttrs;

	return pAttrs;
}

// Wraps lines to a specified width, accounting for prefixes in front of sections of lines (i.e., for quoting).
// Normalizes some line prefixes (i.e., if some prefixes have multiple > characters separated by spaces, the
// spaces in between will be removed, etc.).
//
// Parameters:
//  pTextLineArray: An array of strings containing the text lines to be wrapped
//  pQuotePrefix: The line prefix to prepend to 'new' quote lines (ones without an existing prefix)
//  pIndentQuoteLines: Whether or not to indent the quote lines
//  pTrimSpacesFromQuoteLines: Whether or not to trim spaces from quote lines (for when people
//                             indent the first line of their reply, etc.).  Defaults to true.
//  pMaxWidth: The maximum width of the lines
//  pJoinWrappedLineWithNextLine: Optional boolean - Whether or not to join the next line to wrapped lines.
//                                This defaults to true (this is the default behavior).
//
// Return value: The wrapped text lines, as an array of strings
function wrapTextLinesForQuoting(pTextLines, pQuotePrefix, pIndentQuoteLines, pTrimSpacesFromQuoteLines, pMaxWidth, pJoinWrappedLineWithNextLine)
{
	var quotePrefix = typeof(pQuotePrefix) === "string" ? pQuotePrefix : " > ";
	var maxLineWidth = typeof(pMaxWidth) === "number" && pMaxWidth > 0 ? pMaxWidth : console.screen_columns - 1;
	var indentQuoteLines = typeof(pIndentQuoteLines) === "boolean" ? pIndentQuoteLines : true;
	var trimSpacesFromQuoteLines = typeof(pTrimSpacesFromQuoteLines) === "boolean" ? pTrimSpacesFromQuoteLines : true;
	var joinWrappedLineWithNextLine = typeof(pJoinWrappedLineWithNextLine) === "boolean" ? pJoinWrappedLineWithNextLine : true;

	// Get information about the various 'sections'/paragraphs of the message
	var msgSections = getMsgSections(pTextLines);

	// Add an additional > character to lines with the prefix that have a >, and re-wrap the text lines
	// for the user's terminal width - 1
	var wrappedTextLines = [];
	for (var i = 0; i < msgSections.length; ++i)
	{
		var originalPrefix = msgSections[i].linePrefix;
		var previousSectionPrefix = i > 0 ? msgSections[i-1].linePrefix : "";
		var nextSectionPrefix = i < msgSections.length - 1 ? msgSections[i+1].linePrefix : "";

		// Make a copy of the section prefix. If there are any > characters with spaces (whitespace)
		// between them, get rid of the whitespace. Also, add another > to the end to indicate another
		// level of quoting.
		var thisSectionPrefix = msgSections[i].linePrefix;
		while (/>[\s]+>/.test(thisSectionPrefix))
			thisSectionPrefix = thisSectionPrefix.replace(/>[\s]+>/g, ">>");
		if (thisSectionPrefix.length > 0)
		{
			var charIdx = thisSectionPrefix.lastIndexOf(">");
			if (charIdx > -1)
			{
				// substrWithAttrCodes() is defined in dd_lightbar_menu.js
				var len = thisSectionPrefix.length - (charIdx+1);
				thisSectionPrefix = substrWithAttrCodes(thisSectionPrefix, 0, charIdx+1) + ">" + substrWithAttrCodes(thisSectionPrefix, charIdx+1, len);
			}
			if (!/ $/.test(thisSectionPrefix))
				thisSectionPrefix += " ";
		}
		else
		{
			// This section doesn't have a prefix, so it must be new unqoted text.  If this section
			// contains some non-blank lines, then set the quote prefix to the passed-in/default prefix
			var hasNonBlankLines = false;
			for (var textLineIdx = msgSections[i].begLineIdx; textLineIdx <= msgSections[i].endLineIdx && !hasNonBlankLines; ++textLineIdx)
				hasNonBlankLines = (console.strlen(pTextLines[textLineIdx].trim()) > 0);
			if (hasNonBlankLines)
				thisSectionPrefix = quotePrefix;
		}
		// If there is a prefix, then make sure it's indended or not indented, according to the parameter passed in
		if (thisSectionPrefix.length > 0)
		{
			var firstPrefixChar = thisSectionPrefix.charAt(0);
			if (indentQuoteLines)
			{
				if (firstPrefixChar != " ")
					thisSectionPrefix = " " + thisSectionPrefix;
			}
			else
				thisSectionPrefix = thisSectionPrefix.replace(/^\s+/, ""); // Remove any leading whitespace
		}

		// See if any of the text has UTF-8 characters
		var textIsUTF8 = false;
		if (pTextLines.length > 0)
		{
			for (var textLineIdx = msgSections[i].begLineIdx; textLineIdx <= msgSections[i].endLineIdx; ++textLineIdx)
				textIsUTF8 = str_is_utf8(pTextLines[textLineIdx]);
		}

		// Wrapping will be different if we want to join wrapped lines together or not
		if (joinWrappedLineWithNextLine)
		{
			// Build a long string containing the current section's text
			var arbitraryLongLineLen = 120; // An arbitrary line length (for determining when to add a CRLF)
			var mostOfConsoleWidth = Math.floor(console.screen_columns * 0.85); // For checking when to add a CRLF
			var halfConsoleWidth = Math.floor(console.screen_columns * 0.5); // For checking when to add a CRLF
			var sectionText = "";
			for (var textLineIdx = msgSections[i].begLineIdx; textLineIdx <= msgSections[i].endLineIdx; ++textLineIdx)
			{
				// If the line is a blank line, then count how many blank lines there are in a row
				// and append that many CRLF strings to the section text
				if (stringIsEmptyOrOnlyWhitespace(pTextLines[textLineIdx]))
					sectionText += "\r\n";
				else
				{
					var thisLineTrimmed = pTextLines[textLineIdx].trim();
					if ((nextSectionPrefix != "" && thisLineTrimmed == nextSectionPrefix.trim()) || (previousSectionPrefix != "" && thisLineTrimmed == previousSectionPrefix.trim()))
						sectionText += "\r\n";
					else
					{
						// Trim leading & trailing whitespace from the text & append it to the section text
						//sectionText += pTextLines[textLineIdx].substr(originalPrefix.length).trim() + " ";
						// substrWithAttrCodes() is defined in dd_lightbar_menu.js
						var len = pTextLines[textLineIdx].length - originalPrefix.length;
						//sectionText += substrWithAttrCodes(pTextLines[textLineIdx], originalPrefix.length, len).trim() + " ";
						var currentLineText = substrWithAttrCodes(pTextLines[textLineIdx], originalPrefix.length, len);
						if (trimSpacesFromQuoteLines)
							currentLineText = currentLineText.trim();
						//sectionText += currentLineText + " ";
						sectionText += currentLineText;
						// If the next line isn't blank and the current line is less than half of the
						// terminal width, append a \r\n to the end (the line may be this short on purpose)
						var numLinesInSection = msgSections[i].endLineIdx - msgSections[i].begLineIdx + 1;
						// See if the next line is blank or is a tear/origin line (starting with "--- " or "Origin").
						// In these situations, we want (or may want) to add a hard newline (CRLF: \r\n) at the end.
						var nextLineIsBlank = false;
						var nextLineIsOriginOrTearLine = false;
						if (textLineIdx < msgSections[i].endLineIdx)
						{
							var nextLineTrimmed = pTextLines[textLineIdx+1].trim();
							nextLineIsBlank = (console.strlen(nextLineTrimmed) == 0);
							if (!nextLineIsBlank)
								nextLineIsOriginOrTearLine = msgLineIsTearLineOrOriginLine(nextLineTrimmed);
						}
						// Get the length of the current line (if it needs wrapping, then wrap it & get the
						// length of the last line after wrapping), so that if that length is short, we might
						// put a CRLF at the end to signify the end of the paragraph/section
						var lineLastLength = 0;
						var currentLineScreenLen = console.strlen(pTextLines[textLineIdx]);
						if (currentLineScreenLen < console.screen_columns - 1)
							lineLastLength = currentLineScreenLen;
						else
						{
							var currentLine = pTextLines[textLineIdx];
							if (trimSpacesFromQuoteLines)
								currentLine = currentLine.trim();
							var paragraphLines = lfexpand(word_wrap(currentLine, console.screen_columns-1, null, true, textIsUTF8)).split("\r\n");
							paragraphLines.pop(); // There will be an extra empty line at the end; remove it
							if (paragraphLines.length > 0)
								lineLastLength = console.strlen(paragraphLines[paragraphLines.length-1]);
						}
						// Put a CRLF at the end in certain conditions
						var lineScreenLen = console.strlen(pTextLines[textLineIdx]);
						if (nextLineIsOriginOrTearLine || nextLineIsBlank)
							sectionText += "\r\n";
						// Append a CRLF if the line isn't blank and its length is less than 85% of the user's terminal width
						// ..and if the text line length is originally longer than an arbitrary length (bit arbitrary, but if a line is that long, then
						// it's probably its own paragraph
						else if (lineScreenLen > arbitraryLongLineLen && numLinesInSection > 1 && !nextLineIsBlank && lineLastLength <= mostOfConsoleWidth)
							sectionText += "\r\n";
						else if (lineScreenLen <= halfConsoleWidth && !nextLineIsBlank)
							sectionText += "\r\n";					
						else
							sectionText += " ";
					}
				}
			}
			// Remove the trailing space from the end, and wrap the section's text according to the length
			// with the current prefix, then append the re-wrapped text lines to wrappedTextLines
			sectionText = sectionText.replace(/ $/, "");
			var textWrapLen = maxLineWidth - thisSectionPrefix.length;
			var msgSectionLines = lfexpand(word_wrap(sectionText, textWrapLen, null, true, textIsUTF8)).split("\r\n");
			msgSectionLines.pop(); // There will be an extra empty line at the end; remove it
			for (var wrappedSectionIdx = 0; wrappedSectionIdx < msgSectionLines.length; ++wrappedSectionIdx)
			{
				// Prepend the text line with the section prefix only if the line isn't blank
				if (console.strlen(msgSectionLines[wrappedSectionIdx]) > 0)
					wrappedTextLines.push(thisSectionPrefix + msgSectionLines[wrappedSectionIdx]);
				else
					wrappedTextLines.push(msgSectionLines[wrappedSectionIdx]);
			}
		}
		else
		{
			// joinWrappedLineWithNextLine is false - Wrap lines but don't join them with the next line
			for (var textLineIdx = msgSections[i].begLineIdx; textLineIdx <= msgSections[i].endLineIdx; ++textLineIdx)
			{
				var textLine = pTextLines[textLineIdx].replace(/ $/, "");
				var textWrapLen = console.strlen(textLine) - thisSectionPrefix.length;
				if (textWrapLen < 0)
					textWrapLen = 0;
				var lines = lfexpand(word_wrap(textLine, textWrapLen, null, true, textIsUTF8)).split("\r\n");
				lines.pop(); // There will be an extra empty line at the end; remove it
				for (var linesIdx = 0; linesIdx < lines.length; ++linesIdx)
				{
					// Prepend the text line with the section prefix only if the line isn't blank
					if (console.strlen(lines[linesIdx]) > 0)
						wrappedTextLines.push(thisSectionPrefix + lines[linesIdx]);
					else
						wrappedTextLines.push(lines[linesIdx]);
				}

				// If the line is a blank line, then count how many blank lines there are in a row
				// and append that many CRLF strings to the section text
				//if (stringIsEmptyOrOnlyWhitespace(pTextLines[textLineIdx]))
				//	sectionText += "\r\n";
			}
		}
	}
	return wrappedTextLines;
}
// Helper for wrapTextLinesForQuoting(): Gets an an aray of message sections from an array of text lines.
//
// Parameters:
//  pTextLines: An array of strings
//
// Return value: An array of MsgSection objects representing the different sections of the message with various
//               quote prefixes
function getMsgSections(pTextLines)
{
	var msgSections = [];

	var lastQuotePrefix = findPatternedQuotePrefix(pTextLines[0]);
	var startLineIdx = 0;
	var lastLineIdx = 0;
	for (var i = 1; i < pTextLines.length; ++i)
	{
		var quotePrefix = findPatternedQuotePrefix(pTextLines[i]);
		var lineIsOnlyPrefix = pTextLines[i] == quotePrefix;
		quotePrefix = quotePrefix.replace(/\s+$/, "");
		if (lineIsOnlyPrefix)
			quotePrefix = "";
		//console.print((i+1) + ": Quote prefix length: " + quotePrefix.length + "\r\n"); // Temproary
		//if (quotePrefix.length == 0)
		//	continue;
		/*
		console.print((i+1) + ":" + quotePrefix + ":\r\n");
		console.print("Line is only prefix: " + lineIsOnlyPrefix + "\r\n");
		console.crlf();
		*/
		//console.print((i+1) + ": Quote prefix:" + quotePrefix + ":, last:" + lastQuotePrefix + ":\r\n"); // Temporary
		if (quotePrefix != lastQuotePrefix)
		{
			if (lastQuotePrefix.length > 0)
				msgSections.push(new MsgSection(startLineIdx, i-1, lastQuotePrefix));
			startLineIdx = i;
		}

		lastQuotePrefix = quotePrefix;
	}
	if (msgSections.length > 0)
	{
		// Add the message sections that are missing (no prefix)
		var lineStartIdx = 0;
		var lastEndIdxSeen = 0;
		var numSections = msgSections.length;
		for (var i = 0; i < numSections; ++i)
		{
			if (msgSections[i].begLineIdx > lineStartIdx)
				msgSections.push(new MsgSection(lineStartIdx, msgSections[i].begLineIdx-1, ""));
			lineStartIdx = msgSections[i].endLineIdx + 1;
			lastEndIdxSeen = msgSections[i].endLineIdx;
		}
		if (lastEndIdxSeen+1 < pTextLines.length - 1)
			msgSections.push(new MsgSection(lastEndIdxSeen+1, pTextLines.length - 1, ""));
		// Sort the message sections (by beginning line index)
		msgSections.sort(function(obj1, obj2) {
			if (obj1.begLineIdx < obj2.begLineIdx)
				return -1;
			else if (obj1.begLineIdx == obj2.begLineIdx)
				return 0;
			else if (obj1.begLineIdx > obj2.begLineIdx)
				return 1;
		});
	}
	else // There are no message sections; add one for the whole message with no prefix
		msgSections.push(new MsgSection(0, pTextLines.length - 1, ""));

	return msgSections;
}
// Helper for wrapTextLinesForQuoting(): Creates an object containing information aboug a message section (with or without a common line prefix)
function MsgSection(pBegLineIdx, pEndLineIdx, pLinePrefix)
{
	this.begLineIdx = pBegLineIdx;
	this.endLineIdx = pEndLineIdx;
	this.linePrefix = pLinePrefix;
}
// Looks for a quote prefix with a typical pattern at the start of a string.
// Returns it if found; returns an empty string if not found.
//
// Parameters:
//  pStr: A string to search
//
// Return value: The string's quote prefix, if found, or an empty string if not found
function findPatternedQuotePrefix(pStr)
{
	var strPrefix = "";
	// See if there is a quote prefix with a typical pattern (possibly a space with
	// possibly some characters followed by a > and a space).  Make sure it only gets
	// the first instance of a >, in case there are more.  Look for the first > and
	// get a substring with just that one and try to match it with a regex of a pattern
	// followed by >
	// First, look for just alternating spaces followed by > at the start of the string.
	//var prefixMatches = pStr.match(/^( *>)+ */);
	var prefixMatches = pStr.match(/^( *\S*>)+ */); // Possible whitespace followed by possible-non-whitespace
	if (Array.isArray(prefixMatches) && prefixMatches.length > 0)
	{
		if (pStr.indexOf(prefixMatches[0]) == 0) // >= 0
			strPrefix = prefixMatches[0];
	}
	else
	{
		// Alternating spaces and > at the start weren't found.  Look for the first >
		// and if found, see if it has any spaces & characters before it
		var GTIdx = pStr.indexOf("> "); // Look for the first >
		if (GTIdx >= 0)
		{
			///*var */prefixMatches = pStr.substr(0, GTIdx+2).match(/^ *[\d\w\W]*> /i);
			// substrWithAttrCodes() is defined in dd_lightbar_menu.js
			var len = pStr.length - (GTIdx+2);
			prefixMatches = substrWithAttrCodes(pStr, 0, GTIdx+2, len).match(/^ *[\d\w\W]*> /i);
			if (Array.isArray(prefixMatches) && prefixMatches.length > 0)
			{
				if (pStr.indexOf(prefixMatches[0]) == 0) // >= 0
					strPrefix = prefixMatches[0];
			}
		}
	}
	// If the prefix is over a certain length, then perhaps it's not actually a valid
	// prefix
	if (strPrefix.length > Math.floor(console.screen_columns/2))
		strPrefix = "";
	return strPrefix;
}
// Returns whether a text line is a tear line ("---") or an origin line
function msgLineIsTearLineOrOriginLine(pTextLine)
{
	return (pTextLine == "---" || pTextLine.indexOf("--- ") == 0 || pTextLine.indexOf(" --- ") == 0 || pTextLine.indexOf(" * Origin: ") == 0);
}
// Returns whether a string is empty or only whitespace
function stringIsEmptyOrOnlyWhitespace(pString)
{
	if (typeof(pString) !== "string")
		return false;
	return (pString.length == 0 || /^\s+$/.test(pString));
}


// Gets information about a message area, given a message name.
// This function First tries to read the values from the file
// DDML_SyncSMBInfo.txt in the node directory (written by the Digital
// Distortion Message Lister v1.31 and higher).  If that file can't be read,
// the values will default to the values of bbs.smb_last_msg,
// bbs.smb_total_msgs, and bbs.msg_number/bbs.smb_curmsg.
//
// Parameters:
//  pMsgAreaName: The name of the message area being posted to
//
// Return value: An object containing the following properties:
//  lastMsg: The last message in the sub-board (i.e., bbs.smb_last_msg), or -1 if editing a file
//  totalNumMsgs: The total number of messages in the sub-board (i.e., bbs.smb_total_msgs),
//                or 0 if editing a file
//  curMsgNum: The number/index of the current message being read.  Starting
//             with Synchronet 3.16 on May 12, 2013, this is the absolute
//             message number (bbs.msg_number).  For Synchronet builds before
//             May 12, 2013, this is bbs.smb_curmsg.  Starting on May 12, 2013,
//             bbs.msg_number is preferred because it works properly in all
//             situations, whereas in earlier builds, bbs.msg_number was
//             always given to JavaScript scripts as 0.
//             If editing a file, this will be -1.
//  msgNumIsOffset: Boolean - Whether or not the message number is an offset.
//                  If not, then it is the absolute message number (i.e.,
//                  bbs.msg_number).
//  subBoardCode: The current sub-board code (i.e., bbs.smb_sub_code, "mail", or "" if editing a file)
//  grpIndex: The message group index for the sub-board (-1 if personal mail or editing a file)
function getCurMsgInfo(pMsgAreaName)
{
	var retObj = {
		lastMsg: -1,
		totalNumMsgs: 0,
		curMsgNum: -1,
		msgNumIsOffset: false,
		subBoardCode: "",
		grpIndex: -1
	};
	if (pMsgAreaName.length == 0)
	{
		// No message area name. In this case, the user must be editing a file.
		// We can leave the return values as defaults. SlyEdit can see if the
		// user is editing a file by checking whether subBoardCode is an empty
		// string.
	}
	else if (pMsgAreaName.toUpperCase() == "ELECTRONIC MAIL")
	{
		retObj.subBoardCode = "mail";
		retObj.grpIndex = -1;
		var mailInfoForUser = getPersonalMailInfoForUser();
		retObj.lastMsg = mailInfoForUser.lastMsg;
		retObj.totalNumMsgs = mailInfoForUser.totalNumMsgs;
		retObj.curMsgNum = mailInfoForUser.curMsgNum;
		retObj.msgNumIsOffset = mailInfoForUser.msgNumIsOffset;
	}
	else if (bbs.smb_sub_code.length > 0)
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
	if ((typeof(pMsgAreaName) === "string") && (pMsgAreaName.length > 0) && (retObj.subBoardCode != "") && msg_area.sub.hasOwnProperty(retObj.subBoardCode))
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

// Returns whether the current message being replied to is a poll.
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
// Return value: Whether or not the message being replied to is a poll
function curMsgIsPoll(pMsgInfo)
{
	if (typeof(MSG_TYPE_POLL) == "undefined")
		return false;

	var msgIsPoll = false;

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
				msgIsPoll = Boolean(hdr.type & MSG_TYPE_POLL);
			msgBase.close();
		}
	}

	return msgIsPoll;
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

// Posts a message in a sub-board
//
// Paramters:
//  pSubBoardCode: Synchronet's internal code for the sub-board to post in
//  pFrom: The 'from' name of the person sending the message.  If the user chose to post anonymously
//         in the original sub-board, this will be "ANONYMOUS" (as given by Synchronet).
//  pTo: The name of the person to send the message to
//  pSubj: The subject of the email
//  pMessage: The email message
//  pFromUserNum: The number of the user to use as the message sender.
//                This is optional; if not specified, the current user
//                will be used.
//
// Return value: String - Blank on success, or message on failure.
function postMsgToSubBoard(pSubBoardCode, pFrom, pTo, pSubj, pMessage, pFromUserNum)
{
	// Return if the parameters are invalid.
	if (typeof(pSubBoardCode) != "string")
		return "Sub-board code is not a string";
	if (typeof(pTo) != "string")
		return "To name is not a string";
	if (pTo.length == 0)
		return "The 'to' user name is blank";
	if (typeof(pSubj) != "string")
		return "Subject is not a string";
	if (pSubj.length == 0)
		return "The subject is blank";
	if (typeof(pMessage) != "string")
		return "Message is not a string";
	if (pMessage.length == 0)
		return "Not sending an empty message";
	if (typeof(pFromUserNum) != "number")
		return "From user number is not a number";
	if ((pFromUserNum <= 0) || (pFromUserNum > system.lastuser))
		return "Invalid user number";

	// Load the user record specified by pFromUserNum.  If it's a deleted user,
	// then return an error.
	var fromUser = new User(pFromUserNum);
	if (fromUser.settings & USER_DELETED)
		return "The 'from' user is marked as deleted";

	// Check the posting access requirements for this sub-board.  If the
	// user is not able to post in this sub-board, then don't let them.
	if (!msg_area.sub[pSubBoardCode].can_post)
		return fromUser.name + " cannot post in " + pSubBoardCode;

	// Open the sub-board so that the message can be posted there.
	var msgbase = new MsgBase(pSubBoardCode);
	if (!msgbase.open())
		return ("Error opening the message area: " + msgbase.last_error);

	// Create the message header, and send the message.
	var header = {
		to: pTo,
		from_net_type: NET_NONE,
		to_net_type: NET_NONE
	};
	// For the 'From' name, if the SUB_ANON or SUB_AONLY flag is set, use
	// "Anonymous".  Otherwise, use the user's real name if the sub-board
	// is set up to post using real names; otherwise, use the user's alias.
	if ((msgbase.cfg.settings & SUB_AONLY) == SUB_AONLY) // The sub-board allows only anonymous posts
		header.from = (typeof(pFrom) === "string" ? pFrom : "ANONYMOUS");
	else if((msgbase.cfg.settings & SUB_ANON) == SUB_ANON)
	{
		// The sub-board allows anonymous posts but doesn't require it
		if (typeof(pFrom) === "string")
			header.from = pFrom;
		else
			header.from = ((msgbase.cfg.settings & SUB_NAME) == SUB_NAME ? fromUser.name : fromUser.alias);
	}
	else if ((msgbase.cfg.settings & SUB_NAME) == SUB_NAME)
		header.from = fromUser.name;
	else
		header.from = fromUser.alias;
	header.from_ext = fromUser.number;
	header.from_net_addr = fromUser.netmail;
	header.subject = pSubj;
	var saveRetval = msgbase.save_msg(header, pMessage);
	msgbase.close();

	var retStr = "";
	if (!saveRetval)
		retStr = "Error saving the message: " + msgbase.last_error;

	return retStr;
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
	var userSigFilename = system.data_dir + "user/" + format("%04d.sig", user.number);
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
	var retObj = {
		subCode: "",
		grpIndex: 0
	};

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
//  pReplacementsObj: The object/dictionary to populate.
//  pRegex: Whether or not the text replace feature is configured to use regular
//          expressions.  If so, then the search words in the array will not
//          be converted to uppercase and the replacement text will not be
//          converted to lowercase.
//  pAllowColors: Boolean: Whether or not to allow color/attribute codes
//
// Return value: The number of text replacements added to the array.
function populateTxtReplacements(pReplacementsObj, pRegex, pAllowColors)
{
	var numTxtReplacements = 0;

	// Note: Limited to words without spaces.
	// Open the word replacements configuration file
	var wordReplacementsFilename = genFullPathCfgFilename("SlyEdit_TextReplacements.cfg", js.exec_dir);
	var arrayPopulated = false;
	var wordFile = new File(wordReplacementsFilename);
	if (wordFile.open("r"))
	{
		var fileLine = null;      // A line read from the file
		var equalsPos = 0;        // Position of a = in the line
		var wordToSearch = null; // A word to be replaced
		var wordToSearchUpper = null;
		var substWord = null;    // The word to substitue
		// This tests numTxtReplacements < 9999 so that the 9999th one is the last
		// one read.
		while (!wordFile.eof && (numTxtReplacements < 9999))
		{
			// Read the next line from the config file.
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

			// Extract the word to search and substitution word from the line.  If
			// not using regular expressions, then convert the word to search to
			// all uppercase for case-insensitive searching.
			wordToSearch = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
			wordToSearchUpper = wordToSearch.toUpperCase();
			substWord = trimSpaces(fileLine.substr(equalsPos+1), true, false, true);
			// We'll want to make sure substWord only contains printable characters.  If not, then
			// skip this one.
			var substIsPrintable = true;
			if (pAllowColors)
			{
				// Replace any instances of specifying the control character in substWord with the actual control character
				substWord = substWord.replace(/\\[xX]01/g, "\x01").replace(/\\[xX]1/g, "\x01").replace(/\\1/g, "\x01");
				// Check for only control characters and printable characters in substWord
				for (var i = 0; (i < substWord.length) && substIsPrintable; ++i)
				{
					var currentChar = substWord.charAt(i);
					substIsPrintable = (currentChar == "\x01" || isPrintableChar(currentChar));
				}
			}
			else
			{
				substWord = strip_ctrl(substWord);
				for (var i = 0; (i < substWord.length) && substIsPrintable; ++i)
					substIsPrintable = isPrintableChar(substWord.charAt(i));
			}
			if (!substIsPrintable)
				continue;

			// And add the search word and replacement text to pReplacementsObj.
			if (wordToSearchUpper != substWord.toUpperCase())
			{
				if (pRegex)
					pReplacementsObj[wordToSearch] = substWord;
				else
					pReplacementsObj[wordToSearchUpper] = substWord;
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
   var colorSettingStrings = [];
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
	if (typeof(charIsLetter.regex) === "undefined")
	{
		var regexStr = "^[A-Z";
		var highASCIICodes = [192, 200, 204, 210, 217, 224, 232, 236, 242, 249, 193, 201, 205, 211, 218, 221, 225, 233, 237, 243, 250, 253, 194, 202, 206, 212, 219, 226, 234, 238, 244, 251, 195, 209, 213, 227, 241, 245, 196, 203, 207, 214, 220, 228, 235, 239, 246, 252, 231, 199, 223, 216, 248, 197, 229, 198, 230, 222, 254, 208, 240];
		for (var i = 0; i < highASCIICodes.length; ++i)
			regexStr += ascii(highASCIICodes[i]);
		regexStr += "]$";
		charIsLetter.regex = new RegExp(regexStr);
	}

	return charIsLetter.regex.test(pChar.toUpperCase());
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
			var defaultPath = pDefaultPath;
			if (defaultPath.length > 0 && defaultPath.charAt(defaultPath.length-1) != "/" && defaultPath.charAt(defaultPath.length-1) != "\\")
				defaultPath += "/";
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
//
// Return value: The user's keypress (the return value of console.getkey()
//               or console.inkey()).
function getUserKey(pMode)
{
	var userKey = "";

	// Inactivity timeout in milliseconds, for use with console.inkey().
	// If the user has the inactivity exemption (UFLAG_H), then use -1
	// for no timeout with console.inkey(). Otherwise, use the configured
	// timeout.
	var inputTimeoutMS = (user.security.exemptions&UFLAG_H) ? -1 : console.inactivity_hangup * 1000;

	// If K_UTF8 is defined, then add it to getKeymode.  K_UTF8 specifies not to
	// translate UTF-8 to CP437.
	var inputMode = pMode;
	if (g_K_UTF8Exists)
		inputMode |= K_UTF8;

	// Input a key from the user
	userKey = console.inkey(inputMode, inputTimeoutMS);

	// Get the character code (including the 2nd byte from the user's console), generate
	// the character code, then get the character from the character code.
	// Credit to Deuce for this code (this was seen in fseditor.js)
	if (gUserConsoleSupportsUTF8) // In fseditor, if the print mode contains P_UTF8
	{
		var ab = ascii(userKey);
		var val;
		var bit;
		var tmp;
		// Check if the 128th bit is set (hex 80)
		if (Boolean(ascii(userKey[0]) & 0x80))
		{
			for (bit = 7; ab & (1 << bit); --bit)
				val = ab & ((1 << bit) - 1);

			for (bit = 6; bit > 3 && (ab & (1<<bit)); --bit)
			{
				tmp = console.getbyte(inputTimeoutMS);
				val = (val << 6) | (tmp & 0x3f);
			}

			userKey = String.fromCharCode(val);
		}
	}

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
	var originalCurposX = curPos.x;

	// Build one string containing all the valid keys.
	var allValidKeys = "";
	for (var i = 0; i < pValidKeyStrs.length; ++i)
		allValidKeys += pValidKeyStrs[i];
	allValidKeys += BACKSPACE + CTRL_K;

	// User input loop
	var displayChars = !((pMode & K_NOECHO) == K_NOECHO);
	var userInput = "";
	var inputKey = "";
	var userInputIdx = 0; // For moving the cursor left & right.
	var idx = 0;
	var continueOn = true;
	while (continueOn)
	{
		inputKey = getUserKey(pMode|K_NOECHO);
		// If userInput is blank, then the timeout was probably reached, so don't
		// continue inputting characters.
		if (inputKey.length == 0)
			break;

		switch (inputKey)
		{
			case BACKSPACE: // Delete one key backward
				// If userInput has some characters in it, then remove the current key
				// and move the cursor back one space on the screen.
				if (userInput.length > 0 && userInputIdx > 0)
				{
					//userInput = userInput.substr(0, userInput.length-1);
					userInput = userInput.substring(0, userInputIdx-1) + userInput.substr(userInputIdx+1);
					--userInputIdx;
					if (userInputIdx < 0) userInputIdx = 0; // Shouldn't happen
					// If we are to display the input characters, then also blank out
					// the character on the screen and make sure the cursor is placed
					// properly on the screen.
					if (displayChars && curPos.x > originalCurposX)
					{
						console.gotoxy(--curPos.x, curPos.y);
						console.print(" ");
						console.gotoxy(curPos);
					}
				}
				break;
			case KEY_DEL: // Delete one key forward
				if (userInputIdx < userInput.length)
				{
					userInput = userInput.substr(0, userInputIdx) + userInput.substr(userInputIdx+1);
					if (displayChars)
					{
						var txtToWrite = userInput.substr(userInputIdx);
						console.print(txtToWrite.length > 0 ? txtToWrite : " ");
						console.cleartoeol("\x01n");
						console.gotoxy(curPos.x, curPos.y);
					}
				}
				//else if (userInputIdx == 0 && userInput.length == 1)
				//	userInput = "";
				break;
			case KEY_LEFT:
				// Move the cursor one position to the left
				if (displayChars && userInput.length > 0 && curPos.x > originalCurposX)
				{
					console.gotoxy(--curPos.x, curPos.y);
					--userInputIdx;
					if (userInputIdx < 0) userInputIdx = 0; // Shouldn't happen
				}
				break;
			case KEY_RIGHT:
				// Move the cursor one position to the right
				if (displayChars && userInputIdx < userInput.length)
				{
					console.gotoxy(++curPos.x, curPos.y);
					++userInputIdx;
					if (userInputIdx > userInput.length) userInputIdx = userInput.length; // Shouldn't happen
				}
				break;
			// ESC and Ctrl-K: Cancel out of color selection, whereas
			// ENTER will save the user's input before returning.
			case KEY_ESC:
			case CTRL_K:
				userInput = "";
				userInputIdx = 0;
			case KEY_ENTER:
				continueOn = false;
				break;
			default:
				// If the user pressed a valid key that hasn't been pressed yet, then
				// append/insert it to userInput.
				if (allValidKeys.indexOf(inputKey) > -1 && userInput.indexOf(inputKey) < 0)
				{
					if (userInputIdx == userInput.length)
						userInput += inputKey;
					else if (userInputIdx < userInput.length)
						userInput = userInput.substr(0, userInputIdx) + inputKey + userInput.substr(userInputIdx);
					// If K_NOECHO wasn't passed in pMode, then output the keypress
					if (displayChars)
					{
						//console.print(inputKey);
						var inputToDisplay = userInput.substr(userInputIdx);
						console.print(inputToDisplay);
						++curPos.x;
						if (inputToDisplay.length > 1)
							console.gotoxy(curPos.x, curPos.y);
					}
					++userInputIdx;
				}
				break;
		}
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
   var fileStrs = [];

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
		promptSpellCheckOnSave: false,
		wrapQuoteLines: pSlyEdCfgObj.reWrapQuoteLines,
		joinQuoteLinesWhenWrapping: true,
		useQuoteLineInitials: pSlyEdCfgObj.useQuoteLineInitials,
		// The next setting specifies whether or not quote lines should be
		// prefixed with a space when using author initials.
		indentQuoteLinesWithInitials: pSlyEdCfgObj.indentQuoteLinesWithInitials,
		// Whether or not to trim spaces from quoted lines
		trimSpacesFromQuoteLines: true,
		autoSignMessages: false,
		autoSignRealNameOnlyFirst: true,
		autoSignEmailsRealName: true,
		ctrlQQuote: pSlyEdCfgObj.ctrlQQuote,
		dictionaryFilenames: pSlyEdCfgObj.dictionaryFilenames
	};

	// Open the user settings file
	var userSettingsFile = new File(gUserSettingsFilename);
	if (userSettingsFile.open("r"))
	{
		// Behavior settings
		var behaviorSettings = userSettingsFile.iniGetObject("BEHAVIOR");
		userSettingsFile.close();
		// In case behaviorSettings is null/invalid for some reason, ensure
		// it's an object.
		if (behaviorSettings == null || typeof(behaviorSettings) !== "object")
			behaviorSettings = {};
		// The following are all boolean properties/settings:
		var boolPropNames = ["enableTaglines", "promptSpellCheckOnSave", "wrapQuoteLines", "joinQuoteLinesWhenWrapping",
		                     "useQuoteLineInitials", "indentQuoteLinesWithInitials", "trimSpacesFromQuoteLines",
		                     "autoSignMessages", "autoSignRealNameOnlyFirst", "autoSignEmailsRealName", "ctrlQQuote"];
		for (var i = 0; i < boolPropNames.length; ++i)
		{
			var propName = boolPropNames[i];
			if (behaviorSettings.hasOwnProperty(propName) && typeof(behaviorSettings[propName]) === "boolean")
				userSettingsObj[propName] = behaviorSettings[propName];
		}
		// Other settings
		if (behaviorSettings.hasOwnProperty("dictionaryFilenames") && typeof(behaviorSettings.dictionaryFilenames) === "string")
			userSettingsObj.dictionaryFilenames = parseDictionaryConfig(behaviorSettings.dictionaryFilenames, js.exec_dir);
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
		                                  "promptSpellCheckOnSave",
		                                  "wrapQuoteLines",
		                                  "joinQuoteLinesWhenWrapping",
		                                  "useQuoteLineInitials",
		                                  "indentQuoteLinesWithInitials",
		                                  "trimSpacesFromQuoteLines",
		                                  "autoSignMessages",
		                                  "autoSignRealNameOnlyFirst",
		                                  "autoSignEmailsRealName",
		                                  "ctrlQQuote"];
		userSettingsFile.writeln("[BEHAVIOR]");
		for (var i = 0; i < behaviorBoolSettingNames.length; ++i)
		{
			if (pUserSettingsObj.hasOwnProperty(behaviorBoolSettingNames[i]))
				userSettingsFile.writeln(behaviorBoolSettingNames[i] + "=" + (pUserSettingsObj[behaviorBoolSettingNames[i]] ? "true" : "false"));
		}

		// Write the spell-check dictionary selection
		var dictionaryList = "";
		for (var i = 0; i < pUserSettingsObj.dictionaryFilenames.length; ++i)
		{
			// Strip the full path to get just the filename, remove the .txt filename
			// extension, and remove the "dictionary_" prefix.
			var dictName = file_getname(pUserSettingsObj.dictionaryFilenames[i]);
			if (/\.txt$/.test(dictName))
				dictName = dictName.substr(0, dictName.length-4);
			if (/^dictionary_/.test(dictName))
				dictName = dictName.substr(11);
			dictionaryList += dictName + ",";
		}
		// Remove any trailing comma
		if (/,$/.test(dictionaryList))
			dictionaryList = dictionaryList.substr(0, dictionaryList.length-1);
		// Write the dictionary list to the user settings file
		userSettingsFile.writeln("dictionaryFilenames=" + dictionaryList);

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

// Sets the "pause" text to an empty string, does a console.pause(), then restores the pause text.
function consolePauseWithoutText()
{
	var originalPausePromptText = bbs.text(Pause); // 563: The "Press a key" text in text.dat
	bbs.replace_text(Pause, "");
	console.pause();
	bbs.revert_text(Pause);
}

// Returns an array of dictionary filenames, with the filename pattern
// dictionary_*.txt, in either the sbbs/mods dir, sbbs/ctrl, or the
// given default directory.
//
// Parameters:
//  pDefaultPath: The path to look in besides sbbs/mods and sbbs/ctrl
//
// Return value: An array with the full paths & filenames of dictionary
//               files in either of sbbs/mods, sbbs/ctrl, or pDefaultPath
function getDictionaryFilenames(pDefaultPath)
{
	var filenameWildcard = "dictionary_*.txt";
	var dictionaryFilenames = [];
	var dirEntries = directory(system.mods_dir + filenameWildcard);
	for (var i = 0; i < dirEntries.length; ++i)
		dictionaryFilenames.push(dirEntries[i]);
	dirEntries = directory(system.ctrl_dir + filenameWildcard);
	for (var i = 0; i < dirEntries.length; ++i)
		dictionaryFilenames.push(dirEntries[i]);
	if (typeof(pDefaultPath) == "string")
	{
		// Make sure the default path has a trailing path separator
		var defaultPath = pDefaultPath;
		if (defaultPath.length > 0 && defaultPath.charAt(defaultPath.length-1) != "/" && defaultPath.charAt(defaultPath.length-1) != "\\")
			defaultPath += "/";
		dirEntries = directory(defaultPath + filenameWildcard);
		for (var i = 0; i < dirEntries.length; ++i)
			dictionaryFilenames.push(dirEntries[i]);
	}
	return dictionaryFilenames;
}

// Reads a dictionary file.  Returns an array containing the lines from
// the dictionary file.  The lines will all be lower-case for case-insensitive
// matching.
//
// Parameters:
//  pFilename: The filename of a dictionary file to read
//  pFullyPathed: Boolean - Whether or not the filename is fully pathed
//
// Return value: An array containing the lines read from the file, all lower-case
function readDictionaryFile(pFilename, pFullyPathed)
{
	var dictFilename = "";
	if (pFullyPathed)
		dictFilename = pFilename;
	else
		dictFilename = genFullPathCfgFilename(pFilename, js.exec_dir);

	// Read the lines from the dictionary; skip Aspell copyright header lines
	// if they exist, and lower-case all words for case-insensitive matching.
	var dictionary = [];
	var txtFile = new File(dictFilename);
	if (txtFile.open("r"))
	{
		dictionary = txtFile.readAll(2048);
		txtFile.close();
		// See if there's an Aspell copyright header, and if so, remove it.  Also,
		// ensure all the lines are lower-case.
		var inCopyrightHeader = false;
		var copyrightStartIdx = -1; // Will be >= 0 if we find the first line of the copyright header
		var copyrightEndIdx = -1; // Will be >= 0 if we find the copyright end index.  The index of the last line
		for (var i = 0; i < dictionary.length; ++i)
		{
			if (dictionary[i] == "Custom wordlist generated from http://app.aspell.net/create using SCOWL")
			{
				inCopyrightHeader = true;
				copyrightStartIdx = i;
			}
			else if (inCopyrightHeader)
			{
				if (dictionary[i] == "---")
				{
					inCopyrightHeader = false;
					copyrightEndIdx = i;
				}
			}
			else
				dictionary[i] = dictionary[i].toLowerCase();
		}
		// If we found valid indexes for the Aspell copyright header, then remove it.
		if ((copyrightStartIdx >= 0) && (copyrightEndIdx >= 0))
			dictionary.splice(copyrightStartIdx, copyrightEndIdx-copyrightStartIdx+1);
		// Remove any empty strings from the array
		dictionary = dictionary.filter(function(str) { return str.length > 0; });
	}

	return dictionary;
}

// Finds a word's index in a dictionary array.  If the word is not
// found, returns -1.
//
// Parameters:
//  pDict: An array of words.  This should be sorted with all words the same
//         letter case as the word being searched for.
//  pWord: The word to look for in the dictionary.  This should be the same
//         letter case as the words in the dictionary.
//
// Return value: The index of the word in the dictionary array, or -1 if not found.
function findWordIdxInDictionary(pDict, pWord)
{
	if ((typeof(pDict) != "object") || (typeof(pWord) != "string"))
		return -1;

	var wordIdx = -1;

	// Assuming pDict is sorted, do a binary search to see if the word is
	// in the dictionary.
	var startIdx = 0;
	var endIdx = pDict.length;
	var midIdx = 0;
	var lastStartIdx = 0;
	var lastEndIdx = 0;
	var continueOn = true;
	while ((wordIdx == -1) && continueOn)
	{
		lastStartIdx = startIdx;
		lastEndIdx = endIdx;

		midIdx = startIdx + Math.floor((endIdx - startIdx) / 2);
		if (pWord == pDict[midIdx])
			wordIdx = midIdx;
		else if (pWord < pDict[midIdx])
			endIdx = midIdx;
		else if (pWord > pDict[midIdx])
			startIdx = midIdx;

		// Keep searching if either the start index or end index are different
		// from the last iteration.
		continueOn = ((lastStartIdx != startIdx) || (lastEndIdx != endIdx));
	}

	return wordIdx;
}

// Returns whether a word exists in a dictionary.
//
// Parameters:
//  pDict: An array of words.  This should be sorted with all words the same
//         letter case as the word being searched for.
//  pWord: The word to look for in the dictionary.  This should be the same
//         letter case as the words in the dictionary.
//
// Return value: Boolean: Whether or not the word exists in the dictionary.
function wordExists(pDict, pWord)
{
	return (findWordIdxInDictionary(pDict, pWord) > -1);
}

// Returns whether a word exists.  Checks multiple dictionary arrays.
//
// Parameters:
//  pDicts: An array of arrays of words.  The arrays should be sorted with all words the same
//          letter case as the word being searched for.
//  pWord: The word to look for in the dictionary.  This should be the same
//         letter case as the words in the dictionary.
//
// Return value: Boolean: Whether or not the word exists in the dictionary.
function wordExists_MultipleDictionaries(pDicts, pWord)
{
	var foundWord = false;
	for (var i = 0; (i < pDicts.length) && !foundWord; ++i)
		foundWord = wordExists(pDicts[i], pWord);
	return foundWord;
}

// Parses the dictionary configuration line (from the SlyEdit configuration
// file or user settings file).
//
// Parameters:
//  pDictionarySpec: A comma-separated list of dictionary filenames
//  pDefaultPath: The default directory (must have a trailing separator character)
//
// Return value: An array of fully-pathed dictionary filenames
function parseDictionaryConfig(pDictionarySpec, pDefaultPath)
{
	var dictionaryFilenames = [];
	var dictionaryNames = pDictionarySpec.split(",");
	for (var i = 0; i < dictionaryNames.length; ++i)
	{
		// Allow dictionary filenames as either dictionary_<language>
		// or just <language> (where <language> is a language name.
		// If it doesn't start with "dictionary_", then prepend that.
		// If it doesn't end in a .txt, then append ".txt".
		var filename = dictionaryNames[i];
		if (!/^dictionary_/.test(filename))
			filename = "dictionary_" + filename;
		if (!/\.txt$/.test(filename))
			filename += ".txt";
		filename = genFullPathCfgFilename(filename, pDefaultPath);
		// If the file exists, add the filename to the configuration.
		if (file_exists(filename))
			dictionaryFilenames.push(filename);
	}
	return dictionaryFilenames;
}

// Gets the name of a language from a dictionary filename
//
// Parameters:
//  pFilenameFullPath: The full path & filename to a dictionary file
//
// Return value: The language name from the dictionary file
function getLanguageNameFromDictFilename(pFilenameFullPath)
{
	var justFilename = file_getname(pFilenameFullPath);
	var languageName = "";
	var dotIdx = justFilename.indexOf(".");
	if (dotIdx > -1)
		languageName = justFilename.substr(11, dotIdx-11);
	else
		languageName = justFilename.substr(11);
	// Figure out the language name from common standard localization tags
	var languageNameLower = languageName.toLowerCase();
	var isSupplemental = false;
	if (/[a-z]{2}-[a-z]{2}-supplemental/.test(languageNameLower))
	{
		isSupplemental = true;
		languageNameLower = languageNameLower.substr(0, 5);
	}
	var lower_n_tilde = ascii(164);
	var lower_e_forward_accent = ascii(130);
	if (languageNameLower == "en")
		languageName = "English (General)";
	else if (languageNameLower == "fr")
		languageName = "French (General)";
	else if (languageNameLower == "es")
		languageName = "Espa" + lower_n_tilde + "ol (General)";
	else if (languageNameLower == "pt")
		languageName = "Portug" + lower_e_forward_accent + "s (General)";
	else if (languageNameLower == "de")
		languageName = "Deutsch (General)";
	else if (languageNameLower == "nl")
		languageName = "Dutch (General)";
	else if (languageNameLower == "it")
		languageName = "Italian (General)";
	else if (languageNameLower == "bn-BD")
		languageName = "Bangla (Bangladesh)";
	else if (languageNameLower == "bn-in")
		languageName = "Bangla (India)";
	else if (languageNameLower == "nl-be")
		languageName = "Dutch (Belgium)";
	else if (languageNameLower == "nl-nl")
		languageName = "Dutch (Netherlands)";
	else if (languageNameLower == "en-gb")
		languageName = "English (UK)";
	else if (languageNameLower == "en-us")
		languageName = "English (US)";
	else if (languageNameLower == "en-ca")
		languageName = "English (CA)";
	else if (languageNameLower == "en-in")
		languageName = "English (India)";
	else if (languageNameLower == "en-au")
		languageName = "English (AU)";
	else if (languageNameLower == "en-nz")
		languageName = "English (NZ)";
	else if (languageNameLower == "fr-be")
		languageName = "French (Belgium)";
	else if (languageNameLower == "fr-ch")
		languageName = "French (Switzerland)";
	else if (languageNameLower == "fr-fr")
		languageName = "French (France)";
	else if (languageNameLower == "fr-ca")
		languageName = "French (CA)";
	else if (languageNameLower == "de-at")
		languageName = "Deutsch (Austria)";
	else if (languageNameLower == "de-de")
		languageName = "Deutsch (Deutschland)";
	else if (languageNameLower == "de-ch")
		languageName = "Deutsch (Schweiz)";
	else if (languageNameLower == "it-ch")
		languageName = "Italian (Switzerland)";
	else if (languageNameLower == "it-it")
		languageName = "Italian (Italy)";
	else if (languageNameLower == "pt-pt")
		languageName = "Portug" + lower_e_forward_accent + "s (Portugal)";
	else if (languageNameLower == "pt-br")
		languageName = "Portug" + lower_e_forward_accent + "s (BR)";
	else if (languageNameLower == "es-es")
		languageName = "Espa" + lower_n_tilde + "ol (Espa" + lower_n_tilde + "a)";
	else if (languageNameLower == "es-co")
		languageName = "Espa" + lower_n_tilde + "ol (CO)";
	else if (languageNameLower == "es-cl")
		languageName = "Espa" + lower_n_tilde + "ol (CL)";
	else if (languageNameLower == "es-us")
		languageName = "Espa" + lower_n_tilde + "ol (US)";
	else if (languageNameLower == "es-005")
		languageName = "Espa" + lower_n_tilde + "ol (South America)";
	else if (languageNameLower == "zh-cn")
		languageName = "Chinese (China)";
	else if (languageNameLower == "zh-tw")
		languageName = "Chinese (Taiwan)";
	else if (languageNameLower == "zh-hk")
		languageName = "Chinese (Hong Kong)";
	else if (languageNameLower == "zh-Hans")
		languageName = "Chinese (Simplified)";
	else if (languageNameLower == "zh-Hans-cn")
		languageName = "Chinese (Simplified) (China)";
	else if (languageNameLower == "ta-in")
		languageName = "Tamil (India)";
	else if (languageNameLower == "ta-lk")
		languageName = "Tamil (Sri Lanka)";
	else if (languageNameLower == "af-za")
		languageName = "Afrikaans (ZA)";
	// TODO: Add more language tags
	// http://www.lingoes.net/en/translator/langcode.htm
	else // Default to capitalized first letter & lowercase remainder
		languageName = languageName.substr(0, 1).toUpperCase() + languageName.substr(1).toLowerCase();

	if (isSupplemental)
		languageName += " (Supplemental)";

	return languageName;
}
// Function for sorting an array of objects containing a language name and dictionary filename
function languageNameDictFilenameSort(a, b)
{
	var retVal = 0;
	var generalWithSameLanguageNames = false;
	var genRegex = /^(.*) \(General\)$/;
	var matches1 = a.name.match(genRegex);
	var matches2 = b.name.match(genRegex);
	if ((matches1 != null) || (matches2 != null))
	{
		// Get the language names without the part in paranthesis
		var language1Name = (matches1 != null ? matches1[1] : a.name.substr(0, a.name.indexOf("(")-1));
		var language2Name = (matches2 != null ? matches2[1] : b.name.substr(0, b.name.indexOf("(")-1));
		// If the language names are the same, sort the (General) one before the others.
		if (language1Name == language2Name)
		{
			generalWithSameLanguageNames = true;
			if (matches1 != null)
				retVal = -1;
			else
				retVal = 1;
		}
	}
	if (!generalWithSameLanguageNames)
	{
		if (a.name < b.name)
			retVal = -1;
		else if (a.name > b.name)
			retVal = 1;
		// retVal will remain 0 if the names are equal
	}
	return retVal;
}

// Returns whether a language is selected in the user's settings
//
// Parameters:
//  pUserSettings: The user's settings object
//  pFilenameFullPath: The full path & filename of a dictionary file
//
// Return value: Boolean - Whether or not the language is enabled in the user's settings
function languageIsSelectedInUserSettings(pUserSettings, pFilenameFullPath)
{
	var dictionarySelected = false;
	for (var i = 0; (i < pUserSettings.dictionaryFilenames.length) && !dictionarySelected; ++i)
		dictionarySelected = (pFilenameFullPath == pUserSettings.dictionaryFilenames[i]);
	return dictionarySelected;
}

// Shortens a string, accounting for control/attribute codes.  Returns a new
// (shortened) copy of the string.
//
// Parameters:
//  pStr: The string to shorten
//  pNewLength: The new (shorter) length of the string
//  pFromLeft: Optional boolean - Whether to start from the left (default) or
//             from the right.  Defaults to true.
//
// Return value: The shortened version of the string
function shortenStrWithAttrCodes(pStr, pNewLength, pFromLeft)
{
	if (typeof(pStr) != "string")
		return "";
	if (typeof(pNewLength) != "number")
		return pStr;
	if (pNewLength >= console.strlen(pStr))
		return pStr;

	var fromLeft = (typeof(pFromLeft) == "boolean" ? pFromLeft : true);
	var strCopy = "";
	var tmpStr = "";
	var strIdx = 0;
	var lengthGood = true;
	if (fromLeft)
	{
		while (lengthGood && (strIdx < pStr.length))
		{
			tmpStr = strCopy + pStr.charAt(strIdx++);
			if (console.strlen(tmpStr) <= pNewLength)
				strCopy = tmpStr;
			else
				lengthGood = false;
		}
	}
	else
	{
		strIdx = pStr.length - 1;
		while (lengthGood && (strIdx >= 0))
		{
			tmpStr = pStr.charAt(strIdx--) + strCopy;
			if (console.strlen(tmpStr) <= pNewLength)
				strCopy = tmpStr;
			else
				lengthGood = false;
		}
	}
	return strCopy;
}

// Returns a string with text centered in a certain width.
//
// Paramters:
//  pWidth: The width to center the text in
//  pText: The text to center in the width
//
// Return value: A string pWidth wide with the text centered in the width
function centeredText(pWidth, pText)
{
	var givenText = pText;
	var textLen = console.strlen(givenText);
	if (textLen > pWidth)
	{
		givenText = shortenStrWithAttrCodes(givenText, pWidth);
		textLen = console.strlen(givenText);
	}
	var textX = Math.floor(pWidth / 2) - Math.floor(textLen/2);
	var textStr = format("%" + textX + "s", "") + givenText;
	var numSpacesRemaining = pWidth - console.strlen(textStr);
	textStr += format("%" + numSpacesRemaining + "s", "");
	return textStr;
}

// Finds start & end indexes of non-quote blocks in the message lines.
//
// Parameters:
//  pTextLines: The array of message lines
//
// Return value: An array of objects containing the following properties:
//               start: The start index of a non-quote block
//               end: One past the end index of the non-quote block
function findNonQuoteBlockIndexes(pTextLines)
{
	if (typeof(pTextLines) != "object")
		return [];
	if (pTextLines.length == 0)
		return [];
	// Edge case: If there's only one line and if it's a non-quote block, then
	// return an array with an element about it.
	else if (pTextLines.length == 1)
		return (pTextLines[0].isQuoteLine ? [] : [{ start: 0, end: 1}]);

	var nonQuoteBlockIdxes = [];
	var startIdx = 0;
	var lastEndIdx = 0;
	var inQuoteBlock = pTextLines[0].isQuoteLine;
	for (var i = 1; i < pTextLines.length; ++i)
	{
		if (pTextLines[i].isQuoteLine != inQuoteBlock)
		{
			if (pTextLines[i].isQuoteLine)
			{
				nonQuoteBlockIdxes.push({ start: startIdx, end: i });
				lastEndIdx = i;
			}
			else
				startIdx = i;
			inQuoteBlock = pTextLines[i].isQuoteLine;
		}
	}
	// Edge case: If the last line in the array is a non-quote block, then ensure
	// the last non-quote block is added to nonQuoteBlockIdxes
	if (!pTextLines[pTextLines.length-1].isQuoteLine)
		nonQuoteBlockIdxes.push({ start: startIdx, end: pTextLines.length });

	return nonQuoteBlockIdxes;
}

// Finds start & end indexes of quote blocks in the message lines.
//
// Parameters:
//  pTextLines: The array of message lines
//
// Return value: An array of objects containing the following properties:
//               start: The start index of a non-quote block
//               end: One past the end index of the non-quote block
//               prefix: The string prefix for the quote block
function findQuoteBlockIndexes(pTextLines)
{
	if (typeof(pTextLines) != "object")
		return [];
	if (pTextLines.length == 0)
		return [];
	// Edge case: If there's only one line and if it's a quote block, then
	// return an array with an element about it.
	else if (pTextLines.length == 1)
		return (pTextLines[0].isQuoteLine ? [{ start: 0, end: 1}] : []);

	var quoteBlockIdxes = [];
	var startIdx = 0;
	var lastEndIdx = 0;
	var inQuoteBlock = pTextLines[0].isQuoteLine;
	for (var i = 1; i < pTextLines.length; ++i)
	{
		if (pTextLines[i].isQuoteLine != inQuoteBlock)
		{
			if (!pTextLines[i].isQuoteLine)
			{
				quoteBlockIdxes.push({ start: startIdx, end: i });
				lastEndIdx = i;
			}
			else
				startIdx = i;
			inQuoteBlock = pTextLines[i].isQuoteLine;
		}
	}
	// Edge case: If the last line in the array is a quote block, then ensure
	// the last non-quote block is added to quoteBlockIdxes
	if (pTextLines[pTextLines.length-1].isQuoteLine)
		quoteBlockIdxes.push({ start: startIdx, end: pTextLines.length });

	// Find the common prefixes in each quote block
	for (var quoteBlockI = 0; quoteBlockI < quoteBlockIdxes.length; ++quoteBlockI)
	{
		var linePrefix = commonEditLinesPrefix(pTextLines, quoteBlockIdxes[quoteBlockI].start, quoteBlockIdxes[quoteBlockI].end, ">", true);
		quoteBlockIdxes[quoteBlockI].prefix = linePrefix;
	}

	return quoteBlockIdxes;
}

// Finds start & end indexes of quote blocks and non-quote blocks in the message lines.
//
// Parameters:
//  pTextLines: The array of message lines
//
// Return value: An object with the following properties:
//               quoteBlocks: An array of objects containing the following properties:
//                            start: The start index of a quote block
//                            end: One past the end index of the quote block
//                            prefix: The string prefix for the quote block
//               nonQuoteBlocks: An array of objects containing the following properties:
//                               start: The start index of a non-quote block
//                               end: One past the end index of the non-quote block
//               allBlocks: An array of objects containing information about all text
//                          blocks in order, with these properties:
//                          isQuoteBlock: Boolean - Whether or not the block is a quote block
//                          start: The start index of the block
//                          end: One past the end index of the block
function findQuoteAndNonQuoteBlockIndexes(pTextLines)
{
	var retObj = {
		quoteBlocks: findQuoteBlockIndexes(pTextLines),
		nonQuoteBlocks: findNonQuoteBlockIndexes(pTextLines),
		allBlocks: []
	};

	// Go through both the quote and non-quote blocks and populate allBlocks.
	// Then sort allBlocks (by start index, which should be goood enough to sort with).
	for (var i = 0; i < retObj.quoteBlocks.length; ++i)
		retObj.allBlocks.push({ isQuoteBlock: true, start: retObj.quoteBlocks[i].start, end: retObj.quoteBlocks[i].end, prefix: retObj.quoteBlocks[i].prefix });
	for (var i = 0; i < retObj.nonQuoteBlocks.length; ++i)
		retObj.allBlocks.push({ isQuoteBlock: false, start: retObj.nonQuoteBlocks[i].start, end: retObj.nonQuoteBlocks[i].end });
	retObj.allBlocks.sort(function(obj1, obj2) {
		var retVal = 0;
		if (obj1.start < obj2.start)
			retVal = -1;
		else if (obj1.start == obj2.start)
			retVal = 0;
		else if (obj1.start > obj2.start)
			retVal = 1;
		return retVal;
	});

	return retObj;
}

function commonPrefixUtil(pStr1, pStr2)
{
	var result = "";

	var n1 = pStr1.length;
	var n2 = pStr2.length;

	for (var i = 0, j = 0; (i <= n1 - 1) && (j <= n2 - 1); ++i, ++j)
	{
		if (pStr1.charAt(i) != pStr2.charAt(j))
			break;
		else
			result += pStr1.charAt(i);
	}

	return result;
}

function commonEditLinesPrefix(pEditLines, pStartIdx, pEndIdx, pLastPrefixChar, lastCharShouldBeSpace)
{
	if (pEditLines.length == 0)
		return "";

	if ((pStartIdx < 0) || (pStartIdx >= pEditLines.length) || (pEndIdx < 0) || (pEndIdx >= pEditLines.length))
		return "";

	// Look for the first non-blank line and set that as the prefix
	var prefix = "";
	for (var i = pStartIdx; (i < pEndIdx) && (prefix.length == 0); ++i)
	{
		if (pEditLines[i].text.length > 0)
			prefix = pEditLines[i].text;
	}
	for (var i = pStartIdx+1; i < pEndIdx; ++i)
	{
		if (pEditLines[i].text.length > 0)
			prefix = commonPrefixUtil(prefix, pEditLines[i].text);
	}

	if (typeof(pLastPrefixChar) == "string")
	{
		var idx = prefix.lastIndexOf(pLastPrefixChar);
		if (idx > -1)
			prefix = prefix.substr(0, idx+1);
	}

	if ((prefix.length > 0) && lastCharShouldBeSpace)
	{
		if (prefix.charAt(prefix.length-1) != " ")
			prefix += " ";
	}

	return prefix;
}

// Separates a text string and any Synchronet attributes in the string.
//
// Parameters:
//  pStr: A string, which might contain Synchronet attributes
//
// Return value: An object containing the following properties:
//               textWithoutAttrs: The string without any attribute codes
//               attrs: An object where the properties are indexes in pStr where Synchronet
//                      attributes exist, and the values are the Synchronet attribute codes at the index
function sepStringAndAttrCodes(pStr)
{
	var retObj = {
		textWithoutAttrs: "",
		attrs: {}
	};

	if (typeof(pStr) !== "string" || pStr.length == 0)
		return retObj;

	retObj.textWithoutAttrs = strip_ctrl(pStr);
	var attrRegex = /(\x01[krgybmcwhifn\-_01234567])+/ig;
	var match = attrRegex.exec(pStr);
	var prevMatchesLen = 0;
	while (match !== null)
	{
		//var startTextIdx = +(match.index);
		var startTextIdx = +(match.index) - prevMatchesLen;
		retObj.attrs[startTextIdx] = match[0];
		prevMatchesLen += match[0].length;
		match = attrRegex.exec(pStr);
	}
	return retObj;
}

// Finds the index of the first screen-printable character in a string, excluding Synchronet attribute codes.
//
// Parameters:
//  pStr: The string to check
//
// Return value: The index of the first screen-printable character in the string, or -1 if none is found.
function findFirstPrintableChar(pStr)
{
	var firstPrintableIdx = -1;
	var attrRegex = /(\x01[krgybmcwhifn\-_01234567])+/ig;
	var match = attrRegex.exec(pStr);
	if (match !== null)
	{
		if (+(match.index) == 0)
		{
			firstPrintableIdx = attrRegex.lastIndex;
			if (firstPrintableIdx >= pStr.length)
				firstPrintableIdx = -1;
		}
		else
			firstPrintableIdx = 0;
	}
	else
		firstPrintableIdx = (pStr.length > 0 ? 0 : -1);
	return firstPrintableIdx;
}

// Given a string of attribute characters, this function inserts the control code
// in front of each attribute character and returns the new string.
//
// Parameters:
//  pAttrCodeCharStr: A string of attribute characters (i.e., "YH" for yellow high)
//
// Return value: A string with the control character inserted in front of the attribute characters
function attrCodeStr(pAttrCodeCharStr)
{
	if (typeof(pAttrCodeCharStr) !== "string")
		return "";

	var str = "";
	// See this page for Synchronet color attribute codes:
	// http://wiki.synchro.net/custom:ctrl-a_codes
	for (var i = 0; i < pAttrCodeCharStr.length; ++i)
	{
		var currentChar = pAttrCodeCharStr.charAt(i);
		if (/[krgybmcwKRGYBMCWHhIiEeFfNn01234567]/.test(currentChar))
			str += "\x01" + currentChar;
	}
	return str;
}

// Prints a character, taking into account printing with UTF8 (with the P_UTF8 mode bit)
//
// Parameters:
//  pChar: The character to print
//  pmode: The mode bits (will be checked to see if P_UTF8 is specified)
function printCharConsideringUTF8(pChar, pmode)
{
	// Credit to Deuce for this code (this was seen in fseditor.js)
	if (Boolean(pmode & P_UTF8) || Boolean(pmode & P_AUTO_UTF8))
	{
		var encoded = utf8_encode(pChar.charCodeAt(0));
		for (var i = 0; i < encoded.length; ++i)
			console.putbyte(ascii(encoded[i]));
	}
	else
		console.putbyte(ascii(pChar));
}

// Prints a string, taking into account printing with UTF8 (with the P_UTF8 mode bit),
// as well as Synchronet attribute codes in the string.
//
// Parameters:
//  pStr: The string to print
//  pmode: The mode bits (will be checked to see if P_UTF8 is specified)
function printStrConsideringUTF8(pStr, pmode)
{
	// Look for Synchronet attributes in the string.  For any part of the string
	// that's not Synchronet attributes, use printCharConsideringUTF8() to print
	// each character, and print the Synchronet attributes with console.print().
	var theStr = pStr;
	var attrMatch = theStr.match(/(\x01[krgybmcwhifn\-_01234567])+/i);
	while (Array.isArray(attrMatch) && attrMatch.length > 0)
	{
		var stringPart = theStr.substr(0, attrMatch.index);
		for (var i = 0; i < stringPart.length; ++i)
			printCharConsideringUTF8(stringPart[i], pmode);
		console.print(attrMatch[0], pmode);
		// Remove the (leading) part of the string that we've just printed
		theStr = theStr.substr(attrMatch.index + attrMatch[0].length);
		attrMatch = theStr.match(/(\x01[krgybmcwhifn\-_01234567])+/i);
	}
	// If there's still some text left in the string, print it.
	if (theStr.length > 0)
	{
		for (var i = 0; i < theStr.length; ++i)
			printCharConsideringUTF8(theStr[i], pmode);
	}
}

// Translatees ASCII character codes (numeric) 128-255 to UTF-8 character codes (numeric)
function CP437CodeToUTF8Code(pCP437Code)
{
	// https://www.compart.com/en/unicode/charsets/IBM-Symbols
	var UTF8Code = 0;
	switch(pCP437Code)
	{
		case 128:
			UTF8Code = 0xC7;
			break;
		case 129: // https://www.compart.com/en/unicode/U+00FC
			UTF8Code = 0xFC;
			break;
		case 130:
			UTF8Code = 0xE9;
			break;
		case 131: // https://www.compart.com/en/unicode/U+00E2
			UTF8Code = 0xE2;
			break;
		case 132:
			UTF8Code = 0xE4;
			break;
		case 133:
			UTF8Code = 0xE0;
			break;
		case 134:
			UTF8Code = 0xE5;
			break;
		case 135:
			UTF8Code = 0xE7;
			break;
		case 136:
			UTF8Code = 0xEA;
			break;
		case 137:
			UTF8Code = 0xEB;
			break;
		case 138:
			UTF8Code = 0xE8;
			break;
		case 139:
			UTF8Code = 0xEF;
			break;
		case 140:
			UTF8Code = 0xEE;
			break;
		case 141:
			UTF8Code = 0xEC;
			break;
		case 142:
			UTF8Code = 0xC4;
			break;
		case 143:
			UTF8Code = 0xC5;
			break;
		case 144:
			UTF8Code = 0xC9;
			break;
		case 145:
			UTF8Code = 0xE6;
			break;
		case 146:
			UTF8Code = 0xC6;
			break;
		case 147:
			UTF8Code = 0xF4;
			break;
		case 148:
			UTF8Code = 0xF6;
			break;
		case 149:
			UTF8Code = 0xF2;
			break;
		case 150:
			UTF8Code = 0xFB;
			break;
		case 151:
			UTF8Code = 0xF9;
			break;
		case 152:
			UTF8Code = 0xFF;
			break;
		case 153:
			UTF8Code = 0xD6;
			break;
		case 154:
			UTF8Code = 0xDC;
			break;
		case 155:
			UTF8Code = 0xA2;
			break;
		case 156:
			UTF8Code = 0xA3;
			break;
		case 157:
			UTF8Code = 0xA5;
			break;
		case 158:
			UTF8Code = 0x20A7;
			break;
		case 159:
			UTF8Code = 0x192;
			break;
		case 160:
			UTF8Code = 0xE1;
			break;
		case 161:
			UTF8Code = 0xED;
			break;
		case 162:
			UTF8Code = 0xF3;
			break;
		case 163:
			UTF8Code = 0xFA;
			break;
		case 164:
			UTF8Code = 0xF1;
			break;
		case 165:
			UTF8Code = 0xD1;
			break;
		case 166:
			UTF8Code = 0xAA;
			break;
		case 167:
			UTF8Code = 0xBA;
			break;
		case 168:
			UTF8Code = 0xBF;
			break;
		// https://www.w3.org/TR/xml-entity-names/025.html
		case 169:
			UTF8Code = 0x250E;
			break;
		case 170:
			UTF8Code = 0x2513;
			break;
		case 176:
			UTF8Code = 0x2591;
			break;
		case 177:
			UTF8Code = 0x2592;
			break;
		case 178:
			UTF8Code = 0x2593;
			break;
		case 179:
			UTF8Code = 0x2502;
			break;
		case 180:
			UTF8Code = 0x2538;
			break;
		case 181:
			UTF8Code = 0x2561;
			break;
		case 182:
			UTF8Code = 0x2562;
			break;
		case 183:
			UTF8Code = 0x2556;
			break;
		case 184:
			UTF8Code = 0x2555;
			break;
		case 185:
			UTF8Code = 0x2563;
			break;
		case 186:
			UTF8Code = 0x2551;
			break;
		case 187:
			UTF8Code = 0x2557;
			break;
		case 188:
			UTF8Code = 0x255D;
			break;
		case 189:
			UTF8Code = 0x255C;
			break;
		case 190:
			UTF8Code = 0x255B;
			break;
		case 191:
			UTF8Code = 0x2510;
			break;
		case 192:
			UTF8Code = 0x2514;
			break;
		case 193:
			UTF8Code = 0x2534;
			break;
		case 194:
			UTF8Code = 0x2530;
			break;
		case 195:
			UTF8Code = 0x2520;
			break;
		case 196:
			UTF8Code = 0x2500;
			break;
		case 197:
			UTF8Code = 0x2542;
			break;
		case 198:
			UTF8Code = 0x255E;
			break;
		case 199:
			UTF8Code = 0x255F;
			break;
		case 200:
			UTF8Code = 0x255A;
			break;
		case 201:
			UTF8Code = 0x2554;
			break;
		case 202:
			UTF8Code = 0x2569;
			break;
		case 203:
			UTF8Code = 0x2566;
			break;
		case 204:
			UTF8Code = 0x2560;
			break;
		case 205:
			UTF8Code = 0x2550;
			break;
		case 206:
			UTF8Code = 0x256C;
			break;
		case 207:
			UTF8Code = 0x2567;
			break;
		case 208:
			UTF8Code = 0x2568;
			break;
		case 209:
			UTF8Code = 0x2564;
			break;
		case 210:
			UTF8Code = 0x2565;
			break;
		case 211:
			UTF8Code = 0x2559;
			break;
		case 212:
			UTF8Code = 0x2558;
			break;
		case 213:
			UTF8Code = 0x2552;
			break;
		case 214:
			UTF8Code = 0x2553;
			break;
		case 215:
			UTF8Code = 0x256B;
			break;
		case 216:
			UTF8Code = 0x256A;
			break;
		case 217:
			UTF8Code = 0x2518;
			break;
		case 218:
			UTF8Code = 0x250C;
			break;
		case 219:
			UTF8Code = 0x2588;
			break;
		case 220:
			UTF8Code = 0x2584;
			break;
		case 221:
			UTF8Code = 0x258C;
			break;
		case 222:
			UTF8Code = 0x2590;
			break;
		case 223:
			UTF8Code = 0x2580;
			break;
		case 224:
			UTF8Code = 0x3B1;
			break;
		case 225:
			UTF8Code = 0x3B2;
			break;
		case 226:
			UTF8Code = 0x393;
			break;
		case 227:
			UTF8Code = 0x3C0;
			break;
		case 228:
			UTF8Code = 0x3A3;
			break;
		case 229:
			UTF8Code = 0x3C3;
			break;
		case 230:
			UTF8Code = 0xB5;
			break;
		case 231:
			UTF8Code = 0x3C4;
			break;
		case 232:
			UTF8Code = 0x3A6;
			break;
		case 233:
			UTF8Code = 0x398;
			break;
		case 234:
			UTF8Code = 0x3A9;
			break;
		case 235:
			UTF8Code = 0x3B4;
			break;
		case 236:
			UTF8Code = 0x221E;
			break;
		case 237:
			UTF8Code = 0x3C6;
			break;
		case 238:
			UTF8Code = 0x3B5;
			break;
		case 239:
			UTF8Code = 0x2229;
			break;
		case 240:
			UTF8Code = 0x2261;
			break;
		case 241:
			UTF8Code = 0xB1;
			break;
		case 242:
			UTF8Code = 0x2265;
			break;
		case 243:
			UTF8Code = 0x2264;
			break;
		case 244:
			UTF8Code = 0x2320;
			break;
		case 245:
			UTF8Code = 0x2321;
			break;
		case 246:
			UTF8Code = 0xF7;
			break;
		case 247:
			UTF8Code = 0x2248;
			break;
		case 248:
			UTF8Code = 0xB0;
			break;
		case 249:
			UTF8Code = 0x2219;
			break;
		case 250:
			UTF8Code = 0xFB;
			break;
		case 251:
			UTF8Code = 0x221A;
			break;
		case 252:
			UTF8Code = 0x207F;
			break;
		case 253:
			UTF8Code = 0xB2;
			break;
		case 254:
			UTF8Code = 0x25A0;
			break;
		case 255:
			UTF8Code = 0xA0;
			break;
		default:
			UTF8Code = 0;
	}
	return String.fromCharCode(UTF8Code);
}

// Returns whether the SlyEdit configuration supports UTF-8 (using the user's editor
// code, assuming it's one of the variants of SlyEdit)
function userEditorCfgHasUTF8Enabled()
{
	if (gConfiguredCharset.length > 0) // Will not be read in versions of Synchronet below 3.20
		return (gConfiguredCharset == "UTF-8");
	else
	{
		if (typeof(xtrn_area.editor[user.editor]) === "object")
			return Boolean(xtrn_area.editor[user.editor].settings & XTRN_UTF8);
		else
			return false;
	}
}

// Gets the quote wrap settings for the user's configured external editor (which is assumed to be SlyEdit)
//
// Return value: An object containing the following properties:
//               quoteWrapEnabled: Boolean: Whether or not quote wrapping is enabled for the editor
//               quoteWrapCols: The number of columns to wrap quote lines
//  If the given editor code is not found, quoteWrapEnabled will be false and quoteWrapCols will be -1
function getEditorQuoteWrapCfgFromSCFG()
{
	var retObj = {
		quoteWrapEnabled: false,
		quoteWrapCols: -1
	};

	var editorCode = user.editor.toLowerCase();
	if (!xtrn_area.editor.hasOwnProperty(editorCode))
		return retObj;

	// Set up a cache so that we don't have to keep repeatedly parsing the Synchronet
	// config every time the user replies to a message
	if (typeof(getEditorQuoteWrapCfgFromSCFG.cache) === "undefined")
		getEditorQuoteWrapCfgFromSCFG.cache = {};
	// If we haven't looked up the quote wrap cols setting yet, then do so; otherwise, use the
	// cached setting.
	if (!getEditorQuoteWrapCfgFromSCFG.cache.hasOwnProperty(editorCode))
	{
		if (typeof(xtrn_area.editor[user.editor]) === "object" && Boolean(xtrn_area.editor[user.editor].settings & XTRN_QUOTEWRAP))
		{
			retObj.quoteWrapEnabled = true;
			retObj.quoteWrapCols = console.screen_columns - 1;

			// For Synchronet 3.20 and newer, read the quote wrap setting from xtrn.ini
			if (system.version_num >= 32000)
			{
				// The INI section for the editor should be something like [editor:SLYEDICE], and
				// it should have a quotewrap_cols property
				var xtrnIniFile = new File(system.ctrl_dir + "xtrn.ini");
				if (xtrnIniFile.open("r"))
				{
					var quoteWrapCols = xtrnIniFile.iniGetValue("editor:" + user.editor.toUpperCase(), "quotewrap_cols", 79);
					if (quoteWrapCols > 0)
						retObj.quoteWrapCols = quoteWrapCols;
					xtrnIniFile.close();
				}
			}
			else
			{
				// Synchronet below version 3.20: Read the quote wrap setting from xtrn.cnf
				var cnflib = load({}, "cnflib.js");
				var xtrnCnf = cnflib.read("xtrn.cnf");
				if (typeof(xtrnCnf) === "object")
				{
					for (var i = 0; i < xtrnCnf.xedit.length; ++i)
					{
						if (xtrnCnf.xedit[i].code.toLowerCase() == editorCode)
						{
							if (xtrnCnf.xedit[i].hasOwnProperty("quotewrap_cols"))
							{
								if (xtrnCnf.xedit[i].quotewrap_cols > 0)
									retObj.quoteWrapCols = xtrnCnf.xedit[i].quotewrap_cols;
							}
							break;
						}
					}
				}
			}
		}
		getEditorQuoteWrapCfgFromSCFG.cache[editorCode] = retObj;
	}
	else
		retObj = getEditorQuoteWrapCfgFromSCFG.cache[editorCode];

	return retObj;
}

// Replaces @-codes in a string and returns the new string.
//
// Parameters:
//  pStr: A string in which to replace @-codes
//
// Return value: A version of the string with @-codes interpreted
function replaceAtCodesInStr(pStr)
{
	if (typeof(pStr) != "string")
		return "";

	// This code was originally written by Deuce.  I updated it to check whether
	// the string returned by bbs.atcode() is null, and if so, just return
	// the original string.
	return pStr.replace(/@([^@]+)@/g, function(m, code) {
		var decoded = bbs.atcode(code);
		return (decoded != null ? decoded : "@" + code + "@");
	});
}

// Gets message information for the user's personal email
//
// Return value: An object with the following properties:
//  succeeded: Boolean - Whether or not this function successfully opened the messagebase
//             and got the information
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
function getPersonalMailInfoForUser()
{
	var retObj = {
		succeeded: false,
		lastMsg: -1,
		totalNumMsgs: 0,
		curMsgNum: -1,
		msgNumIsOffset: false
	};

	var msgbase = new MsgBase("mail");
	if (msgbase.open())
	{
		var msgIdxArray = msgbase.get_index();
		msgbase.close();
		if (msgIdxArray != null)
		{
			for (var i = 0; i < msgIdxArray.length; ++i)
			{
				var msgIsToUser = false;
				if (msgIdxArray[i].hasOwnProperty("to"))
				{
					if (msgIdxArray[i].to == user.number)
						msgIsToUser = true;
					else
					{
						msgIsToUser = (msgIdxArray[i].to == crc16_calc(user.handle.toLowerCase()) ||
						               msgIdxArray[i].to == crc16_calc(user.alias.toLowerCase()) ||
						               msgIdxArray[i].to == crc16_calc(user.name.toLowerCase()));
					}
				}
				if (msgIsToUser)
				{
					retObj.lastMsg = msgIdxArray[i].number;
					++retObj.totalNumMsgs;
					if (retObj.curMsgNum == -1 && !Boolean(msgIdxArray[i].attr & MSG_READ))
					{
						retObj.curMsgNum = msgIdxArray[i].number;
						retObj.msgNumIsOffset = false;
					}
				}
			}
			retObj.succeeded = true;
		}
	}

	return retObj;
}

// Displays a Frame object and handles the input loop for navigation until
// the user presses Q, Enter, or ESC To quit the input loop
//
// Parameters:
//  pFrame: The Frame object
//  pScrollbar: The Scrollbar object for the Frame
//  pFrameContentStr: The string content that was added to the Frame
//  pAdditionalQuitKeys: Optional - A string containing additional keys to quit the
//                       input loop.  This is case-sensitive.
//
// Return value: The last keypress/input from the user
function doFrameInputLoop(pFrame, pScrollbar, pFrameContentStr, pAdditionalQuitKeys)
{
	var checkAdditionalQuitKeys = (typeof(pAdditionalQuitKeys) === "string" && pAdditionalQuitKeys.length > 0);

	// Input loop for the frame to let the user scroll it
	var frameContentTopYOffset = 0;
	//var maxFrameYOffset = pFrameContentStr.split("\r\n").length - pFrame.height;
	var maxFrameYOffset = countOccurrencesInStr(pFrameContentStr, "\r\n") - pFrame.height;
	if (maxFrameYOffset < 0) maxFrameYOffset = 0;
	var userInput = "";
	var continueOn = true;
	do
	{
		pFrame.scrollTo(0, frameContentTopYOffset);
		pFrame.invalidate();
		pScrollbar.cycle();
		pFrame.cycle();
		pFrame.draw();
		// Note: getKeyWithESCChars() is defined in dd_lightbar_menu.js.
		userInput = getKeyWithESCChars(K_NOECHO|K_NOSPIN|K_NOCRLF, 30000).toUpperCase();
		if (userInput == KEY_UP)
		{
			if (frameContentTopYOffset > 0)
				--frameContentTopYOffset;
		}
		else if (userInput == KEY_DOWN)
		{
			if (frameContentTopYOffset < maxFrameYOffset)
				++frameContentTopYOffset;
		}
		else if (userInput == KEY_PAGEUP)
		{
			frameContentTopYOffset -= pFrame.height;
			if (frameContentTopYOffset < 0)
				frameContentTopYOffset = 0;
		}
		else if (userInput == KEY_PAGEDN)
		{
			frameContentTopYOffset += pFrame.height;
			if (frameContentTopYOffset > maxFrameYOffset)
				frameContentTopYOffset = maxFrameYOffset;
		}
		else if (userInput == KEY_HOME)
			frameContentTopYOffset = 0;
		else if (userInput == KEY_END)
			frameContentTopYOffset = maxFrameYOffset;

		// Check for whether to continue the input loop
		continueOn = (userInput != "Q" && userInput != KEY_ENTER && userInput != KEY_ESC);
		// If the additional quit keys does not contain the user's keypress, then continue
		// the input loop.
		// In other words, if the additional quit keys includes the user's keypress, then
		// don't continue.
		if (continueOn && checkAdditionalQuitKeys)
			continueOn = (pAdditionalQuitKeys.indexOf(userInput) < 0);
	} while (continueOn);

	return userInput;
}

// This function displays debug text at a given location on the screen, then
// moves the cursor back to a given location.
//
// Parameters:
//  pDebugX: The X lcoation of where to write the debug text
//  pDebugY: The Y lcoation of where to write the debug text
//  pText: The text to write at the debug location
//  pOriginalPos: An object with x and y properties containing the original cursor position
//  pClearDebugLineFirst: Whether or not to clear the debug line before writing the text
//  pPauseAfter: Can be a boolean (whether or not to pause after displaying the text) or a number (number of milliseconds to wait)
//  pPrintMode: Optional print mode. Defaults to P_NONE.
function displayDebugText(pDebugX, pDebugY, pText, pOriginalPos, pClearDebugLineFirst, pPauseAfter, pPrintMode)
{
	var printMode = (typeof(pPrintMode) === "number" ? pPrintMode : P_NONE);
	console.gotoxy(pDebugX, pDebugY);
	if (pClearDebugLineFirst)
		console.clearline();
	// Output the text
	console.print(pText, printMode);
	if (typeof(pPauseAfter) === "boolean" && pPauseAfter)
		console.pause();
	else if (typeof(pPauseAfter) === "number" && pPauseAfter > 0)
		mswait(pPauseAfter);
	if ((typeof(pOriginalPos) != "undefined") && (pOriginalPos != null))
		console.gotoxy(pOriginalPos);
}

// Counts the number of occurrences of a substring within a string
//
// Parameters:
//  pStr: The string to count occurences in
//  pSubstr: The string to look for within pStr
//
// Return value: The number of occurrences of pSubstr found in pStr
function countOccurrencesInStr(pStr, pSubstr)
{
	if (typeof(pStr) !== "string" || typeof(pSubstr) !== "string") return 0;
	if (pStr.length == 0 || pSubstr.length == 0) return 0;

	var count = 0;
	var strIdx = pStr.indexOf(pSubstr);
	while (strIdx > -1 && strIdx < pStr.length)
	{
		++count;
		strIdx = pStr.indexOf(pSubstr, strIdx+1);
	}
	return count;
}

// Pads a string so that it can be displayed in the center of
// the user's terminal width
//
// Parameters:
//  pStr: The string to be centered
//  pTrailingPadding: Optional - Whether or not to pad the end of the
//                    string with spaces to fill the width (-1). Defaults
//                    to false.
//
// Return value: The padded string
function getCenteredTextStr(pStr, pTrailingPadding)
{
	var screenLen = console.strlen(pStr);
	if (screenLen >= console.screen_columns - 1)
		return pStr;

	var padLen = Math.floor((console.screen_columns / 2) - (screenLen / 2)) - 1;
	var padding = format("%*s", padLen, "");
	var paddedStr = padding + pStr;

	var trailingPadding = (typeof(pTrailingPadding) === "boolean" ? pTrailingPadding : false);
	if (trailingPadding)
		paddedStr += format("%*s", console.screen_columns - console.strlen(paddedStr) - 1, "");

	return paddedStr;
}

// Returns a string to display a RIP 'title bar'.
//
// Parameters:
//  pAttr: The attribute slot number (integer)
//  pX: The bar's starting X coordinate
//  pY: The bar's starting Y coordinate
//  pPen: The pen/pattern value (4-digit string, i.e., "0000")
//  pTitle: The text to display
//  pTextColor: The 2-digit hex color (i.e., "0F")
//  pTextX: The text cursor position X coordinate
//  pTextY: The text cursor position Y coordinate
//
// Return value: The RIP string to represent the above
function RIPTitleBar(pAttr, pX, pY, pLineStyle, pPen, pTitle, pTextColor, pTextX, pTextY)
{
	function pad2(n) {
		return ("0" + n).slice(-2);
	}

	var rip = "!";
	rip += "|" + pAttr + "U";
	rip += pad2(pX) + pad2(pY);
	rip += "H";
	rip += "L" + pLineStyle;
	rip += "P" + pPen;
	rip += "|c" + pTextColor;   // Set drawing color for graphics text
	rip += "|@" + pad2(pTextX) + pad2(pTextY) + pTitle.replace(/~/g, "\\~") + "~";   // RIP_TEXT_XY: draw text at (x,y)

	// Older:
	/*
	var rip = "!";
	rip += "|" + pAttr + "U";
	rip += pad2(pX) + pad2(pY);
	rip += "H";
	rip += "L" + pLineStyle;
	rip += "P" + pPen;
	rip += "<>" + pTitle + "<>";
	rip += "|c" + pTextColor;
	rip += "|S" + pad2(pTextX) + pad2(pTextY);
	*/

	return rip;
}
