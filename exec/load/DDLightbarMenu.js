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
This menu library does not display a borders around the menu, so if you
desire a border around the menu, your script can draw the border in any
style you like.

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

Changing the normal item color to green & selected item color to bright green:
lbMenu.colors.itemColor = "\1n\1g";
lbMenu.colors.selectedItemColor = "\1n\1h\1g";
*/

load("sbbsdefs.js");

var KEY_ESC = ascii(27);
var KEY_ENTER = "\x0d";
// PageUp & PageDown keys - Not real key codes, but codes defined
// to be used & recognized in this script
var KEY_PAGE_UP = "\1PgUp";
var KEY_PAGE_DOWN = "\1PgDn";

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
	this.colors = {
		itemColor: "\1n\1w\1" + "4",
		selectedItemColor: "\1n\1b\1" + "7"
	};
	this.selectedItemIdx = 0;
	this.topItemIdx = 0;

	// Member functions
	this.Add = DDLightbarMenu_Add;
	this.SetPos = DDLightbarMenu_SetPos;
	this.SetSize = DDLightbarMenu_SetSize;
	this.SetWidth = DDLightbarMenu_SetWidth;
	this.SetHeight = DDLightbarMenu_SetHeight;
	this.Draw = DDLightbarMenu_Draw;
	this.WriteItem = DDLightbarMenu_WriteItem;
	this.Erase = DDLightbarMenu_Erase;
	this.SetItemHotkey = DDLightbarMenu_SetItemHotkey;
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
	var curPos = { x: this.pos.x, y: this.pos.y };
	// Write the menu items, only up to the height of the menu
	var numItemsWritten = 0;
	//writeWithPause(1, 1, "Top idx: " + this.topItemIdx + ", # items: " + this.items.length + ", height: " + this.size.height + "  ", 1500, "\1n", false); // Temporary
	for (var idx = this.topItemIdx; (idx < this.items.length) && (numItemsWritten < this.size.height); ++idx)
	{
		console.gotoxy(curPos.x, curPos.y++);
		var itemColor = (idx == this.selectedItemIdx ? this.colors.selectedItemColor : this.colors.itemColor);
		printf("\1n" + itemColor + "%-" + itemLen + "s", this.items[idx].text.substr(0, itemLen));
		++numItemsWritten;
	}
	// If there are fewer items than the height of the menu, then write blank lines to fill
	// the rest of the height of the menu.
	if (numItemsWritten < this.size.height)
	{
		for (; numItemsWritten <= this.size.height; ++numItemsWritten)
		{
			console.gotoxy(curPos.x, curPos.y++);
			printf("\1n" + this.colors.itemColor + "%-" + itemLen + "s", "");
		}
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
			itemLen = (this.showScrollbar ? this.size.width - 1 : this.size.width);
		var itemColor = "";
		if (typeof(pHighlight) == "boolean")
			itemColor = (pHighlight ? this.colors.selectedItemColor : this.colors.itemColor);
		else
			itemColor = (pIdx == this.selectedItemIdx ? this.colors.selectedItemColor : this.colors.itemColor);
		printf("\1n" + itemColor + "%-" + itemLen + "s", this.items[pIdx].text.substr(0, itemLen));
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
		if (userInput == KEY_UP)
		{
			if (this.selectedItemIdx > 0)
			{
				// Draw the current item in regular colors
				console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
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
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, true);
				}
			}
		}
		else if (userInput == KEY_DOWN)
		{
			if (this.selectedItemIdx < this.items.length-1)
			{
				// Draw the current item in regular colors
				console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
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
					console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
					this.WriteItem(this.selectedItemIdx, null, true);
				}
			}
		}
		else if (userInput == KEY_PAGE_UP)
		{
			this.selectedItemIdx -= this.size.height;
			this.topItemIdx -= this.size.height;
			if (this.selectedItemIdx < 0)
				this.selectedItemIdx = 0;
			if (this.topItemIdx < 0)
				this.topItemIdx = 0;
			this.Draw();
		}
		else if (userInput == KEY_PAGE_DOWN)
		{
			// Figure out how many pages are needed to list all the items
			//var numPages = Math.ceil(this.items.length / this.size.height);
			// Figure out the top index for the last page.
			//var topIndexForLastPage = (this.size.height * numPages) - this.size.height;
			var topIndexForLastPage = this.items.length - this.size.height;
			// Update the selected & top item indexes
			this.selectedItemIdx += this.size.height;
			this.topItemIdx += this.size.height;
			if (this.selectedItemIdx >= topIndexForLastPage)
				this.selectedItemIdx = topIndexForLastPage;
			if (this.topItemIdx > topIndexForLastPage)
				this.topItemIdx = topIndexForLastPage;
			this.Draw();
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
			// Go to the last item on the current page
			if (this.selectedItemIdx < this.items.length-1)
			{
				// Draw the current item in regular colors
				console.gotoxy(this.pos.x, this.pos.y+this.selectedItemIdx-this.topItemIdx);
				this.WriteItem(this.selectedItemIdx, null, false);
				this.selectedItemIdx = this.topItemIdx + this.size.height - 1;
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
					if (userInput == this.items[i].hotkey)
					{
						retVal = this.items[i].retval;
						continueOn = false;
					}
				}
			}
		}
	}

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
