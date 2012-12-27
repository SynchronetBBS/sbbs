                         SlyEdit message editor
                              Version 1.17
                        Release date: 2012-12-27

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
5. Configuration file
6. Ice-style Color Theme Settings
7. DCT-style Color Theme Settings
8. Revision History (change log)


1. Disclaimer
=============
Although I have tested SlyEdit, I cannot guarantee that it is 100% bug free or
will work as expected in all environments in all cases.  That said, I hope you
find SlyEdit useful and enjoy using it.


2. Introduction
===============
SlyEdit is a message editor that I wrote in JavaScript for Synchronet, which
can mimic the look and feel of IceEdit or DCT Edit.  SlyEdit also supports
customization of colors via theme files.

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

Thanks go out to the following people for testing:
- Nick of Lightning BBS (lightningbbs.dyndns.org) for testing early versions
- Nick (AKA Access Denied) of The Pharcyde (pharcyde.org) for testing the
  newer versions and for his input on various features, mainly message quoting

3. Installation & Setup
=======================
These are the steps for installation:
 1. Unzip the archive.  If you're viewing this file, then you've probably
    already done this. :)
 2. There are 2 ways SlyEdit's files can be copied onto your Synchronet system:
    1. Copy the JavaScript files into your sbbs/exec directory and the .cfg files
       into your sbbs/ctrl directory
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
- Navigation: Page up/down, home/end of line, and arrow keys
- Slash commands (at the start of the line):
  /A: Abort
  /S: Save
  /Q: Quote message
- Sysops can import a file (stored on the BBS machine) into the message
- Sysops can export the current message to a file (on the BBS machine)
- Configuration file with behavior and color settings.  See section 4
  (Configuration File) for more information.

The following is a summary of the keyboard shortcuts (from SlyEdit's command
help screen):

Help keys                                     Slash commands (@ line start)
---------                                     -----------------------------
Ctrl-G       : General help                 | /A     : Abort
Ctrl-P       : Command key help             | /S     : Save
Ctrl-R       : Program information          | /Q     : Quote message

Command/edit keys
-----------------
Ctrl-A       : Abort message                | Ctrl-W : Page up
Ctrl-Z       : Save message                 | Ctrl-S : Page down
Ctrl-Q       : Quote message                | Ctrl-N : Find text
Insert/Ctrl-I: Toggle insert/overwrite mode | ESC    : Command menu
Ctrl-O       : Import a file                | Ctrl-X : Export to file
Ctrl-D       : Delete line


5. Configuration file
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
                                  values are true and false, and this feature
                                  is enabled by default.  With this feature
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
                                  replying to a message.  If this setting is
                                  disabled, SlyEdit will simply prefix the
                                  quoted lines with " > ", as was done in
                                  IceEdit, DCT Edit, and other message editors
                                  of the early-mid 1990s.  If this setting is
                                  not in the configuration file, it will be
                                  enabled by default.

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


6. Ice-style Color Theme Settings
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


7. DCT-style Color Theme Settings
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


8. Revision History (change log)
================================
Version  Date         Description
-------  ----         -----------
1.17     2012-12-27   Updated to add author initials before quote lines.  This
                      is now the default behavior, but this can be disabled
                      with the following line in SlyEdit.cfg:
                      useQuoteLineInitials=false
                      When disabled, SlyEdit will simply prefix quote lines
                      with " > ", as was done in IceEdit, DCT Edit, and other
                      message editors of the early-mid 1990s.
                      Also, fixed a bug related to refreshing the text on the
                      screen when a drop-down menu disappears.  This bug fix is
                      only relevant for DCT mode.
1.16     2012-12-21   Updated to look for the .cfg files first in
                      the sbbs/ctrl directory, and if they're not
                      found there, assume they're in the same
                      directory as the .js files.
1.15     2012-04-22   Improved quoting with the ability to re-wrap quote lines
                      so that they are complete but still look good when
                      quoted.  SlyEdit recognizes quote lines beginning with >
                      or 1 or 2 intials followed by a >.  The configuration
                      option "splitLongQuoteLines" was replaced by
                      "reWrapQuoteLines", and it is enabled by default.
                      Also, added the following configuration options and capabilities:
                      add3rdPartyStartupScript:
                                  Add a 3rd-party JavaScript script to execute
                                  (via loading) upon startup of SlyEdit.  The
                                  parameter must specify the full path & filename
                                  of the JavaScript script.  For example (using
                                  the excellent Desafortunadamente add-on by Art
                                  of Fat Cats BBS):
                                  add3rdPartyStartupScript=D:/BBS/sbbs/xtrn/desafortunadamente/desafortunadamente.js
                      addJSOnStart:
                                  Add a JavaScript command to run on startup.  Any
                                  commands added this way will be executed after
                                  3rd-party scripts are loaded.
                                  Example (using the excellent Desafortunadamente
                                  add-on by Art of Fat Cats BBS):
                                  addJSOnStart=fortune_load();
                      add3rdPartyExitScript:
                                  Add a 3rd-party JavaScript script to execute
                                  (via loading) upon exit of SlyEdit.  The
                                  parameter must specify the full path & filename
                                  of the JavaScript script.
                      addJSOnExit:
                                  Add a JavaScript command to run on exit.
                                  Example (don't actually do this):
                                  addJSOnStart=console.print("Hello\n\1p");
1.145    2011-02-07   The time on the screen will now be updated.  The
                      time is checked every 5 keystrokes and will be
                      updated on the screen when it changes.
1.144    2010-11-21   Minor bug fix: In DCT mode, if the top or bottom border
                      of one of the menus or the abort confirmation box is
                      on the first or last line of text on the screen and the
                      text line ends before the box border ends, the box border
                      is now fully erased when it disappears.
1.143    2010-06-19   Minor bug fix: When typing an entire line of text that
                      doesn't have any spaces, the last character was being
                      discarded when wrapping to the next line.
1.142    2010-02-04   Minor bug fix: When reading quote lines and the
                      splitLongQuoteLines is disabled, it will no longer
                      (incorrectly) insert "null" as the last quote line
                      (as is done when the splitLongQuoteLines option is
                      enabled).
1.141    2010-01-23   Bug fix: The screen wouldn't update when pressing the Delete
                      key on a blank line, which would remove the line.
1.14     2010-01-19   Bug fix: The screen wouldn't update when pressing the Delete
                      key at the end of a line (specifically, with a blank line
                      below it followed by a non-blank line).
                      Also, updated to allow combining quote lines by pressing
                      the Delete key at the end of a quote line.
1.131    2010-01-10   Minor update - The option for splitting long quote
                      lines, which was enabled by default in the previous
                      version, is now disabled by default in this verison.
                      It seems that there may be more sysops that don't
                      like it than those who do like it.
                      The code in the .js files in this version is also
                      a little more refactored.
1.13     2010-01-04   Includes the ability to split up quote lines that
                      are too long, rather than truncating them.  This
                      is an option that can be toggled and is enabled by
                      default.
                      Includes several bug fixes related to message editing
                      (i.e., such as word wrapping for the last word on a
                      line) and other behind-the-scenes bug fixes.
                      Efficiency of screen updates has been improved somewhat
                      in this release.  This is more noticeable on slower
                      connections.
1.12     2009-12-14   Behavior change: Now never removes any spaces from
                      the beginning of a line when the user presses enter
                      at the beginning of a line.
1.11     2009-12-10   Added the ability to customize the quote line color
                      in the color theme files (QuoteLineColor).
                      Fixed a bug where the text color temporarily went
                      back to default (not using the customized text
                      color) when moving to a new line when typing a
                      message.
                      Updated to (hopefully) fixed a bug that could
                      cause the script to abort when adding a line to
                      the message in rare circumstances.
1.10     2009-12-03   Added support for customizable color themes.
                      Fixed a couple of text editing bugs/annoyances.
1.08     2009-11-10   Changed the way the message is saved back to the
                      way SlyEdit was saving in 1.06 and earlier, as
                      this simplifies the code.  The "Expand Line Feeds
                      to CRLF" option needs to be enabled in SCFG for
                      messages to be saved properly in all platforms.
                      Added configuration options to enable/disable the
                      user input timeout, and to specify the input timeout
                      time (in MS).
                      The sysop is now always exempt from the input timeout.
                      Also, started to work on improving the efficiency
                      of refreshing the message area.
1.07     2009-10-23   Bug fix: Changed how the end-of-line newline
                      characters are written to the message file so
                      that the message text lines are saved correctly
                      in Linux.  The bug in previous versions was causing
                      messages going across certain networks to lose their
                      end-of-line characters so that text lines weren't
                      terminated where they were supposed to be.  Thanks
                      goes to Tracker1 (sysop of The Roughnecks BBS) for
                      the tip of how to fix this.
                      New feature: Configuration file with settings
                      for whether or not to display the ending info
                      screen, as well as quote window colors for both
                      Ice and DCT modes.
1.06     2009-09-12   Bug fix: Updated the way it checks for printable
                      characters.  This should fix the problem I've seen
                      with some BBSs where it wouldn't allow typing an
                      upper-case letter.
1.05     2009-08-30   Bug fix: When editing an existing message, the cursor
                      would be at the bottom of the edit area, and it would
                      appear stuck there, unable to move up.  This has been
                      fixed - Now the cursor is always initially placed at
                      the start of the edit area.
                      Bug fix: When saving a message, blank lines are now
                      removed from the end of a message before saving.
1.04     2009-08-27   Bug fix: When wrapping a text line, it would place
                      individual words on lines by themselves, due to a
                      change in 1.03.  This has been fixed.
1.03     2009-08-27   Bug fix: With a small message (less than one screenful),
                      Ctrl-S (page down) now doesn't crash SlyEdit.
                      Bug fix: When typing and the end of the line and it has to
                      wrap the word to the next line, it now always inserts a new
                      line below the current line, pushing the rest of the message
                      down.
1.02     2009-08-26   Bug fix: Now prevents invalid text lines from quotes.txt or
                      the message file from being used.
1.01     2009-08-23   Bug fix: Blank edit lines would be removed
                      when they weren't supposed to if the user
                      used the /S or /A commands on a line that
                      wasn't the last line.
1.00     2009-08-22   First public release
0.99     2009-08-13-  Test release.  Finishing up features, testing,
         2009-08-20   and fixing bugs before general release.