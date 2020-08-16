Avatar Collections
==================
$Id: README.TXT,v 1.1 2018/01/14 19:22:52 rswindell Exp $

This directory (text/avatars) is used to store collections of avatars which may
be used by the users of this BBS as the source of their personal user avatar.
Once an avatar is selected by the user, they can edit it to their liking and
even download it for upload and use on other supporting BBSes.

Avatar collections may be:
  - created or modified by the local sysop (e.g. using Pablodraw)
  - imported-from or exported-to a networked message area
    (e.g. SYNCDATA on DOVE-Net or FidoNet)
  - downloaded or updated from the Synchronet CVS repository

For instructions and technical details about the Synchronet Avatars module, see
http://wiki.synchro.net/module:avatars

Artists
-------
If you're an ANSI/TextGraphics artist and would like to contribute your
modifications or additions to any of the stock (from CVS) collections or you
have collections of your own that you would like to contribute, please send
them to rob{at}synchro{dot}net or coordinate via #synchronet at irc.synchro.net

Sysops
------
If have an avatar collection of your own that you would to share with other
Synchronet sysops, just name your shared collections <QWK-ID>.*.bin (and place
them in the text/avatars directory) and add the '-share' option to the
avatars.js "export" command-line (usually the AVAT-OUT timed event in SCFG->
External Programs->Timed Events). Doing this will automatically export
(to the SYNCDATA message area) your shared collection(s) any time they are
updated and re-export them at the configured ''update_freq'' (set in your
ctrl/modopts.ini file).

When your shared collections are imported into participating BBSes, they will
be copied into their text/avatars directory and the filename, with your QWK-ID
prefix, will remain in tact.

Requirements
------------
In order for shared collections to be successfully exported and imported, they
must first pass the "avatars.js verify" test:

- collections must be in "BinaryText" (.bin) format
- collections must contain a valid SAUCE record
- the file "width" (columns) value must be 10
- the filesize (minus SAUCE) must be evenly divisible by 120 (10 x 6 x 2)
- the characters used in avatars may not contain specific ctrl chars or 255
- the attributes used in avatars may not contain the BLINK attribute (0x80)
- ICE colors are not supported at this time, but are under consideration
  (your feedback on this matter is appreciated)
  
Shared collections should not:

- contain duplicate avatars from other stock or shared collections  
- contain artwork taken from another party without their permission
- contain profanity or highly objectionable images (so subjective, I know)
  
/* End of File */