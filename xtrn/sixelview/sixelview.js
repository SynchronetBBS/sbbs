/* sixelview.js
 *
 * Universal viewer for Sixel terminals (SyncTERM 1.2+) with scrolling and zooming.
 * Displays PDF files, Sixel files (.six), PNG, JPEG, GIF (animated too), BMP, PPM/PGM, SVG, PCX, TGA, ICO,
 * TIFF, IFF/ILBM and XBin/BIN text-mode art; the type is detected from the file's contents.
 *
 *	?sixelview <file> [start-page] [force]
 *
 *	start-page	for PDF files, the 1-based page to open
 *	force		skip the SyncTERM/Sixel capability check
 *
 * Keys: arrow keys scroll, + / - zoom, W fit width, F fit whole image/page, 1 actual
 * size, V invert colors, N/P/G change page (PDF), Q or ESC quit.  See pdfview.js for
 * the full key list and the PDF features that are not supported.
 *
 * Unlike /sbbs/mods/sixel.js (which just dumps a Sixel file to the terminal), the
 * image is decoded and re-rendered, so it can be scrolled and zoomed.  PDF pages are
 * re-rendered at every zoom level (sharp text and vector graphics); raster images
 * are resampled.
 *
 * Requires sixelview_lib.js, which loads pdf2sixel_lib.js.  Tab size is 4.
 */

"use strict";

load("sixelview_lib.js");

function main()
{
	var file = argv.length > 0 ? argv[0] : null;
	if (!file) {
		console.print("\x01n\x01cusage: \x01h?sixelview <file> [page] [force]\x01n\r\n");
		return;
	}
	var doc;
	try {
		var f = new File(file);
		var head = "";
		if (f.exists && f.open("rb")) {
			head = f.read(1024);
			f.close();
		}
		if (head.indexOf("%PDF-") >= 0)
			doc = load_pdf(file);
		else if (/\.svg$/i.test(file) || /<svg[\s>]/i.test(head))
			doc = load_svg(file);
		else
			doc = new ImageDoc(load_image_file(file));
	}
	catch (e) {
		console.print("\x01n\x01h\x01rUnable to display file: \x01w" + String(e).replace(/\x01/g, "") + "\x01n\r\n");
		console.pause();
		return;
	}
	try {
		viewer(doc, parseInt(argv[1], 10) || 0);
	}
	finally {
		mouse_stop();
	}
}

try {
	main();
}
catch (e) {
	log(LOG_ERR, "sixelview error: " + e + (e && e.stack ? "\n" + e.stack : ""));
	console.print("\x01n\x01h\x01rsixelview error: \x01w" + String(e).replace(/\x01/g, "") + "\x01n\r\n");
}
