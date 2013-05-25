                         SlyEdit message editor
                              Version 1.27
                        Release date: 2013-05-24

                                  by

                             Eric Oulashin
                     Sysop of Digital Distortion BBS
                 BBS internet address: digdist.bbsindex.com
                     Email: eric.oulashin@gmail.com



This file describes SlyEdit, a message editor for Synchronet.
Note: For sysops who already have a previous version of SlyEdit
installed and are upgrading to this version, please see the file
Upgrading.txt.

Contents
========
1. Disclaimer
2. Introduction
3. Installation & Setup
4. Features
5. Digital Distortion Message Lister note
6. Configuration file
7. Ice-style Color Theme Settings
8. DCT-style Color Theme Settings


1. Disclaimer
=============
Although I have tested SlyEdit, I cannot guarantee that it is 100% bug free or
will work as expected in all environments in all cases.  That said, I hope you
find SlyEdit useful and enjoy using it.


2. Introduction
===============
SlyEdit is a message editor that I wrote in JavaScript for Synchronet, which
can mimic the look and feel of IceEdit or DCT Edit.  SlyEdit also supports
customization of colors via theme files, cross-posting into other message
areas, text search, and file import & export (works with files on the
BBS machine, and is enabled only for the sysop).

The motivation for creating this was that IceEdit and DCT Edit were always
my two favorite BBS message editors, but in a world where 32-bit (and 64-bit)
Windows and *nix platforms are common, 16-bit DOS emulation is required to run
IceEdit and DCT Edit, which is noticeably slow and sometimes unreliable.
Since SlyEdit is written in JavaScript, it is faster than a DOS-based message
editor and can run in any environment in which Synchronet runs, without any
modification or being re-compiled.

SlyEdit will recognize the user's terminal size and set up the screen
accordingly.  The width of the edit area will always be 80 characters and
support up to 79 characters; however, an increased terminal size will provide
more room for message information to be displayed.  Also, a terminal height
greater than the standard 24 or 25 characters will provide a taller edit area,
allowing more of the message to be seen on the screen at a time.

Also, SlyEdit has no moving parts that can wear out over time.

Thanks go out to the following people for testing:
- Nick of Lightning BBS (lightningbbs.dyndns.org) for testing early versions
- Nick (AKA Access Denied) of The Pharcyde (pharcyde.org) for testing the
  newer versions and for his input on various features, mainly message quoting
- echicken (sysop of Electronic Chicken) for his solution to look up the
  original author of a message after a user changes the "To" name when
  replying to a message

3. Installation & Setup
=======================
These are the steps for installation:
 1. Extract the archive.  If you're viewing this file, then you've probably
    already done this. :)
 2. There are 2 ways SlyEdit's files can be copied onto your Synchronet system:
    1. Copy the JavaScript files into your sbbs/exec directory and the .cfg files
       into your sbbs/ctrl directory.  If you plan to customize your SlyEdit.cfg
       or color theme files, it's recommended to also put the .cfg files in your
       sbbs/mods directory so that your configuration won't be accidentally
       overwritten when updating the files from the Synchronet CVS repository.
       SlyEdit will first check the sbbs/mods directory for the configuration
       files, then sbbs/ctrl, and then SlyEdit's own directory for the
       configuration files.  In other words, the SlyEdit configuration files
       in sbbs/mods take first precedence, sbbs/ctrl 2nd precedence, and
       SlyEdit's own directory takes last precedence for the configuration
       files.
    2. Copy all files together into their own directory of your choice
 3. Set up SlyEdit on your BBS with Synchronet's configration program (SCFG).

SlyEdit can be set to mimic IceEdit or DCT Edit via a command-line parameter.
The values for this parameter are as follows:
  ICE: Mimic the IceEdit look & feel
  DCT: Mimic the DCT Edit look & feel
  RANDOM: Randomly select either the IceEdit or DCT Edit look & feel

To add SlyEdit to Synchronet's list of external editors, run Synchronet's
configuration program (SCFG) and select "External Programs", and then
"External Editors".  The following describes setting up SlyEdit using the
ICE parameter for IceEdit emulation:
 1. Scroll down to the empty slot in the editor list and press Enter to
    select it.
 2. For the external editor name, enter "SlyEdit (Ice style)" (without the quotes)
    (or similar, depending on your personal preference)
 3. For the internal code, use SLYEDICE (or whatever you want, depending on your
    personal preference)
 4. Press Enter to select and edit the new entry.  Asuming that the .js files
    are in the sbbs/exect directory, the settings should be as follows:
      Command line: ?SlyEdit.js %f ICE
      Access requirement string: ANSI
      Intercept standard I/O: No
      Native (32-bit) Executable: No
      Use Shell to Execute: No
      Quoted Text: All
      Editor Information Files: QuickBBS MSGINF/MSGTMP
      Expand Line Feeds to CRLF: Yes
      Strip FidoNet Kludge Lines: No
      BBS Drop File Type: None
    After you've added SlyEdit, your Synchronet configuration window should look
    like this:
    +[¦][?]--------------------------------------------------------------+
    ¦                      SlyEdit (Ice style) Editor                    ¦
    ¦--------------------------------------------------------------------¦
    ¦ ¦Name                            SlyEdit  (Ice style)              ¦
    ¦ ¦Internal Code                   SLYEDICE                          ¦
    ¦ ¦Remote Command Line             ?SlyEdit.js %f ICE                ¦
    ¦ ¦Access Requirements             ANSI                              ¦
    ¦ ¦Intercept Standard I/O          No                                ¦
    ¦ ¦Native (32-bit) Executable      No                                ¦
    ¦ ¦Use Shell to Execute            No                                ¦
    ¦ ¦Quoted Text                     All                               ¦
    ¦ ¦Editor Information Files        QuickBBS MSGINF/MSGTMP            ¦
    ¦ ¦Expand Line Feeds to CRLF       Yes                               ¦
    ¦ ¦Strip FidoNet Kludge Lines      No                                ¦
    ¦ ¦BBS Drop File Type              None                              ¦
    +--------------------------------------------------------------------+

    For DCT Edit mode, use DCT in place of ICE on the command line.  For
    random mode, use RANDOM in place of ICE.

    Note that if you placed the files in a different directory, then the
    command line should include the full path to SlyEdit.js.  For example,
    if SlyEdit was placed in sbbs/xtrn/SlyEdit, then the command line would
    be /BBS/sbbs/xtrn/DigDist/SlyEdit/SlyEdit.js %f ICE


4. Features
===========
As mentioned earlier, SlyEdit is can mimic the look & feel of IceEdit or
DCT Edit.  It also has the following features:
- Text search: Allows the user to search for text in the message.  If
  the text is found, the message area will scroll to it, and it will be
  highlighted.  Repeated searches for the same text will look for the
  next occurrance of that text.
- Message quoting: When replying to a message, users can select lines from the
  message to quote.  By default, SlyEdit puts the initials of the original
  author in front of the quote lines to indicate who originally wrote those
  parts of the message.  Optionally, sysops may disable the use of initials
  in quote lines, in which case SlyEdit simply prefixes quote lines with " > "
  as was done in IceEdit, DCT Edit, and other editors of the early-mid 1990s.
- Cross-posting into other message sub-boards: When posting a message to a
  message sub-board, SlyEdit allows the user to select other message sub-boards
  to post the message into.  SlyEdit will post the message to the user's
  current message sub-board by default.  Using the cross-post feature, the
  user can select any number of other message sub-boards to cross-post his/her
  message into.  SlyEdit respects the ARS security setting for each sub-board -
  If a user is not allowed to post in a message sub-board, SlyEdit will not
  allow the user to select that sub-board for cross-posting.  The user can also
  un-select their current message sub-board.  When the user un-selects their
  current message sub-board and chooses other message sub-boards to post
  his/her message into, Synchronet will say that the message was aborted
  because the message file wasn't saved as it normally is for the current
  message sub-board.  However, SlyEdit will output all of the message
  sub-boards that the message was posted into.  If the user un-selected their
  current message sub-board, SlyEdit will also output a note explaining that
  the BBS will say the message was aborted, even though it was posted into
  other sub-boards and that is normal.
- Navigation: Page up/down, home/end of line, and arrow keys
- Slash commands (at the start of the line):
  /A: Abort
  /S: Save
  /Q: Quote message
  /C: Cross-post message sub-board selection
- Sysops can import a file (stored on the BBS machine) into the message
- Sysops can export the current message to a file (on the BBS machine)
- Configuration file with behavior and color settings.  See section 4
  (Configuration File) for more information.

The following is a summary of the keyboard shortcuts (from SlyEdit's command
help screen):

Help keys                                     Slash commands (on blank line)
---------                                     ------------------------------
Ctrl-G       : General help                 ¦ /A     : Abort
Ctrl-P       : Command key help             ¦ /S     : Save
Ctrl-R       : Program information          ¦ /Q     : Quote message
                                            ¦ /C     : Cross-post selection

Command/edit keys
-----------------
Ctrl-A       : Abort message                ¦ Ctrl-W : Page up
Ctrl-Z       : Save message                 ¦ Ctrl-S : Page down
Ctrl-Q       : Quote message                ¦ Ctrl-N : Find text
Insert/Ctrl-I: Toggle insert/overwrite mode ¦ Ctrl-D : Delete line
ESC          : Command menu                 ¦ Ctrl-C : Cross-post selection
Ctrl-O       : Import a file                ¦ Ctrl-X : Export to file


5. Digital Distortion Message Lister note
-----------------------------------------
If you use Digital Distortion's Message Lister, you must update it to version
1.36 or newer in order to properly work with this version of SlyEdit when
replying to messages.  See the file "SlyEdit_DD_Message_Lister_notes.txt" for a more
detailed explanation.

6. Configuration file
=====================
The configuration file, SlyEdit.cfg, is split up into 3 sections -
Behavior, Ice colors, and DCT colors.  These sections are designated
by [BEHAVIOR], [ICE_COLORS], and [DCT_COLORS], respectively.  These
settings are described below:

Behavior settings
-----------------
Setting                           Description
-------                           -----------
displayEndInfoScreen              Whether or not to display the info
                                  screen when SlyEdit exits.  Valid values
                                  are true and false.  If this option is
                                  not specified, this feature will be
                                  enabled by default.

userInputTimeout                  Whether or not to use an input timeout
                                  for users.  Valid values are true and
                                  false.  Note: The input timeout is not
                                  used for sysops.  If this option is not
                                  specified, this feature will be enabled
                                  by default.

inputTimeoutMS                    The amount of time (in milliseconds) to
                                  use for the input timeout.  If this option
                                  is not specified, this option will default
                                  to 300000.

reWrapQuoteLines                  Whether or not to re-wrap quote lines. Valid
                                  values are true and false.  This feature is
                                  enabled by default.  With this feature
                                  enabled, SlyEdit will re-wrap quote lines
                                  to still be complete and readable after the
                                  quote prefix character is added to the front
                                  of the quote lines.  SlyEdit is able to
                                  recognize quote lines beginning with >
                                  or 2 letters and a > (such as EO>).  If this
                                  feature is disabled, quote lines will simply
                                  be trimmed to make room for the quote prefix
                                  character to be added to the front.

useQuoteLineInitials              Whether or not to prefix quoted message lines
                                  with the previous author's initials when
                                  replying to a message.  Valued values are
                                  true and false.  If this setting is disabled,
                                  SlyEdit will simply prefix the quoted lines
                                  with " > ", as was done in IceEdit, DCT Edit,
                                  and other message editors of the early-mid
                                  1990s.  This setting is enabled by default.

indentQuoteLinesWithInitials      When prefixing quoted messages lines with the
                                  previous author's initials, this setting
                                  specifies whether or not to indent quoted
                                  lines with a space.  Valued values are true
                                  and false.  This setting is disabled by
                                  default.

add3rdPartyStartupScript          Add a 3rd-party JavaScript script to execute
                                  (via loading) upon startup of SlyEdit.  The
                                  parameter must specify the full path & filename
                                  of the JavaScript script.  For example (using
                                  the excellent Desafortunadamente add-on by Art
                                  of Fat Cats BBS):
                                  add3rdPartyStartupScript=D:/BBS/sbbs/xtrn/desafortunadamente/desafortunadamente.js

addJSOnStart                      Add a JavaScript command to run on startup.  Any
                                  commands added this way will be executed after
                                  3rd-party scripts are loaded.
                                  Example (using the excellent Desafortunadamente
                                  add-on by Art of Fat Cats BBS):
                                  addJSOnStart=fortune_load();

add3rdPartyExitScript             Add a 3rd-party JavaScript script to execute
                                  (via loading) upon exit of SlyEdit.  The
                                  parameter must specify the full path & filename
                                  of the JavaScript script.

addJSOnExit                       Add a JavaScript command to run on exit.
                                  Example (don't actually do this):
                                  addJSOnStart=console.print("Hello\r\n\1p");

allowCrossPosting                 Whether or not to allow cross-posting
                                  messages into different/multiple message
                                  sub-boards.  Valid values are true and false.

Ice colors
----------
Setting                           Description
-------                           -----------
ThemeFilename                     The name of the color theme file to use.
                                  Note: Ice-style theme settings are described
                                  in Section 5: Ice-style Color Theme Settings.
                                  If no theme file is specified, then default
                                  colors will be used.

DCT colors
----------
Setting                           Description
-------                           -----------
ThemeFilename                     The name of the color theme file to use.
                                  Note: DCT-style theme settings are described
                                  in Section 6: DCT-style Color Theme Settings.
                                  If no theme file is specified, then default
                                  colors will be used.

The color theme files are plain text files that can be edited with a
text editor.


7. Ice-style Color Theme Settings
=================================
The following options are valid for Ice-style theme files:
----------------------------------------------------------
TextEditColor                     The color for the message text

QuoteLineColor                    The color for quoted lines in the message

BorderColor1                      The first color to use for borders
                                  (for alternating border colors)

BorderColor2                      The other color to use for borders
                                  (for alternating border colors)

KeyInfoLabelColor                 The color to use for key information
                                  labels (displayed on the bottom border)

TopInfoBkgColor                   The color to use for the background in
                                  the informational area at the top

TopLabelColor                     The color to use for informational labels
                                  in the informational area at the top

TopLabelColonColor                The color to use for the colons (:) in the
                                  informational area at the top

TopToColor                        The color to use for the "To" name in the
                                  informational area at the top

TopFromColor                      The color to use for the "From" name in the
                                  informational area at the top

TopSubjectColor                   The color to use for the subject in the
                                  informational area at the top

TopTimeColor                      The color to use for the time left in the
                                  informational area at the top

TopTimeLeftColor                  The color to use for the time left in the
                                  informational area at the top

EditMode                          The color to use for the edit mode text

QuoteWinText                      The color for non-highlighted text in
                                  the quote window

QuoteLineHighlightColor           The color for highlighted text in the
                                  quote window

QuoteWinBorderTextColor           The color for the quote window borders

; Colors for the multi-choice options
SelectedOptionBorderColor         The color to use for the borders around
                                  text for selected multi-choice options

SelectedOptionTextColor           The color to use for the text for selected
                                  multi-choice options

UnselectedOptionBorderColor       The color to use for the borders around
                                  text for unselected multi-choice options

UnselectedOptionTextColor         The color to use for the text for unselected
                                  multi-choice options

crossPostBorder                   The color to use for the border of the cross-
                                  post area selection box

crossPostBorderText               The color to use for the top border text of
                                  the cross-post area selection box

crossPostMsgAreaNum               The color to use for the message group/sub-
                                  board numbers in the cross-post area
                                  selection box

crossPostMsgAreaNumHighlight      The color to use for the highlighted
                                  (lightbar) message group/sub-board numbers
                                  in the cross-post area selection box

crossPostMsgAreaDesc              The color to use for the message group/
                                  sub-board descriptions in the cross-post
                                  area selection box

crossPostMsgAreaDescHighlight     The color to use for the highlighted
                                  (lightbar) message group/sub-board
                                  descriptions in the cross-post area selection
                                  box

crossPostChk                      The color to use for the checkmarks for the
                                  enabled message sub-boards in the cross-post
                                  area selection box

crossPostChkHighlight             The color to use for the highlighted
                                  (lightbar) checkmarks for the enabled
                                  message sub-boards in the cross-post area
                                  selection box

crossPostMsgGrpMark               The color to use for the current message
                                  group marker character in the cross-post area
                                  selection box

crossPostMsgGrpMarkHighlight      The color to use for the highlighted
                                  (lightbar) current message group marker
                                  character in the cross-post area selection
                                  box

8. DCT-style Color Theme Settings
=================================
The following options are valid for DCT-style theme files:
----------------------------------------------------------
TextEditColor                     The color for the message text

QuoteLineColor                    The color for quoted lines in the message

TopBorderColor1                   The first color to use for the
                                  top borders (for alternating border
                                  colors)

TopBorderColor2                   The other color to use for the
                                  top borders (for alternating border
                                  colors)

EditAreaBorderColor1              The first color to use for the
                                  edit area borders (for alternating border
                                  colors)

EditAreaBorderColor2              The other color to use for the
                                  edit area borders (for alternating border
                                  colors)

EditModeBrackets                  The color to use for the square brackets
                                  around the edit mode text that appears
                                  in the bottom border (the [ and ] around
                                  the "INS"/"OVR")

EditMode                          The color to use for the edit mode text

TopLabelColor                     The color to use for the informational labels
                                  in the informational area at the top

TopLabelColonColor                The color to use for the colons (:) in the
                                  informational area at the top

TopFromColor                      The color to use for the "From" name in the
                                  informational area at the top

TopFromFillColor                  The color to use for the filler dots in the
                                  "From" name in the informational area at the top

TopToColor                        The color to use for the "To" name in the
                                  informational area at the top

TopToFillColor                    The color to use for the filler dots in the
                                  "To" name in the informational area at the top

TopSubjColor                      The color to use for the subject in the informational
                                  area at the top

TopSubjFillColor                  The color to use for the filler dots in the subject
                                  in the informational area at the top

TopAreaColor                      The color to use for the "Area" text in the
                                  informational area at the top

TopAreaFillColor                  The color to use for the filler dots in the "Area"
                                  field in the informational area at the top

TopTimeColor                      The color to use for the "Time" text in the
                                  informational area at the top

TopTimeFillColor                  The color to use for the filler dots in the "Time"
                                  field in the informational area at the top

TopTimeLeftColor                  The color to use for the "Time left" text in the
                                  informational area at the top

TopTimeLeftFillColor              The color to use for the filler dots in the "Time left"
                                  field in the informational area at the top

TopInfoBracketColor               The color to use for the square brackets in the
                                  informational area at the top

QuoteWinText                      The color for non-highlighted text in
                                  the quote window

QuoteLineHighlightColor           The color for highlighted text in the
                                  quote window

QuoteWinBorderTextColor           The color to use for the text in the quote window
                                  borders

QuoteWinBorderColor               The color to use for the quote window borders

BottomHelpBrackets                The color to use for the brackets displayed in
                                  the line of help text at the bottom

BottomHelpKeys                    The color to use for the key names written in
                                  the line of help text at the botom

BottomHelpFill                    The color to use for the filler dots in the line of
                                  help text at the bottom

BottomHelpKeyDesc                 The color to use for the key descriptions in the
                                  line of help text at the bottom

TextBoxBorder                     The color to use for text box borders (i.e., the
                                  abort confirmation prompt)

TextBoxBorderText                 The color to use for text in the borders of text
                                  boxes (i.e., the abort confirmation prompt)

TextBoxInnerText                  The color to use for text inside text boxes

YesNoBoxBrackets                  The color to use for the square brackets used for
                                  yes/no confirmation prompt boxes

YesNoBoxYesNoText                 The color to use for the actual "Yes"/"No" text in
                                  yes/no confirmation prompt boxes

SelectedMenuLabelBorders          The color to use for the border characters for the
                                  labels of currently active drop-down menus

SelectedMenuLabelText             The color to use for the text for the labels of
                                  currently active drop-down menus

UnselectedMenuLabelText           The color to use for the text for the labels of
                                  inactive drop-down menus

MenuBorders                       The color to use for the drop-down menu borders

MenuSelectedItems                 The color to use for selected items on the drop-down
                                  menus

MenuUnselectedItems               The color to use for unselected items on the
                                  drop-down menus

MenuHotkeys                       The color to use for the hotkey characters in the
                                  menu items on the drop-down menus

crossPostBorder                   The color to use for the border of the cross-
                                  post area selection box

crossPostBorderText               The color to use for the top border text of
                                  the cross-post area selection box

crossPostMsgAreaNum               The color to use for the message group/sub-
                                  board numbers in the cross-post area
                                  selection box

crossPostMsgAreaNumHighlight      The color to use for the highlighted
                                  (lightbar) message group/sub-board numbers
                                  in the cross-post area selection box

crossPostMsgAreaDesc              The color to use for the message group/
                                  sub-board descriptions in the cross-post
                                  area selection box

crossPostMsgAreaDescHighlight     The color to use for the highlighted
                                  (lightbar) message group/sub-board
                                  descriptions in the cross-post area selection
                                  box

crossPostChk                      The color to use for the checkmarks for the
                                  enabled message sub-boards in the cross-post
                                  area selection box

crossPostChkHighlight             The color to use for the highlighted
                                  (lightbar) checkmarks for the enabled
                                  message sub-boards in the cross-post area
                                  selection box

crossPostMsgGrpMark               The color to use for the current message
                                  group marker character in the cross-post area
                                  selection box

crossPostMsgGrpMarkHighlight      The color to use for the highlighted
                                  (lightbar) current message group marker
                                  character in the cross-post area selection
                                  box