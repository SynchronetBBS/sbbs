// RIPLightbarMenu: A general-purpose lightbar menu class for RIP/RIPScrip terminals.
// Displays a scrollable list of items with optional 3D border and scrollbar,
// handles keyboard and mouse input, and returns the user's selection.
//
// This class is API-compatible with DDLightbarMenu where applicable, supporting:
//  - OnItemSelect(pItemRetval, pSelected): Called when an item is selected/toggled
//  - ValidateSelectItem(pItemRetval): Called to validate selection (return boolean)
//  - OnItemNav(pOldItemIdx, pNewItemIdx): Called when user navigates to a new item
//  - exitOnItemSelect: Boolean controlling whether GetVal exits on selection
//  - multiSelect: Boolean enabling multi-select mode with selectedItemIndexes
//  - NumItems()/GetItem(idx): Overrideable for dynamic item lists
//  - Hotkey support: SetItemHotkey, AddItemHotkey, RemoveItemHotkey, etc.
//  - Additional quit/select keys: AddAdditionalQuitKeys, etc.
//  - lastUserInput: Stores the last keypress for caller inspection

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


// Returns a default menu item object compatible with RIPLightbarMenu.
// This is the canonical shape for all items in the menu's internal list.
// The 'hotkeys' field is a string where each character is a hotkey that
// can trigger immediate selection of this item (case-insensitive by default).
// This matches the item structure used by DDLightbarMenu (getDefaultMenuItem).
function getRIPLBMenuDefaultItem()
{
	return {
		text: "",        // Display text for the item
		retval: null,    // Value returned when the item is selected
		hotkeys: "",     // String of hotkey characters (each char is a separate hotkey)
		isSelectable: true
	};
}


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

	// Navigation state.  Property names match DDLightbarMenu for API compatibility,
	// so callers (e.g. ddfilelister.js) can use the same code for either menu class.
	this.selectedItemIdx = 0; // Index of the currently highlighted item
	this.topItemIdx = 0;      // Index of the first visible item (scroll position)

	// Item row dimensions in pixels (for the default 8x8 RIP font)
	this.itemHeight = 16;   // Height of each menu item row
	this.textYOffset = 4;   // Y offset for text baseline within an item row
	this.textXOffset = 4;   // X offset for text from the left edge of the content area

	// --- Callback functions (matching DDLightbarMenu API) ---
	// ValidateSelectItem is called before an item is selected.  Return false to
	// prevent selection (e.g. to show an error instead).  Default: always allow.
	this.ValidateSelectItem = function(pItemRetval) { return true; };
	// OnItemSelect is called after an item is selected (single-select) or toggled
	// (multi-select).  pSelected is true for selection, false for de-selection.
	this.OnItemSelect = function(pItemRetval, pSelected) { };
	// OnItemNav is called when the user navigates to a different item via arrow
	// keys, page keys, home/end, or mouse scroll.  Useful for updating a detail
	// pane (e.g. extended file descriptions) when the selection changes.
	this.OnItemNav = function(pOldItemIdx, pNewItemIdx) { };
	// When true, OnItemNav is called once at startup (with pOldItemIdx = -1) so
	// the caller can initialize the detail pane for the initially-selected item.
	this.callOnItemNavOnStartup = false;

	// --- Selection behavior ---
	// When true (default), GetVal returns immediately after the user selects an
	// item.  When false, the menu stays visible after selection — useful with
	// OnItemSelect to perform actions without leaving the menu.
	this.exitOnItemSelect = true;
	// Stores the last keypress from GetVal so the caller can inspect what key
	// the user pressed (e.g. to distinguish Enter from a quit key).
	this.lastUserInput = null;

	// --- Multi-select support ---
	// When true, users can toggle individual items on/off (via Space or mouse
	// click) and GetVal returns the selectedItemIndexes object instead of a
	// single retval.
	this.multiSelect = false;
	this.maxNumSelections = -1; // -1 or 0 = unlimited number of selections
	this.multiSelectItemChar = "\xFB"; // CP437 check mark character for display
	// Object whose keys are selected item indexes (value = true).
	// Callers can pass a reference via GetVal's second parameter.
	this.selectedItemIndexes = {};
	// When true and in multi-select mode, pressing Enter also adds the current
	// item to the selection before returning.
	this.enterAndSelectKeysAddsMultiSelectItem = false;

	// --- Hotkey behavior ---
	// When false (default), hotkey matching is case-insensitive: both the
	// pressed key and the stored hotkeys are uppercased before comparison.
	this.hotkeyCaseSensitive = false;

	// --- Additional key strings ---
	// Characters in these strings are treated as quit/select keys in addition
	// to the built-in keys (ESC for quit, Enter for select).  Used by callers
	// like ddfilelister.js to make menu-bar hotkeys exit the input loop.
	this.additionalQuitKeys = "";
	this.additionalSelectItemKeys = "";

	// Internal computed layout (set by _computeLayout)
	this._layout = null;
	// Internal RIPScrollbar instance (created by _computeLayout when needed)
	this._scrollbar = null;

	// Public methods - Item management
	this.Add = RIPLightbarMenu_Add;
	this.Remove = RIPLightbarMenu_Remove;
	this.RemoveAllItems = RIPLightbarMenu_RemoveAllItems;
	this.NumItems = RIPLightbarMenu_NumItems;
	this.GetItem = RIPLightbarMenu_GetItem;
	this.MakeItemWithRetval = RIPLightbarMenu_MakeItemWithRetval;
	this.MakeItemWithTextAndRetval = RIPLightbarMenu_MakeItemWithTextAndRetval;

	// Public methods - Hotkey management
	this.SetItemHotkey = RIPLightbarMenu_SetItemHotkey;
	this.AddItemHotkey = RIPLightbarMenu_AddItemHotkey;
	this.RemoveItemHotkey = RIPLightbarMenu_RemoveItemHotkey;
	this.RemoveAllItemHotkeys = RIPLightbarMenu_RemoveAllItemHotkeys;

	// Public methods - Quit/select key management
	this.AddAdditionalQuitKeys = RIPLightbarMenu_AddAdditionalQuitKeys;
	this.ClearAdditionalQuitKeys = RIPLightbarMenu_ClearAdditionalQuitKeys;
	this.QuitKeysIncludes = RIPLightbarMenu_QuitKeysIncludes;
	this.AddAdditionalSelectItemKeys = RIPLightbarMenu_AddAdditionalSelectItemKeys;
	this.ClearAdditionalSelectItemKeys = RIPLightbarMenu_ClearAdditionalSelectItemKeys;
	this.SelectItemKeysIncludes = RIPLightbarMenu_SelectItemKeysIncludes;

	// Public methods - Navigation/selection
	this.SetSelectedItemIdx = RIPLightbarMenu_SetSelectedItemIdx;
	this.GetVal = RIPLightbarMenu_GetVal;

	// Internal methods
	this._sendRIP = RIPLightbarMenu_sendRIP;
	this._computeLayout = RIPLightbarMenu_computeLayout;
	this._buildItemRIP = RIPLightbarMenu_buildItemRIP;
	this._buildMenuBorderRIP = RIPLightbarMenu_buildMenuBorderRIP;
	this._buildMouseRegionsRIP = RIPLightbarMenu_buildMouseRegionsRIP;
	this._drawFullMenu = RIPLightbarMenu_drawFullMenu;
}


// ---- Item management methods ----

// Adds an item to the menu.
//
// Parameters:
//  pItemText: The text to show for the menu item
//  pItemRetVal: The return value to be returned when the user chooses the item.
//               If undefined, defaults to the current item count (0-based index).
//  pHotkey: Optional hotkey character(s) that select this item immediately when
//           the user presses one of these keys.  Can be a single character like
//           "A" or multiple characters like "aA" — each character in the string
//           is treated as a separate hotkey.  Matching is case-insensitive by
//           default (controlled by this.hotkeyCaseSensitive).
function RIPLightbarMenu_Add(pItemText, pItemRetVal, pHotkey)
{
	var item = getRIPLBMenuDefaultItem();
	item.text = pItemText;
	item.retval = (pItemRetVal === undefined ? this.NumItems() : pItemRetVal);
	if (typeof pHotkey === "string" && pHotkey.length > 0)
		item.hotkeys = pHotkey;
	this.menuItems.push(item);
}

// Removes an item at the given index.
function RIPLightbarMenu_Remove(pIdx)
{
	if (typeof pIdx !== "number" || pIdx < 0 || pIdx >= this.menuItems.length)
		return;
	this.menuItems.splice(pIdx, 1);
	if (this.menuItems.length > 0)
	{
		if (this.selectedItemIdx >= this.menuItems.length)
			this.selectedItemIdx = this.menuItems.length - 1;
	}
	else
	{
		this.selectedItemIdx = 0;
		this.topItemIdx = 0;
	}
}

// Removes all items from the menu.
function RIPLightbarMenu_RemoveAllItems()
{
	this.menuItems = [];
	this.selectedItemIdx = 0;
	this.topItemIdx = 0;
}

// Returns the number of items in the menu.
// This method can be overridden on the instance to support dynamic/virtual item
// lists.  For example, ddfilelister.js overrides it to return gFileList.length
// instead of pre-loading all items into menuItems[].  When overridden, GetItem()
// must also be overridden to return items at arbitrary indexes.  All internal
// methods (_computeLayout, _drawFullMenu, GetVal, etc.) use NumItems()/GetItem()
// rather than accessing menuItems[] directly, so the override is transparent.
function RIPLightbarMenu_NumItems()
{
	return this.menuItems.length;
}

// Returns an item at the given index.
// Can be overridden on the instance for dynamic lists (see NumItems comment).
// The returned object must have at least { text, retval, hotkeys } properties.
// Use MakeItemWithRetval() or MakeItemWithTextAndRetval() to create items with
// the correct shape when overriding this method.
//
// Return value: Item object, or null if pItemIndex is out of range.
function RIPLightbarMenu_GetItem(pItemIndex)
{
	if (pItemIndex < 0 || pItemIndex >= this.NumItems())
		return null;
	return this.menuItems[pItemIndex];
}

// Makes an item object compatible with RIPLightbarMenu, with a given return value.
function RIPLightbarMenu_MakeItemWithRetval(pRetval)
{
	var item = getRIPLBMenuDefaultItem();
	item.retval = pRetval;
	return item;
}

// Makes an item object with text and return value set.
function RIPLightbarMenu_MakeItemWithTextAndRetval(pText, pRetval)
{
	var item = getRIPLBMenuDefaultItem();
	item.text = pText;
	item.retval = pRetval;
	return item;
}


// ---- Hotkey management methods ----

// Sets (replaces) the hotkey(s) for an item at the given index.
function RIPLightbarMenu_SetItemHotkey(pIdx, pHotkey)
{
	if (typeof pIdx === "number" && pIdx >= 0 && pIdx < this.NumItems() && typeof pHotkey === "string")
	{
		var item = this.GetItem(pIdx);
		if (item !== null)
			item.hotkeys = pHotkey;
	}
}

// Adds an additional hotkey to an item (keeps existing hotkeys).
function RIPLightbarMenu_AddItemHotkey(pIdx, pHotkey)
{
	if (typeof pIdx === "number" && pIdx >= 0 && pIdx < this.NumItems() && typeof pHotkey === "string")
	{
		var item = this.GetItem(pIdx);
		if (item !== null && item.hotkeys.indexOf(pHotkey) === -1)
			item.hotkeys += pHotkey;
	}
}

// Removes a specific hotkey character from an item.  If the same character
// appears multiple times in the hotkeys string, all occurrences are removed.
function RIPLightbarMenu_RemoveItemHotkey(pIdx, pHotkey)
{
	if (typeof pIdx === "number" && pIdx >= 0 && pIdx < this.NumItems())
	{
		var item = this.GetItem(pIdx);
		if (item !== null)
		{
			// Loop to remove all occurrences of pHotkey from the hotkeys string
			var hotkeyIdx = item.hotkeys.indexOf(pHotkey);
			while (hotkeyIdx > -1)
			{
				item.hotkeys = item.hotkeys.substr(0, hotkeyIdx) + item.hotkeys.substr(hotkeyIdx + 1);
				hotkeyIdx = item.hotkeys.indexOf(pHotkey);
			}
		}
	}
}

// Removes hotkeys from all items.
function RIPLightbarMenu_RemoveAllItemHotkeys()
{
	for (var i = 0; i < this.NumItems(); ++i)
	{
		var item = this.GetItem(i);
		if (item !== null)
			item.hotkeys = "";
	}
}


// ---- Quit/select key management methods ----

function RIPLightbarMenu_AddAdditionalQuitKeys(pKeys)
{
	if (typeof pKeys === "string")
		this.additionalQuitKeys += pKeys;
}

function RIPLightbarMenu_ClearAdditionalQuitKeys()
{
	this.additionalQuitKeys = "";
}

function RIPLightbarMenu_QuitKeysIncludes(pKey)
{
	return (this.additionalQuitKeys.indexOf(pKey) > -1);
}

function RIPLightbarMenu_AddAdditionalSelectItemKeys(pKeys)
{
	if (typeof pKeys === "string")
		this.additionalSelectItemKeys += pKeys;
}

function RIPLightbarMenu_ClearAdditionalSelectItemKeys()
{
	this.additionalSelectItemKeys = "";
}

function RIPLightbarMenu_SelectItemKeysIncludes(pKey)
{
	return (this.additionalSelectItemKeys.indexOf(pKey) > -1);
}


// ---- Navigation methods ----

// Sets the selected item index, adjusting topItemIdx as needed.
function RIPLightbarMenu_SetSelectedItemIdx(pSelectedItemIdx)
{
	if (typeof pSelectedItemIdx !== "number")
		return;
	if (pSelectedItemIdx < 0 || pSelectedItemIdx >= this.NumItems())
		return;
	this.selectedItemIdx = pSelectedItemIdx;
	// Adjust topItemIdx if the layout has been computed
	if (this._layout !== null)
	{
		if (this.selectedItemIdx >= this.topItemIdx + this._layout.visibleCount)
			this.topItemIdx = this.selectedItemIdx - this._layout.visibleCount + 1;
		else if (this.selectedItemIdx < this.topItemIdx)
			this.topItemIdx = this.selectedItemIdx;
	}
}


// ---- Main input method ----

// Shows the lightbar menu, starts the user input loop to let the user choose
// an item from the menu, and returns the user's chosen item.
//
// In single-select mode: Returns the retval of the selected item, or null if aborted.
// In multi-select mode: Returns the selectedItemIndexes object, or null if aborted.
//
// Parameters (two calling conventions supported):
//  1) GetVal(pDefaultRetVal) - Pre-selects the item whose retval matches pDefaultRetVal
//  2) GetVal(pDrawFunc, pSelectedItemIndexes) - DDLightbarMenu-compatible:
//     pDrawFunc: Optional function to call before drawing the menu
//     pSelectedItemIndexes: Optional object for tracking multi-select state
function RIPLightbarMenu_GetVal(pFirstParam, pSelectedItemIndexes)
{
	var numItems = this.NumItems();
	if (numItems === 0)
		return null;

	// Detect calling convention by type of the first parameter:
	//  - function: DDLightbarMenu-style draw callback (called before rendering)
	//  - number/string: uselect_rip.js-style default retval (find and pre-select
	//    the item whose retval matches this value, using == for loose comparison
	//    so numeric strings match numbers)
	//  - null/undefined: no special behavior
	var drawFunc = null;
	if (typeof pFirstParam === "function")
		drawFunc = pFirstParam;
	else if (typeof pFirstParam === "number" || typeof pFirstParam === "string")
	{
		for (var di = 0; di < numItems; ++di)
		{
			var dItem = this.GetItem(di);
			if (dItem !== null && dItem.retval == pFirstParam)
			{
				this.selectedItemIdx = di;
				break;
			}
		}
	}

	// If the caller passed a selectedItemIndexes object (DDLightbarMenu pattern),
	// use it as our multi-select state so the caller can inspect it after GetVal
	// returns.  This allows the caller to pre-populate selections or share the
	// object across multiple GetVal calls.
	if (typeof pSelectedItemIndexes === "object" && pSelectedItemIndexes !== null)
		this.selectedItemIndexes = pSelectedItemIndexes;

	this.lastUserInput = null;

	this._computeLayout();
	var L = this._layout;
	var sb = this._scrollbar; // May be null if no scrollbar needed

	// Clamp selectedItemIdx to valid range
	if (this.selectedItemIdx < 0)
		this.selectedItemIdx = 0;
	else if (this.selectedItemIdx >= numItems)
		this.selectedItemIdx = numItems - 1;

	// Ensure selected item is within the visible scroll window
	if (this.selectedItemIdx >= this.topItemIdx + L.visibleCount)
		this.topItemIdx = this.selectedItemIdx - L.visibleCount + 1;
	if (this.selectedItemIdx < this.topItemIdx)
		this.topItemIdx = this.selectedItemIdx;

	// Draw the border/background and initial menu contents
	if (drawFunc !== null)
		drawFunc();
	this._sendRIP(this._buildMenuBorderRIP());
	this._drawFullMenu();

	// Call OnItemNav on startup if configured
	if (this.callOnItemNavOnStartup && typeof this.OnItemNav === "function")
		this.OnItemNav(-1, this.selectedItemIdx);

	// Main input loop
	while (bbs.online && !js.terminated)
	{
		var key = console.getkey(K_NOCRLF|K_NOECHO|K_NOSPIN);
		this.lastUserInput = key;

		// Check quit keys
		if (key.toUpperCase() == console.quit_key || this.QuitKeysIncludes(key))
			return null;

		var oldSelected = this.selectedItemIdx;
		var oldTop = this.topItemIdx;
		var ch = key.charCodeAt(0);
		// These two variables track whether a definitive selection was made during
		// this iteration.  They are only used in single-select mode — in multi-select
		// mode, toggling is handled inline and the loop continues.  After the
		// switch/if-else block, if selectionMade is true, the selection is validated
		// via ValidateSelectItem, OnItemSelect is called, and (if exitOnItemSelect
		// is true) the retval is returned to the caller.
		var selectionMade = false;
		var selectedRetval = undefined;

		// --- Mouse event handling ---
		// RIP mouse regions send high-byte characters (0x80+) when clicked.
		// Each visible menu item has a mouse region that sends
		// RIP_LBMENU_MOUSE_ITEM_BASE + visible_position.  The scrollbar widget
		// uses its own set of high-byte identifiers (0xFA-0xFE) for arrow
		// buttons, track areas, and thumb drag.

		// Menu item click
		if (ch >= RIP_LBMENU_MOUSE_ITEM_BASE && ch < RIP_LBMENU_MOUSE_ITEM_BASE + L.visibleCount)
		{
			var clickedIndex = this.topItemIdx + (ch - RIP_LBMENU_MOUSE_ITEM_BASE);
			if (clickedIndex >= 0 && clickedIndex < numItems)
			{
				var clickedItem = this.GetItem(clickedIndex);
				if (clickedItem !== null)
				{
					if (this.multiSelect)
					{
						// In multi-select mode, clicking an item toggles its
						// selected/deselected state (like a checkbox).  The toggle
						// is gated by ValidateSelectItem and maxNumSelections.
						if (this.ValidateSelectItem(clickedItem.retval))
						{
							var wasSelected = (this.selectedItemIndexes[clickedIndex] === true);
							if (wasSelected)
							{
								delete this.selectedItemIndexes[clickedIndex];
								this.OnItemSelect(clickedItem.retval, false);
							}
							else
							{
								if (this.maxNumSelections <= 0 || Object.keys(this.selectedItemIndexes).length < this.maxNumSelections)
								{
									this.selectedItemIndexes[clickedIndex] = true;
									this.OnItemSelect(clickedItem.retval, true);
								}
							}
							// Move the highlight to the clicked item so the user
							// sees which item they toggled
							this.selectedItemIdx = clickedIndex;
						}
					}
					else
					{
						// Single-select: clicking an item selects it immediately
						selectionMade = true;
						selectedRetval = clickedItem.retval;
					}
				}
			}
		}
		// Scrollbar mouse events (only if scrollbar exists)
		else if (sb !== null && ch === sb.mouseChars.scrollUp)
		{
			if (this.selectedItemIdx > 0)
				--this.selectedItemIdx;
		}
		else if (sb !== null && ch === sb.mouseChars.scrollDn)
		{
			if (this.selectedItemIdx < numItems - 1)
				++this.selectedItemIdx;
		}
		else if (sb !== null && ch === sb.mouseChars.trackPgUp)
		{
			this.selectedItemIdx = Math.max(0, this.selectedItemIdx - L.visibleCount);
		}
		else if (sb !== null && ch === sb.mouseChars.trackPgDn)
		{
			this.selectedItemIdx = Math.min(numItems - 1, this.selectedItemIdx + L.visibleCount);
		}
		else if (sb !== null && ch === sb.mouseChars.thumbDrag)
		{
			// Thumb drag: the RIP mouse region for the scrollbar thumb includes
			// the $Y$ text variable, which SyncTerm expands to the mouse Y pixel
			// coordinate at the time of button release.  The coordinate arrives
			// as up to 4 ASCII digit characters immediately after the thumb-drag
			// identifier byte.  We read these digits with a short timeout, parse
			// the Y value, and map it proportionally to a scroll position.
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
				// Map the Y pixel position within the track to a topItemIdx value.
				// The track spans from sbL.trackTop to sbL.trackBot; the thumb
				// occupies thumb.h pixels.  The usable range for positioning the
				// thumb's top edge is trackHeight - thumbHeight pixels, which maps
				// linearly to 0..maxTop scroll positions.
				var maxTop = numItems - L.visibleCount;
				var thumb = sb.getThumbGeometry();
				var thumbRange = sbL.trackHeight - thumb.h;
				if (thumbRange > 0)
				{
					var clampedY = Math.max(sbL.trackTop, Math.min(sbL.trackBot, mouseY));
					var relY = clampedY - sbL.trackTop;
					this.topItemIdx = Math.round(relY * maxTop / thumbRange);
					this.topItemIdx = Math.max(0, Math.min(maxTop, this.topItemIdx));
				}
				// If the highlighted item scrolled out of view, clamp it to the
				// nearest visible item
				if (this.selectedItemIdx < this.topItemIdx)
					this.selectedItemIdx = this.topItemIdx;
				else if (this.selectedItemIdx >= this.topItemIdx + L.visibleCount)
					this.selectedItemIdx = this.topItemIdx + L.visibleCount - 1;
			}
		}
		// --- Keyboard handling ---
		else
		{
			switch (key)
			{
				case KEY_UP:
					if (this.selectedItemIdx > 0)
						--this.selectedItemIdx;
					break;
				case KEY_DOWN:
					if (this.selectedItemIdx < numItems - 1)
						++this.selectedItemIdx;
					break;
				case KEY_HOME:
					this.selectedItemIdx = 0;
					break;
				case KEY_END:
					this.selectedItemIdx = numItems - 1;
					break;
				case KEY_PAGEUP:
					this.selectedItemIdx = Math.max(0, this.selectedItemIdx - L.visibleCount);
					break;
				case KEY_PAGEDN:
					this.selectedItemIdx = Math.min(numItems - 1, this.selectedItemIdx + L.visibleCount);
					break;
				case '\r':
					// Enter key behavior differs between single-select and multi-select:
					if (this.multiSelect)
					{
						// In multi-select mode, Enter optionally adds the current item
						// to the selection (if enterAndSelectKeysAddsMultiSelectItem is
						// true and the item isn't already selected), then returns the
						// selectedItemIndexes object.  This allows the caller to collect
						// all toggled items and process them as a batch.
						if (this.enterAndSelectKeysAddsMultiSelectItem)
						{
							var enterItem = this.GetItem(this.selectedItemIdx);
							if (enterItem !== null && this.ValidateSelectItem(enterItem.retval))
							{
								if (this.selectedItemIndexes[this.selectedItemIdx] !== true)
								{
									if (this.maxNumSelections <= 0 || Object.keys(this.selectedItemIndexes).length < this.maxNumSelections)
									{
										this.selectedItemIndexes[this.selectedItemIdx] = true;
										this.OnItemSelect(enterItem.retval, true);
									}
								}
							}
						}
						if (this.exitOnItemSelect)
							return this.selectedItemIndexes;
					}
					else
					{
						// In single-select mode, Enter marks the current item as
						// selected.  The actual return happens below after the
						// ValidateSelectItem/OnItemSelect flow.
						var selItem = this.GetItem(this.selectedItemIdx);
						if (selItem !== null)
						{
							selectionMade = true;
							selectedRetval = selItem.retval;
						}
					}
					break;
				default:
					// The default case handles three types of keys in order:
					// 1. Space key in multi-select mode (toggle current item)
					// 2. Additional select-item keys (configured by caller)
					// 3. Hotkey matching (scan all items for a matching hotkey)
					// In single-select mode, space falls through to hotkey
					// matching, preserving backward compatibility with callers
					// that don't use multi-select.

					// Space key: toggle selection in multi-select mode
					if (key === ' ' && this.multiSelect)
					{
						var spaceItem = this.GetItem(this.selectedItemIdx);
						if (spaceItem !== null && this.ValidateSelectItem(spaceItem.retval))
						{
							var wasSpaceSel = (this.selectedItemIndexes[this.selectedItemIdx] === true);
							if (wasSpaceSel)
							{
								delete this.selectedItemIndexes[this.selectedItemIdx];
								this.OnItemSelect(spaceItem.retval, false);
							}
							else
							{
								if (this.maxNumSelections <= 0 || Object.keys(this.selectedItemIndexes).length < this.maxNumSelections)
								{
									this.selectedItemIndexes[this.selectedItemIdx] = true;
									this.OnItemSelect(spaceItem.retval, true);
								}
							}
						}
						break;
					}
					// Check additional select item keys
					if (this.SelectItemKeysIncludes(key))
					{
						if (this.multiSelect)
						{
							var selKeyItem = this.GetItem(this.selectedItemIdx);
							if (selKeyItem !== null && this.ValidateSelectItem(selKeyItem.retval))
							{
								var wasSelKey = (this.selectedItemIndexes[this.selectedItemIdx] === true);
								if (wasSelKey)
								{
									delete this.selectedItemIndexes[this.selectedItemIdx];
									this.OnItemSelect(selKeyItem.retval, false);
								}
								else
								{
									if (this.maxNumSelections <= 0 || Object.keys(this.selectedItemIndexes).length < this.maxNumSelections)
									{
										this.selectedItemIndexes[this.selectedItemIdx] = true;
										this.OnItemSelect(selKeyItem.retval, true);
									}
								}
							}
						}
						else
						{
							var selKeyItem2 = this.GetItem(this.selectedItemIdx);
							if (selKeyItem2 !== null)
							{
								selectionMade = true;
								selectedRetval = selKeyItem2.retval;
							}
						}
						break;
					}
					// Hotkey matching: scan all items for one whose hotkeys string
					// contains the pressed key.  When hotkeyCaseSensitive is false
					// (the default), both the pressed key and the item's hotkeys
					// are uppercased before comparison, making matching
					// case-insensitive.  In multi-select mode, a hotkey match just
					// navigates to the item (moves the highlight) without selecting
					// it — the user must press Space or Enter to toggle/confirm.
					// In single-select mode, a hotkey match immediately selects
					// the item.
					var matchKey = this.hotkeyCaseSensitive ? key : key.toUpperCase();
					for (var hi = 0; hi < numItems; ++hi)
					{
						var hkItem = this.GetItem(hi);
						if (hkItem === null || hkItem.hotkeys.length === 0)
							continue;
						var hotkeys = this.hotkeyCaseSensitive ? hkItem.hotkeys : hkItem.hotkeys.toUpperCase();
						if (hotkeys.indexOf(matchKey) > -1)
						{
							if (this.multiSelect)
							{
								this.selectedItemIdx = hi;
							}
							else
							{
								selectionMade = true;
								selectedRetval = hkItem.retval;
							}
							break;
						}
					}
					break;
			}
		}

		// --- Single-select selection flow ---
		// If an item was selected (via Enter, mouse click, or hotkey in
		// single-select mode), validate it and fire the callback.  If
		// ValidateSelectItem returns false, the selection is rejected and the
		// menu continues.  If exitOnItemSelect is true, return the retval to
		// the caller; otherwise, keep the menu open (useful when OnItemSelect
		// performs an action and the user should continue browsing).
		if (selectionMade && typeof selectedRetval !== "undefined")
		{
			if (this.ValidateSelectItem(selectedRetval))
			{
				this.OnItemSelect(selectedRetval, true);
				if (this.exitOnItemSelect)
					return selectedRetval;
			}
		}

		// Notify the caller when the highlighted item changes, so it can update
		// any related display (e.g. an extended description pane).
		if (this.selectedItemIdx !== oldSelected && typeof this.OnItemNav === "function")
			this.OnItemNav(oldSelected, this.selectedItemIdx);

		// --- Scroll adjustment ---
		// If the highlighted item moved outside the visible window (e.g. from
		// arrow keys or page keys), adjust topItemIdx to bring it into view.
		if (this.selectedItemIdx < this.topItemIdx)
			this.topItemIdx = this.selectedItemIdx;
		else if (this.selectedItemIdx >= this.topItemIdx + L.visibleCount)
			this.topItemIdx = this.selectedItemIdx - L.visibleCount + 1;

		// --- Efficient display update ---
		// Only redraw what changed to minimize RIP data sent over the wire:
		if (this.topItemIdx !== oldTop)
		{
			// Viewport scrolled: must redraw all visible items, update the
			// scrollbar thumb position, and rebuild mouse regions (since the
			// visible items shifted).
			this._drawFullMenu();
		}
		else if (this.selectedItemIdx !== oldSelected)
		{
			// Only the highlight moved within the same viewport: redraw just
			// the previously-highlighted item (to remove highlight) and the
			// newly-highlighted item (to add highlight).  This sends much less
			// data than a full redraw.
			var rip = this._buildItemRIP(oldSelected, false)
			        + this._buildItemRIP(this.selectedItemIdx, true);
			rip += RIPGotoXYNumeric(0, 0);
			this._sendRIP(rip);
		}
	}

	return null;
}


// ---- Internal methods ----

// Send a batch of RIP commands to the client.
function RIPLightbarMenu_sendRIP(cmds)
{
	console.write("\r!" + cmds + "\r\n");
}


// Compute internal layout geometry from the current pos, size, item count,
// and border settings.  The layout is stored in this._layout and used by all
// drawing methods.  Also creates a RIPScrollbar instance if the item count
// exceeds the visible area.  Must be called before any drawing, and should be
// called again if the menu's position, size, or item count changes.
//
// Layout structure (all values in pixels):
//   borderLeft/Top/Right/Bottom - outer edge of the border (or content if no border)
//   menuLeftX/menuTopY/menuRightX - content area where items are drawn
//   textX - left edge for item text (menuLeftX + textXOffset)
//   maxVisible - max items that fit in the content area
//   visibleCount - actual number of items shown (min of maxVisible and item count)
//   needsScrollbar - whether a scrollbar is needed
function RIPLightbarMenu_computeLayout()
{
	var L = {};
	var numItems = this.NumItems();
	// borderInset is the total pixel inset on each side for the 3D bevel border.
	// When useRIPBorder is false, items start at the menu's exact pos with no inset.
	var borderInset = this.useRIPBorder ? (RIP_LBMENU_BEVEL_TOTAL + RIP_LBMENU_INNER_PAD) : 0;

	// Border bounds (the outer rectangle of the menu)
	L.borderLeft = this.pos.x;
	L.borderTop = this.pos.y;
	L.borderRight = this.pos.x + this.size.width - 1;

	// Menu content area (inside the border, where item rows are drawn)
	L.menuLeftX = L.borderLeft + borderInset;
	L.menuTopY = L.borderTop + borderInset;
	var contentRight = L.borderRight - borderInset;
	var contentHeight = this.size.height - 2 * borderInset;

	// How many item rows fit vertically
	L.maxVisible = Math.floor(contentHeight / this.itemHeight);
	L.visibleCount = Math.min(numItems, L.maxVisible);

	// If there are more items than fit, a scrollbar is needed to the right of
	// the item area.  The scrollbar takes SCROLLBAR_WIDTH pixels plus a gap,
	// so the item area is narrowed accordingly.
	L.needsScrollbar = (numItems > L.maxVisible);
	L.menuRightX = contentRight
	               - (L.needsScrollbar ? RIP_LBMENU_SCROLLBAR_WIDTH + RIP_LBMENU_SCROLLBAR_GAP : 0);
	L.textX = L.menuLeftX + this.textXOffset;

	// Shrink the border to tightly fit the visible items (don't leave empty
	// space below the last item row), but don't exceed the original size.
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
		this._scrollbar.setScrollState(numItems, L.visibleCount, this.topItemIdx);
		this._scrollbar.computeLayout();
	}
	else
	{
		this._scrollbar = null;
	}
}


// Build RIP commands for the 3D border around the menu area, including the
// scrollbar.  The border uses a "chisel" or "Windows 3.1" style with two
// bevels: an outer raised bevel (bright top-left, dark bottom-right edges)
// and an inner sunken bevel (dark top-left, bright bottom-right edges),
// separated by a gap filled with the border surface color.  This creates a
// recessed panel effect for the menu content area.
// If useRIPBorder is false, only the content background is drawn (no border).
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
// Draws the item's background bar, text, and (in multi-select mode) a check
// mark indicator if the item is in the selectedItemIndexes set.  The isSelected
// parameter controls whether to use the highlighted (selectedBg/Fg) or normal
// (itemBg/Fg) colors — this is the lightbar highlight, independent of the
// multi-select toggle state.
function RIPLightbarMenu_buildItemRIP(index, isSelected)
{
	var L = this._layout;
	var item = this.GetItem(index);
	if (item === null)
		return "";

	var bgColor = isSelected ? this.colors.selectedBg : this.colors.itemBg;
	var fgColor = isSelected ? this.colors.selectedFg : this.colors.itemFg;
	var visPos = index - this.topItemIdx;
	var y = L.menuTopY + visPos * this.itemHeight;

	var rip = "";
	// Draw filled background rectangle for this item
	rip += RIPFillStyleNumeric(1, bgColor);
	rip += RIPBarNumeric(L.menuLeftX, y, L.menuRightX, y + this.itemHeight - 1);
	// Draw item text
	rip += RIPColorNumeric(fgColor);
	rip += RIPFontStyleNumeric(RIP_FONT_DEFAULT, 0, 1, 0);
	rip += RIPTextXYNumeric(L.textX, y + this.textYOffset, item.text);
	// Multi-select indicator
	if (this.multiSelect && this.selectedItemIndexes[index] === true)
	{
		// Draw a small check mark indicator at the left edge
		rip += RIPColorNumeric(isSelected ? this.colors.selectedFg : this.colors.itemFg);
		rip += RIPTextXYNumeric(L.menuLeftX + 1, y + this.textYOffset, "\xFB");
	}
	return rip;
}


// Build RIP mouse region commands for all visible items and scrollbar controls.
// Each visible item gets a mouse region that sends a unique high-byte character
// (RIP_LBMENU_MOUSE_ITEM_BASE + visible_position) when clicked.  The scrollbar
// mouse regions are delegated to the RIPScrollbar instance.  All existing mouse
// regions are killed first (RIPKillMouseFields) to ensure stale regions from a
// previous scroll position are removed.
function RIPLightbarMenu_buildMouseRegionsRIP()
{
	var L = this._layout;
	var numItems = this.NumItems();
	var rip = RIPKillMouseFields();

	// Mouse region for each visible menu item
	var dispCount = Math.min(L.visibleCount, numItems - this.topItemIdx);
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
	var numItems = this.NumItems();
	var rip = "";

	// Draw each visible item
	for (var i = this.topItemIdx; i < this.topItemIdx + L.visibleCount && i < numItems; ++i)
		rip += this._buildItemRIP(i, i === this.selectedItemIdx);

	// If fewer items than max visible, clear the remaining area
	if (numItems - this.topItemIdx < L.visibleCount)
	{
		var clearY = L.menuTopY + (numItems - this.topItemIdx) * this.itemHeight;
		rip += RIPFillStyleNumeric(1, this.colors.contentBg);
		rip += RIPBarNumeric(L.menuLeftX, clearY, L.menuRightX,
		                     L.menuTopY + L.visibleCount * this.itemHeight - 1);
	}

	// Update the scrollbar thumb position
	if (this._scrollbar !== null)
	{
		this._scrollbar.topIndex = this.topItemIdx;
		rip += this._scrollbar.buildThumbRIP();
	}

	// Rebuild mouse regions (item positions and thumb position may have changed)
	rip += this._buildMouseRegionsRIP();

	rip += RIPGotoXYNumeric(0, 0);
	this._sendRIP(rip);
}
