/* pdfview.js
 *
 * Interactive PDF viewer for Sixel terminals (SyncTERM 1.2+): scroll with the
 * arrow keys, zoom with + and -.  The page is re-rendered at each zoom level, so
 * text and vector graphics stay sharp.
 *
 *	?pdfview <file.pdf> [start-page] [force]
 *
 *	file.pdf	absolute path of the file to display.  NOTE: the file is read
 *			with the BBS's own permissions, so never pass an unvalidated
 *			user-supplied path.
 *	start-page	optional 1-based page number to open
 *	force		skip the SyncTERM/Sixel capability check (for other
 *			Sixel-capable terminals, e.g. xterm -ti vt340, mlterm, foot)
 *
 * Keys:
 *	Arrow keys (or h/j/k/l)	scroll left/down/up/right
 *	PgUp/PgDn, Backspace/Space	scroll one screen (continuing onto the prev/next page)
 *	Home / End		top-left / bottom-left of the page
 *	+ / -			zoom in / out (also i / o)
 *	W			fit page width       F  fit whole page      1  actual size
 *	N / P			next / previous page G  go to page
 *	V			invert colours       ?  help               Q / ESC  quit
 *
 * Not supported: JPEG 2000 (JPXDecode), JBIG2 and progressive JPEG images (shown as
 * grey boxes), tiling patterns (approximated by a flat colour), transparency
 * groups/blend modes, AES-256 (V5) encryption, and password-protected files.
 *
 * univiewer.js does the same for PDFs and also displays images and Sixel files; this
 * program is the PDF-only version.  See also pdf2sixel.js (convert to a Sixel file).
 * Requires load/univiewer_lib.js, which loads pdf2sixel_lib.js.  Tab size is 4.
 */

"use strict";

load("univiewer_lib.js");

function main()
{
	var file = argv.length > 0 ? argv[0] : null;
	if (!file) {
		console.print("\x01n\x01cusage: \x01h?pdfview <file.pdf> [page] [force]\x01n\r\n");
		return;
	}
	var doc;
	try {
		doc = load_pdf(file);
	}
	catch (e) {
		console.print("\x01n\x01h\x01rUnable to display PDF: \x01w" + String(e).replace(/\x01/g, "") + "\x01n\r\n");
		console.pause();
		return;
	}
	viewer(doc, parseInt(argv[1], 10) || 0);
}

try {
	main();
}
catch (e) {
	log(LOG_ERR, "pdfview error: " + e + (e && e.stack ? "\n" + e.stack : ""));
	console.print("\x01n\x01h\x01rpdfview error: \x01w" + String(e).replace(/\x01/g, "") + "\x01n\r\n");
}
