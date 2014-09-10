ecReader
A message lister & reader for Synchronet BBS 3.16+

Features:

- Lightbar message group & area selector
- Lightbar flat or threaded message lister
- Scrollable message reader with message area & thread browsing
- Operates at a slow, relaxed pace
- Uses up a good chunk of memory that would otherwise be sitting empty

Installation:

In 'scfg' (BBS->Configure in Windows), do the following:

- Select "External Programs"
- Select "Online Programs (Doors)"
- Choose an external programs section
- Select "Available online programs"
- Create a new entry as follows:

	Online Program Name: ecReader
	Internal Code: ECREADER
	Start-up Directory: ../xtrn/ecreader
	Command Line: ?ecReader.js
	Multiple Concurrent Users: Yes

All other options can be left at their default values.

Support:

- Post a message to 'echicken' in the 'Synchronet Sysops' sub on DOVE-Net
- Send an email to echicken -at- bbs.electronicchicken.com
- Find me in #synchronet on irc.synchro.net

