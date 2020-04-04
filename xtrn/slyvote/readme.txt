                                   SlyVote
                                 Version 1.07
                           Release date: 2020-04-04

                                     by

                                Eric Oulashin
                                AKA Nightfox
                          Sysop of Digital Distortion
                  BBS internet address: digitaldistortionbbs.com
                     Alternate address: digdist.synchro.net
                        Email: eric.oulashin@gmail.com



Contents
========
1. Disclaimer
2. Introduction
3. Installation & Setup
4. Configuration file
5. Notes for sysops


1. Disclaimer
=============
I cannot guarantee that this script is 100% free of bugs.  However, I have
tested it, and I used it often during development, so I have some confidence
that there are no serious issues with it (at least, none that I have seen).


2. Introduction
===============
SlyVote is a voting booth door for Synchronet, which allows users to add and
vote on various topics with multiple-choice answers.  SlyVote makes use of the
voting capabilities in the Synchronet messagebase, which were added to
Synchronet 3.17.  Thus, SlyVote requires version 3.17 or higher of Synchronet,
along with the latest JavaScript files for Synchronet, such as the following:
- sbbsdefs.js
- text.js
- frame.js
- scrollbar.js
- dd_lightbar_menu.js
- smbdefs.js
- avatar_lib.js
- Possibly other JavaScript files

SlyVote requiers Synchronet 3.17 or newer, since SlyVote makes use of the
messagebase poll feature and user avatar feature, both of which were added in
Synchronet 3.17.

SlyVote also requires an ANSI terminal, since SlyVote makes use of lightbar
menus which do cursor movement, etc.

Since SlyVote makes use of Synchronet's messagebase, SlyVote can be configured
to use one or more of your message sub-boards to work with.  If more than one
sub-board is specified, SlyVote will prompt the user which sub-board to use
when launching SlyVote.  You might want to create a separate local sub-board
just for poll voting topics, or perhaps multiple local sub-boards for poll
voting if you want to have separate categories of poll topics.  Also, it's
certainly possible to use SlyVote with a networked sub-board, as long as the
sub-board supports voting.  Message sub-boards that support voting would need
to be networked only with other Synchronet BBSes (Dove-Net is a common one,
although a few BBSes on Dove-Net are not Synchronet).  FidoNet networks and
other message networks that might not have Synchronet BBSes should not support
voting by default in Synchronet.

SlyVote also supports avatars, which was added in Synchronet 3.17.  Users can
select a small ANSI graphic "avatar" to represent them, and SlyVote will
display the avatar of the people who posted polls.  Avatar support requires
smbdefs.js and avatar_lib.js, as well as the latest sbbsdefs.js.

SlyVote stores user configuration files, which stores the user's last-read
message numbers in the message areas, which are used when the user is viwing
poll results.  Even though Synchronet can keep track of the user's last-read
message numbers, they are stored separately by SlyVote so that they don't
interfere with the user's normal message reading.  The SlyVote user
configuration files are stored in sbbs/data/user, with the filenames in the
format <user number>.slyvote.cfg.  The user number is 0-padded up to 4 digits.
For example, user 1 would be 0001.slyvote.cfg.

SlyVote's look and feel was initially based on DCT Vote, a voting booth door
which was available in the 1990s.


3. Installation & Setup
=======================
First, ensure that you have an up-to-date Synchronet system running Synchronet
3.17 or higher, and ensure that your Synchronet JavaScript files (in
sbbs/exec/load) are up to date.  Before the official Synchronet 3.17 is/was
released, daily 3.17 beta builds can be downloaded from Vertrauen (the home
BBS of Synchronet).  See the section below titled "Updating Synchronet".

Setting up SlyVote
------------------
SlyVote is comprised of the following files:
1. slyvote.js             The SlyVote script

2. slyvote.cfg            The SlyVote configuration file

3. dd_lightbar_menu.js    A JavaScript lightbar menu class used by SlyVote.
                          You do not need to edit or use this file directly.
                          You can keep this file in the SlyVote directory or
                          copy it to your sbbs/exec/load directory if it's
                          not already there.  This file has been added to
                          Synchronet's CVS source code repository in
                          sbbs/exec/load, so it may already be part of your
                          Synchronet setup if you have updated your JavaScript
                          files.

slyvote.cfg is a plain text file, so it can be edited using any text editor.

Installing into Synchronet's external programs menu
---------------------------------------------------
This section assumes that SlyVote is installed in the directory
sbbs/xtrn/slyvote.

To set up SlyVote in the Synchronet external programs menu:
1. Run SCFG
2. Choose "External Programs" and then "Online Programs (Doors)"
3. Choose a program section, and then add an entry that is set up as follows:
+[¦][?]----------------------------------------------------+
¦                          SlyVote                         ¦
¦----------------------------------------------------------¦
¦ ¦Name                       SlyVote                      ¦
¦ ¦Internal Code              SLYVOTE                      ¦
¦ ¦Start-up Directory         ../xtrn/slyvote              ¦
¦ ¦Command Line               ?slyvote.js                  ¦
¦ ¦Clean-up Command Line                                   ¦
¦ ¦Execution Cost             None                         ¦
¦ ¦Access Requirements        ANSI                         ¦
¦ ¦Execution Requirements                                  ¦
¦ ¦Multiple Concurrent Users  Yes                          ¦
¦ ¦Intercept I/O              No                           ¦
¦ ¦Native Executable          No                           ¦
¦ ¦Use Shell to Execute       No                           ¦
¦ ¦Modify User Data           No                           ¦
¦ ¦Execute on Event           No                           ¦
¦ ¦Pause After Execution      No                           ¦
¦ ¦BBS Drop File Type         None                         ¦
¦ ¦Place Drop File In         Node Directory               ¦
+----------------------------------------------------------+


Installing into a command shell
-------------------------------
This section assumes that SlyVote is installed in the directory
sbbs/xtrn/slyvote.

If you are unsure which command shell you are using, you are likely using
Synchronet's "default" command shell, which is contained in the files
default.src and default.bin in the synchronet/exec directory.  To modify the
default command shell, you'll need to open default.src with a text editor.
Then, you can add a menu command to run SlyVote as follows:
  exec "?../xtrn/slyvote/slyvote.js"

If you are using a JavaScript command shell, the process would be similar.  You
can add a menu command to run SlyVote as follows:
bbs.exec("?../xtrn/slyvote/slyvote.js");


Updating Synchronet
-------------------
SlyVote requires Synchronet 3.17 or higher, along with the latest Synchronet
JavaScript files - at least the ones which were included with Synchronet 3.17.
The Synchronet 3.17 fresh installer can be downloaded here:
ftp://ftp.synchro.net/sbbs317b.zip
The Synchroent 3.17 update package (for upgrading from older versions of
Synchronet) can be downloaded here:
ftp://ftp.synchro.net/sbup317b.zip


4. Configuration file
=====================
The format of slyvote.cfg is setting=value.  slyvote.cfg supports the following
settings:

Setting                               Description
-------                               -----------
showAvatars                           Whether or not to show avatars of the
                                      people who post polls in the results.
                                      Valid values are true and false.
                                      Defaults to true.

useAllAvailableSubBoards              Whether or not to use all available
                                      sub-boards where voting is enabled.
                                      Valid values are true and false.  This
                                      defaults to true.  If this is set to
                                      true, then any subBoardsCodes values
                                      in the configuration file (described
                                      below) will be ignored.

subBoardCodes                         A comma-separated list of internal
                                      sub-board codes specifying which
                                      sub-board or sub-boards to use for voting
                                      topics.  A single sub-board code can be
                                      specified if you only want to use one
                                      sub-board with SlyVote.  Also,
                                      subBoardCodes can appear multiple times
                                      in slyvote.cfg, and all specified
                                      sub-boards will be used.  If any of the
                                      specified sub-board codes don't exist or
                                      refer to sub-boards that don't allow
                                      voting, then they will not be used.

startupSubBoardCode                   Optional: An internal sub-board code for
                                      a sub-board to automatically start in
                                      if there are multiple sub-boards
                                      configured.  If this is set, SlyVote
                                      will not prompt the user for a sub-board
                                      on startup and will start in the sub-board
                                      specified by this setting.

5. Notes for sysops
===================
For sysops, polls can be deleted when viewing results from SlyVote.

SlyVote uses the following lines from Synchronet's text.dat file (located in
the sbbs/ctrl directory):
120 (CantPostOnSub)
503 (SelectItemWhich)
759 (CantReadSub)
779 (VotingNotAllowed)
780 (VotedAlready)
781 (R_Voting)
787 (PollVoteNotice)
791 (BallotHdr)