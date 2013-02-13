Thirstyville
------------
echicken -at- bbs.electronicchicken.com
February 2013

1) About
2) Installation
3) Local or interBBS


1) About
--------

Inspired by the classic door game "Lemonade", Thirstyville places the player
in the role of a cafe owner.  Players must purchase inventory (which carries
over from one day to the next) and then prepare beverages for sale each day.
Decisions as to how many of each type of beverage to prepare can be based on
the weather forecast and upon demographic information for the neighbourhood.
Choosing the appropriate hot and cold drinks for the weather and for the area
is key: at the end of the day, unsold beverages are discarded at a loss.

Thirstyville is interBBS capable, featuring live updates of player activity,
scores, and a shared pool of ingredients available to be purchased each day.
It's possible for one player to put another out of business by buying up all
of the available ingredients.  Each player is subject to the same demographic
and weather conditions, so while there is some randomness involved the odds
are fairly even.


2) Installation
---------------

In SCFG (BBS->Configure in the Synchronet Control Panel on Windows,) go to
External Programs -> Online Programs (Doors), then choose an Externals section
to add Thirstyville to.  Create a new item with the following details, leaving
all over settings at their default values:

¦Name                       Thirstyville
¦Internal Code              THIRSTY
¦Start-up Directory         ../xtrn/thirsty
¦Command Line               ?thirsty.js
¦Multiple Concurrent Users  Yes


3) Local or interBBS
--------------------

SUGGESTED:
----------

I highly recommend that you configure Thirstyville to connect to my JSON DB
server for interBBS play.  The more people we have playing from multiple BBSs,
the more fun it will be for everyone.  To do so:

Open 'game.ini', and edit the 'server' and 'port' values at the top to read:

server = bbs.electronicchicken.com
port = 10088

If you chose to go this route, you can skip over the "OPTIONAL" section below.

OPTIONAL:
---------
By default Thirstyville is configured to connect to a JSON DB server at the
same host as the BBS it is running on. If you want to keep it this way, you
must ensure that you have the following section in ctrl/services.ini:

[JSON]
Port=10088
Options=STATIC | LOOP
Command=json-service.js

(If you set the port for your JSON DB server to something other than 10088 in
ctrl/services.ini, sure to edit the corresponding 'port' value in game.ini.)

And that you have the following section in ctrl/json-service.ini:

[thirsty]
dir=../xtrn/thirsty/

(After editing services.ini or json-service.ini, you must recycle services or
restart Synchronet for the changes to take effect.)

4) Support
----------

Post a message to 'echicken' in the Synchronet Sysops sub on Dovenet, find me
in #synchronet on irc.synchro.net, or contact me at bbs.electronicchicken.com.

echicken