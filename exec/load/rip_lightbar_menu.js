// RIPLightbarMenu: A general-purpose lightbar menu class for RIP/RIPScrip terminals.
// Displays a scrollable list of items with optional 3D border and scrollbar,
// handles keyboard and mouse input, and returns the user's selection.

"use strict";

require("sbbsdefs.js", "K_NOCRLF");
require("key_defs.js", "KEY_UP");
require("rip_lib.js", "RIPWindow");
require("rip_scrollbar.js", "RIPScrollbar");

// Mouse click identifiers for menu items (high-byte chars safe for CP437 terminals)
var RIP_LBMENU_MOUSE_ITEM_BASE = 0x80; // 0x80 + visible position for item clicks

// Border bevel dimensions (pixels)
var RIP_LBMENU_BEVEL_OUTER = 2;
var RIP_LBMENU_BEVEL_GAP   = 2;
var RIP_LBMENU_BEVEL_INNER = 2;
var RIP_LBMENU_BEVEL_TOTAL = RIP_LBMENU_BEVEL_OUTER + RIP_LBMENU_BEVEL_GAP + RIP_LBMENU_BEVEL_INNER;
var RIP_LBMENU_INNER_PAD   = 1;

// Scrollbar dimensions (pixels)
var RIP_LBMENU_SCROLLBAR_WIDTH = 14;
var RIP_LBMENU_SCROLLBAR_GAP   = 2;


// RIPLightbarMenu constructor.
//
// Parameters:
//  pX: The X coordinate of the upper-left corner of the lightbar menu (numeric, pixels)
//  pY: The Y coordinate of the upper-left corner of the lightbar menu (numeric, pixels)
//  pWidth: The width to use for the lightbar menu (numeric, pixels).
//          If the RIP border is used, the width includes the border.
//  pHeight: The height to use for the lightbar menu (numeric, pixels).
//           If the RIP border is used, the height includes the border.
//  pUseRIPBorder: Boolean - Whether or not to display a stylized RIP border around the menu.
function RIPLightbarMenu(pX, pY, pWidth, pHeight, pUseRIPBorder)
{
	this.pos = {
		x: (typeof pX === "number" ? pX : 0),
		y: (typeof pY === "number" ? pY : 0)
	};

	this.size = {
		width: (typeof pWidth === "number" ? pWidth : 640),
		height: (typeof pHeight === "number" ? pHeight : 286)
	};

	this.colors = {
		itemBg:        RIP_COLOR_LT_GRAY,
		itemFg:        RIP_COLOR_BLACK,
		selectedBg:    RIP_COLOR_CYAN,
		selectedFg:    RIP_COLOR_WHITE,
		borderSurface: RIP_COLOR_BLUE,
		bevelBright:   RIP_COLOR_WHITE,
		bevelDark:     RIP_COLOR_DK_GRAY,
		scrollTrack:   RIP_COLOR_DK_GRAY,
		scrollThumb:   RIP_COLOR_LT_GRAY,
		scrollArrowBg: RIP_COLOR_LT_GRAY,
		scrollArrowFg: RIP_COLOR_BLACK,
		contentBg:     RIP_COLOR_BLACK
	};

	this.useRIPBorder = (typeof(pUseRIPBorder) === "boolean" ? pUseRIPBorder : true);

	this.menuItems = [];
	this.selectedIndex = 0;
	this.topIndex = 0;
	this.itemHeight = 16;   // Height of each menu item in pixels
	this.textYOffset = 4;   // Y offset for text within an item row
	this.textXOffset = 4;   // X offset for text within the menu content area

	// Internal computed layout (set by _computeLayout)
	this._layout = null;
	// Internal RIPScrollbar instance (created by _computeLayout when needed)
	this._scrollbar = null;

	// Public methods
	this.Add = RIPLightbarMenu_Add;
	this.GetVal = RIPLightbarMenu_GetVal;

	// Internal methods
	this._sendRIP = RIPLightbarMenu_sendRIP;
	this._computeLayout = RIPLightbarMenu_computeLayout;
	this._buildItemRIP = RIPLightbarMenu_buildItemRIP;
	this._buildMenuBorderRIP = RIPLightbarMenu_buildMenuBorderRIP;
	this._buildMouseRegionsRIP = RIPLightbarMenu_buildMouseRegionsRIP;
	this._drawFullMenu = RIPLightbarMenu_drawFullMenu;
}


// For the RIPLightbarMenu class: Adds an item to the menu.
//
// Parameters:
//  pItemText: The text to show for the menu item
//  pItemRetVal: The return value to be returned when the user chooses the item.
//               This could be anything.
//  pHotkey: Optional single character hotkey that selects this item immediately
//           when pressed.
function RIPLightbarMenu_Add(pItemText, pItemRetVal, pHotkey)
{
	this.menuItems.push({
		text: pItemText,
		retval: pItemRetVal,
		hotkey: (typeof pHotkey === "string" && pHotkey.length > 0 ? pHotkey.toUpperCase() : null)
	});
}


// For the RIPLightbarMenu class: Shows the lightbar menu, starts the user input
// loop to let the user choose an item from the menu, and returns the user's
// chosen item.  If the user didn't choose the item (such as if they aborted),
// this function will return null.
//
// Parameters:
//  pDefaultRetVal: Optional - The return value of the item to pre-select.
//                  If not provided, the first item is selected.
function RIPLightbarMenu_GetVal(pDefaultRetVal)
{
	if (this.menuItems.length === 0)
		return null;

	this._computeLayout();
	var L = this._layout;
	var sb = this._scrollbar; // May be null if no scrollbar needed

	// Set initial selection to the item matching pDefaultRetVal
	if (typeof pDefaultRetVal !== "undefined" && pDefaultRetVal !== null)
	{
		for (var i = 0; i < this.menuItems.length; ++i)
		{
			if (this.menuItems[i].retval == pDefaultRetVal)
			{
				this.selectedIndex = i;
				break;
			}
		}
	}

	// Clamp selectedIndex to valid range
	if (this.selectedIndex < 0)
		this.selectedIndex = 0;
	else if (this.selectedIndex >= this.menuItems.length)
		this.selectedIndex = this.menuItems.length - 1;

	// Ensure selected item is within the visible scroll window
	if (this.selectedIndex >= this.topIndex + L.visibleCount)
		this.topIndex = this.selectedIndex - L.visibleCount + 1;
	if (this.selectedIndex < this.topIndex)
		this.topIndex = this.selectedIndex;

	// Draw the border/background and initial menu contents
	this._sendRIP(this._buildMenuBorderRIP());
	this._drawFullMenu();

	// Main input loop
	while (bbs.online && !js.terminated)
	{
		var key = console.getkey(K_NOCRLF|K_NOECHO|K_NOSPIN);
		if (key.toUpperCase() == console.quit_key)
			return null;

		var oldSelected = this.selectedIndex;
		var oldTop = this.topIndex;
		var ch = key.charCodeAt(0);
		var retval = undefined; // undefined = no selection yet

		// --- Mouse event handling ---

		// Menu item click
		if (ch >= RIP_LBMENU_MOUSE_ITEM_BASE && ch < RIP_LBMENU_MOUSE_ITEM_BASE + L.visibleCount)
		{
			var clickedIndex = this.topIndex + (ch - RIP_LBMENU_MOUSE_ITEM_BASE);
			if (clickedIndex >= 0 && clickedIndex < this.menuItems.length)
				retval = this.menuItems[clickedIndex].retval;
		}
		// Scrollbar mouse events (only if scrollbar exists)
		else if (sb !== null && ch === sb.mouseChars.scrollUp)
		{
			if (this.selectedIndex > 0)
				--this.selectedIndex;
		}
		else if (sb !== null && ch === sb.mouseChars.scrollDn)
		{
			if (this.selectedIndex < this.menuItems.length - 1)
				++this.selectedIndex;
		}
		else if (sb !== null && ch === sb.mouseChars.trackPgUp)
		{
			this.selectedIndex = Math.max(0, this.selectedIndex - L.visibleCount);
		}
		else if (sb !== null && ch === sb.mouseChars.trackPgDn)
		{
			this.selectedIndex = Math.min(this.menuItems.length - 1, this.selectedIndex + L.visibleCount);
		}
		else if (sb !== null && ch === sb.mouseChars.thumbDrag)
		{
			// Read the Y coordinate digits that follow (from the $Y$ text
			// variable expanded by SyncTerm at mouse button release time)
			var yStr = "";
			for (var d = 0; d < 4; ++d)
			{
				var dc = console.inkey(0, 500);
				if (dc === "")
					break;
				yStr += dc;
			}
			var mouseY = parseInt(yStr, 10);
			var sbL = sb._layout;
			if (!isNaN(mouseY) && sbL.trackHeight > 0)
			{
				var maxTop = this.menuItems.length - L.visibleCount;
				var thumb = sb.getThumbGeometry();
				var thumbRange = sbL.trackHeight - thumb.h;
				if (thumbRange > 0)
				{
					// Map the release Y position to a topIndex value
					var clampedY = Math.max(sbL.trackTop, Math.min(sbL.trackBot, mouseY));
					var relY = clampedY - sbL.trackTop;
					this.topIndex = Math.round(relY * maxTop / thumbRange);
					this.topIndex = Math.max(0, Math.min(maxTop, this.topIndex));
				}
				// If the selected item scrolled out of view, adjust:
				// scrolled down -> select topmost visible item
				// scrolled up -> select bottommost visible item
				if (this.selectedIndex < this.topIndex)
					this.selectedIndex = this.topIndex;
				else if (this.selectedIndex >= this.topIndex + L.visibleCount)
					this.selectedIndex = this.topIndex + L.visibleCount - 1;
			}
		}
		// --- Keyboard handling ---
		else
		{
			switch (key)
			{
				case KEY_UP:
					if (this.selectedIndex > 0)
						--this.selectedIndex;
					break;
				case KEY_DOWN:
					if (this.selectedIndex < this.menuItems.length - 1)
						++this.selectedIndex;
					break;
				case KEY_HOME:
					this.selectedIndex = 0;
					break;
				case KEY_END:
					this.selectedIndex = this.menuItems.length - 1;
					break;
				case KEY_PAGEUP:
					this.selectedIndex = Math.max(0, this.selectedIndex - L.visibleCount);
					break;
				case KEY_PAGEDN:
					this.selectedIndex = Math.min(this.menuItems.length - 1, this.selectedIndex + L.visibleCount);
					break;
				case '\r':
					retval = this.menuItems[this.selectedIndex].retval;
					break;
				default:
					// Check for hotkey match
					var upperKey = key.toUpperCase();
					for (var hi = 0; hi < this.menuItems.length; ++hi)
					{
						if (this.menuItems[hi].hotkey === upperKey)
						{
							retval = this.menuItems[hi].retval;
							break;
						}
					}
					break;
			}
		}

		// If a selection was made, return it
		if (typeof retval !== "undefined")
			return retval;

		// Adjust scroll position if selected item moved out of view
		if (this.selectedIndex < this.topIndex)
			this.topIndex = this.selectedIndex;
		else if (this.selectedIndex >= this.topIndex + L.visibleCount)
			this.topIndex = this.selectedIndex - L.visibleCount + 1;

		// Update the display efficiently
		if (this.topIndex !== oldTop)
		{
			// Viewport scrolled: redraw all visible items, scrollbar, and mouse regions
			this._drawFullMenu();
		}
		else if (this.selectedIndex !== oldSelected)
		{
			// Only redraw the previously-selected and newly-selected items
			var rip = this._buildItemRIP(oldSelected, false)
			        + this._buildItemRIP(this.selectedIndex, true);
			rip += RIPGotoXYNumeric(0, 0);
			this._sendRIP(rip);
		}
	}

	return null;
}


// ---- Internal methods ----

// Send a batch of RIP commands to the client.
// Uses \r (CR) before ! instead of \r\n (CRLF) to avoid advancing the text
// cursor to a new line, which would scroll the text window. Parks the text
// cursor at (0,0) after each batch to prevent cursor drift.
function RIPLightbarMenu_sendRIP(cmds)
{
	//console.write("\r!" + cmds + RIPGotoXYNumeric(0, 0) + "\r\n");
	console.write("\r!" + cmds + "\r\n");
}


// Compute internal layout geometry from the current pos, size, item count,
// and border settings. Creates the RIPScrollbar instance if scrolling is
// needed. Must be called before any drawing.
function RIPLightbarMenu_computeLayout()
{
	var L = {};
	var borderInset = this.useRIPBorder ? (RIP_LBMENU_BEVEL_TOTAL + RIP_LBMENU_INNER_PAD) : 0;

	// Border bounds (the outer rectangle of the menu)
	L.borderLeft = this.pos.x;
	L.borderTop = this.pos.y;
	L.borderRight = this.pos.x + this.size.width - 1;

	// Menu content area (inside the border)
	L.menuLeftX = L.borderLeft + borderInset;
	L.menuTopY = L.borderTop + borderInset;
	var contentRight = L.borderRight - borderInset;
	// Available pixel height for item rows (subtracting top and bottom insets)
	var contentHeight = this.size.height - 2 * borderInset;

	// Maximum number of visible items that fit
	L.maxVisible = Math.floor(contentHeight / this.itemHeight);
	L.visibleCount = Math.min(this.menuItems.length, L.maxVisible);

	// Determine if scrollbar is needed
	L.needsScrollbar = (this.menuItems.length > L.maxVisible);

	// Menu right edge (narrower if scrollbar is present)
	L.menuRightX = contentRight
	               - (L.needsScrollbar ? RIP_LBMENU_SCROLLBAR_WIDTH + RIP_LBMENU_SCROLLBAR_GAP : 0);
	L.textX = L.menuLeftX + this.textXOffset;

	// Compute actual border bottom to tightly fit the visible items
	L.borderBottom = L.menuTopY + L.visibleCount * this.itemHeight + borderInset;
	var maxBottom = this.pos.y + this.size.height - 1;
	if (L.borderBottom > maxBottom)
		L.borderBottom = maxBottom;

	this._layout = L;

	// Create or clear the RIPScrollbar instance
	if (L.needsScrollbar)
	{
		var sbX = L.menuRightX + RIP_LBMENU_SCROLLBAR_GAP;
		var sbWidth = contentRight - sbX + 1;
		var sbHeight = L.visibleCount * this.itemHeight;
		this._scrollbar = new RIPScrollbar(sbX, L.menuTopY, sbWidth, sbHeight);
		// Copy scrollbar-related colors from the menu's color settings
		this._scrollbar.colors.track = this.colors.scrollTrack;
		this._scrollbar.colors.thumb = this.colors.scrollThumb;
		this._scrollbar.colors.arrowBg = this.colors.scrollArrowBg;
		this._scrollbar.colors.arrowFg = this.colors.scrollArrowFg;
		this._scrollbar.colors.bevelBright = this.colors.bevelBright;
		this._scrollbar.colors.bevelDark = this.colors.bevelDark;
		// Set the scroll state
		this._scrollbar.setScrollState(this.menuItems.length, L.visibleCount, this.topIndex);
		this._scrollbar.computeLayout();
	}
	else
	{
		this._scrollbar = null;
	}
}


// Build RIP commands for the 3D border around the menu area (or just the
// content background if useRIPBorder is false), including the scrollbar.
function RIPLightbarMenu_buildMenuBorderRIP()
{
	var L = this._layout;
	var rip = "";

	if (!this.useRIPBorder)
	{
		// No border: just fill the content area background
		rip += RIPFillStyleNumeric(1, this.colors.contentBg);
		rip += RIPBarNumeric(L.menuLeftX, L.menuTopY,
		                     L.menuRightX, L.menuTopY + L.visibleCount * this.itemHeight - 1);
		if (this._scrollbar !== null)
			rip += this._scrollbar.buildFullRIP();
		return rip;
	}

	// Fill the entire border area with the surface color
	rip += RIPFillStyleNumeric(1, this.colors.borderSurface);
	rip += RIPBarNumeric(L.borderLeft, L.borderTop, L.borderRight, L.borderBottom);

	// Outer raised bevel: bright edges on top and left
	rip += RIPFillStyleNumeric(1, this.colors.bevelBright);
	rip += RIPBarNumeric(L.borderLeft, L.borderTop,
	                     L.borderRight, L.borderTop + RIP_LBMENU_BEVEL_OUTER - 1);
	rip += RIPBarNumeric(L.borderLeft, L.borderTop + RIP_LBMENU_BEVEL_OUTER,
	                     L.borderLeft + RIP_LBMENU_BEVEL_OUTER - 1, L.borderBottom);
	// Outer raised bevel: dark edges on bottom and right
	rip += RIPFillStyleNumeric(1, this.colors.bevelDark);
	rip += RIPBarNumeric(L.borderLeft, L.borderBottom - RIP_LBMENU_BEVEL_OUTER + 1,
	                     L.borderRight, L.borderBottom);
	rip += RIPBarNumeric(L.borderRight - RIP_LBMENU_BEVEL_OUTER + 1, L.borderTop,
	                     L.borderRight, L.borderBottom - RIP_LBMENU_BEVEL_OUTER);

	// Inner sunken bevel (inset from outer bevel + gap)
	var ix0 = L.borderLeft + RIP_LBMENU_BEVEL_OUTER + RIP_LBMENU_BEVEL_GAP;
	var iy0 = L.borderTop + RIP_LBMENU_BEVEL_OUTER + RIP_LBMENU_BEVEL_GAP;
	var ix1 = L.borderRight - RIP_LBMENU_BEVEL_OUTER - RIP_LBMENU_BEVEL_GAP;
	var iy1 = L.borderBottom - RIP_LBMENU_BEVEL_OUTER - RIP_LBMENU_BEVEL_GAP;
	// Dark edges on top and left
	rip += RIPFillStyleNumeric(1, this.colors.bevelDark);
	rip += RIPBarNumeric(ix0, iy0, ix1, iy0 + RIP_LBMENU_BEVEL_INNER - 1);
	rip += RIPBarNumeric(ix0, iy0 + RIP_LBMENU_BEVEL_INNER,
	                     ix0 + RIP_LBMENU_BEVEL_INNER - 1, iy1);
	// Bright edges on bottom and right
	rip += RIPFillStyleNumeric(1, this.colors.bevelBright);
	rip += RIPBarNumeric(ix0, iy1 - RIP_LBMENU_BEVEL_INNER + 1, ix1, iy1);
	rip += RIPBarNumeric(ix1 - RIP_LBMENU_BEVEL_INNER + 1, iy0,
	                     ix1, iy1 - RIP_LBMENU_BEVEL_INNER);

	// Fill the content area with the content background color
	rip += RIPFillStyleNumeric(1, this.colors.contentBg);
	rip += RIPBarNumeric(L.menuLeftX, L.menuTopY,
	                     L.menuRightX, L.menuTopY + L.visibleCount * this.itemHeight - 1);

	// Draw the scrollbar (to the right of the menu items)
	if (this._scrollbar !== null)
		rip += this._scrollbar.buildFullRIP();

	return rip;
}


// Build RIP commands to draw a single menu item at its visible position.
function RIPLightbarMenu_buildItemRIP(index, isSelected)
{
	var L = this._layout;
	var bgColor = isSelected ? this.colors.selectedBg : this.colors.itemBg;
	var fgColor = isSelected ? this.colors.selectedFg : this.colors.itemFg;
	var visPos = index - this.topIndex;
	var y = L.menuTopY + visPos * this.itemHeight;

	var rip = "";
	// Draw filled background rectangle for this item
	rip += RIPFillStyleNumeric(1, bgColor);
	rip += RIPBarNumeric(L.menuLeftX, y, L.menuRightX, y + this.itemHeight - 1);
	// Draw item text
	rip += RIPColorNumeric(fgColor);
	rip += RIPFontStyleNumeric(RIP_FONT_DEFAULT, 0, 1, 0);
	rip += RIPTextXYNumeric(L.textX, y + this.textYOffset, this.menuItems[index].text);
	return rip;
}


// Build RIP mouse region commands for all visible items and scrollbar controls.
function RIPLightbarMenu_buildMouseRegionsRIP()
{
	var L = this._layout;
	var rip = RIPKillMouseFields();

	// Mouse region for each visible menu item
	var dispCount = Math.min(L.visibleCount, this.menuItems.length - this.topIndex);
	for (var i = 0; i < dispCount; ++i)
	{
		var y = L.menuTopY + i * this.itemHeight;
		var clickChar = String.fromCharCode(RIP_LBMENU_MOUSE_ITEM_BASE + i);
		rip += RIPMouseNumeric(0, L.menuLeftX, y, L.menuRightX, y + this.itemHeight - 1,
		                       true, false, 0, clickChar);
	}

	// Scrollbar mouse regions (delegated to the RIPScrollbar instance)
	if (this._scrollbar !== null)
		rip += this._scrollbar.buildMouseRegionsRIP();

	return rip;
}


// Draw all visible menu items, update the scrollbar thumb, and rebuild
// mouse regions. Sends everything as a single RIP command batch.
function RIPLightbarMenu_drawFullMenu()
{
	var L = this._layout;
	var rip = "";

	// Draw each visible item
	for (var i = this.topIndex; i < this.topIndex + L.visibleCount && i < this.menuItems.length; ++i)
		rip += this._buildItemRIP(i, i === this.selectedIndex);

	// If fewer items than max visible, clear the remaining area
	if (this.menuItems.length - this.topIndex < L.visibleCount)
	{
		var clearY = L.menuTopY + (this.menuItems.length - this.topIndex) * this.itemHeight;
		rip += RIPFillStyleNumeric(1, this.colors.contentBg);
		rip += RIPBarNumeric(L.menuLeftX, clearY, L.menuRightX,
		                     L.menuTopY + L.visibleCount * this.itemHeight - 1);
	}

	// Update the scrollbar thumb position
	if (this._scrollbar !== null)
	{
		this._scrollbar.topIndex = this.topIndex;
		rip += this._scrollbar.buildThumbRIP();
	}

	// Rebuild mouse regions (item positions and thumb position may have changed)
	rip += this._buildMouseRegionsRIP();

	rip += RIPGotoXYNumeric(0, 0);
	this._sendRIP(rip);
}
