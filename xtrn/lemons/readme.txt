Lemons
------
A game for Synchronet BBS 3.16+
by echicken -at- bbs.electronicchicken.com, March 2015


Contents
--------

1) About
2) Requirements
3) Installation
	3.1) Connect to the networked scoreboard
	3.2) Host your own scoreboard
4) Support


1) About
--------

Sometimes this is how games are created:

<MegaloYeti> someone needs to make a new lemmings game
<mcmlxxix> bbs lemmings?
<mcmlxxix> hmmm
<echicken> mhm
<echicken> little @ signs walking off of cliffs
<echicken> or wee lemming sprites
<echicken> actually that would be fairly easy
<echicken> claimed
<mcmlxxix> :|

Okay, so it's "Lemons" and it's pared down somewhat, but the basic premise is
the same.  Just try it, read the help screen, and you'll get the idea.


2) Requirements
---------------

This game may run on Synchronet 3.15, but has not been tested with anything
less than 3.16.

Ensure that you have the latest copies of the following files in your exec/load/
directory.  You can grab them one by one, or do a CVS update.  (If you choose
to update everything, backing up your BBS is a good idea.)

- frame.js:
	http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/frame.js
- tree.js:
	http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/tree.js
- event-timer.js:
	http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/event-timer.js
- json-client.js:
	http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/json-client.js
- sprite.js:
	http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/sprite.js


3) Installation
---------------

In 'scfg' (that's BBS->Configure from the Synchronet Control Panel in Windows),
go to "External Programs -> Online Programs (Doors)" and select the area you
wish to add this game to.  Create a new entry, and set it up as follows:

Name: Lemons
Internal Code: LEMONS
Start-up Directory: ../xtrn/lemons/
Command Line: ?lemons.js
Multiple Concurrent Users: Yes

(All other options can be left at their default values.)

	3.1) Connect to the networked scoreboard
	----------------------------------------

	You're welcome to connect your local game to the networked scoreboard that
	I host.  To do so, Create a file called 'server.ini' in the Lemons game
	directory, and put the following two lines in it:

		host = bbs.electronicchicken.com
		port = 10088


	3.2) Host your own scoreboard
	-----------------------------

	If you prefer not to connect to my scoreboard, you will need to set up your
	own or the game will not work.  To do so, ensure that the JSON service is
	enabled via the following entry in your 'ctrl/services.ini'	file:

		[JSON]
		Port=10088
		Options=STATIC | LOOP
		Command=json-service.js

	If you've just added the above to your services.ini file, you may need to
	restart your services (just restart your BBS if you don't know how) in
	order for the change to take effect.

	You will also need to add the following to your 'ctrl/json-service.ini'
	file (create one if it doesn't already exist):

	[lemons]
	dir=../xtrn/lemons/

	In the absence of a 'server.ini' file in its directory, Lemons will attempt
	to connect to a JSON-DB server at 127.0.0.1 on port 10088. If your JSON
	server binds to another port or address, create a 'server.ini' file with
	the appropriate values.

	You can allow other systems to connect to your scoreboard by opening your
	JSON-DB server's port to them.  They will need to create or modify their
	'server.ini' files accordingly.


4) Support
----------

There are three easy ways to reach me:

- Post a message to echicken in the Synchronet Sysops sub on DOVE-Net
- Find me in #synchronet on irc.synchro.net
- Contact me on my BBS at bbs.electronicchicken.com