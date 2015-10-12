# synchronet-web-v4
A web interface for Synchronet BBS

###Quick start

- Back up your Synchronet installation
- Shut down your BBS
- Clone [or download an archive of](https://github.com/echicken/synchronet-web-v4/archive/master.zip) this repository to a convenient location
	- Copy the contents of the downloaded *mods* directory into your local *mods* directory
	- Copy the contents of the downloaded *text* directory into your local *text* directory
	- Rename your current *web* directory to something like *web.old* and then copy the downloaded *web* directory in its place
- [Download fTelnet](https://github.com/rickparrish/fTelnet/archive/master.zip)
	- Extract the archive
	- Copy the *release* subdirectory into your new *web/root/* directory
	- Rename the copied *release* subdirectory to *ftelnet*
- Add the following section to your *ctrl/modopts.ini* file:
```ini
[web]
	; Unauthenticated visitors will be logged in as the user with this alias
	; (Only give this user privileges you want unknown web visitors to have)
	guest = Guest
	; Login sessions expire after this many seconds of inactivity
	timeout = 43200
	; Users disappear from the "Who's online" list after this many seconds
	inactivity = 900
	; Allow new users to register via the web interface
	user_registration = true
	; Enforce a minimum password length if user_registration is true
	minimum_password_length = 6
	; Limit the length of a telegram (in characters) that a web user can send
	maximum_telegram_length = 800
	; Which external program sections to list on the Games page (comma-separated)
	xtrn_sections = games,coa,programs
	; Where (absolute or relative to 'exec') the 'lib' and 'root' directories live
	web_directory = ../web
	; Path to a .ans file to use as the ftelnet splash screen
	ftelnet_splash = ../text/synch.ans
```
- Add the following section to your *ctrl/services.ini* file if it isn't there already:
```ini
[WebSocket]
Port=1123
Options=NO_HOST_LOOKUP
Command=websocketservice.js
```
- Add the following section to your *ctrl/services.ini* file:
```ini
[WebSocketRLogin]
Port=1513
Options=NO_HOST_LOOKUP
Command=websocket-rlogin-service.js
```