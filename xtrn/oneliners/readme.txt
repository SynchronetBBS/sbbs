Synchronet Oneliners
by echicken

Contents

1) Installation
1.1) Connecting to a remote wall
1.2) Hosting your own wall
1.3) External program configuration
1.4) XJS oneliners lister for the web
2) Rules (there are none)
3) Support
4) To-do list

1) Installation

1.1) Connecting to a remote service

Connecting to a remote service means accessing a shared, inter-BBS oneliners
wall.  Most sysops will likely want to go this route, as it will mean more
activity and "fun" for everybody.

Edit the 'settings.ini' file in your 'xtrn/oneliners/' directory to read as
follows:

server=bbs.electronicchicken.com
port=10088

Your oneliners scripts will now access the shared wall that I host.

1.2) Hosting your own wall

If you'd prefer to host your own wall, just leave the 'settings.ini' file in
your 'xtrn/oneliners/' directory as-is.

If you want other people to be able to connect to your wall, you'll need to
ensure that the JSON-DB service is enabled on your BBS, and that the port that
you choose for it is open and forwarded to your BBS.  To enable the JSON-DB
service (if you haven't already,) add the following section to your
'ctrl/services.ini' file:

[JSON-Service]
Port=10088
Options=STATIC|LOOP
Command=json-service.js

You'll also need to add the following to your 'ctrl/json-service.ini' file:

[oneliners]
dir=../xtrn/oneliners/

1.3) External Program Configuration

In 'scfg' (that's 'BBS->Configure' from the Synchronet Control Panel in
Windows,) go to 'External Programs->Online Programs (Doors)', select or create
the section you want to add the oneliners program to, select 'Available Online
Programs', hit enter on a blank line, and configure the external program as
follows:

Online Program Name: Synchronet Oneliners
Internal Code: ONELINER
Start-up Directory: ../xtrn/oneliners
Command Line: ?oneliners.js
Multiple Concurrent Users: Yes

If you want this program run automatically when a user logs in, set 'Execute
on Event' to 'Logon', and answer 'No' to 'Execute as Event Only.'

There are a bunch of other options that you can leave in their default states.

1.4) XJS oneliners lister for the web

If you're running ecWeb, you can place the "oneliners.xjs" file in your
"web/root/ecwebv3/sidebar/" directory.  This will add a "Synchronet Oneliners"
sidebar module listing the (by default) five most recent oneliners.  Adjusting
the number of displayed oneliners is a simple matter of changing the value of
the "show" variable at the top of the script.

As always, you can rename the file when placing it in the sidebar directory to
change the order in which it will be loaded ("004-oneliners.xjs", etc.)
Sidebar modules are loaded based no the numeric and then alphabetical order of
their filenames.

There is nothing about this script that makes it particular to ecWeb.  If
you're running the stock Synchronet web interface or something of your own
devising, this file can always be loaded on its own, or in an iframe or
whatever works for you.

2) Rules (there are none)

I don't care if people want to spam my shared oneliners wall with profanity
or BBS advertisements.  I'm not interested in policing or censoring the
content.  Everyone's free to set up their own wall and manage it as they like.

3) Support

If you need help, there are a few options:

- Post a message to 'echicken' in the 'Synchronet Sysops' sub on DOVE-Net
- Send an email to echicken -at- bbs.electronicchicken.com
- Find me in #synchronet on irc.synchro.net

A oneliners wall is not a great place for support discussions.  You can post
your complaints and problems there, but if I see them I'll either ignore them
or ask you to contact me via one of the above methods.

4) To-do list

- Make the colours of various elements configurable
- Create an SSJS script to allow listing/posting oneliners via the web
- Create a web API to enable access for non-Synchronet systems