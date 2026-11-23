WHAT IS SIXELVIEW?
==================
SixelView by nelgin (and Claude) is a pure Synchronet script to convert
different graphics file formats and pdfs and display them in a sixel viewer.
It doesn't require any external conversion tools, which makes it portable
on any platform that runs Synchronet.

As of version 1.2 it is also an all-in-one file library browser: text files,
ANSI and ASCII art, PDFs, pictures and ZIP files, from directories on your
BBS or from online archives, all in one lightbar program.  If you are
running ansiview and a textfiles.com section, sixelviewlibrary.js can
replace both of them.

WHAT IT CAN SHOW
================
Pictures and documents (shown in the Sixel viewer, which needs a terminal that
supports Sixel, such as SyncTERM):
  PDF                       text, vector graphics, pictures, links and
                            bookmarks; scanned books too (JPEG, JPEG 2000,
                            JBIG2, CCITT); blank-password encrypted files

  PNG JPG JPEG GIF BMP      GIFs with several frames are played as animations

  PPM PGM SIX               Sixel files are shown as they are

  SVG                       drawn as vectors

  PCX TGA ICO TIF TIFF      older picture formats

  IFF LBM ILBM              Amiga pictures, including EHB and HAM

  XB XBIN BIN               text-mode art, drawn as a picture with the file's
                            own font and palette, at its own size (100%) so
                            the font stays sharp; scroll to see the rest.  Any
                            other picture more than twice as tall as it is
                            wide opens at full width.

  HTML HTM XHTML EPUB       laid out as pages; links work

Text (shown in the text reader or drawn to the terminal):
  TXT ASC MSG DIZ NFO BBS MD LOG CFG INI CONF 1ST, and any file with another
  extension or none; ANS and ANSI (and anything holding ANSI codes) with
  SAUCE records and iCE colors

Archives (unpacked on demand; see below):
  ZIP RAR CBZ CBR

USING THE MOUSE
===============
It works in any terminal that reports the mouse (SyncTERM does).
  Lists          click a row to select it; click the selected row to open it;
                 click in the scroll bar to jump to that part of the list;
                 the wheel moves up and down; the right button goes back (Q).

  Text reader    the wheel scrolls; the right button quits.

  Picture / PDF  the wheel scrolls half a screen at a time and carries on to
                 the next or previous page at the end of a page; click a link
                 (or a form control on a web page) to follow it; the right
                 button quits.

  Bottom line    every menu bar works as buttons: click Enter, Q, S, N/P,
                 (C)ontents and so on to press that key.  Items that stand for
                 two keys, such as Up/Dn, PgUp/PgDn and Left/Right, press the
                 first key when you click the part before the slash and the
                 second for the part after it.

HOW TO USE SIXELVIEW TO VIEW ONE FILE:
=====================================
To view a single pdf or picture, simply call sixelview with the path to the
file, for example if you want to use it in a menu:

'P': { exec: '/sbbs/xtrn/sixelview/sixelview.js ' +
       '/sbbs/xtrn/sixelview/demo/sixelview_demo.pdf' },

You could also add it to run from a door menu if you wish.

The file will be rendered into its own viewer.

It's not perfect, it's going to be a little slow, using native JS
rather than external tools, however it is completely portable without
having to rely on OS tools.


THE FILES:
=========
inflate.js		provides zlib/deflate decompression

pdf2sixel_lib.js	PDF parser, renderer and Sixel encoder

pdf2sixel_fonts.js	outline data for the standard Helvetica, Times
			and Courier faces. More could be added.

sixelview_lib.js	the viewer, plus Sixel, PNG, JPEG, GIF, BMP,
			PPM, SVG and the other picture loaders

pdf2sixel.js		converts a PDF page to Sixel (stdout, a file, or
			the terminal)

sixelview.js		Universal viewer: PDF plus images

pdfview.js		Viewer for a single PDF file (?pdfview file.pdf
			[page])

pdfunmark.js		Host tool: removes watermark text from PDF books (see
			REMOVING WATERMARKS below).  Not used by the BBS.

sixelviewlibrary.js	The library browser (see below)

http_lib.js		The web requests the browser makes (GET and POST, with
			the limits and checks described under WEB PAGES)

settings-example.ini	Example configuration for the library browser

DefaultTheme.cfg	The colors and text the library browser uses

hooks/			Small scripts that connect the browser to online
			archives, such as hooks/sixteencolors.js

demo/			Demo files to try the viewers on


THE LIBRARY BROWSER - SIXELVIEWLIBRARY.JS
=========================================
Let me walk you through how it works, because it is a lot easier to explain
by talking it through than by listing options.

When a user runs sixelviewlibrary.js they see a list of "libraries".  A
library is just a name that points at a directory of files on your BBS (or
at an online archive, more on that later).  The user moves up and down with
the cursor keys and presses Enter to open one.  If they are only allowed to
see one library, they are dropped straight into it.

Inside a library they see the files and, if you allow it, sub-directories.
Enter opens a file, Q goes back up a level and the cursor goes back to the
item they were on, so they don't have to hunt for their place again.

The mouse works too, for terminals that report it (SyncTERM does).  The
scroll wheel moves up and down in the lists, the text reader and the picture
viewer (in the picture viewer the wheel and the Up/Down keys move half a
screen at a time; j and k scroll in small steps; scrolling on past the bottom
or top of a page carries on to the next or previous page).  In the lists,
clicking a row selects it, clicking the selected row opens it (so a "double
click" is two clicks), clicking in the scroll bar jumps to that part of the
list, and the right button goes back, like Q.  The right button also quits the
text reader and the picture viewer.  The menu bar on the bottom line works as
buttons everywhere: click Enter, Q, S, N/P and so on to press that key.  Items
that stand for two keys, such as Up/Dn, PgUp/PgDn and Left/Right, send the
first key when you click the part before the slash (Up) and the second for the
part after it (Dn).  Elsewhere in the picture viewer only the wheel is used.

What happens when they open a file depends on what it is:

  - PDFs and pictures go to the Sixel viewer, with scrolling and zooming,
    if their terminal supports Sixel (SyncTERM does).  The picture types are
    pdf png jpg jpeg gif bmp ppm pgm six svg, plus the older formats pcx tga
    ico tif tiff iff lbm ilbm (Amiga pictures, including EHB and HAM), and
    xb xbin bin (text-mode art, drawn as a picture with the file's own font
    and palette).  SVG drawings are drawn directly as vectors, so they stay
    sharp at any zoom; see the notes at the end for what isn't drawn.

  - A GIF with several frames is played as an animation: A starts and stops
    it.  Sending a whole picture for every frame is far too slow to keep up
    with the GIF's timing, so when an animation starts it is prepared first:
    every frame is drawn once at the current zoom (a message shows the
    progress, and any key cancels), all frames are given one shared set of
    colors, and for each frame only the part of the picture that changed
    since the previous frame is worked out.  Playback then sends just those
    changes, on the GIF's own clock, so the BBS's work and your connection
    speed matter much less and a small moving sprite costs very little.
    The < and > keys (or , and .) play it slower or faster, from a quarter to
    four times the GIF's own speed.  It starts at half speed, which suits
    SyncTERM better, and the status line shows the speed when you change it.
    A frame delay of 10 milliseconds or less is treated as 100, as web
    browsers do.  Zooming or scrolling prepares it again for the new view.
    Turning dithering off (D) plays it the old way, sending every frame in
    full.  An animation too large to keep prepared in memory is also played
    that way.

  - Plain text files (txt asc msg diz nfo bbs md log cfg ini conf 1st, and
    any file with an extension the viewer doesn't know, or none) open in a
    text reader.  Up
    and down arrows scroll, PgUp/PgDn/space move a page, Home and End jump
    to the top and bottom.  Long lines are wrapped, UTF-8 files (like most
    Project Gutenberg books) are converted so they display properly, and the
    invisible marker some editors put at the start of a file is removed.

  - ANSI art (ans ansi) and anything containing ANSI codes is displayed the
    way ansiview displays it, including SAUCE records and iCE colors.  Press
    a key when it has finished.  Ctrl-C, Esc or Q stops it while it is still
    drawing and takes you straight back to the list.  With pause on, it stops
    after every screenful with a "-- More --" line (any key continues, Q
    stops).  This does its own counting, because art that moves the cursor up
    after each line (a lot of it does) fools Synchronet's built-in pause so it
    never pauses.  The speed setting is done by the BBS rather than by
    SyncTERM's speed emulation, so that it can always be interrupted.

  - HTML pages and EPUB books (html htm xhtml epub) are laid out as pages and
    shown like a PDF; see WEB PAGES, HTML FILES AND BOOKS below.

  - ZIP and RAR files and CBZ/CBR comic books (a .cbz or .cbr is a zip or rar
    of page images) are unpacked for you, on demand.  The first time somebody
    selects one, it is extracted into a directory with the same name beside it
    (pack.zip becomes pack/, story.cbz becomes story/) and the user is taken
    inside.  What the list shows afterwards depends on the kind of file:

      .zip   Once it has been extracted, the ZIP (or RAR) is hidden and its
             directory is shown instead, exactly as ansiview does.  That suits
             art packs, where people want to browse the folders.

      .cbz   (and .cbr)  The comic stays in the list as one entry (with its
             size and date), and the directory it was extracted into is
             hidden.  Selecting the comic takes you straight to its pages, and
             Q or the .. entry brings you back to the list with the cursor on
             the comic.  This is like an EPUB, which is also a single entry
             (EPUBs are unpacked into the cache, so they never leave a
             directory in your library at all).  If a library's exclude
             setting hides *.cbz, the directory is shown instead, so the pages
             can't be lost.

    Names with numbers in them are sorted by the number, so page2 comes
    before page10.  The extracted directories are ordinary directories on
    your disk, so you can delete them to free space; they are made again the
    next time somebody asks for the archive.

    For this to work the library must have subdirs=true (archives aren't
    listed without it) and the BBS needs permission to write to the library
    directory.  Extraction uses Synchronet's own Archive support, so unzip is
    not normally needed.  A few old ZIPs (the "imploded" kind from the early
    90s, common in art packs) can't be read that way; for those it falls back
    to the unzip program in /sbbs/exec if you have one (on Linux, a symlink to
    /usr/bin/unzip is enough), and cleans up after itself if both fail.  RAR
    files work the same way, with the unrar program (/usr/bin/unrar) as the
    fallback; many old DOS-era RARs need it.  As a safety net, archives with
    unsafe file names, more than 2000 files or more than 50 MB of data are
    refused (zip_max_files and zip_max_mb in settings.ini change those
    limits).  The user is told how many files or how much data the archive
    holds against the limit, and that the sysop has been told.  That means a
    warning in the BBS log and a netmail (file path, problem, user) to the
    sysop, or to whoever error_mail= in settings.ini names: an alias, a user
    number or an Internet address, or "none" for no mail.  Each file is mailed
    once a day.  To count the files in an archive yourself:  unzip -Zt
    book.epub

The keys, in any list:

  Up/Down, PgUp/PgDn, Home/End    move around

  Enter                           open the item

  Q or Esc                        go back up one level (like the .. entry);
                                  from the top of a library, back to the list
                                  of libraries

  S                               cycle the output speed for ANSI files
                                  (Full, 300 ... 115200)

  P                               turn screen pausing on and off

  I                               (online libraries only) show the picture
                                  of the ANSI file instead of the ANSI itself

  digits and letters              jump to a library number, or to the next
                                  file starting with that letter

In the PDF and picture viewer the bottom line shows the keys that matter, with
the key letter in yellow: (C)ontents, (</>) Page, (Q)uit and (H)elp (a single
picture shows (N)ext/(P)rev File instead of the page keys).
Press H (or ?) for a pop-up box that lists every key that applies to what you
are looking at (grouped under headings when the screen is tall enough, and
scrolled with Up and Down if it is too short).  Any other key closes it.

Books usually have a table of contents built into the file (the bookmarks
a PDF reader shows in its side panel).  Press C to see it: a list of the
chapters and sections, indented by level, with page numbers.  Pick one with
the cursor keys and Enter and you jump straight to it; b takes you back.  C
or Q closes the list.  The list starts on the entry nearest to the page you
are reading.  If a PDF has no bookmarks the status line says so.

Links inside the pages themselves, such as the entries on a printed contents
page, work too.  Press Tab (or ]) to select the first link on the
screen and Tab again to move to the next one; [ goes back to the previous one.
The selected link gets a yellow box and the status line says where it goes.
Enter follows it, jumping to the page (and position) it points at, and b
takes you back to where you were, as many times as you followed links.  L
draws a thin blue box around every link on the page.  Only links that jump
to another place in the same document are used; web addresses and links to
other files are ignored.

In every viewer (text, ANSI art, pictures and PDFs), N moves to the next
file in the list and P to the previous one, so you can flip through a whole
directory of files without going back to the list each time.  The cursor
keys are left alone for scrolling and panning.  In a document with pages, the
< and > keys (or , and .) go to the previous and next page, PgUp and PgDn
scroll a screenful (and turn the page when you reach its end), and G goes to
a page number; when a PDF is opened on its own with sixelview.js, N and P
change pages too.  On an animated GIF, which has no pages, < and > change the
speed instead.


SETTINGS.INI
============
Everything is configured in settings.ini in this directory.  There is a
settings-example.ini that explains every option; copy it to settings.ini and
edit that.  The file has two parts: some settings at the top that apply to
everything, and then one section for each library.

The top part looks like this, and everything in it is optional:

  theme=DefaultTheme.cfg
  pause=false
  speed=2400
  zip_max_mb=50
  zip_max_files=2000
  error_mail=
  text_types=
  image_max_mb=3
  cache_max_days=14
  cache_max_mb=100

"theme" is the file that holds all the colors (see the next section).
"pause" and "speed" are the starting values for the P and S keys, and users
can still change them while they browse.  They only affect ANSI and ASCII art;
text files open in the scrolling reader instead.  The zip_ settings are the
limits for extracting archives, and error_mail says who is told when one is
refused.  text_types lists extra file extensions to show as plain text
(text_types=conf,sh).  image_max_mb is the biggest picture an online library
may show.  The cache settings are explained under hooks, below.

Each library is a section called [library:Something], and the name after the
colon is what users see in the list.  Here is a local library:

  [library:Textfiles.com]
  dir = /library/textfiles
  desc = textfiles.com Mirror
  ars = LEVEL 50
  subdirs = true
  exclude = *.exe,*.com,*.html,*.rip,*.jpg,*.gif,*.___,*.rar

"dir" is the directory.  If it doesn't start with a / it is taken to be
relative to this sixelview directory.  "desc" is the description beside the
name in the list.

"ars" is a normal Synchronet access requirement string, so anything you
can use elsewhere works: LEVEL 50, SYSOP, FLAG A, AGE 18 AND LEVEL 10 and so
on.  Users who don't pass don't even see the library.  Leave it out and
everybody can see it.

"subdirs" lets users browse into sub-directories of the library.  They can
never go above the directory you named.  It defaults to false.  It also
needs to be true for ZIP files to be shown.

"include" and "exclude" decide which files are listed, using wildcards: *
matches anything and ? matches one character, and capital letters don't
matter.  Separate patterns with commas.  Use "include" when you only want
certain files, for example "include = *.pdf" for a library of books.  Use
"exclude" when you want everything except certain files, like the example
above.  If you give both, include is applied first and then exclude removes
things from what's left.  A plain extension like "pdf" is understood as
"*.pdf".  (If you are coming from ansiview, its "hide" setting works here
too as another name for exclude.)

Any file whose extension isn't one of the types listed above is treated as a
text file, so things like "turkey.fun", "recipe.012" or a file with no
extension at all are listed and opened in the text reader.  If such a file
turns out to be a picture, a PDF or other binary data, it is opened as that
or reported as unable to display.  Use "exclude" to hide what you don't want
users to see (*.exe,*.dll,*.dat and so on).

"sort" can be name, date or size, and you can put a minus sign in front to
reverse it, so "sort = -date" gives newest first.  The default is name.

Descriptions.  To give files and sub-directories a description without
cluttering settings.ini, put a hidden file named .desc in the directory.  It
is plain text with one line per item: the file (or sub-directory) name, a
Tab, then the description.  Lines that are blank or start with ; or # are
ignored, and names must match exactly, capital letters included.  Like this
(the gap is a Tab):

  turkey.fun    Grandma's turkey recipe, with the stuffing she wouldn't share
  recipes       Everything else from the kitchen

The list shows the descriptions in a column after the name.  Names and
descriptions are often longer than the screen, so the Left and Right arrow
keys scroll them sideways half a screen at a time, while the size and date
stay where they are.  Small < and > marks in the column heading show when
there is more to see on that side.  Files whose names begin with a dot, like
.desc, are never listed.

The textfiles.com mirror comes with a .desc file in every directory, made from
the descriptions on the web site (about 60,000 of its 64,000 files have one;
the site itself has none for the rest).

That's all you need for as many local libraries as you like.  Just add
more sections.  Libraries appear in the list in the order you write them.


HOOKS - ONLINE ARCHIVES
=======================
A library doesn't have to be a directory on your disk.  If instead of "dir"
you give it a "module", the browser asks a small script called a hook to
fetch the listing from a remote server.  The hook scripts live in the
hooks directory.  The one that ships with sixelview is for the Sixteen
Colors ANSI/ASCII art archive (https://16colo.rs):

  [library:Sixteen Colors]
  desc = Long standing archive of ANSI and ASCII artwork
  module = sixteencolors.js
  cachettl = 86400

That is the whole thing.  Users see Years, then Packs, then the files in a
pack, in the same lightbar as everywhere else, and ars, include, exclude and
desc all work exactly as they do for a local library.  (subdirs and sort
don't apply, because the hook decides the layout.)

Pressing Enter on a file downloads it and shows the real ANSI, with speed
and pausing as usual.  Pressing I shows Sixteen Colors' picture of the file
instead, which is a nice way to see a big piece of art all at once.  N and P
still move to the next and previous file.

Everything that is downloaded is kept in a .cache directory here so that it
doesn't have to be fetched again.  "cachettl" says how many seconds a
downloaded list of years, packs or files is trusted before it is asked for
again.  The cache cleans up after itself each time somebody starts the
browser and an online library is configured: files older than cache_max_days
(14 by default) are deleted, and if what is left is still bigger than
cache_max_mb (100 by default), the files nobody has looked at for the longest
time are deleted until it fits.  Set either one to 0 to turn that rule off.
Files from the last ten minutes are never touched, in case somebody is using
them.

Writing your own hook is not hard.  A hook is a JavaScript file in the
hooks directory that has one function, create(ctx), and returns an object
with two functions: list(path) that returns the items at that level of the
archive, and fetch(path, entry) that downloads one item and returns the
local file name.  ctx gives you helpers for downloading and caching so you
don't have to write that part.  Look at hooks/sixteencolors.js, it's short
and the top of the file explains the details.

The browser does all the screens and keys, so a hook is only a few dozen
lines.  Anybody with an archive that has a web API can make one.


WEB PAGES, HTML FILES AND BOOKS
==============================
HTML files (html htm xhtml) and EPUB books (epub) in a library open in the
same viewer as PDFs, and that is not an accident: the library lays the page
out (headings, paragraphs, lists, tables, quotes, preformatted text, pictures)
and writes it as a PDF, and the viewer shows that.  So everything you already
know works: scrolling, zoom, PgUp/PgDn between pages, Tab and Enter for links,
b to go back, and C for a table of contents built from the headings.  Style
sheets, scripts and colors aren't used, and forms only work on web libraries
that turn them on (see below), so pages look plain, but they read well.

An HTML file in a library can link to other HTML files in the same library
(relative links), and those links are clickable.  An EPUB is unpacked into the
cache, its chapters are put in reading order, and its own table of contents is
used for C.  Links between chapters work.

A library can also be a web site, which turns the viewer into a small, strict
web browser.  Instead of dir you give it a url:

  [library:Wikipedia (simple)]
  desc = Simple English Wikipedia
  url = https://simple.wikipedia.org/wiki/Main_Page
  allow = simple.wikipedia.org, upload.wikimedia.org
  content = searchform, firstHeading, mw-content-text
  forms = true
  ars = LEVEL 20

When a user opens it they see that page.  Pages are laid out in a single
column whatever the site's own layout, so a page that is in three columns on a
big screen comes out as one long column, but the menus and side bars come
first, which is a nuisance.  content lists the ids of the elements you want
(see the page's HTML, or the example above for Wikipedia); only those are
shown, in that order, and a page without any of them is shown whole.  The
sysop decides where the browser may go: allow is the list of host names
(wildcards are fine, for example *.example.com) that pages, pictures and links
may come from.  If you leave it out, only the host in url is allowed.  A link
to any other host simply isn't clickable, and pictures from other hosts aren't
loaded.  Redirects are followed only to allowed hosts.  Pages and pictures
have size limits (max_page_kb and max_image_kb) and a time limit, downloaded
files are cached under .cache/web (so the cache cleaner looks after them), and
pages are kept for cachettl seconds (600 by default).  images = false turns
pictures off.  Nothing is ever run on the BBS: scripts are ignored and only
plain pages and pictures are fetched.

Users follow a link with Tab and Enter, and press b to go back to the last
page.  Q leaves the site.  Users can only reach what you listed, so a library
is a good way to offer a handful of sites that suit a text terminal.

Forms (search boxes and so on) are off by default.  Add forms = true to a web
library and the controls on its pages are drawn in blue brackets and selected
with Tab like links:

  [text_______]   a box to type in: Enter, type, Enter again (Esc cancels)
  [x]  [ ]        a check box: Enter ticks or clears it
  (*)  ( )        a radio button: Enter chooses it
  [Medium v]      a list: Enter shows the next entry
  [ Search ]      a button: Enter sends the form

Pressing Enter in the only text box on a form sends it, as a browser does.
GET and POST forms are supported (HTML forms never use PUT).  The page you get
back is shown like any other and b returns to the form.  What the user entered
is kept while they are on the page.  Not supported: file uploads (shown but
inactive), multipart/form-data (text fields are sent in the ordinary way),
cookies and logins, and anything that needs scripts.

What is typed can't hurt the BBS: it is only ever data in a web request, never
run, never used as a file name, never passed to a program.  Control characters
are removed, each value is cut to 512 characters and the whole form to 8 KB,
and everything is percent-encoded, so it can't add a header or a second
request.  A form is only sent to an address the library's allow list permits
(a form that points elsewhere is refused), a redirect after a POST is followed
only as a plain GET and only to an allowed host (307 and 308 are refused), a
password is never sent over plain http, a user can send ten forms a minute,
and what was typed is never written to the log (the log records the user
number and the address without its query).  The BBS's name is not sent to the
site.

COLORS - THE THEME FILE
========================
Colors are not in settings.ini.  They are in a separate theme file, named
by the "theme=" line in settings.ini.  DefaultTheme.cfg is the one that
comes with sixelview and uses the same look as DDMsgReader: cyan text,
bright yellow numbers, bright blue dates, a blue highlight bar and a white
help line at the bottom with red hotkeys.

To make your own, copy DefaultTheme.cfg to MyTheme.cfg, change what you
like, and put theme=MyTheme.cfg in settings.ini.  Anything you leave out of
your theme falls back to the built-in default, so a theme only has to
contain the things you want to change.

Each color is a few letters, the same codes as Synchronet's Ctrl-A
attributes without the Ctrl-A, and DDMsgReader's theme files use the same
thing.  Put as many together as you like:

  n    normal - clears everything before it, so start with this
  h    high intensity (bright)
  i    blink

  Foreground:   k black    r red      g green    y yellow
                b blue     m magenta  c cyan     w white
  Background:   0 black    1 red      2 green    3 brown
                4 blue     5 magenta  6 cyan     7 white

So "nc" is plain cyan, "nhy" is bright yellow, "n7r" is red on a white
background and "hw" is bright white.  For example:

  listNumColor=nhy
  listNameColor=nc
  listDateColor=hb

The names in DefaultTheme.cfg tell you what they color: the header bar
(headerTextColor, headerNameColor), the column headings, the list columns
(number, name, description, size, date, directory), the selected row
(listHighlightBkgColor is just a background number, then each column has a
Highlight color), the help line at the bottom (a background plus colors for
general text, hotkeys and the bracket between them), the scrollbar and the
color of text in the reader (textBodyColor).

At the bottom of the theme file are a few pieces of text, like the message
shown when a library is empty.  Those can contain \x01 followed by a color
letter if you want colors inside the message, for example \x01h\x01y for
bright yellow.  Where you see %s, the program puts something in, such as the
error that happened.

The scrollbar characters are written as \xb0 and \xdb, the CP437 character
codes, so you can change them to whichever characters you prefer.


ADDING IT TO YOUR MENUS
=======================
There are three ways to give users access.  If your Synchronet is a modern
one (3.18 or later), the easiest is the first.

The first is to make it your text file section.  In SCFG go to System,
Loadable Modules and choose Text Section.  This is the module Synchronet
runs whenever a user chooses the text files section, which on the default
menus is the G key, so there is nothing to edit in default.js and nothing
to add to your menu screens.  Set the command to:

  ../xtrn/sixelview/sixelviewlibrary.js

and, if you want, put an access requirement on it such as LEVEL 50.
Synchronet runs the first module whose requirements the user meets, so you can
leave a fallback behind it for users who don't qualify.  Remember to give the
libraries their own ars lines in settings.ini as well.  If you don't have a
Loadable Modules entry in SCFG (older Synchronet), use one of the next two
ways.

The second is as an online program (door).  In SCFG go to External Programs,
Online Programs, pick a section and add:

  Name:                        SixelView Library
  Internal Code:               SXLIB
  Start-up Directory:          ../xtrn/sixelview
  Command Line:                ?sixelviewlibrary.js
  Multiple Concurrent Users:   Yes

The third is directly from your command shell, so it is one keystroke from
the main menu.  Synchronet's default shell is /sbbs/exec/default.js, and you
should never edit the files in exec directly, because an upgrade will
overwrite them.  Synchronet looks in /sbbs/mods before /sbbs/exec, so the
trick is to copy the file there and change the copy.  Do this first, before
you change anything:

  cp /sbbs/exec/default.js /sbbs/mods/default.js

(If /sbbs/mods/default.js already exists, you've customized it before, so
edit that one and don't copy over it.)

Now edit /sbbs/mods/default.js.  Find the list of commands in the main menu
(the lines that look like 'A': { eval: 'bbs.auto_msg()' }, ) and add a line
for sixelview with a key nobody else is using.  The format is the same as
the entries around it, for example the one that logs a user off:

  'O': { eval: 'logoff(/* fast: */false)' },

and sixelview's is:

  'Z': { exec: '/sbbs/xtrn/sixelview/sixelviewlibrary.js' },

(the quotes and the comma at the end matter).  You can use any key that
isn't taken; look through the existing entries to see what is free.  If you
are using this to replace ansiview and your textfiles section, you could
reuse their keys and remove their lines.

The new key won't show up on the menu screen by itself.  Edit your main
menu display, in /sbbs/text/menu/ (the main.* files, whichever you use), and
add the new key and name where you want it.


SOME NOTES AND LIMITS
=====================
The viewers are written in plain JavaScript, so everything is slow on a
debug build of Synchronet and much quicker on a release build.

JPEG 2000 pictures inside PDFs (which scanned books use a lot) are decoded by
the program itself: JP2 files and plain codestreams with the usual options
(any wavelet, color transform, tiles, layers, palettes).  The pictures are
decoded at only the resolution they are shown at, which makes it much quicker,
and again in more detail if you zoom in.  A few unusual options (non-default
code block styles, packed packet headers, sub-sampled color) are shown as gray
boxes.

JBIG2 pictures are decoded when they are bitmaps, or use symbol dictionaries
(including refined and aggregate symbols) and text regions with arithmetic
coding, which is what scanned books use, and halftone regions (a full page
takes five to fifteen seconds); Huffman-coded JBIG2, MMR-coded halftones and
refinement-only regions are shown as gray boxes.  Arithmetic-coded JPEGs
aren't supported (progressive JPEGs and interlaced PNGs are); BMP must be
uncompressed.

Encrypted PDFs are opened when they need no password, which is the usual case
for the "encrypted" books you find (RC4 and AES, blank user password).  A PDF
that really asks for a password is refused.

SVG files are drawn directly as vectors.  What is drawn: all the basic shapes
and paths (curves and arcs, dashes, joins and caps), transforms, use and
symbol, CSS styles, linear and radial gradients (their units, transforms,
spread methods and stop opacity), patterns, clip paths, masks, group opacity,
markers (arrowheads), pictures (data: URIs, or picture files in the same
directory as the SVG), text, and filters (blur, drop shadow, offset, flood,
merge, composite, blend, color matrix, component transfer and morphology).

Text is drawn with the same built-in Sans, Serif and Mono faces the PDF viewer
uses, so it follows the font family, size, weight, style, spacing, anchoring
and per-character positions in the file but not the exact typeface, and
characters outside the Latin range show as ?.

What isn't drawn: text laid out along a path, the filters that make textures
and lighting (feTurbulence, feDiffuseLighting, feSpecularLighting,
feDisplacementMap, feConvolveMatrix, feImage and feTile; the rest of the
picture is still drawn), scripts, animation, foreignObject, and SVG files
used as pictures inside another SVG.  The status line says so when a file uses
something it can't draw.  Compressed .svgz files are not supported.  Filters
and masks on a large drawing take a while to compute, especially on a debug
build of Synchronet.

ansiview's per-directory ansiview.ini description files are not read.

An online library downloads each picture in full before it is shown, and
only checks the size afterwards.

SLOW PDF FILES AND CONVERTING THEM
==================================
A PDF that is a scan of a book (every
page one big picture) can take several seconds per page, because the picture
has to be decoded each time.  The slowest are JBIG2 and JPEG 2000 scans and
pages of 3 megapixels or more.  A file over 200 MB isn't opened at all, and
SixelView keeps the whole file in memory (about four times its size) while it
is open, so very large scans also strain the BBS.  You can convert such files
once, on the host and not in SixelView, so that SixelView only has to read
plain pictures:

    gs -q -dNOPAUSE -dBATCH -sDEVICE=pdfwrite -dCompatibilityLevel=1.5 \
       -dAutoFilterMonoImages=false -dMonoImageFilter=/FlateEncode \
       -dDownsampleMonoImages=false \
       -dAutoFilterGrayImages=false -dGrayImageFilter=/DCTEncode \
       -dDownsampleGrayImages=false \
       -dAutoFilterColorImages=false -dColorImageFilter=/DCTEncode \
       -dDownsampleColorImages=false \
       -dPassThroughJPEGImages=true -dPassThroughJPXImages=false \
       -dDetectDuplicateImages=true \
       -sOutputFile=new.pdf old.pdf

This needs Ghostscript (gs).  Black-and-white pictures become Flate (fastest;
use /CCITTFaxEncode for a smaller file), and gray and color ones become JPEG.
Text, fonts, links and the outline are kept.  The new file is typically 3 to
8 times bigger and pages show about twice as fast once open, but it takes
longer to open, so only convert books that are really slow.  Keep the result
under about 100 MB (a book that comes out bigger than that is better left as
it was).  Check the page count and a few pages before replacing the original.

REMOVING WATERMARKS - PDFUNMARK.JS
==================================
If someone has stamped text into your PDF books (a web site name, "Downloaded
from ...") you can remove it.  This is a host tool, run from the sixelview
directory, and it is not part of the BBS:

    jsexec -m536870912 pdfunmark.js book.pdf "watermark text" out=clean.pdf

Give it the text to remove (several texts can follow one another, each in
quotes).  Case and spaces don't matter, and it finds the text even when the
file splits it into single letters.  With out= the changed book is written to
a new file; without it the book itself is changed (the changes are added to
the end, so the old text is still inside the file; keep a copy if you want it
back).  It says how many pieces of text it hid, and changes nothing if it
finds none.

It only works on text.  A watermark that is part of a scanned picture can't be
removed this way, and if the watermark shares one piece of text with real
words, those words are hidden too.  Encrypted books are handled when they use
RC4, but not AES.

This set of programs comes with absolutely no support and warranty other than
it works fine on my system. If you have any suggestions or improvements,  you
can contact me at sysop@endofthelinebbs.com or sysop@1:124/5016

There are always going to be edge cases that cannot be displayed for some 
reason. Feel free to upload your document to the sysop at 
endofthelinebbs.com telnet, ssh, or rlogin, and send a netmail or email
describing the problem and I will take a look.

CHANGE LOG
Version 1.0		10 Oct 2026	Initial Release
Version 1.1		17 Nov 2026	Submitted to synchronet git
					Moved to ./xtrn/sixelview
Version 1.2		20 Nov 2026	Added sixelviewlibrary.js:
					settings.ini libraries with ARS,
					wildcard include and exclude, themes,
					text reader, ANSI viewing, ZIP
					extraction, hooks for online archives
					(Sixteen Colors) with a self-cleaning
					cache
Version 1.3		23 Nov 2026	Pictures and PDFs: JPEG 2000 and JBIG2
					decoding (including halftones),
					encrypted PDFs with a blank password,
					deeply nested page trees, better
					scaling of scanned pages, a Loading
					message and faster opening of big
					files.
					Viewer: < and > (or , and .) change
					page, links can be clicked, text-mode
					art opens at 100%, a tidier help box.
					Web libraries: forms (forms = true),
					content=, with the web code in
					http_lib.js.
					Library: clickable menu bars and mouse
					in every viewer, RAR/CBR, .desc files,
					side scrolling, error_mail, HTML
					merged into sixelviewlibrary.js, EPUB
					pictures kept as they are (much
					faster, no picture limit of 120).
					Host tool: pdfunmark.js (remove
					watermark text).
