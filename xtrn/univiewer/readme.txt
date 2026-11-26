WHAT IS UNIVERSAL VIEWER?
=========================
Universal Viewer by nelgin (and Claude) is a pure Synchronet script to convert
different graphics file formats and pdfs and display them in a sixel viewer.
It doesn't require any external conversion tools, which makes it portable
on any platform that runs Synchronet.

It was called SixelView until version 2.0.  It grew past pictures: it now
reads text, ANSI art, books, web pages and plays music too, so it has a
name that doesn't say "sixel" (Sixel graphics are still how pictures and
pages are shown).  If you are upgrading from SixelView, see UPGRADING FROM
SIXELVIEW below.

As of version 1.2 it is also an all-in-one file library browser: text files,
ANSI and ASCII art, PDFs, pictures and ZIP files, from directories on your
BBS or from online archives, all in one lightbar program.  If you are
running ansiview and a textfiles.com section, univiewerlibrary.js can
replace both of them.

CREDIT
======
Inspiration for this project came from Nightfox. I wanted to create a
viewer similar to DDMsgReader and AreaChooers so it fit in with the
color theme of my BBS.

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
  JSON XML XSD XSL XSLT RSS ATOM PLIST YAML YML TOML MARKDOWN are text too,
  and INI CFG CONF MD are colored or laid out as well (see COLORS FOR
  CONFIGURATION FILES, JSON, XML AND MARKDOWN below)

Archives (opened on demand; see below):
  ZIP RAR CBZ CBR (extracted), 7Z TAR TGZ TBZ TBZ2 TB2 TXZ TLZ TZST CPIO
  LZH LHA CAB ISO (browsed in place), FAT disk images IMG IMA DSK VFD
  (browsed in place), and single compressed files GZ BZ2 XZ LZMA ZST

Music (played through the terminal's audio; see MUSIC below):
  MP3 OGG OGA OPUS FLAC WAV AIF AIFF AIFC AU

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

HOW TO USE UNIVERSAL VIEWER TO VIEW ONE FILE:
============================================
To view a single pdf or picture, simply call univiewer with the path to the
file, for example if you want to use it in a menu:

'P': { exec: '/sbbs/xtrn/univiewer/univiewer.js ' +
       '/sbbs/xtrn/univiewer/demo/univiewer_demo.pdf' },

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

univiewer_lib.js	the viewer, plus Sixel, PNG, JPEG, GIF, BMP,
			PPM, SVG and the other picture loaders

pdf2sixel.js		converts a PDF page to Sixel (stdout, a file, or
			the terminal)

univiewer.js		Viewer for one file: PDF plus images

pdfview.js		Viewer for a single PDF file (?pdfview file.pdf
			[page])

pdfunmark.js		Host tool: removes watermark text from PDF books (see
			REMOVING WATERMARKS below).  Not used by the BBS.

univiewerlibrary.js	The library browser (see below)

univiewer_music_lib.js	The music player the library browser uses (see MUSIC)

univiewer_syntax_lib.js	Colors and lays out configuration files, JSON, XML and
			Markdown in the text reader (see COLORS FOR
			CONFIGURATION FILES, JSON, XML AND MARKDOWN)

univiewer_disk_lib.js	Reads FAT disk images (.img and so on) for the library
			browser (see the archives notes under ZIP above)

univiewer_zip_lib.js	Unpacks the old "imploded" kind of ZIP file, which
			Synchronet's own support can't read

univiewer_index.js	Builds the file name search index (see SEARCHING THE
			LIBRARIES); run it from jsexec

univiewer_search_lib.js	The index's layout and the search, used by the library
			and by univiewer_index.js

univiewer_music.js		Plays a music file or directory by itself
			(?univiewer_music <file or directory>)

http_lib.js		The web requests the browser makes (GET and POST, with
			the limits and checks described under WEB PAGES)

settings-example.ini	Example configuration for the library browser

DefaultTheme.cfg	The colors and text the library browser uses

install-xtrn.ini	Data for Synchronet's installer (see ADDING IT TO
			YOUR MENUS)

.gitignore		Keeps settings.ini and .cache/ out of git

hooks/			Small scripts that connect the browser to online
			archives, such as hooks/sixteencolors.js

demo/			Demo files to try the viewers on


THE LIBRARY BROWSER - UNIVIEWERLIBRARY.JS
=========================================
Let me walk you through how it works, because it is a lot easier to explain
by talking it through than by listing options.

When a user runs univiewerlibrary.js they see a list of "libraries".  A
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
    Press / to find text: type it and press Enter (Esc cancels, and an empty
    search clears the marks).  Capitals don't matter.  Every match on the
    screen is marked, the one you are on in a different color, and the
    header shows which match it is.  The . or > or m key goes to the next
    match and , or < or Shift-M to the previous one, wrapping round at the
    ends.  N and P
    still mean next and previous file, so there is no separate search mode.
    A match split between two screen lines by the wrapping isn't found.  It
    works on every file the reader opens, including colored JSON and XML.
    V switches a colored or laid-out file (INI, YAML, TOML, JSON, XML,
    Markdown) to the file exactly as it is, and back.

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

  - Archives are handled for you, on demand, in three ways.  Synchronet's own
    Archive support does it all, so there is nothing to install (unzip and
    unrar can optionally be named for a few old ZIPs and RARs; see below).

    ZIP and RAR files, and CBZ/CBR comic books (a .cbz or .cbr is a zip or
    rar of page images), are extracted.  The first time somebody selects
    one, it is extracted into a directory with the same name beside it
    (pack.zip becomes pack/, story.cbz becomes story/) and the user is
    taken inside.  What the list shows afterwards depends on the kind of
    file:

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

    7Z, TAR (and tar compressed with gzip, bzip2, xz, lzma or zstd: .tar.gz
    .tgz .tar.bz2 .tbz2 .tar.xz .txz .tar.zst and so on), CPIO, LZH, LHA,
    CAB and ISO disc images are browsed in place.  Selecting one shows what
    is inside as folders and files, and nothing is written to the library
    directory.  Only a file the user opens is extracted, into the cache, and
    it is then shown like any other file of its type (an XML file inside a
    CAB is laid out and colored as XML, a picture is drawn, and so on).  The
    library's include and exclude settings apply inside the archive too, so
    a library can hide what can't be viewed (exclude = *.exe,*.dll,*.efi,
    *.img).  A file that is not a viewable type is reported as unable to
    display.  A file bigger than zip_max_mb is refused.  An archive or disk
    image inside another (a DOS floppy image in an ISO, say) is extracted
    into the cache when it is selected and then browsed the same way.

    FAT disk images (.img, .ima, .dsk and .vfd: DOS floppy and hard disk
    images) are browsed in place the same way.  Synchronet's Archive support
    can't read them, so the viewer reads them itself, in JavaScript: FAT12,
    FAT16 and FAT32, long file names, and the partitions of a hard disk image
    (each shows as a folder, Partition 1 and so on).  Other file systems
    (NTFS, ext2 and so on) are not recognized, and an .img that isn't a FAT
    disk, such as a boot image, is listed and treated as any other file.

    A single compressed file (GZ, BZ2, XZ, LZMA or ZST, as in notes.txt.gz
    or data.json.xz) is opened in place: selecting it shows the file inside,
    by its own name, in the viewer for that type (a .json.gz as JSON).  It
    is unpacked into the cache and the compressed file stays in the list.

    Every extracted archive (ZIP, RAR, comic books and single compressed
    files) is checked against the size limit by the size it will have when
    unpacked.

    Names with numbers in them are sorted by the number, so page2 comes
    before page10.  The extracted directories are ordinary directories on
    your disk, so you can delete them to free space; they are made again the
    next time somebody asks for the archive.

    For this to work the library must have subdirs=true (archives aren't
    listed without it) and the BBS needs permission to write to the library
    directory.  Extraction uses Synchronet's own Archive support, so
    nothing needs installing.  The "imploded" ZIPs from the early 90s
    (common in DOS-era files and art packs), which that support can't read,
    are unpacked by the viewer's own JavaScript (univiewer_zip_lib.js), in
    folders, in archives and on disk images alike.  Some old DOS-era RARs
    can't be read either, and the unzip and unrar programs can be used for
    them and for anything else odd.  They are optional and used only if you
    name them in settings.ini:

      unzip = /usr/bin/unzip
      unrar = /usr/bin/unrar

    A program that is named is used for every ZIP (or RAR); if it fails on a
    file, or none is named, Synchronet's own support does it.  Nothing is
    looked for on the path.  If neither works the file is reported as unable
    to extract and the problem is logged.  As a safety net, archives with
    unsafe file names, more than 2000 files or more than 500 MB of data are
    refused (zip_max_files and zip_max_mb in settings.ini change those
    limits).  The user is told how many files or how much data the archive
    holds against the limit, and that the sysop has been told.  That means a
    warning in the BBS log and a netmail (file path, problem, user) to the
    sysop, or to whoever error_mail= in settings.ini names: an alias, a user
    number or an Internet address, or "none" for no mail.  Each file is
    mailed once a day.

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

Finding text.  In a PDF, a web page, an HTML file or an EPUB book (anything
that has text), press / and type what to look for, then Enter.  Capitals
don't matter.  The viewer goes to the first match from where you are, reading
on through the following pages if there isn't one on this page (the status
line shows which page it is looking at, and any key stops it).  All the
matches on a page are marked in yellow and the one you are on in orange; the
status line says "match 2/5 on page 3".  Press m for the next match and
Shift-M for the previous one.  The search wraps round at the ends of the
document, and the marks stay until you press / and give an empty answer.  It
finds text the way it is drawn on the page, so a word split by a line break
with a hyphen isn't found, and a PDF that is only a scan of pages (pictures,
no text) has nothing to find.  Pictures and music have no text.  The text of
each page is read once and kept while the file is open, so the first search
through a long book takes a little while.

In every viewer (text, ANSI art, pictures and PDFs), N moves to the next
file in the list and P to the previous one, so you can flip through a whole
directory of files without going back to the list each time.  The cursor
keys are left alone for scrolling and panning.  In a document with pages, the
< and > keys (or , and .) go to the previous and next page, PgUp and PgDn
scroll a screenful (and turn the page when you reach its end), and G goes to
a page number; when a PDF is opened on its own with univiewer.js, N and P
change pages too.  On an animated GIF, which has no pages, < and > change the
speed instead.


SETTINGS.INI
============
Everything is configured in settings.ini in this directory.  There is a
settings-example.ini that explains every option; copy it to settings.ini and
edit that (the installer does the copying for you).  The file has two parts:
some settings at the top that apply to everything, and then one section for
each library.

The top part looks like this, and everything in it is optional:

  theme=DefaultTheme.cfg
  pause=false
  speed=2400
  zip_max_mb=500
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
relative to this univiewer directory.  "desc" is the description beside the
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

A pattern that ends in / matches directories only, and the others match files
only.  So "exclude = old/,*.exe" hides every directory named old and every
.exe file, and "include = *.pdf,books/" lists only PDF files and, with
subdirs on, only the directory books.  If an include has patterns for just
one kind (files or directories), the other kind isn't limited by it:
"include = pdf" still lists every directory, and "include = books/" lists
every file in the directory it is in.  Directory patterns only matter when
subdirs is true.

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


SEARCHING THE LIBRARIES
=======================
Users can search the file names and descriptions in the libraries.  They
press / in a list: inside a library it searches that library, and in the
list of libraries it searches all the libraries they can use.  The results
are one list of files, with the name, size and date of each, and a line
under the list that says which library and folder the selected file is in
and shows its description.  Names are never shortened: Left and Right scroll
a long one sideways, as in the other lists.  Enter opens a file, N and P go
to the next and previous file of the results, / starts another search, and Q
goes back.  If a result is an archive or disk image, Enter takes the user to
its folder with the cursor on it.

A library has no / key until the sysop has built its index.  The index is
made by univiewer_index.js, which is run from jsexec, and run again now and
then when the files change; once a night is plenty.  For example, as a
Synchronet timed event:

  jsexec /sbbs/xtrn/univiewer/univiewer_index.js

Synchronet's installer (see ADDING IT TO YOUR MENUS) offers to add a timed
event, UVINDEX, that runs it every night at 4:00 am, so after the first run
by hand nothing more needs doing.

Add -q to stop it printing what it did.  With no file named it indexes the
libraries in settings.ini.  A settings file that a menu entry names (see
USING MORE THAN ONE SETTINGS FILE) is indexed by naming it, and has its own
index:

  jsexec /sbbs/xtrn/univiewer/univiewer_index.js \
      settings.ini music.ini

What goes in the index is what a user would see in the library's lists: the
same include, exclude, subdirs and file types, the descriptions from the
.desc files, and the files of the sub-directories if subdirs is true.
Online libraries (module= or url=) are not indexed.  Who may search a
library is decided by its ars= when somebody searches, not when the index is
built.

To leave a library out, put noindex = true in its section (a big library on a
slow network disk, for example).  It then has no / key, and the indexer skips
it and removes any index file it made for it before.  noindex = true at the
top of the settings file, before the first section, does that for every
library in the file: the indexer indexes nothing, and removes the index files
it made, even when it is run.  Both default to false.

The index is a plain text file for each library, in the .index directory
inside the univiewer directory.  There is no database, because a database
needs file locking, which can't be relied on over the network shares that
several hosts of one Synchronet install use (a Windows host and a Linux host
sharing the install, for example).  Each file is written whole, under
another name, and renamed when it is complete, so searches can go on while a
new index is being made, from any host.  A second run for the same settings
file is refused while one that began less than six hours ago is still going.
The BBS only needs to be able to read the directory.  The files can be
deleted at any time; the libraries just lose their / key until the indexer
runs again.  Nearly all of the time it takes is the time to read the
folders: a few seconds for tens of thousands of files on a local disk (all
of 132,000 files in five libraries took 17 seconds), but much longer over
NFS (the 32,000 music files took four minutes).  An index file is about 100
bytes for each file.

Only the first run reads every folder.  After that the indexer remembers
each folder's modification time (in a .dirs file beside each index file) and
reads again only the folders that have changed, and copies the rest from the
old index.  The second run in the test, with nothing changed, took under a
second instead of 17 for the libraries on a local disk, and 35 seconds
instead of 3 minutes for the 6,000 folders of the music library on NFS,
where each folder still costs one request to ask its time.  A folder's time
changes when a file or folder is added, removed or renamed in it, but not
when a file is edited in place, so the size and date shown for a file that
was only edited stay as they were.  A .desc file that is changed is noticed.
Add -full to read everything again (see below).  Everything is also read
again when the library's dir, include, exclude or subdirs setting has
changed, when the .dirs file is damaged or doesn't belong to the index file,
and for any folder changed within ten minutes before the last run began (a
change made while that run was reading might not show in the time: a file
server's clock can be some seconds out, and FAT disks and some network
shares show times only to two seconds).

To make the indexer read every folder again on request, add -full:

  jsexec /sbbs/xtrn/univiewer/univiewer_index.js -full

You may want to do that, perhaps once a week, only if files in your
libraries are edited in place (replaced by a newer version with the same
name): the nightly run doesn't notice that, so the size and date shown in
the search results stay as they were until a -full run.  Names, descriptions
and the files found are never affected.  The sysop can make a weekly run a
second timed event in SCFG (External Programs, Timed Events) with the
command ?univiewer_index.js -q -full and the start-up directory
../xtrn/univiewer/, at a time that is not that of the nightly one.  Most
libraries don't need it.

The indexer refuses to start while another run for the same settings file is
still going, so give the two events times far enough apart.

The indexer writes each new index under another name and renames it over the
old one when it is complete.  It deletes what an earlier run that was
stopped left behind (files ending in .new) at the start, and again at the
end of every run, whatever happened, and it removes its lock file.  The
files in .index can all be deleted safely: the next run just makes them
again.

How a search works.  The words typed are looked for in the file names and
the descriptions, ignoring capitals, and every word must be found.  A word
can be part of a longer one, so "cat" finds "category", and "at" finds
"cat".  No word is left out for being common.  Instead the files are put in
order of how good the match is: a file with the word on its own in the name
first, then one whose name starts with it, then one with it inside a name,
and last one that has it only in the description.  At most 500 files are
listed (the sysop can change that with search_max in settings.ini, up to
10000), and the title says how many there were in all ("first 500 of 3120"),
so a very short word like "at" shows the best matches and a longer or second
word narrows it down.  A file that has been removed since the index was
built is reported when it is opened.

A search reads one library's index file into memory at a time (a library of
60,000 files needs about 30 MB while it is searched) and looks for the
longest word with the fast built-in text search, so a search of one big
library takes a few tens of milliseconds, and a search of all five libraries
in the test took under a second, even for a single letter.

HOOKS - ONLINE ARCHIVES
=======================
A library doesn't have to be a directory on your disk.  If instead of "dir"
you give it a "module", the browser asks a small script called a hook to
fetch the listing from a remote server.  The hook scripts live in the
hooks directory.  The one that ships with Universal Viewer is for the Sixteen
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

COLORS FOR CONFIGURATION FILES, JSON, XML AND MARKDOWN
======================================================
The text reader colors these so they are easier to read:

  INI CFG CONF   comments, [sections], keys, and values (numbers and
                 true/false/yes/no/on/off get their own color)
  YAML YML       comments, keys, strings, numbers and true/false/null, list
                 dashes, anchors and tags, and the text of | and > blocks
  TOML           comments, [tables], keys, strings (including multi-line
                 ones), numbers, dates and true/false
  JSON           keys, strings, and numbers/true/false/null
  XML XSD XSL XSLT RSS ATOM PLIST
                 tags (cyan), attribute names (yellow), attribute values,
                 comments, CDATA and entities such as &amp;
  MD MARKDOWN    headings, **bold**, *italic*, `code`, code blocks, links,
                 quotes, lists and rules

JSON, XML and Markdown are also laid out again, so a file saved on one long
line is readable.  JSON and XML get two-space indents.  Nothing in the file
is changed except spaces and line breaks between items: numbers, strings and
the order of keys stay exactly as written.  In XML, text mixed in with tags
(as in XHTML) is left as it was written, because spaces matter there.
Markdown is shown as it is meant to be read: the marks are taken off and the
text is colored instead (a link shows as its text followed by its address).
Tables, and anything else Markdown can do that isn't listed above, are shown
as they were written.

Press V to switch between this view and the file exactly as it is.  The
header at the top says which one you are looking at, for example "(V)iew:
Formatted" (or Rendered or Colored) or "(V)iew: As is", and only appears for
files that have a second view.  The menu at the bottom doesn't change.

A file that is not valid JSON or XML (a missing bracket or closing tag, say),
or is larger than the limit (4 MB for XML, YAML, TOML and INI, 1 MB for JSON
and Markdown, which take much more memory to lay out), is shown exactly as it
is, with no colors and no changes.  Pure JavaScript does all of this, so
there is nothing to install.  The colors are the five syntax entries in the
text reader section of the theme file (syntaxCommentColor and so on; see
COLORS). Markdown uses the same five: headings are the tag color, bold and
bullets the number color, italic and link text the key color, code the
string color, and quotes, rules and link addresses the comment color.  A
library that has an include setting needs these types added to it
(*.json,*.xml,*.yaml and so on); one with no include already lists them.

COLORS - THE THEME FILE
========================
Colors are not in settings.ini.  They are in a separate theme file, named
by the "theme=" line in settings.ini.  DefaultTheme.cfg is the one that
comes with Universal Viewer and uses the same look as DDMsgReader: cyan text,
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
color of text in the reader (textBodyColor).  The five syntax...Color entries
color configuration files, JSON, XML and Markdown (see the section before
this one), and searchMatchColor and searchCurrentColor mark the text found
with /.

At the bottom of the theme file are a few pieces of text, like the message
shown when a library is empty.  Those can contain \x01 followed by a color
letter if you want colors inside the message, for example \x01h\x01y for
bright yellow.  Where you see %s, the program puts something in, such as the
error that happened.

The scrollbar characters are written as \xb0 and \xdb, the CP437 character
codes, so you can change them to whichever characters you prefer.


ADDING IT TO YOUR MENUS
=======================
The easiest way to add it as an online program is Synchronet's installer.
From your exec directory (or with ;exec ?install-xtrn from the terminal) run:

  jsexec install-xtrn ../xtrn/univiewer

It adds the UVLIB online program (the second way below) and copies
settings-example.ini to settings.ini if you don't have one yet; an existing
settings.ini is kept.  It also offers to add a timed event, UVINDEX, that
rebuilds the file name search index every night at 4:00 am (see SEARCHING
THE LIBRARIES); change the time in SCFG, External Programs, Timed Events if
you like.  It can't make Universal Viewer your text file section, so for
that use the first way below.

There are three ways to give users access.  If your Synchronet is a modern
one (3.18 or later), the easiest manual way is the first.

The first is to make it your text file section.  In SCFG go to System,
Loadable Modules and choose Text Section.  This is the module Synchronet
runs whenever a user chooses the text files section, which on the default
menus is the G key, so there is nothing to edit in default.js and nothing
to add to your menu screens.  Set the command to:

  ../xtrn/univiewer/univiewerlibrary.js

and, if you want, put an access requirement on it such as LEVEL 50.
Synchronet runs the first module whose requirements the user meets, so you can
leave a fallback behind it for users who don't qualify.  Remember to give the
libraries their own ars lines in settings.ini as well.  If you don't have a
Loadable Modules entry in SCFG (older Synchronet), use one of the next two
ways.

The second is as an online program (door).  In SCFG go to External Programs,
Online Programs, pick a section and add:

  Name:                        Universal Viewer Library
  Internal Code:               UVLIB
  Start-up Directory:          ../xtrn/univiewer
  Command Line:                ?univiewerlibrary.js
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
for Universal Viewer with a key nobody else is using.  The format is the same
as the entries around it, for example the one that logs a user off:

  'O': { eval: 'logoff(/* fast: */false)' },

and Universal Viewer's is:

  'Z': { exec: '/sbbs/xtrn/univiewer/univiewerlibrary.js' },

(the quotes and the comma at the end matter).  You can use any key that
isn't taken; look through the existing entries to see what is free.  If you
are using this to replace ansiview and your textfiles section, you could
reuse their keys and remove their lines.

The new key won't show up on the menu screen by itself.  Edit your main
menu display, in /sbbs/text/menu/ (the main.* files, whichever you use), and
add the new key and name where you want it.


USING MORE THAN ONE SETTINGS FILE
=================================
By default univiewerlibrary.js reads settings.ini.  If you give it the name of
an .ini file as its first argument, it reads that file instead:

  ?univiewerlibrary.js music.ini

The name is looked for in the univiewer directory unless it is a full path,
and ".ini" is added if you leave it off.  The file is laid out exactly like
settings.ini (a theme and [library:...] sections).

Why: the same script runs wherever you put it on the menus, and a single
settings.ini gives every one of those places the same list of libraries.
With a file per menu entry you can have, for example, a Text Files section
that lists your textfiles, a Music entry that lists only your music
directories, and a Documents entry for PDFs, each with its own libraries,
access requirements and even its own theme, and all of them still run the
one copy of the program.  Use it in the Command Line of an online program
(see the second way above), in the exec line of a command shell key, or after
the module name in the Text Section command in SCFG:

  ../xtrn/univiewer/univiewerlibrary.js music.ini

Synchronet runs the first Text Section module whose access requirements the
user meets, so you can give different groups of users different settings
files that way.

MUSIC
=====
Choosing an MP3, Ogg Vorbis, Opus, FLAC, WAV, AIFF or AU file in a library
plays it, using the audio support in SyncTERM.  It needs nothing but
Synchronet: the BBS reads the file's tags and length itself and sends the
file to the terminal, which decodes and plays it.  Terminals that can't play
audio are told so.  MP3 also needs libsndfile 1.1 or later on the user's
computer.  Titles and artists are shown for MP3, Ogg, Opus, FLAC and AIFF
files that have them.

The screen shows the title, artist, album and format, a progress bar with the
time, a volume bar and a menu on the bottom line:

  S       Stop or start the song (Space too); a stopped song starts over
  N / P   Next and previous track.  P goes back to the beginning of the
          song first if it has been playing for more than a few seconds.
          The tracks are the music files in the same directory.
  Up/Down Volume (+ and - too); M mutes
  A       Autoplay on or off: with it on, the next song starts when one
          ends.  It is off to begin with.
  Q       Quit (Ctrl-C and a right click do too)

Every item on the bottom line, the buttons and the volume bar can be clicked.

Why it is limited: SyncTERM can't seek, pause or say where it is in a song,
so there is no skipping ahead and a stopped song starts again from the
beginning.  The position shown is the player's own clock.  Each song is sent
once when it starts, up to 64 MB (less on a BBS with a small JavaScript heap).

SENDING SMALLER FILES: a song is sent to the user's terminal whole, so a big
FLAC or 320 kbps MP3 takes a while on a slow connection (the bottom line shows
the size and, after the first song, a time estimate).  If you set music_kbps
in settings.ini:

  music_kbps = 128

then any song with a higher bit rate is sent as a smaller Opus copy at about
that rate.  This and the unzip and unrar settings for archives (see ZIP
above) are the only places an outside program is used, and all are
optional.  ffmpeg (with libopus) is optional but highly recommended: the
music player works fully without it, but with it songs are a fraction of the
size, which saves bandwidth and gets them to slow connections much sooner.
FLAC files and high bit rate MP3s benefit the most.  It is never used unless
you set music_kbps.
Why ffmpeg: re-encoding music inside Synchronet's JavaScript would take
minutes per song.

ffmpeg is looked for in the exec directory and then on the path, but the BBS
often runs with a shorter path than your own login, so you can name it:

  ffmpeg = /usr/bin/ffmpeg

(a full path, with no quotes; spaces are fine but shell characters such as
$ or ; are not).  If that file isn't there, nothing is converted and an error
is written to the Synchronet log.

The copy is made the first time someone plays the song (the user sees "making
a smaller copy"), kept in .cache/music/ and cleaned up by cache_max_days and
cache_max_mb like the other cached files.  Songs that are already small,
files whose names contain unusual characters, and any song ffmpeg can't
convert are sent as they are.  Titles and artists are kept; the screen shows
"Opus (from FLAC)" or similar.  Leave music_kbps out to always send the
original files.

music_kbps can be set for all libraries (at the top of settings.ini) or in
a single [library:...] section, where it overrides the top one.  For example,
set 128 at the top for everything and put music_kbps = 0 in a library of
small files, or leave the top alone and set it only in a library that holds
FLAC.  ffmpeg = is always global.

To make a music section, add a library to a settings file, for example:

  [library:Music]
  dir = /sbbs/xtrn/music

and, if you like, keep it in its own file (see USING MORE THAN ONE SETTINGS
FILE above).

UPGRADING FROM SIXELVIEW
========================
Version 2.0 renamed everything, so a BBS that already runs SixelView needs a
few changes.  The program is the same.  Your settings.ini and the theme file
work as they are.

The directory xtrn/sixelview is now xtrn/univiewer, and the files in it:

  sixelview.js          univiewer.js
  sixelview_lib.js      univiewer_lib.js
  sixelviewlibrary.js   univiewerlibrary.js
  sixelmusic.js         univiewer_music.js
  sixelmusic_lib.js     univiewer_music_lib.js
  demo/sixelview_demo.* demo/univiewer_demo.*

The other files (pdf2sixel*.js, pdfview.js, pdfunmark.js, inflate.js,
http_lib.js, DefaultTheme.cfg, hooks/) keep their names.

  1. Put the new files in xtrn/univiewer.
  2. Copy your settings.ini from xtrn/sixelview to xtrn/univiewer (and your
     own theme file, if you made one).  The .cache directory can be copied
     too, or just left behind to be rebuilt.
  3. Change everything that points at the old files: the Text Section
     command in SCFG (../xtrn/univiewer/univiewerlibrary.js), the command
     line and start-up directory of an online program (the installer's one is
     now called UVLIB), the exec lines of command shell keys, and any menu
     that runs sixelview.js.
  4. Once it works, delete xtrn/sixelview.  The directories named in
     settings.ini are not affected, because they are not inside it.

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
Universal Viewer keeps the whole file in memory (about four times its size)
while it is open, so very large scans also strain the BBS.  You can convert
such files once, on the host and not in Universal Viewer, so that it only has
to read plain pictures:

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
from ...") you can remove it.  This is a host tool, run from the univiewer
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
					settings-example.ini has a demo
					library active, and its two stray
					lines are now comments.
					Host tool: pdfunmark.js (remove
					watermark text).
Version 1.4		23 Nov 2026	Install: install-xtrn.ini (run
					jsexec install-xtrn ../xtrn/sixelview)
					and a .gitignore for settings.ini.
Version 1.5		23 Nov 2026	Music: MP3, Ogg Vorbis, Opus, FLAC,
					WAV, AIFF and AU play in SyncTERM
					(sixelmusic_lib.js, sixelmusic.js):
					next, previous, volume, mute,
					autoplay option, keyboard and mouse.
					music_kbps= sends big songs as a
					smaller Opus copy made with ffmpeg
					(named by ffmpeg=, or found on the
					path), for slow connections; it can
					be set per library too.
					Library: an .ini file named on the
					command line replaces settings.ini
					(?sixelviewlibrary.js music.ini),
					so each menu entry can have its own
					libraries.  include and exclude:
					a pattern ending in / matches
					directories only.
Version 2.0		24 Nov 2026	Renamed from SixelView to
					Universal Viewer, since it does
					far more than Sixel now.  The
					directory is xtrn/univiewer and the
					files are univiewer*.js (see
					UPGRADING FROM SIXELVIEW).  The
					installer's online program is now
					UVLIB.
Version 2.1		25 Nov 2026	Text reader.  INI, CFG, CONF, YAML
					and TOML files are colored.  JSON
					and XML (and XSD, XSL, XSLT, RSS,
					ATOM, PLIST) are laid out with
					indents and colored.  Markdown is
					shown with its marks removed, and
					colored.  V switches to the file
					as it is, and the header says
					which view it is.  A file that is
					not valid, or is too big (4 MB; 1
					MB for JSON and Markdown), is
					shown as it is.  Text files up to
					4 MB open in the reader (it was 2
					MB).  New theme colors
					syntaxCommentColor,
					syntaxKeyColor, syntaxTagColor,
					syntaxStringColor and
					syntaxNumberColor.
					/ finds text in the reader (. , >
					< m and Shift-M for the next and
					previous match).  New theme colors
					searchMatchColor and
					searchCurrentColor.
					Very long lines (minified JSON)
					now wrap in a fraction of a second
					instead of a minute.
Version 2.2		25 Nov 2026	Pictures and PDFs.  / finds text
					in PDFs, web pages, HTML files and
					EPUB books, marking every match on
					the page; m and Shift-M go to the
					next and previous match, across
					pages.
					The viewer no longer redraws when
					a key (Down at the last page, say)
					doesn't move anything.
					A standard font such as Times-
					Roman with no encoding named now
					uses StandardEncoding, so the fi
					and fl ligatures and curly quotes
					draw correctly.
					pdf2sixel_lib.js: the full version
					is back (progressive JPEG, JPEG
					2000, JBIG2, animated GIF
					encoding), after an update had
					replaced it with an older copy.
					Scanned pages and animated GIFs
					draw correctly again.
Version 2.3		25 Nov 2026	Archives and disk images.  7Z, TAR
					(also .tar.gz, .tar.bz2, .tar.xz
					and so on), CPIO, LZH/LHA, CAB,
					ISO and FAT disk images (.img .ima
					.dsk .vfd) are browsed in place:
					only a file that is opened is
					extracted, into the cache.  An
					archive or disk image inside
					another is browsed too.  The disk
					images are read by the viewer's
					own JavaScript.
					A single GZ, BZ2, XZ, LZMA or ZST
					file is opened in place, as the
					file inside it.
					Old "imploded" ZIP files (PKZIP
					1.x) are unpacked in JavaScript
					when Synchronet's Archive support
					can't.
					unzip= and unrar= (optional) name
					programs to extract ZIP and RAR
					files with; if one isn't named, or
					fails, Synchronet's own Archive
					support does it.
					zip_max_mb now defaults to 500,
					and for archives browsed in place
					it is the largest single file
					opened.
Version 2.4		25 Nov 2026	File name search.  / searches the file
					names and descriptions of a library
					(or of all the libraries, from the
					list of them).  The sysop builds the
					index, plain text files in .index,
					with univiewer_index.js from jsexec
					now and then.  Results are put in
					order of how good the match is, so
					common words don't swamp them.
					search_max sets how many files a
					search lists (default 500).
					noindex = true (in a library, or at
					the top of settings.ini for all of
					them) turns indexing off.
					After the first run the indexer
					reads only the folders that have
					changed (-full reads them all), and
					it cleans up after itself.  The
					installer can add a nightly timed
					event, UVINDEX, to run it.
