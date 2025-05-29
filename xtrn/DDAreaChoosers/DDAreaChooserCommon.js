// This file defines functions & things used by both
// DDFileAreaChooser and DDMsgAreaChooser.

"use strict";

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
	while (colonIdx > -1)
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
