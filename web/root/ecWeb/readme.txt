readme.txt, from ecWeb v2 for Synchronet BBS 3.15+
by Derek Mullin (echicken -at- bbs.electronicchicken.com)
---------------------------------------------------------

Installating ecWeb v2:
----------------------

Make a backup of your Synchronet's web document root (typically /sbbs/web/root)

Check your Synchronet installation for the following files and folders:

/sbbs/web/root/ecWeb/
/sbbs/ctrl/web.ini
/sbbs/exec/load/webInit.ssjs

If any of the above are missing from your system, you can check them out from
the CVS at cvs.synchro.net.

If you would like ecWeb to be your default web interface, you can place the
content of the ecWeb folder at the top level of your webserver's document root.
You could also change the RootDirectory value under [Web] in sbbs.ini; if so,
you'll want to copy the 'error' directory from the former docroot into the
ecWeb folder.

Edit /sbbs/ctrl/web.ini to your liking.  There are plenty of comments in the
file to help you along.

I'm adding more comprehensive documentation to the wiki. This should be enough
to get you started.
