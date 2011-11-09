readme.txt, from ecWeb v2 for Synchronet BBS 3.15+
by Derek Mullin (echicken -at- bbs.electronicchicken.com)
---------------------------------------------------------

About
-----

ecWeb is a drop-in replacement for the stock Synchronet web interface. It's
meant to be easy to customize and add content to, and features a thread-sorted
interface to your BBS' message bases. Installation is easy - most people will
only need to make one or two small edits to a couple of files in order to get
ecWeb up and running on their board.

I'm using ecWeb at http://bbs.electronicchicken.com/ if you want to take a look
at it.  Bear in mind that I also do a certain amount of testing there, so don't
be shocked if strange things happen from time to time while you browse. :|

ecWeb v2 is still quite new as of the time of this writing (August 16, 2011.)
It's only been out for a day or so now, and I'm busy improving things and
taking care of bugs as they crop up.  Early adopters should be prepared for
some hiccups (that said, it's running smoothly for most people so far.)


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
file to help you along, so please read them.

ecWeb includes fTelnet and lightIRC (thanks to Rick and Valentin respectively,)
both of which require a flash socket policy server to be running on the host
to which they will connect. If your Synchronet installation is up to date,
you already have flashpolicyserver.js. Make sure that it's enabled in your
ctrl/services.ini file, that port 843 is open to your BBS, and add port 6667
to the extra_ports key in the [flashpolicyserver] section of ctrl/modopts.ini.

I'm adding more comprehensive documentation to the wiki. This should be enough
to get you started.


Support:
--------

Assuming you've followed the above instructions, I'll be happy to help you. You
can get in touch with me in one of the following ways:

- Post a message to me (echicken) in the Synchronet Sysops sub on DOVE-Net
- Find me in #synchronet on irc.bbs-scene.org
- Send an email to the address at the top of this file
