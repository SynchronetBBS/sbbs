// This file defines the ChoiceScrollbox class, which is meant
// to provide a scrollable lightbar-based dialog for user settings.
// This is used in several Digital Distortion mods (SlyEdit,
// DDMsgReader, message/file area choosers, etc.). This requires
// an ANSI terminal.
//
// To use this, you can create a ChoiceScrollbox with the constructor
// by calling 'new ChoiceScrollbox' with the appropriate parameters.
// To show the dialog and let the user work with it, call the doInputLoop()
// method.  If you want to handle certain extra keypresses while showing
// the menu, you can add additional 'quit' keys by calling the
// addInputLoopExitKey() method and providing it with a key; then, the
// ChoiceScrollbox dialog will exit its input loop when the user presses
// that key - You can then handle it, and then call doInputLoop() again if
// you want to keep showing the choice scroobox.
//
// You can see SlyEdit.js (in sbbs/exec) for an example of using this.
//
// Author: Eric Oulashin (AKA Nightfox)
// BBS: Digital Distortion
// BBS address: digitaldistortionbbs.com (or digdist.synchro.net)

"use strict";

require("cp437_defs.js", "CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE");

// Returns the minimum width for a ChoiceScrollbox. This
// is arbitrary for now.
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
	var addTopTCharsAroundText = (typeof(pAddTCharsAroundTopText) === "boolean" ? pAddTCharsAroundTopText : true);
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
		while (firstSpcIdx > -1 && nonSpcIdx > -1)
		{
			firstStrPart = pTopBorderText.substr(startIdx, (firstSpcIdx-startIdx));
			lastStrPart = pTopBorderText.substr(nonSpcIdx);
			numSpaces = nonSpcIdx - firstSpcIdx;
			if (numSpaces > 0)
			{
				pTopBorderText = firstStrPart + "\x01n" + pCfgObj.colors.listBoxBorder;
				for (var i = 0; i < numSpaces; ++i)
					pTopBorderText += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
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
	this.topBorder = "\x01n" + pCfgObj.colors.listBoxBorder + CP437_BOX_DRAWINGS_UPPER_LEFT_SINGLE;
	if (addTopTCharsAroundText)
		this.topBorder += CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT;
	this.topBorder += "\x01n" + pCfgObj.colors.listBoxBorderText
	               + pTopBorderText + "\x01n" + pCfgObj.colors.listBoxBorder;
	if (addTopTCharsAroundText)
		this.topBorder += CP437_BOX_DRAWINGS_LIGHT_LEFT_T;
	const topBorderTextLen = console.strlen(pTopBorderText);
	var numHorizBorderChars = innerBorderWidth - topBorderTextLen - 20;
	if (addTopTCharsAroundText)
		numHorizBorderChars -= 2;
	for (var i = 0; i <= numHorizBorderChars; ++i)
		this.topBorder += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	this.topBorder += CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT + "\x01n" + pCfgObj.colors.listBoxBorderText
	               + "Page    1 of    1" + "\x01n" + pCfgObj.colors.listBoxBorder + CP437_BOX_DRAWINGS_LIGHT_LEFT_T
	               + CP437_BOX_DRAWINGS_UPPER_RIGHT_SINGLE;

	// Bottom border string
	this.btmBorderNavText = "\x01n\x01h\x01c" + UP_ARROW + "\x01b, \x01c" + DOWN_ARROW + "\x01b, \x01cN\x01y)\x01bext, \x01cP\x01y)\x01brev, "
	                      + "\x01cF\x01y)\x01birst, \x01cL\x01y)\x01bast, \x01cHOME\x01b, \x01cEND\x01b, \x01cEnter\x01y=\x01bSelect, "
	                      + "\x01cESC\x01n\x01c/\x01h\x01cQ\x01y=\x01bEnd";
	this.bottomBorder = "\x01n" + pCfgObj.colors.listBoxBorder + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE
	                  + CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT + this.btmBorderNavText + "\x01n" + pCfgObj.colors.listBoxBorder
	                  + CP437_BOX_DRAWINGS_LIGHT_LEFT_T;
	var numCharsRemaining = this.dimensions.width - console.strlen(this.btmBorderNavText) - 6;
	for (var i = 0; i < numCharsRemaining; ++i)
		this.bottomBorder += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	this.bottomBorder += CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE;

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
function ChoiceScrollbox_SetBottomBorderText(pText, pAddTChars, pAutoStripIfTooLong, pCenter)
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
	var maxWidth = this.dimensions.width - 2; // -2 for the corner characters
	if (pAddTChars)
		maxWidth -= 2; // -2 for the T characters
	this.bottomBorder = "\x01n" + this.programCfgObj.colors.listBoxBorder + CP437_BOX_DRAWINGS_LOWER_LEFT_SINGLE;
	if (pCenter)
	{
		var numCharsToAdd = Math.floor(maxWidth / 2) - Math.floor(console.strlen(pText)/2);
		if (numCharsToAdd > 0)
		{
			for (var i = 0; i < numCharsToAdd; ++i)
				this.bottomBorder += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
		}
	}
	if (pAddTChars)
		this.bottomBorder += CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT;
	if (pText.indexOf("\x01n") != 0)
		this.bottomBorder += "\x01n";
	this.bottomBorder += pText + "\x01n" + this.programCfgObj.colors.listBoxBorder;
	if (pAddTChars)
		this.bottomBorder += CP437_BOX_DRAWINGS_LIGHT_LEFT_T;
	var numCharsRemaining = this.dimensions.width - console.strlen(this.bottomBorder) - 1; // - 3
	for (var i = 0; i < numCharsRemaining; ++i)
		this.bottomBorder += CP437_BOX_DRAWINGS_HORIZONTAL_SINGLE;
	this.bottomBorder += CP437_BOX_DRAWINGS_LOWER_RIGHT_SINGLE;
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
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
		console.gotoxy(this.dimensions.bottomRightX, screenRow);
		console.print(CP437_BOX_DRAWINGS_LIGHT_VERTICAL);
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
		lastKeypress: "",
		userQuit: false
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
				retObj.userQuit = true;
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
					retObj.userQuit = true;
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
