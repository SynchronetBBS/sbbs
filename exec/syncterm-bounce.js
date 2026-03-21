// syncterm-bounce.js — Bouncing logo animation using SyncTERM pixel graphics
//
// This script demonstrates how to use SyncTERM's pixel-level APC (Application
// Program Command) extensions to animate a sprite over the existing screen
// content.  It implements a classic "bouncing DVD logo" effect: a 64x64 pixel
// SyncTERM logo moves diagonally across the screen, reversing direction when
// it hits an edge.  The original screen content is preserved underneath and
// restored when the animation ends.
//
// Key concepts demonstrated:
//   1. Feature detection  — querying CTerm capabilities before using them
//   2. File caching       — uploading images once and reusing via MD5 check
//   3. Pixel buffers      — Copy/Paste screen regions for flicker-free restore
//   4. Mask buffers       — PBM transparency masks for non-rectangular sprites
//   5. Animation loop     — frame timing, movement, and edge-bounce logic
//
// All "APC" sequences have the form:  ESC _ <payload> ESC \
//   ESC _ = APC introducer (0x1B 0x5F)
//   ESC \ = ST  terminator  (0x1B 0x5C)
//
// All "CSI" sequences have the form:  ESC [ <params> <final-byte>
//   ESC [ = CSI introducer (0x1B 0x5B)

// Load Synchronet user flag constants (provides USER_ANSI among others).
require("userdefs.js", "USER_ANSI");

// Shorthand: write a raw string to the terminal with no added CRLF or
// attribute processing.  console.write() sends bytes verbatim, which is
// essential for escape sequences that would be mangled by console.print().
function w(str)
{
	console.write(str);
}

// ---------------------------------------------------------------------------
// read_apc() — Read an APC response string from the terminal.
//
// SyncTERM replies to certain APC queries with:  APC <payload> ST
// where APC = ESC _ (0x1B 0x5F) and ST = ESC \ (0x1B 0x5C).
//
// This function implements a simple state machine that:
//   state 0: waits for ESC
//   state 1: expects '_' (0x5F = 95) to confirm APC start
//   state 2: accumulates payload bytes until ESC is seen
//   state 3: expects '\' (0x5C = 92) to confirm ST end
//
// Returns the payload string on success, or undefined on timeout/error.
// ---------------------------------------------------------------------------
function read_apc()
{
	var ret = '';
	var ch;
	var state = 0;

	for(;;) {
		ch = console.getbyte(1000);
		if (ch === null)
			return undefined;
		switch(state) {
			case 0:
				if (ch == 0x1b) {
					state++;
					break;
				}
				break;
			case 1:
				if (ch == 95) {
					state++;
					break;
				}
				state = 0;
				break;
			case 2:
				if (ch == 0x1b) {
					state++;
					break;
				}
				ret += ascii(ch);
				break;
			case 3:
				if (ch == 92) {
					return ret;
				}
				return undefined;
		}
	}
	return undefined;
}

// ---------------------------------------------------------------------------
// read_dim() — Read the screen's pixel dimensions from an XTSRGA response.
//
// After sending CSI ? 2 ; 1 S (XTerm Set or Request Graphics Attribute),
// SyncTERM replies with:  CSI ? 2 ; 0 ; <width> ; <height> S
//
// This function parses that response byte-by-byte:
//   states 0-1: match ESC [
//   state  2:   match '?' (0x3F = 63)
//   state  3:   match '2' (0x32 = 50)
//   state  4:   match ';' (0x3B = 59)
//   state  5:   match '0' (0x30 = 48)   — status=success
//   state  6:   match ';'
//   state  7:   accumulate width digits until ';'
//   state  8:   accumulate height digits until 'S' (0x53 = 83) terminates
//
// Returns {width, height} in pixels.
// ---------------------------------------------------------------------------
function read_dim()
{
	var ret = {width:0, height:0};
	var ch;
	var state = 0;

	for(;;) {
		ch = console.getbyte();
		switch(state) {
			case 0:
				if (ch == 0x1b) {
					state++;
					break;
				}
				break;
			case 1:
				if (ch == 91) {
					state++;
					break;
				}
				state = 0;
				break;
			case 2:
				if (ch == 63) {
					state++;
					break;
				}
				state = 0;
				break;
			case 3:
				if (ch == 50) {
					state++;
					break;
				}
				state = 0;
				break;
			case 4:
				if (ch == 59) {
					state++;
					break;
				}
				state = 0;
				break;
			case 5:
				if (ch == 48) {
					state++;
					break;
				}
				state = 0;
				break;
			case 6:
				if (ch == 59) {
					state++;
					break;
				}
				state = 0;
				break;
			case 7:
				if (ch >= ascii('0') && ch <= ascii('9')) {
					ret.width = ret.width * 10 + (ch - ascii('0'));
					break;
				}
				else if(ch == 59) {
					state++;
					break;
				}
				state = 0;
				break;
			case 8:
				if (ch >= ascii('0') && ch <= ascii('9')) {
					ret.height = ret.height * 10 + (ch - ascii('0'));
					break;
				}
				else if(ch == 83) {
					return ret;
				}
				state = 0;
				break;
		}
	}
	return undefined;
}

// ---------------------------------------------------------------------------
// pixel_capability() — Check whether the terminal supports pixel operations.
//
// Sends CSI < 0 c (CTerm Device Attributes request) and parses the reply:
//   CSI < 0 ; Ps1 ; Ps2 ; ... c
//
// Each Ps value indicates a supported feature:
//   1 = loadable fonts via DCS
//   2 = bright background (DECSET 32)
//   3 = palette modification via OSC
//   4 = pixel operations (sixel/PPM graphics)
//   5 = font selection via CSI Ps1;Ps2 sp D
// This function checks for Ps=3 (palette modification) as a proxy for
// pixel support — the capability list is cumulative, so if 3 is present
// then 4 (pixel ops) is also guaranteed to be present.
//
// State machine:
//   states 0-1: match ESC [
//   state  2:   match '<' (0x3C = 60)
//   state  3:   match '0' — confirms this is a type-0 response
//   state  4:   match ';'
//   state  5:   accumulate each Ps value; on ';' or 'c' (0x63 = 99),
//               check if the value was 3 and set ret=true if so
//
// Returns true if pixel graphics are available, false otherwise.
// ---------------------------------------------------------------------------
function pixel_capability()
{
	var ret = false;
	var ch;
	var state = 0;
	var optval = 0;

	for(;;) {
		ch = console.getbyte(1000);
		if (ch == null)
			break;
		switch(state) {
			case 0:
				if (ch == 0x1b) { // ESC
					state++;
					break;
				}
				break;
			case 1:
				if (ch == 91) {   // [
					state++;
					break;
				}
				state = 0;
				break;
			case 2:
				if (ch == 60) {   // <
					state++;
					break;
				}
				state = 0;
				break;
			case 3:
				if (ch == 48) {   // 0
					state++;
					break;
				}
				state = 0;
				break;
			case 4:
				if (ch == 59) {   // ;
					state++;
					break;
				}
				state = 0;
				break;
			case 5:
				if (ch >= ascii('0') && ch <= ascii('9')) {
					optval = optval * 10 + (ch - ascii('0'));
					break;
				}
				else if(ch == 59) {
					if (optval === 3)
						ret = true;
					optval = 0;
					break;
				}
				else if (ch === 99) { // c
					if (optval === 3)
						ret = true;
					return ret;
				}
				state = 0;
				break;
		}
	}
	return ret;
}

// ===========================================================================
// Main program
// ===========================================================================

// Bail out immediately if the terminal doesn't support ANSI sequences at all.
if (!console.term_supports(USER_ANSI))
	exit(0);

// console.cterm_version encodes major*1000 + minor.  Version 1.316+ is
// required for the APC-based pixel operations used below.  Older terminals
// just get a pause prompt and exit.
if (console.cterm_version < 1316) {
	console.pause();
}
else {
	// Save and modify ctrlkey_passthru so that ESC (Ctrl-[) keypresses
	// are passed through to console.getbyte() instead of being intercepted
	// by the BBS input handler.  "+[" adds Ctrl-[ (ESC) to the passthru set.
	var opt = console.ctrlkey_passthru;
	console.ctrlkey_passthru = "+[";

	// --- Step 1: Feature detection ---
	// Send CSI < c (CTerm Device Attributes) to check for pixel support.
	// pixel_capability() parses the response and returns true if Ps=3 is
	// present, indicating pixel/sixel/PPM operations are available.
	w('\x1b[<c');
	if (!pixel_capability()) {
		console.pause()
		console.ctrlkey_passthru = opt;
	}
	else {

		// --- Step 2: Upload sprite images to the per-connection cache ---
		// SyncTERM maintains a file cache per connection.  Files persist for
		// the session, so we first list what's cached and check MD5 sums to
		// avoid re-uploading unchanged files.
		//
		// APC SyncTERM:C;L;SyncTERM.p?m ST — list cached files matching
		// the glob "SyncTERM.p?m" (matches both .ppm and .pbm).
		// SyncTERM responds with an APC containing lines of:
		//   <filename> TAB <md5sum> LF
		w('\x1b_SyncTERM:C;L;SyncTERM.p?m\x1b\\');
		var lst = read_apc();

		// --- Upload the PPM sprite (color image) if not already cached ---
		// SyncTERM.ppm is a 64x64 raw PPM (Portable PixMap) image of the
		// SyncTERM logo, base64-encoded in the APC Store command.
		// Only upload if the cached MD5 doesn't match the expected hash.
		//
		// APC SyncTERM:C;S;<filename>;<base64-data> ST — store a file in
		// the cache.  The "UDYKNjQgNjQKMjU1C..." payload is the base64
		// encoding of a raw PPM file (magic "P6\n", 64x64, max 255).
		var m = lst.match(/\nSyncTERM.ppm\t([0-9a-f]+)\n/);
		if (m == null || m[1] !== '69de4f5fe394c1a8221927da0bfe9845') {
			w('\x1b_SyncTERM:C;S;SyncTERM.ppm;UDYKNjQgNjQKMjU1C'+
			    'v///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////3h4eHBwcGhoaGJiYl1dXVpaWldXV1ZWVlZWVldXV1paWl1dXWJiYmhoaHBwcHh4eP///////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////3JycmJiYlVVVUpKSkFBQTo6OjMzMy0tLSgoKCQkJCIiIh8fHx4eHh4eHh4eHh4eHh8fHyIiIiQkJCgoKC0tLTMzMzo6OkFBQUpKSlVVVWJiYnJycv///////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////29vb1paWklJSTw8PC8vLyUlJR4eHh4eHh8fHyAgICEhISIiIiEhIRsbGx4eHiMjIyMjIyMjIyMjIyMjIyMjIx4eHhsbGyEhISIiIiEhISAgIB8fHx4eHh4eHiUlJS8vLzs7O0lJSVpaWm9vb////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////2pqalJSUj4+Pi0tLSIiIh4eHiAgICIiIhsbGyMjIyMjIxYWFg0NDQ0NDQ0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDQ0NDRYWFhYWFiMjIxsbGyIiIiAgIB4eHiIiIi0tLT4+PlFRUWpqav///////////////////////////////////////////'+
			    '////////////////////////////////3V1dVdXVz8/PywsLB4eHh8fHyIiIhsbGyMjIxYWFg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDRYWFiMjIx4eHiIiIh8fHx4eHiwsLD8/P1dXV3V1df///////////////////////////////'+
			    '////////////////////////21tbU1NTTMzMyIiIh8fHyIiIiMjIxYWFg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADAAAegAAdAAAAAAAAAAAagAABgAAAAAAAA0NDRYWFiMjIyIiIh8fHyEhITMzM0xMTG1tbf///////////////////////'+
			    '////////////////3BwcEtLSy8vLx4eHiEhIRsbGxYWFg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABwAACgAAAAAAEAAAFgAAKQAADwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAfwAARwAAMgAAAAAAAAAALQAAAgAAAAYGBggICAwMDAcHBxscGxYWFhsbGyEhIR4eHi8vL0tLS3BwcP///////////////'+
			    '////////319fVNTUzIyMh4eHiEhIR4eHhYWFgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAICAgMDAwAAAAAAAAAAAAAAOwAAVAAAAwAAjAAAkwAAOwAAIQAADgAAAwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAlAAAUQAAAwAAAAAAAAAAAAAAAAAAAAAAABUVFRscGxcXFxcXFxEREQAAAAAAAA0NDR4eHiEhIR4eHjIyMlNTU319ff///////'+
			    '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'+
			    'Do6OigoKB4eHiEhIRYWFgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABgYMzh0paWGFxcAAAAAAAAAAAACgUF9QEBMgYG+QEBUgUF1QEBMAAADAYG+QEBRQMDggUF3QAAGwAADgYG+QEBSAEBSQYG+QAAHwICdgYG+QICXQICWwYG+QICdQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABYWFiEhIR8fHygoKDo6O'+
			    'v///2FhYT09PSMjIyEhIR4eHg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABDQ218V83NCUBAAAAAAAAAAAAABwQEqwAAAAYG+QAAJwMDkgAAIQAAAAYG+QYG+QYG+QAAJwAAAAAAAAYG+QYG+QYG+QMDpQAAAAICagQEwwQEsgQErQQEwwICaQAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDR4eHiEhISMjIz09PWFhYf///'+
			    '////////319fVNTUzIyMh4eHiEhIR4eHg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAK+vAD09AAAAAAAAAAAAAAAAAAAAAAAAAAYG+QAAJwAAAAAAAAAAAAYG+QEBRQQExQMDiAAACQAAAAYG+QEBUgUF7AMDjgAAAAICdAMDmwQExgQEwgMDnAICcwAAAAAAAAAAAAAAAAAAAA0NDSMjIyIiIh4eHjIyMlJSUn19ff///////'+
			    '////////////////29vb0tLSy8vLx4eHiEhIRsbGxYWFgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIeHAAEBAAAAAAAAAAAAAAAAAAAACgEBMgYG+QEBUgAAEgAAAAAADQYG+QEBSAAAJwYG+QAAJwAAEQYG+QEBOgICewUF6wAAHgMDjAQEpgEBRAEBQwQEpgMDiwAAAAAAAAAAABYWFhsbGyEhIR4eHi8vL0pKSm9vb////////////////'+
			    '////////////////////////21tbUxMTDMzMyIiIh8fHyIiIiMjIxYWFgAAAAAAAAAAAAAAAAAAAAAAAFxcAAAAAAAAAAAAAAAAAAAAAAICXAYG+QYG+QYG+QMDggAAAAMDgQYG+QYG+QYG+QYG+QAAIQMDhQYG+QQEvQAADAUF4gQEugUF6QUF+AAAJQAAJgYG+QUF6hYWFiMjIyIiIh8fHyEhITMzM0xMTG1tbf///////////////////////'+
			    '////////////////////////////////3V1dVdXVz4+PisrKx4eHh8fHyIiIh4eHhYWFg0NDQ0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDQ0NDRYWFh4eHiIiIh8fHx4eHisrKz4+PldXV3V1df///////////////////////////////'+
			    '////////////////////////////////////////////2pqalJSUj4+Pi0tLSEhIR4eHiAgICIiIhsbGyMjIxYWFg0NDQ0NDQ0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDQ0NDRYWFiMjIxsbGyIiIiAgIB4eHiEhIS0tLT4+PlFRUWpqav///////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////29vb1paWklJSTs7Oy4uLiUlJR4eHh4eHh8fHyAgICIiIiIiIiEhIRsbGx4eHiMjIyMjIyMjIyMjIyMjIyMjIx4eHhsbGyEhISIiIiIiIiAgIB8fHx4eHh4eHiUlJS4uLjs7O0lJSVlZWW5ubv///////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////3JycmJiYlVVVUpKSkFBQTo6OjIyMi0tLSgoKCQkJCEhIR8fHx4eHh0dHR0dHR4eHh8fHyEhISQkJCgoKC0tLTIyMjk5OUFBQUpKSlVVVWJiYnJycv///////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////3h4eG9vb2hoaGJiYl1dXVpaWldXV1ZWVlZWVldXV1paWl1dXWJiYmhoaG9vb3h4eP///////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    'w==\x1b\\');
		}
		// Load the cached PPM into pixel buffer 1.
		// B=1 selects buffer 1 (there are two buffers, 0 and 1).
		// Buffer 1 will hold the sprite image for drawing.
		w('\x1b_SyncTERM:C;LoadPPM;B=1;SyncTERM.ppm\x1b\\');

		// --- Upload the PBM transparency mask if not already cached ---
		// SyncTERM.pbm is a 72x136 PBM (Portable BitMap) used as a mask.
		// In PBM, 1-bits = draw from source, 0-bits = leave screen alone.
		// It contains TWO 72x68 frames stacked vertically:
		//
		//   Top half (rows 0-67) — the DRAW mask:
		//     A filled circle inscribed in cols 0-63, rows 0-63, with the
		//     surrounding area clear (transparent).  Only the first 64
		//     columns matter here — the Paste command clips the mask to
		//     the source buffer dimensions (64x64).
		//
		//   Bottom half (rows 68-135) — the ERASE mask:
		//     The INVERSE of the circle, centered at cols 4-67 within a
		//     72x68 rectangle.  The 4px solid border on each side plus the
		//     circular hole in the center means: border pixels are restored
		//     from background (erasing the trail), but pixels inside the
		//     circle are left untouched (preserving the freshly-drawn sprite).
		//
		// The 72-pixel width (vs. the 64-pixel sprite) provides a 4-pixel
		// border on each side, matching the movement step size so the erase
		// pass always covers the sprite's previous position completely.
		m = lst.match(/\nSyncTERM.pbm\t([0-9a-f]+)\n/);
		if (m == null || m[1] !== '9b8a444559d6982566f87caf575a5fe2') {
			w('\x1b_SyncTERM:C;S;SyncTERM.pbm;UDQKNzIgMTM2C'+
			    'gAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAP//AAAA/'+
			    'wAAP////AAA/'+
			    'wAD/////8AA/'+
			    'wAf//////gA/'+
			    'wD///////8A/'+
			    'wP////////A/'+
			    'w/////////w/'+
			    'z/////////8/'+
			    '3/////////+/'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '3/////////+/'+
			    'z/////////8/'+
			    'w/////////w/'+
			    'wP////////A/'+
			    'wD///////8A/'+
			    'wAf//////gA/'+
			    'wAD/////8AA/'+
			    'wAAP////AAA/'+
			    'wAAAP//AAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '/////AAD////'+
			    '////AAAAD///'+
			    '///wAAAAAP//'+
			    '//+AAAAAAB//'+
			    '//wAAAAAAAP/'+
			    '//AAAAAAAAD/'+
			    '/8AAAAAAAAA/'+
			    '/wAAAAAAAAAP'+
			    '/gAAAAAAAAAH'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/gAAAAAAAAAH'+
			    '/wAAAAAAAAAP'+
			    '/8AAAAAAAAA/'+
			    '//AAAAAAAAD/'+
			    '//wAAAAAAAP/'+
			    '//+AAAAAAB//'+
			    '///wAAAAAP//'+
			    '////AAAAD///'+
			    '/////AAD////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    'w==\x1b\\');
		}
		// Load the PBM mask into the mask buffer.  Unlike LoadPPM which
		// takes a B= buffer selector, LoadPBM loads into a single shared
		// mask buffer referenced by the MBUF keyword in draw/paste commands.
		w('\x1b_SyncTERM:C;LoadPBM;SyncTERM.pbm\x1b\\');

		// --- Step 3: Query the screen pixel dimensions ---
		// CSI ? 2 ; 1 S is XTSRGA (XTerm Set or Request Graphics Attribute).
		// Ps1=2 selects "number of pixels", Ps2=1 requests the value.
		// Response: CSI ? 2 ; 0 ; <width> ; <height> S
		w('\x1b[?2;1S');
		var dim = read_dim()

		// --- Step 4: Save the current screen into pixel buffer 0 ---
		// APC SyncTERM:P;Copy;B=0 ST copies the entire screen (all pixels)
		// into buffer 0.  This snapshot is used during animation to restore
		// the background behind the sprite each frame.
		w('\x1b_SyncTERM:P;Copy;B=0\x1b\\');

		// --- Step 5: Initialize sprite position and velocity ---
		// The sprite is 64x64 pixels.  We keep a 4-pixel margin from each
		// screen edge (matching the movement step size) so the erase rect
		// never goes off-screen.
		var imgdim = {width:64, height:64};
		var pos = {x:random(dim.width - imgdim.width - 8) + 4, y:random(dim.height - imgdim.height - 8) + 4};
		var remain;
		// Movement: 4 pixels/frame horizontally, 2 pixels/frame vertically.
		// Initial direction is random (positive or negative).
		var dir = {x:4 * (random(1) ? -1 : 1), y:2 * (random(1) ? -1 : 1)};

		// --- Step 6: Animation loop ---
		// Runs until the user presses any key.  console.inkey(1) returns ''
		// if no key is available within 1ms.
		while (console.inkey(1) == '') {
			mswait(10);
			pos.x += dir.x;
			pos.y += dir.y;

			// Bounce off left edge
			if (pos.x - 4 < 0) {
				dir.x = 4;
				pos.x += 4;
			}
			// Bounce off right edge (sprite width + 8 for the 4px border
			// on each side of the mask)
			if (pos.x + 4 + imgdim.width + 8 >= dim.width) {
				dir.x = -4;
				pos.x -= 4;
			}
			// Bounce off top edge
			if (pos.y - 4 < 0) {
				dir.y = 2;
				pos.y += 2;
			}
			// Bounce off bottom edge
			if (pos.y + 4 + imgdim.height + 8 >= dim.height) {
				dir.y = -2;
				pos.y -= 2;
			}

			// --- Draw the sprite at its new position ---
			// Paste from buffer 1 (the PPM sprite) to the screen at (pos.x, pos.y).
			// MBUF applies the loaded PBM mask buffer so only pixels inside
			// the circular mask shape are drawn — the rest is transparent.
			// B=1 selects buffer 1 (the sprite image).
			w('\x1b_SyncTERM:P;Paste;DX='+pos.x+';DY='+pos.y+';MBUF;B=1\x1b\\');

			// --- Erase the sprite's trail (restore background) ---
			// Paste from buffer 0 (the saved screen background) to restore
			// the area the sprite just vacated.  The source and destination
			// rectangles are identical — we're copying from the background
			// snapshot back to the same screen coordinates.
			//
			// The region is (imgdim.width + 8) x (imgdim.height + 8) pixels,
			// starting 4 pixels before the sprite position in each axis.
			// This oversized region ensures the trail from the previous
			// frame (which was up to 4px away) is fully erased.
			//
			// MY=64 offsets into the BOTTOM half of the PBM mask (row 64+),
			// which is the "erase" frame — a rectangle with a circular HOLE
			// matching the sprite shape.  This means:
			//   - Border pixels (the 4px margin around the sprite) are
			//     restored from the background, erasing the old trail.
			//   - Pixels inside the circle are LEFT UNTOUCHED (0-bits in
			//     mask), so the sprite we just drew above is preserved.
			//
			// This two-mask trick (draw circle, then erase inverse) avoids
			// flicker: the sprite is never erased and redrawn — it's drawn
			// first, then only the surrounding area is restored.
			w('\x1b_SyncTERM:P;Paste;SX='+(pos.x - 4)+';SY='+(pos.y - 4)+';SW='+(imgdim.width + 8)+';SH='+(imgdim.height + 8)+';DX='+(pos.x - 4)+';DY='+(pos.y - 4)+';MBUF;MY=64\x1b\\');
		}

		// --- Cleanup: restore the background under the final sprite position ---
		// One last paste from buffer 0 (background) without any mask, so the
		// full rectangular region is restored, removing the sprite entirely.
		w('\x1b_SyncTERM:P;Paste;SX=' + pos.x + ';SY=' + pos.y + ';SW=' + imgdim.width + ';SH=' + imgdim.height + ';DX=' + pos.x + ';DY=' + pos.y + '\x1b\\');

		// Restore original ctrlkey_passthru so ESC works normally again.
		console.ctrlkey_passthru = opt;
	}
}
// Reset the line counter so the BBS doesn't think we've scrolled a bunch
// of lines and pause for "more?".
console.line_counter=0;
// Push a space into the input buffer — this satisfies any pending
// console.pause() or input prompt that may follow this script.
console.ungetstr(' ');
