Star Trek
For Synchronet 3.16
-------------------
echicken -at- bbs.electronicchicken.com
February 2013


Installation
------------

In SCFG (BBS->Configure in the Synchronet Control Panel on Windows,) go to
External Programs -> Online Programs (Doors), then choose an Externals section
to add Star Trek to.  Create a new item with the following details, leaving
all over settings at their default values:

¦Name                       Star Trek
¦Internal Code              STARTREK
¦Start-up Directory         ../xtrn/startrek
¦Command Line               ?startrek.js
¦Multiple Concurrent Users  Yes


Star Trek features an interBBS scoreboard.  To use the scoreboard on my JSON
DB server, open 'server.ini', and edit the 'host' and 'port' values at the top
to read:

host = bbs.electronicchicken.com
port = 10088

If you would rather host your own scoreboard, ensure that you are running the
JSON DB service (you should have something like the following in your
ctrl/services.ini file):

[JSON]
Port=10088
Options=STATIC | LOOP
Command=json-service.js

And that you have the following section in ctrl/json-service.ini:

[startrek]
dir=../xtrn/startrek/

(After editing services.ini or json-service.ini, you must recycle services or
restart Synchronet for the changes to take effect.)


Support
-------

Post a message to 'echicken' in the Synchronet Sysops sub on Dovenet, find me
in #synchronet on irc.synchro.net, or contact me at bbs.electronicchicken.com.

echicken