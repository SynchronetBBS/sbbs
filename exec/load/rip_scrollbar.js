// RIPScrollbar: A reusable scrollbar widget for RIP/RIPScrip terminals.
// Renders a vertical or horizontal scrollbar with arrow buttons, a track, and a
// draggable thumb. Provides RIP command strings for drawing and mouse regions.
// Does not handle input directly — the consumer is responsible for reading
// input and calling the appropriate methods to update the scroll state.
//
// Orientation:
//   By default the scrollbar is vertical (up/down arrows, thumb moves vertically).
//   Set the 'horizontal' property to true before calling computeLayout() to get
//   a horizontal scrollbar (left/right arrows, thumb moves horizontally).
//   In horizontal mode, the thumb drag uses $X$ instead of $Y$ for the mouse
//   coordinate, and getThumbGeometry() returns { x, w } instead of { y, h }.

"use strict";

require("rip_lib.js", "RIPWindow");

// Default mouse click identifiers (high-byte chars safe for CP437 terminals).
// These can be overridden via the mouseChars property to avoid conflicts
// when multiple scrollbars or other mouse-aware widgets coexist.
var RIP_SB_MOUSE_SCROLL_UP   = 0xFE;
var RIP_SB_MOUSE_SCROLL_DN   = 0xFD;
var RIP_SB_MOUSE_TRACK_PG_UP = 0xFC;
var RIP_SB_MOUSE_TRACK_PG_DN = 0xFB;
var RIP_SB_MOUSE_THUMB_DRAG  = 0xFA;


// RIPScrollbar constructor.
//
// Parameters:
//  pX: The X coordinate of the upper-left corner of the scrollbar (numeric, pixels)
//  pY: The Y coordinate of the upper-left corner of the scrollbar (numeric, pixels)
//  pWidth: The width of the scrollbar (numeric, pixels)
//  pHeight: The height of the scrollbar (numeric, pixels)
function RIPScrollbar(pX, pY, pWidth, pHeight)
{
	this.pos = {
		x: (typeof pX === "number" ? pX : 0),
		y: (typeof pY === "number" ? pY : 0)
	};

	this.size = {
		width: (typeof pWidth === "number" ? pWidth : 14),
		height: (typeof pHeight === "number" ? pHeight : 100)
	};

	this.colors = {
		track:      RIP_COLOR_DK_GRAY,
		thumb:      RIP_COLOR_LT_GRAY,
		arrowBg:    RIP_COLOR_LT_GRAY,
		arrowFg:    RIP_COLOR_BLACK,
		bevelBright: RIP_COLOR_WHITE,
		bevelDark:  RIP_COLOR_DK_GRAY
	};

	// Size of each arrow button along the scrollbar's primary axis (pixels).
	// For vertical mode this is the height of the up/down buttons; for
	// horizontal mode this is the width of the left/right buttons.
	this.arrowHeight = 14;

	// Orientation: false = vertical (default), true = horizontal.
	// Set this before calling computeLayout().
	this.horizontal = false;

	// Scroll state (set by the consumer before building RIP commands)
	this.totalItems = 0;
	this.visibleCount = 0;
	this.topIndex = 0;

	// Mouse click identifiers (configurable to avoid conflicts with other widgets).
	// In horizontal mode, scrollUp/scrollDn correspond to "scroll left" and
	// "scroll right" respectively, and trackPgUp/trackPgDn correspond to
	// "page left" and "page right".
	this.mouseChars = {
		scrollUp:  RIP_SB_MOUSE_SCROLL_UP,
		scrollDn:  RIP_SB_MOUSE_SCROLL_DN,
		trackPgUp: RIP_SB_MOUSE_TRACK_PG_UP,
		trackPgDn: RIP_SB_MOUSE_TRACK_PG_DN,
		thumbDrag: RIP_SB_MOUSE_THUMB_DRAG
	};

	// Internal computed layout (set by computeLayout)
	this._layout = null;

	// Public methods
	this.computeLayout = RIPScrollbar_computeLayout;
	this.setScrollState = RIPScrollbar_setScrollState;
	this.buildFullRIP = RIPScrollbar_buildFullRIP;
	this.buildThumbRIP = RIPScrollbar_buildThumbRIP;
	this.buildMouseRegionsRIP = RIPScrollbar_buildMouseRegionsRIP;
	this.getThumbGeometry = RIPScrollbar_getThumbGeometry;

	// Internal methods
	this._buildArrowRIP = RIPScrollbar_buildArrowRIP;
	this._buildHArrowRIP = RIPScrollbar_buildHArrowRIP;
}


// Compute the internal layout geometry from the current pos, size,
// arrowHeight, and orientation. Must be called before any build methods.
// Should also be called if the scrollbar's position, size, or orientation
// changes.
//
// For vertical mode, the layout contains:
//   left, right, top, bottom, arrowUpTop, arrowUpBot, arrowDnTop, arrowDnBot,
//   trackTop, trackBot, trackHeight
//
// For horizontal mode, the layout contains:
//   left, right, top, bottom, arrowLeftLeft, arrowLeftRight, arrowRightLeft,
//   arrowRightRight, trackLeft, trackRight, trackWidth
function RIPScrollbar_computeLayout()
{
	var L = {};

	L.left = this.pos.x;
	L.right = this.pos.x + this.size.width - 1;
	L.top = this.pos.y;
	L.bottom = this.pos.y + this.size.height - 1;

	if (this.horizontal)
	{
		// Horizontal: arrow buttons are at the left and right edges.
		// arrowHeight is used as the width of each arrow button.
		L.arrowLeftLeft = L.left;
		L.arrowLeftRight = L.left + this.arrowHeight - 1;
		L.arrowRightLeft = L.right - this.arrowHeight + 1;
		L.arrowRightRight = L.right;

		// Track runs horizontally between the two arrow buttons
		L.trackLeft = L.arrowLeftRight + 1;
		L.trackRight = L.arrowRightLeft - 1;
		L.trackWidth = L.trackRight - L.trackLeft + 1;
	}
	else
	{
		// Vertical (default): arrow buttons are at the top and bottom edges.
		L.arrowUpTop = L.top;
		L.arrowUpBot = L.top + this.arrowHeight - 1;
		L.arrowDnTop = L.bottom - this.arrowHeight + 1;
		L.arrowDnBot = L.bottom;

		// Track runs vertically between the two arrow buttons
		L.trackTop = L.arrowUpBot + 1;
		L.trackBot = L.arrowDnTop - 1;
		L.trackHeight = L.trackBot - L.trackTop + 1;
	}

	this._layout = L;
}


// Sets the scroll state of the scrollbar.
function RIPScrollbar_setScrollState(pTotalItems, pVisibleCount, pTopIndex)
{
	if (typeof(pTotalItems) === "number" && pTotalItems >= 0)
		this.totalItems = pTotalItems;
	if (typeof(pVisibleCount) === "number" && pVisibleCount >= 0)
		this.visibleCount = pVisibleCount;
	if (typeof(pTopIndex) === "number" && pTopIndex >= 0)
		this.topIndex = pTopIndex;
}


// Calculate the scrollbar thumb position and size based on the current
// totalItems, visibleCount, and topIndex.
//
// Return value:
//   Vertical mode: Object with { y: thumbTopPixel, h: thumbHeightPixels }
//   Horizontal mode: Object with { x: thumbLeftPixel, w: thumbWidthPixels }
function RIPScrollbar_getThumbGeometry()
{
	var L = this._layout;

	if (this.horizontal)
	{
		// Horizontal: thumb moves along the X axis within trackLeft..trackRight
		if (this.totalItems <= this.visibleCount || L.trackWidth <= 0)
			return { x: L.trackLeft, w: L.trackWidth };

		var maxTop = this.totalItems - this.visibleCount;
		var thumbW = Math.max(8, Math.floor(L.trackWidth * this.visibleCount / this.totalItems));
		var thumbRange = L.trackWidth - thumbW;
		var thumbX = L.trackLeft + (maxTop > 0 ? Math.floor(thumbRange * this.topIndex / maxTop) : 0);
		return { x: thumbX, w: thumbW };
	}
	else
	{
		// Vertical: thumb moves along the Y axis within trackTop..trackBot
		if (this.totalItems <= this.visibleCount || L.trackHeight <= 0)
			return { y: L.trackTop, h: L.trackHeight };

		var maxTop = this.totalItems - this.visibleCount;
		var thumbH = Math.max(8, Math.floor(L.trackHeight * this.visibleCount / this.totalItems));
		var thumbRange = L.trackHeight - thumbH;
		var thumbY = L.trackTop + (maxTop > 0 ? Math.floor(thumbRange * this.topIndex / maxTop) : 0);
		return { y: thumbY, h: thumbH };
	}
}


// Build RIP commands for a small filled triangle arrow pointing up or down
// (used in vertical mode).
//
// Parameters:
//  centerX: X center of the arrow button
//  topY: Top Y coordinate of the arrow button area
//  pointsUp: true for an upward-pointing arrow, false for downward
function RIPScrollbar_buildArrowRIP(centerX, topY, pointsUp)
{
	var rip = "";
	rip += RIPColorNumeric(this.colors.arrowFg);
	rip += RIPFillStyleNumeric(1, this.colors.arrowFg);
	var triH = 5;
	var triW = 8;
	var baseY = pointsUp ? (topY + 3 + triH) : (topY + 3);
	var tipY = pointsUp ? (topY + 3) : (topY + 3 + triH);
	var pts = [
		{ x: centerX - Math.floor(triW / 2), y: baseY },
		{ x: centerX + Math.floor(triW / 2), y: baseY },
		{ x: centerX, y: tipY }
	];
	rip += RIPFillPolygonNumeric(pts);
	return rip;
}


// Build RIP commands for a small filled triangle arrow pointing left or right
// (used in horizontal mode).
//
// Parameters:
//  centerY: Y center of the arrow button
//  leftX: Left X coordinate of the arrow button area
//  pointsLeft: true for a left-pointing arrow, false for right
function RIPScrollbar_buildHArrowRIP(centerY, leftX, pointsLeft)
{
	var rip = "";
	rip += RIPColorNumeric(this.colors.arrowFg);
	rip += RIPFillStyleNumeric(1, this.colors.arrowFg);
	var triW = 5;  // Width of the triangle along the X axis
	var triH = 8;  // Height of the triangle along the Y axis
	var baseX = pointsLeft ? (leftX + 3 + triW) : (leftX + 3);
	var tipX = pointsLeft ? (leftX + 3) : (leftX + 3 + triW);
	var pts = [
		{ x: baseX, y: centerY - Math.floor(triH / 2) },
		{ x: baseX, y: centerY + Math.floor(triH / 2) },
		{ x: tipX, y: centerY }
	];
	rip += RIPFillPolygonNumeric(pts);
	return rip;
}


// Build RIP commands for the complete scrollbar (arrow buttons + track + thumb).
//
// Return value: RIP command string
function RIPScrollbar_buildFullRIP()
{
	var L = this._layout;
	var rip = "";

	if (this.horizontal)
	{
		var centerY = Math.floor((L.top + L.bottom) / 2);

		// Left arrow button (raised bevel)
		rip += RIPFillStyleNumeric(1, this.colors.arrowBg);
		rip += RIPBarNumeric(L.arrowLeftLeft, L.top, L.arrowLeftRight, L.bottom);
		rip += RIPFillStyleNumeric(1, this.colors.bevelBright);
		rip += RIPBarNumeric(L.arrowLeftLeft, L.top, L.arrowLeftRight, L.top);
		rip += RIPBarNumeric(L.arrowLeftLeft, L.top, L.arrowLeftLeft, L.bottom);
		rip += RIPFillStyleNumeric(1, this.colors.bevelDark);
		rip += RIPBarNumeric(L.arrowLeftLeft, L.bottom, L.arrowLeftRight, L.bottom);
		rip += RIPBarNumeric(L.arrowLeftRight, L.top, L.arrowLeftRight, L.bottom);
		rip += this._buildHArrowRIP(centerY, L.arrowLeftLeft, true);

		// Right arrow button (raised bevel)
		rip += RIPFillStyleNumeric(1, this.colors.arrowBg);
		rip += RIPBarNumeric(L.arrowRightLeft, L.top, L.arrowRightRight, L.bottom);
		rip += RIPFillStyleNumeric(1, this.colors.bevelBright);
		rip += RIPBarNumeric(L.arrowRightLeft, L.top, L.arrowRightRight, L.top);
		rip += RIPBarNumeric(L.arrowRightLeft, L.top, L.arrowRightLeft, L.bottom);
		rip += RIPFillStyleNumeric(1, this.colors.bevelDark);
		rip += RIPBarNumeric(L.arrowRightLeft, L.bottom, L.arrowRightRight, L.bottom);
		rip += RIPBarNumeric(L.arrowRightRight, L.top, L.arrowRightRight, L.bottom);
		rip += this._buildHArrowRIP(centerY, L.arrowRightLeft, false);

		// Track background
		rip += RIPFillStyleNumeric(1, this.colors.track);
		rip += RIPBarNumeric(L.trackLeft, L.top, L.trackRight, L.bottom);
	}
	else
	{
		var centerX = Math.floor((L.left + L.right) / 2);

		// Up arrow button (raised bevel)
		rip += RIPFillStyleNumeric(1, this.colors.arrowBg);
		rip += RIPBarNumeric(L.left, L.arrowUpTop, L.right, L.arrowUpBot);
		rip += RIPFillStyleNumeric(1, this.colors.bevelBright);
		rip += RIPBarNumeric(L.left, L.arrowUpTop, L.right, L.arrowUpTop);
		rip += RIPBarNumeric(L.left, L.arrowUpTop, L.left, L.arrowUpBot);
		rip += RIPFillStyleNumeric(1, this.colors.bevelDark);
		rip += RIPBarNumeric(L.left, L.arrowUpBot, L.right, L.arrowUpBot);
		rip += RIPBarNumeric(L.right, L.arrowUpTop, L.right, L.arrowUpBot);
		rip += this._buildArrowRIP(centerX, L.arrowUpTop, true);

		// Down arrow button (raised bevel)
		rip += RIPFillStyleNumeric(1, this.colors.arrowBg);
		rip += RIPBarNumeric(L.left, L.arrowDnTop, L.right, L.arrowDnBot);
		rip += RIPFillStyleNumeric(1, this.colors.bevelBright);
		rip += RIPBarNumeric(L.left, L.arrowDnTop, L.right, L.arrowDnTop);
		rip += RIPBarNumeric(L.left, L.arrowDnTop, L.left, L.arrowDnBot);
		rip += RIPFillStyleNumeric(1, this.colors.bevelDark);
		rip += RIPBarNumeric(L.left, L.arrowDnBot, L.right, L.arrowDnBot);
		rip += RIPBarNumeric(L.right, L.arrowDnTop, L.right, L.arrowDnBot);
		rip += this._buildArrowRIP(centerX, L.arrowDnTop, false);

		// Track background
		rip += RIPFillStyleNumeric(1, this.colors.track);
		rip += RIPBarNumeric(L.left, L.trackTop, L.right, L.trackBot);
	}

	// Thumb (works for both orientations via buildThumbRIP)
	rip += this.buildThumbRIP();

	return rip;
}


// Build RIP commands for just the scrollbar track and thumb.
// Used for efficient updates when only the scroll position changes.
//
// Return value: RIP command string
function RIPScrollbar_buildThumbRIP()
{
	var L = this._layout;

	if (this.horizontal)
	{
		if (L.trackWidth <= 0)
			return "";

		var thumb = this.getThumbGeometry();
		var rip = "";
		// Clear track
		rip += RIPFillStyleNumeric(1, this.colors.track);
		rip += RIPBarNumeric(L.trackLeft, L.top, L.trackRight, L.bottom);
		// Draw thumb (raised look)
		rip += RIPFillStyleNumeric(1, this.colors.thumb);
		rip += RIPBarNumeric(thumb.x, L.top, thumb.x + thumb.w - 1, L.bottom);
		// Thumb bevel: bright top/left, dark bottom/right
		rip += RIPFillStyleNumeric(1, this.colors.bevelBright);
		rip += RIPBarNumeric(thumb.x, L.top, thumb.x + thumb.w - 1, L.top);
		rip += RIPBarNumeric(thumb.x, L.top, thumb.x, L.bottom);
		rip += RIPFillStyleNumeric(1, this.colors.bevelDark);
		rip += RIPBarNumeric(thumb.x, L.bottom, thumb.x + thumb.w - 1, L.bottom);
		rip += RIPBarNumeric(thumb.x + thumb.w - 1, L.top, thumb.x + thumb.w - 1, L.bottom);
		return rip;
	}
	else
	{
		if (L.trackHeight <= 0)
			return "";

		var thumb = this.getThumbGeometry();
		var rip = "";
		// Clear track
		rip += RIPFillStyleNumeric(1, this.colors.track);
		rip += RIPBarNumeric(L.left, L.trackTop, L.right, L.trackBot);
		// Draw thumb (raised look)
		rip += RIPFillStyleNumeric(1, this.colors.thumb);
		rip += RIPBarNumeric(L.left, thumb.y, L.right, thumb.y + thumb.h - 1);
		// Thumb bevel: bright top/left, dark bottom/right
		rip += RIPFillStyleNumeric(1, this.colors.bevelBright);
		rip += RIPBarNumeric(L.left, thumb.y, L.right, thumb.y);
		rip += RIPBarNumeric(L.left, thumb.y, L.left, thumb.y + thumb.h - 1);
		rip += RIPFillStyleNumeric(1, this.colors.bevelDark);
		rip += RIPBarNumeric(L.left, thumb.y + thumb.h - 1, L.right, thumb.y + thumb.h - 1);
		rip += RIPBarNumeric(L.right, thumb.y, L.right, thumb.y + thumb.h - 1);
		return rip;
	}
}


// Build RIP mouse region commands for the scrollbar controls.
//
// For vertical mode: up/down arrow buttons, track areas above/below the thumb
// (page up/down), and the thumb itself (drag-to-scroll via $Y$).
//
// For horizontal mode: left/right arrow buttons, track areas left/right of the
// thumb (page left/right), and the thumb itself (drag-to-scroll via $X$).
//
// Note: This does NOT call RIPKillMouseFields(). The consumer is responsible
// for killing existing mouse fields before adding new ones, typically by
// batching this output with other mouse region commands.
//
// Return value: RIP command string
function RIPScrollbar_buildMouseRegionsRIP()
{
	var L = this._layout;
	var rip = "";

	if (this.horizontal)
	{
		// Left/right arrow buttons
		rip += RIPMouseNumeric(0, L.arrowLeftLeft, L.top, L.arrowLeftRight, L.bottom,
		                       true, false, 0, String.fromCharCode(this.mouseChars.scrollUp));
		rip += RIPMouseNumeric(0, L.arrowRightLeft, L.top, L.arrowRightRight, L.bottom,
		                       true, false, 0, String.fromCharCode(this.mouseChars.scrollDn));

		// Track and thumb regions (only if there's a track area)
		if (L.trackWidth > 0)
		{
			var thumb = this.getThumbGeometry();

			// Track area left of thumb (page left / page up equivalent)
			if (thumb.x > L.trackLeft)
				rip += RIPMouseNumeric(0, L.trackLeft, L.top, thumb.x - 1, L.bottom,
				                       false, false, 0,
				                       String.fromCharCode(this.mouseChars.trackPgUp));

			// Thumb itself: on release, sends prefix char + $X$ (X coordinate).
			// SyncTerm expands $X$ to the mouse X position at release time,
			// enabling drag-to-scroll along the horizontal axis.
			rip += RIPMouseNumeric(0, thumb.x, L.top, thumb.x + thumb.w - 1, L.bottom,
			                       false, false, 0,
			                       String.fromCharCode(this.mouseChars.thumbDrag) + "$X$");

			// Track area right of thumb (page right / page down equivalent)
			if (thumb.x + thumb.w <= L.trackRight)
				rip += RIPMouseNumeric(0, thumb.x + thumb.w, L.top, L.trackRight, L.bottom,
				                       false, false, 0,
				                       String.fromCharCode(this.mouseChars.trackPgDn));
		}
	}
	else
	{
		// Up/down arrow buttons
		rip += RIPMouseNumeric(0, L.left, L.arrowUpTop, L.right, L.arrowUpBot,
		                       true, false, 0, String.fromCharCode(this.mouseChars.scrollUp));
		rip += RIPMouseNumeric(0, L.left, L.arrowDnTop, L.right, L.arrowDnBot,
		                       true, false, 0, String.fromCharCode(this.mouseChars.scrollDn));

		// Track and thumb regions (only if there's a track area)
		if (L.trackHeight > 0)
		{
			var thumb = this.getThumbGeometry();

			// Track area above thumb (page up)
			if (thumb.y > L.trackTop)
				rip += RIPMouseNumeric(0, L.left, L.trackTop, L.right, thumb.y - 1,
				                       false, false, 0,
				                       String.fromCharCode(this.mouseChars.trackPgUp));

			// Thumb itself: on release, sends prefix char + $Y$ (Y coordinate).
			// SyncTerm expands $Y$ to the mouse Y position at release time,
			// enabling drag-to-scroll along the vertical axis.
			rip += RIPMouseNumeric(0, L.left, thumb.y, L.right, thumb.y + thumb.h - 1,
			                       false, false, 0,
			                       String.fromCharCode(this.mouseChars.thumbDrag) + "$Y$");

			// Track area below thumb (page down)
			if (thumb.y + thumb.h <= L.trackBot)
				rip += RIPMouseNumeric(0, L.left, thumb.y + thumb.h, L.right, L.trackBot,
				                       false, false, 0,
				                       String.fromCharCode(this.mouseChars.trackPgDn));
		}
	}

	return rip;
}
