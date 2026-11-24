/* pdf2sixel.js
 *
 * Convert one page of a PDF file to a Sixel image.
 *
 * Under jsexec the Sixel data is written to stdout (or a file), so it can be
 * redirected straight into a file:
 *
 *	jsexec pdf2sixel.js /full/path/file.pdf > page1.six
 *	jsexec pdf2sixel.js /full/path/file.pdf page=3 w=800 h=600 > page3.six
 *	jsexec pdf2sixel.js /full/path/file.pdf out=page1.six		(write to a file)
 *	jsexec pdf2sixel.js /full/path/file.pdf out=-			(stdout, explicitly)
 *
 * Under the BBS (?pdf2sixel file.pdf [page=N] ...) the image is sent to the user's
 * terminal once.  Use pdfview.js for interactive viewing and univiewer.js to view
 * the resulting .six files (or other images).
 *
 * Options (key=value, in any order after the file name):
 *	page=N		page number to render (default 1)
 *	w=W h=H		window size in pixels (default 640x384; h is rounded down to a
 *			multiple of 6)
 *	fit=width|page	initial scale: fit page width (default) or the whole page
 *	zoom=Z		then zoom by factor Z
 *	x=PX y=PX	then scroll by PX pixels
 *	out=FILE|-	write the Sixel data to FILE, or to stdout if "-" (the default
 *			under jsexec)
 *	ppm=FILE	also write the rendered image as a binary PPM file
 *	maxn=1|2|4|8	JPEG detail limit (smaller is faster, default 8)
 *
 * Progress and timing messages go to stderr so they never mix with the data.
 *
 * Requires: pdf2sixel_lib.js (and its inflate.js, pdf2sixel_fonts.js).
 * Tab size is 4.
 */

"use strict";

load("pdf2sixel_lib.js");

function message(str)
{
	if (typeof stderr != "undefined")
		stderr.writeln(str);
}

function main()
{
	var opt = { w: 640, h: 384, page: 1, zoom: 0, x: 0, y: 0, fit: "width", maxn: 8 };
	var file = null;
	for (var i = 0; i < argv.length; i++) {
		var m = /^([a-z]+)=(.*)$/.exec(argv[i]);
		if (m)
			opt[m[1]] = (m[1] == "out" || m[1] == "ppm" || m[1] == "fit") ? m[2] : parseFloat(m[2]);
		else if (!file)
			file = argv[i];
	}
	if (!file) {
		message("usage: jsexec pdf2sixel.js /full/path/file.pdf [page=N] [w=W] [h=H] [fit=width|page] [zoom=Z] [x=PX] [y=PX] [out=FILE|-] [ppm=FILE] [maxn=N]");
		return;
	}
	var tty = (typeof console != "undefined");
	if (tty && opt.out === undefined)
		opt.out = "";
	else if (!tty && opt.out === undefined)
		opt.out = "-";
	var t0 = Date.now();
	var doc = load_pdf(file);
	opt.h -= opt.h % 6;
	var view = new View(doc, opt.w, opt.h);
	view.page = Math.max(0, Math.min(view.pages.length - 1, opt.page - 1));
	if (opt.fit == "page")
		view.fit_page();
	else
		view.fit_width();
	if (opt.zoom > 0)
		view.zoom(opt.zoom);
	view.sx += opt.x;
	view.sy += opt.y;
	view.clamp();
	view.render(null, opt.maxn);
	if (opt.ppm)
		write_ppm(opt.ppm, FB, FW, FH);
	var data = sixel_encode(FB, FW, FH);
	if (opt.out === "-")
		stdout.write(data);
	else if (opt.out) {
		var f = new File(opt.out);
		if (!f.open("wb"))
			throw "Unable to create " + opt.out;
		f.write(data);
		f.close();
	}
	else if (tty)
		console.write("\x1b[?80l" + data);
	if (!tty)
		message("page " + (view.page + 1) + " of " + view.pages.length + ", " + FW + "x" + FH + " pixels, "
			+ data.length + " bytes, " + (Date.now() - t0) + "ms");
	for (var why in IMG_FAIL)
		message("  note: " + IMG_FAIL[why] + " x " + why);
}

try {
	main();
}
catch (e) {
	var msg = "pdf2sixel: " + e;
	if (typeof console == "undefined")
		message(msg);
	else
		console.print("\x01n\x01h\x01r" + msg.replace(/\x01/g, "") + "\x01n\r\n");
}
