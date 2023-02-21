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

	Please see https://wttr.in/:help for a list of options. You may pass any
	of the 'Units' and 'View options' values to this script on the command
	line to customize the output, for example:

		Command Line	?wttr.js m0AFn

	The default is 'AFn' for ANSI, no 'Follow' line, and narrow output.

	Note that your command line argument will completely replace the default
	parameters. You will probably want to specify 'A' and 'n' in addition to
	your chosen values.
