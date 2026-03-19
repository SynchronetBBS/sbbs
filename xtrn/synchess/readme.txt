SynChess by Claude, assisted by Nelgin.

This is a chess game for modern versions of Synchronet and SyncTERM
and others that that support jpegXL. Fear not, a less pretty interface
is available for those without. 


The only guarantee you receive with this software is that it works
on my system! If you find any bugs or have any suggestions, you're
welcome to fix them and submit updated to gitlab or report them and
assign to Nigel Reed (Nelgin) and I'll do my best to fix them.


The game features a built-in engine, but it also supports stockfish
for deeper thinking and more natural play.

To install Stockfish:

Debian / Ubuntu		sudo apt install stockfish
			Very reliable; usually includes NNUE support.

Arch Linux		sudo pacman -S stockfish
			Always gets the bleeding-edge version first.

RHEL / CentOS		sudo dnf install stockfish
			May require EPEL or a COPR repo for the newest builds.

Raspbian (Pi)		sudo apt install stockfish
			Optimized for ARM; runs great on Pi 4/5.

Gentoo / Slackware	emerge --ask games-board/stockfish
			Compiling from source is the standard here.

Windows (Modern)	https://stockfishchess.org/download/
			Based on your processor, pick the best option.

MacOS			brew install stockfish
			Homebrew is the easiest install method.



Features:
 Play as white, black, or random.

 Select 1-10 levels which equate to ELOs upto 2200 using the engine and 3000
 using Stockfish.

 Change level during game if it's too easy/hard.

 Turn on/off hints as to which squares a piece can move to.

 Turn on/off ability to undo moves.

 Option to show the name of the opening being played. There are over
 500 opening lines built into the game.

 Options to show all moves, last move or no movies. Last move is useful
 for "blindfold mode".

 Keeps track of wins/losses/draws and by which method (checkmate, agreed
 draw, insufficient material etc).

 Configure light/dark/hint square color and color of pieces.

 White, black or both colors can be marked invisible for "blindfold move"
 type games.

 Per user settings are saved between sessions.

 CTerm compatible terminals will use graphics and able to use the mouse to
 move pieces.

 Various moves via keyboard accepted such as c2c4 is the same as c4. fg5
 or fxg5 are accepted for pawn takes, for example. Disambiguation such as
 Ncd5 accepted as well as c2d5.

 All special moves handled such as castling king and queen side, taking
 en passant, and promotion to queen, rook, bishop, or knight.

 The system will decide whether to take a draw or not based on the position.

 Ability to see PGN of a completed game and opportunity to download it as a
 file.

 Quitting will ask the player if they wish to save the game and it will be
 resumed next time, otherwise if the player has made a move, it will count
 as an abandonded game.

 Caputred pieces are displayed along with points value difference, if either
 player is over 1 point higher in material.

 
 Game will end automatically on 3 move repetition, 50 move rule, or
 insufficient material.

Todo:

 Player v Player mode
 Possibly interbbs mode

 

Setup:

 You can take advantage of the Synchronet XTRN installer to get going. You
 will find instructions at https://wiki.synchro.net/module:install-xtrn

 If you wish to do it the manual way, read on.

 Copy synchess-dist.ini to synchess.ini and make any configuration changes.
 

 Simply add SynChess to your External Programs > Online Programs (Door)
 section of scfg.

[SynChess]
 1: Name                       SynChess
 2: Internal Code              SYNCHESS
 3: Start-up Directory         ../xtrn/synchess
 4: Command Line               ?synchess
 5: Clean-up Command Line
 6: Execution Cost             None
 7: Access Requirements        ANSI AND 80 COLS
 8: Execution Requirements
 9: Multiple Concurrent Users  Yes
10: Native Executable          No
11: I/O Method                 Standard
12: Use Shell or New Context   No
13: Modify User Data           No
14: Execute on Event           No
15: Pause After Execution      No
16: Disable Local Display      No
17: BBS Drop File Type         None
18: Place Drop File In         Node Directory
19: Time Options...

Let your board recycle and you're good to go. 
