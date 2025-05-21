Multi Relay Chat (MRC) client for Synchronet BBS 3.17b+
echicken -at- bbs.electronicchicken.com

1) Quick Start
2) Client -> Server -> Server
3) Customization
4) MRC Stats
5) SSL Support
6) Themes
7) Support


1) Quick Start

The following assumes that Synchronet is installed at /sbbs or c:\sbbs, and
that MRC is installed at /sbbs/xtrn/mrc.  Adjust paths accordingly if your
setup differs.

- Ensure that you have the latest version of /sbbs/exec/load/scrollbar.js:
  http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/scrollbar.js
- In /sbbs/xtrn/mrc:
  - Copy mrc-connector.example.ini to mrc-connector.ini
  - Copy mrc-client.example.ini to mrc-client.ini
- Edit '/sbbs/ctrl/services.ini' and paste in the following section:

    [MRC-Connector]
    Port=5000
    Options=STATIC | LOOP
    Command=../xtrn/mrc/mrc-connector.js

- In SCFG, add a new External Program with the following options, leaving all
  other options at their default values:

    Name: MRC
    Internal Code: MRC
    Start-up Directory: /sbbs/xtrn/mrc
    Command Line: ?mrc-client.js

- Your services and BBS threads should automatically recycle once they are free
  to do so, but you may want to restart your BBS now to force these changes to
  take effect.


2) Client -> Server -> Server

MRC-Connector is a service that runs locally on your BBS.  It maintains a
connection with a remote MRC server, and passes traffic between the remote
server and local clients.  Local clients (users running mrc-client.js) connect
to MRC-Connector rather than connecting to the remote MRC server directly.

You do not have to, and should not, open the MRC-Connector port to the outside
world.  Clients are connecting to it from within your BBS, and in the typical
Synchronet configuration, that means they're coming from the same machine.


3) Customization

Because MRC was originally a Mystic thing, you can use pipe codes wherever
colour codes are permitted:

  http://wiki.mysticbbs.com/doku.php?id=displaycodes#color_codes_pipe_colors


mrc-connector.ini:

  - You can adjust the 'server' and 'port' values if you wish to connect to
    something other than the default MRC server.
  - You can uncomment and edit the fields in the [info] section to alter what
    information is shown about your BBS via the /info command.  If you do not
    uncomment these fields, defaults will be read from your system config.

mrc-client.ini:

  - The 'server' and 'port' values dictate where mrc-client.js will connect to
    MRC-Connector.  The defaults should suffice unless you deviated from the
    above instructions while editing '/sbbs/ctrl/services.ini'.
  - The 'ping_interval' setting should be left at the default value unless you
    have a good reason for changing it.
  - The values in the [startup] section control the following:
      room:     Which room the client joins on startup.
      motd:     Whether the Message of the Day is displayed.
      commands: Any additional commands to execute, eg: "chatters" to display
                the list of active users in the room on startup. Use commas 
                to separate multiple commands.
      splash:   Whether the "Multi Relay Chat for Synchronet" splash is shown
                on startup.
  - The [aliases], [theme], [msg_color], [show_nicks], [twit_list], and 
    [chat_sounds] sections are for user settings which get added or updated any
    time a user uses them.


4) MRC Stats

MRC-Connector makes requests for server stats every 20 seconds. These global 
stats are displayed on the MRC-Client screen by typing /toggle_stats as well 
as stored in the mrcstats.dat file for display elsewhere on the BBS.

For example (optional): The file "chat-with-mrc-stats-example.msg" is included
to demonstrate how to display MRC stats on a menu file. The script file
"mrc-display-menu-stats.js" must be copied to the /sbbs/mods directory since
the menu file calls this script to fetch the stats whenever it's displayed.

*** NOTE: If you are running mrc-display-menu-stats.js like a door
          from your external programs menu, you're doing it wrong.

If you do decide to make use of this msg file, rename it as chat.msg and copy
it to your /sbbs/text/menu directory, replacing the old chat.msg file. Also be
sure to copy "chat_sec-with-mrc-example.js" to your /sbbs/mods directory and
rename it "chat_sec.js", in order to add M as a valid menu option for
Multi-Relay Chat.


5) SSL Support
If you would like connections between your server and the MRC server to be
secure, you can enable SSL as follows:

   edit mrc-connector.ini and make these changes in the upper global section:

   change port from 5000 to 5001
   add ssl=true

Then recycle services with touch /sbbs/ctrl/services.recycle or use your windows
control panel. Client to local server connections are not yet encrypted but that
shouldn't be difficult. Once connected to MRC you can type /BBSES and you should
see "Yes" next to your BBS in the SSL column.


6) Themes

MRC comes with several customizable theme files. These can be added/edited as
the sysop wishes. The client automatically detects any available theme files 
matching the pattern:

    mrc-theme-<theme_name>.ini

Available themes are listed in /help theme. User selects a theme by typing 
/theme <name>, and the selected theme gets saved to settings for future
sessions.

If a theme file is not available, the classic blue theme gets used by default.

To customize the splash that gets shown on startup, edit the mrc-splash.msg
file. Or just disable it by setting splash = false in mrc-client.ini


7) Known issues

- Outgoing messages beginning with a semicolon do not get sent out to the MRC
  server. For example, a winking face emoticon:  ;)  or  ;-)
  
  This is because the ; character is used by the inputline.js library to 
  indicate the start of a sysop command. In the case of the MRC client, such
  text is simply ignored and does not get sent over.
  
  As a workaround, you may simply insert a space in front of the message you 
  want to send that starts with a semicolon.


8) Support

- Post a message to 'echicken' in the Synchronet Sysops area on DOVE-Net
- Find me on irc.synchro.net in #synchronet
