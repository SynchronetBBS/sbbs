Gooble Gooble
by echicken

Any resemblance to a popular arcade game is entirely coincidental.

1)   Installation
1.1) External program configuration
1.2) Connecting to the global scoreboard
1.3) Hosting your own scoreboard
2)   Support
3)   Gooble Gooble?

1) Installation

Whether you're upgrading or installing this game for the first time, ensure
that you have the latest copies of the following files in place:

exec/load/frame.js
exec/load/sprite.js

The most recent copies can be found on the Synchronet CVS repository, at
cvs.synchro.net.

1.1) External program configuration

If you're upgrading from the previous version of this game, you can skip this
step and proceed to section 1.2.

In scfg (BBS->Configure in the Synchronet Control Panel under Windows,) do as
follows:

- Go to: External Programs -> Online Programs (Doors)
- Select an Online Program Section to add this game to
- Select "Available Online Programs..."
- Add a new entry with the following details:
  - Name: Gooble Gooble
  - Internal Code: Gooble
  - Start-up directory: ../xtrn/gooble
  - Multiple Concurrent Users: Yes
- All other options can be left at their default settings

1.2) Connecting to the global scoreboard

In your 'xtrn/gooble/' directory, create a 'server.ini' file containing the
following two lines:

host = bbs.electronicchicken.com
port = 10088

(Substitute those values as appropriate if you're connecting to a scoreboard
hosted by someone other than me.)

1.3) Hosting your own scoreboard

If you prefer to host your own scoreboard, you'll need to ensure that you have
the following section in your 'ctrl/services.ini' file:

[JSON]
Port=10088
Options=STATIC|LOOP
Command=json-service.js

(You can use another port if you wish.  If you do, you must create a
'server.ini' file as described in section 1.2, with 'host' set to 'localhost'
and 'port' set to whichever port you chose.)

You'll also need to add the following to your 'ctrl/json-service.ini' file:

[gooble2]
dir=../xtrn/gooble/

2) Support

Post a message to 'echicken' in the "Synchronet Sysops" sub-board of DOVE-Net,
find me on IRC in #synchronet on irc.synchro.net, or send an email to
echicken -at- bbs.electronicchicken.com.

3) Gooble Gooble?

Q) Shouldn't that be "Gobble Gobble," because the yellow-circle-guy goes
   around gobbling up all of the pellets and ghosts?

A) No.  See the main-menu background for a relevant IRC quote.  It'll be a
   chilly day in heck before I question Phil's spelling.

