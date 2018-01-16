# synchronet-web-v4
A web interface for Synchronet BBS

* [Disclaimer](#disclaimer)
* [Requirements](#requirements)
* [Quick start](#quick-start)
* [Configuration](#configuration)
	* [Advanced / Optional Settings](#optional-settings)
	* [Additional fTelnet Targets](#additional-ftelnet-targets)
* [Customization](#customization)
	* [Sidebar Modules](#sidebar-modules)
	* [Pages](#pages)
* [Uninstall](#uninstall)
* [Getting Support](#getting-support)
* [FAQ](#faq)

### Disclaimer

- Use this software at your own risk.  It's still being developed, and hasn't been thoroughly tested yet.

### Requirements

- This web interface has been tested with Synchronet BBS 3.17a.  It will probably work with later versions.
- In addition to updating your Synchronet build, be sure to update your the javascript modules in your *exec* and *exec/load* directories as well.  You can get them from [the Synchronet CVS repository](http://cvs.synchro.net/) or in the [sbbs_run.zip archive](http://vert.synchro.net/Synchronet/sbbs_run.zip).
	- Do *not* simply unzip sbbs_run.zip overtop your existing sbbs directory structure.  This may overwrite much of your local configuration.  Extract the archive elsewhere and copy things over piecemeal as needed.

### Quick start

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
	; Where (absolute or relative to 'exec') the 'lib' and 'root' directories live
	web_directory = ../web
	; Path to a .ans file to use as the ftelnet splash screen
	ftelnet_splash = ../text/synch.ans
	; Enable or disable keyboard navigation in message threads
	keyboard_navigation = false
	; Display upvote/downvote buttons in message threads (3.17)
	vote_functions = true
	; Refresh nodelist, vote counts, etc. this often (in milliseconds)
	refresh_interval = 60000
	; External Programs (or entire sections) to exclude from the Games page
	xtrn_blacklist = scfg,oneliner
	; Disable the sidebar altogether
	layout_sidebar_off = false
	; Place the sidebar on the left-hand side of the page
	layout_sidebar_left = false
	; Make the page consume the entire width of the browser window
	layout_full_width = false
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

### Configuration

- Ensure that the *guest* user specified in the [web] section of *ctrl/modopts.ini* exists and has only the permissions that you want an unauthenticated visitor from the web to have.  This user probably shouldn't be able to post messages, and definitely shouldn't be able to post to networked message areas.
- Customise the *xtrn_blacklist* setting in the [web] section of *ctrl/modopts.ini*.  This is a comma-separated list of *internal codes* of any programs (or Online Program Sections) that you wish to *exclude* from your games page.

#### Optional Settings

The following *optional* settings can be added to the [web] section of your *ctrl/modopts.ini* file:

```ini
	; Use extended-ASCII characters when displaying forum messages (default: true)
	forum_extended_ascii = false
	; Only load this many messages from each sub (default: 0 for all)
	max_messages = 500
	; Connect to the websocket proxies on ports other than specified in ctrl/services.ini
	websocket_telnet_port = 1124
	websocket_rlogin_port = 1514
	allowed_ftelnet_targets = some.other.bbs:23,yet.another.bbs:2323
	; Serve this web interface from a subdirectory of the webserver's document root
	web_root = ../web/root/ecwebv4
```

- Setting *forum_extended_ascii* to *false* may resolve problems with displaying UTF-8 encoded characters; character codes > 127 will not be assumed to be extended-ASCII references to CP437 characters.
- Tweaking *max_messages* may improve forum performance or resolve 'Out of memory' errors, if you see any of those when browsing the forum
- Normally, fTelnet will try to connect to the websocket proxies based on information from *ctrl/services.ini*.  In some situations (eg. when using an HTTPS reverse proxy) it may be necessary to connect to another port unknown to Synchronet.
- The *allowed_ftelnet_targets* setting is only necessary if you will be adding additional pages through which users can connect to external systems via fTelnet (see below).  This is a comma-separated list of host:port entries.
- If you placed the *contents* of the *web/root/* directory from this repository at some other location within your webserver's document root, use the *web_root* value to point to that directory.  (Note that the *contents* of the *web/lib/* directory from this repository must still live at *[web_directory]/lib* on your system.)

### Customization

This web interface uses [Bootstrap 3.3.5](http://getbootstrap.com/).  It should be possible to use any compatible stylesheet.

- You can place your own CSS overrides in *web/root/css/custom.css*.  Create this file and use it if you want to use fonts, colours, etc. other than the defaults.  It is not recommended that you modify any of the existing stylesheets.

#### Sidebar Modules

Sidebar modules are the widgets displayed in the narrow column running down the right (or left) side of the page.  A sidebar module can be an SSJS, XJS, HTML, or TXT file.

- Sidebar modules are loaded in alphanumeric order from the *web/root/sidebar/* directory; see the included files for examples and for a file-naming convention that enforces order of appearance
- HTML, XJS, and SSJS sidebar modules should not be complete HTML documents.  They should not have \<html\>, \<head\>, or \<body\> tags.  Instead, they should contain (or produce) an HTML snippet suitable for inclusion in the overall page.
- TXT sidebar modules are displayed inside of \<pre\> tags to preserve fixed-width formatting.
- Support for additional file formats can be added if necessary, but by using HTML and Javascript you should be able to display whatever you like.  (If you want to put an image in the sidebar, a simple HTML file containing an \<img\> tag will do the job, for example.)

#### Pages

Like sidebar modules, pages can be HTML, XJS, SSJS, or TXT files.

- Pages are loaded in alphanumeric order from the *web/root/pages/* directory.  See the included files for examples and for a file-naming convention that enforces order of appearance.
- As with sidebar modules, HTML, XJS, and SSJS pages should not be complete HTML documents.  They should contain (or generate) snippets of text or HTML suitable for inclusion in the overall page.
- Your *web/root/pages/* directory should contain a [webctrl.ini](http://wiki.synchro.net/server:web#webctrlini_per-directory_configuration_file) file.
	- You can use this file to restrict access to certain pages so that guests or unprivileged users can't see them
- In an HTML, XJS, or SSJS file, the first line *containing a comment* is treated as the *control line* for the page.
	- The format of the control line is *OPTION|OPTION:Title*
	- Available options at the moment are *HIDDEN* and *NO_SIDEBAR*
	- *HIDDEN* pages will not appear in the menus or in the activity fields of the *who's online* list
	- The *NO_SIDEBAR* directive tells the layout script not to include a sidebar on this page
		- The main content will expand to fill the space normally used by the sidebar
	- The part of the control line following the first colon is treated as the title of the page.  This is the text that appears in the menu, browser title bar, and *who's online* list.
		- If you don't need to specify any options, (and as long as your page title doesn't contain a colon) you can omit the colon, and the entire string will be treated as the title of the page, or you can start the control line with a colon.

Here's an example control line for a hidden HTML file:

```html
<!--HIDDEN:My Awesome Web Page-->
```

Here's an example control line for a hidden XJS page with no sidebar:

```html
<!--HIDDEN|NO_SIDEBAR:My Awesome Web Page-->
```

Here's an example control line for an SSJS script with no settings:

```js
//My Awesome Web Page
```

If your page title contains a colon, it's necessary to prepend a dummy one like so:

```js
//:Awesome Web Pages: This is one
```

Of course, that's not an issue if you're providing some settings:

```html
<!--NO_SIDEBAR:Awesome Web Pages: This is one-->
```

You can add drop-down menus to the navigation bar by adding subdirectories to the *web/root/pages/* directory.  The files within these subdirectories follow the same format described above.
- The name of the subdirectory is used as the text of the menu item.
- You can nest additional subdirectories to create sub-menus.
- Subdirectories with names beginning with *.* will be ignored.
- Each subdirectory can contain a [webctrl.ini](http://wiki.synchro.net/server:web#webctrlini_per-directory_configuration_file) file for access control.

#### Additional fTelnet targets

The fTelnet embed on the home page is configured to automatically connect to a BBS on the same system that is hosting the website.  If you run another BBS or wish to allow users of your website to connect via fTelnet to some other BBS:

- Edit *ctrl/modopts.ini*
- In the *[web]* section, add the *allowed_ftelnet_targets* key if it doesn't already exist (see *Additional/advanced settings* above)
- Add an entry to the list for each additional host you want to allow fTelnet to connect to
- Create a new *.xjs* file in your *pages/* directory or in a subdirectory thereof.  (In this example, I've created */sbbs/web/root/pages/More/006-some-other-bbs.xjs*.)  Populate it with the following content:

```html
<!--Some Other BBS-->
<?xjs
	if (typeof argv[0] != 'boolean' || !argv[0]) exit();
	load(settings.web_lib + 'ftelnet.js');
?>

<script src="./ftelnet/ftelnet.norip.xfer.min.js" id="fTelnetScript"></script>
<style>.fTelnetStatusBar { display : none; }</style>
<div id="fTelnetContainer" style="margin-bottom:1em;clear:both;"></div>
<div class="row">
	<div class="center-block" style="width:200px;margin-bottom:1em;">
		<button id="ftelnet-connect" class="btn btn-primary">Connect via Telnet</button>
	</div>
</div>
<script type="text/javascript">
	Options.Hostname = 'valhalla.synchro.net';
	Options.Port = 23;
    Options.ProxyHostname = '<?xjs write(http_request.vhost); ?>';
    Options.ProxyPort = <?xjs write(settings.websocket_telnet_port || webSocket.Port); ?>;;
    Options.ProxyPortSecure = <?xjs write(settings.websocket_telnet_port || webSocket.Port); ?>;;
	Options.ConnectionType = 'telnet';
	Options.SplashScreen = '<?xjs write(getSplash()); ?>';
	Options.StatusBarVisible = false;
	Options.VirtualKeyboardVisible = (
		/Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(
			navigator.userAgent
		)
	);
	var ftClient = new fTelnetClient('fTelnetContainer', Options);
	$('#ftelnet-connect').click(function() { ftClient.Connect(); });
</script>
```

- Edit the first line so that the title of the page (*Some Other BBS*) is to your liking
- Edit the *Options.Hostname* line so that the text within the quotes points to the system you want to target
- Edit the *Options.Port* line if the target system's telnet server is listening on some port other than *23*

### Uninstall

To stop using this web interface, you can just revert to your previous *web* directory at any time.

- The [web] section added to *ctrl/modopts.ini* won't hurt anything if you leave it there, but you can delete it if you want
- Revert your *ctrl/services.ini* file to the backup you made prior to installing this web interface
- Undo any changes you made to your firewall & router during the *Quick Start*

### Getting Support

#### Via GitHub

- Please browse the existing [issues](https://github.com/echicken/synchronet-web-v4/issues) for this project, including those marked as closed.  You may find that your question has already been asked (and hopefully answered).
- Open a new [issue](https://github.com/echicken/synchronet-web-v4/issues) here on GitHub

#### On IRC

- You can find me in #synchronet on irc.synchro.net.  I may be AFK, but ask your question and idle for a while; I'll respond eventually.

#### On DOVE-Net

- Post a message to 'echicken' in the Synchronet Sysops area.

#### Email

- Please don't.  Public support discussions are better for everyone, especially those searching the web for answers in the future.

### FAQ

#### I followed the instructions, but it isn't working

It usually turns out that the instructions were not followed closely enough.  Please try again.  (I'm not pretending that this thing is bug-free or that the instructions are perfectly clear.  I'm just sayin'.)

#### System performance is poor / memory usage has increased since I installed this

The Forum page is the most likely culprit.  Sorting messages into threads takes up a certain amount of memory and CPU time.  Bots crawling through your Forum may hit several pages at once, etc.  You may be able to mitigate this by playing with the *max_messages* setting as [described above](#optional-settings).  You may also consider consider restricting access to your Forum page so that only authenticated / non-guest users may browse it; access control via *webctrl.ini* files is described in the [Pages](#pages) section above.

#### Something related to Fidonet isn't working!

I don't care.

#### Why aren't you using *insert flavour-of-the-month web framework here*

Because I don't feel like it, and I don't want to hear about it.  I'm sure it's awesome, though.

#### Why don't you use *insert javascript-tooling-thing here*

Because I don't feel like it.  Also, if it adds additional dependencies or setup steps on behalf of the end-user (sysops), I'm not interested in supporting it.
