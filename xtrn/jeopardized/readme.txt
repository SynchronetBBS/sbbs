Jeopardized!
------------
A networked trivia game for Synchronet BBS 3.15+
by echicken, January 2016


Contents
--------

	1) About
	2) Installation
		2.1) Copy Files
		2.2) External Program Setup
		2.3) Update
	3) Hosting Your own Game
		3.1) Hosting your own Jeopardized! JSON Database
			3.1.1) Add a JSONDB module
			3.1.2) Schedule a nightly maintenance event
		3.2) Hosting your own Jeopardy! Web API
	4) Support
		4.1) DOVE-Net
		4.2) electronic chicken bbs
		4.3) IRC
		4.4) Email


1) About
--------

	Jeopardized! is a networked trivia game for Synchronet BBS inspired by (and
	using data from) the Jeopardy! TV game show.  This game should work with
	Synchronet BBS 3.15 and above.

	Players on all participating BBSs are presented with the same categories and
	questions, and can play up to three rounds of the game each day.  The game
	is reset each night, however players' running totals and statistics will
	persist.

	More information is available in the in-game help screen.


2) Installation
---------------

	2.1) Copy Files
	---------------

		Copy the 'jeopardized' directory that you extracted from the
		installation archive into your 'xtrn/' directory, where 'xtrn/' is
		relative to the root of your Synchronet BBS installation.		

	2.2) External Program Setup
	---------------------------

		Add a new External Program, supplying these details at the prompts:

			Online Program Name: Jeopardized!

			Internal Code: JEOPARDY

		Edit the program so that its settings look like the following:

			Name						Jeopardized!
			Internal Code				JEOPARDY
			Start-up Directory			../xtrn/Jeopardized
			Command Line				?jeopardized.js
			Multiple Concurrent Users	Yes

		Any other settings not mentioned above can be left alone.

	2.3) Update
	-----------

		You can skip this step if you believe that your BBS is fairly up to
		date.  However, if you run into problems, updating these files may help.

		Grab the latest copies of the following files:

			- exec/load/frame.js
			- exec/load/tree.js
			- exec/load/layout.js
			- exec/load/event-timer.js
			- exec/load/json-client.js
			- exec/load/typeahead.js
			- exec/load/scrollbar.js

			All paths above are relative to the root of your Synchronet BBS
			installation.

			You can get these files by doing a CVS update of your 'exec/load/'
			directory, by browsing the Synchronet CVS on the web here:

				http://cvs.synchro.net/cgi-bin/viewcvs.cgi/exec/load/

			or by downloading the nightly sbbs_run.zip archive from here:

				http://vert.synchro.net/Synchronet/sbbs_run.zip


3) Hosting Your own Game (or: Don't Do It)
------------------------------------------

	3.1) Hosting your own Jeopardized! JSON Database

		The default 'settings.ini' file tells your local copy of this program to
		connect to a game database server at 'bbs.electronicchicken.com'.  For
		ease of setup, and to allow your users to play with everyone else using
		that shared database, it's recommended that you leave these settings as
		they are.

		However, if you prefer to limit the game and its in-game chat features
		to your own BBS, you can do the following:

		3.1.1) Add a JSONDB module
		--------------------------

			Edit your 'ctrl/json-service.ini' file and add a section like this:

				[jeopardized]
				dir=../xtrn/jeopardized/server/

			If you aren't already running the JSONDB service, then edit your
			'ctrl/services.ini' file and add a section like this:

				[JSONDB]
				Port=10088
				Options=STATIC | LOOP
				Command=json-service.js

			Recycle your 'Services' thread, or restart your BBS.

			Edit your 'xtrn/jeopardized/settings.ini' file, and in the [JSONDB]
			section change the 'host' value to point to your BBS ('localhost'
			should do) and edit	the 'port' value if necessary.

		3.1.2) Schedule a nightly maintenance event
		-------------------------------------------

			In SCFG, add a new Timed event as follows:

				Internal Code					JPRDYMNT
				Start-up directory 				../xtrn/jeopardized/server
				Command Line					?maintenance.js
				Enabled							Yes
				Execution Days of Week			All
				Execution Time 					00:00
				Always Run After Init/Re-init	No

			Leave any other settings alone.

	3.2) Hosting your own Jeopardy! Web API
	---------------------------------------

		The nightly maintenance task fetches categories and questions from a
		web API.  The user-facing game itself does not talk to this API.  Even
		if you do wish to host your own JSONDB module for this game, you don't
		necessarily need to host your own web API.  It's recommended that you
		only follow this step if you have a good reason (eg. the author of this
		program has died and the default web API is no longer online.)

		The web API runs under node.js.  You'll want to start by installing it
		from here:

			https://nodejs.org/en/download/

		You can grab a copy of the web API application here:

			https://github.com/echicken/jeopardy-web-api

		You must run 'npm install' in the application's directory in order to
		install any dependencies.

		This, in turn, relies on a 'clues.db' file generated by this:

			https://github.com/whymarrh/jeopardy-parser

		Which, in turn, relies on the J! Archive website.  If that disappears or
		is offline, you're out of luck.

		If you're successful in generating a 'clues.db' file, place it in the
		top directory of the web API application.

		If you manage to succeed in following all of the above steps, you can
		run the web API by executing 'node index.js' in the application
		directory.  Ideally you should run it as a service with something like
		'forever' (https://www.npmjs.com/package/forever).

		By default, the web API listens on port 3001.  You can change this by
		editing the 'index.js' file.

		To tell your maintenance script to connect to your own web API, edit the
		[WebAPI] section of 'xtrn/jeopardized/settings.ini' and modify the URL
		value as necessary.


4) Support
----------

	You can contact me for support via any of the following means, in the
	following order of preference:

	DOVE-Net

		Post a message to 'echicken' in the 'Synchronet Sysops' sub-board.
		Unless I'm dead or on vacation, I'll probably get back to you within a
		day or so.

	electronic chicken bbs
	
		Post a message in the 'Support' sub-board of the 'Local' message group
		on my BBS, bbs.electronicchicken.com

	IRC : #synchronet on irc.synchro.net
	
		I'm not always in front of a computer, so you won't always receive an
		immediate response if you contact me on IRC.  That said, if you stay
		online and idle, I'll probably see your message and respond eventually.

	Email
	
		You can email me at echicken -at- bbs.electronicchicken.com, however I
		prefer to discuss problems & provide support in a public forum in case
		any information that comes up can be of benefit to somebody else in the
		future.