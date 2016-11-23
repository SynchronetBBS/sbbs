# synchronet-web-v4
A web interface for Synchronet BBS

###Disclaimer

- Use this software at your own risk.  It's still being developed, and hasn't been thoroughly tested yet.

###Requirements

- This web interface has been tested with Synchronet BBS 3.17a.  It will probably work with later versions.
- In addition to updating your Synchronet build, be sure to update your the javascript modules in your *exec* and *exec/load* directories as well.  You can get them from [the Synchronet CVS repository](http://cvs.synchro.net/) or in the [sbbs_run.zip archive](http://vert.synchro.net/Synchronet/sbbs_run.zip).

###Quick start

- Back up your Synchronet installation (particularly 'ctrl' and 'web')
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
	; Web-based users disappear from the "Who's online" list after this many seconds
	inactivity = 900
	; Allow new users to register via the web interface
	user_registration = true
	; Enforce a minimum password length if user_registration is true
	minimum_password_length = 6
	; Limit the length of a telegram (in characters) that a web user can send
	maximum_telegram_length = 800
	; Which external program sections to list on the Games page (comma-separated)
	xtrn_sections = games,puzzle,rpg,erotic
	; Where (absolute or relative to 'exec') the 'lib' and 'root' directories live
	web_directory = ../web
	; Path to a .ans file to use as the ftelnet splash screen
	ftelnet_splash = ../text/synch.ans
	; Only load this many messages from each sub (default: 0 for all)
	; (If you get 'Out of memory' errors when viewing subs, tweak this setting)
	max_messages = 0
	; Enable or disable keyboard navigation in message threads
	keyboard_navigation = false
	; Display upvote/downvote buttons in message threads (3.17)
	vote_functions = true
	; Refresh nodelist, vote counts, etc. this often (in milliseconds)
	refresh_interval = 60000
```
- Add the following section to your *ctrl/services.ini* file:
```ini
[WebSocketTelnet]
Port=1123
Options=NO_HOST_LOOKUP
Command=websocket-telnet-service.js
```
- Add the following section to your *ctrl/services.ini* file:
```ini
[WebSocketRLogin]
Port=1513
Options=NO_HOST_LOOKUP
Command=websocket-rlogin-service.js
```
- Tell your router and firewall to open and forward ports *1123* and *1513* to your BBS
- If you were running ecWeb v3 and modified the *RootDirectory* value in the *[Web]* section of *ctrl/sbbs.ini* to point to *../web/root/ecwebv3*, change it back to *../web/root*.

- Edit the *[logon]* section of your *ctrl/modopts.ini* file, and ensure that it has an *rlogin_auto_xtrn* key with a value of *true*
	- NB: that's the *[logon]* section and not the *[login]* section

```ini
[logon]
rlogin_auto_xtrn = true
```

- Your *logon.js* file should have a block of code near the top that looks like this, but if it doesn't you should add it in:

```js
var options = load("modopts.js", "logon");

// Check if we're being asked to auto-run an external (web interface external programs section uses this)
if (options && (options.rlogin_auto_xtrn) && (bbs.sys_status & SS_RLOGIN) && (console.terminal.indexOf("xtrn=") === 0)) {
    var external_code = console.terminal.substring(5);
    if (!bbs.exec_xtrn(external_code)) {
        alert(log(LOG_ERR,"!ERROR Unable to launch external: '" + external_code + "'"));
    }
    bbs.hangup();
	exit();
}
```

- Start your BBS back up again

###Configuration

- Ensure that the *guest* user specified in the [web] section of *ctrl/modopts.ini* exists and has only the permissions that you want an unauthenticated visitor from the web to have.  This user probably shouldn't be able to post messages, and definitely shouldn't be able to post to networked message areas.
- Look at those *xtrn_sections* in the [web] section of *ctrl/modopts.ini*.  They should be the *internal codes* of all External Programs sections that you want to make available to authenticated users via the web.  (You probably don't have an *erotic* External Programs area, but if you do ... that's cool.)

###Customization

- This web interface uses [Bootstrap 3.3.5](http://getbootstrap.com/).  It should be possible to use any compatible stylesheet.
	- You can place your own CSS overrides in *web/root/css/style.css*
	- You can load another stylesheet of your own choosing in the &lt;head&gt; section of *web/root/index.xjs* (load it after the others)
- The sidebar module & page structure is *mostly* similar to the system used in ecWeb v3.  See [the old instructions](http://wiki.synchro.net/howto:ecweb#the_sidebar) for info on adding content.
	- You can force a link to a page to be placed in the *More* menu by using an underscore as a separator in its filename rather than a hyphen

###Uninstall

- To stop using this web interface, you can just revert to your previous *web* directory at any time.
- The [web] section added to *ctrl/modopts.ini* won't hurt anything if you leave it there, but you can delete it if you want
- Revert your *ctrl/services.ini* file to the backup you made prior to installing this web interface
- Undo any changes you made to your firewall & router during the *Quick Start*

###Support

####Via GitHub

- Please browse the existing [issues](https://github.com/echicken/synchronet-web-v4/issues) for this project, including those marked as closed.  You may find that your question has already been asked (and hopefully answered).
- Open a new [issue](https://github.com/echicken/synchronet-web-v4/issues) here on GitHub

####On DOVE-Net

- Post a message to *echicken* in [Synchronet Discussion](https://bbs.electronicchicken.com/?page=001-forum.ssjs&sub=sync).  I read this sub regularly and will respond to you there.

####On IRC

- You can find me in #synchronet on irc.synchro.net.  I may be AFK, but ask your question and idle for a while; I'll respond eventually.

####Email

- Please don't.  Public support discussions are better for everyone, especially those searching the web for answers in the future.