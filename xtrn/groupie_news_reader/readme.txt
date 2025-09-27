                          Groupie (newsgroup reader)
                                Version 1.01
                           Release date: 2025-09-27

                            Initially developed by:
                                Eric Oulashin
                          Sysop of Digital Distortion
                  BBS internet address: digitaldistortionbbs.com
                     Alternate address: digdist.synchro.net
                        Email: eric.oulashin@gmail.com


This file describes the Groupie news reader.

Contents
========
1. Disclaimer
2. Introduction
3. Files included
4. Configuration & setting user credentials
5. Installation
   - Loadable Modules setup
   - Command shell setup
   - Background: Running JavaScript scripts in Synchronet
   - Synchronet command shell background
   - Installing into a command shell
6. Strings referenced from text.dat


1. Disclaimer
=============
I cannot guarantee that this script is 100% free of bugs.  However, I have
tested it, and I used it often during development, so I have some confidence
that there are no serious issues with it (at least, none that I have seen).


2. Introduction
===============
This is a newsgroup (NNTP) reader door for the text terminal (for Synchronet).
This reader interacts directly with a news (NNTP) server, allowing a user to
list newsgroups, choose a newsgroup and list & read messages, post messages, and
reply to messages. Although Synchronet has the ability to provide message sub-
boards that synchronize with newsgroups (via its Newslink script), this news
reader script will connect directly to a news server so that you don't need to
spend time adding sub-boards to your Synchronet setup, and Synchronet won't need
to download the articles to the sub-boards (which would take up space in your
Synchronet setup). This reader also uses each user's own credentials to log into
the news server.

If you provide news server access for your users, it will be up to you to
create accounts for your users and provide them with the server information
(hostname and port, as well as their username and password).

The first time a user runs Groupie, they will be prompted for the news server,
their username, and password for the news server. The server hostname/IP addres,
port, and username will be filled in, but the user can change those, in addition
to providing their password. For instance:

Settings file does not exist.  Configuration settings are needed.
Server hostname: 127.0.0.1
Server port (default=119): 119
Username: Nightfox
Password: 

Once the user has provided that information, Groupie will save their settings
in sbbs/data/user/<user#>.groupie.ini (where <user#> is a 0-padded user number
up to 4 digits). As of this writing, if a user needs their settings changed,
you can delete their .groupie.ini file and the user will be prompted for that
information the next time they run Groupie.

If Groupie is unable to connect to the server when the user runs Groupie (and
it may take some time before the timeout occurs), the user will be given the
chance to change their server settings. If the user does so, it will try to
connect again. If the server doesn't change their server settings at that point,
Groupie will exit.

When a user first runs this reader, if they don't have a settings file yet, the
reader will prompt the user for their server settings.  The hostname, port, and
username will be populated, but the user can change those; the user will also
be prompted for their news server password. These settings will be saved in the
user's configuration file (as mentioned above) and will be loaded the next time
the user runs this reader. In the future, if the reader fails to connect or
authenticate with the news server, the user will be given the option to change
their settings/credentials so that they can successfully log in again.

Additionally, another user data file that will be saved in sbbs/data/user is
<user#>.DDMsgReader_Usage.json, which will contain newsgroup usage information
for the user, such as the user's preferred maximum number of articles to
retrieve for each newsgroup. This file will also store the previous estimated
number of articles in the newsgroups the user has visited.

This reader has a configuration file which is used to specify default server
settings, as well as to specify a filename of a theme configuration file, which
is used for configuring the colors used by the reader.

An ANSI or .asc header file can be loaded and displayed above the newsgroup list
and article lists, as configured in the configuration file. For each, the
list_headers subdirectory will first be searched, then the same directory as the
script. It will first look for an .ans version, then an .asc version. For
example, if myHeader is specified, then the reader will look for myHeader.ans if
it exists, and if not, the reader will look for myHeader.asc. Additionally, you
can have multiple header files for different terminal widths; for example,
myHeader-80.ans for an 80-column terminal, myHeader-140.ans for a 140-column
terminal, etc.

After the reader connects to a news server, it will write a file with a list
of newsgroups, as a cache to make it faster to show the list of newsgroups when
a user later connects to the newsgroup server.  The file will be in the same
directory as groupie.js and will be called <hostname>.grpListCache,
where <hostname> will be the hostname of the NNTP server. You can force it to
refresh the newsgroup list cache by deleting this file.


3. Files included
=================
These are the files included:
- groupie.js: The main news reader script. This is the script to run.
- groupie_cfg.js: This is a configurator for Groupe, which looks similar to
  SCFG and lets you change the settings, as well as edit users' NNTP server
  settings.
- nntp_client_lib.js: A NNTP client library, which provides functions for
  interacting with a NNTP server, such as connecting, logging in, getting lists
  of newsgroups & articles, etc.
- groupie.example.ini: The default configuration file
- default_theme.ini: The default color theme configuration file
- list_headers subdirectory: This has some header ANSI/.asc files you can use
  for display above the newsgroup list and article lists, if you want to.
  Of course, you can always make your own to use with the reader.


4. Configuration & setting user credentials
===========================================
You may wish to configure news server credentials for your users. To do so, you
can use the configurator, groupie_cfg.js. You can run that on your BBS machine
at a command prompt using jsexec. For example:

jsexec groupie_cfg.js

You could also leave off the .js extension:

jsexec groupie_cfg

One thing you can configure to make things easier is the "Default server
password for users" setting in the configurator (or directly edit the
default_server_password option in the .ini file with a text editor) - That is
the default password to use for users on the news server. When a user first
runs Groupie, they won't have any server settings, so it will try to connect
with the default server settings. It's possible that it will fail to connect. By
default, users can't change their server settings, and Groupie will tell them to
ask the sysop to edit their news server settings. There is an option to allow
users to change their server settings - If that's enabled, a user will have the
opportunity to change their server settings upon failure to connect.

In the configurator, the first menu item is "User account configuration" - You
can select that option to add/edit server configuration & credentials for your
users. It will show a list of your users, and when you select a user, it will
show a dialog with server configuration information for them that you can
configure.

In groupie_cfg, the 2nd menu option lets you choose the sorting optoin for the
user list for when you want to edit users' server configuration.

This reader includes a default configuration file, groupie.example.ini.
Copy that to groupie.ini and then edit groupie.ini with any text
editor. The settings are in the format setting=value, where "setting" is the
name of the setting or color, and "value" is the corresponding value to use.

There is an included theme configuration file, default_theme.ini. If you want
to change any of the color settings, make a copy of that file and edit it with
any text editor, and change the value of the theme_config_filename setting in
your groupie.ini to specify the name of your theme file.
The colors specified in the theme configuration file are Synchronet
color/attribute codes (without the control character).

Comments are allowed in the configuration files - Commented lines begin with a
semicolon (;).

The configuration & theme file can be in the same directory as the news reader
(by default, xtrn/groupie).  However, you can also place your
configuration & theme files in one of the following directories:
 sbbs/mods (this directory will be checked first)
 sbbs/ctrl (this directory will be checked second)

The directory where the reader is located (xtrn/groupie) will be checked
third. If your configuration & theme files are not in sbbs/mods or sbbs/ctrl,
then the one in xtrn/groupie will be used.

The configuration settings are described in the sections below:

Main configuration file (groupie.example.ini)
----------------------------------------------------
Setting                               Description
-------                               -----------
use_lightbar_interface                Whether or not to use a lightbar interface.
                                      Valid values are true and false. If the
                                      user's terminal is not ANSI-capable, then
                                      the reader will revert back to a non-
                                      lightbar interface.

users_can_change_their_nttp_settings  Whether or not users are allowed to change
                                      their NNTP settings

receive_bufer_size_bytes              The default receive buffer size for the
                                      NNTP client, in bytes

receive_timeout_seconds               The default receive timeout, in seconds

hostname                              The default hostname/IP address & port of
                                      the news server. Users can specify a
                                      different hostname in their configuration
                                      if they need to.

host_port                             The default server port. Users can specify
                                      a different port in their configuration if
                                      they need to.

default_server_password               The default news server password for users

prepend_foward_msg_subject            When forwarding messages, whether or not
                                      to prepend the subject with "Fwd: ".
                                      Valid values are true and false.

default_max_num_msgs_from_newsgroup   Default maximum number of messages to
                                      retrieve from a newsgroup

msg_save_dir                          For the sysop: This is the default
                                      directory on the BBS machine to save
                                      messages to

newsgroup_list_hdr_filename_base      The filename to use (without the
                                      extension) for a header to display above
                                      the newsgroup list. The list_headers
                                      subdirectory will first be searched, then
                                      the same directory as the script. It will
                                      first look for an .ans version, then an
                                      .asc version. For example, if myHeader is
                                      specified, then the reader will look for
                                      myHeader.ans if it exists, and if not, the
                                      reader will look for myHeader.asc.
                                      Additionally, you can have multiple header
                                      files for different terminal widths; for
                                      example, myHeader-80.ans for an 80-column
                                      terminal, myHeader-140.ans for a
                                      140-column terminal, etc.

newsgroup_list_hdr_max_num_lines      The maximum number of lines from the
                                      newsgroup list header file to use

article_list_hdr_filename_base        The filename to use (without the
                                      extension) for a header to display above
                                      the article list. The list_headers
                                      subdirectory will first be searched, then
                                      the same directory as the script. It will
                                      first look for an .ans version, then an
                                      .asc version. For example, if myHeader is
                                      specified, then the reader will look for
                                      myHeader.ans if it exists, and if not, the
                                      reader will look for myHeader.asc.
                                      Additionally, you can have multiple header
                                      files for different terminal widths; for
                                      example, myHeader-80.ans for an 80-column
                                      terminal, myHeader-140.ans for a
                                      140-column terminal, etc.

article_list_hdr_max_num_lines        The maximum number of lines from the
                                      article list header file to use

theme_config_filename                 The name of the theme configuration file
                                      to use


Theme configuration file
------------------------
The theme configuration file starts with a line that says [COLORS]. Currently,
that is the only section; that is the section that specifies the various colors.

Setting                               Description
-------                               -----------
; Newsgroup list
grpListHdrColor                       The newsgroup list header color

grpListNameColor                      The newsgroup name color for the newsgroup
                                      list

grpListDescColor                      The newsgroup description color for the
                                      newsgroup list

grpListNameSelectedColor              The 'selected' color for a newsgorup name
                                      in the newsgroup list

grpListDescSelectedColor              The 'selected' color for a newsgroup
                                      description in the newsgroup list
									  
lightbarNewsgroupListHelpLineBkgColor For the lightbar interface: The background
                                      color for the hotkey help line shown at
                                      the bottom of the screen for the newsgroup
                                      list
									  
lightbarNewsgroupListHelpLineGeneralColor  For the lightbar interface: General
                                           text color for the hotkey help line
                                           shown at the bottom of the screen
                                           for the newsgroup list

lightbarNewsgroupListHelpLineHotkeyColor  For the lightbar interface: The color
                                          for hotkeys in the hotkey help line
                                          shown at the bottom of the screen
                                          for the newsgroup list

lightbarNewsgroupListHelpLineParenColor  For the lightbar interface: The color
                                         for parenthesis in the hotkey help line
                                         shown at the bottom of the screen
                                         for the newsgroup list

articleListHdrColor                   The header color for the article list

articleNumColor                       The article number color for the article
                                      list

articleFromColor                      The 'from' name color for the article list

articleToColor                        The 'to' name color for the article list

articleSubjColor                      The subject color for the article list

articleDateColor                      The date color for the article list

articleTimeColor                      

; Article list help line (for lightbar mode) - Only used for ANSI terminals
; (7: Background)
lightbarArticleListHelpLineBkgColor   For the lightbar interface: The background
                                      color for the hotkey help line shown at
                                      the bottom of the screen for the article
                                      list

lightbarArticleListHelpLineGeneralColor  For the lightbar interface: General
                                         text color for the hotkey help line
                                         shown at the bottom of the screen for
                                         the article list

lightbarArticleListHelpLineHotkeyColor  For the lightbar interface: The color
                                        for hotkeys in the hotkey help line
                                        shown at the bottom of the screen for
                                        the article list

lightbarArticleListHelpLineParenColor  For the lightbar interface: The color
                                       for parenthesis in the hotkey help line
                                       shown at the bottom of the screen
                                       for the article list

articleNumHighlightedColor            For the lightbar interface: The highlight
                                      color for the article numbers for the
                                      lightbar article list

articleFromHighlightedColor           For the lightbar interface: The highlight
                                      color for the 'from' name for the lightbar
                                      article list

articleToHighlightedColor             For the lightbar interface: The highlight
                                      color for the 'to' name for the lightbar
                                      article list

articleSubjHighlightedColor           For the lightbar interface: The highlight
                                      color for the subjects for the lightbar
                                      article list

articleDateHighlightedColor           For the lightbar interface: The highlight
                                      color for the dates for the lightbar
                                      article list

articleTimeHighlightedColor           For the lightbar interface: The highlight
                                      color for the times for the lightbar
                                      article list

; Scrollable reader mode help line (for lightbar mode) - Only used for ANSI terminals
scrollableReadHelpLineBkgColor        For the lightbar interface: The color for
                                      the background for the hotkey help line
                                      shown at the bottom of the screen when
                                      reading a message

scrollableReadHelpLineGeneralColor    For the lightbar interface: The general
                                      color for text for the hotkey help line
                                      shown at the bottom of the screen when
                                      reading a message

scrollableReadHelpLineHotkeyColor     For the lightbar interface: The color for
                                      hotkeys shown in the hotkey help line at
                                      the bottom of the screen when reading a
                                      message

scrollableReadHelpLineParenColor      For the lightbar interface: The color for
                                      the parenthesis in the hotkey help line at
                                      the bottom of the screen when reading a
                                      message

helpText                              The color to use for text on the help
                                      screens

; Colors for the default message header displayed above message
msgHdrMsgNumColor                     For the default message header: The color
                                      for message numbers in the header shown
                                      above a message when reading a message

msgHdrFromColor                       For the default message header: The color
                                      for the 'from' name in the header shown at
                                      the top of a message when reading a
                                      message

msgHdrToColor                         For the default message header: The color
                                      for the 'to' name in the header shown at
                                      the top of a message when reading a
                                      message

msgHdrToUserColor                     For the default message header: The color
                                      for the 'to' name, when written to the
                                      current user, in the header shown at the
                                      top of a message when reading a
                                      message

msgHdrSubjColor                       For the default message header: The color
                                      for the subject in the header shown at the
                                      top of a message when reading a
                                      message

msgHdrDateColor                       For the default message header: The color
                                      for the date in the header shown at the
                                      top of a message when reading a
                                      message

errorMsg                              The color to use for error messages


5. Installation
================
One way to install this would be to edit your command shell and add an entry to
one of your menus to run groupie.js.

For instance, if you're using the default JavaScript command shell (default.js),
first look for the section defining which menu you want to add the command to
which will run the news reader.  As an example, the main menu commands are
defined starting with the line that looks like this:

const main_menu = {

Within there, there is a line that looks like this:
        command: {
That 'command' line is basically the beginning of a set of commands for the main
menu that perform various actions.  In there, you can add a command to run the
news reader similar to this:
         '/R': { eval: 'bbs.exec("?../xtrn/groupie/groupie.js")' },
In this example, when the user types /R, it will run the news reader. You can
change the command string/hotkey ('/R') to whatever you like.

If you're using your own custom command shell, you'd have to just add the
following to any new command of one of your menus:

bbs.exec("?../xtrn/groupie/groupie.js")

Alternately, you could add an entry in External Programs to run the reader in
SCFG; then, in your command shell, you could use bbs.exec_xtrn() to run it with
the internal code.

This is an example of a SCFG entry for External Programs to run the reader:
╔═════════════════════════════════════════════════════[< >]╗
║                  Groupie (News Reader)                   ║
╠══════════════════════════════════════════════════════════╣
║ │Name                       Groupie (Newsgroup Reader)   ║
║ │Internal Code              GROUPIE_READER               ║
║ │Start-up Directory         ../xtrn/groupie              ║
║ │Command Line               ?groupie.js                  ║
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

bbs.exec_xtrn("GROUPIE_READER");


6. Strings referenced from text.dat
===================================
These string values from text.dat are used in this reader:

Posting (019)
Aborted (030)
