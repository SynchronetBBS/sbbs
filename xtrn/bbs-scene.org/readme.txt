BBS-Scene.org scripts for Synchronet 3.16+
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

*** UNSUPPORTED ***

The author is no longer interested in supporting these scripts.
Have fun.

Requirements:
-=-=-=-=-=-=-

All requirements can be fetched from the Synchronet CVS at cvs.synchro.net,
and include the latest copies of the following files:

/sbbs/exec/load/http.js
/sbbs/exec/load/funclib.js
/sbbs/exec/load/inputline.js
/sbbs/exec/load/frame.js
/sbbs/ctrl/modopts.ini (Add the [bbs-scene.org] section to your local copy.)


Contents of this directory:
-=-=-=-=-=-=-=-=-=-=-=-=-=-

client.js		- Internal use only
onelinerz.bin	- Header graphic for onelinerz.js
onelinerz.js	- Global Onelinerz script for the telnet/console side
onelinerz.ssjs	- Sidebar widget for ecWeb
randombbs.js	- Fetches and displays a random BBS ANSI


Installation & use:
-=-=-=-=-=-=-=-=-=-

modopts.ini
-=-=-=-=-=-

The file /sbbs/ctrl/modopts.ini contains configuration options for various
optional modules.  If this file does not already contain one, create a section
as follows:

[bbs-scene.org]
	username = <your BBS-Scene.org username>
	password = <your BBS-Scene.org password>
	
If the section already exists, just edit the username and password values to
match your BBS-Scene.org account.

onelinerz.js
-=-=-=-=-=-=

In SCFG, go to the External Programs section, and then select "Online Programs
(Doors)".  Here you may either create a new section for BBS-Scene.org-related
externals, or simply add these scripts to an existing area such as "Main".  In
either case, select the desired section and then add a new item, supplying the
following details when prompted:

Online Program Name: BBS-Scene Onelinerz
Internal Code: 1LINERZ

Select the newly created external program and configure the following values as
shown (substituting the path to your Synchronet BBS installation as required.)

Start-up Directory: /sbbs/xtrn/bbs-scene.org
Command Line: ?onelinerz.js
Multiple Concurrent Users: Yes

All other settings can be left at default values.

randombbs.js
-=-=-=-=-=-=

Create another external program in the same section as follows:

Online Program Name: BBS-Scene Random ANSI
Internal Code: RNDMANSI
Start-Up Directory: /sbbs/xtrn/bbs-scene.org
Command Line: ?randombbs.js
Multiple Concurrent Users: Yes

If you want the Global Onelinerz to appear during your logon process, you have
a couple of options:

1) In SCFG, return to the BBS-Scene Onelinerz external program that you just
   configured, and change the "Execute on Event" setting to "Logon" (selecting
   "No" for "Execute as event only?" when prompted.)
   
2) Place a copy of /sbbs/exec/logon.js in /sbbs/mods/logon.js (if you don't
   already have a copy there) and then insert the following line into an
   appropriate spot:
   
   bbs.exec_xtrn("1LINERZ");
   
If you want to display a random BBS ANSI advertisement on logoff, in SCFG edit
the BBS-Scene Random ANSI external and set its "Execute on Event" value to
"Logoff" (again, select "No" for "Execute as event only?" when prompted.)

onelinerz.ssjs
-=-=-=-=-=-=-=

Users of ecWeb can copy 'onelinerz.ssjs' into the sidebar/ directory of their
ecWeb installation.  Prepending a string of digits to the filename (eg.
renaming it to "005-onelinerz.ssjs") will affect the order in which it is
loaded into the sidebar.  If you rename the file, edit the "scriptname"
variable at the top of the script to reflect the new filename.

Users of the default or custom web interfaces may be able to add the command:

load('/sbbs/xtrn/bbs-scene.org/onelinerz.ssjs');

into their own SSJS or XJS scripts to output a list of recent onelinerz to
the visitor's browser.