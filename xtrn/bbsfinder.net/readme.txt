BBSfinder.net updater for Synchronet 3.15+
echicken -at- bbs.electronicchicken.com

Requirements:
-------------

It's recommended that you grab the latest copies of the following files from
the Synchronet CVS at cvs.synchro.net:

exec/load/http.js
exec/load/event-timer.js
xtrn/bbsfinder.net/bbsfinder.js
xtrn/bbsfinder.net/readme.txt


Installation:
-------------

I'm going to assume that your copy of bbsfinder.js is located at:

/sbbs/xtrn/bbsfinder.net/bbsfinder.js

Add the following section to your ctrl/modopts.ini file:

[bbsfinder]
username = <username>
password = <password>


Usage:
------

Method #1: Running as a timed event

	Launch scfg (that's BBS->Configure from the Synchronet Control Panel in
	Windows,) select "External Programs", then "Timed Events", and create a
	new item as follows:
	
	Event Internal Code: BBSFINDR
	
	Edit the timed event that you've just created so that it looks like this:

		BBSFINDR Timed Event
		----------------------------------------------------------
		¦Internal Code                   BBSFINDR
		¦Start-up Directory              /sbbs/xtrn/bbsfinder.net
		¦Command Line                    ?bbsfinder.js
		¦Enabled                         Yes
		¦Execution Node                  1
		¦Execution Months                Any
		¦Execution Days of Month         Any
		¦Execution Days of Week          All
		¦Execution Frequency             360 times a day
		¦Requires Exclusive Execution    No
		¦Force Users Off-line For Event  No
		¦Native Executable               No
		¦Use Shell to Execute            No
		¦Background Execution            No
		¦Always Run After Init/Re-init   Yes

	Hint: to set the "Execution frequency" to "360 times a day", select
	"Execution frequency" and answer "No" at the "Execute at a specific time?"
	prompt.  We want this event to run every four minutes, so (24*60)/4 = 360.
	
Method #2: Running from jsexec

	From a command prompt, execute the following, substituting paths as
	required:
	
	/sbbs/exec/jsexec /sbbs/xtrn/bbsfinder.net/bbsfinder.js -l
	

Support:
--------

Post a message to 'echicken' in DOVENet's Synchronet Sysops echo, or find me
in #synchronet on irc.synchro.net.