// This file defines functions & things used by both
// DDFileAreaChooser and DDMsgAreaChooser.

"use strict";

// Definitions for getAreaHeirarchy()
var DDAC_FILE_AREAS = 0;
var DDAC_MSG_AREAS = 1;


// Creates a file/message area heirarchy object. This function exists because
// we may want to build such a structure with name collapsing, and the result
// may be different than how the message/file areas are structured in the BBS
// configuration. The return value is an array which is a sort of recursive
// array structure of objects. Each entry in the array will be an object with
// a 'name' property and either an 'items' property if it has sub-items or a
// 'subItemObj' property if it's a sub-item object (either a Synchronet
// sub-board or file directory). The last item in the array chain will have
// the 'subItemObj' property. Note: The topLevelIdx properties refer to either
// the message group index or file library index, depending on which structure
// it's retreiving.
//
// Parameters:
//  pWhichAreas: Either DDAC_FILE_AREAS to get a file area heirarchy or DDAC_MSG_AREAS to
//               get a message area heirarchy
//  pCollapsing: Boolean - Whether or not to use directory collapsing
//  pCollapsingSeparator: The separator used to split file lib/dir names when using collapsing
function getAreaHeirarchy(pWhichAreas, pCollapsing, pCollapsingSeparator)
{
	// Determine whether to use the file areas or message areas
	var topLevelArray;
	var subArrayName;
	switch (pWhichAreas)
	{
		case DDAC_FILE_AREAS:
			topLevelArray = file_area.lib_list;
			subArrayName = "dir_list";
			break;
		case DDAC_MSG_AREAS:
		default:
			topLevelArray = msg_area.grp_list;
			subArrayName = "sub_list";
			break;
	}

	// Create the heirarchy object
	var areaHeirarchy = [];
	if (pCollapsing)
	{
		// First, check the library descriptions for strings before the separator character
		var topArrayItemsBeforeSeparator = {}; // Will be an object indexed by description with a count as the value
		for (var topLevelIdx = 0; topLevelIdx < topLevelArray.length; ++topLevelIdx)
		{
			var topArrayItemDesc = skipsp(truncsp(topLevelArray[topLevelIdx].description));
			var sepIdx = topLevelArray[topLevelIdx].description.indexOf(pCollapsingSeparator);
			if (sepIdx > -1)
			{
				var libDescBeforeSep = topLevelArray[topLevelIdx].description.substr(0, sepIdx);
				if (topArrayItemsBeforeSeparator.hasOwnProperty(libDescBeforeSep))
					topArrayItemsBeforeSeparator[libDescBeforeSep] += 1;
				else
					topArrayItemsBeforeSeparator[libDescBeforeSep] = 1;
			}
		}

		// A regular expression intended to be used for replacing all double instances
		// of the separator character with a single instance
		// Modifiers can be "gi" for global case insensitive
		var doubleSepCharGlobalRegex = new RegExp(quoteLiteralCharsForRegEx(pCollapsingSeparator + pCollapsingSeparator), "g");

		// Build the heirarchy
		// For each library, go through each directory
		var uniqueNumber = 0; // A unique number for each item (for sorting as 'none')
		for (var topLevelIdx = 0; topLevelIdx < topLevelArray.length; ++topLevelIdx)
		{
			for (var subArrayIdx = 0; subArrayIdx < topLevelArray[topLevelIdx][subArrayName].length; ++subArrayIdx)
			{
				// 1. Join the library name & directory name separated by a colon
				// 2. Split on colons into an array (of names)
				// 3. Go through the array of names and build the appropriate structure in areaHeirarchy.
				// TODO: Initially, I thought of having collapsing restricted to
				// areas where there are no spaces before or after the :, but that
				// isn't how I did it before..
				/*
				var topArrayAndSubItemName = topLevelArray[topLevelIdx].description + ":" + topLevelArray[topLevelIdx][subArrayName][subArrayIdx].description;
				var nameArray = splitStrNoSpacesBeforeSeparator(topArrayAndSubItemName, pCollapsingSeparator);
				*/
				var topArrayItemDesc = skipsp(truncsp(topLevelArray[topLevelIdx].description));
				var subArrayItemDesc = skipsp(truncsp(topLevelArray[topLevelIdx][subArrayName][subArrayIdx].description));
				var topArrayAndSubItemName = topArrayItemDesc + pCollapsingSeparator + subArrayItemDesc;
				var nameArray = removeEmptyStrsFromArray(splitStringOnSingleCharByItself(topArrayAndSubItemName, pCollapsingSeparator));
				var arrayToSearch = areaHeirarchy;
				// If the library description has the separator character and the first element
				// only appears once, then use the whole library name as one name
				var sepCountInLibDesc = countSubstrInStr(topArrayItemDesc, pCollapsingSeparator);
				var startIdx = 0;
				if (sepCountInLibDesc > 0 && topArrayItemsBeforeSeparator.hasOwnProperty(nameArray[0]) && topArrayItemsBeforeSeparator[nameArray[0]] == 1)
					startIdx += sepCountInLibDesc;
				for (var i = startIdx; i < nameArray.length; ++i)
				{
					//var name = skipsp(truncsp(nameArray[i]));
					var name = "";
					if (startIdx > 0 && i == startIdx)
						name = topArrayItemDesc;
					else
						name = skipsp(truncsp(nameArray[i]));
					// Replace any double instances of the separator character with
					// a single instance
					name = name.replace(doubleSepCharGlobalRegex, pCollapsingSeparator);
					// Look for this one in the heirarchy; if not found, add it.
					// Look for an entry in the array that matches the name and has its own "items" array
					var heirarchyIdx = -1;
					for (var j = 0; j < arrayToSearch.length; ++j)
					{
						if (arrayToSearch[j].name == name && arrayToSearch[j].hasOwnProperty("items"))
						{
								heirarchyIdx = j;
								break;
						}
					}
					if (heirarchyIdx > -1)
						arrayToSearch = arrayToSearch[heirarchyIdx].items;
					else
					{
						// If we're at the last name, add a subItemObj item; otherwise, add
						// an items array.
						if (i == nameArray.length - 1)
						{
							arrayToSearch.push(
							{
								name: name,
								altName: topLevelArray[topLevelIdx][subArrayName][subArrayIdx].name,
								topLevelIdx: topLevelIdx,
								parent: arrayToSearch,
								uniqueNumber: uniqueNumber++,
								subItemObj: topLevelArray[topLevelIdx][subArrayName][subArrayIdx]
							});
						}
						else
						{
							arrayToSearch.push(
							{
								name: name,
								altName: name,
								topLevelIdx: topLevelIdx,
								parent: arrayToSearch,
								uniqueNumber: uniqueNumber++,
								items: []
							});
							arrayToSearch = arrayToSearch[arrayToSearch.length-1].items;
						}
					}
				}
			}
		}
	}
	else
	{
		// No collapsing. Have areaHeirarchy match the lib/dir structure
		// configured on the BBS.
		for (var topLevelIdx = 0; topLevelIdx < topLevelArray.length; ++topLevelIdx)
		{
			areaHeirarchy.push(
			{
					name: topLevelArray[topLevelIdx].description,
					altName: topLevelArray[topLevelIdx].name,
					topLevelIdx: topLevelIdx,
					parent: arrayToSearch,
					uniqueNumber: uniqueNumber++,
					items: []
			});
			var libIdxInHeirarchy = areaHeirarchy.length - 1;
			for (var subArrayIdx = 0; subArrayIdx < topLevelArray[topLevelIdx][subArrayName].length; ++subArrayIdx)
			{
				areaHeirarchy[libIdxInHeirarchy].items.push(
				{
					name: topLevelArray[topLevelIdx][subArrayName][subArrayIdx].description,
					altName: topLevelArray[topLevelIdx][subArrayName][subArrayIdx].name,
					topLevelIdx: topLevelIdx,
					parent: arrayToSearch,
					uniqueNumber: uniqueNumber++,
					subItemObj: topLevelArray[topLevelIdx][subArrayName][subArrayIdx]
				});
			}
		}
	}
	// Ensure the heirarchy items (including arrays) have a 'sorted' property. Set them
	// to true; the sorting type can be considered to be unsorted
	//setSortedPropertyInStructure(areaHeirarchy, true);
	setSortedPropertyInStructure(areaHeirarchy, false);
	return areaHeirarchy;
}

// Returns the number of times a substring apears in a string
//
// Parameters:
//  pStr: The string to search
//  pSubstr: The substring inside the string to count
//
// Return: The number of times the substring appears in the string
function countSubstrInStr(pStr, pSubstr)
{
	var substrCount = 0;
	var substrIdx = pStr.indexOf(pSubstr);
	while (substrIdx > -1)
	{
		++substrCount;
		substrIdx = pStr.indexOf(pSubstr, substrIdx+1);
	}
	return substrCount;
}

// Removes empty strings from an array - Given an array,
// makes a new array with only the non-empty strings and returns it.
function removeEmptyStrsFromArray(pArray)
{
	var newArray = [];
	for (var i = 0; i < pArray.length; ++i)
	{
		if (pArray[i].length > 0 && !/^\s+$/.test(pArray[i]))
			newArray.push(pArray[i]);
	}
	return newArray;
}

// Splits a string on a single instance of a character (more than 1 repeated
// instances of the character in a row won't result in a split point)
//
// Parameters:
//  pStr: The string to be split
//  pSeparatorChar: The character to split the string with
//
// Return value: An array of substrings as a result of the split
function splitStringOnSingleCharByItself(pStr, pSeparatorChar)
{
	if (typeof(pStr) !== "string")
		return [];
	if (typeof(pSeparatorChar) !== "string")
		return [pStr];

	var strArray = [];
	var startIdx = 0;
	var colonIdx = indexOfOneCharByItself(pStr, pSeparatorChar, startIdx);
	while (colonIdx > -1 && startIdx < pStr.length)
	{
		strArray.push(pStr.substr(startIdx, colonIdx-startIdx));
		startIdx = colonIdx + 1;
		colonIdx = indexOfOneCharByItself(pStr, pSeparatorChar, startIdx);
	}
	if (colonIdx == -1 && startIdx < pStr.length)
		strArray.push(pStr.substr(startIdx));
	return strArray;
}

// Finds the (next) index of a single instance of a character (more than 1
// repeated instances of the character in a row will be ignored)
//
// Parameters:
//  pStr: The string to search
//  pChar: The character to find
//  pStartIdx: Optional - The starting index to search from. Defaults to 0.
//
// Return value: The index of the found character, or -1 if not found
function indexOfOneCharByItself(pStr, pChar, pStartIdx)
{
	var startIdx = 0;
	if (typeof(pStartIdx) === "number" && pStartIdx >= 0 && pStartIdx < pStr.length)
		startIdx = pStartIdx;
	var foundIdx = -1;
	var lastStrIdx = pStr.length - 1;

	for (var i = startIdx; i < pStr.length && foundIdx == -1; ++i)
	{
		if (pStr[i] == pChar)
		{
			if (pStr[i] == pChar)
			{
				if (i == lastStrIdx)
					foundIdx = i;
				else
				{
					if (pStr[i+1] == pChar)
						++i;
					else
						foundIdx = i;
				}
			}
		}
	}
	return foundIdx;
}

// Splits a string on a separator, except when there's a space after a
// separator; if there's a space after a the separator, both parts of the
// string before & after the separator are included as one item in the
// resulting array. For instance, "Magic: The Gathering:Information"
// would be split up such that "Magic: The Gathering" would be one string
// and "Information" would be another, whereas "Mirror:Simtel:DOS:Games"
// would all be split up into 4 strings
function splitStrNoSpacesBeforeSeparator(pStr, pSep)
{
	var strArray = [];
	var splitArray = pStr.split(pSep);
	for (var i = 0; i < splitArray.length; ++i)
	{
		if (i < splitArray.length-1 && splitArray[i+1].indexOf(" ") == 0)
			strArray.push(splitArray[i] + ":" + splitArray[++i]);
		else
			strArray.push(splitArray[i]);
	}
	return strArray;
}

// Adds backslash characters to a string to quote characters as needed
// to match the literal character in a regular expression
//
// Parameters:
//  pStr: The string to search
//
// Return value: The string with special regex characters back-quoted
function quoteLiteralCharsForRegEx(pStr)
{
	if (typeof(pStr) !== "string")
		return "";

	const specialChars = "^$.*+?()[]{}|\\";

	var newStr = "";
	for (var i = 0; i < pStr.length; ++i)
	{
		if (specialChars.indexOf(pStr[i]) > -1)
			newStr += "\\";
		newStr += pStr[i];
	}
	return newStr;
}

// Recursively sets the/a 'sorted' property in an area structure and any of its
// items
//
// Parameters:
//  pStructure: The area information structure (should have an "items" property, as
//              that's the level where it would also have a 'sorted' property)
//
//  pSorted: Boolean - Whether or not the structure is known to be sorted
function setSortedPropertyInStructure(pStructure, pSorted)
{
	if (Array.isArray(pStructure))
		pStructure.sorted = pSorted;
	else if (pStructure.hasOwnProperty("items"))
	{
		pStructure.sorted = pSorted;
		pStructure.items.sorted = pSorted;
		for (var i = 0; i < pStructure.items.length; ++i)
			setSortedPropertyInStructure(pStructure.items[i], pSorted);
	}
}

function findMatchingAreaInfoObjFromInfoObj(pInfoObj, pName, pAltName)
{
	var obj = null;
	if (pInfoObj.hasOwnProperty("name") && pInfoObj.hasOwnProperty("altName") && pName == pInfoObj.name && pAltName == pInfoObj.altName)
	{
		obj = pInfoObj;
	}
	else if (pInfoObj.hasOwnProperty("items"))
	{
		for (var i = 0; i < pInfoObj.items.length && obj == null; ++i)
			obj = findMatchingAreaInfoObjFromInfoObj(pInfoObj.items[i], pName, pAltName);
	}
	return obj;
}

/*
function findMatchingArrayOfStructuresInStructureArray(pTopLevelArray, pArrayToFind, namesInArrayToFind, altNamesInArrayToFind, topLevelIndexesInArrayToFind)
{
	var theArray = null;
	if (Array.isArray(pTopLevelArray) && Array.isArray(pArrayToFind))
	{
		if (!Array.isArray(namesInArrayToFind) && !Array.isArray(altNamesInArrayToFind) && !Array.isArray(topLevelIndexesInArrayToFind))
		{
			namesInArrayToFind = [];
			altNamesInArrayToFind = [];
			topLevelIndexesInArrayToFind = [];
			for (var j = 0; j < pArrayToFind.length; ++j)
			{
				if (pArrayToFind[j].hasOwnProperty("name") && pArrayToFind[j].hasOwnProperty("altName") && pArrayToFind[j].hasOwnProperty("topLevelIdx"))
				{
					namesInArrayToFind.push(pArrayToFind[j].name);
					altNamesInArrayToFind.push(pArrayToFind[j].altName);
					topLevelIndexesInArrayToFind.push(pArrayToFind[j].topLevelIdx);
				}
			}
		}

		for (var i = 0; i < pTopLevelArray.length && theArray == null; ++i)
		{
			if (pTopLevelArray[i].hasOwnProperty("items"))
			{
				for (var j = 0; j < pTopLevelArray[i].length && theArray == null; ++i)
				{
					var nameFound = namesInArrayToFind.indexOf(pTopLevelArray[i].items[j].name) > -1;
					var altNameFound = altNamesInArrayToFind.indexOf(pTopLevelArray[i].items[j].altName) > -1;
					var topLevelIndexFound = topLevelIndexesInArrayToFind.indexOf(pTopLevelArray[i].items[j].topLevelIdx) > -1;
					if (nameFound && altNameFound && topLevelIndexFound)
						theArray = pTopLevelArray[i].items[j];
					else
						theArray = findMatchingArrayOfStructuresInStructureArray(pTopLevelArray[i].items, pArrayToFind, namesInArrayToFind, altNamesInArrayToFind, topLevelIndexesInArrayToFind);
				}
			}
		}
	}
	return theArray;
}
*/

// Recursively finds an object in an area info array with a matching name, alternate name, and top-level index
function findMatchingStructureInStructureArray(pArray, pName, pAltName, pTopLevelIdx)
{
	var obj = null;
	for (var i = 0; i < pArray.length && obj == null; ++i)
	{
		if (pName == pArray[i].name && pAltName == pArray[i].altName && pTopLevelIdx == pArray[i].topLevelIdx)
			obj = pArray[i];
		else if (pArray[i].hasOwnProperty("items"))
			obj = findMatchingStructureInStructureArray(pArray[i].items, pName, pAltName, pTopLevelIdx);
	}
	/*
	if (obj == null)
	{
		for (var i = 0; i < pArray.length && obj == null; ++i)
		{
			if (pArray[i].hasOwnProperty("items"))
				obj = findMatchingStructureInStructureArray(pArray[i].items, pName, pAltName, pTopLevelIdx);
		}
	}
	*/
	return obj;
}

// For user settings with the traditional UI: Returns a string where for each word,
// the first letter will have one set of Synchronet attributes applied and the remainder of the word
// will have another set of Synchronet attributes applied
function colorFirstCharAndRemainingCharsInWords(pStr, pWordFirstCharAttrs, pWordRemainderAttrs)
{
	if (typeof(pStr) !== "string" || pStr.length == 0)
		return "";
	if (typeof(pWordFirstCharAttrs) !== "string" || typeof(pWordRemainderAttrs) !== "string")
		return pStr;

	var wordsArray = pStr.split(" ");
	for (var i = 0; i < wordsArray.length; ++i)
	{
		if (wordsArray[i] != " ")
			wordsArray[i] = "\x01n" + pWordFirstCharAttrs + wordsArray[i].substr(0, 1) + "\x01n" + pWordRemainderAttrs + wordsArray[i].substr(1);
	}
	return wordsArray.join(" ");
}

// For user settings with the traditional UI: Returns a string where for each word,
// the first letter will have one set of Synchronet attributes applied and the remainder of the word
// will have another set of Synchronet attributes applied
function printTradUserSettingOption(pOptNum, pStr, pWordFirstCharAttrs, pWordRemainderAttrs)
{
	printf("\x01c\x01h%d\x01g: %s\r\n", pOptNum, colorFirstCharAndRemainingCharsInWords(pStr, pWordFirstCharAttrs, pWordRemainderAttrs));
}

///////////////////////////////////////////////////////////////////////////////////
// ChoiceScrollbox stuff (this was copied from SlyEdit_Misc.js; maybe there's a better way to do this)

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
//  pCfgObj: The script/program configuration object (color settings are used)
//  pAddTCharsAroundTopText: Optional, boolean - Whether or not to use left & right T characters
//                           around the top border text.  Defaults to true.
// pReplaceTopTextSpacesWithBorderChars: Optional, boolean - Whether or not to replace
//                           spaces in the top border text with border characters.
//                           Defaults to false.
function ChoiceScrollbox(pLeftX, pTopY, pWidth, pHeight, pTopBorderText, pCfgObj,
                         pAddTCharsAroundTopText, pReplaceTopTextSpacesWithBorderChars)
{
	if (pCfgObj == null || typeof(pCfgObj) !== "object")
		pCfgObj = {};
	if (pCfgObj.colors == null || typeof(pCfgObj.colors) !== "object")
	{
		pCfgObj.colors = {
			listBoxBorder: "\x01n\x01g",
			listBoxBorderText: "\x01n\x01b\x01h",
			listBoxItemText: "\x01n\x01c",
			listBoxItemHighlight: "\x01n\x01" + "4\x01w\x01h"
		};
	}
	else
	{
		if (!pCfgObj.colors.hasOwnProperty("listBoxBorder"))
			pCfgObj.colors.listBoxBorder = "\x01n\x01g";
		if (!pCfgObj.colors.hasOwnProperty("listBoxBorderText"))
			pCfgObj.colors.listBoxBorderText = "\x01n\x01b\x01h";
		if (!pCfgObj.colors.hasOwnProperty("listBoxItemText"))
			pCfgObj.colors.listBoxItemText = "\x01n\x01c";
		if (!pCfgObj.colors.hasOwnProperty("listBoxItemHighlight"))
			pCfgObj.colors.listBoxItemHighlight = "\x01n\x01" + "4\x01w\x01h";
	}

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
				pTopBorderText = firstStrPart + "\x01n" + pCfgObj.colors.listBoxBorder;
				for (var i = 0; i < numSpaces; ++i)
					pTopBorderText += HORIZONTAL_SINGLE;
				pTopBorderText += "\x01n" + pCfgObj.colors.listBoxBorderText + lastStrPart;
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

	this.programCfgObj = pCfgObj;

	var minWidth = ChoiceScrollbox_MinWidth();

	this.dimensions = {
		topLeftX: pLeftX,
		topLeftY: pTopY,
		width: 0,
		height: pHeight,
		bottomRightX: 0,
		bottomRightY: 0
	};
	// Make sure the width is the minimum width
	if ((pWidth < 0) || (pWidth < minWidth))
		this.dimensions.width = minWidth;
	else
		this.dimensions.width = pWidth;
	this.dimensions.bottomRightX = this.dimensions.topLeftX + this.dimensions.width - 1;
	this.dimensions.bottomRightY = this.dimensions.topLeftY + this.dimensions.height - 1;

	// The text item array and member variables relating to it and the items
	// displayed on the screen during the input loop
	this.txtItemList = [];
	this.chosenTextItemIndex = -1;
	this.topItemIndex = 0;
	this.bottomItemIndex = 0;

	// Top border string
	var innerBorderWidth = this.dimensions.width - 2;
	// Calculate the maximum top border text length to account for the left/right
	// T chars and "Page #### of ####" text
	var maxTopBorderTextLen = innerBorderWidth - (pAddTCharsAroundTopText ? 21 : 19);
	if (console.strlen(pTopBorderText) > maxTopBorderTextLen)
		pTopBorderText = pTopBorderText.substr(0, maxTopBorderTextLen);
	this.topBorder = "\x01n" + pCfgObj.colors.listBoxBorder + UPPER_LEFT_SINGLE;
	if (addTopTCharsAroundText)
		this.topBorder += RIGHT_T_SINGLE;
	this.topBorder += "\x01n" + pCfgObj.colors.listBoxBorderText
	               + pTopBorderText + "\x01n" + pCfgObj.colors.listBoxBorder;
	if (addTopTCharsAroundText)
		this.topBorder += LEFT_T_SINGLE;
	const topBorderTextLen = console.strlen(pTopBorderText);
	var numHorizBorderChars = innerBorderWidth - topBorderTextLen - 20;
	if (addTopTCharsAroundText)
		numHorizBorderChars -= 2;
	for (var i = 0; i <= numHorizBorderChars; ++i)
		this.topBorder += HORIZONTAL_SINGLE;
	this.topBorder += RIGHT_T_SINGLE + "\x01n" + pCfgObj.colors.listBoxBorderText
	               + "Page    1 of    1" + "\x01n" + pCfgObj.colors.listBoxBorder + LEFT_T_SINGLE
	               + UPPER_RIGHT_SINGLE;

	// Bottom border string
	this.btmBorderNavText = "\x01n\x01h\x01c" + UP_ARROW + "\x01b, \x01c" + DOWN_ARROW + "\x01b, \x01cN\x01y)\x01bext, \x01cP\x01y)\x01brev, "
	                      + "\x01cF\x01y)\x01birst, \x01cL\x01y)\x01bast, \x01cHOME\x01b, \x01cEND\x01b, \x01cEnter\x01y=\x01bSelect, "
	                      + "\x01cESC\x01n\x01c/\x01h\x01cQ\x01y=\x01bEnd";
	this.bottomBorder = "\x01n" + pCfgObj.colors.listBoxBorder + LOWER_LEFT_SINGLE
	                  + RIGHT_T_SINGLE + this.btmBorderNavText + "\x01n" + pCfgObj.colors.listBoxBorder
	                  + LEFT_T_SINGLE;
	var numCharsRemaining = this.dimensions.width - console.strlen(this.btmBorderNavText) - 6;
	for (var i = 0; i < numCharsRemaining; ++i)
		this.bottomBorder += HORIZONTAL_SINGLE;
	this.bottomBorder += LOWER_RIGHT_SINGLE;

	// Item format strings
	this.listIemFormatStr = "\x01n" + pCfgObj.colors.listBoxItemText + "%-"
	                      + +(this.dimensions.width-2) + "s";
	this.listIemHighlightFormatStr = "\x01n" + pCfgObj.colors.listBoxItemHighlight + "%-"
	                               + +(this.dimensions.width-2) + "s";

	// Key functionality override function pointers
	this.enterKeyOverrideFn = null;

	// inputLoopeExitKeys is an object containing additional keypresses that will
	// exit the input loop.
	this.inputLoopExitKeys = {};

	// For drawing the menu
	this.pageNum = 0;
	this.numPages = 0;
	this.numItemsPerPage = 0;
	this.maxItemWidth = 0;
	this.pageNumTxtStartX = 0;

	// Input loop quit override (to be used in overridden enter function if needed to quit the input loop there
	this.continueInputLoopOverride = true;

	// Object functions
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
	this.drawInnerMenu = ChoiceScrollbox_DrawInnerMenu;
	this.refreshOnScreen = ChoiceScrollbox_RefreshOnScreen;
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
		if (console.strlen(pText) > innerWidth)
			pText = pText.substr(0, innerWidth);
	}

	// Re-build the bottom border string based on the new text
	this.bottomBorder = "\x01n" + this.programCfgObj.colors.listBoxBorder + LOWER_LEFT_SINGLE;
	if (pAddTChars)
		this.bottomBorder += RIGHT_T_SINGLE;
	if (pText.indexOf("\x01n") != 0)
		this.bottomBorder += "\x01n";
	this.bottomBorder += pText + "\x01n" + this.programCfgObj.colors.listBoxBorder;
	if (pAddTChars)
		this.bottomBorder += LEFT_T_SINGLE;
	var numCharsRemaining = this.dimensions.width - console.strlen(this.bottomBorder) - 1; // - 3
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
function ChoiceScrollbox_DrawInnerMenu(pSelectedIndex)
{
	var selectedIndex = (typeof(pSelectedIndex) == "number" ? pSelectedIndex : -1);
	var startArrIndex = this.pageNum * this.numItemsPerPage;
	var endArrIndex = startArrIndex + this.numItemsPerPage;
	if (endArrIndex > this.txtItemList.length)
		endArrIndex = this.txtItemList.length;
	var selectedItemRow = this.dimensions.topLeftY+1;
	var screenY = this.dimensions.topLeftY + 1;
	for (var i = startArrIndex; i < endArrIndex; ++i)
	{
		console.gotoxy(this.dimensions.topLeftX+1, screenY);
		if (i == selectedIndex)
		{
			printf(this.listIemHighlightFormatStr, this.txtItemList[i].substr(0, this.maxItemWidth));
			selectedItemRow = screenY;
		}
		else
			printf(this.listIemFormatStr, this.txtItemList[i].substr(0, this.maxItemWidth));
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
	console.gotoxy(this.pageNumTxtStartX, this.dimensions.topLeftY);
	printf("\x01n" + this.programCfgObj.colors.listBoxBorderText + "Page %4d of %4d", this.pageNum+1, this.numPages);
	return selectedItemRow;
}
function ChoiceScrollbox_RefreshOnScreen(pSelectedIndex)
{
	this.drawBorder();
	this.drawInnerMenu(pSelectedIndex);
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
		console.print(this.programCfgObj.colors.listBoxItemHighlight);
	else
		console.print(this.programCfgObj.colors.listBoxItemText);
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
	this.numItemsPerPage = this.dimensions.height - 2;
	this.topItemIndex = 0;    // The index of the message group at the top of the list
	// Figure out the index of the last message group to appear on the screen.
	this.bottomItemIndex = getBottommostItemIndex(this.txtItemList, this.topItemIndex, this.numItemsPerPage);
	this.numPages = Math.ceil(this.txtItemList.length / this.numItemsPerPage);
	const topIndexForLastPage = (this.numItemsPerPage * this.numPages) - this.numItemsPerPage;

	if (pDrawBorder)
		this.drawBorder();

	// User input loop
	// For the horizontal location of the page number text for the box border:
	// Based on the fact that there can be up to 9999 text replacements and 10
	// per page, there will be up to 1000 pages of replacements.  To write the
	// text, we'll want to be 20 characters to the left of the end of the border
	// of the box.
	this.pageNumTxtStartX = this.dimensions.topLeftX + this.dimensions.width - 19;
	this.maxItemWidth = this.dimensions.width - 2;
	this.pageNum = 0;
	var startArrIndex = 0;
	this.chosenTextItemIndex = retObj.selectedIndex = 0;
	var endArrIndex = 0; // One past the last array item
	var curpos = { // For keeping track of the current cursor position
		x: 0,
		y: 0
	};
	var refreshList = true; // For screen redraw optimizations
	this.continueInputLoopOverride = true;
	var continueOn = true;
	while (continueOn && this.continueInputLoopOverride)
	{
		if (refreshList)
		{
			this.bottomItemIndex = getBottommostItemIndex(this.txtItemList, this.topItemIndex, this.numItemsPerPage);

			// Write the list of items for the current page.  Also, drawInnerMenu()
			// will return the selected item row.
			var selectedItemRow = this.drawInnerMenu(retObj.selectedIndex);

			// Just for sane appearance: Move the cursor to the first character of
			// the currently-selected row and set the appropriate color.
			curpos.x = this.dimensions.topLeftX+1;
			curpos.y = selectedItemRow;
			console.gotoxy(curpos.x, curpos.y);
			console.print(this.programCfgObj.colors.listBoxItemHighlight);

			refreshList = false;
		}

		// Get a key from the user (upper-case) and take action based upon it.
		retObj.lastKeypress = console.getkey(K_UPPER|K_NOCRLF|K_NOSPIN);
		if (console.aborted)
			break;
		switch (retObj.lastKeypress)
		{
			case 'N': // Next page
			case KEY_PAGEDN:
				//if (user.is_sysop) console.print("\x01n\r\nMenu page down pressed\r\n\x01p"); // Temproary;
				refreshList = (this.pageNum < this.numPages-1);
				if (refreshList)
				{
					++this.pageNum;
					this.topItemIndex += this.numItemsPerPage;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				else if (retObj.selectedIndex < this.bottomItemIndex)
				{
					// Go to the last item
					// Display the current line un-highlighted
					console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
					printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					// Select the bottommost item, and display it highlighted.
					this.chosenTextItemIndex = retObj.selectedIndex = this.bottomItemIndex;
					curpos.x = this.dimensions.topLeftX+1;
					curpos.y = this.dimensions.bottomRightY-1;
					console.gotoxy(curpos);
					printf(this.listIemHighlightFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					console.gotoxy(curpos); // Move the cursor into place where it should be
					refreshList = false;
				}
				break;
			case 'P': // Previous page
			case KEY_PAGEUP:
				refreshList = (this.pageNum > 0);
				if (refreshList)
				{
					--this.pageNum;
					this.topItemIndex -= this.numItemsPerPage;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				else if (retObj.selectedIndex > this.topItemIndex)
				{
					// Go to the first item
					// Display the current line un-highlighted
					console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
					printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					// Select the top item, and display it highlighted.
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					curpos.x = this.dimensions.topLeftX+1;
					curpos.y = this.dimensions.topLeftY+1;
					console.gotoxy(curpos);
					printf(this.listIemHighlightFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					console.gotoxy(curpos); // Move the cursor into place where it should be
					refreshList = false;
				}
				break;
			case 'F': // First page
				refreshList = (this.pageNum > 0);
				if (refreshList)
				{
					this.pageNum = 0;
					this.topItemIndex = 0;
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					// Note: this.bottomItemIndex is refreshed at the top of the loop
				}
				break;
			case 'L': // Last page
				refreshList = (this.pageNum < this.numPages-1);
				if (refreshList)
				{
					this.pageNum = this.numPages-1;
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
						--this.pageNum;
						this.topItemIndex -= this.numItemsPerPage;
						// Note: this.bottomItemIndex is refreshed at the top of the loop
						refreshList = true;
					}
					else
					{
						// Display the current line un-highlighted
						console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
						printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
						// Display the previous line highlighted
						curpos.x = this.dimensions.topLeftX+1;
						--curpos.y;
						console.gotoxy(curpos);
						printf(this.listIemHighlightFormatStr, this.txtItemList[previousItemIndex].substr(0, this.maxItemWidth));
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
						++this.pageNum;
						this.topItemIndex += this.numItemsPerPage;
						// Note: this.bottomItemIndex is refreshed at the top of the loop
						refreshList = true;
					}
					else
					{
						// Display the current line un-highlighted
						console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
						printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
						// Display the previous line highlighted
						curpos.x = this.dimensions.topLeftX+1;
						++curpos.y;
						console.gotoxy(curpos);
						printf(this.listIemHighlightFormatStr, this.txtItemList[nextItemIndex].substr(0, this.maxItemWidth));
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
					printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					// Select the top item, and display it highlighted.
					this.chosenTextItemIndex = retObj.selectedIndex = this.topItemIndex;
					curpos.x = this.dimensions.topLeftX+1;
					curpos.y = this.dimensions.topLeftY+1;
					console.gotoxy(curpos);
					printf(this.listIemHighlightFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					console.gotoxy(curpos); // Move the cursor into place where it should be
					refreshList = false;
				}
				break;
			case KEY_END: // Go to the last row in the box
				if (retObj.selectedIndex < this.bottomItemIndex)
				{
					// Display the current line un-highlighted
					console.gotoxy(this.dimensions.topLeftX+1, curpos.y);
					printf(this.listIemFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
					// Select the bottommost item, and display it highlighted.
					this.chosenTextItemIndex = retObj.selectedIndex = this.bottomItemIndex;
					curpos.x = this.dimensions.topLeftX+1;
					curpos.y = this.dimensions.bottomRightY-1;
					console.gotoxy(curpos);
					printf(this.listIemHighlightFormatStr, this.txtItemList[retObj.selectedIndex].substr(0, this.maxItemWidth));
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
			case "": // User input timeout
				console.attributes = "N";
				console.print(bbs.text(bbs.text.CallBackWhenYoureThere));
				bbs.hangup();
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

	this.continueInputLoopOverride = true; // Reset

	console.attributes = "N"; // To prevent outputting highlight colors, etc..
	return retObj;
}

///////////////////////////////////////////////////////////////////////////////////
