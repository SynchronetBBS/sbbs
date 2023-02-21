wttr.in viewer for Synchronet BBS
by echicken -at- bbs.electronicchicken.com

Contents

	1) About
	2) Installation
	3) Customization

1) About

	This script pulls a weather report from https://wttr.in and displays it in
	the terminal. wttr.in is a console-oriented weather forecast service
	created and hosted by Igor Chubin (https://github.com/chubin/wttr.in).

2) Installation

	In SCFG, go to 'External Programs', then 'Online Programs (Doors)', and
	choose the section to which you'd like to add this mod. Fill out the new
	entry with the following details:

		Name                       wttr.in Weather Forecast
		Internal Code              WTTR
		Start-up Directory         /sbbs/xtrn/wttr.in
		Command Line               ?wttr.js
		Multiple Concurrent Users  Yes

	If you want this mod to run during your logon process, set the following:

		Execute on Event			logon

	All other options can be left at their default settings.

3) Customization

	The default query URL is https://wttr.in/?AFn for ANSI, no 'Follow' line,
	and narrow output. You can override this by supplying an alternate URL on
	the command line, such as:

		Command Line	?wttr.js https://wttr.in/?m0AFn

	Responses are cached in your OS temp directory and remain valid for 3600
	seconds (one hour). You can override this default cache TTL by supplying
	a numeric value (in seconds) on the command line, such as:

		Command Line	?wttr.js 1800

	You can combine the parameters in whichever order you prefer:

		Command Line 	?wttr.js https://wttr.in/?m0AFn 1800
		Command Line 	?wttr.js 1800 https://wttr.in/?m0AFn
