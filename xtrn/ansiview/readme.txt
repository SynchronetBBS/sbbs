ansiview.js
by echicken -at- bbs.electronicchicken.com
February 15, 2012

You can add ansiview.js as an external program via SCFG, or simply launch it
from a javascript shell with bbs.exec("?/sbbs/xtrn/ansiview/ansiview.js"); 
(your path may vary.)

By default, ansiview.js looks in its own directory (eg. /sbbs/xtrn/ansiview)
for a subdirectory called "library" (eg. /sbbs/xtrn/ansiview/library).  This
is the root directory of your ANSI library, and can contain an entire tree of
subdirectories containing ANSI/ASCII files.  You can specify another location
and change a few other configuration options by editing some values in the
"Configuration variables" section at the top of ansiview.js.

ansiview.ans and help.ans are the graphics for the file browser screen and
the help screen, respectively.  You can modify or replace them as you like,
but bear in mind that if your new design changes the location of the path
bar or the location and dimensions of the file list, you will need to edit
ansiview.js to work within the new boundaries that you've set.


