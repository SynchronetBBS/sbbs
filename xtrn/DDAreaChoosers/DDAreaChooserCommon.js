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
		var doubleSepCharGlobalRegex = new RegExp(pCollapsingSeparator + pCollapsingSeparator, "g"); // "gi" for global case insensitive

		// Build the heirarchy
		// For each library, go through each directory
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
				var topArrayAndSubItenMane = topLevelArray[topLevelIdx].description + ":" + topLevelArray[topLevelIdx][subArrayName][subArrayIdx].description;
				var nameArray = splitStrNoSpacesBeforeSeparator(topArrayAndSubItenMane, pCollapsingSeparator);
				*/
				var topArrayItemDesc = skipsp(truncsp(topLevelArray[topLevelIdx].description));
				var subArrayItemDesc = skipsp(truncsp(topLevelArray[topLevelIdx][subArrayName][subArrayIdx].description));
				var topArrayAndSubItenMane = topArrayItemDesc + pCollapsingSeparator + subArrayItemDesc;
				var nameArray = removeEmptyStrsFromArray(splitStringOnSingleCharByItself(topArrayAndSubItenMane, pCollapsingSeparator));
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
					subItemObj: topLevelArray[topLevelIdx][subArrayName][subArrayIdx]
				});
			}
		}
	}
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
