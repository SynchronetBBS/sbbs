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
borderEnabled property to true.  Without a border, the menu gains 2
characters of width and 2 lines of height.

This script provides an object, DDLightbarMenu.  Use the DDLightbarMenu
constructor to create the object.  Some other notable methods:
Add()
SetItemHotkey()
AddItemHotkey()
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
property to false.

This menu object supports adding multiple hotkeys to each menu item.  A hotkey
can be specified in the Add() method a couple of different ways - By specifying
a hotkey as the 3rd parameter and/or by putting a & in the menu item text just
before a key you want to use as the hotkey.  For example, in the text "E&xit",
"x" would be used as the hotkey for the item.  If you want to disable the use of
ampersands for hotkeys in menu items (for instance, if you want an item to
literally display an ampersand before a character), set the
ampersandHotkeysInItems property to false.  For instance:
lbMenu.ampersandHotkeysInItems = false;
Note that ampersandHotkeysInItems must be set before adding menu items.

You can call the SetItemHotkey() method to set a single hotkey to be used for
a menu item or AddItemHotkey() to add an additional hotkey for an item in
addition to any existing hotkeys it might already have.

This menu class also supports an optional "numbered mode", where each option is
displayed with a number to the left (starting at 1), and the user is allowed to
choose an option by typing the number of the item.  Numbered mode is disabled
by default and can be enabled by setting the numberedMode property to true.
For example:
lbMenu.numberedMode = true;
When numbered mode is enabled and the user starts typing a number, the menu will
prompt the user for an item number.  Note that the prompt will be located on the
line below the menu, so in addition to the menu's height, you'll also need an
extra line on the screen to account for the item prompt.  In addition, when the
user presses the enter key after the item number, a carriage return/line feed
will be outputted, so in numbered mode, the menu's height should not go further
than 2 lines below the console height.  Otherwise, the display of the menu will
not be correct if the user decides not to enter a number.

This menu also supports multiple options selected (by default, that is not enabled).
To enable that, set the multiSelect property to true.  When enabled, the GetVal()
method will return an array of the user's selections rather than a string (or null if
the user aborted).  You can also set a limit on the number of items selected in
multi-select mode by setting the maxNumSelections property.  The default value is -1,
which means no limit (0 also means no limit).
Example, with a limit:
lbMenu.multiSelect = true;
lbMenu.maxNumSelections = 5;

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

To enable borders, set the borderEnabled property to true.  Example:
lbMenu.borderEnabled = true;

The menu object has an object called borderChars, which stores the characters used
to draw the border.  you can change the characters used to draw the border by
setting the following properties of the borderChars object:
  upperLeft: The character to use for the upper-left corner
  upperRight: The character to use for the upper-right corner
  lowerLeft: The character to use for the lower-left corner
  lowerRight: The character to use for the lower-right corner
  top: The character to use for the top border
  bottom: The character to use for the bottom border
  left: The character to use for the left border
  right: The character to use for the right border
For example:
lbMenu.borderChars.upperLeft = "\xDA"; // Single-line upper-left character

If you want hotkeys to be case-sensitive, you can set the hotkeyCaseSensitive
property to true (it is false by default).  For example:
lbMenu.hotkeyCaseSensitive = true;
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
// Other characters
var CHECK_CHAR = "\xFB";

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
	this.borderEnabled = false;
	this.colors = {
		itemColor: "\1n\1w\1" + "4",
		selectedItemColor: "\1n\1b\1" + "7",
		itemTextCharHighlightColor: "\1y\1h",
		borderColor: "\1n\1b"
	};
	// Characters to use to draw the border
	this.borderChars = {
		upperLeft: UPPER_LEFT_DOUBLE,
		upperRight: UPPER_RIGHT_DOUBLE,
		lowerLeft: LOWER_LEFT_DOUBLE,
		lowerRight: LOWER_RIGHT_DOUBLE,
		top: HORIZONTAL_DOUBLE,
		bottom: HORIZONTAL_DOUBLE,
		left: VERTICAL_DOUBLE,
		right: VERTICAL_DOUBLE
	};
	this.selectedItemIdx = 0;
	this.topItemIdx = 0;
	this.wrapNavigation = true;
	this.hotkeyCaseSensitive = false;
	this.ampersandHotkeysInItems = true;
	this.multiSelect = false;
	this.maxNumSelections = -1; // -1 or 0 means no limit on the number of selections
	this.numberedMode = false;
	this.itemNumLen = 0; // For the length of the item numbers in numbered mode

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
	this.AddItemHotkey = DDLightbarMenu_AddItemHotkey;
	this.RemoveItemHotkey = DDLightbarMenu_RemoveItemHotkey;
	this.RemoveItemHotkeys = DDLightbarMenu_RemoveItemHotkeys;
	this.RemoveAllItemHotkeys = DDLightbarMenu_RemoveAllItemHotkeys;
	this.GetVal = DDLightbarMenu_GetVal;
	this.SetBorderChars = DDLightbarMenu_SetBorderChars;

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
		retval: pRetval,
		hotkeys: []
	};
	if (pRetval == undefined)
		item.retval = this.items.length;
	// If pHotkey is defined, then use it as the hotkey.  Otherwise, if
	// ampersandHotkeysInItems is true, look for the first & in the item text
	// and if there's a non-space after it, then use that character as the
	// hotkey.
	if (typeof(pHotkey) == "string")
		item.hotkeys.push(pHotkey);

	if (this.ampersandHotkeysInItems)
	{
		var ampersandIndex = pText.indexOf("&");
		if (ampersandIndex > -1)
		{
			// See if the next character is a space character.  If not, then
			// don't count it in the length.
			if (pText.length > ampersandIndex+1)
			{
				var nextChar = pText.substr(ampersandIndex+1, 1);
				if (nextChar != " ")
					item.hotkeys.push(nextChar);
			}
		}
	}

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
	if (this.borderEnabled)
	{
		itemLen -= 2;
		++curPos.x;
		++curPos.y;
		this.DrawBorder();
	}
	// For numbered mode, we'll need to know the length of the longest item number
	// so that we can use that space to display the item numbers.
	if (this.numberedMode)
	{
		this.itemNumLen = this.items.length.toString().length;
		itemLen -= this.itemNumLen;
		--itemLen; // Have a space for separation between the numbers and items
	}

	// Write the menu items, only up to the height of the menu
	var numPossibleItems = (this.borderEnabled ? this.size.height - 2 : this.size.height);
	var numItemsWritten = 0;
	for (var idx = this.topItemIdx; (idx < this.items.length) && (numItemsWritten < numPossibleItems); ++idx)
	{
		console.gotoxy(curPos.x, curPos.y++);
		this.WriteItem(idx, itemLen, idx == this.selectedItemIdx);
		++numItemsWritten;
	}
	// If there are fewer items than the height of the menu, then write blank lines to fill
	// the rest of the height of the menu.
	if (numItemsWritten < numPossibleItems)
	{
		for (; numItemsWritten < numPossibleItems; ++numItemsWritten)
		{
			console.gotoxy(curPos.x, curPos.y++);
			console.print("\1n");
			if (this.numberedMode)
				printf("\1n%" + this.itemNumLen + "s ", "");
			printf(this.colors.itemColor + "%-" + itemLen + "s", "");
		}
	}
}

// Draws the border around the menu items
function DDLightbarMenu_DrawBorder()
{
	if (!this.borderEnabled)
		return;

	// Draw the border around the menu options
	console.print("\1n" + this.colors.borderColor);
	// Upper border
	console.gotoxy(this.pos.x, this.pos.y);
	if (this.borderChars.hasOwnProperty("upperLeft") && (typeof(this.borderChars.upperLeft) == "string"))
		console.print(this.borderChars.upperLeft);
	else
		console.print(" ");
	var lineLen = this.size.width - 2;
	if (this.borderChars.hasOwnProperty("top") && (typeof(this.borderChars.top) == "string"))
	{
		for (var i = 0; i < lineLen; ++i)
			console.print(this.borderChars.top);
	}
	else
	{
		for (var i = 0; i < lineLen; ++i)
			console.print(" ");
	}
	if (this.borderChars.hasOwnProperty("upperRight") && (typeof(this.borderChars.upperRight) == "string"))
		console.print(this.borderChars.upperRight);
	else
		console.print(" ");
	// Lower border
	console.gotoxy(this.pos.x, this.pos.y+this.size.height-1);
	if (this.borderChars.hasOwnProperty("lowerLeft") && (typeof(this.borderChars.lowerLeft) == "string"))
		console.print(this.borderChars.lowerLeft);
	else
		console.print(" ");
	var lineLen = this.size.width - 2;
	if (this.borderChars.hasOwnProperty("bottom") && (typeof(this.borderChars.bottom) == "string"))
	{
		for (var i = 0; i < lineLen; ++i)
			console.print(this.borderChars.bottom);
	}
	else
	{
		for (var i = 0; i < lineLen; ++i)
			console.print(" ");
	}
	if (this.borderChars.hasOwnProperty("lowerRight") && (typeof(this.borderChars.lowerRight) == "string"))
		console.print(this.borderChars.lowerRight);
	else
		console.print(" ");
	// Side borders
	var leftSideChar = " ";
	var rightSideChar = " ";
	if (this.borderChars.hasOwnProperty("left") && (typeof(this.borderChars.left) == "string"))
		leftSideChar = this.borderChars.left;
	if (this.borderChars.hasOwnProperty("right") && (typeof(this.borderChars.right) == "string"))
		rightSideChar = this.borderChars.right;
	lineLen = this.size.height - 2;
	var lineNum = 1;
	for (var lineNum = 1; lineNum <= lineLen; ++lineNum)
	{
		console.gotoxy(this.pos.x, this.pos.y+lineNum);
		console.print(leftSideChar);
		console.gotoxy(this.pos.x+this.size.width-1, this.pos.y+lineNum);
		console.print(rightSideChar);
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
//  pSelected: Optional - Whether or not this item is selected (mainly intended for multi-select
//             mode).  Defaults to false.  If true, then a mark character will be displayed
//             at the end of the item's text.
function DDLightbarMenu_WriteItem(pIdx, pItemLen, pHighlight, pSelected)
{
	if ((pIdx >= 0) && (pIdx < this.items.length))
	{
		var itemLen = 0;
		if (typeof(pItemLen) == "number")
			itemLen = pItemLen;
		else
		{
			itemLen = (this.showScrollbar ? this.size.width - 1 : this.size.width);
			if (this.borderEnabled)
				itemLen -= 2;
			// For numbered mode, we'll need to know the length of the longest item number
			// so that we can use that space to display the item numbers.
			if (this.numberedMode)
			{
				this.itemNumLen = this.items.length.toString().length;
				itemLen -= this.itemNumLen;
				--itemLen; // Have a space for separation between the numbers and items
			}
		}
		var itemColor = "";
		if (typeof(pHighlight) == "boolean")
			itemColor = (pHighlight ? this.colors.selectedItemColor : this.colors.itemColor);
		else
			itemColor = (pIdx == this.selectedItemIdx ? this.colors.selectedItemColor : this.colors.itemColor);
		var selected = (typeof(pSelected) == "boolean" ? pSelected : false);

		// Get the item text, and truncate it to the displayable item width
		var itemText = this.items[pIdx].text;
		if (itemTextDisplayableLen(itemText, this.ampersandHotkeysInItems) > itemLen)
			itemText = itemText.substr(0, itemLen);
		// Add the item color to the text
		itemText = itemColor + itemText;
		// If ampersandHotkeysInItems is true, see if there's an ampersand in
		// the item text.  If so, we'll want to highlight the next character
		// with a different color.
		if (this.ampersandHotkeysInItems)
		{
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
		}
		// If the item is selected, then display a check mark at the end of the item text.
		// TODO: Is this right?
		if (selected)
			itemText = format("%-" + (itemLen-2) + "s %s", itemText.substr(0, itemLen-2), CHECK_CHAR);
		// Ensure the item text fills the width of the menu (in case there's a
		// background color, it should be used for the entire width of the item
		// text).  Then write the item.
		var currentTextLen = itemTextDisplayableLen(itemText, this.ampersandHotkeysInItems);
		if (currentTextLen < itemLen)
			itemText += format("%" + +(itemLen-currentTextLen) + "s", ""); // Append spaces to the end of itemText
		// If in numbered mode, add the item number to the front of the item text.
		if (this.numberedMode)
			itemText = format("\1n%" + this.itemNumLen + "d ", pIdx+1) + itemText;
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
	{
		this.items[pIdx].hotkeys = [];
		this.items[pIdx].hotkeys.push(pHotkey);
	}
}

// Adds a hotkey for a menu item (in addition to the item's other hotkeys)
//
// Parameters:
//  pIdx: The index of the menu item
//  pHotkey: The hotkey to add for the menu item
function DDLightbarMenu_AddItemHotkey(pIdx, pHotkey)
{
	if ((typeof(pIdx) == "number") && (pIdx >= 0) && (pIdx < this.items.length) && (typeof(pHotkey) == "string"))
		this.items[pIdx].hotkeys.push(pHotkey);
}

// Removes a specific hotkey from an item.
//
// Parameters:
//  pIdx: The index of the item
//  pHotkey: The hotkey to remove from the item
function DDLightbarMenu_RemoveItemHotkey(pIdx, pHotkey)
{
	if ((typeof(pIdx) == "number") && (pIdx >= 0) && (pIdx < this.items.length))
	{
		for (var i = 0; i < this.items[pIdx].hotkeys.length; ++i)
		{
			if (this.items[pIdx].hotkeys[i] == pHotkey)
				this.items[pIdx].hotkeys.splice(i, 1);
		}
	}
}

// Removes all hotkeys for an item
//
// Parameters:
//  pIdx: The index of the item
function  DDLightbarMenu_RemoveItemHotkeys(pIdx)
{
	if ((typeof(pIdx) == "number") && (pIdx >= 0) && (pIdx < this.items.length))
		this.items[pIdx].hotkeys = [];
}

// Removes the hotkeys from all items
function DDLightbarMenu_RemoveAllItemHotkeys()
{
	for (var i = 0; i < this.items.length; ++i)
		this.items[i].hotkeys = [];
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
	var userChoices = null; // For multi-select mode
	var selectedItemIndexes = { }; // For multi-select mode
	var retVal = null; // For single-choice mode
	var continueOn = true;
	while (continueOn)
	{
		var userInput = getKeyWithESCChars(K_NOECHO|K_NOSPIN|K_NOCRLF);
		if ((userInput == KEY_UP) || (userInput == KEY_LEFT))
		{
			if (this.selectedItemIdx > 0)
			{
				// Draw the current item in regular colors
				if (this.borderEnabled)
					console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
				else
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
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
					if (this.borderEnabled)
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					else
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				}
			}
			else
			{
				// selectedItemIdx is 0.  If wrap navigation is enabled, then go to the
				// last item.
				if (this.wrapNavigation)
				{
					// Draw the current item in regular colors
					if (this.borderEnabled)
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					else
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					// Go to the last item and scroll to the bottom if necessary
					this.selectedItemIdx = this.items.length - 1;
					var oldTopItemIdx = this.topItemIdx;
					var numItemsPerPage = (this.borderEnabled ? this.size.height - 2 : this.size.height);
					this.topItemIdx = this.items.length - numItemsPerPage;
					if (this.topItemIdx < 0)
						this.topItemIdx = 0;
					if (this.topItemIdx != oldTopItemIdx)
						this.Draw();
					else
					{
						// Draw the new current item in selected colors
						if (this.borderEnabled)
							console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
						else
							console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
						this.WriteItem(this.selectedItemIdx, null, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					}
				}
			}
		}
		else if ((userInput == KEY_DOWN) || (userInput == KEY_RIGHT))
		{
			if (this.selectedItemIdx < this.items.length-1)
			{
				// Draw the current item in regular colors
				if (this.borderEnabled)
					console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
				else
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				++this.selectedItemIdx;
				// Draw the new current item in selected colors
				// If the selected item is below the bottom of the menu, then we'll need to
				// scroll the items up.
				var numItemsPerPage = (this.borderEnabled ? this.size.height - 2 : this.size.height);
				if (this.selectedItemIdx > this.topItemIdx+numItemsPerPage-1)
				{
					++this.topItemIdx;
					this.Draw();
				}
				else
				{
					// The selected item is not below the bottom of the menu, so we can
					// just draw the selected item highlighted.
					if (this.borderEnabled)
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					else
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				}
			}
			else
			{
				// selectedItemIdx is the last item index.  If wrap navigation is enabled,
				// then go to the first item.
				if (this.wrapNavigation)
				{
					// Draw the current item in regular colors
					if (this.borderEnabled)
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					else
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					// Go to the first item and scroll to the top if necessary
					this.selectedItemIdx = 0;
					var oldTopItemIdx = this.topItemIdx;
					this.topItemIdx = 0;
					if (this.topItemIdx != oldTopItemIdx)
						this.Draw();
					else
					{
						// Draw the new current item in selected colors
						if (this.borderEnabled)
							console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
						else
							console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
						this.WriteItem(this.selectedItemIdx, null, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					}
				}
			}
		}
		else if (userInput == KEY_PAGE_UP)
		{
			var numItemsPerPage = (this.borderEnabled ? this.size.height - 2 : this.size.height);
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
			var numItemsPerPage = (this.borderEnabled ? this.size.height - 2 : this.size.height);
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
				if (this.borderEnabled)
					console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
				else
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				this.selectedItemIdx = this.topItemIdx;
				// Draw the new current item in selected colors
				if (this.borderEnabled)
					console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
				else
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
			}
		}
		else if (userInput == KEY_END)
		{
			var numItemsPerPage = this.size.height;
			if (this.borderEnabled)
				numItemsPerPage -= 2;
			// Go to the last item on the current page
			if (this.selectedItemIdx < this.items.length-1)
			{
				// Draw the current item in regular colors
				if (this.borderEnabled)
					console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
				else
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				this.selectedItemIdx = this.topItemIdx + numItemsPerPage - 1;
				if (this.selectedItemIdx >= this.items.length)
					this.selectedItemIdx = this.items.length - 1;
				// Draw the new current item in selected colors
				if (this.borderEnabled)
					console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
				else
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
			}
		}
		else if (userInput == KEY_ENTER)
		{
			// If multi-select is enabled and if the user hasn't made any choices,
			// then add the current item to the user choices.  Otherwise, choose
			// the current item.  Then exit.
			if (this.multiSelect)
			{
				if (Object.keys(selectedItemIndexes).length == 0)
					selectedItemIndexes[this.selectedItemIdx] = true;
			}
			else
				retVal = this.items[this.selectedItemIdx].retval;
			continueOn = false;
		}
		else if (userInput == " ")
		{
			// Select the current item
			if (this.multiSelect)
			{
				if (selectedItemIndexes.hasOwnProperty(this.selectedItemIdx))
					delete selectedItemIndexes[this.selectedItemIdx];
				else
				{
					var addIt = true;
					if (this.maxNumSelections > 0)
						addIt = (Object.keys(selectedItemIndexes).length < this.maxNumSelections);
					if (addIt)
						selectedItemIndexes[this.selectedItemIdx] = true;
				}
				// TODO: Draw a check-mark next to the item
			}
		}
		else if (userInput == KEY_ESC)
		{
			continueOn = false;
			// Ensure any returned choice objects are null/empty to signal
			// that the user aborted
			userChoices = null; // For multi-select mode
			selectedItemIndexes = { }; // For multi-select mode
			retVal = null; // For single-choice mode
		}
		// For numbered mode, if the user enters a number, allow the user to
		// choose an item by typing its number.
		else if (/[0-9]/.test(userInput) && this.numberedMode)
		{
			var originalCurpos = console.getxy();

			// Put the user's input back in the input buffer to
			// be used for getting the rest of the message number.
			console.ungetstr(userInput);
			// Move the cursor to the bottom of the screen and
			// prompt the user for the message number.
			var promptX = this.pos.x;
			var promptY = this.pos.y+this.size.height;
			console.gotoxy(promptX, promptY);
			printf("\1n%" + this.size.width + "s", ""); // Blank out what might be on the screen already
			console.gotoxy(promptX, promptY);
			console.print("\1cItem #: \1h");
			var userEnteredItemNum = console.getnum(this.items.length);
			// Blank out the input prompt
			console.gotoxy(promptX, promptY);
			printf("\1n%" + this.size.width + "s", "");
			// If the user entered a number, then get that item's return value
			// and stop the input loop.
			if (userEnteredItemNum > 0)
			{
				this.selectedItemIdx = userEnteredItemNum-1;
				if (this.multiSelect)
				{
					if (selectedItemIndexes.hasOwnProperty(this.selectedItemIdx))
						delete selectedItemIndexes[this.selectedItemIdx];
					else
					{
						var addIt = true;
						if (this.maxNumSelections > 0)
							addIt = (Object.keys(selectedItemIndexes).length < this.maxNumSelections);
						if (addIt)
							selectedItemIndexes[this.selectedItemIdx] = true;
					}
					// TODO: Put a check-mark next to the selected item
					// TODO: Screen refresh?
				}
				else
				{
					retVal = this.items[this.selectedItemIdx].retval;
					continueOn = false;
				}
			}
			else
				console.gotoxy(originalCurpos); // Move the cursor back where it was
		}
		else
		{
			// See if the user pressed a hotkey set for one of the items.  If so,
			// then choose that item.
			for (var i = 0; i < this.items.length; ++i)
			{
				for (var h = 0; h < this.items[i].hotkeys.length; ++h)
				{
					var userPressedHotkey = false;
					if (this.hotkeyCaseSensitive)
						userPressedHotkey = (userInput == this.items[i].hotkeys[h]);
					else
						userPressedHotkey = (userInput.toUpperCase() == this.items[i].hotkeys[h].toUpperCase());
					if (userPressedHotkey)
					{
						if (this.multiSelect)
						{
							if (selectedItemIndexes.hasOwnProperty(i))
								delete selectedItemIndexes[i];
							else
							{
								var addIt = true;
								if (this.maxNumSelections > 0)
									addIt = (Object.keys(selectedItemIndexes).length < this.maxNumSelections);
								if (addIt)
									selectedItemIndexes[i] = true;
							}
							// TODO: Screen refresh?
						}
						else
						{
							retVal = this.items[i].retval;
							this.selectedItemIdx = i;
							continueOn = false;
						}
						break;
					}
				}
			}
		}
	}

	// Set the screen color back to normal so that text written to the screen
	// after this looks good.
	console.print("\1n");
	
	// If in multi-select mode, populate userChoices with the choices
	// that the user selected.
	if (this.multiSelect && (Object.keys(selectedItemIndexes).length > 0))
	{
		userChoices = [];
		for (var prop in selectedItemIndexes)
			userChoices.push(this.items[prop].retval);
	}

	return (this.multiSelect ? userChoices : retVal);
}

// Sets the characters to use for drawing the border
//
// Parameters:
//  pBorderChars: An object with the following properties:
//                upperLeft: The character to use for the upper-left corner
//                upperRight: The character to use for the upper-right corner
//                lowerLeft: The character to use for the lower-left corner
//                lowerRight: The character to use for the lower-right corner
//                top: The character to use for the top border
//                bottom: The character to use for the bottom border
//                left: The character to use for the left border
//                right: The character to use for the right border
function DDLightbarMenu_SetBorderChars(pBorderChars)
{
	if (typeof(pBorderChars) != "object")
		return;

	var borderPropNames = [ "upperLeft", "upperRight", "lowerLeft", "lowerRight",
	                        "top", "bottom", "left", "right" ];
	for (var i = 0; i < borderPropNames.length; ++i)
	{
		if (pBorderChars.hasOwnProperty(borderPropNames))
			this.borderChars[borderPropNames[i]] = pBorderChars[borderPropNames[i]];
	}
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
//
// Parameters:
//  pText: The text to test
//  pAmpersandHotkeysInItems: Boolean - Whether or not ampersand hotkeys are enabled for the item text
function itemTextDisplayableLen(pText, pAmpersandHotkeysInItems)
{
	var textLen = strip_ctrl(pText).length;
	// If pAmpersandHotkeysInItems is true, look for ampersands immediately
	// before a non-space and if found, don't count those.
	if (pAmpersandHotkeysInItems)
	{
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
	}
	return textLen;
}