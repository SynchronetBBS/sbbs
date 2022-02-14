// $Id: dd_lightbar_menu.js,v 1.24 2020/05/08 04:49:10 nightfox Exp $

/* Digital Distortion Lightbar Menu library
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * Addresses: digitaldistortionbbs.com
 *            digdist.synchro.net

This is a lightbar menu library.  This allows creating a scrollable menu of
text items for the user to choose from.  The user can naviate the list using
the up & down arrows, PageUp, PageDown, Home, and End keys.  The enter key
selects an item.  The ESC key will exit the menu and return null.
This menu library requires the use of an ANSI terminal.
By default, this menu library does not display a border around the menu.
If you want this library to draw a border around the menu, you can set the
borderEnabled property to true.  Without a border, the menu gains 2
characters of width and 2 lines of height.  If using a border, a title (text)
can be displayed in the top border by setting the menuTitle property (it
defaults to an empty string, for no title).

This script provides an object, DDLightbarMenu.  Use the DDLightbarMenu
constructor to create the object.  Some other notable methods:
Add()
SetItemHotkey()
AddItemHotkey()
SetPos()
SetSize()
GetVal()
AddAdditionalSelectItemKeys()
SetBorderChars()
SetColors()

To change the colors used for displaying the items, you can change the values
in the colors object within the DDLightbarMenu object.  These are the current
supported colors:
itemColor: The color to use for non-selected items (current default is white
           on blue).  This can be a string (with the color/attribute values)
           or an array to specify colors for different sections of the item
		   text to display in the menu.  See the note on item color arrays
		   below.
selectedItemColor: The color to use for selected items (current default is blue
                   on white).  This can be a string (with the color/attribute values)
                   or an array to specify colors for different sections of the item
		           text to display in the menu.  See the note on item color arrays
		           below.
itemTextCharHighlightColor: The color of a highlighted non-space character in an
                            item text (specified by having a & in the item text).
							It's important not to specify a "\1n" in here in case
							the item text should have a background color.
borderColor: The color for the borders (if borders are enabled)
You can also call SetColors() and pass in a JS object with any or all of the
above properties to set the colors internally in the DDLightbarMenu object.

Item color arrays: Currently, colors.itemColor and colors.seletedItemColor within
a DDLightbarMenu object can be either a string (containing color/attribute codes)
or an array with color/attribute codes for different sections of the item strings
to display in the menu.  The array is to contain objects with the following
properties:
start: The index of the first character in the item string to apply the colors to
end: One past the last character index in the string to apply the colors to
attrs: The Synchronet attribute codes to apply to the section of the item string
For the last item, the 'end' property can be -1, 0, or greater than the length
of the item to apply the color/attribute codes to the rest of the string.


By default, the menu selection will wrap around to the beginning/end when using
the down/up arrows.  That behavior can be disabled by setting the wrapNavigation
property to false.

You can enable the display of a scrollbar by setting the scrollbarEnabled property
to true.  By default, it is false.  For instance (assuming the menu object is lbMenu):
lbMenu.scrollbarEnabled = true;
The scrollbar can help to visually show how far the user is through the menu.  When
enabled, the scrollbar will appear on the right side of the menu.  If borders are enabled,
the scrollbar will appear just inside the right border. Also, if the scrollbar is
enabled but all the items would fit in a single "page" in the menu, then the scrollbar
won't be displayed.
The scrollbar uses block characters to draw the scrollbar: ASCII character 176 for
the background and ASCII 177 for the block that moves on the scrollbar.  If you want
to change those characters, you can change the scrollbarInfo.BGChar and
scrollbarInfo.blockChar properties in the menu object.
By default, the scrollbar colors are high (bright) black for the background and high
(bright) white for the moving block character.  If desired, those can be changed
with the colors.scrollbarBGColor and colors.scrollbarScrollBlockColor properties in
the menu object.

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

You can call AddAdditionalSelectItemKeys() to add additional keys that can be
used to select any item (in addition to Enter).  That function takes a string
of characters, and the keys are case-sensitive.  For example, to add the key E
to select an item:
lbMenu.AddAdditionalSelectItemKeys("E");
To make a case-insensitive verison, both the uppercase and lowercase letter
would need to be added, as in the following example for E:
lbMenu.AddAdditionalSelectItemKeys("Ee");

Also, after showing the menu & getting a value from the user (using the GetVal()
function), the lastUserInput property will have the user's last keypress.

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
require("dd_lightbar_menu.js", "DDLightbarMenu");
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
Alternately, you can call the SetBorderChars() function and pass in a JS object
with any or all of the above properties to set those values internally in the
DDLightbarMenu object.

If you want hotkeys to be case-sensitive, you can set the hotkeyCaseSensitive
property to true (it is false by default).  For example:
lbMenu.hotkeyCaseSensitive = true;

To add additional key characters as quit keys (in addition to ESC), call
AddAdditionalQuitKeys() with a string of characters.  For example:
lbMenu.AddAdditionalQuitKeys("qQ");

To enable the border and set top and bottom border text:
lbMenu.borderEnabled = true;
lbMenu.topBorderText = "Options";
lbMenu.bottomBorderText = "Enter = Select";


For a more advanced usage, if you have another large list of items you want
to use in the menu instead of the menu's own list of items, you can replace
the NumItems and GetItem functions in the menu object and write your own
versions that access a different list of items.  This can be useful, for instance,
if you're working with a Synchronet messagebase (which may include a large number
of messages), so you can avoid the time taken to add those items to a DDLightbarMenu.
NumItems() needs to return the number of items in the list.  GetItem() takes an item
index as a parameter and needs to return an item object that is compatible with
DDLightbarMenu.  You can get a default item object by calling MakeItemWithRetval()
or MakeItemWithTextAndRetval(), then change its text and retval properties as
needed, then return the item object.  In the item object, the 'text' property
is the text to display in the menu, and the 'retval' proprety is the value to return
when the user chooses that item.
An example (assuming the lightbar menu object is called lbMenu):
lbMenu.NumItems = function() {
	// Do your own thing to get the number of items in your list.
	// ...
	// Assuming myNumItems is the number of items in your list:
	return myNumItems;
};
lbMenu.GetItem = function(pItemIndex) {
	// Get a default item object from the menu with an initial return value of -1
	var menuItemObj = this.MakeItemWithRetval(-1);
	// Do your own thing to get the item text and return value for the menu.
	// ...
	// Assuming itemText is the text to display in the menu and itemRetval is
	// the return value to return from the menu:
	menuItemObj.text = itemText;
	menuItemObj.retval = itemRetval;
	return menuItemObj; // The DDLightbarMenu object will use this when displaying the menu
};

If you want to set the currently selected item before calling GetVal() to allow user input,
you should call the SetSelectedItemIdx() function and pass the index to that.
lbMenu.SetSelectedItemIdx(5);

The property inputTimeoutMS sets the input timeout in milliseconds (defaults to 300000).
lbMenu.inputTimeoutMS = 300000; // 300,000 milliseconds (5 minutes)

The property mouseEnabled can be used to enable mouse support.  By default it is false.
When mouse support is enabled, there can be problems inputting the ESC key from the user.
lbMenu.mouseEnabled = true;

For selecting an item, it may be desirable to validate whether a user should be allowed
to select the item.  DDLightbarMenu has a member function it calls, ValidateSelectItem(),
to do just that.  It takes the selected item's return value and returns a boolean to signify
whether the user can select it.   By default, it just returns true (allowing the user to
select any item).  When the user can't choose a value, your code should output why.
To change its behavior, you can overwrite it as follows (assuming lbMenu
is a DDLightbarMenu object):

lbMenu.ValidateSelectItem = function(pItemRetval) {
	// Should the user be able to select the item with the return val indicated
	// by pItemRetval?
	if (yourValidationCode(pItemRetval))
		return true;
	else
	{
		console.print("* Can't choose " + pItemRetval + " because blah blah blah!\r\n\1p");
		return false;
	}
}

OnItemSelect is a function that is called when an item is selected, or toggled 
if multi-select is enabled.

Parameters:
 pItemRetval: The return value of the item selected
 pSelected: Boolean - Whether the item was selected or de-selected.  De-selection
            is possible when multi-select is enabled.
lbMenu.OnItemSelect = function(pItemRetval, pSelected)
{
	// Do something with pItemRetval.  pSelected tells whether the item was selected,
	// or de-selected if multi-select is enabled.
}

The property exitOnItemSelect specifies whether or not to exit the input loop when an item is
selected/submitted (i.e. with ENTER; not for toggling with multi-select).  This is true by
default.  It can be desirable to set this to false in some situations, such as when you want a
menu with a custom OnItemSelect() function specified and you want the menu to continue to
be displayed allowing the user to select an item.
lbMenu.exitOnItemSelect = false;

The 'key down' behavior can be called explicitly, if needed, by calling the DoKeyDown() function.
It takes 2 parameters: An object of selected item indexes (as passed to GetVal()) and, optionally,
the pre-calculated number of items.
lbMenu.DoKeyDown(pNumItems, pSelectedItemIndexes);


For screen refreshing, DDLightbarMenu includes the function DrawPartial(), which can be used to
redraw only a portion of the menu, specified by starting X & Y coordinates, width, and height.
The starting X & Y coordinates are relative to the upper-left corner of the menu (not absolute
screen coordinates) and start at (1, 1).  The function signature looks like this:
 DrawPartial(pStartX, pStartY, pWidth, pHeight, pSelectedItemIndexes)
The parameters:
 pStartX: The column of the character in the menu to start at
 pStartY: The row of the character in the menu to start at
 pWidth: The width of the content to draw
 pHeight: The height of the content to draw
 pSelectedItemIndexes: Optional - An object containing indexes of selected items

Another function, DrawPartialAbs(), provies the same functionality but with absolute screen coordinates
(also starting at (1, 1) in the upper-left corner):
 DrawPartialAbs(pStartX, pStartY, pWidth, pHeight, pSelectedItemIndexes)
The parameters:
 pStartX: The column of the character in the menu to start at
  pStartY: The row of the character in the menu to start at
 pWidth: The width of the content to draw
 pHeight: The height of the content to draw
 pSelectedItemIndexes: Optional - An object containing indexes of selected items
*/

if (typeof(require) === "function")
{
	require("sbbsdefs.js", "K_UPPER");
	require("mouse_getkey.js", "mouse_getkey");
}
else
{
	load("sbbsdefs.js");
	load("mouse_getkey.js");
}


// Keyboard keys
var KEY_ENTER = "\x0d";
// PageUp & PageDown keys - Synchronet 3.17 as of about December 18, 2017
// use CTRL-P and CTRL-N for PageUp and PageDown, respectively.  key_defs.js
// defines them as KEY_PAGEUP and KEY_PAGEDN (key_defs.js is loaded by
// sbbsdefs.js).
//var KEY_ESC = ascii(27);

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
var BLOCK1 = "\xB0"; // Dimmest block
var BLOCK2 = "\xB1";
var BLOCK3 = "\xB2";
var BLOCK4 = "\xDB"; // Brightest block

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
	this.scrollbarEnabled = false;
	this.borderEnabled = false;
	this.drawnAlready = false;
	this.colors = {
		itemColor: "\1n\1w\1" + "4", // Can be either a string or an array specifying colors within the item
		selectedItemColor: "\1n\1b\1" + "7", // Can be either a string or an array specifying colors within the item
		altItemColor: "\1n\1w\1" + "4", // Alternate item color.  Can be either a string or an array specifying colors within the item
		altSelectedItemColor: "\1n\1b\1" + "7", // Alternate selected item color.  Can be either a string or an array specifying colors within the item
		itemTextCharHighlightColor: "\1y\1h",
		borderColor: "\1n\1b",
		scrollbarScrollBlockColor: "\1h\1w",
		scrollbarBGColor: "\1h\1k"
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
	// Scrollbar information (characters, etc.)
	this.scrollbarInfo = {
		blockChar: BLOCK2,
		BGChar: BLOCK1,
		numSolidScrollBlocks: 0,
		numNonSolidScrollBlocks: 0,
		solidBlockLastStartRow: 0
	};

	this.selectedItemIdx = 0;
	this.topItemIdx = 0;
	this.wrapNavigation = true;
	this.hotkeyCaseSensitive = false;
	this.ampersandHotkeysInItems = true;
	this.multiSelect = false;
	this.maxNumSelections = -1; // -1 or 0 means no limit on the number of selections
	this.multiSelectItemChar = CHECK_CHAR; // The character to display for a selected item in multi-select mode
	this.numberedMode = false;
	this.itemNumLen = 0; // For the length of the item numbers in numbered mode
	this.additionalQuitKeys = ""; // A string of additional keys besides ESC to quit out of the menu
	this.additionalSelectItemKeys = ""; // A string of additional keys to select any item
	this.topBorderText = ""; // Text to display in the top border
	this.bottomBorderText = ""; // Text to display in the bottom border
	this.lastUserInput = null; // The user's last keypress when the menu was shown/used
	// nextDrawOnlyItemSubstr can be an object containing start & end properties, as
	// indexes (end is one past last index) for drawing shortened versions of items on the
	// next draw
	this.nextDrawOnlyItemSubstr = null;
	// nextDrawOnlyItems is an array specifying the indexes of the items to write
	// on the next draw.  If this is empty, then all items on the page will be drawn.
	this.nextDrawOnlyItems = [];

	// This is a regex to do a case-insensitive test for Synchronet attribute
	// codes in strings.
	// For one that looks at the whole word having only Synchronet attribute
	// codes, it would have ^ and $ around it, as in
	// /^\1[krgybmcw01234567hinpq,;\.dtl<>\[\]asz]$/i
	this.syncAttrRegex = /\1[krgybmcw01234567hinpq,;\.dtl<>\[\]asz]/i;

	// Whether or not to exit the input loop when an item is selected/submitted
	// (i.e. with ENTER; not for toggling with multi-select)
	this.exitOnItemSelect = true;

	this.inputTimeoutMS = 300000; // Input timeout in ms
	this.mouseEnabled = false;

	// Member functions
	this.Add = DDLightbarMenu_Add;
	this.Remove = DDLightbarMenu_Remove;
	this.RemoveAllItems = DDLightbarMenu_RemoveAllItems;
	this.NumItems = DDLightbarMenu_NumItems;
	this.GetItem = DDLightbarMenu_GetItem;
	this.SetPos = DDLightbarMenu_SetPos;
	this.SetSize = DDLightbarMenu_SetSize;
	this.SetWidth = DDLightbarMenu_SetWidth;
	this.SetHeight = DDLightbarMenu_SetHeight;
	this.Draw = DDLightbarMenu_Draw;
	this.DrawBorder = DDLightbarMenu_DrawBorder;
	this.WriteItem = DDLightbarMenu_WriteItem;
	this.WriteItemAtItsLocation = DDLightbarMenu_WriteItemAtItsLocation;
	this.DrawPartial = DDLightbarMenu_DrawPartial;
	this.DrawPartialAbs = DDLightbarMenu_DrawPartialAbs;
	this.GetItemText = DDLightbarMenu_GetItemText;
	this.Erase = DDLightbarMenu_Erase;
	this.SetItemHotkey = DDLightbarMenu_SetItemHotkey;
	this.AddItemHotkey = DDLightbarMenu_AddItemHotkey;
	this.RemoveItemHotkey = DDLightbarMenu_RemoveItemHotkey;
	this.RemoveItemHotkeys = DDLightbarMenu_RemoveItemHotkeys;
	this.RemoveAllItemHotkeys = DDLightbarMenu_RemoveAllItemHotkeys;
	this.GetMouseClickRegion = DDLightbarMenu_GetMouseClickRegion;
	this.GetVal = DDLightbarMenu_GetVal;
	this.DoKeyDown = DDLightbarMenu_DoKeyDown;
	this.SetBorderChars = DDLightbarMenu_SetBorderChars;
	this.SetColors = DDLightbarMenu_SetColors;
	this.GetNumItemsPerPage = DDLightbarMenu_GetNumItemsPerPage;
	this.GetTopItemIdxOfLastPage = DDLightbarMenu_GetTopItemIdxOfLastPage;
	this.SetTopItemIdxToTopOfLastPage = DDLightbarMenu_SetTopItemIdxToTopOfLastPage;
	this.AddAdditionalQuitKeys = DDLightbarMenu_AddAdditionalQuitKeys;
	this.QuitKeysIncludes = DDLightbarMenu_QuitKeysIncludes;
	this.ClearAdditionalQuitKeys = DDLightbarMenu_ClearAdditionalQuitKeys;
	this.AddAdditionalSelectItemKeys = DDLightbarMenu_AddAdditionalSelectItemKeys;
	this.SelectItemKeysIncludes = DDLightbarMenu_SelectItemKeysIncludes;
	this.ClearAdditionalSelectItemKeys = DDLightbarMenu_ClearAdditionalSelectItemKeys;
	this.DisplayInitialScrollbar = DDLightbarMenu_DisplayInitialScrollbar;
	this.UpdateScrollbar = DDLightbarMenu_UpdateScrollbar;
	this.CalcScrollbarBlocks = DDLightbarMenu_CalcScrollbarBlocks;
	this.CalcScrollbarSolidBlockStartRow = DDLightbarMenu_CalcScrollbarSolidBlockStartRow;
	this.UpdateScrollbarWithHighlightedItem = DDLightbarMenu_UpdateScrollbarWithHighlightedItem;
	this.CanShowAllItemsInWindow = DDLightbarMenu_CanShowAllItemsInWindow;
	this.MakeItemWithTextAndRetval = DDLightbarMenu_MakeItemWithTextAndRetval;
	this.MakeItemWithRetval = DDLightbarMenu_MakeItemWithRetval;
	this.ItemUsesAltColors = DDLightbarMenu_ItemUsesAltColors;
	this.GetColorForItem = DDLightbarMenu_GetColorForItem;
	this.GetSelectedColorForItem = DDLightbarMenu_GetSelectedColorForItem;
	this.SetSelectedItemIdx = DDLightbarMenu_SetSelectedItemIdx;

	// ValidateSelectItem is a function for validating that the user can select an item.
	// It takes the selected item's return value and returns a boolean to signify whether
	// the user can select it.
	this.ValidateSelectItem = function(pItemRetval) { return true; }

	// OnItemSelect is a function that is called when an item is selected, or toggled 
	// if multi-select is enabled.
	//
	// Parameters:
	//  pItemRetval: The return value of the item selected
	//  pSelected: Boolean - Whether the item was selected or de-selected.  De-selection
	//             is possible when multi-select is enabled.
	this.OnItemSelect = function(pItemRetval, pSelected) { }

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
	var item = getDefaultMenuItem();
	item.text = pText;
	item.retval = (pRetval == undefined ? this.NumItems() : pRetval);
	// If pHotkey is defined, then use it as the hotkey.  Otherwise, if
	// ampersandHotkeysInItems is true, look for the first & in the item text
	// and if there's a non-space after it, then use that character as the
	// hotkey.
	if (typeof(pHotkey) == "string")
		item.hotkeys += pHotkey;

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
					item.hotkeys += nextChar;
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

// Returns the number of items in the menu
function DDLightbarMenu_NumItems()
{
	return this.items.length;
}

// Returns an item from the list
//
// Parameters:
//  pItemIndex: The index of the item to get
//
// Return value: The item (or null if pItemIndex is invalid)
function DDLightbarMenu_GetItem(pItemIndex)
{
	if ((pItemIndex < 0) || (pItemIndex >= this.items.length))
		return null;
	return this.items[pItemIndex];
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
//
// Parameters:
//  pSelectedItemIndexes: An object that can contain multiple indexes of selected
//                        items.  Only for multi-select mode.  These are used
//                        for drawing a marking character in the item text.
//  pDrawBorders: Optional boolean - Whether or not to draw the borders, if borders
//                are enabled.  Defaults to true.
//  pDrawScrollbar: Optional boolean - Whether or not to draw the scrollbar, if
//                  the scrollbar is enabled.  Defaults to this.scrollbarEnabled, and the scrollbar
//                  will only be drawn if not all items can be shown in a single page.
function DDLightbarMenu_Draw(pSelectedItemIndexes, pDrawBorders, pDrawScrollbar)
{
	var drawBorders = (typeof(pDrawBorders) == "boolean" ? pDrawBorders : true);
	var drawScrollbar = (typeof(pDrawScrollbar) == "boolean" ? pDrawScrollbar : true);

	var curPos = { x: this.pos.x, y: this.pos.y }; // For writing the menu items
	var itemLen = this.size.width;
	// If borders are enabled, then adjust the item length, starting x, and starting
	// y accordingly, and draw the border.
	if (this.borderEnabled)
	{
		itemLen -= 2;
		++curPos.x;
		++curPos.y;
		if (drawBorders)
			this.DrawBorder();
	}
	if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
		--itemLen; // Leave room for the scrollbar in the item lengths
	// If the scrollbar is enabled & needed and we are to update it,
	// then calculate the scrollbar blocks and update it on the screen.
	if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow() && drawScrollbar)
	{
		this.CalcScrollbarBlocks();
		if (!this.drawnAlready)
			this.DisplayInitialScrollbar(this.pos.y);
		else
			this.UpdateScrollbarWithHighlightedItem(true);
	}
	// For numbered mode, we'll need to know the length of the longest item number
	// so that we can use that space to display the item numbers.
	if (this.numberedMode)
	{
		this.itemNumLen = this.NumItems().toString().length;
		itemLen -= this.itemNumLen;
		--itemLen; // Have a space for separation between the numbers and items
	}

	// Write the menu items, only up to the height of the menu
	var numPossibleItems = (this.borderEnabled ? this.size.height - 2 : this.size.height);
	var numItemsWritten = 0;
	var writeTheItem = true;
	for (var idx = this.topItemIdx; (idx < this.NumItems()) && (numItemsWritten < numPossibleItems); ++idx)
	{
		writeTheItem = ((this.nextDrawOnlyItems.length == 0) || (this.nextDrawOnlyItems.indexOf(idx) > -1));
		if (writeTheItem)
		{
			console.gotoxy(curPos.x, curPos.y);
			var showMultiSelectMark = (this.multiSelect && (typeof(pSelectedItemIndexes) == "object") && pSelectedItemIndexes.hasOwnProperty(idx));
			this.WriteItem(idx, itemLen, idx == this.selectedItemIdx, showMultiSelectMark, curPos.x, curPos.y);
		}
		++curPos.y;
		++numItemsWritten;
	}
	// If there are fewer items than the height of the menu, then write blank lines to fill
	// the rest of the height of the menu.
	if (numItemsWritten < numPossibleItems)
	{
		var numberFormatStr = "%" + this.itemNumLen + "s ";
		var itemFormatStr = "%-" + itemLen + "s";
		for (; numItemsWritten < numPossibleItems; ++numItemsWritten)
		{
			writeTheItem = ((this.nextDrawOnlyItems.length == 0) || (this.nextDrawOnlyItems.indexOf(numItemsWritten) > -1));
			if (writeTheItem)
			{
				console.gotoxy(curPos.x, curPos.y++);
				console.print("\1n");
				if (this.numberedMode)
					printf(numberFormatStr, "");
				var itemText = addAttrsToString(format(itemFormatStr, ""), this.colors.itemColor);
				console.print(itemText);
			}
		}
	}

	this.drawnAlready = true;
	this.nextDrawOnlyItemSubstr = null;
	this.nextDrawOnlyItems = [];
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
		// Display the top border text (if any) in the top border.  Ensure the text
		// length is no longer than the maximum possible length (lineLen).
		var borderText = shortenStrWithAttrCodes(this.topBorderText, lineLen);
		console.print("\1n" + borderText + "\1n" + this.colors.borderColor);
		var remainingLineLen = lineLen - console.strlen(borderText);
		for (var i = 0; i < remainingLineLen; ++i)
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
		// Display the bottom border text (if any) in the bottom border.  Ensure the text
		// length is no longer than the maximum possible length (lineLen).
		var borderText = shortenStrWithAttrCodes(this.bottomBorderText, lineLen);
		console.print("\1n" + borderText + "\1n" + this.colors.borderColor);
		var remainingLineLen = lineLen - console.strlen(borderText);
		for (var i = 0; i < remainingLineLen; ++i)
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
//  pScreenX: Optional - The horizontal screen coordinate of the start of the item
//  pScreenY: Optional - The vertical screen coordinate of the start of the item
function DDLightbarMenu_WriteItem(pIdx, pItemLen, pHighlight, pSelected, pScreenX, pScreenY)
{
	var itemText = this.GetItemText(pIdx, pItemLen, pHighlight, pSelected);
	// If this.nextDrawOnlyItemSubstr is an object with start & end properties,
	// then create a string that is shortened from itemText from those start & end
	// indexes, and add color to it.
	// Otherwise, just print the full item text.
	if ((this.nextDrawOnlyItemSubstr != null) && (typeof(this.nextDrawOnlyItemSubstr) == "object") && this.nextDrawOnlyItemSubstr.hasOwnProperty("start") && this.nextDrawOnlyItemSubstr.hasOwnProperty("end") && (typeof(pScreenX) == "number") && (typeof(pScreenY) == "number"))
	{
		var len = this.nextDrawOnlyItemSubstr.end - this.nextDrawOnlyItemSubstr.start;
		var shortenedText = substrWithAttrCodes(itemText, this.nextDrawOnlyItemSubstr.start, len);
		console.gotoxy(pScreenX+this.nextDrawOnlyItemSubstr.start, pScreenY);
		console.print(shortenedText + "\1n");
	}
	else
		console.print(itemText + "\1n");
}

// Writes a menu item at its location on the menu.  This should only be called
// if the item is on the current page.
//
// Parameters:
//  pIdx: The index of the item to write
//  pHighlight: Whether or not the item should be highlighted
//  pSelected: Whether or not the item is selected
function DDLightbarMenu_WriteItemAtItsLocation(pIdx, pHighlight, pSelected)
{
	if (this.borderEnabled)
		console.gotoxy(this.pos.x+1, this.pos.y+pIdx-this.topItemIdx+1);
	else
		console.gotoxy(this.pos.x, this.pos.y+pIdx-this.topItemIdx);
	this.WriteItem(pIdx, null, pHighlight, pSelected);
}

// Draws part of the menu, starting at a certain location within the menu and
// with a given width & height (for screen refreshing).  The start X and Y location
// are relative to the menu (not the screen), and they start at (1, 1) in the upper-left
//
// Parameters:
//  pStartX: The column of the character in the menu to start at
//  pStartY: The row of the character in the menu to start at
//  pWidth: The width of the content to draw
//  pHeight: The height of the content to draw
//  pSelectedItemIndexes: Optional - An object containing indexes of selected items
function DDLightbarMenu_DrawPartial(pStartX, pStartY, pWidth, pHeight, pSelectedItemIndexes)
{
	// Sanity check the parameters
	if (typeof(pStartX) !== "number" || typeof(pStartY) !== "number" || typeof(pWidth) !== "number" || typeof(pHeight) !== "number")
		return;
	if (pStartX < 1 || pStartX > this.size.width)
		return;
	if (pStartY < 1 || pStartY > this.size.height)
		return;

	// Fix the width & height if needed
	var width = pWidth;
	if (width > (this.size.width - pStartX + 1))
		width = (this.size.width - pStartX + 1);
	var height = pHeight;
	if (height > (this.size.height - pStartY + 1))
		height = (this.size.height - pStartY + 1);

	var selectedItemIndexes = { }; // For multi-select mode
	if (typeof(pSelectedItemIndexes) == "object")
		selectedItemIndexes = pSelectedItemIndexes;

	// If borders are enabled, draw any border characters in the region first
	// The X & Y locations are 1-based
	var lastLineNum = (pStartY  + this.pos.y + height) - 1; // Last line # on the screen
	if (lastLineNum > this.pos.y + this.size.height - 1)
		lastLineNum = this.pos.y + this.size.height - 1;
	if (this.borderEnabled)
	{
		var lastX = pStartX + width - 1;
		for (var lineNum = pStartY + this.pos.y - 1; lineNum <= lastLineNum; ++lineNum)
		{
			// Top line
			if (lineNum == this.pos.y)
			{
				console.print("\1n" + this.colors.borderColor);
				for (var posX = pStartX; posX <= lastX; ++posX)
				{
					console.gotoxy(posX, lineNum);
					if (posX == this.pos.x)
						console.print(this.borderChars.upperLeft);
					else if (posX == this.pos.x + this.size.width - 1)
						console.print(this.borderChars.upperRight);
					else
						console.print(this.borderChars.top);
				}
			}
			// Bottom line
			else if (lineNum == this.pos.y + this.size.height - 1)
			{
				console.print("\1n" + this.colors.borderColor);
				for (var posX = pStartX; posX <= lastX; ++posX)
				{
					console.gotoxy(posX, lineNum);
					if (posX == this.pos.x)
						console.print(this.borderChars.lowerLeft);
					else if (posX == this.pos.x + this.size.width - 1)
						console.print(this.borderChars.lowerRight);
					else
						console.print(this.borderChars.bottom);
				}
			}
			// Somewhere between the top & bottom line
			else
			{
				var printedBorderColor = false;
				for (var posX = pStartX; posX <= lastX; ++posX)
				{
					console.gotoxy(posX, lineNum);
					if (posX == this.pos.x)
					{
						if (!printedBorderColor)
						{
							console.print("\1n" + this.colors.borderColor);
							printedBorderColor = true;
						}
						console.print(this.borderChars.left);
					}
					else if (posX == this.pos.x + this.size.width - 1)
					{
						if (!printedBorderColor)
						{
							console.print("\1n" + this.colors.borderColor);
							printedBorderColor = true;
						}
						console.print(this.borderChars.right);
					}
				}
			}
		}
	}
	// Calculate the width and starting index of the menu items
	// Note that pStartX is relative to the menu, not the screen
	var itemLen = width;
	var writeMenuItems = true; // Might not if the draw area only includes the scrollbar or border
	var itemTxtStartIdx = pStartX - 1;
	if (this.borderEnabled)
	{
		if (itemTxtStartIdx > 0)
			--itemTxtStartIdx; // pStartX - 2
		if (pStartX == 1)
			--itemLen;
		// Starts on 2 & width is 5: 2, 3, 4, 5, 6
		var lastCol = this.pos.x + pStartX + width - 1;
		if (this.pos.x + pStartX + width - 1 >= lastCol) // The last column drawn will contain the right border char
			--itemLen;
		if ((pStartX == 1 && width == 1) || pStartX == this.size.width)
			writeMenuItems  = false;
		else if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow() && pStartX == this.size.width-1)
			writeMenuItems = false;
	}
	if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
	{
		var scrollbarCol = this.borderEnabled ? this.pos.x + this.size.width - 2 : this.pos.x + this.size.width - 1;
		if (this.pos.x + pStartX + width - 1 >= scrollbarCol) // The last column drawn includes the scrollbar
			--itemLen;
		if (!this.borderEnabled && pStartX == this.size.width)
			writeMenuItems = false;
		// Just draw the whole srollbar to ensure it's updated
		this.DisplayInitialScrollbar(this.scrollbarInfo.solidBlockLastStartRow, this.scrollbarInfo.numSolidScrollBlocks);
	}
	if (itemTxtStartIdx < 0)
		itemTxtStartIdx = 0;
	// Write the menu items
	if (writeMenuItems)
	{
		var blankItemTextFormatStr = "\1n%" + itemLen + "s";
		for (var lineNum = pStartY + this.pos.y - 1; lineNum <= lastLineNum; ++lineNum)
		{
			var startX = pStartX;
			// If borders are enabled, skip the top & bottom lines since borders were already drawn
			if (this.borderEnabled)
			{
				if (lineNum == this.pos.y || lineNum == lastLineNum)
					continue;
				else
				{
					if (pStartX + this.pos.x - 1 == this.pos.x)
						++startX;
				}
			}
			// Write the menu item text
			var itemIdx = this.topItemIdx + (lineNum - this.pos.y);
			if (this.borderEnabled) --itemIdx;
			var highlightItem = itemIdx == this.selectedItemIdx;
			var itemText = this.GetItemText(itemIdx, null, highlightItem, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
			var shortenedText = substrWithAttrCodes(itemText, itemTxtStartIdx, itemLen);
			// If shortenedText is empty (perhaps there's no menu item for this line),
			// then make shortenedText consist of all spaces at the proper length
			if (shortenedText.length == 0)
				shortenedText = format(blankItemTextFormatStr, "");
			console.gotoxy(startX, lineNum);
			console.print(shortenedText + "\1n");
		}
	}
}
// Draws part of the menu, starting at a certain location within the menu and
// with a given width & height (for screen refreshing).  For this version, the start X
// and Y location are absolute on the screen.  They start at (1, 1) in the upper-left.
//
// Parameters:
//  pStartX: The column of the character in the menu to start at
//  pStartY: The row of the character in the menu to start at
//  pWidth: The width of the content to draw
//  pHeight: The height of the content to draw
//  pSelectedItemIndexes: Optional - An object containing indexes of selected items
function DDLightbarMenu_DrawPartialAbs(pStartX, pStartY, pWidth, pHeight, pSelectedItemIndexes)
{
	if (typeof(pStartX) !== "number" || typeof(pStartY) !== "number" || typeof(pWidth) !== "number" || typeof(pHeight) !== "number")
		return;

	// Calculate the start X & Y coordinates relative to the menu (1-based), and adjust height &
	// width if necessary.  Then draw partial.
	var height = pHeight;
	var width = pWidth;
	var startX = pStartX - this.pos.x + 1;
	var startY = pStartY - this.pos.y + 1;
	if (startX < 1)
	{
		var XDiff = 1 - startX;
		startX += XDiff;
		width -= XDiff;
	}
	if (startY < 1)
	{
		var YDiff = 1 - startY;
		startY += YDiff;
		height -= YDiff;
	}
	this.DrawPartial(startX, startY, width, height, pSelectedItemIndexes);
}


// Gets the text of a menu item with colors applied
//
// Parameters:
//  pIdx: The index of the item to get
//  pItemLen: Optional - Calculated length of the item (in case the scrollbar is showing).
//            If this is not given, then this will be calculated.
//  pHighlight: Optional - Whether or not to highlight the item.  If this is not given,
//              the item will be highlighted based on whether the current selected item
//              matches the given index, pIdx.
//  pSelected: Optional - Whether or not this item is selected (mainly intended for multi-select
//             mode).  Defaults to false.  If true, then a mark character will be displayed
//             at the end of the item's text.
function DDLightbarMenu_GetItemText(pIdx, pItemLen, pHighlight, pSelected)
{
	var itemText = "";
	var numItems = this.NumItems();
	if ((pIdx >= 0) && (pIdx < numItems))
	{
		var itemLen = 0;
		if (typeof(pItemLen) == "number")
			itemLen = pItemLen;
		else
		{
			itemLen = this.size.width;
			// If the scrollbar is enabled & we can't show all items in the window,
			// then subtract 1 from itemLen to make room for the srollbar.
			if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
				--itemLen;
			// If borders are enabled, then subtract another 2 from itemLen to make
			// room for the left & right borders
			if (this.borderEnabled)
				itemLen -= 2;
			// For numbered mode, we'll need to know the length of the longest item number
			// so that we can use that space to display the item numbers.
			if (this.numberedMode)
			{
				this.itemNumLen = numItems.toString().length;
				itemLen -= this.itemNumLen;
				--itemLen; // Have a space for separation between the numbers and items
			}
		}

		// Decide which color(s) to use for the item text
		var menuItem = this.GetItem(pIdx);
		var normalItemColor;
		var selectedItemColor;
		if (menuItem.itemColor != null)
			normalItemColor = menuItem.itemColor;
		else
			normalItemColor = (menuItem.useAltColors ? this.colors.altItemColor : this.colors.itemColor);
		if (menuItem.itemSelectedColor != null)
			selectedItemColor = menuItem.itemSelectedColor;
		else
			selectedItemColor = (menuItem.useAltColors ? this.colors.altSelectedItemColor : this.colors.selectedItemColor);
		var itemColor = "";
		if (typeof(pHighlight) == "boolean")
			itemColor = (pHighlight ? selectedItemColor : normalItemColor);
		else
			itemColor = (pIdx == this.selectedItemIdx ? selectedItemColor : normalItemColor);
		var selected = (typeof(pSelected) == "boolean" ? pSelected : false);

		// Get the item text, and truncate it to the displayable item width.
		// Use strip_ctrl to ensure there are no attribute codes, since we will
		// apply our own.  This might be only a temporary item returned by a
		// replaced GetItem(), so we just have to strip_ctrl() it here.
		itemText = strip_ctrl(menuItem.text);
		if (itemTextDisplayableLen(itemText, this.ampersandHotkeysInItems) > itemLen)
			itemText = itemText.substr(0, itemLen);
		// If the item text is empty, then fill it with spaces for the item length
		// so that the line's colors/attributes will be applied for the whole line
		// when written
		if (strip_ctrl(itemText).length == 0)
			itemText = format("%" + itemLen + "s", "");
		// Add the item color to the item text
		itemText = addAttrsToString(itemText, itemColor);
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
		if (selected)
		{
			var itemTextLen = itemTextDisplayableLen(itemText, this.ampersandHotkeysInItems);
			if (itemTextLen < this.size.width)
			{
				var numSpaces = itemLen - itemTextLen - 2;
				// Kludge? If numSpaces is positive, append a space and then the selected
				// character,  Otherwise, we'll need to replace the last character in
				// itemText with the selected character.
				if (numSpaces > 0)
					itemText += format("%" + numSpaces + "s %s", "", this.multiSelectItemChar);
				else
					itemText = itemText.substr(0, itemText.length-1) + this.multiSelectItemChar;
			}
			else
			{
				// itemText should already be shortened to only the menu width, so
				// take 2 characters off the end and add a space and mark character
				itemText = itemText.substr(0, itemText.length-2) + " " + this.multiSelectItemChar;
			}
		}
		// Ensure the item text fills the width of the menu (in case there's a
		// background color, it should be used for the entire width of the item
		// text).  Then write the item.
		var currentTextLen = itemTextDisplayableLen(itemText, this.ampersandHotkeysInItems);
		if (currentTextLen < itemLen)
			itemText += format("%" + +(itemLen-currentTextLen) + "s", ""); // Append spaces to the end of itemText
		// If in numbered mode, add the item number to the front of the item text.
		if (this.numberedMode)
			itemText = format("\1n%" + this.itemNumLen + "d ", pIdx+1) + itemText;
	}
	return itemText;
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
		this.items[pIdx].hotkeys = pHotkey;
}

// Adds a hotkey for a menu item (in addition to the item's other hotkeys)
//
// Parameters:
//  pIdx: The index of the menu item
//  pHotkey: The hotkey to add for the menu item
function DDLightbarMenu_AddItemHotkey(pIdx, pHotkey)
{
	if ((typeof(pIdx) == "number") && (pIdx >= 0) && (pIdx < this.items.length) && (typeof(pHotkey) == "string") && (this.items[pIdx].hotkeys.indexOf(pHotkey) == -1))
		this.items[pIdx].hotkeys += pHotkey;
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
		var hotkeyIdx = this.items[pIdx].hotkeys.indexOf(pHotkey);
		while (hotkeyIdx > -1)
		{
			this.items[pIdx].hotkeys = this.items[pIdx].hotkeys.substr(0, hotkeyIdx) + this.items[pIdx].hotkeys.substr(hotkeyIdx+1);
			hotkeyIdx = this.items[pIdx].hotkeys.indexOf(pHotkey);
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
		this.items[pIdx].hotkeys = "";
}

// Removes the hotkeys from all items
function DDLightbarMenu_RemoveAllItemHotkeys()
{
	for (var i = 0; i < this.items.length; ++i)
		this.items[i].hotkeys = "";
}

// Returns an object specifying the mouse valid click region for the menu,
// with properties left, right, top, and bottom.
function DDLightbarMenu_GetMouseClickRegion()
{
	var clickRegion = {
		left: this.pos.x,
		right: this.pos.x + this.size.width - 1,
		top: this.pos.y,
		bottom: this.pos.y + this.size.height - 1
	};
	if (this.borderEnabled)
	{
		++clickRegion.left;
		++clickRegion.top;
		--clickRegion.right;
		--clickRegion.bottom;
	}
	return clickRegion;
}

// Waits for user input, optionally drawing the menu first.
//
// Parameters:
//  pDraw: Optional - Whether or not to draw the menu first.  By default, the
//         menu will be drawn first.
//  pSelectedItemIndexes: Optional - An object containing indexes of selected items
function DDLightbarMenu_GetVal(pDraw, pSelectedItemIndexes)
{
	this.lastUserInput = null;

	var numItems = this.NumItems();
	if (numItems == 0)
		return null;

	if (typeof(this.lastMouseClickTime) == "undefined")
		this.lastMouseClickTime = -1;

	var draw = (typeof(pDraw) == "boolean" ? pDraw : true);
	if (draw)
	{
		this.Draw(pSelectedItemIndexes);
		if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
			this.DisplayInitialScrollbar(this.scrollbarInfo.solidBlockLastStartRow);
	}

	// User input loop
	var userChoices = null; // For multi-select mode
	var selectedItemIndexes = { }; // For multi-select mode
	if (typeof(pSelectedItemIndexes) == "object")
		selectedItemIndexes = pSelectedItemIndexes;
	var retVal = null; // For single-choice mode
	// mouseInputOnly_continue specifies whether to continue to the
	// next iteration if the mouse was clicked & there's no need to
	// process user input further
	var mouseInputOnly_continue = false;
	var continueOn = true;
	while (continueOn)
	{
		if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
			this.UpdateScrollbarWithHighlightedItem();

		mouseInputOnly_continue = false;

		// TODO: With mouse_getkey(), it seems you need to press ESC twice
		// to get the ESC key and exit the menu
		var inputMode = K_NOECHO|K_NOSPIN|K_NOCRLF;
		var mk = null; // Will be used for mouse support
		var mouseNoAction = false;
		if (this.mouseEnabled)
		{
			mk = mouse_getkey(inputMode, this.inputTimeoutMS > 1 ? this.inputTimeoutMS : undefined, this.mouseEnabled);
			if (mk.mouse !== null)
			{
				// See if the user clicked anywhere in the region where items are
				// listed or the scrollbar
				var clickRegion = this.GetMouseClickRegion();
				// Button 0 is the left/main mouse button
				if (mk.mouse.press && (mk.mouse.button == 0) && (mk.mouse.motion == 0) &&
					(mk.mouse.x >= clickRegion.left) && (mk.mouse.x <= clickRegion.right) &&
					(mk.mouse.y >= clickRegion.top) && (mk.mouse.y <= clickRegion.bottom))
				{
					var isDoubleClick = ((this.lastMouseClickTime > -1) && (system.timer - this.lastMouseClickTime <= 0.4));

					// If the scrollbar is enabled, then see if the mouse click was
					// in the scrollbar region.  If below the scrollbar bright blocks,
					// then we'll want to do a PageDown.  If above the scrollbar bright
					// blocks, then we'll want to do a PageUp.
					var scrollbarX = this.pos.x + this.size.width - 1;
					if (this.borderEnabled)
						--scrollbarX;
					if ((mk.mouse.x == scrollbarX) && this.scrollbarEnabled)
					{
						var scrollbarSolidBlockEndRow = this.scrollbarInfo.solidBlockLastStartRow + this.scrollbarInfo.numSolidScrollBlocks - 1;
						if (mk.mouse.y < this.scrollbarInfo.solidBlockLastStartRow)
							this.lastUserInput = KEY_PAGEUP;
						else if (mk.mouse.y > scrollbarSolidBlockEndRow)
							this.lastUserInput = KEY_PAGEDN;
						else
						{
							// Mouse click no-action
							// TODO: Can we detect if they're holding the mouse down
							// and scroll while the user holds the mouse & scrolls on
							// the scrollbar?
							this.lastUserInput = "";
							mouseNoAction = true;
							mouseInputOnly_continue = true;
						}
					}
					else
					{
						// The user didn't click on the scrollbar or the scrollbar
						// isn't enabled.
						// For a double-click, if multi-select is enabled, set the
						// last user input to a space to select/de-select the item.
						if (isDoubleClick)
						{
							if (this.multiSelect)
								this.lastUserInput = " ";
							else
							{
								// No mouse action
								this.lastUserInput = "";
								mouseNoAction = true;
								mouseInputOnly_continue = true;
							}
						}
						else
						{
							// Make the clicked-on item the currently highlighted
							// item.  Only select the item if the index is valid.
							var topItemY = (this.borderEnabled ? this.pos.y + 1 : this.pos.y);
							var distFromTopY = mk.mouse.y - topItemY;
							var itemIdx = this.topItemIdx + distFromTopY;
							if ((itemIdx >= 0) && (itemIdx < this.NumItems()))
							{
								this.WriteItemAtItsLocation(this.selectedItemIdx, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
								this.selectedItemIdx = itemIdx;
								this.WriteItemAtItsLocation(this.selectedItemIdx, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
							}
							// Don't have the later code do anything
							this.lastUserInput = "";
							mouseNoAction = true;
							mouseInputOnly_continue = true;
						}
					}

					this.lastMouseClickTime = system.timer;
				}
				else
				{
					// The mouse click is outside the click region.  Set the appropriate
					// variables for mouse no-action.
					// TODO: Perhaps this may also need to be done in some places above
					// where no action needs to be taken
					this.lastUserInput = "";
					mouseNoAction = true;
					mouseInputOnly_continue = true;
				}
			}
			else
			{
				// mouse is null, so a keybaord key must have been pressed
				this.lastUserInput = mk.key;
			}
		}
		else // this.mouseEnabled is false
		{
			this.lastUserInput = getKeyWithESCChars(inputMode, this.inputTimeoutMS);
		}

		// If no further input processing needs to be done due to a mouse click
		// action, then continue to the next loop iteration.
		if (mouseInputOnly_continue)
			continue;

		// Take the appropriate action based on the user's last input/keypress
		if ((this.lastUserInput == KEY_ESC) || (this.QuitKeysIncludes(this.lastUserInput)))
		{
			// Only exit if there was not a no-action mouse click
			// TODO: Is this logic good and clean?
			var goAheadAndExit = true;
			if (mk !== null && mk.mouse !== null)
			{
				goAheadAndExit = !mouseNoAction; // Only really needed with an input timer?
			}
			if (goAheadAndExit)
			{
				continueOn = false;
				// Ensure any returned choice objects are null/empty to signal
				// that the user aborted
				userChoices = null; // For multi-select mode
				selectedItemIndexes = { }; // For multi-select mode
				retVal = null; // For single-choice mode
			}
		}
		else if ((this.lastUserInput == KEY_UP) || (this.lastUserInput == KEY_LEFT))
		{
			if (this.selectedItemIdx > 0)
			{
				// Draw the current item in regular colors
				this.WriteItemAtItsLocation(this.selectedItemIdx, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				--this.selectedItemIdx;
				// Draw the new current item in selected colors
				// If the selected item is above the top of the menu, then we'll need to
				// scroll the items down.
				if (this.selectedItemIdx < this.topItemIdx)
				{
					--this.topItemIdx;
					this.Draw(selectedItemIndexes);
				}
				else
				{
					// The selected item is not above the top of the menu, so we can
					// just draw the selected item highlighted.
					this.WriteItemAtItsLocation(this.selectedItemIdx, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				}
			}
			else
			{
				// selectedItemIdx is 0.  If wrap navigation is enabled, then go to the
				// last item.
				if (this.wrapNavigation)
				{
					// Draw the current item in regular colors
					//this.WriteItemAtItsLocation(pIdx, pHighlight, pSelected)
					this.WriteItemAtItsLocation(this.selectedItemIdx, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					// Go to the last item and scroll to the bottom if necessary
					this.selectedItemIdx = numItems - 1;
					var oldTopItemIdx = this.topItemIdx;
					var numItemsPerPage = (this.borderEnabled ? this.size.height - 2 : this.size.height);
					this.topItemIdx = numItems - numItemsPerPage;
					if (this.topItemIdx < 0)
						this.topItemIdx = 0;
					if (this.topItemIdx != oldTopItemIdx)
						this.Draw(selectedItemIndexes);
					else
					{
						// Draw the new current item in selected colors
						this.WriteItemAtItsLocation(this.selectedItemIdx, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					}
				}
			}
		}
		else if ((this.lastUserInput == KEY_DOWN) || (this.lastUserInput == KEY_RIGHT))
		{
			this.DoKeyDown(selectedItemIndexes, numItems);
		}
		else if (this.lastUserInput == KEY_PAGEUP)
		{
			// Only do this if we're not already at the top of the list
			if (this.topItemIdx > 0)
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
					this.Draw(selectedItemIndexes);
				}
				else
				{
					// The top index is the top index for the last page.
					// If wrapping is enabled, then go back to the first page.
					if (this.wrapNavigation)
					{
						var topIndexForLastPage = numItems - numItemsPerPage;
						if (topIndexForLastPage < 0)
							topIndexForLastPage = 0;
						else if (topIndexForLastPage >= numItems)
							topIndexForLastPage = numItems - 1;

						this.topItemIdx = topIndexForLastPage;
						this.selectedItemIdx = topIndexForLastPage;
						this.Draw(selectedItemIndexes);
					}
				}
			}
			else
			{
				// We're already showing the first page of items.
				// If the currently selected item is not the first
				// item, then make it so.
				if (this.selectedItemIdx > 0)
				{
					this.WriteItemAtItsLocation(this.selectedItemIdx, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					this.selectedItemIdx = 0;
					this.WriteItemAtItsLocation(this.selectedItemIdx, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				}
			}
		}
		else if (this.lastUserInput == KEY_PAGEDN)
		{
			var numItemsPerPage = (this.borderEnabled ? this.size.height - 2 : this.size.height);
			// Only do the pageDown if we're not showing the last item already
			var lastItemIdx = this.NumItems() - 1;
			if (lastItemIdx > this.topItemIdx+numItemsPerPage-1)
			{
				// Figure out the top index for the last page.
				var topIndexForLastPage = numItems - numItemsPerPage;
				if (topIndexForLastPage < 0)
					topIndexForLastPage = 0;
				else if (topIndexForLastPage >= numItems)
					topIndexForLastPage = numItems - 1;
				if (topIndexForLastPage != this.topItemIdx)
				{
					// Update the selected & top item indexes
					this.selectedItemIdx += numItemsPerPage;
					this.topItemIdx += numItemsPerPage;
					if (this.selectedItemIdx >= topIndexForLastPage)
						this.selectedItemIdx = topIndexForLastPage;
					if (this.topItemIdx > topIndexForLastPage)
						this.topItemIdx = topIndexForLastPage;
					this.Draw(selectedItemIndexes);
				}
				else
				{
					// The top index is the top index for the last page.
					// If wrapping is enabled, then go back to the first page.
					if (this.wrapNavigation)
					{
						this.topItemIdx = 0;
						this.selectedItemIdx = 0;
					}
					this.Draw(selectedItemIndexes);
				}
			}
			else
			{
				// We're already showing the last page of items.
				// If the currently selected item is not the last
				// item, then make it so.
				if (this.selectedItemIdx < lastItemIdx)
				{
					this.WriteItemAtItsLocation(this.selectedItemIdx, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					this.selectedItemIdx = lastItemIdx;
					this.WriteItemAtItsLocation(this.selectedItemIdx, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				}
			}
		}
		else if (this.lastUserInput == KEY_HOME)
		{
			// Go to the first item in the list
			if (this.selectedItemIdx > 0)
			{
				// If the current item index is not on first current page, then scroll.
				// Otherwise, draw more efficiently by drawing the current item in
				// regular colors and the first item in highlighted colors.
				this.topItemIdx = 0;
				var numItemsPerPage = this.GetNumItemsPerPage();
				if (this.selectedItemIdx >= 0 + numItemsPerPage)
				{
					this.selectedItemIdx = 0;
					this.Draw(pSelectedItemIndexes, false);
				}
				else
				{
					// We're already on the first page, so we can re-draw the
					// 2 items more efficiently.
					// Draw the current item in regular colors
					if (this.borderEnabled)
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					else
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					this.selectedItemIdx = 0;
					// Draw the new current item in selected colors
					if (this.borderEnabled)
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					else
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				}
			}
		}
		else if (this.lastUserInput == KEY_END)
		{
			// Go to the last item in the list
			var numItemsPerPage = this.GetNumItemsPerPage();
			if (this.selectedItemIdx < numItems-1)
			{
				var lastPossibleTop = numItems - numItemsPerPage;
				if (lastPossibleTop < 0)
					lastPossibleTop = 0;
				var lastItemIdx = numItems - 1;
				// If the last item index is below the current page, then scroll.
				// Otherwise, draw more efficiently by drawing the current item in
				// regular colors and the last item in highlighted colors.
				if (lastItemIdx >= this.topItemIdx + numItemsPerPage)
				{
					this.topItemIdx = lastPossibleTop;
					this.selectedItemIdx = lastItemIdx;
					this.Draw(pSelectedItemIndexes, false);
				}
				else
				{
					// We're already on the last page, so we can re-draw the
					// 2 items more efficiently.
					// Draw the current item in regular colors
					if (this.borderEnabled)
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					else
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, false, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
					this.selectedItemIdx = this.topItemIdx + numItemsPerPage - 1;
					if (this.selectedItemIdx >= numItems)
						this.selectedItemIdx = lastItemIdx;
					// Draw the new current item in selected colors
					if (this.borderEnabled)
						console.gotoxy(this.pos.x+1, this.pos.y+this.selectedItemIdx-this.topItemIdx+1);
					else
						console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, true, selectedItemIndexes.hasOwnProperty(this.selectedItemIdx));
				}
			}
		}
		// Enter key or additional select-item key: Select the item & quit out of the input loop
		else if ((this.lastUserInput == KEY_ENTER) || (this.SelectItemKeysIncludes(this.lastUserInput)))
		{
			// Let the user select the item if ValidateSelectItem() returns true
			var allowSelectItem = true;
			if (typeof(this.ValidateSelectItem) === "function")
				allowSelectItem = this.ValidateSelectItem(this.GetItem(this.selectedItemIdx).retval);
			if (allowSelectItem)
			{
				// If multi-select is enabled and if the user hasn't made any choices,
				// then add the current item to the user choices.  Otherwise, choose
				// the current item.  Then exit.
				if (this.multiSelect)
				{
					if (Object.keys(selectedItemIndexes).length == 0)
						selectedItemIndexes[+(this.selectedItemIdx)] = true;
				}
				else
					retVal = this.GetItem(this.selectedItemIdx).retval;

				// Run the OnItemSelect event function
				if (typeof(this.OnItemSelect) === "function")
					this.OnItemSelect(retVal, true);

				// Exit the input loop if this.exitOnItemSelect is set to true
				if (this.exitOnItemSelect)
					continueOn = false;
			}
		}
		else if (this.lastUserInput == " ") // Add the current item to multi-select
		{
			// Add the current item to multi-select if multi-select is enabled
			if (this.multiSelect)
			{
				// Only let the user select the item if ValidateSelectItem() returns true
				var allowSelectItem = true;
				if (typeof(this.ValidateSelectItem) === "function")
					allowSelectItem = this.ValidateSelectItem(this.GetItem(this.selectedItemIdx).retval);
				if (allowSelectItem)
				{
					var added = false; // Will be true if added or false if deleted
					if (selectedItemIndexes.hasOwnProperty(+(this.selectedItemIdx)))
						delete selectedItemIndexes[+(this.selectedItemIdx)];
					else
					{
						var addIt = true;
						if (this.maxNumSelections > 0)
							addIt = (Object.keys(selectedItemIndexes).length < this.maxNumSelections);
						if (addIt)
						{
							selectedItemIndexes[+(this.selectedItemIdx)] = true;
							added = true;
						}
					}

					// Run the OnItemSelect event function
					if (typeof(this.OnItemSelect) === "function")
					{
						//this.OnItemSelect = function(pItemRetval, pSelected) { }
						this.OnItemSelect(this.GetItem(this.selectedItemIdx).retval, added);
					}

					// Draw a character next to the item if it's selected, or nothing if it's not selected
					var XPos = this.pos.x + this.size.width - 2;
					var YPos = this.pos.y+(this.selectedItemIdx-this.topItemIdx);
					if (this.borderEnabled)
					{
						--XPos;
						++YPos;
					}
					if (this.scrollbarEnabled && !this.CanShowAllItemsInWindow())
						--XPos;
					console.gotoxy(XPos, YPos);
					if (added)
					{
						// If the item color is an array, then default to a color string here
						var itemColor = this.GetColorForItem(this.selectedItemIdx, true);
						if (Array.isArray(itemColor))
						{
							var bkgColor = getBackgroundAttrAtIdx(itemColor, this.size.width-1);
							itemColor = "\1n\1h\1g" + bkgColor;
						}
						console.print(itemColor + " " + this.multiSelectItemChar + "\1n");
					}
					else
					{
						// Display the last 2 characters of the regular item text
						var itemText = this.GetItemText(this.selectedItemIdx, null, true, false);
						var textToPrint = substrWithAttrCodes(itemText, console.strlen(itemText)-2, 2);
						console.print(textToPrint + "\1n");
					}
				}
			}
		}
		// For numbered mode, if the user enters a number, allow the user to
		// choose an item by typing its number.
		else if (/[0-9]/.test(this.lastUserInput) && this.numberedMode)
		{
			var originalCurpos = console.getxy();

			// Put the user's input back in the input buffer to
			// be used for getting the rest of the message number.
			console.ungetstr(this.lastUserInput);
			// Move the cursor to the bottom of the screen and
			// prompt the user for the message number.
			var promptX = this.pos.x;
			var promptY = this.pos.y+this.size.height;
			console.gotoxy(promptX, promptY);
			printf("\1n%" + this.size.width + "s", ""); // Blank out what might be on the screen already
			console.gotoxy(promptX, promptY);
			console.print("\1cItem #: \1h");
			var userEnteredItemNum = console.getnum(numItems);
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
					if (selectedItemIndexes.hasOwnProperty(+(this.selectedItemIdx)))
						delete selectedItemIndexes[+(this.selectedItemIdx)];
					else
					{
						var addIt = true;
						if (this.maxNumSelections > 0)
							addIt = (Object.keys(selectedItemIndexes).length < this.maxNumSelections);
						if (addIt)
							selectedItemIndexes[+(this.selectedItemIdx)] = true;
					}
					// TODO: Put a check-mark next to the selected item
					// TODO: Screen refresh?
				}
				else
				{
					retVal = this.GetItem(this.selectedItemIdx).retval;
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
			for (var i = 0; i < numItems; ++i)
			{
				var theItem = this.GetItem(i);
				for (var h = 0; h < theItem.hotkeys.length; ++h)
				{
					var userPressedHotkey = false;
					if (this.hotkeyCaseSensitive)
						userPressedHotkey = (this.lastUserInput == theItem.hotkeys.charAt(h));
					else
						userPressedHotkey = (this.lastUserInput.toUpperCase() == theItem.hotkeys.charAt(h).toUpperCase());
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
							retVal = theItem.retval;
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
			userChoices.push(this.GetItem(prop).retval);
	}

	return (this.multiSelect ? userChoices : retVal);
}
// Performs the key-down behavior for showing the menu items
//
// Parameters:
//  pSelectedItemIndexes: An object containing indexes of selected items.  This is
//                        normally a temporary object created/used in GetVal().
//  pNumItems: The pre-calculated number of menu items.  If this not given, this
//             will be retrieved by calling NumItems().
function DDLightbarMenu_DoKeyDown(pSelectedItemIndexes, pNumItems)
{
	var selectedItemIndexes = (typeof(pSelectedItemIndexes) === "object" ? pSelectedItemIndexes : {});
	var numItems = (typeof(pNumItems) === "number" ? pNumItems : this.NumItems());

	if (this.selectedItemIdx < numItems-1)
	{
		// Draw the current item in regular colors
		this.WriteItemAtItsLocation(this.selectedItemIdx, false, selectedItemIndexes.hasOwnProperty(+(this.selectedItemIdx)));
		++this.selectedItemIdx;
		// Draw the new current item in selected colors
		// If the selected item is below the bottom of the menu, then we'll need to
		// scroll the items up.
		var numItemsPerPage = (this.borderEnabled ? this.size.height - 2 : this.size.height);
		if (this.selectedItemIdx > this.topItemIdx+numItemsPerPage-1)
		{
			++this.topItemIdx;
			this.Draw(selectedItemIndexes);
		}
		else
		{
			// The selected item is not below the bottom of the menu, so we can
			// just draw the selected item highlighted.
			this.WriteItemAtItsLocation(this.selectedItemIdx, true, selectedItemIndexes.hasOwnProperty(+(this.selectedItemIdx)));
		}
	}
	else
	{
		// selectedItemIdx is the last item index.  If wrap navigation is enabled,
		// then go to the first item.
		if (this.wrapNavigation)
		{
			// Draw the current item in regular colors
			this.WriteItemAtItsLocation(this.selectedItemIdx, false, selectedItemIndexes.hasOwnProperty(+(this.selectedItemIdx)));
			// Go to the first item and scroll to the top if necessary
			this.selectedItemIdx = 0;
			var oldTopItemIdx = this.topItemIdx;
			this.topItemIdx = 0;
			if (this.topItemIdx != oldTopItemIdx)
				this.Draw(selectedItemIndexes);
			else
			{
				// Draw the new current item in selected colors
				this.WriteItemAtItsLocation(this.selectedItemIdx, true, selectedItemIndexes.hasOwnProperty(+(this.selectedItemIdx)));
			}
		}
	}
}

// Sets the characters to use for drawing the border.  Takes an object specifying
// the values to set, but does not overwrite the whole borderChars object in the
// menu object.
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
		if (pBorderChars.hasOwnProperty(borderPropNames[i]))
			this.borderChars[borderPropNames[i]] = pBorderChars[borderPropNames[i]];
	}
}

// Sets the colors to use with the menu.  Takes an object specifying the values
// to set, but does not overwrite the whole colors object in the menu object.
//
// Parameters:
//  pColors: An object with the following properties:
//           itemColor: The color to use for non-highlighted items
//           selectedItemColor: The color to use for selected items
//           itemTextCharHighlightColor: The color to use for a highlighted
//                                       non-space character in an item text
//                                       (specified by having a & in the item
//                                       text).
//                                       It's important not to specify a "\1n"
//                                       in here in case the item text should
//                                       have a background color.
//           borderColor: The color to use for the border
//           scrollbarScrollBlockColor: The color to use for the scrollbar block
//           scrollbarBGColor: The color to use for the scrollbar background
function DDLightbarMenu_SetColors(pColors)
{
	if (typeof(pColors) != "object")
		return;

	var colorPropNames = ["itemColor", "selectedItemColor", "altItemColor", "altSelectedItemColor",
	                      "itemTextCharHighlightColor", "borderColor", "scrollbarScrollBlockColor",
	                      "scrollbarBGColor"];
	for (var i = 0; i < colorPropNames.length; ++i)
	{
		if (pColors.hasOwnProperty(colorPropNames[i]))
			this.colors[colorPropNames[i]] = pColors[colorPropNames[i]];
	}
}

// Returns the number of (possible) items per page
function DDLightbarMenu_GetNumItemsPerPage()
{
	var numItemsPerPage = this.size.height;
	if (this.borderEnabled)
		numItemsPerPage -= 2;
	return numItemsPerPage;
}

// Gets the top item index of the last page of items
function DDLightbarMenu_GetTopItemIdxOfLastPage()
{
	var numItemsPerPage = this.size.height;
	if (this.borderEnabled)
		numItemsPerPage -= 2;
	var topItemIndex = this.NumItems() - numItemsPerPage;
	if (topItemIndex < 0)
		topItemIndex = 0;
	return topItemIndex;
}

// Sets the top item index to the top item of the last page of items
function DDLightbarMenu_SetTopItemIdxToTopOfLastPage()
{
	var numItemsPerPage = this.size.height;
	if (this.borderEnabled)
		numItemsPerPage -= 2;
	this.topItemIdx = this.NumItems() - numItemsPerPage;
	if (this.topItemIdx < 0)
		this.topItemIdx = 0;
}

// Adds additional key characters to cause quitting out of the menu
// in addition to ESC.  The keys will be case-sensitive.
//
// Parameters:
//  pAdditionalQuitKeys: A string of key characters
function DDLightbarMenu_AddAdditionalQuitKeys(pAdditionalQuitKeys)
{
	this.additionalQuitKeys += pAdditionalQuitKeys;
}

// Returns whether or not the additional quit keys array contains a given
// key character.
//
// Parameters:
//  pKey: The key to look for in the additional quit keys
//
// Return value: Boolean - Whether or not the additional quit keys includes
//               pKey
function DDLightbarMenu_QuitKeysIncludes(pKey)
{
	return (this.additionalQuitKeys.indexOf(pKey) > -1);
}

// Clears the string of additional key characters to quit out of the menu
function DDLightbarMenu_ClearAdditionalQuitKeys()
{
	this.additionalQuitKeys = "";
}

// Adds additional key characters to select any item.  The keys will be case-sensitive.
//
// Parameters:
//  pAdditionalAddItemKeys: A string containing key characters
function DDLightbarMenu_AddAdditionalSelectItemKeys(pAdditionalAddItemKeys)
{
	this.additionalSelectItemKeys += pAdditionalAddItemKeys;
}

// Returns whether or not the additional select-item keys array contains a given
// key character.
//
// Parameters:
//  pKey: The key to look for in the additional select-itemkeys
//
// Return value: Boolean - Whether or not the additional select-item keys includes
//               pKey
function DDLightbarMenu_SelectItemKeysIncludes(pKey)
{
	return (this.additionalSelectItemKeys.indexOf(pKey) > -1);
}

// Clears the string of additional key characters to select any item
function DDLightbarMenu_ClearAdditionalSelectItemKeys()
{
	this.additionalSelectItemKeys = "";
}

// Displays an initial scrollbar
//
// Parameters:
//  pSolidBlockStartRow: The starting row for the solid/bright blocks
//  pNumSolidBlocks: The number of solid/bright blocks to write.  If this
//                   is omitted, this.scrollbarInfo.numSolidScrollBlocks
//                   will be used.
function DDLightbarMenu_DisplayInitialScrollbar(pSolidBlockStartRow, pNumSolidBlocks)
{
	var numSolidBlocks = (typeof(pNumSolidBlocks) == "number" ? pNumSolidBlocks : this.scrollbarInfo.numSolidScrollBlocks);

	var numSolidBlocksWritten = 0;
	var wroteBrightBlockColor = false;
	var wroteDimBlockColor = false;
	var startY = this.pos.y;
	var screenBottomRow = this.pos.y + this.size.height - 1;
	var scrollbarCol = this.pos.x + this.size.width - 1;
	if (this.borderEnabled)
	{
		++startY;
		--screenBottomRow;
		--scrollbarCol;
	}
	this.scrollbarInfo.solidBlockLastStartRow = startY;
	for (var screenY = startY; screenY <= screenBottomRow; ++screenY)
	{
		console.gotoxy(scrollbarCol, screenY);
		if ((screenY >= pSolidBlockStartRow) && (numSolidBlocksWritten < numSolidBlocks))
		{
			if (!wroteBrightBlockColor)
			{
				console.print("\1n" + this.colors.scrollbarScrollBlockColor);
				wroteBrightBlockColor = true;
				wroteDimBlockColor = false;
			}
			console.print(this.scrollbarInfo.blockChar);
			++numSolidBlocksWritten;
		}
		else
		{
			if (!wroteDimBlockColor)
			{
				console.print("\1n" + this.colors.scrollbarBGColor);
				wroteDimBlockColor = true;
			}
			console.print(this.scrollbarInfo.BGChar);
		}
	}
}

// For the DigDistMsgReader class: Updates the scrollbar for a message, for use
// in enhanced reader mode.  This does only the necessary character updates to
// minimize the number of characters that need to be updated on the screen.
//
// Parameters:
//  pNewStartRow: The new (current) start row for solid/bright blocks
//  pOldStartRow: The old start row for solid/bright blocks
//  pNumSolidBlocks: The number of solid/bright blocks.  If this is omitted,
//                   this.scrollbarInfo.numSolidScrollBlocks will be used.
function DDLightbarMenu_UpdateScrollbar(pNewStartRow, pOldStartRow, pNumSolidBlocks)
{
	var numSolidBlocks = (typeof(pNumSolidBlocks) == "number" ? pNumSolidBlocks : this.scrollbarInfo.numSolidScrollBlocks);

	var startY = this.pos.y;
	var screenBottomRow = this.pos.y + this.size.height - 1;
	var scrollbarCol = this.pos.x + this.size.width - 1;
	if (this.borderEnabled)
	{
		++startY;
		--screenBottomRow;
		--scrollbarCol;
	}

	// Calculate the difference in the start row.  If the difference is positive,
	// then the solid block section has moved down; if the diff is negative, the
	// solid block section has moved up.
	var solidBlockStartRowDiff = pNewStartRow - pOldStartRow;
	var oldLastRow = pOldStartRow + numSolidBlocks - 1;
	var newLastRow = pNewStartRow + numSolidBlocks - 1;
	if (solidBlockStartRowDiff > 0)
	{
		// The solid block section has moved down
		if (pNewStartRow > oldLastRow)
		{
			// No overlap
			// Write dim blocks over the old solid block section
			console.print("\1n" + this.colors.scrollbarBGColor);
			for (var screenY = pOldStartRow; screenY <= oldLastRow; ++screenY)
			{
				console.gotoxy(scrollbarCol, screenY);
				console.print(this.scrollbarInfo.BGChar);
			}
			// Write solid blocks in the new locations
			console.print("\1n" + this.colors.scrollbarScrollBlockColor);
			for (var screenY = pNewStartRow; screenY <= newLastRow; ++screenY)
			{
				console.gotoxy(scrollbarCol, screenY);
				console.print(this.scrollbarInfo.blockChar);
			}
		}
		else
		{
			// There is some overlap
			// Write dim blocks on top
			console.print("\1n" + this.colors.scrollbarBGColor);
			for (var screenY = pOldStartRow; screenY < pNewStartRow; ++screenY)
			{
				console.gotoxy(scrollbarCol, screenY);
				console.print(this.scrollbarInfo.BGChar);
			}
			// Write bright blocks on the bottom
			console.print("\1n" + this.colors.scrollbarScrollBlockColor);
			for (var screenY = oldLastRow+1; screenY <= newLastRow; ++screenY)
			{
				console.gotoxy(scrollbarCol, screenY);
				console.print(this.scrollbarInfo.blockChar);
			}
		}
	}
	else if (solidBlockStartRowDiff < 0)
	{
		// The solid block section has moved up
		if (pOldStartRow > newLastRow)
		{
			// No overlap
			// Write dim blocks over the old solid block section
			console.print("\1n" + this.colors.scrollbarBGColor);
			for (var screenY = pOldStartRow; screenY <= oldLastRow; ++screenY)
			{
				console.gotoxy(scrollbarCol, screenY);
				console.print(this.scrollbarInfo.BGChar);
			}
			// Write solid blocks in the new locations
			console.print("\1n" + this.colors.scrollbarScrollBlockColor);
			for (var screenY = pNewStartRow; screenY <= newLastRow; ++screenY)
			{
				console.gotoxy(scrollbarCol, screenY);
				console.print(this.scrollbarInfo.blockChar);
			}
		}
		else
		{
			// There is some overlap
			// Write bright blocks on top
			console.print("\1n" + this.colors.scrollbarScrollBlockColor);
			var endRow = pOldStartRow;
			for (var screenY = pNewStartRow; screenY < endRow; ++screenY)
			{
				console.gotoxy(scrollbarCol, screenY);
				console.print(this.scrollbarInfo.blockChar);
			}
			// Write dim blocks on the bottom
			console.print("\1n" + this.colors.scrollbarBGColor);
			endRow = pOldStartRow + numSolidBlocks;
			for (var screenY = pNewStartRow+numSolidBlocks; screenY < endRow; ++screenY)
			{
				console.gotoxy(scrollbarCol, screenY);
				console.print(this.scrollbarInfo.BGChar);
			}
		}
	}
}

// Calculates the starting row for the solid blocks on the scrollbar
//
// Return value: The starting row for the solid blocks on the scrollbar
function DDLightbarMenu_CalcScrollbarSolidBlockStartRow()
{
	var scrollbarStartY = this.pos.y;
	var scrollbarHeight = this.size.height;
	if (this.borderEnabled)
	{
		++scrollbarStartY;
		scrollbarHeight -= 2;
	}
	var scrollbarBottomY = scrollbarStartY + scrollbarHeight - 1;
	var solidBlockStartRow = scrollbarStartY;
	var numMenuItems = this.NumItems();
	if (numMenuItems > 0)
	{
		var scrollbarFraction = this.selectedItemIdx / numMenuItems;
		var scrollbarStartRow = scrollbarStartY + Math.floor(scrollbarHeight * scrollbarFraction);
		solidBlockStartRow = scrollbarStartRow - Math.floor(this.scrollbarInfo.numSolidScrollBlocks / 2);
		// Don't let the solid blocks go above the starting screen row or below the ending
		// screen row of the scrollbar
		if (solidBlockStartRow < scrollbarStartY)
			solidBlockStartRow = scrollbarStartY;
		else if (solidBlockStartRow + this.scrollbarInfo.numSolidScrollBlocks > scrollbarBottomY)
			solidBlockStartRow = scrollbarBottomY - this.scrollbarInfo.numSolidScrollBlocks + 1;
	}
	return solidBlockStartRow;
}

// Updates the scrollbar position based on the currently-selected
// item index, this.selectedItemIdx.
//
// Parameters:
//  pForceUpdate: Boolean - Whether or not to force the redraw regardless of block location.
//                Defaults to false.
function DDLightbarMenu_UpdateScrollbarWithHighlightedItem(pForceUpdate)
{
	var forceUpdate = (typeof(pForceUpdate) === "boolean" ? pForceUpdate : false);
	var solidBlockStartRow = this.CalcScrollbarSolidBlockStartRow();
	if (forceUpdate || (solidBlockStartRow != this.scrollbarInfo.solidBlockLastStartRow))
		this.UpdateScrollbar(solidBlockStartRow, this.scrollbarInfo.solidBlockLastStartRow, this.scrollbarInfo.numSolidScrollBlocks);
	this.scrollbarInfo.solidBlockLastStartRow = solidBlockStartRow;
}

function DDLightbarMenu_CanShowAllItemsInWindow()
{
	var pageHeight = (this.borderEnabled ? this.size.height - 2 : this.size.height);
	return (this.NumItems() <= pageHeight);
}

// Makes an item object that is compatible with DDLightbarMenu, with a given
// item text and return value.
//
// Parameters:
//  pText: The text to show in the menu for the item
//  pRetval: The return value of the item when the user selects it from the menu
//
// Return value: An object with the given text & return value compatible with DDLightbarMenu
function DDLightbarMenu_MakeItemWithTextAndRetval(pText, pRetval)
{
	var item = getDefaultMenuItem();
	item.text = pText;
	item.retval = pRetval;
	return item;
}

// Makes an item object that is compatible with DDLightbarMenu, with a given
// return value.
//
// Parameters:
//  pRetval: The return value of the item when the user selects it from the menu
//
// Return value: An object with the given return value compatible with DDLightbarMenu
function DDLightbarMenu_MakeItemWithRetval(pRetval)
{
	var item = getDefaultMenuItem();
	item.retval = pRetval;
	return item;
}

// Returns whether an item is set to use the alternate item colors
//
// Parameters:
//  pItemIndex: The index of the item
//
// Return value: Boolean - Whether or not an item is configured to use alternate item colors
function DDLightbarMenu_ItemUsesAltColors(pItemIndex)
{
	if ((pItemIndex < 0) || (pItemIndex >= this.NumItems()))
		return false;

	return this.GetItem(pItemIndex).useAltColors;
}

// Returns either the normal or alternate color for an item
//
// Parameters:
//  pItemIndex: The index of the item
//  pSelected: Whether or not to use selected item colors.  Defaults to false.
//
// Return value: Either colors.itemColor or colors.altItemColor
function DDLightbarMenu_GetColorForItem(pItemIndex, pSelected)
{
	if ((pItemIndex < 0) || (pItemIndex >= this.NumItems()))
		return "";

	var selected = (typeof(pSelected) == "boolean" ? pSelected : false);
	if (selected)
		return (this.GetItem(pItemIndex).useAltColors ? this.colors.altSelectedItemColor : this.colors.selectedItemColor);
	else
		return (this.GetItem(pItemIndex).useAltColors ? this.colors.altItemColor : this.colors.itemColor);
}

// Returns either the selected or alternate selected color for an item
//
// Parameters:
//  pItemIndex: The index of the item
//
// Return value: Either colors.selectedItemColor or colors.altSelectedItemColor
function DDLightbarMenu_GetSelectedColorForItem(pItemIndex)
{
	if (typeof(pItemIndex) !== "number")
		return;
	if ((pItemIndex < 0) || (pItemIndex >= this.NumItems()))
		return "";

	return (this.GetItem(pItemIndex).useAltColors ? this.colors.altSelectedItemColor : this.colors.selectedItemColor);
}

// Sets the selected item index for the menu, and sets anything else as appropriate
// (such as the index of the topmost menu item).
//
// Parameters:
//  pSelectedItemIdx: The index of the selected item
function DDLightbarMenu_SetSelectedItemIdx(pSelectedItemIdx)
{
	if (typeof(pSelectedItemIdx) !== "number")
		return;
	if ((pSelectedItemIdx < 0) || (pSelectedItemIdx >= this.NumItems()))
		return;

	this.selectedItemIdx = pSelectedItemIdx;
	if (this.selectedItemIdx == 0)
		this.topItemIdx = 0;
	else if (this.selectedItemIdx >= this.topItemIdx+this.GetNumItemsPerPage())
		this.topItemIdx = this.selectedItemIdx - this.GetNumItemsPerPage() + 1;
	else if (this.selectedItemIdx < this.topItemIdx)
		this.topItemIdx = this.selectedItemIdx;
}

// Calculates the number of solid scrollbar blocks & non-solid scrollbar blocks
// to use.  Saves the information in this.scrollbarInfo.numSolidScrollBlocks and
// this.scrollbarInfo.numNonSolidScrollBlocks.
function DDLightbarMenu_CalcScrollbarBlocks()
{
	var menuDisplayHeight = this.size.height;
	if (this.borderEnabled)
		menuDisplayHeight -= 2;
	var numMenuItems = this.NumItems();
	if (numMenuItems > 0)
	{
		var menuListFractionShown = menuDisplayHeight / numMenuItems;
		if (menuListFractionShown > 1)
			menuListFractionShown = 1.0;
		this.scrollbarInfo.numSolidScrollBlocks = Math.floor(menuDisplayHeight * menuListFractionShown);
		if (this.scrollbarInfo.numSolidScrollBlocks <= 0)
			this.scrollbarInfo.numSolidScrollBlocks = 1;
		else if (this.scrollbarInfo.numSolidScrollBlocks > menuDisplayHeight)
			this.scrollbarInfo.numSolidScrollBlocks = menuDisplayHeight;
		this.scrollbarInfo.numNonSolidScrollBlocks = menuDisplayHeight - this.scrollbarInfo.numSolidScrollBlocks;
	}
	else
	{
		this.scrollbarInfo.numSolidScrollBlocks = menuDisplayHeight;
		this.scrollbarInfo.numNonSolidScrollBlocks = 0;
	}
}

//////////////////////////////////////////////////////////
// Helper functions, not part of the DDLightbarMenu class

// Returns the length of an item's text, not counting non-displayable
// characters (such as Synchronet color attributes and an ampersand
// immediately before a non-space)
//
// Parameters:
//  pText: The text to test
//  pAmpersandHotkeysInItems: Boolean - Whether or not ampersand hotkeys are enabled for the item text
function itemTextDisplayableLen(pText, pAmpersandHotkeysInItems)
{
	var textLen = console.strlen(pText);
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

// Returns whether or not all string attribute objects in an array have the
// expected properties, and that the property types are correct, for menu item
// string color definitions.
//
// Parameters:
//  pAttrsArray: An array of objects which are expected to containg the
//               following properties: start, end, attrs
//
// Return value: Boolean - Whether or not all elements in the array
//               have all the expected properties
function attrsArrayElementsHaveAllCorrectProps(pAttrsArray)
{
	var allElementsHaveCorrectProps = true;
	for (var i = 0; (i < pAttrsArray.length) && allElementsHaveCorrectProps; ++i)
	{
		allElementsHaveCorrectProps = ((typeof(pAttrsArray[i]) == "object") &&
		                               pAttrsArray[i].hasOwnProperty("start") &&
		                               pAttrsArray[i].hasOwnProperty("end") &&
		                               pAttrsArray[i].hasOwnProperty("attrs") &&
		                               (typeof(pAttrsArray[i].start) == "number") &&
		                               (typeof(pAttrsArray[i].end) == "number") &&
		                               (typeof(pAttrsArray[i].attrs) == "string"));
	}
	return allElementsHaveCorrectProps;
}

// Adds color/attribute codes to a string.
//
// Parameters:
//  pStr: The string to add attribute codes to
//  pAttrs: This can be either a string containing attribute codes or an array
//          of objects with start, end, and color properties, for applying attribute
//          codes to different parts of the string.  These are the properties of
//          each object in the string (note: for the last one, end can be 0 or -1
//          to apply the attributes to the rest of the string):
//           start: The start index in the string to apply the attributes to
//           end: One past the last index in the part of the string to apply the attributes to
//           attrs: The attributes to apply to that part of the string
//
// Return value: A copy of the string with attributes applied
function addAttrsToString(pStr, pAttrs)
{
	if (typeof(pStr) != "string")
		return "";
	else if (pStr.length == 0)
		return "";

	var str;
	if (Array.isArray(pAttrs))
	{
		// To use the attributes array, the array must have some objects and
		// each element of the array must have start, end, and attrs properties
		if ((pAttrs.length > 0) && attrsArrayElementsHaveAllCorrectProps(pAttrs))
		{
			// Colorize the string with the object in pAttrs.
			// Don't do the last object in this loop, because for the last object,
			// we'll want to check if its end index is valid.
			str = "";
			var lastEnd = -1;
			for (var i = 0; i < pAttrs.length; ++i)
			{
				// If the current object's start is more than 1 character after
				// the last's end, then append the gap in the string with the
				// normal attribute
				if ((i > 0) && (pAttrs[i].start > pAttrs[i-1].end))
					str += "\1n" + pStr.substring(pAttrs[i-1].end, pAttrs[i].start);
				// If the properties for the current attrib object are all valid, append
				// the current part of the string with the given attributes
				if ((pAttrs[i].start >= lastEnd) && (pAttrs[i].start >= 0) && (pAttrs[i].start < pStr.length) && (pAttrs[i].end > pAttrs[i].start) && (pAttrs[i].end <= pStr.length))
					str += "\1n" + pAttrs[i].attrs + pStr.substring(pAttrs[i].start, pAttrs[i].end);
				// For the last attribute object, allow the end index to be <= 0 or
				// more than the length of the string to apply the attributes to the
				// rest of the string.
				//else if ((i == pAttrs.length-1) && (pAttrs[i].start >= lastEnd) && (pAttrs[i].start >= 0) && (pAttrs[i].start < pStr.length) && (pAttrs[i].end <= 0))
				else if ((i == pAttrs.length-1) && (pAttrs[i].start >= lastEnd) && (pAttrs[i].start >= 0) && (pAttrs[i].start < pStr.length) && ((pAttrs[i].end <= 0) || (pAttrs[i].end > pStr.length)))
					str += "\1n" + pAttrs[i].attrs + pStr.substring(pAttrs[i].start);
				lastEnd = pAttrs[i].end;
			}

			// If str is shorter than the passed-in string, then append the rest of the string
			// with the normal attribute.
			var theStrLen = console.strlen(str);
			if (theStrLen < pStr.length)
				str += "\1n" + pStr.substring(theStrLen);
		}
		else
			str = pStr;
	}
	else if (typeof(pAttrs) == "string")
		str = "\1n" + pAttrs + pStr;
	else
		str = pStr;
	return str;
}

function getBackgroundAttrAtIdx(pAttrs, pIdx)
{
	if (typeof(pIdx) != "number")
		return "";
	if (pIdx < 0)
		return "";

	// Synchronet background color codes:
	// Black: 0
	// Red: 1
	// Green: 2
	// Yellow/brown: 3
	// Blue: 4
	// Magenta: 5
	// Cyan: 6
	// White/grey: 7
	var syncBkgAttrRegex = /\1[01234567]/;
	var bkgAttr = "";
	if (Array.isArray(pAttrs))
	{
		if ((pAttrs.length > 0) && attrsArrayElementsHaveAllCorrectProps(pAttrs))
		{
			// Go through the array, and if a start & end is found where pIdx
			// falls between, check that objects attrs property for its last
			// background attribute, if there is one
			for (var i = 0; i < pAttrs.length; ++i)
			{
				if ((pIdx >= pAttrs[i].start) && ((pIdx < pAttrs[i].end) || (pAttrs[i].end == 0)))
				{
					// Check the attrs for the last background attribute, starting
					// from the end
					if (pAttrs[i].attrs.length >= 2)
					{
						for (var attrIdx = pAttrs[i].attrs.length - 2; attrIdx >= 0; attrIdx -= 2)
						{
							var currentTwo = pAttrs[i].attrs.substr(attrIdx, 2);
							if (syncBkgAttrRegex.test(currentTwo))
							{
								bkgAttr = currentTwo;
								break;
							}
						}
					}
					break;
				}
			}
		}
	}
	else if (typeof(pAttrs) == "string")
	{
		if ((pIdx >= 0) || (pIdx < pAttrs.length))
		{
			// Starting from pIdx, go backwards through pAttrs, and if a Synchronet
			// background attribute code is found, then use it.
			for (var i = pIdx - 2; i >= 0; i -= 2)
			{
				var currentTwo = pAttrs.substr(i, 2);
				if (syncBkgAttrRegex.test(currentTwo))
				{
					bkgAttr = currentTwo;
					break;
				}
			}
		}
	}
	return bkgAttr;
}

// Returns a default item object for a DDLightbarMenu
function getDefaultMenuItem() {
	return {
		text: "",
		retval: null,
		hotkeys: "",
		useAltColors: false,
		itemColor: null,
		itemSelectedColor: null
	};
}

// Returns a substring of a string, accounting for Synchronet attribute
// codes (not including the attribute codes in the start index or length)
//
// Parameters:
//  pStr: The string to perform the substring on
//  pLen: The length of the substring
//
// Return value: A substring of the string according to the parameters
function substrWithAttrCodes(pStr, pStartIdx, pLen)
{
	if (typeof(pStr) != "string")
		return "";
	if (typeof(pStartIdx) != "number")
		return "";
	if (typeof(pLen) != "number")
		return "";
	if ((pStartIdx <= 0) && (pLen >= console.strlen(pStr)))
		return pStr;

	// Find the real start index.  If there are Synchronet attribute 
	var startIdx = printedToRealIdxInStr(pStr, pStartIdx);
	if (startIdx < 0)
		return "";
	// Find the actual length of the string to get
	var len = pLen;
	var printableCharCount = 0;
	var syncAttrCount = 0;
	var syncAttrRegexWholeWord = /^\1[krgybmcw01234567hinpq,;\.dtl<>\[\]asz]$/i;
	var i = startIdx;
	while ((printableCharCount < pLen) && (i < pStr.length))
	{
		if (syncAttrRegexWholeWord.test(pStr.substr(i, 2)))
		{
			++syncAttrCount;
			i += 2;
		}
		else
		{
			++printableCharCount;
			++i;
		}
	}
	len += (syncAttrCount * 2);
	var shortenedStr = pStr.substr(startIdx, len);
	// Include any attribute codes that might appear before the start index
	// in the string
	var attrIdx = pStr.lastIndexOf("\1", startIdx);
	if (attrIdx >= 0)
	{
		var attrStartIdx = -1;
		// Generate a string of all Synchronet attributes at the found location
		for (var i = attrIdx; i >= 0; i -= 2)
		{
			if (syncAttrRegexWholeWord.test(pStr.substr(i, 2)))
				attrStartIdx = i;
			else
				break;
		}
		if (attrStartIdx > -1)
		{
			var attrStr = pStr.substring(attrStartIdx, attrIdx+2);
			shortenedStr = attrStr + shortenedStr;
		}
	}
	return shortenedStr;
}

// Converts a 'printed' index in a string to its real index in the string
//
// Parameters:
//  pStr: The string to search in
//  pIdx: The printed index in the string
//
// Return value: The actual index in the string object, or -1 on error
function printedToRealIdxInStr(pStr, pIdx)
{
	if (typeof(pStr) != "string")
		return -1;
	if ((pIdx < 0) || (pIdx >= pStr.length))
		return -1;

	// Store the character at the given index if the string didn't have attribute codes.
	// Also, to help ensure this returns the correct index, get a substring with several
	// characters starting at the given index to match a word within the string
	var strWithoutAttrCodes = strip_ctrl(pStr);
	var substr_len = 5;
	var substrWithoutAttrCodes = strWithoutAttrCodes.substr(pIdx, substr_len);
	var printableCharAtIdx = strWithoutAttrCodes.charAt(pIdx);
	// Iterate through pStr until we find that character and return that index.
	var realIdx = 0;
	for (var i = 0; i < pStr.length; ++i)
	{
		// tempStr is the string to compare with substrWithoutAttrCodes
		var tempStr = strip_ctrl(pStr.substr(i)).substr(0, substr_len);
		if ((pStr.charAt(i) == printableCharAtIdx) && (tempStr == substrWithoutAttrCodes))
		{
			realIdx = i;
			break;
		}
	}
	return realIdx;
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
//  pInputTimeoutMS: The input timeout in milliseconds (defaults to 300000).
//                   If the user is a sysop, this will use a timeout of 0 for no timeout.
//
// Return value: The user's keypress
function getKeyWithESCChars(pGetKeyMode, pInputTimeoutMS)
{
	var getKeyMode = (typeof(pGetKeyMode) === "number" ? pGetKeyMode : K_NONE);
	var inputTimeoutMS = (typeof(pInputTimeoutMS) === "number" ? pInputTimeoutMS : 300000);
	if (inputTimeoutMS == 0)
		inputTimeoutMS = 300000;
	// Input a key from the user and take action based on the user's input.  If
	// the user is a sysop, don't use an input timeout.
	var userInput = "";
	if (user.compare_ars("SYSOP"))
		userInput = console.getkey(getKeyMode);
	else
		userInput = console.inkey(getKeyMode, inputTimeoutMS);
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
