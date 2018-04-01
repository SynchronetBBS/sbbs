ANSIView
Browse and view text files, ANSI & ASCII art on Synchronet BBS 3.15+
echicken -at- bbs.electronicchicken.com

0) Introduction
1) Prerequisites
2) Installation
3) Customization
4) Configure a local art or text file archive
	4.1) ZIP files
5) Connect your BBS to some online ANSI & ASCII art archives
6) Develop your own browser module (or, "Nobody will ever do this")

0) Introduction

	ANSIView is a browser and viewer for text files, ANSI and ASCII art.  You
	can present your users with a library of content local to your BBS, and
	also connect to multiple online archives.  ANSIView uses a lightbar user-
	interface which has been described as "okay".

	Users can turn screen pausing on and off for the duration of their
	ANSIView session, and can adjust the output speed of the displayed files.
	When SyncTERM is in use, its native output speed emulation will be
	employed.  When non-SyncTERM clients are in use, an attempt will be made
	to simulate the desired output speed using server-side techniques.

	ANSIView won't do much of anything until you configure it, so please read
	through this document in order to learn how to make ANSIView do stuff.
	Feel free to skip over section 6.

	ANSIView comes without any warranty, but if it ruins your life I promise
	to feel kinda bad about it.

1) Prerequisites

	ANSIView should work with Synchronet BBS 3.15+.

	If you're fairly certain that your system is up-to-date, just move ahead
	to the Installation section.  Bear in mind that at least one dependency,
	exec/load/filebrowser.js, is newer than the release of Synchronet 3.16c.

	Grab the latest copies of the following files from the Synchronet CVS:

		exec/load/funclib.js
		exec/load/frame.js
		exec/load/tree.js
		exec/load/scrollbar.js
		exec/load/filebrowser.js
		exec/load/http.js

	You can find them via the CVS web interface here:

		http://cvs.synchro.net/cgi-bin/viewcvs.cgi/exec/load/

	Or just use your usual CVS update method, backing up first, etc.

2) Installation

	In SCFG (BBS -> Configure from the Synchronet Control Panel on Windows),
	go to External Programs -> Online Programs (Doors), choose a section to
	add this program to, and then create a new entry in that section with
	the following details:

		Name:						ANSIView
		Internal Code:				ANSIVIEW
		Start-up Directory:			../xtrn/ansiview
		Command Line:				?ansiview.js
		Multiple Concurrent Users:	Yes

	All other settings can be left at their default values.

3) Customization

	Open the file 'xtrn/ansiview/settings.ini' with a text editor.  You'll see
	that the root section has the following default values:

		border = LIGHTBLUE,CYAN,LIGHTCYAN,WHITE
		fg = WHITE
		bg = BG_BLACK
		lfg = WHITE
		lbg = BG_CYAN
		sfg = WHITE
		sbg = BG_BLUE

	The 'border' value can be a single color-name, or a comma-separated list
	of color names.  You can find valid color names in exec/load/sbbsdefs.js.

	The other values must be single color names, and they are as follows:

		- 'fg' is the foreground color of a non-highlighted list item
		- 'bg' is the background color of a non-highlighted list item
		- 'lfg' is the foreground color of a highlighted list item
		- 'lbg' is the background color of a highlighted list item
		- 'sfg' is the foreground color of the path & status bars
		- 'sbg' is the background color of the path & status bars

	Optional settings that can be added to the root section of 'settings.ini':

		top_level - Text to use in place of the default menu name (Gallery Menu)
		pause - true/false, whether pausing is on or off by default
		speed - 300,600,1200,2400,4800,9600,19200,38400,57600,76800,115200
        header - The path to a .ans graphic file to display above the browser
        header_rows - The number of lines the header graphic occupies

	Any additional sections in 'settings.ini' will be treated as configuration
	sections for local or online art archives.

4) Configure a local art or text file archive

	The following example section of 'xtrn/ansiview/settings.ini' is commented
	out by default:

		;[ANSI Gallery]
		;description = A local archive of ANSI and ASCII artwork
		;module = local.js
		;path = /path/to/my/ansi/art/collection
		;hide = *.exe,*.com

	You can enable this section by removing the semicolons from the start of
	each line.

	To point this section at a local directory containing your archive of
	art and/or text files, edit the 'path' value accordingly.

	You can make copies of this section and point them at different directories
	in order to list multiple distinct local libraries from ANSIView's main
	menu.

	Don't modify the 'module' line unless you know what you're doing.  The
	'local.js' module invokes and configures a filesystem browser specifically
	for use with ANSIView.

	The 'hide' value is a comma-separated list of wildcard-patterns that will
	be used to identify files that should be hidden from users. These patterns
	are not case-sensitive.  The * and ? wildcards are supported.  You can add
	as many patterns to the list as you like, but it's recommended that you
	do not delete any of the default expressions.

	Each directory and subdirectory within your local archive can contain an
	'ansiview.ini' file.  This file can contain a 'descriptions' section
	which maps the names of files in that directory to a description of the
	file, like so:

	[descriptions]
	hugewang.ans = A large ANSI drawing of a Wang 2200 computer.
	boobies = A directory full of pictures of blue-footed boobies.

	The keys of the 'descriptions' section (ie. the filenames) must be in
	lowercase.

	Future updates to ANSIView may introduce other optional per-directory
	settings to ansiview.ini, such as access requirements.  The ansiview.ini
	files will be hidden from all directory listings.

	4.1) ZIP files

		When a user selects a .zip file from the list, ANSIView will check to
		see if a subdirectory already exists for that filename (minus the .zip
		extension.)  If a subdirectory is found, the file browser will jump to
		that directory.  If no subdirectory is found, ANSIView will attempt to
		extract the archive.  If extraction succeeds, ANSIView will bring the
		browser to the new subdirectory.

		This means that you can download a bunch of artpacks without having to
		unzip all of them.  ANSIView will extract them only as needed, as your
		users browse the archive.

		In order for this to work, you must have a copy of (or a symlink to)
		'unzip' in your BBS' exec/ directory.  (Most installations of SBBS on
		Windows will already have this.  On UNIX/Linux, you probably already
		have InfoZIP or compatible installed for use with Synchronet, so just
		create a symlink to 'unzip' in your exec/ directory.)

		Once extracted, .zip files are not deleted - however they will be
		hidden from future directory listings.

		If you would prefer that ANSIView not attempt to extract .zip files,
		then add '*.zip' to the 'hide' list as described above.

5) Connect your BBS to some online ANSI & ASCII art archives

	The following section of 'xtrn/ansiview/settings.ini' is commented
	out by default:

		;[electronic chicken bbs]
		;description = An online archive of ANSI and ASCII artwork
		;module = ecbbs.js
		;cache = true
		;cachettl = 86400

	You can enable access to this online archive by removing the semicolons
	from the beginning of each line of the section.

	Please consider leaving the 'cache' value set to 'true' (without quotes),
	as this will reduce the amount of traffic between your system and the
	online archive.

	If caching is enabled, each file that your user opens on the online
	archive will be downloaded and saved to your local system, and it will
	not be deleted unless you take steps to do so manually.  The caches are
	stored in 'xtrn/ansiview/.cache/'.  If they start to consume too much
	space, you can delete them without causing any problems.

	The 'cachettl' value determines the time-to-live, in seconds, for cached
	web API responses.  Data will be refreshed after this amount of time
	passes.  (Note that this applies to things like file lists, and not to
	files themselves.)  86400 seconds is 24 hours.

	Some caching is done either way, but if 'cache' is set to false, the
	cache will be cleared after your user logs off.

6) Develop your own browser module
	-or-
6) Nobody will ever do this

	Each non-root section of settings.ini describes a 'browser module'.  When
	a user selects that module from ANSIView's main menu, the script pointed
	at by the 'module' setting is loaded.  One argument is passed to that
	script, which is a JSON string of the module's section of 'settings.ini',
	with an additional property ('colors') comprised of the root section of
	settings.ini.  (See 'local.js' for a basic example of handling this
	argument.)

	The loaded script must return an instance of an object that responds to
	the following method calls:

		- .open()
			-	Open the browser and draw it in the user's terminal
		- .cycle()
			-	Perform any regular housekeeping tasks
			-	This will be called every time through ANSIView's main loop
		- .refresh()
			-	Redraw the browser on the screen
		- .getcmd(cmd)
			-	Handle a string of input from the user (usually one character)
		- .close()
			-	Close the browser and remove it from the user's terminal

	The following globals will be available to the module:

		- root
			-	The full path to ANSIView's startup directory
		- browserFrame
			-	An instance of Frame (exec/load/frame.js) which the browser
				should use as a parent frame for any of its own frames.
		- printFile(file)
			-	A function that prints a file to the user's terminal, with
				knowledge of ANSIView's current settings re: speed & pausing
			-	'file' is the full path to the file to be displayed

	For an example of returning an instance of an object from a load() call,
	see local.js, or read up on self-executing anonymous functions in JS.

	For consistency's sake, all browser modules should ideally use:

		- Frame
			- exec/load/frame.js
		- Tree
			- exec/load/tree.js
		- ScrollBar
			- exec/load/scrollbar.js

	If the module uses a path/address bar, it should get its color settings
	from JSON.parse(argv[0]).colors.sfg/sbg.

	The module's file-list Tree should take its tree.colors.fg/bg/lfg/lbg
	settings from JSON.parse(argv[0]).colors.fg/bg/lfg/lbg.
