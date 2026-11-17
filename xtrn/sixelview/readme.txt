WHAT IS SIXELVIEW?
==================
SixelView by nelgin (and Claude) is a pure Synchronet script to convert
different graphics file formats and pdfs and display them in a sixel viewer.
It doesn't require any external conversion tools, which makes it portable
on any platform that runs Synchronet.


HOW TO USE SIXELVIEW:
====================
To use, simply call sixelview with the path to your pdf file, for example
if you want to use it in a menu:

'P': { exec: '/sbbs/xtrn/sixelview/sixelview.js /sbbs/xtrn/sixelview/text/sixelview_demo.pdf' },

You could also add it to run from a door menu if you wish.

The file will be rendered into its own viewer.

It's not perfect, it's going to be a little slow, using native JS
rather than external tools, however it is completely portable without
having to rely on OS tools.

inflate.js		provides zlib/deflate decompression

pdf2sixel_lib.js	PDF parser, renderer and Sixel encoder

pdf2sixel_fonts.js	outline data for the standard Helvetica, Times
			and Courier faces. More coudl be added.
sixelview_lib.js	the viewer, plus Sixel, PNG, JPEG, GIF, BMP and
			PPM loading
pdf2sixel.js		converts a PDF page to Sixel (stdout, a file, or
			the terminal)
sixelview.js		Universal viewer: PDF plus images                                       │

There are some files in the text directory that you can use as a demo.

This set of programs comes with absolutely no support and warranty other than
it works fine on my system. If you have any suggestions or improvements,  you
can contact me at sysop@endofthelinebbs.com or sysop@1:124/5016

CHANGE LOG
Version 1.0		10 Oct 2026	Initial Release
Version 1.1		17 Nov 2026	Submitted to synchronet git
					Moved to ./xtrn/sixelview
