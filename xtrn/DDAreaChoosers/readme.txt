                     Digital Distortion Area Choosers
                              Version 1.20
                        Release date: 2020-04-19

                                  by

                             Eric Oulashin
                     Sysop of Digital Distortion BBS
                 BBS internet address: digdist.bbsindex.com
                     Email: eric.oulashin@gmail.com



This file describes the Digital Distortion area chooser scripts.

Contents
========
1. Disclaimer
2. Introduction
3. Installation & Setup
4. Configuration file
5. DDMsgAreaChooser class: Properties & methods
6. DDFileAreaChooser class: Properties & methods


1. Disclaimer
=============
The only guarantee that I can make about these scripts is that they will take
up space on your computer.  I have tested these with the Windows verison of
Synchronet 3.15 and 3.16 in Windows 2000 and Windows XP, and Synchronet 3.17 in
Windows 7.
I created these scripts to customize the message & file area selection on my
BBS and am providing them to the Synchronet BBS community in case other
Synchronet sysops might find them useful.


2. Introduction
===============
The Digital Distortion message & file area chooser scripts provide a lightbar
or traditional user interface to let the user choose their message and file
area.  The colors are customizable via a configuration file.

By default, the scripts will let the user choose both the message group and
sub-board, and file library and directory - There is also a command-line
parameter that will only allow the user to choose a message sub-board within
their current message group, or file directory only within their current file
library, if desired.    Additionally, the message area
chooser will show the date & time of the latest post for each of the
sub-boards.

The file & message area chooser scripts can be run in several ways:
- Executed from a JavaScript/Baja script
- Loaded in a JavaScript script, then use the DDMsgAreaChooser or
  DDFileAreaChooser object to let the user choose their message/file area
- Set up as a door

These scripts require Synchronet version 3.14 or higher.


3. Installation & Setup
=======================
Step 1: Copy the following files to a directory of your choice (i.e., sbbs/exec
or sbbs/mods):
DDFileAreaChooser.js
DDMsgAreaChooser.js
DDMsgAreaChooser.cfg
DDFileAreaChooser.cfg

Step 2: Set up the scripts to run on your BBS.  To do so, you will need to
update your command shell to execute the included script instead of
Synchronet's built-in area chooser functionality.  Synchronet does not use a
traditional menu system like some other BBS packages do; rather, Synchronet
uses a "command shell", which is a script that handles user input, performs
actions based on the user's input, displays menu files, etc.  If you have not
modified your Synchronet installation much, you may be using the default
command shell, which (at the time of this writing) is default.src and its
compiled version, default.bin, which is written in Baja (Synchronet's own
scripting language).  It's also possible to use a command shell written in
JavaScript - Synchronet includes a few examples, such as classic_shell.js.

For these examples, I will use DDMsgAreaChooser.js.

To use these scripts in a Baja script (such as the 'default' script mentioned
above), you can run the area chooser as follows:
exec "?DDMsgAreaChooser.js"

If you're using a JavaScript command shell, you can run it this way:
bbs.exec("?DDMsgAreaChooser.js");

To use the message area chooser in the 'default' command shell, search for the
text "msg_select_area" (without the double-quotes).  It should be underneath a
line that says "cmdkey J" (meaning the J key is used to jump to another message
area).  Replace msg_select_area with this line:
exec "?DDMsgAreaChooser.js"

The process is similar for the file area chooser - Search for file_select_area
and replace that with the following:
exec "?DDFileAreaChooser.js"

For example: If you are using the JavaScript command shell classic_shell.js, do
the following for message area selection (some knowledge of the JavaScript
language will be helpful):
- Search for the text "case 'J':" (without the double-quotes)
- Comment out the text from the next line until (and not including) the next
  case statement
- Underneath the "case 'J':" line, put the following two lines:
bbs.exec("?DDMsgAreaChooser.js");
break;

In classic_shell.js, the process will be similar for file area selection.

Command-line parameters
-----------------------
The first command-line parameter specifies whether or not to allow the user to
choose a message group (for the message area chooser) or file library (for the
file area chooser).  These parameters are true by default.  If you want to
allow the user to choose a message sub-board only within their current message
group, or file directory only within their current file library, add the word
"false" (without the double-quotes) as a command-line parameter after the .js
script filename.  For example:
For JavaScript:
bbs.exec("?DDMsgAreaChooser.js false");
bbs.exec("?DDFileAreaChooser.js false");
For Baja:
exec "?DDMsgAreaChooser.js false"
exec "?DDFileAreaChooser.js false"

The second command-line parameter specifies whether or not to execute the
script - This is for use when loading the script with the load() function
instead of bbs.exec() in JavaScript.  See the next section for more details.


Advanced installation notes (optional)
--------------------------------------
The area chooser functionality is encapsulated into JavaScript objects so
that you can customize the settings & colors within your script if you like.
To do this, follow these steps:
 1. Include the following line in your JavaScript script (preferably near the
    top):
 load("DDMsgAreaChooser.js", true, false);
 2. Where you want to have area choosing functionality, instantiate the object
    and call its SelectMsgArea() function (for the message area chooser) or its
    SelectFileArea() function (for the file area chooser).  An example:
    var msgAreaChooser = new DDMsgAreaChooser();
	 msgAreaChooser.SelectMsgArea();
You can also list message groups (using the same colors as the chooser) as
follows:
  msgAreaChooser.ListMsgGrps();
You can also use the message lister object to list the sub-boards in the
current message group (using the same colors as the chooser) as follows:
  msgAreaChooser.ListSubBoardsInMsgGroup();
For listing file libraries and file directories inside of a library, you can
do the following:
  var fileAreaChooser = new DDFileAreaChooser();
  fileAreaChooser.ListFileLibs(); // List file libraries
  fileAreaChooser.ListDirsInFileLib(); // List directories inside the current library


If you would like to set up these scripts as doors, the following is an example
setup of the message area chooser (assuming it is placed in sbbs/exec or
sbbs/mods):
+[¦][?]----------------------------------------------------+
¦                   Message Area Chooser                   ¦
¦----------------------------------------------------------¦
¦ ¦Name                       Message Area Chooser         ¦
¦ ¦Internal Code              MSGARCHO                     ¦
¦ ¦Start-up Directory                                      ¦
¦ ¦Command Line               ?DDMsgAreaChooser.js         ¦
¦ ¦Clean-up Command Line                                   ¦
¦ ¦Execution Cost             None                         ¦
¦ ¦Access Requirements                                     ¦
¦ ¦Execution Requirements                                  ¦
¦ ¦Multiple Concurrent Users  Yes                          ¦
¦ ¦Intercept Standard I/O     No                           ¦
¦ ¦Native (32-bit) Executable No                           ¦
¦ ¦Use Shell to Execute       No                           ¦
¦ ¦Modify User Data           No                           ¦
¦ ¦Execute on Event           No                           ¦
¦ ¦Pause After Execution      No                           ¦
¦ ¦BBS Drop File Type         None                         ¦
¦ ¦Place Drop File In         Node Directory               ¦
+----------------------------------------------------------+
To run that from a JavaScript, include this line:
bbs.exec_xtrn("MSGARCHO");
To run that from a Baja script, include this line:
exec_xtrn MSGARCHO


4. Configuration file
=====================
If you want to change the default beavior and colors for one of these scripts,
you can edit its configuration file, which is a plain text file.  The
configuration files have two sections: A behavior section (denoted by
[BEHAVIOR]) and a colors section (denoted by [COLORS]).  For each setting or
color, the syntax is as folows:

setting=value

where "setting" is the behavior setting or color, and "value" is the corresponding
value for the setting/color.  The colors are Synchronet color codes.

Also, comments are allowed in the configuration file.  Comments begin with a
semicolon (;).

Behavior section: Message area chooser
--------------------------------------
Setting                               Description
-------                               -----------
useLightbarInterface                  true/false: Whether or not to use a
                                      lightbar user interface.

showImportDates                       true/false: Whether or not to show the
                                      import dates (rather than message dates)
                                      in the latest date & time column in the
                                      sub-board lists.

areaChooserHdrFilenameBase            The filename to use (without the
                                      extension) for a header to display above
                                      the message area chooser list.  For
                                      example, if areaChgHeader is specified,
                                      then the chooser will look for
                                      areaChgHeader.ans if it exists, and if
                                      not, the chooser will look for
                                      areaChgHeader.asc.  Additionally, you
                                      can have multiple header files for
                                      different terminal widths; fpr example,
                                      areaChgHeader-80.ans for an 80-column
                                      terminal, areaChgHeader-140.ans for a
                                      140-column terminal, etc.

areaChooserHdrMaxLines                The maximum number of lines to use from
                                      the message area chooser header file

showDatesInSubBoardList               Specifies whether or not to show the date
                                      & time of the latest message in the
                                      sub-boards.  Valid values are true and
                                      false.

Colors section: Message area chooser
------------------------------------
Color setting                        Description
-------------                        -----------
areaNum                              The color to use for area numbers

desc                                 The color to use for descriptions

numItems                             The color to use for the item counts

header                               The color to use for list headers

subBoardHeader                       The color to use for the header in the
                                     sub-board list showing "Sub-boards of"
                                     with the group description and page number
                                     (note that the group description will have
                                     he bright attribute applied)

areaMark                             The color to use for the marker character
                                     used to show the area that is currently
                                     selected

latestDate                           The color to use for the latest post date

latestTime                           The color to use for the latest post time

bkgHighlight                         The background highlight color for
                                     lightbar mode

areaNumHighlight                     The color to use for an area number for
                                     a selected item in lightbar mode

descHighlight                        The color to use for a description for
                                     a selected item in lightbar mode

dateHighlight                        The color to use for the date for a
                                     selected item in lightbar mode

timeHighlight                        The color to use for the time for a
                                     selected item in lightbar mode

numItemsHighlight                    The color to use for the number of items
                                     for a selected item in lightbar mode

lightbarHelpLineBkg                  The background color to use for the help
                                     text line displayed at the bottom of the
                                     screen in lightbar mode

lightbarHelpLineGeneral              The color to use for general text in the
                                     help text line displayed at the bottom of
                                     the screen in lightbar mode

lightbarHelpLineHotkey               The color to use for hotkeys in the help
                                     text line displayed at the bottom of the
                                     screen in lightbar mode

lightbarHelpLineParen                The color to use for the ) characters in
                                     the help text line displayed at the bottom
                                     of the screen in lightbar mode

Behavior section: File area chooser
-----------------------------------
Setting                               Description
-------                               -----------
useLightbarInterface                  true/false: Whether or not to use a
                                      lightbar user interface.

areaChooserHdrFilenameBase            The filename to use (without the
                                      extension) for a header to display above
                                      the file area chooser list.  For example,
                                      if areaChgHeader is specified, then the
                                      chooser will look for areaChgHeader.ans
                                      if it exists, and if not, the chooser
                                      will look for areaChgHeader.asc.
                                      Additionally, you can have multiple
                                      header files for different terminal
                                      widths; fpr example, areaChgHeader-80.ans
                                      for an 80-column terminal,
                                      areaChgHeader-140.ans for a 140-column
                                      terminal, etc.

areaChooserHdrMaxLines                The maximum number of lines to use from
                                      the message area chooser header file

Colors section: File area chooser
------------------------------------
Color setting                        Description
-------------                        -----------
areaNum                              The color to use for area numbers

desc                                 The color to use for descriptions

numItems                             The color to use for the item counts

header                               The color to use for list headers

fileAreaHdr                          The color to use for the header in the
                                     directory list showing "Directories of"
                                     with the group description and page number
                                     (note that the group description will have
                                     the bright attribute applied)

areaMark                             The color to use for the marker character
                                     used to show the area that is currently
                                     selected

bkgHighlight                         The background highlight color for
                                     lightbar mode

areaNumHighlight                     The color to use for an area number for
                                     a selected item in lightbar mode

descHighlight                        The color to use for a description for
                                     a selected item in lightbar mode

numItemsHighlight                    The color to use for the number of items
                                     for a selected item in lightbar mode

lightbarHelpLineBkg                  The background color to use for the help
                                     text line displayed at the bottom of the
                                     screen in lightbar mode

lightbarHelpLineGeneral              The color to use for general text in the
                                     help text line displayed at the bottom of
                                     the screen in lightbar mode

lightbarHelpLineHotkey               The color to use for hotkeys in the help
                                     text line displayed at the bottom of the
                                     screen in lightbar mode

lightbarHelpLineParen                The color to use for the ) characters in
                                     the help text line displayed at the bottom
                                     of the screen in lightbar mode


5. DDMsgAreaChooser class: Properties & methods
===============================================
The following are the properties and methods of the DDMsgAreaChooser class, which
is the class used for letting the user choose a message area:
Property name                         Description
-------------                         -----------
showImportDates                       Boolean: Whether or not to show the
                                      import dates (rather than message dates)
                                      in the latest date & time column in the
                                      sub-board lists.

useLightbarInterface                  Boolean: Whether or not to use a
                                      lightbar user interface.

Methods
-------
Method name                           Description
-----------                           -----------
DDMsgAreaChooser()                    Constructor

SelectMsgArea()                       Lets the user choose a message sub-board.
                                      If the useLightbarInterface property is
                                      true and the user's terminal supports
                                      ANSI, it will use the lightbar interface;
                                      otherwise, it will use traditional
                                      interface.

SelectMsgArea_Lightbar()              Lets the user choose a message sub-board,
                                      with a lightbar user interface.

SelectMsgArea_Traditional()           Lets the user choose a message sub-board,
                                      with a traditional user interface.

ListMsgGrps()                         Lists the message groups

ListSubBoardsInMsgGroup(pGrpIndex,    Lists the sub-boards in the user's
                        pMarkIndex,   currently-selected message group.
                        pSortType)    The parameters are all optional.  They
                                      specify the index of the message group,
                                      the index of the sub-board to mark with
                                      the "chosen" character, and a sort type,
                                      which can be "none" (default sorting),
                                      "dateAsc" for date ascending,
                                      "dateDesc" for date descending, or
                                      "description" for description.

6. DDFileAreaChooser class: Properties & methods
===============================================
The following are the properties and methods of the DDMsgAreaChooser class, which
is the class used for letting the user choose a message area:
Property name                         Description
-------------                         -----------
useLightbarInterface                  Boolean: Whether or not to use a
                                      lightbar user interface.

Methods
-------
Method name                           Description
-----------                           -----------
DDFileAreaChooser()                   Constructor

SelectFileArea()                      Lets the user choose a file directory.
                                      If the useLightbarInterface property is
                                      true and the user's terminal supports
                                      ANSI, it will use the lightbar interface;
                                      otherwise, it will use traditional
                                      interface.

SelectFileArea_Lightbar()             Lets the user choose a file directory,
                                      with a lightbar user interface.

SelectFileArea_Traditional()          Lets the user choose a file directory,
                                      with a traditional user interface.

ListFileLibs()                        Lists the file libraries

ListDirsInFileLib(pLibIndex,          Lists the directories in the user's
                  pMarkIndex)         currently-selected file library.
                                      The parameters are optional.  They
                                      specify the index of the file library
                                      and the index of the directory to mark
                                      with the "chosen" character.
