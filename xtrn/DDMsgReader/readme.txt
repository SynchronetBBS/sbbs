                      Digital Distortion Message Reader
                                 Version 1.25
                           Release date: 2019-08-29

                                     by

                                Eric Oulashin
                          Sysop of Digital Distortion
                  BBS internet address: digitaldistortionbbs.com
                     Alternate address: digdist.bbsindex.com
                        Email: eric.oulashin@gmail.com



This file describes the Digital Distortion Message Reader.

Contents
========
1. Disclaimer
2. Introduction
3. Installation & Setup
   - Loadable Modules setup
   - Command shell setup
   - Background: Running JavaScript scripts in Synchronet
   - Command-line parameters
   - Synchronet command shell background
   - Installing into a command shell
4. Header ANSI/asc file
5. User avatars
6. Configuration file & color/text theme configuration file
   - Main configuration file (DDMsgReader.cfg)
   - Theme configuration file
7. Drop file for replying to messages with Synchronet message editors
8. text.dat lines used in Digital Distortion Message Reader


1. Disclaimer
=============
I cannot guarantee that this script is 100% free of bugs.  However, I have
tested it, and I used it often during development, so I have some confidence
that there are no serious issues with it (at least, none that I have seen).


2. Introduction
===============
Digital Distortion Message Reader is a script for Synchronet that provides an
alternate message reading interface.  For ANSI users, a reader interface is
provided which allows scrolling the message up & down (with the up & down arrow
keys, as well as the PageUp & PageDown keys), navigating through the messages
in the message area (AKA sub-board) using the left & right arrow keys, replying
to messages, and other common messagebase functionality.  The Digital
Distortion Message Reader can also list messages in the message area and allows
forward & reverse navigation through the message list using a lightbar or
traditional user interface.  An integrated message area chooser feature is also
included, allowing the user to change to a different message area to read/list
messages.  Message newscan and various types of message searching are also
available.  This script requires Synchronet version 3.15 or newer.

When using Synchronet 3.17, the message voting features added in Synchronet
3.17 are supported.  For regular messages, users can vote a message up or down,
and users can also vote in poll messages.

If the user's terminal does not support ANSI, the reader will fall back to a
traditional user interface (which does not support scrolling).  The user
interface style can also be toggled by the sysop in the configuration file
in case the sysop wants the reader to use the traditional interface even for
ANSI users.

This reader effectively replaces the Digital Distortion Message Lister, which
provided a traditional message reader interface but not the scrollable reader
interface for ANSI users.

Thanks goes to Accession/Access Denied (sysop of The Pharcyde) and Psi-Jack
(sysop of Decker's Heaven) for testing the reader while it was in development.

The following is a list of features:
- Provides an enhanced, very functional yet intuitive user interface for
  reading messages for ANSI users, including the ability to scroll the message,
  navigate forward & back through the messages in the message area, reply to
  messages, etc.  If the user's terminal does not support ANSI, the reader will
  fall back to a more traditional user interface (which, for instance, does not
  allow scrolling the message up & down).
- Can be used to read message sub-boards or personal email
- Allows switching between the enhanced reader interface and the message list
  to allow the user to browse messages & select another message to read.  The
  message list displays a formatted summary list of the messages in the user's
  current message area and can be  navigated forward & backward, and allows
  selection of a message to read (basically, switching into reader mode).  The
  message list can be configured for a lightbar or traditional user interface
  (the lightbar interface will only be available for ANSI users).
- Message scanning and searching is supported: New-message scan, new-to-you
  scan, all-to-you scan, keyword search, from name search, and to name search
- Allows a custom message header ANSI/.asc file to be used in the reader mode.
  If there is no custom header file, a default header style will be used.  For
  There can also be different custom header files for various terminal widths.
- Allows changing to a different message area from within the reader or message
  list.  The area chooser will use a lightbar interface for ANSI users or a
  traditional interface for users whose terminal doesn't support ANSI.
- Allows the user to delete and edit existing messages that they've written, if the
  sub-board supports those operations.
- Allows the user to download file attachments, whether uploaded to their
  mailbox on Synchronet or attached to internet emails
- Allows the user to forward a message to an email address or another user
  (using the O key).  This can be useful, for instance, if the user wants to
  send a message in a public sub-board to their personal email for future
  reference or send a message from a public sub-board to another user to
  discuss the topic privately.
- Allows sysops to save a message to the BBS machine for future reference
- Allows sysops to edit the user account of the message author, if the user's
  account exists on the BBS.  This is done with the U key while reading a
  message.  This can be useful for BBSes that require new users to send a
  message to the sysop when they sign up, in case the sysop needs to edit their
  account.
- Allows the ability to batch-delete multiple messages.  This is most useful,
  for instance, if a user gets many spam emails in their personal inbox.  Batch
  deleting is only allowed when the user has permission to delete messages
  (such as their own personal email).  To batch-delete messages, the user can
  select multiple messages (from the message list) and then press CTRL-D (from
  the message list) to delete them.  Messages can be selected in the following
  ways:
  o Lightbar message list: The spacebar selects an individual message.  CTRL-A
    lets the user select or un-select all messages.
  o Traditional message list: The S key lets the user select or un-select
    messages, by typing message numbers, A to select all, or N to select none
    (un-select all).  The list of message numbers is comma-separated or
    space-separated, allowing for number ranges such as 120-130 for instance.
  o Reader interface: The spacebar selects the message.
To delete the selected messages, the user must be in the message list; the
CTRL-D key combo is used for batch delete, and it will prompt the user for
confirmation before deleting the messages.
- The program settings, colors, and some text can be changed via configuration
  files.  The configuration files may be placed in the same directory as the
  .js script or in the sbbs/ctrl directory.

If a message has been marked for deletion, it will appear in the message list
with a blinking red asterisk (*) after the message number.

When displaying a message to the user, this script will honor the attribute
code toggles set up under Synchronet's configuration program (SCFG),
under Message Options > Extra Attribute Codes.

If a message contains ANSI codes and the scrollable reader interface is being
used, Digital Distortion Message Reader will try to use frame.js and
scrollbar.js if those are available on your BBS filesystem.  The reason is that
frame.js is able to parse and display ANSI in a scrollable interface very well.
If frame.js and scrollbar.js are not available on your BBS system, the reader
will still display the message, but it might look garbled if it has ANSI codes.
So for best results when reading ANSI messages,ensure that you have the latest
frame.js and scrollbar.js in your sbbs/exec/load directory.  frame.js and
scrollbar.js are included in Synchronet 3.16, and I believe in earlier versions
too.  As of this writing, the latest versions of those files can be found in
Synchronet's CVS repository at the following URLs:
frame.js:
http://cvs.synchro.net/cgi-bin/viewcvs.cgi/exec/load/frame.js?view=log
scrollbar.js:
http://cvs.synchro.net/cgi-bin/viewcvs.cgi/exec/load/scrollbar.js?view=log


3. Installation & Setup
=======================
Digital Distortion Message Reader is comprised of the following files:
1. DDMsgReader.js         The Digital Distortion Message Reader script

2. DDMsgReader.cfg        The reader configuration file

3. DefaultTheme.cfg       The default theme file containing colors & some
                          configurable text strings used in the reader

4. DDReadPersonalMail.js  A script to be used for "Read mail" script in
                          Synchronet's Loadable Module configuration

5. DDScanMsgs.js          A script to be used for "Scan Msgs" script in
                          Synchronet's Loadable Module configuration

6. DDScanSubs.js          A script to be used for "Scan Subs" script in
                          Synchronet's Loadable Module configuration

The configuration files are plain text files, so they can be edited using any
editor.

The first 3 files (DDMsgReader.js, DDMsgReader.cfg, and DefaultTheme) can be
placed together in any directory; the next 3 (DDReadPersonalMail.js,
DDScanMsgs.js, and DDScanSubs.js) should be copied to your sbbs/mods directory.

The examples in this document will assume the first 3 files are in the
sbbs/xtrn/DDMsgReader directory.

Loadable Modules setup
----------------------
In Synchronet 3.16 builds starting on April 27, 2015, there are a few new
options in the Loadable Lodules configuration in SCFG: "Read Mail", "Scan
Msgs", and "Scan Subs".  Functionality for these was updated on May 5, 2015.
These loadable modules options can enable the use of a message reader script
for the various message reading, searching, and scanning options provided by
Synchronet.  That is probably the easiest way to install the reader, since it
only requires specifying a script in the Loadable Modules options and does not
require modification of your command shell.  Setting it up that way also has
the advantage that the reader will be used for reading personal email and
performing a message newscan during the login process.  If you have a recent
build of Synchronet 3.16 (or newer), then your version of Synchronet will
support this.  If you are using an older version of Synchronet, skip ahead to
the "Command shell setup" subsection.

To set up the reader with the Loadable Module scripts, do the following:
1. Copy DDReadPersonalMail.js, DDScanMsgs.js, and DDScanSubs.js from the
   "Loadable module scripts" directory to your sbbs/mods directory
2. If you will be running the script from a directory other than
   xtrn/DDMsgReader, edit the above scripts and search for the text "SYSOPS:"
   (without the double-quotes).  One or two lines below that, there is a
   variable called msgReaderPath - Change that so that it contains the path
   where you copied DDMsgReader.js.
3. Run Synchronet's configuration program (scfg from the command prompt, or
   from the GUI, select BBS > Configure).  In there, select System, then
   Loadable Modules
4. For the 'Read Mail' option, put in DDReadPersonalMail
5. For the 'Scan Msgs' option, put in DDScanMsgs
6. For the 'Scan Subs' option, put in DDScanSubs

There are a few search modes that Synchronet provides that Digital Distortion
Message Reader doesn't support yet (such as continuous newscan and browse new
scan), and for those situations, the Loadable Modules scripts will fall back to
the stock Synchronet behavior.

A note about DDReadPersonalMail.js:
For reading personal email, DDReadPersonalMail.js contains a line that you
can change to configure the reader to start up in list mode or reader mode.
To change that, open DDReadPersonalMail.js using a text editor and search for
the text "var readerStartmode" (without the quotes).  You can set the value
to "read" to have the reader start in reader mode or "list" to start in list
mode.

Command shell setup
-------------------
Digital Distortion Message Reader can be set up by adding options to your
command shell to run DDMsgReader.js for any or all of the desired functionality
(reading, searching, message scanning, etc).  The command-line parameters are
described in the subsection "Command-line parameters".  Installing into a
command shell is described in the subsection "Installing into a command shell".

Background: Running JavaScript scripts in Synchronet
----------------------------------------------------
The general syntax for a command to run a JavaScript script in Synchronet is
with a question mark before the .js file.  For example:
?../xtrn/DDMsgReader/DDMsgReader.js

In a Baja script, you can use the 'exec' command to run a JavaScript script, as
in the following example:
exec "?../xtrn/DDMsgReader/DDMsgReader.js"

In a JavaScript script, you can use the bbs.exec() function to run a JavaScript
script, as in the following example:
bbs.exec("?../xtrn/DDMsgReader/DDMsgReader.js");

Alternately, the reader can be installed as an external program (in SCFG in
External Programs > Online Programs (Doors)).  See the following document for
more information:
http://wiki.synchro.net/howto:door:index?s[]=doors

Command-line parameters
-----------------------
The Digital Distortion Message Reader supports command-line parameters to
specify some behavior options. The command-line parameters are used in the
command string after DDMsgReader.js.

Most of the command-line arguments are in -arg=val format, where arg is the
argument (parameter) name, and val is the value for that argument.  Some of
the command-line parameters are simply in -arg format, to enable an option.
For example, -search=new_msg_scan will start the reader to do a new message
scan.  Another example is -personalEmail which lets the user read their
personal email.

The following are the command-line parameters supported by DDMsgReader.js:
-search: A search type.  Available options:
 keyword_search: Do a keyword search in message subject/body text (current message area)
 from_name_search: 'From' name search (current message area)
 to_name_search: 'To' name search (current message area)
 to_user_search: To user search (current message area)
 new_msg_scan: New message scan (prompt for current sub-board, current
               group, or all)
 new_msg_scan_all: New message scan (all sub-boards)
 new_msg_scan_cur_grp: New message scan (current message group only)
 new_msg_scan_cur_sub: New message scan (current sub-board only)
                       This can (optionally) be used with the -subBoard
								       command-line parameter, which specifies an internal
								       code for a sub-board, which may be different from the
								       user's currently selected sub-board.
 to_user_new_scan: Scan for new (unread) messages to the user (prompt
                   for current sub-board, current group, or all)
 to_user_new_scan_all: Scan for new (unread) messages to the user
                       (all sub-boards)
 to_user_new_scan_cur_grp: Scan for new (unread) messages to the user
                           (current group)
 to_user_new_scan_cur_sub: Scan for new (unread) messages to the user
                           (current sub-board)
 to_user_all_scan: Scan for all messages to the user (prompt for current
                   sub-board, current group, or all)
 prompt: Prompt the user for one of several search/scan options to
         choose from
Note that if the -personalEmail option is specified (to read personal email),
the only valid search types are keyword_search and from_name_search.

-suppressSearchTypeText: Disable the search type text that would appear
                         above searches or scans (such as "New To You
                         Message Scan", etc.)
-startMode: Startup mode.   This overrides the startMode option in the
           confiruation file.  Available options are read (or reader) for
           reader mode and list (or lister) for message list mode.
-configFilename: Specifies the name of the configuration file to use.  Defaults
                 to DDMsgReader.cfg.
-subBoard: The sub-board (internal code or number) to read, other than the
           user's current sub-board.  This is optional; if this is specified,
           the sub-board specified by this option will be used instead of the
           user's current sub-board.  If this option is specified, the
           -chooseAreaFirst option will be ignored.
-personalEmail: Read personal email to the user.  This is a true/false value.
                It doesn't need to explicitly have a =true or =false afterward;
                simply including -personalEmail will enable it.  If this option
                is specified, the -chooseAreaFirst and -subBoard options will
                be ignored.
-personalEmailSent: Read personal email to the user.  This is a true/false
                    value.  It doesn't need to explicitly have a =true or =false
                    afterward; simply including -personalEmailSent will enable it.
-chooseAreaFirst: Display the message area chooser before reading/listing
                  messages.  This is a true/false value.  It doesn't need
                  to explicitly have a =true or =false afterward; simply
                  including -chooseAreaFirst will enable it.  If -personalEmail
					   or -subBoard is specified, then this option won't have any
                  effect.

The following parameters generally shouldn't be used unless you know what
you're doing.  These were added for use by the Loadable Modules scripts, which
Synchronet will load for various scenarios:

-userNum: Specify a user number for reading personal email.  This parameter is
          there because although usually the current user will be reading their
          own personal mail, there are situations where a sysop can read other
          users' personal mail.
-allPersonalEmail: Read all personal email (to/from all).  There are instances
                   where Synchronet supports this, but more than likely only
                   for sysops.

Synchronet command shell background
-----------------------------------
If you are already familiar with Synchronet's command shell concepts, you can
skip to the subsection "Installing into a command shell" below.  If you are not
yet familiar with how Synchronet's menus are controlled, the key is that
Synchronet doesn't have a menu editor like some other BBS packages do.
Instead, Synchronet uses a "command shell", which is a script that runs when a
user logs in and controls the flow of activity as it responds to the user's
commands.  Synchronet supports two languages for its scripts: Baja and
JavaScript.  Baja is Synchronet's own scripting language; JavaScript is an
industry standard scripting language that Synchronet has provided extensions
for to allow scripting while also being able to take advantage of some of
JavaScript's other features.  Baja scripts need to be compiled before running,
whereas JavaScript scripts don't.  For more information on Synchronet command
shells & scripting, see the following documentation:
- Synchronet command shells:
http://wiki.synchro.net/custom:command_shell
- Synchronet modules:
http://wiki.synchro.net/module:index?s[]=scripts
- Baja language reference:
http://www.synchro.net/docs/baja.html
- Synchronet's JavaScript object reference (it's best to become familiar with
  JavaScript before referring to this document):
http://www.synchro.net/docs/jsobjs.html

Installing into a command shell
-------------------------------
Examples for running the reader will assume that it's installed in the
directory sbbs/xtrn/DDMsgReader.

If you are unsure which command shell you are using, you are likely using
Synchronet's "default" command shell, which is contained in the files
default.src and default.bin in the synchronet/exec directory.  To modify the
default command shell, you'll need to open default.src with a text editor.
These are the key things to modify in default.src:
- Search for msg_read and replace that with the following:
  exec "?../xtrn/DDMsgReader/DDMsgReader.js -startMode=read"
- Where msg_your_scan appears, run DDMsgReader.js with the following:
  exec "?../xtrn/DDMsgReader/DDMsgReader.js -search=to_user_new_scan -startMode=read"
- Where msg_your_scan_all appears, run DDMsgReader.js with the command-line
  exec "?../xtrn/DDMsgReader/DDMsgReader.js -search=to_user_new_scan_all -startMode=read"

If you are using a JavaScript command shell, the process would be similar.  You
will need to determine where the current message operations are done in the
shell and replace them with the appropriate commands for running
DDMsgReader.js.

The following are example command line strings for running the reader to
perform some common message operations.  These command lines can be used with
the exec command in Baja, bbs.exec() method in JavaScript, or as set up as an
external door in SCFG.  This list is not complete but provides examples of some
common message operations.

- Read messages in the current sub-board:
?../xtrn/DDMsgReader/DDMsgReader.js -startMode=read

- List messages in the current sub-board:
?../xtrn/DDMsgReader/DDMsgReader.js -startMode=list

- New message scan:
?../xtrn/DDMsgReader/DDMsgReader.js -startMode=read -search=new_msg_scan

- New-to-user message scan (scan for new messages to the user):
?../xtrn/DDMsgReader/DDMsgReader.js -startMode=read -search=to_user_new_scan

- Scan for all messages to the user:
?../xtrn/DDMsgReader/DDMsgReader.js -startMode=read -search=to_user_all_scan

- Text (keyword) search in the current sub-board, and list the messages found:
?../xtrn/DDMsgReader/DDMsgReader.js -search=keyword_search -startMode=list

- 'From' name message search in the current sub-board, and list messages found:
?../xtrn/DDMsgReader/DDMsgReader.js -search=from_name_search -startMode=list

- 'To' name message search in the current sub-board, and list messages found:
?../xtrn/DDMsgReader/DDMsgReader.js -search=to_name_search -startMode=list

- Search for all messages to the logged-in user in the current sub-board, and
  list the messages found:
?../xtrn/DDMsgReader/DDMsgReader.js -search=to_user_search -startMode=list

- Read personal email:
?../xtrn/DDMsgReader/DDMsgReader.js -personalEmail -startMode=read

- List personal email:
?../xtrn/DDMsgReader/DDMsgReader.js -personalEmail -startMode=list

- Read sent personal email:
?../xtrn/DDMsgReader/DDMsgReader.js -personalEmailSent -startMode=read


Text customization using text.dat
---------------------------------
Digital Distortion Message Reader uses several lines of text from text.dat
(included with Synchronet in the sbbs/ctrl directory):
- Text # 10 (i.e., "E-mail (User name or number):"): Used when prompting the
user to confirm an email address/user number when sending a private reply to a
message
- Text # 30 (i.e., "Aborted."): Used when saving a reaply message was aborted
(i.e., because the user typed an empty email address)
- Text # 54 (i.e., "Delete mail from %s"): Used to confirm whether the user
wants to delete a personal email
- Text # 563 (i.e., "[Hit a key] "): Used for the screen pause when displaying
the help screen
- Text # 662 (i.e., "Download attached file: xyz.txt (500 bytes)"): Used to
confirm downloading an attached file

the user chooses to downloaded attached files, the reader will prompt to
confirm downloading of each file.  For the prompt text, the reader will use
text number 662 from text.dat (in the sbbs/ctrl directory).  If you want to
customize the text for that confirmation prompt, you would need to open
text.dat and modify text number 662.

4. Header ANSI/asc file
=======================
Digital Distortion Message Reader supports its own message header ANSI/.asc
file to be displayed above a message.  This is separate from Synchronet's
msghdr file in the sbbs/text/menu directory.  Digital Distortion Message
Reader's header file is read from the same directory as DDMsgReader.js and
needs to have the filename enhMsgHeader and can be in .ans or .asc format.
enhMsgHeader is short for "enhanced message header".  If an ANSI-format (.ans)
version is found, it will be converted to a Synchronet .asc file using
Synchronet's ans2asc tool before being displayed.  Also, you can create header
files for multiple terminal widths: To do so, the header filename format is
enhMsgHeader-width.  For example, to create a header for a 132-column terminal,
the header filename would be enhMsgHeader-132.asc (or .ans).  Digital
Distortion Message Reader will choose the one matching the user's terminal
width, if one exists.  If no others are found, enhMsgHeader.asc (or .ans) will
be used, if it exists.  If no enhMsgHdr file exists, Digital Distortion Message
Reader will use a default header (which adjusts to any terminal width).

Many of Synchronet's @-codes (message variables) related to message information
are supported in the enhMsgHeader file.  For a list of Synchronet's @-codes,
refer to the following web page:
http://www.synchro.net/docs/customization.html#MessageVariables
In addition, the @-codes can be displayed in fixed-width fields in the
enhMsgHeader file, using # characters in the @-code.  For instance, to display
a message's subject left-justified in a width of 30 characters, the @-code
would look like this:
@MSG_SUBJECT-L###############@

There is also an additional @-code supported by Digital Distortion Message
Reader, MSG_NUM_AND_TOTAL (and MSG_NUM_AND_TOTAL-L), which will display both
the current message number and the total number of messages in the message
area.  It can also be used in a fixed-width field.  For example, to display it
left-justified in a width of 30 characters, the @-code would look like this:
@MSG_NUM_AND_TOTAL-L#########@

Another additional @-code supported by Digital Distortion Message Reader is
@MSG_FROM_AND_FROM_NET@, which shows the 'from' username along with the 'from'
network type in paranthesis.  There is also a -L version for left-
justification with length.  For instance:
@MSG_FROM_AND_FROM_NET-L######@


5. User avatars
===============
Digital Distortion Message Reader supports Synchronet's Avatar feature, which
was added in Synchronet 3.17.  Basically, this feature allows users to have a
small (10x6) text-based artwork that represents themselves and is displayed in
the message header when reading messages.  Digital Distortion Message Reader
has a setting in the configuration file, displayAvatars, which lets you toggle
whether or not to dislpay user avatars.  Valid values are true and false.  For
more information on Synchronet's Avatar feature, see the following wiki page
with a web browser:
http://wiki.synchro.net/module:avatars
For the avatars feature to work, ensure you have all the latest Synchronet .js
files in your sbbs/exec and sbbs/exec/load directories.  Specifically, Digital
Distortion Message Reader loads smbdefs.js and avatar_lib.js, which are both
in the sbbs/exec/load directory.  If those files are not there, then Digital
Distortion Message Reader will still work but it won't display avatars.
When avatars are enabled with Digital Distortion Message Reader, they will be
displayed in the message header above a message, on the right side of the
screen.  If you create a custom message header file for Digital Distortion
Message Reader, you may want to reserve the rightmost 12 characters for the
user avatar.


6. Configuration file & color/text theme configuration file
===========================================================
Digital Distortion Message Reader allows changing some settings, colors, and
some of the text via configuration files.  The configuration files are plain
text and can be edited with any text editor.  These are the configuration files
used by Digital Distortion Message Reader:
- DDMsgReader.cfg: The main configuration file
- DefaultTheme.cfg: Defines colors & some text strings used in the reader.
  The name of this file can be specified in DDMsgReader.cfg, so that alternate
  "theme" configuration files can be used if desired.

Each setting in the configuration files has the format setting=value, where
"setting" is the name of the setting or color, and "value" is the corresponding
value to use.  The colors specified in the theme configuration file are
Synchronet color/attribute codes.  Comments are allowed in the configuration
files - Commented lines begin with a semicolon (;).

Digital Distortion Message Reader will look for the configuration files in the
following directories, in the following order:
1. sbbs/mods
2. sbbs/ctrl
3. The same directory as DDMsgReader.js
If you customize your configuration files, you can copy them to your sbbs/mods
or sbbs/ctrl directory so that they'll be more difficutl to accidentally
override if you update your xtrn/DDMsgReader from the Synchronet CVS
repository, where this reader's files are checked in.

The configuration settings are described in the sections below:

Main configuration file (DDMsgReader.cfg)
-----------------------------------------
Setting                               Description
-------                               -----------
interfaceStyle                        String: The user interface to use.  Valid values
                                      are Traditional (traditional user interface
                                      with user prompted for input at the end of each
                                      screenful) and Lightbar (use the lightbar user
                                      interface).

reverseListOrder                      true/false: Whether or not to display the list
                                      in reverse order, or the string "ask" to ask the user
                                      whether they want the list displayed in reverse.  If
                                      the list is displayed in reverse, that has the effect
                                      of listing in descending order by  date & time.

readerInterfaceStyle                  The user interface style to use for the
                                      message reader.  Valid values are
                                      Scrollable and Traditional.  The
                                      scrollable interface allows scrolling the
                                      message up and down and is only available
                                      for ANSI users.  If a user is not using
                                      ANSI, the reader will use the traditional
                                      user interface instead, regardless of
                                      this setting.

readerInterfaceStyleForANSIMessages   The user interface style to use for
                                      reading messages with ANSI content.  Valid
                                      values are Scrollable and Traditional.
                                      The scrollable interface allows scrolling
                                      the message up and down.  If false, the
                                      reader will use a traditional (non-
                                      scrolling) user interface to display
                                      messages with ANSI content.  If a user is
                                      not using ANSI, the reader will use the
                                      traditional user interface instead,
                                      regardless of this setting.

displayBoardInfoInHeader              true/false: Whether or not to display sub-board
                                      information above the column headers when listing
                                      the message information.  If this is set to true,
                                      2 extra lines on the screen will be used at the top
                                      to display message group and sub-board.

promptToContinueListingMessages       true/false: Whether or not to prompt the
                                      user to continue listing messages after
                                      a message is read.

promptConfirmReadMessage              true/false: Whether or not to prompt the
                                      user to read a message when one is
                                      selected.

msgListDisplayTime                    Specifies the message date to display.
                                      Valid values are imported and written.
                                      imported: Display the message import dates
                                      written: Display the message written dates
                                      When using the message written dates, the
                                      message written dates will be adjusted to
                                      the BBS's local time zone so that they are
                                      all consistent.

msgAreaList_lastImportedMsg_time      In the message sub-board lists, the date
                                      & time of the last message in the
                                      sub-board.  This setting specifies
                                      whether to use the imported time or the
                                      written time.  Valid values are imported
                                      and written.  When using the message
                                      written dates, the message written dates
                                      will be adjusted to the BBS's local time
                                      zone so that they are all consistent.

startMode                             Specifies whether to start in list mode
                                      or reader mode.  Valid values are Reader
                                      (or Read) and Lister (or List).  Note
                                      that this setting can be overridden by
                                      the -startMode command-line argument
                                      (valid values are list and read).

tabSpaces                             The number of spaces to use for tabs in
                                      the message reader (tabs will be replaced
                                      by this many spaces).

pauseAfterNewMsgScan                  Whether or not to pause (i.e., with a
                                      "finished" message) after doing a new
                                      message scan.  Valid values are true
                                      and false.

readingPostOnSubBoardInsteadOfGoToNext  For reading messages (not for a newscan,
                                      etc.): Whether or not to ask the user
                                      whether to post on the sub-board in reader
                                      mode after reading the last message
                                      instead of prompting to go to the next
                                      sub-board.  This is like the stock
                                      Synchronet behavior. Valid values are true
                                      and false.  This defaults to false.

areaChooserHdrFilenameBase            The filename to use (without the
                                      extension) for a header to display above
                                      the message area chooser list.  For
                                      example, if areaChgHeader is specified,
                                      then the reader will look for
                                      areaChgHeader.ans if it exists, and if
                                      not, the reader will look for
                                      areaChgHeader.asc.  Additionally, you
                                      can have multiple header files for
                                      different terminal widths; fpr example,
                                      areaChgHeader-80.ans for an 80-column
                                      terminal, areaChgHeader-140.ans for a
                                      140-column terminal, etc.

areaChooserHdrMaxLines                The maximum number of lines to use from
                                      the message area chooser header file.

displayAvatars                        Whether or not to display user avatars in
                                      message headers.  Valid values are true
                                      and false.

rightJustifyAvatars                   Whether or not to right-justify avatars.
                                      Valid values are true and false.  Flase
                                      means to left-justify avatars.

themeFilename                         The name of the configuration file to
                                      use for colors & string settings

Theme configuration file
------------------------
The convention for the setting names in the theme configuration file is that
setting names ending in 'Text' are for whole text strings, and the setting
names that don't end in 'Text' are for colors.

Setting                              Description
-------                              -----------
headerMsgGroupTextColor              Color for the header line in the message
                                     list displaying the message group, for the
                                     text "Current msg group:"

headerMsgGroupNameColor              Color for the message list header line
                                     displaying the message group

headerSubBoardTextColor              Color for the message list header line
                                     displaying the message sub-board, for the
                                     text "Current msg sub-board:"

headerMsgSubBoardNameColor           Color for the message list header line
                                     displaying the message sub-board

listColHeaderColor                   Color for the column names in the message
                                     list

msgListMsgNumColor                   Color for the message number (in the
                                     message list)
msgListFromColor                     Color for the sender name (in the message
                                     list)
msgListToColor                       Color for the destination name (in the
                                     message list)
msgListSubjectColor                  Color for the message subject (in the
                                     message list)
msgListScoreColor                    Color for the message score (in the
                                     message list) - For terminals at least
                                     86 characters wide
msgListDateColor                     Color for the message date (in the message
                                     list)
msgListTimeColor                     Color for the message time (in the message
                                     list)

msgListToUserMsgNumColor             Color for the message numer, for messages
                                     to the user (in the message list)
msgListToUserFromColor               Color for the sender name, for messages to
                                     the user (in the message list)
msgListToUserToColor                 Color for the destination name, for
                                     messages to the user (in the message list)
msgListToUserSubjectColor            Color for the message subject, for
                                     messages to the user (in the message list)
msgListToUserScoreColor              Color for the message score, for
                                     messages to the user (in the message list)
                                     - For terminals at least 86 characters
                                     wide
msgListToUserDateColor               Color for the message date, for messages
                                     to the user (in the message list)
msgListToUserTimeColor               Color for the message time, for messages
                                     to the user (in the message list)

msgListFromUserMsgNumColor           Color for the message number, for messages
                                     from the user (in the message list)
msgListFromUserFromColor             Color for the mender name, for messages
                                     from the user (in the message list)
msgListFromUserToColor               Color for the mestination name, for
                                     messages from the user (in the message
                                     list)
msgListFromUserSubjectColor          Color for the message subject, for
                                     messages from the user (in the message
                                     list)
msgListFromUserScoreColor            Color for the message score, for
                                     messages from the user (in the message
                                     list) - For terminals at least 86
                                     characters wide
msgListFromUserDateColor             Color for the message date, for messages
                                     from the user (in the message list)
msgListFromUserTimeColor             Color for the message time, for messages
                                     from the user (in the message list)

msgListHighlightBkgColor             Message list highlighting background color
                                     (for lightbar mode)
msgListMsgNumHighlightColor          Message list highlighted message number
                                     color (for lightbar mode)
msgListFromHighlightColor            Message list highlighted 'from' name color
                                     (for lightbar mode)
msgListToHighlightColor              Message list highlighted 'to' name color
                                     (for lightbar mode)
msgListSubjHighlightColor            Message list highlighted subject color
                                     (for lightbar mode)
msgListScoreHighlightColor           Message list highlighted score color
                                     (for lightbar mode) - For terminals at
                                     least 86 characters wide
msgListDateHighlightColor            Message list highlighted date color (for
                                     lightbar mode)
msgListTimeHighlightColor            Message list highlighted time color (for
                                     lightbar mode)

lightbarMsgListHelpLineBkgColor      Background color for the lightbar message
                                     list help line
lightbarMsgListHelpLineGeneralColor  Color for the general text in the lightbar
                                     message list help line
lightbarMsgListHelpLineHotkeyColor   Color for the hotkeys in the lightbar
                                     message list help line
lightbarMsgListHelpLineParenColor    Color for the ) characters in the lightbar
                                     message list help line

tradInterfaceContPromptMainColor     Color for the text displayed in the
                                     'continue' prompt in the traditional user
                                     interface in the message list

tradInterfaceContPromptHotkeyColor   Color for the hotkeys displayed in the
                                     'continue' prompt in the traditional user
                                     interface in the message list

tradInterfaceContPromptUserInputColor Color for the user input at screen pauses
                                      in the traditional user interface for the
                                      message list

msgBodyColor                         Color for the message body when reading a
                                     message

readMsgConfirmColor                  Color for the text used to confirm whether
                                     to read a message

readMsgConfirmNumberColor            Color for the message number displayed
                                     when asking the user if they really want
                                     to read the message

afterReadMsg_ListMorePromptColor     Color for the text used for asking the user
                                     if they want to continue listing messages

tradInterfaceHelpScreenColor         Color for the text used in the traditional
                                     user interface message list help screen

areaChooserMsgAreaNumColor            Color for the message area numbers when
                                     choosing a different message area

areaChooserMsgAreaDescColor          Color for the message area descriptions
                                     when choosing a different message area
  
areaChooserMsgAreaNumItemsColor      Color for the number of items when
                                     choosing a different message area

areaChooserMsgAreaHeaderColor        Color for the message area header line
                                     when choosing a different message area.
                                     This is the line that shows the
                                     message group and page number.

areaChooserSubBoardHeaderColor       Color for the sub-board information line
                                     when choosing a different message area

areaChooserMsgAreaMarkColor           Color for the mark character showing the
                                     current message area when choosing a
                                     different message area

areaChooserMsgAreaLatestDateColor    Color for the latest message date when
                                     choosing a different message area

areaChooserMsgAreaLatestTimeColor    Color for the latest message date when
                                     choosing a different message area

- Highlighted versions of the above message area list colors:
areaChooserMsgAreaBkgHighlightColor
areaChooserMsgAreaNumHighlightColor
areaChooserMsgAreaDescHighlightColor
areaChooserMsgAreaDateHighlightColor
areaChooserMsgAreaTimeHighlightColor
areaChooserMsgAreaNumItemsHighlightColor

lightbarAreaChooserHelpLineBkgColor     Background color for the lightbar
                                        message area chooser help line
lightbarAreaChooserHelpLineGeneralColor Color for the general text in the
                                        lightbar message area chooser help
                                        line
lightbarAreaChooserHelpLineHotkeyColor  Color for the hotkeys in the lightbar
                                        message area chooser help line
lightbarAreaChooserHelpLineParenColor   Color for the ) characters in the
                                        lightbar message area chooser help line

scrollbarBGColor                     Color for the scrollbar background in the
                                     scrollable message reader interface

scrollbarBGChar                      The character to use for the scrollbar
                                     background characters in the scrollable
                                     message reader interface

scrollbarScrollBlockColor            Color for the moving scrollbar block in
                                     the scrollable message reader interface

scrollbarScrollBlockChar             The character to use for the moving
                                     scrollbar block characters in the
                                     scrollable message reader interface

enhReaderPromptSepLineColor          Color for the line drawn in the 2nd to
                                     last line of the message area in the
                                     scrollable message reader interface
                                     before a prompt is displayed on the next
                                     line

goToPrevMsgAreaPromptText            Text to use for prompting the user whether
                                     or not to go to the previous message area
                                     (without the ? on the end)

goToNextMsgAreaPromptText            Text to use for prompting the user whether
                                     or not to go to the next message area
                                     (without the ? on the end)

enhReaderHelpLineBkgColor            Color to use for the background of the
                                     hotkey help line displayed at the bottom
                                     of the scrollable message reader interface

enhReaderHelpLineGeneralColor        Color to use for general text in the
                                     hotkey help line displayed at the bottom
                                     of the scrollable message reader interface

enhReaderHelpLineHotkeyColor         Color to use for hotkeys in the hotkey
                                     help line displayed at the bottom of the
                                     scrollable message reader interface

enhReaderHelpLineParenColor          Color to use for ) characters in the
                                     hotkey help line displayed at the bottom
                                     of the scrollable message reader interface

postOnSubBoard                       The text to use for asking the user whether
                                     they want to post on a sub-board (for
                                     instance, after reading the last message).
                                     The two %s will be replaced with the
                                     message group name and sub-board
                                     description, respectively.

newMsgScanText                       The first text displayed when doing a new
                                     message scan, before the sub-board/group/
                                     all prompt is displayed

newToYouMsgScanText                  The first text displayed when doing a
                                     new-to-you message scan, before the
                                     sub-board/group/all prompt is displayed

allToYouMsgScanText                  The first text displayed when doing a
                                     all-messages-to-you message scan, before
                                     the sub-board/group/all prompt is
                                     displayed

scanScopePromptText                  The text used for prompting to scan in the
                                     sub-board, group, or all (or ENTER to
                                     cancel)

msgScanCompleteText                  Text to display when the message scan is
                                     complete

msgScanAbortedText                   Text to display when the message scan has
                                     been aborted

searchingSubBoardAbovePromptText     Text for "Searching message sub-board: .."
                                     above the search text prompt (%s is
                                     replaced with the sub-board name)

searchingSubBoardText                For displaying the sub-board name when
                                     doing a search.  %s will be replaced with
                                     a sub-board name.

noMessagesInSubBoardText             No messages in a sub-board (i.e., trying
                                     to read a sub-board that has no messages).
                                     %s will be replaced with a sub-board name.

noSearchResultsInSubBoardText        For no search results found in a
                                     sub-board.  %s will be replaced with a
                                     sub-board name.

readMsgNumPromptText                 Prompt text to input a number of a message
                                     to read

invalidMsgNumText                    Text to display for an invalid message
                                     number.  %d will be replaced with the
                                     message number.

msgHasBeenDeletedText                Text to display when a message has been
                                     deleted.  %d will be replaced with the
                                     message number.

noKludgeLinesForThisMsgText          Text to display when the user tries to
                                     display kludge lines but there are no
                                     kludge lines in the message

searchingPersonalMailText            Text for "Seraching personal mail"

searchTextPromptText                 Text for prompting for search text

fromNamePromptText                   Text for prompting for a 'from' name

toNamePromptText                     Text for prompting for a 'To' name

abortedText                          Text for an aborted operation (i.e.,
                                     "Aborted")

loadingPersonalMailText              Text for the status message "Loading
                                     personal mail..."  %s will be replaced
                                     with "Personal mail".

msgDelConfirmText                    Prompt text to confirm message deletion
                                     (without the ? at the end).  %d will be
                                     replaced with the message number.

delSelectedMsgsConfirmText           Prompt text to confirm deletion of
                                     selected messages (i..e, for batch message
                                     deletion), without the ? at the end.

msgDeletedText                       Text for when a message has been marked
                                     for deletion.  %d will be replaced with
                                     the message number.

selectedMsgsDeletedText              Text for when selected messages have been
                                     marked for deletion

cannotDeleteMsgText_notYoursNotASysop  Error text for cannot delete a message
                                       because the message is not the user's
                                       message or the user is not a sysop.  %d
                                       will be replaced with the message
                                       number.

cannotDeleteMsgText_notLastPostedMsg   Error text for cannot delete a message
                                       because it's not the user's last posted
                                       message in a sub-board.  %d will be
                                       replaced with the message number.

msgEditConfirmText                   Prompt text to confirm message edit
                                     (without the ? at the end).  %d will be
                                     replaced with the message number.

noPersonalEmailText                  The text to output when the user has no
                                     personal email messages (i.e., "You have
                                     no messages.")

deleteMsgNumPromptText               The text to output when prompting for the
                                     number of a message to delete

editMsgNumPromptText                 The text to output when prompting for the
                                     number of a message to edit

hdrLineLabelColor                    The color to use for the header/kludge
                                     line labels

hdrLineValueColor                    The color to use for the header/kludge
                                     line values

selectedMsgMarkColor                 The color to use for the checkmark for
                                     selected messages (used in the message
                                     list)

7. Drop file for replying to messages with Synchronet message editors
=====================================================================
When reading a message, the message lister will write a drop file in the node
directory, called DDML_SyncSMBInfo.txt, which contains some information about
the message being read, for use by Synchronet message editors.  This drop file
is provided for Synchronet message editors to get information about the current
message being read if needed.  In Synchronet's JavaScript object model, there
are certain variables that are provided about the current message and sub-board
being read that are normally updated by Synchronet's built-in message read
prompt, but JavaScript scripts cannot modify those values.  Because of that,
the solution was for the message lister to provide the message information in a
drop file in the node directory just before allowing the user to reply to a
message.  When the user is done replying to the message, DDML_SyncSMBInfo.txt
will be deleted.
DDML_SyncSMBInfo.txt contains the following information:
Line 1: The highest message number in the sub-board.  Equivalent to
        bbs.smb_last_msg in Synchronet's JavaScript object model.
Line 2: The total number of messages in the sub-board.  Equivalent to
        bbs.smb_total_msgs in Synchronet's JavaScript object model.
Line 3: The (absolute) message number of the message being read.  Equivalent to
        bbs.msg_number in Synchronet's JavaScript object model and the "header"
        property of a message header.
Line 4: The sub-board code (text).  Equivalent to bbs.smb_sub_code in
        Synchronet's JavaScript object model.

One message editor in particular, SlyEdit, needs to be able to determine which
message is currently being read and replied to so that it can retrieve the
message author's initials for use in quoting the messsage.  Normally, SlyEdit
will get the message information from the aforementioned JavaScript variables
provided by Synchronet, but if DDML_SyncSMBInfo.txt exists, SlyEdit will read
the message information from that file instead.
Note that if you have SlyEdit installed on your BBS, this version of Digital
Distortion Message Reader (1.00) requires version 1.27 or newer of SlyEdit in
order for SlyEdit to properly get the correct message from the message lister.

8. text.dat lines used in Digital Distortion Message Reader
===========================================================
This message reader uses the following lines from Synchronet's text.dat file
(located in the sbbs/ctrl directory):
10 (Email)
30 (Aborted)
54 (DeleteMailQ)
501 (SelectItemHdr)
503 (SelectItemWhich)
563 (Pause)
662 (DownloadAttachedFileQ)
759 (CantReadSub)
779 (VotingNotAllowed)
780 (VotedAlready)
781 (R_Voting)
783 (VoteMsgUpDownOrQuit)
787 (PollVoteNotice)
