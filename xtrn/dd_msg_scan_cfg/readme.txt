                    Digital Distortion Message Scan Config
                                Version 1.00
                           Release date: 2025-09-23

                                     by

                                Eric Oulashin
                          Sysop of Digital Distortion
                  BBS internet address: digitaldistortionbbs.com
                      Alternate address: digdist.synchro.net
                        Email: eric.oulashin@gmail.com

This file describes Digital Distortion Message Scan Config.

Contents
========
1. Disclaimer
2. Introduction
3. Files included
4. Configuration file
5. Installation
   - Loadable Modules setup
   - Command shell setup
   - Background: Running JavaScript scripts in Synchronet
   - Synchronet command shell background
   - Installing into a command shell


1. Disclaimer
=============
I cannot guarantee that this script is 100% free of bugs.  However, I have
tested it, and I used it often during development, so I have some confidence
that there are no serious issues with it (at least, none that I have seen).


2. Introduction
===============
This is a JavaScript script for Synchronet that provides lightbar interface for
the user to change their message scan configuration, toggling their unread &
to-you options for sub-boards in the various message groups. If the user's
terminal doesn't support ANSI, this script will fall back to a traditional user
interface. You can also set the interface type to Traditional if you don't
want to use the lightbar interface.

When this script runs, a lightbar menu will appear to let the user choose a
message group; when the user chooses a message group, the sub-boards for that
group will be listed with options to toggle the unread & to-you scan options.
For example:

                      Choose a message group to configure
    # Description                                                       # Subs
  ┌───────────────────────────────────────────────────────────────────────────┐
  │ 1 Local message areas                                                   20│
  │ 2 MusicalNet                                                            30│
  │ 3 DeveloperNet                                                          27│
  │ 4 DOVE-Net                                                              23│
  │ 5 AgoraNet                                                              10│
  │ 6 FidoNet                                                              211│
  │ 7 FSXNet                                                                14│
  │ 8 HobbyNet                                                              30│
  │ 9 HealthNet                                                             15│
  │10 StarNet                                                               16│
  │11 League 10                                                             17│
  │12 USENET Newsgroups|Misc.                                               17│
  │13 USENET Newsgroups|BBS                                                 33│
  │14 USENET Newsgroups|Computer                                             7│
  └───────────────────────────────────────────────────────────────────────────┘

                                   FidoNet
        U: Toggle unread for all on/off; Y: Toggle to-you for all on/off
   Sub-board                                                     Unread To-you
  ┌───────────────────────────────────────────────────────────────────────────┐
  │  1 10th Amendment Echo                                     ▒  [ ]    [√]  │
  │  2 Hourly Echomail stats from 1:229/426                    ░  [ ]    [√]  │
  │  3 AARP Fraud Warning Network news                         ░  [ ]    [√]  │
  │  4 disABLED Users Information Exchange.                    ░  [ ]    [√]  │
  │  5 AdeptXBBS Sysop-Only Help and Discussion                ░  [ ]    [√]  │
  │  6 The International AFTERSHOCK Android Cli                ░  [ ]    [√]  │
  │  7 Alaska Off Topic Chatter                                ░  [ ]    [√]  │
  │  8 Allfix File Announce Conference                         ░  [ ]    [√]  │
  │  9 Astronomy                                               ░  [√]    [√]  │
  │ 10 Aviation                                                ░  [√]    [√]  │
  │ 11 I'ntl ALLFIX Support                                    ░  [ ]    [√]  │
  │ 12 Politics Unlimited                                      ░  [ ]    [√]  │
  │ 13 [GNG] Gated, Filtered newsgroup alt.bbs.                ░  [ ]    [√]  │
  │ 14 [GNG] Gated, Filtered alt.comp.anti-viru                ░  [ ]    [√]  │
  │ 15 AMATEUR RADIO / HAM RADIO                               ░  [√]    [√]  │
  │ 16 Amiga International Echo                                ░  [ ]    [√]  │
  └───────────────────────────────────────────────────────────────────────────┘

For each sub-board, the user can move a highlighted selection between unread and
to-you and toggle it by pressing the Enter key. The user can also type a number
corresponding to the item on the list. Then, the user can exit out of the menus
by pressing either Q or ESC.


3. Files included
=================
These are the files included:
- dd_msg_scan_cfg.js: The script to be run/executed
- dd_msg_scan_cfg.example.ini: The default configuration file - Copy this to
  dd_msg_scan_cfg.ini and make your changes. You can also copy the file to your
  sbbs/mods directory if you want.
- dd_msg_scan_cfg_default_theme.ini: The default color theme configuration file
- readme.txt: This file
- version_history: The version history of Digital Distortion Message Scan Config


4. Configuration file
=====================
There is a default configuration file, dd_msg_scan_cfg.example.ini. Copy that
to dd_msg_scan_cfg.ini and then edit that with any text editor. The settings are
in the format setting=value, where "setting" is the name of the setting or
color, and "value" is the corresponding value to use.

There is an included theme configuration file,
dd_msg_scan_cfg_default_theme.ini. If you want to change any of the color
settings, make a copy of that file and edit it with any text editor, and change
the value of the themeFilename setting in your dd_msg_scan_cfg.ini to specify
the name of your theme file. The colors specified in the theme configuration
file are Synchronet color/attribute codes (without the control character).

Comments are allowed in the configuration files - Commented lines begin with a
semicolon (;).

The configuration & theme file can be in the same directory as dd_msg_scan_cfg.js
(by default, xtrn/dd_msg_scan_cfg).  However, you can also place your
configuration & theme files in one of the following directories:
 sbbs/mods (this directory will be checked first)
 sbbs/ctrl (this directory will be checked second)

The directory where the reader is located (xtrn/dd_msg_scan_cfg) will be checked
third. If your configuration & theme files are not in sbbs/mods or sbbs/ctrl,
then the one in xtrn/dd_msg_scan_cfg will be used.

The configuration settings are described in the sections below:

Main configuration file (dd_msg_scan_cfg.example.ini)
----------------------------------------------------
BEHAVIOR section:

Setting                               Description
-------                               -----------
interfaceStyle                        The user interface style to use. Can be
                                      Lightbar or Traditional.

COLORS section:

Setting                               Description
-------                               -----------
themeFilename                         The name of the theme configuration file
                                      to use


Theme configuration file
------------------------
The theme configuration file starts with a line that says [COLORS]. Currently,
that is the only section; that is the section that specifies the various colors.

Setting                               Description
-------                               -----------
msgGrpHdr                             Message group name header

msgGrpBorderColorCorner1              Message group menu border corner color 1

msgGrpBorderColorCorner2              Message group menu border corner color 2

msgGrpBorderColorCorner3              Message group menu border corner color 3

msgGrpBorderColorNormal               Message group menu border normal color

msgGrpMenuNumColor                    Message group number color

msgGrpMenuDescColor                   Message group description color

msgGrpMenuNumItemsColor               Color for the number of items text in the
                                      group menu

msgGrpMenuNumHighlightColor           Menu highlight color for the group number

msgGrpMenuDescHighlightColor          Menu highlight color for the group
                                      description

msgGrpMenuNumItemsHighlightColor      Menu highlight color for the number of
                                      items in the message group menu

lightbarHelpLineBkg                   Lightbar help line background color

lightbarHelpLineGeneral               Lightbar help line general text color

lightbarHelpLineHotkey                Lightbar help line hotkey color

lightbarHelpLineParen                 Lightbar help line parenthesis color

lightbarNumInputPromptText            Color for the # input prompt for the
                                      lightbar interface

lightbarNumInputPromptColon           Color for the colon used in the # input
                                      prompt for the lightbar interface

lightbarNumInputPromptUserInput       Color for user input for the # input for
                                      the lightbar user interface

traditionalNumInputPromptText         Color for the # input prompt for the
                                      traditional/non-lightbar interface

traditionalNumInputPromptColon        Color for the colon used in the # input
                                      prompt for the traditional/non-lightbar
                                      interface

traditionalNumInputPromptUserInput    Color for user input for the # input for
                                      the traditional/non-lightbar user interface

subBoardMenuNumColor                  Color for the item number in the sub-board
                                      menu

subBoardMenuItemColor                 Color for the item text in the sub-board
                                      menu

subBoardMenuSelectedItemColor         Color for the selected item text in the
                                      sub-board menu

toggleMenuItemColor                   Color for item text in the toggle menus
                                      for unread/to-you

toggleMenuSelectedItemColor           Color for item text in the toggle menus
                                      for unread/to-you

helpText                              The color to use for help text

errorMsg                              The color to use for error messages


5. Installation
================
One way to install this would be to edit your command shell and add an entry to
one of your menus to run dd_msg_scan_cfg.js.

For instance, if you're using the default JavaScript command shell (default.js),
first look for the section defining which menu you want to add the command to
which will run the script.  As an example, the main menu commands are defined
starting with the line that looks like this:

const main_menu = {

Within there, there is a line that looks like this:
        command: {
That 'command' line is basically the beginning of a set of commands for the main
menu that perform various actions.  In there, you can add a command to run the
script similar to this:
         '/R': { eval: 'bbs.exec("?../xtrn/dd_msg_scan_cfg/dd_msg_scan_cfg.js")' },
In this example, when the user types /R, it will run the script. You can change
the command string/hotkey ('/R') to whatever you like.

If you're using your own custom command shell, you'd have to just add the
following to any new command of one of your menus:

bbs.exec("?../xtrn/groupie/dd_msg_scan_cfg.js")

Alternately, you could add an entry in External Programs to run the script in
SCFG; then, in your command shell, you could use bbs.exec_xtrn() to run it with
the internal code.

This is an example of a SCFG entry for External Programs to run the script:
╔═════════════════════════════════════════════════════[< >]╗
║                 DD Message Scan Config                   ║
╠══════════════════════════════════════════════════════════╣
║ │Name                       DD Message Scan Config       ║
║ │Internal Code              DD_MSG_SCAN_CFG              ║
║ │Start-up Directory         ../xtrn/dd_msg_scan_cfg      ║
║ │Command Line               ?dd_msg_scan_cfg.js          ║
║ │Clean-up Command Line                                   ║
║ │Execution Cost             None                         ║
║ │Access Requirements                                     ║
║ │Execution Requirements                                  ║
║ │Multiple Concurrent Users  Yes                          ║
║ │I/O Method                 FOSSIL or UART               ║
║ │Native Executable/Script   Yes                          ║
║ │Use Shell or New Context   No                           ║
║ │Modify User Data           No                           ║
║ │Execute on Event           No                           ║
║ │Pause After Execution      No                           ║
║ │Disable Local Display      No                           ║
║ │BBS Drop File Type         None                         ║
╚══════════════════════════════════════════════════════════╝

Then, in the above examples for editing a JavaScript command shell, instead of
using bbs.exec(), you would use this:

bbs.exec_xtrn("DD_MSG_SCAN_CFG");
