Bullshit 3.0
by echicken -at- bbs.electronicchicken.com

Contents

	1) About
	2) Setup
	3) Customization
	4) Bullshit for the web
	5) Support


1) About

	Bullshit is a lightbar bulletin lister/reader for Synchronet BBS 3.16+.
	A message sub-board is used as a storage back-end, so that you can add a
	new bulletin to the list just by posting a message.  You can also include
	.ANS or .TXT files - such as door game score files - as pinned items at the
	top of the list.


2) Setup

	2.1) Create a message area

		This step is recommended, but optional.  You can skip this part if you
		don't want Bullshit to pull bulletins out of a message base.

		Launch SCFG (BBS->Configure in the Synchronet Control Panel on Windows.)

		In 'Message Areas', select your local message group, and create a new
		sub	with the following details:

			Long Name                  Bulletins
			Short Name                 Bulletins
			QWK Name                   BULLSHIT
			Internal Code              BULLSHIT
			Access Requirements        LEVEL 90
			Reading Requirements       LEVEL 90
			Posting Requirements       LEVEL 90

			Toggle Options...

				Default on for new scan		No
				Forced on for new scan		No
				Default on for your scan	No

		Note: You can name this message area whatever you want, and you can use
		an existing message area if you wish.  Ideally only the sysop will be
		able to read or post to this area, and it won't be included in the new
		message scan.


	2.2) Create an External Program entry

		Still in SCFG, return to the main menu, select 'External Programs',
		then 'Online Programs (Doors)', choose the section you wish to add
		Bullshit to, then create a new entry with the following details:

			Name                       Bullshit
			Internal Code              BULLSHIT
			Start-up Directory         /sbbs/xtrn/bullshit
			Command Line               ?bullshit.js
			Multiple Concurrent Users  Yes

		If you want Bullshit to run during your logon process, set the
		following:

			Execute on Event			logon

		All other options can be left at their default settings.


3) Customization

	Some customization settings are available in 'bullshit.ini':

	In the root section:

		-	The 'messageBase' setting specifies the internal code of the
			message sub-board that Bullshit should load.
		-	The 'maxMessages' setting specifies how many of the most recent
			messages in this area should be listed. (Use 0 for no limit.)
		-	If you don't want to load bulletins from a message base, simply
			delete the 'messageBase' setting from 'bullshit.ini'.
		-	If the optional 'newOnly' setting is present and 'true', only
			unread messages and recently updated files will be shown to
			users.  Per-user read/unread and last-viewed file timestamps
			are stored in data/user/####.bullshit JSON files.  If nothing
			is available to be displayed to the user. Bullshit will exit
			silently.

	In the 'colors' section:

		-	'title' and 'text' are the colors used when viewing an item
		-	'heading' controls color or the 'Title' and 'Date' column headings
		-	'lightbarForeground' and 'lightbarBackground' control the color of
			a highlighted item in the list
		-	'listForeground' controls the color of a non-highlighted item
		-	'footer' controls the color of the 'Q to quit ...' text
		-	'border' can be a single color, or a comma-separated list

	The 'files' section is empty by default.  Here you can add any number of
	entries for files that you wish to include in the list.  The key will be
	used as the item's title in the list, while the value must be the path to
	the file, for example:

		LORD Scores = /sbbs/xtrn/lord/score.ans

	At the moment, only files with .ANS and .TXT extensions can be loaded.  If
	that's a huge pain for you, let me know and I'll maybe do something about
	it at some point.


4) Bullshit for the web

	The included '999-bullshit.xjs' file is compatible with my "new" web UI for
	Synchronet (https://github.com/echicken/synchronet-web-v4).  You can add it
	to your site by copying it to the 'pages' subdirectory, renaming it as
	needed.  You could also just copy the contents of this file into another of
	your pages if you wish for bulletins to show up there (000-home.xjs, for
	example.)

	This will probably work with ecweb v3 as well, but I haven't bothered to
	test it.

	For cosmetic and snobbish design reasons, your pinned 'files' list will not
	be displayed in the web UI.  You're better off creating a 'Scores' page of
	some kind for this purpose.


5) Support

	You can contact me for support via any of the following means, in the
	following order of preference:

	DOVE-Net:

		Post a message to 'echicken' in the 'Synchronet Sysops' sub-board.
		Unless I'm dead or on vacation, I'll probably get back to you within a
		day or so.


	electronic chicken bbs:

		Post a message in the 'Support' sub-board of the 'Local' message group
		on my BBS, bbs.electronicchicken.com


	IRC : #synchronet on irc.synchro.net:

		I'm not always in front of a computer, so you won't always receive an
		immediate response if you contact me on IRC.  That said, if you stay
		online and idle, I'll probably see your message and respond eventually.


	Email:

		You can email me at echicken -at- bbs.electronicchicken.com, however I
		prefer to discuss problems & provide support in a public forum in case
		any information that comes up can be of benefit to somebody else in the
		future.
