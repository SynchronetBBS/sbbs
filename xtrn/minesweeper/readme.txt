Synchronet Minesweeper - Sysop Documentation
============================================
It's the classic game you know and love, written from scratch for Synchronet
BBS in JavaScript (ES5).

by Digital Man, September 2019

Details
-------
The objective was to try to mimic the old Microsoft versions of the game that
I remember playing in the 1990's, but support more difficulty levels (5) and
maximize the number of cells usable in terminals, while keeping the game
playable and easy on the eyes (my daughter, Emma, helped with the color scheme,
as she is *not* colorblind). There's no mouse support; use the keyboard.

Multiple users can play the game simultaneously, but there is no inter-user
interaction of any kind. The normal Synchronet inter-node paging/listing
controls (Ctrl-P/U) are disabled while running the game.

The game supports ANSI and PETSCII terminals from display widths (columns) of
40 to 132+ and heights (rows) of 24+.

For more details on game play, see winesweeper.hlp (the "Help" text available
while running the game).

Winners
-------
The game winners are stored in an ever-growing line-delimited JSON file
(winners.jsonl). If you ever want to reset the list of local winners, you can
just delete that file. The winners are ranked (sorted) first based on
difficulty level (higher levels are harder) and then by elapsed game time
(lower times are better). There may be fractional difficulty levels (e.g. 3.3)
to compensate for terminal size limitations (e.g. not all terminals can support
a level 5 30x30 game grid). The mine density (percentage of cells occupied by
mines, based on selected difficulty level) is retained however, regardless of
calculated/used game grid size.

If you have a "syncdata" message area setup (networked via DOVE-Net or FidoNet)
your game winners will be automatically shared to that message base for
visibility on other BBSes and when viewing your game winners, those from other
BBSes will also appear in the rankings.

The "top X" value (number of winners) displayed is configurable and defaults
to 20. It's quite possible to "win" the game but not appear in the winners
list depending on what other players have accomplished and the configured
display threshold.

Losers
------
Lost games are logged in an ever-growing line-delimited JSON file
(losers.jsonl). The combined logs of local winners and losers is visible from
within the game.

Requirements
------------
Tested with Synchronet v3.17c (in development). Older versions of Synchronet
(e.g. v3.17b) may work however.

An up-to-date set of exec/load/*.js files (from cvs.synchronet) are needed.

Install
-------
Add to SCFG->External Programs->Online Programs(Doors)->Games->
Available Online Programs...

    Name                       Synchronet Minesweeper       
    Internal Code              MSWEEPER                     
    Start-up Directory         ../xtrn/minesweeper          
    Command Line               ?minesweeper                 
    Clean-up Command Line                                   
    Execution Cost             None                         
    Access Requirements                                     
    Execution Requirements                                  
    Multiple Concurrent Users  Yes                          
    Intercept I/O              No                           
    Native Executable          No                           
    Use Shell to Execute       No                           
    Modify User Data           No                           
    Execute on Event           No                           
    Pause After Execution      No                           
    BBS Drop File Type         None                         
    Place Drop File In         Node Directory
    
Since it is a JavaScript module, most of the settings don't apply and should
be left default/disabled/No/None.

Optionally, if you want the top-X winners displayed after exiting game, set:
    Clean-up Command Line      ?minesweeper winners

If you want the top-x winners displayed during logons, create an additional
program entry:

    Name                       Synchronet Minesweeper Winners
    Internal Code              MSWINNER
    Start-up Directory         ../xtrn/minesweeper
    Command Line               ?minesweeper winners
    ...
    Execute on Event           Logon, Only

Configure
---------
Command-line arguments supported:

"winners [num]"	- display list of top-[num] winners and exit
"nocls"   		- don't clear the screen upon exit
<level>   		- set the initial game difficulty level (1-5)

The ctrl/modopts.ini may contain a [minesweeper] section. The following
options are supported (with default values):
    sub = syncdata
    timelimit = 60
    timewarn = 5
    winners = 20
    difficulty = 1
    selector = 0
    highlight = true

The user's choice of the following settings is saved to their user properties
file (data/user/*.ini) and used for future game plays:

    difficulty
    selector
    highlight

Upgrade
-------
If you were a lucky "early adopter" and had rev 1.7 running on your BBS,
you probably noticed the following important changes introduced in rev 1.8:
    
    * exec/minesweeper.js was moved to xtrn/minesweeper/minesweeper.js
    * text/minesweeper.hlp was moved to xtrn/minesweeper/minesweeper.hlp
    * data/minesweeper.jsonl was moved to xtrn/minesweeper/winners.jsonl
    
If you already had game winners and want to retain them in the list, copy or
rename the data/minesweeper.jsonl to xtrn/minesweeper/winners.jsonl. If you
don't care, the game will run fine and new winners will be added to the
new/correct filename.

$Id: readme.txt,v 2.1 2019/10/07 20:53:14 rswindell Exp $
