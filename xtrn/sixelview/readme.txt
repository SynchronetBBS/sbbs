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


HOW TO USE SIXELVIEW TO VIEW ONE FILE:
=====================================
To view a single pdf or picture, simply call sixelview with the path to the
file, for example if you want to use it in a menu:

'P': { exec: '/sbbs/xtrn/sixelview/sixelview.js /sbbs/xtrn/sixelview/text/sixelview_demo.pdf' },

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

sixelview_lib.js	the viewer, plus Sixel, PNG, JPEG, GIF, BMP and
			PPM loading

pdf2sixel.js		converts a PDF page to Sixel (stdout, a file, or
			the terminal)

sixelview.js		Universal viewer: PDF plus images

sixelviewlibrary.js	The library browser (see below)

settings-example.ini	Example configuration for the library browser

DefaultTheme.cfg	The colors and text the library browser uses

hooks/			Small scripts that connect the browser to online
			archives, such as hooks/sixteencolors.js

text/			Some demo files you can try

There are some files in the text directory that you can use as a demo.


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

What happens when they open a file depends on what it is:

  - PDFs and pictures (pdf png jpg jpeg gif bmp ppm pgm six) go to the
    Sixel viewer, with scrolling and zooming, if their terminal supports
    Sixel (SyncTERM does).

  - Plain text files (txt asc msg diz nfo bbs) open in a text reader.  Up
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

  - ZIP files are unpacked for you.  The first time somebody selects a ZIP,
    it is extracted into a directory with the same name beside it (pack.zip
    becomes pack/) and the user is taken inside.  After that the ZIP is
    hidden from the list and the directory is shown instead, exactly like
    ansiview does.  This uses Synchronet's own Archive support, so unzip is
    not normally needed.  A few old ZIPs (the "imploded" kind from the early
    90s, common in art packs) can't be read that way; for those it falls back
    to the unzip program in /sbbs/exec if you have one (on Linux, a symlink to
    /usr/bin/unzip is enough), and cleans up after itself if both fail.  For
    this to work the library must have
    subdirs=true and the BBS needs permission to write to that directory.
    As a safety net, archives with unsafe file names, more than 1000 files
    or more than 50 MB of data are refused.

The keys, in any list:

  Up/Down, PgUp/PgDn, Home/End    move around
  Enter                           open the item
  Q or Esc                        go back
  S                               cycle the output speed for ANSI files
                                  (Full, 300 ... 115200)
  P                               turn screen pausing on and off
  I                               (online libraries only) show the picture
                                  of the ANSI file instead of the ANSI itself
  digits and letters              jump to a library number, or to the next
                                  file starting with that letter

In the PDF and picture viewer the bottom line shows the keys that matter, with
the key letter in yellow: (C)ontents, (N)ext/(P)rev File, (Q)uit and (H)elp.
Press H (or ?) for a pop-up box that lists every key that applies to what you
are looking at, grouped under moving, zoom, pages, contents and links, and
files.  Any key closes it.

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
keys are left alone for scrolling and panning.  Because P and N are used for
this, the pages of a PDF opened from the library are changed with PgUp and
PgDn (or G to go to a page); when a PDF is opened on its own with
sixelview.js, N and P still change pages.


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
  zip_max_files=1000
  image_max_mb=3
  cache_max_days=14
  cache_max_mb=100

"theme" is the file that holds all the colors (see the next section).
"pause" and "speed" are the starting values for the P and S keys, and users
can still change them while they browse.  They only affect ANSI and ASCII art;
text files open in the scrolling reader instead.  The zip_ settings are the limits
for extracting archives.  image_max_mb is the biggest picture an online
library may show.  The cache settings are explained under hooks, below.

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

Whatever patterns you use, only file types the viewers understand are shown
(the ones listed above, plus zip), so you don't need to exclude every .exe
under the sun, but it doesn't hurt either.

"sort" can be name, date or size, and you can put a minus sign in front to
reverse it, so "sort = -date" gives newest first.  The default is name.

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
(number, name, description, type, size, date, directory), the selected row
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

and, if you want, put an access requirement on it such as LEVEL 50.  Synchronet
runs the first module whose requirements the user meets, so you can leave a
fallback behind it for users who don't qualify.  Remember to give the
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

(If /sbbs/mods/default.js already exists, you've customised it before, so
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

JPEG 2000, JBIG2 and progressive JPEG pictures are shown as grey boxes;
password protected PDFs aren't supported; PNG must be non-interlaced, BMP
uncompressed and GIF shows the first frame only.

XBin and BIN files are not supported in the library browser (ansiview does
them if you need them).  ansiview's per-directory ansiview.ini description
files are not read.

An online library downloads each picture in full before it is shown, and
only checks the size afterwards.

This set of programs comes with absolutely no support and warranty other than
it works fine on my system. If you have any suggestions or improvements,  you
can contact me at sysop@endofthelinebbs.com or sysop@1:124/5016

CHANGE LOG
Version 1.0		10 Oct 2026	Initial Release
Version 1.1		17 Nov 2026	Submitted to synchronet git
					Moved to ./xtrn/sixelview
Version 1.2		20 Nov 2026	Added sixelviewlibrary.js: settings.ini
					libraries with ARS, wildcard include
					and exclude, themes, text reader, ANSI
					viewing, ZIP extraction, hooks for
					online archives (Sixteen Colors) with
					a self-cleaning cache

