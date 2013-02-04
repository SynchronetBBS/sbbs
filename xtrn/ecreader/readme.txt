ecReader for Synchronet 3.16+
echicken -at- bbs.electronicchicken.com, 2012

A lightbar message browser/reader/poster for Synchronet BBS, with threaded
sub-board views and many other features.

Disclaimer:
-----------
This script works fine on my system, but it can consume a fair bit of memory.
Always keep your BBS backed up just in case something happens, like some
crazy message reader causing your system to slow down and crash, pooping on
all of your files when it does.

Requirements:
-------------
An up-to-date Synchronet installation, with recent copies of:
exec/load/frame.js
exec/load/tree.js
exec/load/msgutils.js

Installation:
-------------

In SCFG (BBS->Configure in the Synchronet Control Panel on Windows), go to
External Programs->Online Programs (Doors), and then select the area you wish
to add ecReader to.  Create a new entry with the following details:

¦Name                       ecReader
¦Internal Code              ECREADER
¦Start-up Directory         ../xtrn/ecreader
¦Command Line               ?ecReader.js
¦Multiple Concurrent Users  Yes

Leave all other options at their default settings.

You can call this external via javascript with:

bbs.exec_xtrn("ECREADER");

Configuration:
--------------
You can change the appearance and behavior of ecReader on a system-wide basis
by editing the following variables at the top of ecReader.js:

var showMail = true;	// Allow access to the private 'mail' sub-board
var threaded = true;	// False to default to flat view
var setPointers = true; // False to leave scan pointers unaffected by reading
var oldestFirst = false;// False to list messages in descending date order
var lbg = BG_CYAN;		// Lightbar background
var lfg = WHITE;		// Foreground colour of highlighted text
var nfg = LIGHTGRAY;	// Foreground colour of non-highlighted text
var xfg = LIGHTCYAN;	// Subtree expansion indicator colour
var tfg = LIGHTCYAN;	// Uh, that line beside subtree items
var fbg = BG_BLUE;		// Title, Header, Help frame background colour
var urm = WHITE;		// Unread message foreground colour
var hfg = "\1h\1w"; 	// Heading text (CTRL-A format, for now)
var sffg = "\1h\1c";	// Heading sub-field text (CTRL-A format, for now)
var mfg = "\1n\1w";		// Message text colour (CTRL-A format, for now)

I will probably move these settings out into an INI file later on, and allow
some of them to be configured on a per-user basis.

Support:
--------
email: echicken -at- bbs.electronicchicken.com
  irc: #synchronet on irc.synchro.net