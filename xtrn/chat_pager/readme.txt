Some kinda page-sysop thingie
by echicken, November 2017
echicken at bbs.electronicchicken.com

1) Setup

- Run scfg
- Select 'Chat Features'
- Select 'External Sysop Chat Pagers'
- Hit enter on the blank line at the bottom to create a new record
- Set the 'Command Line' to: ?/sbbs/xtrn/chat_pager/page_sysop.js

2) Configuration

- Browse to the 'xtrn/chat_pager/' directory
- Copy 'example-settings.ini' to a new file, name it 'settings.ini'
- Edit 'settings.ini'
- In the [terminal] section, edit irc_server, irc_port, and irc_channel as needed

3) IRC Bot setup

- Browse to the 'xtrn/chat_pager/ircbot/' directory
- Open 'for-ircbot.ini' with a text editor
- Copy its contents to your clipboard
- Browse to your 'ctrl/' directory
- Open 'ircbot.ini' with a text editor
- Paste the previously copied file contents into the bottom of this file

4) Finishing Up

- Synchronet will need to recycle the terminal server thread to pick up changes
- It may be necessary to restart your BBS
- You will need to issue the 'restart' command to your IRC bot

5) Using

Users who page the sysop will be shown a progress/time bar while they wait for
you to respond.

Your IRC bot will notify you in the channel you've configured that somebody is
paging you.  Respond with the command '!chat n', where 'n' is the node number,
to pull the user into IRC chat.

6) Notes

I haven't tested this much.  Use at your own risk.

If you run your IRC bot via jsexec, the 'IRC Pull' feature will not work; I will
add a workaround for that soon.  It will work if you run your IRC bot as a
service.

Additional notification methods (other than IRC) may be added in the future.

You don't have to use the 'IRC Pull' feature; you can always go to your host
system and break into chat with the built in tools (from sbbsctrl or umonitor).
