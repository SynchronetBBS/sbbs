wttr.in viewer for Synchronet BBS
by echicken -at- bbs.electronicchicken.com

Contents

	1) About
	2) Installation
	3) Customization
	4) Notes

1) About

	This script pulls a weather report from https://wttr.in and displays it in
	the terminal. wttr.in is a console-oriented weather forecast service
	created and hosted by Igor Chubin (https://github.com/chubin/wttr.in).

	The output that you see on your BBS was generated entirely by wttr.in,
	except that this script translates their xterm colours to ANSI-BBS. See
	the 'Customization' section below for information on changing the format.
	I can't (won't) change the look any further (different icons, etc.).

	Geolocation is provided automatically by wttr.in. In a future update, we
	might use our own geolocation provider, but I'd rather avoid it for
	simplicity's sake.

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

	See https://wttr.in/:help for details on query parameters.

	By default, this script will query https://wttr.in/?1AFn and will cache
	non-error responses for 3600 seconds (one hour). If the user's IP address
	is available, wttr.in will use it for geolocation. If the user's IP
	address is not available, your BBS' external IP address will be used for
	geolocation.

	If you wish to override any of this behaviour, create this file:

		ctrl/modopts.d/wttr.ini

	Paste this into the file:

		[wttr.in]
		base_url = https://wttr.in/
		units = m
		view = 1AFn
		cache_ttl = 3600

	All of the above settings are optional and any may be omitted. Substitute
	the default values shown above with your preferred settings (obviously).

	You may also add either of these optional settings:

		fallback_location = Mountain View
		fallback_ip = 8.8.8.8

	If fallback_location is set, it will be used instead of your BBS' external
	IP address to find the default weather report.

	If fallback_ip is set, it will be used instead of your BBS' external IP
	address. If present, this setting will override fallback_location.

4) Notes

	Ensure that your ctrl/modopts.ini file includes this line:

		!include modopts.d/*.ini

	If this line does not exist, add it to the bottom of the file. This
	ensures that everything in your ctrl/modopts.d directory is loadable.
