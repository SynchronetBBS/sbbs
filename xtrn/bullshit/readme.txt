Bullshit 2.0
-=-=-=-=-=-=
by echicken -at- bbs.electronicchicken.com

Bullshit is a bulletin listing/reading module for Synchronet BBS 3.16+.  It
uses a sysops-only message base on your BBS as a storage back-end.  Adding
new bulletins to your system becomes as easy as posting a message.


Installation:
-=-=-=-=-=-=-

Launch SCFG (BBS->Configure in the Synchronet Control Panel on Windows.)

In 'Message Areas', select your local message group, and create a new sub with
the following details:

Long Name                  Bulletins
Short Name                 Bulletins
QWK Name                   BULLSHIT
Internal Code              BULLSHIT
Access Requirements        LEVEL 90
Reading Requirements       LEVEL 90
Posting Requirements       LEVEL 90

In the "Toggle Options" for the sub, ensure that "Default on for new scan",
"Forced on for new scan", and "Default on for your scan" are set to 'No'.

(You can set the Access, Reading, and Posting requirements to any ARS value
that you want.  Ideally, only the Sysop should be able to see or post to this
sub, and Bullshit will be the interface by which users view messages here.)

Return to the main menu of SCFG, go to 'External Programs', then to
'Online Programs (Doors)'.  Select whatever externals section you want to
place Bullshit in ('Main' might be a good choice) and then add a new item
with the following details:

Name                       Bullshit
Internal Code              BULLSHIT
Start-up Directory         /sbbs/xtrn/bullshit
Command Line               ?bullshit.js
Multiple Concurrent Users  Yes

If you want Bullshit to run during your logon process, set the following:

Execute on Event			logon

Next you can edit 'bullshit.ini' to your liking.  It contains some options for
colors and such.

Now go ahead and start posting messages in the Bulletins sub-board that you
created.  The subject line of each message will appear as the bulletin title,
and the message body will be the text of the bulletin.