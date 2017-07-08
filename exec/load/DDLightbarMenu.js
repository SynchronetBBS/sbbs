// $Id$

/* Digital Distortion Lightbar Menu library
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * Addresses: digitaldistortionbbs.com
 *            digdist.bbsindex.com
 *            digdist.synchro.net

This is a lightbar menu library.  This allows creating a scrollable menu of
text items for the user to choose from.  The user can naviate the list using
the up & down arrows, PageUp, PageDown, Home, and End keys.  The enter key
selects an item.  The ESC key will exit the menu and return null.
This menu library requires the use of an ANSI terminal.
By default, this menu library does not display a border around the menu.
If you want this library to draw a border around the menu, you can set the
borderStyle property to one of the following:
BORDER_SINGLE: A single-line border
BORDER_DOUBLE: A double-line border
There is also BORDER_NONE, which specifies to not display a border around the
menu.  Without a border, the menu gains 2 characters of width and 2 lines of
height.

This script provides an object, DDLightbarMenu.  Use the DDLightbarMenu
constructor to create the object.  Some other notable methods:
Add()
SetItemHotkey()
SetPos()
SetSize()
GetVal()

To change the colors used for displaying the items, you can change the values
in the colors object within the DDLightbarMenu object.  These are the current
supported colors:
itemColor: The color to use for non-selected items (current default is white
           on blue)
selectedItemColor: The color to use for selected items (current default is blue
                   on white)
itemTextCharHighlightColor: The color of a highlighted non-space character in an
                            item text (specified by having a & in the item text).
							It's important not to specify a "\1n" in here in case
							the item text should have a background color.
borderColor: The color for the borders (if borders are enabled)

By default, the menu selection will wrap around to the beginning/end when using
the down/up arrows.  That behavior can be disabled by setting the wrapNavigation
property to false.  Also, by default, there will be no borders.

If you want hotkeys to be case-sensitive, you can set the hotkeyCaseSensitive
property to true (it is false by default).  For example:
lbMenu.hotkeyCaseSensitive = true;

Example usage:
load("DDLightbarMenu.js");
// Create a menu at position 1, 3 with width 45 and height of 10
var lbMenu = new DDLightbarMenu(1, 3, 45, 10);
// Add 12 items to the menu, each of which will return the text of the item
for (var i = 0; i < 12; ++i)
	lbMenu.Add("Item " + +(i+1), "Item " + +(i+1));
// Set up the hotkey "s" to select the 2nd item
lbMenu.SetItemHotkey(1, "s");
// Show the menu and get the chosen item from the user
var val = lbMenu.GetVal();
// Output the chosen menu item
console.print("\1n\r\n");
console.print("Value:" + val + ":, type: " + typeof(val) + "\r\n");
console.pause();

// Changing the normal item color to green & selected item color to bright green:
lbMenu.colors.itemColor = "\1n\1g";
lbMenu.colors.selectedItemColor = "\1n\1h\1g";

// Disabling the navigation wrap behavior:
lbMenu.wrapNavigation = false;

// If you want a particular character in an item's text highlighted with
// a different color, you can put a & character immediately before it, as
// long as it's not a space.  For instance, to highlight the "x" in "Exit":
lbMenu.Add("E&xit", -1);
*/

load("sbbsdefs.js");

// Keyboard keys
var KEY_ESC = ascii(27);
var KEY_ENTER = "\x0d";
// PageUp & PageDown keys - Not real key codes, but codes defined
// to be used & recognized in this script
var KEY_PAGE_UP = "\1PgUp";
var KEY_PAGE_DOWN = "\1PgDn";

// Box-drawing/border characters: Single-line
var UPPER_LEFT_SINGLE = "\xDA";
var HORIZONTAL_SINGLE = "\xC4";
var UPPER_RIGHT_SINGLE = "\xBF";
var VERTICAL_SINGLE = "\xB3";
var LOWER_LEFT_SINGLE = "\xC0";
var LOWER_RIGHT_SINGLE = "\xD9";
var T_SINGLE = "\xC2";
var LEFT_T_SINGLE = "\xC3";
var RIGHT_T_SINGLE = "\xB4";
var BOTTOM_T_SINGLE = "\xC1";
var CROSS_SINGLE = "\xC5";
// Box-drawing/border characters: Double-line
var UPPER_LEFT_DOUBLE = "\xC9";
var HORIZONTAL_DOUBLE = "\xCD";
var UPPER_RIGHT_DOUBLE = "\xBB";
var VERTICAL_DOUBLE = "\xBA";
var LOWER_LEFT_DOUBLE = "\xC8";
var LOWER_RIGHT_DOUBLE = "\xBC";
var T_DOUBLE = "\xCB";
var LEFT_T_DOUBLE = "\xCC";
var RIGHT_T_DOUBLE = "\xB9";
var BOTTOM_T_DOUBLE = "\xCA";
var CROSS_DOUBLE = "\xCE";
// Box-drawing/border characters: Vertical single-line with horizontal double-line
var UPPER_LEFT_VSINGLE_HDOUBLE = "\xD5";
var UPPER_RIGHT_VSINGLE_HDOUBLE = "\xB8";
var LOWER_LEFT_VSINGLE_HDOUBLE = "\xD4";
var LOWER_RIGHT_VSINGLE_HDOUBLE = "\xBE";

// Border types for a menu
var BORDER_NONE = 0;
var BORDER_SINGLE = 1;
var BORDER_DOUBLE = 2;

// DDLightbarMenu object contstructor
//
// Parameters:
//  pX: Optional - The column (X) of the upper-left corner.  Defaults to 1.
//  pY: Optional - The row (Y) of the upper-left corner.  Defaults to 1.
//  pWidth: Optional - The width of the menu.  Defaults to 45.
//  pHeight: Optional - The height of the menu.  Defaults to 10.
function DDLightbarMenu(pX, pY, pWidth, pHeight)
{
	// Data members
	this.items = [];
	this.pos = {
		x: 1,
		y: 1
	};
	this.size = {
		width: 45,
		height: 10
	};
	this.showScrollbar = false;
	this.borderStyle = BORDER_NONE;
	this.colors = {
		itemColor: "\1n\1w\1" + "4",
		selectedItemColor: "\1n\1b\1" + "7",
		itemTextCharHighlightColor: "\1y\1h",
		borderColor: "\1n\1b"
	};
	this.selectedItemIdx = 0;
	this.topItemIdx = 0;
	this.wrapNavigation = true;
	this.hotkeyCaseSensitive = false;

	// Member functions
	this.Add = DDLightbarMenu_Add;
	this.Remove = DDLightbarMenu_Remove;
	this.RemoveAllItems = DDLightbarMenu_RemoveAllItems;
	this.SetPos = DDLightbarMenu_SetPos;
	this.SetSize = DDLightbarMenu_SetSize;
	this.SetWidth = DDLightbarMenu_SetWidth;
	this.SetHeight = DDLightbarMenu_SetHeight;
	this.Draw = DDLightbarMenu_Draw;
	this.DrawBorder = DDLightbarMenu_DrawBorder;
	this.WriteItem = DDLightbarMenu_WriteItem;
	this.Erase = DDLightbarMenu_Erase;
	this.SetItemHotkey = DDLightbarMenu_SetItemHotkey;
	this.RemoveItemHotkey = DDLightbarMenu_RemoveItemHotkey;
	this.RemoveAllItemHotkeys = DDLightbarMenu_RemoveAllItemHotkeys;
	this.GetVal = DDLightbarMenu_GetVal;

	// Set some things based on the parameters passed in
	if ((typeof(pX) == "number") && (typeof(pY) == "number"))
		this.SetPos(pX, pY);
	if (typeof(pWidth) == "number")
		this.SetWidth(pWidth);
	if (typeof(pHeight) == "number")
		this.SetHeight(pHeight);
}

// Adds an item to the menu
//
// Parameters:
//  pText: The text of the menu item
//  pRetval: The value to return when the item is chosen.  Can be any type of value.
//  pHotkey: Optional - A key to select the item when pressed by the user
function DDLightbarMenu_Add(pText, pRetval, pHotkey)
{
	var item = {
		text: pText,
		retval: pRetval
	};
	if (pRetval == undefined)
		item.retval = this.items.length;
	if (typeof(pHotkey) == "string")
		item.hotkey = pHotkey;
	this.items.push(item);
}

// Removes an item
//
// Parameters:
//  pIdx: The index of the item to remove
function DDLightbarMenu_Remove(pIdx)
{
	if ((typeof(pIdx) != "number") || (pIdx < 0) || (pIdx >= this.items.length))
		return; // pIdx is invalid

	this.items.splice(pIdx, 1);
	if (this.items.length > 0)
	{
		if (this.selectedItemIdx >= this.items.length)
			this.selectedItemIdx = this.items.length - 1;
	}
	else
	{
		this.selectedItemIdx = 0;
		this.topItemIdx = 0;
	}
}

// Removes all items
function DDLightbarMenu_RemoveAllItems()
{
	this.items = [];
	this.selectedItemIdx = 0;
	this.topItemIdx = 0;
}

// Sets the menu's upper-left corner position
//
// Parameters:
//  pX: The column (X) of the upper-left corner.
//  pY: The row (Y) of the upper-left corner.
function DDLightbarMenu_SetPos(pX, pY)
{
	if (typeof(pX) == "object")
	{
		if (pX.hasOwnProperty("x") && pX.hasOwnProperty("y"))
		{
			this.pos.x = pX.x;
			this.pos.y = pX.y;
		}
		
	}
	else if ((typeof(pX) == "number") && (typeof(pY) == "number"))
	{
		this.pos.x = pX;
		this.pos.y = pY;
	}
}

// Sets the menu's size.
//
// Parameters:
//  pSize: An object containing 'width' and 'height' members (numeric)
function  DDLightbarMenu_SetSize(pSize)
{
	if (typeof(pSize) == "object")
	{
		if (pSize.hasOwnProperty("width") && pSize.hasOwnProperty("height") && (typeof(pSize.width) == "number") && (typeof(pSize.height) == "number"))
		{
			if ((pSize.width > 0) && (pSize.width <= console.screen_columns))
				this.size.width = pSize.width;
			if ((pSize.height > 0) && (pSize.height <= console.screen_rows))
				this.size.height = pSize.height;
		}
	}
}

// Sets the menu's width
//
// Parameters:
//  pWidth: The width of the menu
function DDLightbarMenu_SetWidth(pWidth)
{
	if (typeof(pWidth) == "number")
	{
		if ((pWidth > 0) && (pWidth <= console.screen_columns))
			this.size.width = pWidth;
	}
}

// Sets the height of the menu
//
// Parameters:
//  pHeight: The height of the menu
function DDLightbarMenu_SetHeight(pHeight)
{
	if (typeof(pHeight) == "number")
	{
		if ((pHeight > 0) && (pHeight <= console.screen_rows))
			this.size.height = pHeight;
	}
}

// Draws the menu with all menu items.  The selected item will be highlighted.
function DDLightbarMenu_Draw()
{
	var itemLen = (this.showScrollbar ? this.size.width - 1 : this.size.width);
	var curPos = { x: this.pos.x, y: this.pos.y }; // For writing the menu items
	// If there is a border, then adjust the item length, starting x, and starting
	// y accordingly, and draw the border.
	if (this.borderStyle != BORDER_NONE)
	{
		itemLen -= 2;
		++curPos.x;
		++curPos.y;
		this.DrawBorder();
	}

	// Write the menu items, only up to the height of the menu
	var numItemsWritten = 0;
	for (var idx = this.topItemIdx; (idx < this.items.length) && (numItemsWritten < this.size.height); ++idx)
	{
		console.gotoxy(curPos.x, curPos.y++);
		this.WriteItem(idx, null, idx == this.selectedItemIdx);
		++numItemsWritten;
	}
	// If there are fewer items than the height of the menu, then write blank lines to fill
	// the rest of the height of the menu.
	var numPossibleItems = (this.borderStyle == BORDER_NONE ? this.size.height : this.size.height - 2);
	if (numItemsWritten < numPossibleItems)
	{
		for (; numItemsWritten <= numPossibleItems; ++numItemsWritten)
		{
			console.gotoxy(curPos.x, curPos.y++);
			printf("\1n" + this.colors.itemColor + "%-" + itemLen + "s", "");
		}
	}
}

// Draws the border around the menu items
function DDLightbarMenu_DrawBorder()
{
	if (this.borderStyle == BORDER_NONE)
		return;

	// Decide which characters to use for the border
	var upperLeftCornerChar;
	var upperRightCornerChar;
	var lowerLeftCornerChar;
	var lowerRightCornerChar;
	var horizontalChar;
	var verticalChar;
	switch (this.borderStyle)
	{
		case BORDER_SINGLE:
			upperLeftCornerChar = UPPER_LEFT_SINGLE;
			upperRightCornerChar = UPPER_RIGHT_SINGLE;
			lowerLeftCornerChar = LOWER_LEFT_SINGLE;
			lowerRightCornerChar = LOWER_RIGHT_SINGLE;
			horizontalChar = HORIZONTAL_SINGLE;
			verticalChar = VERTICAL_SINGLE;
			break;
		case BORDER_DOUBLE:
			upperLeftCornerChar = UPPER_LEFT_DOUBLE;
			upperRightCornerChar = UPPER_RIGHT_DOUBLE;
			lowerLeftCornerChar = LOWER_LEFT_DOUBLE;
			lowerRightCornerChar = LOWER_RIGHT_DOUBLE;
			horizontalChar = HORIZONTAL_DOUBLE;
			verticalChar = VERTICAL_DOUBLE;
			break;
	}

	// Draw the border around the menu options
	// Upper border
	console.gotoxy(this.pos.x, this.pos.y);
	console.print("\1n" + this.colors.borderColor);
	console.print(upperLeftCornerChar);
	var lineLen = this.size.width - 2;
	for (var i = 0; i < lineLen; ++i)
		console.print(horizontalChar);
	console.print(upperRightCornerChar);
	// Lower border
	console.gotoxy(this.pos.x, this.pos.y+this.size.height);
	console.print(lowerLeftCornerChar);
	for (var i = 0; i < lineLen; ++i)
		console.print(horizontalChar);
	console.print(lowerRightCornerChar);
	// Side borders
	lineLen = this.size.height - 1;
	var lineNum = 1;
	for (var lineNum = 1; lineNum <= lineLen; ++lineNum)
	{
		console.gotoxy(this.pos.x, this.pos.y+lineNum);
		console.print(verticalChar);
		console.gotoxy(this.pos.x+this.size.width-1, this.pos.y+lineNum);
		console.print(verticalChar);
	}
}

// Writes a single menu item
//
// Parameters:
//  pIdx: The index of the item to write
//  pItemLen: Optional - Calculated length of the item (in case the scrollbar is showing).
//            If this is not given, then this will be calculated.
//  pHighlight: Optional - Whether or not to highlight the item.  If this is not given,
//              the item will be highlighted based on whether the current selected item
//              matches the given index, pIdx.
function DDLightbarMenu_WriteItem(pIdx, pItemLen, pHighlight)
{
	if ((pIdx >= 0) && (pIdx < this.items.length))
	{
		var itemLen = 0;
		if (typeof(pItemLen) == "number")
			itemLen = pItemLen;
		else
		{
			itemLen = (this.showScrollbar ? this.size.width - 1 : this.size.width);
			if (this.borderStyle != BORDER_NONE)
				itemLen -= 2;
		}
		var itemColor = "";
		if (typeof(pHighlight) == "boolean")
			itemColor = (pHighlight ? this.colors.selectedItemColor : this.colors.itemColor);
		else
			itemColor = (pIdx == this.selectedItemIdx ? this.colors.selectedItemColor : this.colors.itemColor);

		// Get the item text, and truncate it to the displayable item width
		var itemText = this.items[pIdx].text;
		if (itemTextDisplayableLen(itemText) > itemLen)
			itemText = itemText.substr(0, itemLen);
		// Add the item color to the text
		itemText = itemColor + itemText;
		// See if there's an ampersand in the item text.  If so, we'll want to highlight the
		// next character with a different color.
		var ampersandIndex = itemText.indexOf("&");
		if (ampersandIndex > -1)
		{
			// See if the next character is a space character.  If not, then remove
			// the ampersand and highlight the next character in the text.
			if (itemText.length > ampersandIndex+1)
			{
				var nextChar = itemText.substr(ampersandIndex+1, 1);
				if (nextChar != " ")
				{
					itemText = itemText.substr(0, ampersandIndex) + this.colors.itemTextCharHighlightColor
					         + nextChar + "\1n" + itemColor + itemText.substr(ampersandIndex+2);
				}
			}
		}
		// Ensure the item text fills the width of the menu (if there's a background color,
		// use it for the entire width of the item text).  Then write the item.
		while (itemTextDisplayableLen(itemText) < itemLen)
			itemText += " ";
		console.print(itemText + "\1n");
	}
}

// Erases the menu - Draws black (normal color) where the menu was
function DDLightbarMenu_Erase()
{
	var formatStr = "%" + this.size.width + "s"; // For use with printf()
	console.print("\1n");
	var curPos = { x: this.pos.x, y: this.pos.y };
	for (var i = 0; i < this.size.height; ++i)
	{
		console.gotoxy(curPos.x, curPos.y++);
		printf(formatStr, "");
	}
	console.gotoxy(curPos);
}

// Sets a hotkey for a menu item
//
// Parameters:
//  pIdx: The index of the menu item
//  pHotkey: The hotkey to set for the menu item
function DDLightbarMenu_SetItemHotkey(pIdx, pHotkey)
{
	if ((typeof(pIdx) == "number") && (pIdx >= 0) && (pIdx < this.items.length) && (typeof(pHotkey) == "string"))
		this.items[pIdx].hotkey = pHotkey;
}

// Removes an item's hotkey
//
// Parameters:
//  pIdx: The index of the item
function  DDLightbarMenu_RemoveItemHotkey(pIdx)
{
	if ((typeof(pIdx) == "number") && (pIdx >= 0) && (pIdx < this.items.length))
	{
		if (this.items[pIdx].hasOwnProperty("hotkey"))
			delete this.items[pIdx].hotkey;
	}
}

// Removes the hotkeys from all items
function DDLightbarMenu_RemoveAllItemHotkeys()
{
	for (var i = 0; i < this.items.length; ++i)
	{
		if (this.items[i].hasOwnProperty("hotkey"))
			delete this.items[i].hotkey;
	}
}

// Waits for user input, optionally drawing the menu first.
//
// Parameters:
//  pDraw: Optional - Whether or not to draw the menu first.  By default, the
//         menu will be drawn first.
function DDLightbarMenu_GetVal(pDraw)
{
	if (this.items.length == 0)
		return "No menu items";

	var draw = (typeof(pDraw) == "boolean" ? pDraw : true);
	if (draw)
		this.Draw();

	// User input loop
	var retVal = null;
	var continueOn = true;
	while (continueOn)
	{
		var userInput = getKeyWithESCChars(K_NOECHO|K_NOSPIN|K_NOCRLF);
		if ((userInput == KEY_UP) || (userInput == KEY_LEFT))
		{
			if (this.selectedItemIdx > 0)
			{
				// Draw the current item in regular colors
				if (this.borderStyle == BORDER_NONE)
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				else
					console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
				this.WriteItem(this.selectedItemIdx, null, false);
				--this.selectedItemIdx;
				// Draw the new current item in selected colors
				// If the selected item is above the top of the menu, then we'll need to
				// scroll the items down.
				if (this.selectedItemIdx < this.topItemIdx)
				{
					--this.topItemIdx;
					this.Draw();
				}
				else
				{
					// The selected item is not above the top of the menu, so we can
					// just draw the selected item highlighted.
					if (this.borderStyle == BORDER_NONE)
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					else
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					this.WriteItem(this.selectedItemIdx, null, true);
				}
			}
			else
			{
				// selectedItemIdx is 0.  If wrap navigation is enabled, then go to the
				// last item.
				if (this.wrapNavigation)
				{
					// Draw the current item in regular colors
					if (this.borderStyle == BORDER_NONE)
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					else
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					this.WriteItem(this.selectedItemIdx, null, false);
					// Go to the last item and scroll to the bottom if necessary
					this.selectedItemIdx = this.items.length - 1;
					var oldTopItemIdx = this.topItemIdx;
					this.topItemIdx = this.items.length - this.size.height;
					if (this.topItemIdx < 0)
						this.topItemIdx = 0;
					if (this.topItemIdx != oldTopItemIdx)
						this.Draw();
					else
					{
						// Draw the new current item in selected colors
						if (this.borderStyle == BORDER_NONE)
							console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
						else
							console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
						this.WriteItem(this.selectedItemIdx, null, true);
					}
				}
			}
		}
		else if ((userInput == KEY_DOWN) || (userInput == KEY_RIGHT))
		{
			if (this.selectedItemIdx < this.items.length-1)
			{
				// Draw the current item in regular colors
				if (this.borderStyle == BORDER_NONE)
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				else
					console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
				this.WriteItem(this.selectedItemIdx, null, false);
				++this.selectedItemIdx;
				// Draw the new current item in selected colors
				// If the selectd item is below the bottom of the menu, then we'll need to
				// scroll the items up.
				if (this.selectedItemIdx > this.topItemIdx+this.size.height-1)
				{
					++this.topItemIdx;
					this.Draw();
				}
				else
				{
					// The selected item is not below the bottom of the menu, so we can
					// just draw the selected item highlighted.
					if (this.borderStyle == BORDER_NONE)
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					else
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					this.WriteItem(this.selectedItemIdx, null, true);
				}
			}
			else
			{
				// selectedItemIdx is the last item index.  If wrap navigation is enabled,
				// then go to the first item.
				if (this.wrapNavigation)
				{
					// Draw the current item in regular colors
					if (this.borderStyle == BORDER_NONE)
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					else
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					this.WriteItem(this.selectedItemIdx, null, false);
					// Go to the first item and scroll to the top if necessary
					this.selectedItemIdx = 0;
					var oldTopItemIdx = this.topItemIdx;
					this.topItemIdx = 0;
					if (this.topItemIdx != oldTopItemIdx)
						this.Draw();
					else
					{
						// Draw the new current item in selected colors
						if (this.borderStyle == BORDER_NONE)
							console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
						else
							console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
						this.WriteItem(this.selectedItemIdx, null, true);
					}
				}
			}
		}
		else if (userInput == KEY_PAGE_UP)
		{
			var numItemsPerPage = this.size.height;
			if (this.borderStyle != BORDER_NONE)
				numItemsPerPage -= 2;
			var newTopItemIdx = this.topItemIdx - numItemsPerPage;
			if (newTopItemIdx < 0)
				newTopItemIdx = 0;
			if (newTopItemIdx != this.topItemIdx)
			{
				this.topItemIdx = newTopItemIdx;
				this.selectedItemIdx -= numItemsPerPage;
				if (this.selectedItemIdx < 0)
					this.selectedItemIdx = 0;
				this.Draw();
			}
		}
		else if (userInput == KEY_PAGE_DOWN)
		{
			var numItemsPerPage = this.size.height;
			if (this.borderStyle != BORDER_NONE)
				numItemsPerPage -= 2;
			// Figure out how many pages are needed to list all the items
			//var numPages = Math.ceil(this.items.length / this.size.height);
			// Figure out the top index for the last page.
			//var topIndexForLastPage = (this.size.height * numPages) - this.size.height;
			var topIndexForLastPage = this.items.length - numItemsPerPage;
			if (topIndexForLastPage < 0)
				topIndexForLastPage = 0;
			else if (topIndexForLastPage >= this.items.length)
				topIndexForLastPage = this.items.length - 1;
			if (topIndexForLastPage != this.topItemIdx)
			{
				// Update the selected & top item indexes
				this.selectedItemIdx += numItemsPerPage;
				this.topItemIdx += numItemsPerPage;
				if (this.selectedItemIdx >= topIndexForLastPage)
					this.selectedItemIdx = topIndexForLastPage;
				if (this.topItemIdx > topIndexForLastPage)
					this.topItemIdx = topIndexForLastPage;
				this.Draw();
			}
		}
		else if (userInput == KEY_HOME)
		{
			// Go to the first item on the current page
			if (this.selectedItemIdx > this.topItemIdx)
			{
				// Draw the current item in regular colors
				console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, false);
				this.selectedItemIdx = this.topItemIdx;
				// Draw the new current item in selected colors
				console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, true);
			}
		}
		else if (userInput == KEY_END)
		{
			var numItemsPerPage = this.size.height;
			if (this.borderStyle != BORDER_NONE)
				numItemsPerPage -= 2;
			// Go to the last item on the current page
			if (this.selectedItemIdx < this.items.length-1)
			{
				// Draw the current item in regular colors
				console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, false);
				this.selectedItemIdx = this.topItemIdx + numItemsPerPage - 1;
				if (this.selectedItemIdx >= this.items.length)
					this.selectedItemIdx = this.items.length - 1;
				// Draw the new current item in selected colors
				console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, true);
			}
		}
		else if (userInput == KEY_ENTER)
		{
			retVal = this.items[this.selectedItemIdx].retval;
			continueOn = false;
		}
		else if (userInput == KEY_ESC)
			continueOn = false;
		else
		{
			// See if the user pressed a hotkey set for one of the items.  If so,
			// then choose that item.
			for (var i = 0; i < this.items.length; ++i)
			{
				if (this.items[i].hasOwnProperty("hotkey"))
				{
					var userPressedHotkey = false;
					if (this.hotkeyCaseSensitive)
						userPressedHotkey = (userInput == this.items[i].hotkey);
					else
						userPressedHotkey = (userInput.toUpperCase() == this.items[i].hotkey.toUpperCase());
					if (userPressedHotkey)
					{
						retVal = this.items[i].retval;
						this.selectedItemIdx = i;
						continueOn = false;
						break;
					}
				}
			}
		}
	}

	// Set the screen color back to normal so that text written to the screen
	// after this looks good.
	console.print("\1n");

	return retVal;
}

// Inputs a keypress from the user and handles some ESC-based
// characters such as PageUp, PageDown, and ESC.  If PageUp
// or PageDown are pressed, this function will return the
// string "\1PgUp" (KEY_PAGE_UP) or "\1Pgdn" (KEY_PAGE_DOWN),
// respectively.  Also, F1-F5 will be returned as "\1F1"
// through "\1F5", respectively.
// Thanks goes to Psi-Jack for the original impementation
// of this function.
//
// Parameters:
//  pGetKeyMode: Optional - The mode bits for console.getkey().
//               If not specified, K_NONE will be used.
//
// Return value: The user's keypress
function getKeyWithESCChars(pGetKeyMode)
{
   var getKeyMode = K_NONE;
   if (typeof(pGetKeyMode) == "number")
      getKeyMode = pGetKeyMode;

   var userInput = console.getkey(getKeyMode);
   if (userInput == KEY_ESC) {
      switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
         case '[':
            switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
               case 'V':
                  userInput = KEY_PAGE_UP;
                  break;
               case 'U':
                  userInput = KEY_PAGE_DOWN;
                  break;
           }
           break;
         case 'O':
           switch (console.inkey(K_NOECHO|K_NOSPIN, 2)) {
              case 'P':
                 userInput = "\1F1";
                 break;
              case 'Q':
                 userInput = "\1F2";
                 break;
              case 'R':
                 userInput = "\1F3";
                 break;
              case 'S':
                 userInput = "\1F4";
                 break;
              case 't':
                 userInput = "\1F5";
                 break;
           }
         default:
           break;
      }
   }

   return userInput;
}

// Returns the length of an item's text, not counting non-displayable
// characters (such as Synchronet color attributes and an ampersand
// immediately before a non-space)
function itemTextDisplayableLen(pText)
{
	var textLen = strip_ctrl(pText).length;
	// Look for ampersands immediately before a non-space and if found, don't
	// count those
	var startIdx = 0;
	var ampersandIndex = pText.indexOf("&", startIdx);
	while (ampersandIndex > -1)
	{
		// See if the next character is a space character.  If not, then
		// don't count it in the length.
		if (pText.length > ampersandIndex+1)
		{
			var nextChar = pText.substr(ampersandIndex+1, 1);
			if (nextChar != " ")
				--textLen;
		}
		startIdx = ampersandIndex+1;
		ampersandIndex = pText.indexOf("&", startIdx);
	}
	return textLen;
}