ansiview.js - An ANSI art browser for Synchronet BBS v3.15+
by echicken -at- bbs.electronicchicken.com

You can add  ansiview.js  as an external program  via SCFG,  or simply launch it
from a Javascript shell with bbs.exec("?/sbbs/xtrn/ansiview/ansiview.js"); (your
path may vary.)

By default, ansiview.js looks in its own directory (eg. /sbbs/xtrn/ansiview) for
a subdirectory called "library" (eg. /sbbs/xtrn/ansiview/library).   This is the
root  directory  of  your  ANSI  library,  and  can  contain an  entire tree  of
directories containing ANSI/ASCII files or ZIP artpacks. You can specify another
location by  editing  the ansiRoot  variable at the top of  ansiview.js.   Other 
configurable variables are  present there  that will let you disallow files with 
certain names and extensions from being opened and to change the look and output
speed of this script.

ansiview.ans and  help.ans are the graphics  for the file browser screen and the 
help screen, respectively.  You can modify or replace them as you like, but bear 
in mind  that if  your new  design changes  the location  of the path bar or the 
location and  dimensions of the file list,  you will need to edit ansiview.js to 
work within the new boundaries that you've set.   I can make it easier to change
the size and location of the path bar and file list if there's demand for it.

If a ZIP archive is  selected in the file browser,  ansiview.js will check for a 
directory  matching the archive's  filename  less the ".zip"  extension.   If no
matching  directory is found,  the archive will be unzipped into a new directory
named as described above.  In either case,  the file browser will then load that
directory. This feature might make it easier to use ansiview.js with an existing 
collection of ANSI art residing in your BBS' file libraries.

If you need support, post a message addressed to me in DOVENet Synchronet Sysops
or DOVENet Synchronet  Programming (Javascript),  look for me in  #synchronet on
irc.bbs-scene.org / irc.synchro.net,  or send an email to the address at the top
of this file.
