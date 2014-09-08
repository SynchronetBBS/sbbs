SyncWall
An Inter-BBS ANSI graffiti wall for Synchronet BBS
by echicken

Contents

1) Installation
2) Hosting your own wall
3) SyncWall version 1 connector
4) JSON database schema
5) JSON service modules


1) Installation

Requirements:

	SyncWall is known to work on Synchronet 3.16b.  It will likely continue to
	work with later versions, and may function with versions back to 3.15 with
	appropriate updates.

	You should at minimum update your copies of exec/load/ansiedit.js and
	exec/load/frame.js in order to use SyncWall, especially if they date back
	prior to September 4th, 2014.  You can find the latest versions of these
	files on the Synchronet CVS, cvs.synchro.net.

Installation procedure:

	Create a 'server.ini' file in your xtrn/syncwall/ directory containing
	the following:

		host = bbs.electronicchicken.com
		port = 10088

	Using 'scfg' (BBS -> Configure from the Synchronet Control Panel on
	Windows) do the following:

	- Go to External Programs -> Online Programs (Doors)
	- Select the Online Program Section where you want to add SyncWall
	- Create a new entry
	- For "Online Program Name", provide: SyncWall
	- For "Internal Code", provide: SYNCWALL
	- For "Start-up Directory", provide: ../xtrn/syncwall/
	- For "Command Line", provide: ?syncwall.js
	- For "Multiple Concurrent Users", select: Yes

	All other options can be left at their default values, unless you have
	some reason to set them otherwise.

	If you already have the previous version of SyncWall configured, adjust
	your configuration if necessary so that it matches the above.

	The above instructions are for a typical Synchronet installation.  It's
	assumed that if they need to be adapted to your circumstances, you'll
	know why and how to do it.


2) Hosting your own wall

3) SyncWall version 1 connector

4) JSON database schema

{	'MONTH' : "092014",
	'MONTHS' : [ "082014" ],
	'USERS' : [ "echicken", ... ],
	'SYSTEMS' : ["electronic chicken bbs", ... ],
	'STATE' : { 0 : { 0 : { 'c' : "a", 'a' : 7, 'u' : 0, 's' : 0 } ... } ... },
	'SEQUENCE' : [ { 'y' : 0, 'x' : 0, 'c' : "a", 'a' : 7, 'u' : 0, 's' : 0 } ... ],
	'LATEST' : { 'y' : 0, 'x' : 0, 'c' : "a", 'a' : 7, 'u' : 0, 's' : 0 }
}

	MONTH 

		The month and year of the current wall data. (MMYYYY string.)
		Read-only except for sysops of wall-host systems.

	MONTHS

		An array of MMYYYY strings representing monthly histories that
		are available for viewing.
		Read-only except for sysops of wall-host systems.

	USERS 

		An array of user aliases known to SyncWall.
		Full access available to sysops of wall-host systems.
		Subscribe, unsubscribe, read, slice, and push available to others.

	SYSTEMS

		An array of BBS names known to SyncWall.
		Full access available to sysops of wall-host systems.
		Subscribe, unsubscribe, read, slice, and push available to others.

	STATE

		The current state of the SyncWall editor canvas.

		Full access is available to sysops of wall-host systems.
		Read access is available to others.
		
		Each property at the top level is an object named for the Y coordinate
		of a line.
		
		Each sub-property of a Y-coordinate object is an object named for
		the X coordinate of a character on that line.

		Each sub-property of an X-coordinate object is an object with
		properties detailing the data at that character-cell as follows:

			'c'	- The character at that position. (string)

			'a' - The attributes for the character at that position. (number)
				  (Compare against colour & attribute masks from sbbsdefs.js)

			'u'	- The user who drew the character. (number, index into USERS)

			's'	- The originating BBS. (number, index into SYSTEMS)

	SEQUENCE

		An array of all characters drawn on this SyncWall canvas since the
		last monthly (or unscheduled) reset, in the order in which they were
		drawn.

		Full access available to sysops of wall-host systems.
		Read and slice are available to others.

		Each element in the array is an object with the following properties:

			'y'	- The Y coordinate of the character. (number)

			'x'	- The X coordinate of the character. (number)

			'c'	- The character at that position. (string)
				  (If "CLEAR", ANSIEdit.clear() was triggered at this point.)

			'a' - The attributes for the character at that position. (number)

			'u'	- The user who drew the character. (number, index into USERS)

			's'	- The originating BBS. (number, index into SYSTEMS)

	LATEST

		An object representing the most recent character drawn on the canvas.
		The properties of this object are the same as those of a SEQUENCE
		element.

		Subscribe, unsubscribe, read and write are available to everyone.
		Written data is validated by the wall-host system prior to actually
		being written to the database or published to subscribers.

		Full access available to sysops of wall-host systems.


5)	JSON service modules

	The following scripts are loaded automatically by the JSON service module
	(json-service.js) if you are hosting your own SyncWall database.  (You
	may want to make use of this in your own JSON DB enabled applications.)

	commands.js

		Intercepts all requests from JSON clients, verifies that appropriate
		permissions are available for the operation requested, validates data
		prior to adding it to the database and publishing it to clients.

	service.js

		A background service which connects to the JSON service as a client,
		and authenticates as the sysop of the host system.  This script has
		full write access to the entire DB.  It initializes the database if
		necessary, performs monthly resets, and maintains the STATE object
		and SEQUENCE array.