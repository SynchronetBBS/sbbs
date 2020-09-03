Multi Relay Chat (MRC) client for Synchronet BBS 3.17b+
echicken -at- bbs.electronicchicken.com

1) Quick Start
2) Client -> Server -> Server
3) Customization
4) Support


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

I may write a CTRL-A to pipe converter at some later date.

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
  - The values in the [startup] section determine which room the client joins
    on startup, and whether the Message of the Day and banners are displayed.


4) Support

- Post a message to 'echicken' in the Synchronet Sysops area on DOVE-Net
- Find me on irc.synchro.net in #synchronet
