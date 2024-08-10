# bullshit

Lightbar bulletin lister/reader for Synchronet BBS 3.16+. Post your bulletins to a message base to add them to the list, or load bulletins from text or ANSI files.

## Setup

### Create a message area

This step is optional. You can skip this part if you don't want to pull bulletins out of a message base.

Launch SCFG (BBS->Configure in the Synchronet Control Panel on Windows.)

In 'Message Areas', select your local message group, and create a new sub with the following details:

```
Long Name                  Bulletins
Short Name                 Bulletins
QWK Name                   BULLSHIT
Internal Code              BULLSHIT
Access Requirements        LEVEL 90
Reading Requirements       LEVEL 90
Posting Requirements       LEVEL 90
```

Toggle Options...

```
Default on for new scan		No
Forced on for new scan		No
Default on for your scan	No
```

_Name this message area whatever you want, and you can use an existing message area if you wish. Ideally only the sysop can read or post to this area, and it won't be included in the new message scan._

### Create an external program entry

SCFG, return to the main menu, select 'External Programs', then 'Online Programs (Doors)', choose the section you wish to add Bullshit to, then create a new entry with the following details:

```
Name                       Bullshit
Internal Code              BULLSHIT
Start-up Directory         /sbbs/xtrn/bullshit
Command Line               ?build/bullshit.js
Multiple Concurrent Users  Yes
```

If you want Bullshit to run during your logon process, set the following:

```
Execute on Event			logon
```

All other options can be left at their default settings.

## Customization

_If you were running a previous version of Bullshit where settings were stored at `xtrn/bullshit/bullshit.ini` you can skip this step; your settings will be migrated automatically._

Create `ctrl/modopts.d/bullshit.ini`. Here's an example. Change `messageBase` to match the internal code of your message base, or omit it if you don't want to use one. You can leave out the `[bullshit:files]` section if you don't have any files to include.

```ini
[bullshit]
messageBase = BULLSHIT
maxMessages = 100
newOnly = false

[bullshit:colors]
title = WHITE
text = LIGHTGRAY
lightBar = BG_CYAN|LIGHTCYAN
border = WHITE,LIGHTCYAN,CYAN,LIGHTBLUE

[bullshit:files]
LORD Scores = /path/to/lordscor.ans
```

## Bullshit for the web

The included '999-bullshit.xjs' file is compatible with webv4. You can add it to your site by copying it to the `webv4/mods/pages` subdirectory, renaming it as	needed. You could also just copy the contents of this file into another of your pages if you wish for bulletins to show up there (000-home.xjs, for	example.)

_I haven't actually tested the latest version of this XJS file, so, uh ... yeah._

## Support

### DOVE-Net

Post a message to `echicken` in the Synchronet Sysops area.

### IRC

Find me in `#synchronet` on `irc.synchro.net`.