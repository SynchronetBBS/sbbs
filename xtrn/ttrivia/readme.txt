Tournament Trivia for Synchronet BBS - Installation Guide
==========================================================

This is a Synchronet JavaScript port of Tournament Trivia v1.0 by Evan Elias. It
runs natively in Synchronet without needing OpenDoors, XSDK, or any external
door kit.

This was ported from the C++ source code to Synchronet JavaScript using Claude
AI, then small parts of it were manually tweaked.

                From: Eric Oulashin (AKA Nightfox)
               Email: eric.oulashin@gmail.com
                 BBS: Digital Distortion
BBS internet address: digitaldistortionbbs.com
   Alternate address: digdist.synchro.net

REQUIREMENTS
------------
- Synchronet BBS v3.x or later
- JavaScript support enabled (default in modern Synchronet)

ARCHITECTURE
------------
No client-server split - each BBS node runs its own script instance, coordinating through shared JSON files with file locking
Shared game state via data/game_state.json - all nodes see the same question, clues, and active players
Broadcast messages via data/messages.json - polled every 500ms for real-time notifications
CP437 characters use raw byte values (same convention as Synchronet's cp437_defs.js)
Compatible with original .ENC encoded question files and plain-text .QST/.TXT files

FEATURES PORTED FROM THE C++ VERSION
------------------------------------
- Multi-node simultaneous play with shared questions
- Question timer with automatic clue generation
- Answer checking with progressive letter reveal on wrong answers
- Scoring system (1-3 points based on clues needed, single-player always 1)
- Monthly score reset with previous winner tracking
- All commands: WHO, SKIP, SCORES, TELL, HELP, MENU, CORRECTION, SUBMIT, EXIT
- Single-letter shortcuts (S, P, N, T, C, H, X, ?, !)
- Chat/say for non-command text (broadcast to all players)
- Sysop configuration menu (question frequency, max clues, question files, etc.)
- Stale player cleanup using Synchronet's node list


FILES INCLUDED
--------------

These are the files included with Tournament Trivia JS:

      ttriv.js: This is the script to run. It has the game loop and all commands
 ttriv_game.js: Game state, questions, scoring, multi-node coordination
   ttriv_ui.js: Display helpers (colors, CP437 box-drawing, text formatting)
ttrivconfig.js: Configurator script which can be run on your BBS machine using
                jsexec, as a way to set configuration settings
    readme.txt: This file
  database.enc: Questions & answers from the original Tournament Trivia release
    custom.txt: Custom questions & answers from the original Tournament Trivia release

Questions from all configured question files will be included in the game.

Data files:

The game automatically creates a data/ subdirectory for:
  settings.json: Game configuration
   players.json: Player scores (reset monthly)
game_state.json: Current game state (shared between nodes)
  messages.json: Inter-node broadcast messages

These files are managed automatically. To reset scores, delete
players.json. To reset all game state, delete the entire data/
directory.


CONFIGURATION
-------------
If you want to create/edit the configuration, the script ttrivconfig.js is
included to make it a little more convenient. You can run ttrivconfig.js on
your BBS machine in the ttrivia directory using jsexec. For instance:

jsexec ttrivconfig.js

By default, it will edit the configuration file data/settings.json. You can
specify an alternate configuration file if you wish by using the
-settings=<filename> command-line option. For instance:

jsexec ttrivconfig.js -settings=otherSettings.json

That will create otherSettings.json in the data directory.

The settings file is in JSON format, so once it's created, it can be edited
using a regular text editor.


INSTALLATION
------------

1. In SCFG (Synchronet Configuration), go to:

       External Programs -> Online Programs (Doors)

   Add a new external program with these settings:

       Name:                   Tournament Trivia
       Internal Code:          TTRIV
       Start-up Directory:     ../xtrn/ttriv
       Command Line:           ?ttriv.js
       Multiple Concurrent Users: Yes

   The "?" prefix tells Synchronet to run it as a JavaScript module.
   Set "Multiple Concurrent Users" to Yes for multi-node play.

   For the settings, it will use settings.json in the data directory by
   default. You can specify an alternate configuration file if you wish
   by using the -settings=<filename> command-line option. For instance:

   ?ttriv.js -settings=otherSettings.json

   That would make it use otherSettings.json in the data directory.

2. (Optional) Create a custom.txt file with your own trivia questions.
   Format: alternating lines of question and answer:

       What is the capital of France?
       Paris
       Who wrote Romeo and Juliet?
       William Shakespeare

   Up to 10 question files can be configured via the in-game
   sysop configuration menu (type SYSOP or CONFIG during gameplay
   if you are logged in as a sysop with security level >= 90).


MULTI-NODE PLAY
---------------

Multiple users can play simultaneously. The game coordinates
through shared JSON files with file locking. All players see
the same question, and the first player to answer correctly
earns the points. Points are awarded based on how few clues
were needed:

    0 clues given:  3 points
    1 clue given:   2 points
    2+ clues given: 1 point
    Single player:  always 1 point

SYSOP CONFIGURATION
-------------------

Sysops (security level >= 90) can type SYSOP or CONFIG during
gameplay to access the configuration menu. Settings include:

    Question Frequency:  25-75 seconds between questions (default: 50)
    Max Clues:          0-4 clues per question (default: 3)
    Show Sysops:        Include sysops on the score list
    Player Timeout:     60-600 seconds of inactivity (default: 300)
    Question Files:     Up to 10 question file paths

COMMANDS
--------

Players can type these commands during gameplay:

    ?  / MENU       - Show the command menu
    S  / SCORES     - View top 10 scores this month
    P  / PLAYERS    - List online players
    N  / SKIP       - Vote to skip the current question
    T  / TELL       - Send a private message to another player
    C  / CORRECTION - Report a question/answer error
    H  / HELP       - Show instructions
    X  / EXIT       - Leave the game

Or just type your answer to the current question!

QUESTION FILE FORMAT
--------------------

Question files are plain text with alternating lines:
    Line 1: Question text (max 160 characters)
    Line 2: Answer text (max 80 characters)
    Line 3: Next question...
    Line 4: Next answer...

Files with the .enc extension use the original Tournament Trivia
encoding (compatible with the C++ version's database.enc file).

CREDITS
-------

Original Tournament Trivia:  (c) 2003 Evan Elias
                              http://trivia.doormud.com

Synchronet JavaScript port:   Ported with the help of Claude AI
