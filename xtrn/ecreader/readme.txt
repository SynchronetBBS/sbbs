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
Installation will depend on your command shell.  Some understanding of how to
edit a command shell and add or modify a hotkey triggered event is required.

To launch ecReader from a Javascript command shell:

bbs.exec("?/sbbs/xtrn/ecreader/ecReader.js");

Support:
--------
email: echicken -at- bbs.electronicchicken.com
  irc: #synchronet on irc.synchro.net
  
To Do:
------
- Update scan pointers when new messages are read
- New message scan
- Allow browsing of the 'mail' sub-board