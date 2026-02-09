                         SlyEdit message editor
                              Version 1.92g
                        Release date: 2026-02-08

                                  by

                             Eric Oulashin
                     Sysop of Digital Distortion BBS
               BBS internet address: digitaldistortionbbs.com
			         Alternate: digdist.synchro.net
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
 6. Strings configuration file
 7. Ice-style Color Theme Settings
 8. DCT-style Color Theme Settings
 9. Common colors (appearing in both Ice and DCT color theme files)
10. Text replacements (AKA Macros)
11. User settings
12. Taglines
13. Spell check and dictionaries
14. Version history


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

SlyEdit allows the user to select/change the color of their text using the
CTRL-K key, if the allowColorSelection is set to true in SlyEdit.cfg.  Also,
you can have SlyEdit convert colors to ANSI when posting messages, for
compatibility with other BBS software.  To do so, set the saveColorsAsANSI
setting to true in SlyEdit.cfg.

As of version 1.88b (2024-02-11), SlyEdit supports inputting UTF-8 text.

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
    are in the sbbs/exect directory, the settings should be as follows (the
    wording and options shown may vary, depending on the version of Synchronet
    you have installed):
      Command line: ?SlyEdit.js %f ICE
      Access requirement string: ANSI
      Intercept standard I/O: No
      Native (32-bit) Executable: No
      Use Shell to Execute: No
      Record Terminal Width: Yes
      Word-wrap Quoted Text: Your decision - Wrap to specific width, 80, or no
      Automatically Quoted Text: All
      Editor Information Files: QuickBBS MSGINF/MSGTMP
      Expand Line Feeds to CRLF: Yes
      Strip FidoNet Kludge Lines: No
      BBS Drop File Type: None
    After you've added SlyEdit, your Synchronet configuration window should look
    like this:
    +[�][?]--------------------------------------------------------------+
    �                      SlyEdit (Ice style) Editor                    �
    �--------------------------------------------------------------------�
    � �Name                            SlyEdit  (Ice style)              �
    � �Internal Code                   SLYEDICE                          �
    � �Remote Command Line             ?SlyEdit.js %f ICE                �
    � �Access Requirements             ANSI                              �
    � �Intercept Standard I/O          No                                �
    � �Native (32-bit) Executable      No                                �
    � �Use Shell to Execute            No                                �
    � �Record Terminal Width           Yes                               �
    � �Word-wrap Quoted Text           Yes, for terminal width           �
    � �Automatically Quoted Text       All                               �
    � �Editor Information Files        QuickBBS MSGINF/MSGTMP            �
    � �Expand Line Feeds to CRLF       Yes                               �
    � �Strip FidoNet Kludge Lines      No                                �
    � �BBS Drop File Type              None                              �
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
- Text search: Allows the user to search for text in the message.  If
  the text is found, the message area will scroll to it, and it will be
  highlighted.  Repeated searches for the same text will look for the
  next occurrance of that text.
- Allows changing the message subject
  Note: If using a Synchronet 3.17c development build (pre official release),
  you will need a build from July 21, 2019 or newer.
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
- Spell check

The following is a summary of the keyboard shortcuts, from SlyEdit's command
help screen (note that file import, with Ctrl-O & Ctrl-X, are only available to
the sysop or users with sysop access, and file import/export takes place on the
BBS machine):

Help keys                                     Slash commands (on blank line)
---------                                     ------------------------------
Ctrl-G       : Input graphic character      � /A      : Abort
Ctrl-L       : Command key list (this list) � /S      : Save
Ctrl-T       : List text replacements       � /T      : List text replacements
                                            � /Q      : Quote message
                                            � /M      : Add a meme
                                            � /U      : Your user settings
                                            � /C      : Cross-post selection
                                            � /UPLOAD : Upload a message

Command/edit keys
-----------------
Ctrl-A       : Abort message                � PageUp  : Page up
Ctrl-Z       : Save message                 � PageDown: Page down
Ctrl-Q       : Quote message                � Ctrl-W  : Word/text search
Insert/Ctrl-I: Toggle insert/overwrite mode � Ctrl-D  : Delete line
Ctrl-S       : Change subject               � ESC     : Command menu
Ctrl-U       : Your user settings           � Ctrl-C  : Cross-post selection
Ctrl-K       : Change text color            � Ctrl-R  : Spell checker
Ctrl-O       : Import a file                � Ctrl-X  : Export to file




5. Configuration file
=====================
The configuration file included in the Synchronet repository is
SlyEdit.example.cfg in sbbs/ctrl.  You can copy it to SlyEdit.cfg to make your
own customizations.

The configuration file is split up into 3 sections - Behavior, Ice colors, and
DCT colors.  These sections are designated by [BEHAVIOR], [ICE_COLORS], and
[DCT_COLORS], respectively.

To help with configuration, there is a menu-driven configuration script in
sbbs/exec called slyedcfg.js. To run it, open a command prompt in the sbbs/exec
directory and run it with this command:
jsexec slyedcfg

Alternately, you can specify the .js filename extension if you want to:
jsexec slyedcfg.js

The configuration script will copy your SlyEdit configuration file to
SlyEdit.cfg in your sbbs/mods directory. SlyEdit will pick up its configuration
from sbbs/mods if the configuration file is there.

The configuration settings are described below:

Behavior settings
-----------------
Setting                           Description
-------                           -----------
displayEndInfoScreen              Whether or not to display the info
                                  screen when SlyEdit exits.  Valid values
                                  are true and false.  If this option is
                                  not specified, this feature will be
                                  enabled by default.

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

allowColorSelection               Whether or not to let the user change the
                                  color/attributes of the text they're typing.
                                  Defaults to true.  When this is enabled, the
                                  user can change their text color with the
                                  CTRL-K hotkey.

saveColorsAsANSI                  Whether or not to save message color/attribute
                                  codes as ANSI (if not, they will be saved as
                                  Synchronet attribute codes). Saving colors as
                                  ANSI in posted messages provides a
                                  compatibility with other BBS software and
                                  message readers that might not understand
                                  Synchronet attribute codes (but most do
                                  understand ANSI).

allowCrossPosting                 Whether or not to allow cross-posting
                                  messages into different/multiple message
                                  sub-boards.  Valid values are true and false.

enableTextReplacements            Toggles the use of text replacements (AKA
                                  macros) as the user types a message.  Valid
                                  values are true, false, and regex.  If regex
                                  is used, text replacements will be enabled
                                  and used as regular expressions as
                                  implemented by JavaScript.  For more
                                  information, see section 10 (Text
                                  replacements (AKA Macros)).

tagLineFilename                   Specifies the name of the file that stores
                                  taglines, which users can optionally choose
                                  from to be appended to their message upon
                                  saving the message.

taglinePrefix                     Some text to be added to the front of
                                  a tagline that is selected from the user.
                                  The intention of this setting is to
                                  differentiate taglines from the rest of the
                                  message.  This defaults to the string "... "
                                  (3 dots and a space).  If desired, this can
                                  be set to a blank string (nothing after the
                                  =) - for instance, if you only want to put
                                  double quotes around text lines, you could
                                  set this to blank and enable the
                                  quoteTagLines setting.

quoteTaglines                     Whether or not to put selected taglines in
                                  double quotes.  This defaults to false.  If
                                  you like taglines to have double quotes and
                                  no prefix, you can set this to true and set
                                  taglinePrefix to a blank string (nothing
                                  after the =).

shuffleTaglines                   Whether or not to shuffle (randomize) the
                                  list of taglines displayed to the user for
                                  selection.

allowUserSettings                 Whether or not to allow users to configure
                                  their own user settings.  This defaults to
                                  true.

allowEditQuoteLines               Whether or not to allow the user to edit
                                  quote lines.  Valid values are true and
                                  false.  If this option is not specified, the
                                  default value is true.

allowSpellCheck                   Whether or not to allow spell check.  Valid
                                  values are true and false.  Defaults to true.
                                  You might want to disable spell check if,
                                  for instance, the spell check feature causes
                                  an "out of memory" error on your system.

ctrlQQuote                        Whether or not to use Ctrl-Q as the hotkey for
                                  quoting. If not, Ctrl-Y will be used. Valid
                                  values are true and false. Defaults to true.
                                  This is a default value for a user setting;
                                  users are allowed to toggle this according
                                  to their preference (also dependent on what
                                  terminal they're using, since Ctrl-Q can be
                                  used to toggle XON/XOFF flow control or
                                  disconnect.

useQuoteLineInitials              Whether or not to prefix quoted message lines
                                  with the previous author's initials when
                                  replying to a message.  Valued values are
                                  true and false.  If this setting is disabled,
                                  SlyEdit will simply prefix the quoted lines
                                  with " > ", as was done in IceEdit, DCT Edit,
                                  and other message editors of the early-mid
                                  1990s.  This setting is enabled by default.
                                  This setting serves as the default for the
                                  user setting, which users can configure in
                                  their own settings.

indentQuoteLinesWithInitials      When prefixing quoted messages lines with the
                                  previous author's initials, this setting
                                  specifies whether or not to indent quoted
                                  lines with a space.  Valued values are true
                                  and false.  This setting is disabled by
                                  default.  This setting serves as the default
                                  for the user setting, which users can
                                  configure in their own settings.

enableTagLines                    Whether or not to enable the option for users
                                  to choose a tagline to append to their
                                  message upon saving the message.  This
                                  setting serves as the default for the user
                                  setting, which users can configure in their
                                  own settings.

dictionaryFilenames               Dictionary filenames (used for spell check).
                                  This is a comma-separated list of dictionary
                                  filenames.  The dictionary filenames are in
                                  the format dictionary_<language>.txt, where
                                  <language> is the language name.  In this
                                  list, the filenames can be in that format, or
                                  just <language>.txt, or just <language>.
                                  Leave blank to use all dictionary files that
                                  exist on the system.  The dictionary files
                                  are located in either sbbs/mods, sbbs/ctrl,
                                  or the same directory as SlyEdit.

memeMaxTextLen                    For appending a 'meme' to a message, this
                                  specifies the maximum text length for a meme.

memeDefaultWidth                  The default width for memes

memeStyleRandom                   For appending a meme to a message, whether or
                                  not to choose a random style (border style &
                                  color) for the meme. The user will still be
                                  able to change it. Valid values are true and
                                  false. If this is true, the border & color
                                  settings will be ignored.
								  
memeDefaultBorder                 Default border for posting a meme. This can be
                                  one of the following:
                                  none
                                  single
                                  mixed1
                                  mixed2
                                  mixed3
                                  double
                                  ornate1
                                  ornate2
                                  ornate3
								  This can also be a number between 1 and the
                                  maximum number of meme border styles. You can
                                  refer to sbbs/exec/load/meme_lib.js - Near the
                                  top, there are definitions such as
                                  BORDER_NONE, BORDER_SINGLE, etc., up to
                                  BORDER_COUNT (which is the number of border
                                  styles supported).

memeDefaultColor                  For appending a meme to a message, this is a
                                  number that specifies the coloring for the
                                  meme. This can be between 1 and the maximum
                                  number of coloring options supported by
                                  sbbs/exec/load/meme_chooser.js. You can refer
                                  to sbbs/exec/load/meme_chooser.js. At the time
                                  of this writing, under "function main" in that
                                  file, there is an array of attribute codes,
                                  declared via "var attr", and there are are 7
                                  of them as of now.

memeJustify                       For appending a meme to a message, this
                                  specifies the text justification. Valid
                                  values are center, left, and right.

String settings
-----------------
Setting                           Description
-------                           -----------
stringsFilename                   The name of the file containing text strings
                                  displayed in SlyEdit. The default is
                                  SlyEditStrings_En.cfg, which is located in
                                  sbbs/ctrl - You can copy this to sbbs/mods and
                                  keep a modified copy there if you want. The
                                  strings in this file can contain Synchronet
                                  attribute codes (prefixed with \x01).

Ice colors
----------
Setting                           Description
-------                           -----------
ThemeFilename                     The name of the color theme file to use.
                                  Note: Ice-style theme settings are described
                                  in Section 5: Ice-style Color Theme Settings.
                                  If no theme file is specified, then default
                                  colors will be used.
menuOptClassicColors              Whether or not to use the classic Ice-style
                                  menu option colors.  Valid options are true
                                  and false.  If this option is set to false,
                                  then new colors will be used for Ice-style
                                  menu options.  If this is set to true, then
                                  the classic Ice-style menu option colors will
                                  be used.  Note that if this is set to false,
                                  the color setting SelectedOptionTextColor
                                  won't be used.

DCT colors
----------
Setting                           Description
-------                           -----------
ThemeFilename                     The name of the color theme file to use.
                                  Note: DCT-style theme settings are described
                                  in Section 8: DCT-style Color Theme Settings.
                                  If no theme file is specified, then default
                                  colors will be used.

The color theme files are plain text files that can be edited with a text
editor.  For each color setting, you can specify color attribute characters as
specified in the Synchronet documentation for attribute (Ctrl-A) codes:
http://wiki.synchro.net/custom:ctrl-a_codes
Note that you don't need control (Ctrl-A) characters for the color settings;
just the attribute characters.  A couple examples of specifying colors:

High green: gh
Normal cyan: c


6. Strings configuration file
=============================
The strings configuration file (added in SlyEdit v1.89e, 2025-02-09) is a file
similar in concept to text.dat, which is intended to contain strings displayed
by SlyEdit. The format is like the other configuration files, in name=value
format.

Currently, the only configuratble string in there is areYouThere, which
specifies a string to display for the input timeout warning (like item 668,
AreYouThere in text.dat). The reason SlyEdit has its own version of this string
is in case you decide to change the one in text.dat; for SlyEdit, it should be
a string that can fit on one line on the screen.


7. Ice-style Color Theme Settings
=================================
Note that you don't need control (Ctrl-A) characters for the color settings;
just the attribute characters.

The following options are valid for Ice-style theme files:
----------------------------------------------------------
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

8. DCT-style Color Theme Settings
=================================
Note that you don't need control (Ctrl-A) characters for the color settings;
just the attribute characters.

The following options are valid for DCT-style theme files:
----------------------------------------------------------
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

9. Common colors (appearing in both Ice and DCT color theme files)
==================================================================
Note that you don't need control (Ctrl-A) characters for the color settings;
just the attribute characters.

listBoxBorder                     The color to use for the border of list
                                  boxes, such as the cross-post area selection
                                  box and the text replacements list box.  Note
                                  that this setting replaces crossPostBorder,
                                  which was used in previous versions of
                                  SlyEdit.

listBoxBorderText                 The color to use for text in the borders of
                                  list boxes, such as the cross-post area
                                  selection box and the text replacements list
                                  box.  Note that this setting replaces
                                  crossPostBorderText, which was used in
                                  previous versions of SlyEdit.

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

msgWillBePostedHdr                The color to use for the text "Your message
                                  will be posted into the following area(s)"
                                  text when exiting SlyEdit

msgPostedGrpHdr                   The color to use for the group name header
                                  when listing the message's posted message
                                  areas when exiting SlyEdit

msgPostedSubBoardName             The color to use for the message sub-boards
                                  when listing the message's posted message
                                  areas when exiting SlyEdit

msgPostedOriginalAreaText         The color to use for the text "(original
                                  message area)" when listing the message's
                                  posted message areas when exiting SlyEdit

msgHasBeenSavedText               The color to use for the text "The message
                                  has been saved." when exiting SlyEdit

msgAbortedText                    The color to use for the Message Aborted
                                  text when exiting SlyEdit

emptyMsgNotSentText               The color to use for the Message Not Sent
                                  text when exiting SlyEdit

genMsgErrorText                   The color to use for general message error
                                  text when exiting SlyEdit

listBoxItemText                   The color to use for text appearing in list
                                  boxes (such as the list of text replacements
                                  and the list of tag lines)

listBoxItemHighlight              The color to use for the currently selected
                                  item in list boxes (such as the list of text
                                  replacements and the list of tag lines)

10. Text replacements (AKA Macros)
==================================
SlyEdit version 1.29 added text replacements (AKA Macros), which lets you (the
sysop) define words to be replaced with other text as the user types a message.
This feature can be used, for instance, to replace commonly misspelled words
with their correct versions or to replace swear words with less offensive
words.  This feature is toggled by the enableTextReplacements option in
SlyEdit.cfg can can have one of three values:
false : Text replacement is disabled
true  : Text replacement is enabled and performed as literal search and replace
regex : Text replacement is enabled using regular expressions as implemented by
        JavaScript (more accurately, Synchronet's JavaScript interpreter, which
        at the time of this writing is Mozilla's JavaScript engine (AKA
        JavaScript-C or "SpiderMonkey").

The text searches are performed on single words only, as the user types the
message, and are replaced by whatever text you configure for the word.  The
configuration for text replacing is read from a configuration file called
SlyEdit_TextReplacements.cfg, which is plain text and can be placed in either
sbbs/ctrl or in the same directory as SlyEdit's .js files (SlyEdit.js,
SlyEdit_Misc.js, etc.).  Each line in SlyEdit_TextReplacements.cfg needs to
have the following format:
originalWord=replacementText
where originalWord is the word to be replaced, and replacementText is the text
to replace the word with.

The options for the enableTextReplacements configuration optoin are explained
in more detail below:

- false: Text replacing is disabled

- true: Literal text search and replace - The words will be matched literally
and replaced with the text on the right side of the = in
SlyEdit_TextReplacements.cfg.  In this mode, word matching is not
case-sensitive.  The words can have both uppercase and lowercase letters and
will still be matched to the words configured in SlyEdit_TextReplacements.cfg.
In this mode, if the first letter of the original word is uppercase, then the
first letter of the replaced text will also be uppercase.
While this works, one drawback to literal text searching is that it won't
replace a word if there are punctuation characters or other characters around
the word or if the word is misspelled.

- regex: With this option, SlyEdit will treat the word searches (on the left
side of the = in SlyEdit_TextReplacements.cfg) as regular expressions, as
implemented by JavaScript.  When using regular expressions, SlyEdit will start
trying all the regular expressions provided and apply only the first one that
changes the text and will stop there.
Regular expressions offer a more flexible way to serach and replace text.  For
example, in case a word is surrounded by punctuation or other characters, a
regular expression can be given that will still match the word.  For example,
you might want a regular expression that changes "teh" to "the" (since "teh"
is a common misspelling of "the").  To ensure that the word is replaced even if
the word has other characters around it, you could use a regular exprssion and
replacement such as this:
(.*)teh(.*)=$1the$2
That way, even if the word is enclosed in quotes (such as "teh"), the word will
still be converted to "the".  And to preserve the first letter's case (if it's
uppercase or lowercase), this regular expression and replacement string would
handle that:
(.*)([tT])eh(.*)=$1$2he$3
SlyEdit applies the regular expressions on a per-word basis; that is, the
expression .* in a regular expression will match any characters only in the
last word the user typed, not the entire line.

For more information on regular expressions, the
following are some web pages that explain them:

General information about regular expressions:
http://www.regular-expressions.info/tutorial.html

Information on text grouping and backreferencing with parenthesis (the page
calls them "round brackets") - A very powerful feature of regular expressions:
http://www.regular-expressions.info/brackets.html

General information about regular expressions geared toward JavaScript:
http://www.javascriptkit.com/javatutors/re.shtml

Information about using backreferences with regular expressions in JavaScript:
http://stackoverflow.com/questions/2447915/javascript-string-regex-backreferences

JavaScript-specific information on regular exprssions:
http://www.w3schools.com/js/js_obj_regexp.asp
http://www.w3schools.com/jsref/jsref_obj_regexp.asp

One nice thing about JavaScript's implementation (among others) of regular
expressions is that it supports the use of parentheses for character grouping
and backreferencing to place a portion of the matched text in the replacement
text.  In JavaScript, each numbered capture buffer is preceded by a dollar sign
($).  For example, the regular expression (darn) will match the word "darn" and
store it in buffer 1, and in JavaScript (and with SlyEdit's search and
replace), you would use $1 to refer to the word "darn".  For example, for
(darn), the replacement $1it would replace the word "darn" with "darnit".

11. User settings
=================
Since version 1.32, SlyEdit has the ability for each user to configure some
settings for themselves.  The user settings include the following:
- Whether or not to enable the option to use taglines
- Whether or not to prompt for spell checking when saving a message
- Whether or not to add the original author's initials to quote lines
  (this was previously only configurable in SlyEdit.cfg)
- Whether or not to indent quote lines that use author's intials (this was
  previously only configurable in SlyEdit.cfg)
- Whether or not to trim spaces from quoted lines (when replying to messages).
  Some users indent their paragraphs with spaces as a writing style.  When
  this setting is enabled, this feature removes leading spaces from quoted
  lines when replying to a message.  A user might want to disable this, for
  example, when they want to ensure that a message is quoted exactly as-is.
- Whether or not to auto-sign messages.  When this setting is enabled, the
  user's messages are automatically signed with their alias or their real
  name, depending on the sub-board's settings.  There is additionally a setting
  to specify whehter to use only their first name when signing with their
  real name, and also whether to sign emails with their real name (emails
  don't have a configuration like sub-boards do).
- Dictionaries for spell-check.  If there is only one dictionary configured,
  this option won't appear on the user options list.  If there are multiple
  dictionary files on the BBS machine (in either sbbs/mods, sbbs/ctrl, or in
  the same directory as SlyEdit), the user will be able to choose any or all
  dictionaries to be used for the spell check feature.  Multiple dictionaries
  can be selected, so if the user writes messages in more than one language,
  or if a message could contain multiple languages, the spell-check feature
  will use all selected dictionaries for spell checking.

The user settings files will be stored in the sbbs/data/user directory with the
filename <user number>.SlyEdit_Settings, and the user number will be 0-padded
up to 4 digits.

12. Taglines
============
SlyEdit version 1.32 added the ability for users to optionally choose a tagline
to be appended to their message upon saving the message.  Each user can
configure in their user settings whether or not they want to enable this
feature.  The setting enableTaglines in SlyEdit.cfg sets the default setting,
but users are free to change it for themselves.

The taglines are loaded from the text file SlyEdit_Taglines.txt, which can be
located in any of the following directories, searched in the following order:
- sbbs/mods
- sbbs/ctrl
- The same directory as SlyEdit's .js files

Synchronet version 3.16 has the ability to place the tag line after the user's
signature, so if you are running version 3.16 or newer of Synchronet, the tag
line will be placed after the user's signature, if the user has a signature.

Specifically, Synchronet 3.16 starting with the beta build on July 5, 2015 will
behave that way; with earlier versions of Synchronet, the tag line will be
placed before the user's signature.  On June 22, 2015, Synchronet added support
for a file called editor.tag (in each node directory's temp directory) which
can contain an editor tagline, and on July 5, 2015, Synchronet included an
update in which the full path & filename to the tagline file will be written
to line 7 of the MSGINF drop file, which SlyEdit reads on startup.  If the
MSGINF file includes the 7th line, then the tagline will appear after the
user's signature (if they have one).  If the MSGINF file does not include the
7th line, then the tagline will appear before the user's signature.


13. Spell check and dictionaries
================================
Since version 1.64, SlyEdit has a spell check feature.  Spell check can be
started by the user with the Ctrl-R hotkey, or by the Edit > Spell Checker
option in the DCT-style ESC menu (only available when in DCT mode).  For spell
check, SlyEdit will check each word to see if it exists in the configured
dictionary/dictionaries, and if not, it will prompt the user for a corrected
version of the word.  The sysop can configure one or multiple dictionaries in
SlyEdit.cfg with the dictionaryFilenames option.  Dictionary files need to be
in the format dictionary_<languageName>.txt, where <languageName> is the name
of the language.  Also, ideally, the language names should follow standard
language tags, as in "en-us" for English (US), etc.  See these web pages for
more information:
https://en.wikipedia.org/wiki/Language_localisation
http://www.lingoes.net/en/translator/langcode.htm
Also, the dictionary filenames will be parsed and the language name will be
used in the menu of dictionaries in the user options - The user is able to
enable/disable the configured dictionaries for their own use for the spell
checker.

For the dictionaryFilenames option in SlyEdit.cfg, you can specify a comma-
separated list of dictionary filenames.  The dictionary filenames are in the
format dictionary_<languageName>.txt, where <language> is the language name.
In this list, the filenames can be in that format, or just <languageName>.txt,
or just <languageName>.  You can leave the setting blank to use all dictionary
files that exist on the system.

SlyEdit will search for the dictionary files in the following directories, in
this order:
1. sbbs/mods
2. sbbs/ctrl
3. SlyEdit's own directory

Dictionary files must be sorted in order for word matching to work properly
(SlyEdit uses a binary search for quick word matching, which requires the words
to be sorted).  Also, all of the words in the dictionary files must be lower-
case, since SlyEdit does case-insensitive matching by converting words in the
message to lower-case and comparing them with the words in the dictionary.


14. Version history
===================
Version  Date         Description
-------  ----         -----------
1.92g    2026-02-08   Fix: For Ice mode, when the user changes their quote
                      hotkey (Ctrl-Q or Ctrl-Y), refresh the hotkey displayed
                      in the bottom edit border so that it shows the correct
                      hotkey
1.92f    2026-01-30   Fix for the 'from' name location when using a wide
                      terminal (> 80 columns)
1.92e    2025-11-11   When loading user settings, if the settings object is null
                      or invalid for some reason, ensure it's an object. This
                      this should fix GiitLab issue #1006, filed by Fernando
                      Toledo.
1.92d    2025-10-26   New user setting to allow users to toggle for themselves
                      whether to use Ctrl-Q to quote (if not, Ctrl-Y will be
                      used). The SlyEdit configuration option ctrlQQuote is now
                      a default for the user setting.
1.92c    2025-10-25   New configuration option to toggle whether Ctrl-Q is the
                      hotkey to be used for quoting (if not, Ctrl-Y will be used
                      instead). The option name in SlyEdit.cfg is ctrlQQuote; it
                      can also be toggled using slyedcfg.js.
                      Improvement to user input for choosing a graphic character
                      to insert.
1.92b    2025-08-02   Cross-post selection fix to not select when just enter is
                      pressed (just confirm; this change was mainly in
                      dd_lightbar_menu.js). Also, updated cross-post help text
                      and bottom border help text for othe cross-post menu to
                      explain that better.
1.92a    2025-06-24   Bug fix: For cross-posting, when selecting a sub-board,
                      the additional menu quit keys were set as item-select keys
                      instead; now they are quit keys as they should be, so that
                      quitting/exiting out of the cross-post sub-board selection
                      menu does not select the sub-board.
1.92     2025-06-10   Added a new user setting to toggle whether to wrap quote
                      lines to the terminal width. Turning this off can help
                      preserve message formatting when quoting. Also, added
                      a user toggle option for whether to join wrapped quote
                      lines.
                      Also, the included configuration file is now
                      SlyEdit.example.cfg. You can copy it to SlyEdit.cfg to
                      make your own customizations. slyedcfg.js will copy it
                      to sbbs/mods/SlyEdit.cfg.
1.91     2025-05-20   Bug fix: Message text is now properly re-written when the
                      Program Info box (available from DCT mode) is erased
1.90     2025-05-07   Better behavior when editing a general file rather than a
                      message (i.e., if the user is editing an SSH key): Not
                      adding a space after each (wrapped) line, etc.. Lines will
                      still be wrapped in the user's terminal during editing but
                      should be saved properly.

                      Color/attribute codes are no longer removed from quote
                      lines. Although quote lines still appear in SlyEdit with
                      one (configurable) color, the quoted text will retain
                      color/attribute codes when the message is posted.

                      New feature: A 'meme' can be added to the message by
                      typing /m on an empty line by itself and pressing enter.

                      Bug fix: When closing the User Settings dialog, quote
                      lines are refreshed with the configured quote line color
                      (instead of with any color codes included in those lines)
 
                      Bug fix: When closing the User Settings dialog, existing
                      message lines are refreshed better; sometimes, there were
                      small parts of the beginnings of some lines that were
                      blanked out.
 
                      Bug fix (sort of): DCT mode File menu wasn't drawing when
                      the user presses the ESC key to bring up the menu. I don't
                      know why that's happening. I found a kludge that seems to
                      fix it.
 
                      On startup, SlyEdit now sets the 'normal' attribute on the
                      console to clear away any background color or other
                      attribute(s) that may have been set.
1.89e    2025-02-09   User inactivity timeout: Display a warning message
                      (without messing with the screen). New [STRINGS]
                      configuration section with stringsFilename to specify the
                      name of a strings file, which for now just contains an
                      areYouThere setting
1.89d    2025-01-26   User inactivity timeout improvement (via use of
                      console.getkey() instead of the custom function that was
                      being used)
1.89c    2024-10-29   Open the quote file in binary mode to avoid EOF issues on
                      Windows if a unicode "right arrow" code exists in the
                      message, which would get truncated to ASCII Ctrl-Z (ASCII
                      26), which is interpreted as EOF on Windows.
1.89b    2024-05-11   In SlyEdit_Misc.js, check to see that
                      xtrn_area.editor[user.editor] is an object before using it.
1.89a    2024-05-04   Don't line-wrap poll messages for quoting, as that could
                      mess up the formatting of the poll options.  Also, a
                      minor fix for the 'to' name length in DCT mode when using
                      a wide terminal.
1.89     2024-04-30   Quote wrapping length: When re-wrapping quote lines, read
                      the editor configuration settings for "word-wrap quoted
                      text" as specified in xtrn.ini (settable in scfg) - Wrap
                      quote lines to the width specified there, or if not
                      specified, default to 79 columns. This is to help ensure
                      quoted text is a reasonable width for many terminals. The
                      wrapping logic is also called for prepending quoted text
                      with the quote prefix, so (for now) there needs to be a
                      default quote wrap width.
1.88d    2024-02-16   Header display update for UTF-8. And printing from/to/subj
                      after writing the header with empty data so that the
                      header 'graphic' characters & everything lines up
                      properly.
1.88c    2024-02-12   UTF-8 support in the displayed header and when quoting
                      text and when quoting message text
1.88b    2024-02-11   Previous change reverted; now has "real" UTF-8 support.
                      New feature: Entering a graphic char with Ctrl-G (Ctrl-G
                      was previously the key for general help, which wasn't much)
1.88     2024-02-07   Support for entering UTF-8/Unicode characters; internally
                      uses K_CP437 to convert to CP437, so the strings are still
                      in CP437 internally in Synchronet
1.87a    2023-12-17   Using the msg_area.sub object to check sub-board settings
                      instead of opening the sub-board (for determining whether
                      to post with real name)
1.87     2023-08-15   Improvement to paragraph/line breaks in quote line wrapping
1.86     2023-07-26   Started refactoring the re-wrapping of quote lines to work
                      better for the various quote prefixes used in various
                      messages.
1.85     2023-05-15   Internal: Refactored readColorConfig() in _DCTStuff.js
                      and _IceStuff.js.
                      Removed the readValueSettingConfigFile() function.
1.84     2023-02-10   Sysops: When importing a file from the BBS machine,
                      SlyEdit now prompts to send it immediately or not (if not,
                      edit it before sending).  Sending immediately can be
                      useful for posting ANSI files unmodified.
1.83     2022-12-13   Quote lines that are wider than the user's terminal width
                      are now wrapped to the user's terminal width to ensure
                      all the quote lines are entirely available to be quoted.
1.82     2022-12-01   Bug fix: Added some safety checks when reading the
                      configuration file
1.81     2022-11-26   Refactored the way the configuration file is read. Also,
                      the color configuration files now can just specify
                      attribute characters, without the control character.
1.80     2022-07-04   Added the ability to change/set the text color (using the
                      CTRL-K hotkey).  If desired, changing the text color can
                      be disabled if, and the colors can be saved as ANSI
                      (rather than Synchronet attribute codes) for compatibility
                      with other BBS software & message readers. The
                      allowColorSelection option in SlyEdit.cfg specifies
                      whether or not to allow changing text colors, and the
                      saveColorsAsANSI option specifies whether or not to save
                      the message colors as ANSI.
1.79     2022-06-21   JS strict mode enabled. Small JS issues fixed.
1.78     2022-06-09   Removed high-ascii characters from the SlyEdit JS files;
                      used ascii() with their numeric ASCII values instead. This
                      should avoid issues with text editors converting
                      characters incorrectly.
1.77     2022-05-27   Fixed a few instances where SlyEdit was trying to access
                      sub-board information with an empty sub-board code (in the
                      rare case when no sub-boards are configured).
1.76     2022-03-05   When selecting quote lines in a reply message, SlyEdit now
                      remembers the position in the quote selection menu so that
                      the quote menu isn't always at the top whenever it's
                      opened again.  This issue may have been introduced when
                      SlyEdit was refactored to use DDLightbarMenu for its
                      lightbar stuff.
1.75     2021-12-11   Refactored the cross-post menu and quote selection window
                      to use DDLightbarMenu instead of SlyEdit's own lightbar
                      code.
1.74     2021-01-23   Making use of the new K_NUL and checking user input
                      against null, SlyEdit no longer thinks a 0x0 (sent with
                      CTRL-Space on a Mac) is a timeout.  K_NUL was added on
                      Jan. 21, 2021 by Rob Swindell. (This update was from Deon
                      George)
1.73     2020-03-31   Now uses DDLightbarMenu instead of SlyEdit's own internal
                      choice menu.
1.72     2020-03-04   For cross-posting, to make sure the user can post in a
                      sub-board, SlyEdit now checks the can_post property of the
					  sub-board rather than checking the ARS.  The can_post
					  property covers more cases.
1.71     2020-03-03   Added a new configuration option, allowSpellCheck, which
                      the sysop can use to configure whether or not spell check
                      is allowed.  You might want to disable spell check if the
                      spell check feature causes SlyEdit to abort with an error
                      saying it's out of memory.
1.70     2019-08-15   Fix for a bug introduced in the flowing-line update in
                      1.68 where some quote blocks were sometimes not being
                      included when saving a message.  Also, quote lines are now
                      rapped to the user's terminal width rather than 80
                      olumns.
1.69     2019-08-14   Updated to only use console.inkey() for user input and not
                      use console.getkey() anymore. The change was made in the
                      getUserKey() function in SlyEdit_Misc.js. Also, SlyEdit
                      will now write the editor style (ICE or DCT) to result.ed
                      at the end when a message is saved.  Also, when editing a
                      message, if the cursor is at the end of the last line and
                      the user presses the DEL key, then treat it as a
                      backspace. Some terminals send a delete for backspace,
                      particularly with keyboards that have a delete key but no
                      backspace key.
1.68     2019-08-09   Updated to honor the SUB_ANON and SUB_AONLY flags for the
                      sub-boards when cross-posting so that the "from" name is
                      "Anonymous" if either of those flags enabled.
                      Updated to allow message uploading. Started working on
                      updates to save new text lines as one long line, to help
                      with word wrapping in offline readers etc.
1.67     2019-07-21   Now supports the RESULT.ED drop file, with the ability to
                      change the subject. Synchronet 3.17c development builds
                      from July 21, 2019 onward support result.ed even for
                      editors configured for QuickBBS MSGINF/MSGTMP.
1.66     2019-05-29   Added more parsing for dictionary filenames for 'general'
                      dictionaries and 'supplimental' ones
1.65     2019-05-24   Added support for parsing many standard language tags for
                      the dictionary filenames
1.64     2019-05-24   Started working on adding a spell check feature. Also,
                      updated to use require() instead of load() for .js
                      scripts when possible.
1.63     2019-04-18   Started working on supporting word-wrapping for the entire
                      width of any terminal size, beyond 79.
1.62     2018-11-11   Updated to save the message if the user disconnects, to
                      support Synchronet's message draft feature that was added
                      recently.
1.61     2018-08-03   Updated to delete instances of User objects that are
                      created, due to an optimization in Synchronet 3.17 that
                      leaves user.dat open
1.54     2017-12-26   Improved quoting with author initials when a > character
                      exists in the quote lines: Not mistaking the preceding
                      text as a quote prefix if it has 3 or more non-space
                      characters before the >.  Also fixed an issue where
                      wrapped quote lines were sometimes missing the quote line
                      prefix.
1.53     2017-12-19   Updated the PageUp and PageDown keys to ensure they match
                      what's in sbbsdefs.js, since Synchronet added key codes
                      for those keys on December 17, 2018.  SlyEdit should still
                      work with older and newer builds of Synchronet, with or
                      without the updated sbbsdefs.js.
1.52     2017-12-17   Added the ability for the sysop to toggle whether or not
                      to allow users to edit quote lines, using the
					  configuration option allowEditQuoteLines.
.. Removed some history comments ..
1.00     2009-08-22   Initial public release
         2009-05-11   Started development
