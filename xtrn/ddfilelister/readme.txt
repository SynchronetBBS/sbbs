                        Digital Distortion File Lister
                                 Version 2.28
                           Release date: 2025-02-22

                                     by

                                Eric Oulashin
                          Sysop of Digital Distortion
                  BBS internet address: digitaldistortionbbs.com
                      Alternate address: digdist.synchro.net
                        Email: eric.oulashin@gmail.com



This file describes the Digital Distortion File Lister.

Contents
========
1. Disclaimer
2. Introduction
3. Installation & Setup
   - Loadable Module setup
   - Command shell setup
   - Background: Running JavaScript scripts in Synchronet
4. Configuration file & color/text theme configuration file
5. Strings used from text.dat


1. Disclaimer
=============
I cannot guarantee that this script is 100% free of bugs.  However, I have
tested it, and I used it often during development, so I have some confidence
that there are no serious issues with it (at least, none that I have seen).


2. Introduction
===============
This release is version 2.## because I had previously released a message lister
mod (version 1.##) for Synchronet which was just a list header and a command bar
to display under the list, and it still used Synchronet's stock file list.  Now
that Synchronet provides a JavaScript interface to its filebases (as of version
3.19), more customization is possible with JavaScript.

Digital Distortion File Lister is a script for Synchronet that provides an
enhanced user interface (for ANSI terminals) for listing files in the user's
current file directory.  This file lister uses a lightbar interface to list the
files, a 'command bar' at the bottom of the screen allowing the user to use the
left & right arrow keys or the first character of the action to select an
action.  The file lister also uses message boxes to display information.

If the user's terminal does not support ANSI, the file lister will run the
stock Synchronet file lister interface instead.

Digital Distortion File Lister makes use of the user's extended description
setting.  If the user's extended description setting is enabled, the lister
will use a split interface, with the lightbar file list on the left side and
the extended file description for the highlighted file displayed on the right
side.  If the user's extended file description setting is disabled, the
lightbar file menu will use the entire width of the screen, with the short
file descriptions being displayed in a single row with each file.

In order to display extended descriptions, however, this file lister currently
requires ANSI support (for cursor movements & such) and a terminal width of at
least 80 characters in order to display everything, since the extended
descriptions will be displayed to the right of the menu. If the user's terminal
doesn't meet these requirements, short descriptions will be displayed.

The user can toggle extended descriptions on/off from within this file lister
using the X key. However, if the user's terminal doesn't meet the above
requiremtnts for extended description mode, the user won't be able to toggle
extended descriptions from within this file lister.

If a filename is too long to be fully displayed in the menu item, the full file
description will be displayed above the description (wrapped to the description
area width) if extended descriptions are enabled.

When adding files to the user's batch download queue or (for the sysop)
selecting files to move or delete, multi-select mode can be used, allowing
the user to select multiple files using the spacebar.  If the spacebar is not
used, the file at the current highlighted lightbar location will be used.

For viewing file information or viewining file contents, only the current file
where the lightbar location will be used.

For the lightbar file list, basic descriptions and short filenames are used in
order for the information for each file to fit on a single line in the list.
The user can view extended information for each file, in which case a window
will be displayed with the filename, size, timestamp, and extended description.
The file lister also provides the ability to view files (according to
Synchronet's viewable files configuration), and adding files to the user's
batch download queue.  Additionally, sysops can delete files and move files to
another file directory.

In addition to listing files in the user's current directory, this lister can
perform a file search (via filespec, description, or new file search since last
search).  The default is to list files in the current directory, but a search
mode can be specified with the command-line option -MODE.  search_filename,
search_description, or new_file_search will perform the searching; list_curdir
lists files in the user's current directory, which is the default.


3. Installation & Setup
=======================
Aside from readme.txt revision_history.txt, Digital Distortion File Lister is
comprised of the following files:

1. ddfilelister.js        The Digital Distortion File Lister script

2. ddfilelister.cfg       The file lister configuration file

3. defaultTheme.cfg       The default theme file containing colors used in the
                          file lister

The configuration files are plain text files, so they can be edited using any
editor.

The .js script and .cfg files can be placed together in any directory.  When
the lister reads the configuration file & theme file, the lister will first
look in your sbbs/mods directory, then sbbs/ctrl, then in the same directory
where the .js script is located.  So, if you desire, you could place
ddfilelister.js in sbbs/exec and the .cfg file sin your sbbs/ctrl directory,
for example.

One way to install ddfilelister.js is to run it directly from your command
shell.  Also, as of Synchronet 3.20 (built on February 25, 2023 and newer),
there are a couple of loadable module options in SCFG for file operations;
ddfilelister.js can be used as a loadable module for List Files.
The advantage of having ddfilelister.js set up as a loadable module is that
ddfilelister.js will be used from standard command shells and using the standard
file list/search commands/functions.

Loadable Module setup
---------------------
Note: This only works in Synchronet 3.20 (built from February 25, 2020) and
newer.

As a loadable module, ddfilelister.js works for the List Files option. It's best
NOT to use ddfilelister for the Scan Dirs option.  These options are available
in SCFG > System > Loadable Modules.
If you have ddfilelister.js in your mods directory or other standard directory,
you can specify the settings as follows:
  List Files          ddfilelister.js

If you keep ddfilelister.js in sbbs/xtrn/ddfilelister, you can specify it as
follows:
  List Files          ../xtrn/ddfilelister/ddfilelister.js


Command shell setup
-------------------
To running the file lister in a command shell, you simply need to add a command
to your command shell run ddfilelister.js.  To list the files in the user's
current file directory, no command-line parameters are required. The command to
run ddfilelister.js can be placed in a command shell for a command key, or in
SCFG > External Programs > Online Programs (Doors) in one of your program
groups, and then you could have your command shell run the lister via the
internal program code you configured in SCFG.


Background: Running JavaScript scripts in Synchronet
----------------------------------------------------
The command line to run this script would be as follows (if the script is in
your sbbs/xtrn/ddfilelister directory):
?../xtrn/ddfilelister/ddfilelister.js

In a Baja script, you can use the 'exec' command to run a JavaScript script, as
in the following example:
exec "?../xtrn/ddfilelister/ddfilelister.js"

In a JavaScript script, you can use the bbs.exec() function to run a JavaScript
script, as in the following example:
bbs.exec("?../xtrn/ddfilelister/ddfilelister.js");

To perform searching, you can add the -MODE option on the command line.
To do a filename search:
?../xtrn/DigDist/ddfilelister/ddfilelister.js -MODE=search_filename
To do a description search:
?../xtrn/DigDist/ddfilelister/ddfilelister.js -MODE=search_description
To search for new files since the last search:
?../xtrn/DigDist/ddfilelister/ddfilelister.js -MODE=new_file_search
You can also specify a mode to list the user's current directory, which is
already the default action:
?../xtrn/DigDist/ddfilelister/ddfilelister.js -MODE=list_curdir


To install the file lister as an external program (in SCFG in External
Programs > Online Programs (Doors)), see the following document for more
information:
http://wiki.synchro.net/howto:door:index?s[]=doors


4. Configuration file & color/text theme configuration file
===========================================================
Digital Distortion File Lister allows changing some settings and colors via
configuration files.  The configuration files are plain text and can be edited
with any text editor.  These are the configuration files used by Digital
Distortion File Lister:
- ddfilelister.cfg: The main configuration file
- defaultTheme.cfg: The default theme configuration file which defines colors
  for items displayed in the file lister.  The name of this file can be
  specified in ddfilelister.cfg, so that alternate "theme" configuration files
  can be used if desired.

Each setting in the configuration files has the format setting=value, where
"setting" is the name of the setting or color, and "value" is the corresponding
value to use.  The colors specified in the theme configuration file are
Synchronet color/attribute codes.  Comments are allowed in the configuration
files - Commented lines begin with a semicolon (;).

Digital Distortion File Lister will look for the configuration files in the
following directories, in the following order:
1. sbbs/mods
2. sbbs/ctrl
3. The same directory as ddfilelister.js
If you customize your configuration files, you can copy them to your sbbs/mods
or sbbs/ctrl directory so that they'll be more difficult to accidentally
override if you update your xtrn/DDMsgReader from the Synchronet Git
repository, where this reader's files are checked in.

The configuration settings are described in the sections below:

Main configuration file (DDMsgReader.cfg)
-----------------------------------------
Setting                               Description
-------                               -----------
interfaceStyle                        The user interface style to use: lightbar
                                      or traditional. Lightbar requires ANSI
                                      support in the user's terminal; if the
                                      user's terminal doesn't support ANSI,
                                      ddfilelister will fall back to a
                                      traditional user interface.

traditionalUseSyncStock               If using the traditional user interface,
                                      whether or not to use Synchronet's stock
                                      file lister instead of ddfilelister. Valid
                                      values are true and false. If true,
                                      ddfilelister won't be used at all, and
                                      instead, Synchronet's stock file lister
                                      will be used. If false and interfaceStyle
                                      is traditional, ddfilelister's traditional
                                      user interface will be used.

sortOrder                             String: The file sort order to use.
                                      Valid values are:
                                      PER_DIR_CFG: According to the directory
                                      configuration in SCFG > File Areas > library > File Directories > dir > Advanced Options > Sort Value and Direction
                                      NATURAL: Natural sort order (same as DATE_A)
                                      NAME_AI: Filename ascending, case insensitive sort order
                                      NAME_DI: Filename descending, case insensitive sort order
                                      NAME_AS: Filename ascending, case sensitive sort order
                                      NAME_DS: Filename descending, case sensitive sort order
                                      DATE_A: Import date/time ascending sort order
                                      DATE_D: Import date/time descending sort order


pauseAfterViewingFile                 Whether or not to pause after viewing a
                                      file
									  
blankNFilesListedStrIfLoadableModule  When used as a loadable module, whether or
                                      not to blank out the "# Files Listed"
                                      string (from text.dat) so that Synchronet
                                      won't display it after the lister exits

displayUserAvatars                    Whether or not to display uploader avatars
                                      in extended information for files. Valid
                                      values are true and false.

useFilenameIfNoDescription            If a file's description is unavailable,
                                      whether or not to use the filename in the
                                      list instead. Valid values are true and
                                      false.

displayNumFilesInHeader               Whether or not to display the number of
                                      files in the directory in the header at
                                      the top of the list

themeFilename                         The name of the configuration file to
                                      use for colors & string settings

Theme configuration file
------------------------
The convention for the setting names in the theme configuration file is that
setting names ending in 'Text' are for whole text strings, and the setting
names that don't end in 'Text' are for colors.

Setting                              Element in the file lister
-------                              --------------------------
filename                             Filename in the file list

fileSize                             File size in the file list

desc                                 Description in the file list

bkgHighlight                         Background color for the highlighted
                                     (selected) file menu item

filenameHighlight                    Highlight filename color for the file list

fileSizeHighlight                    Highlight file size color for the file
                                     list

descHighlight                        Highlight description color for the file
                                     list

fileTimestamp                        File timestamp color for showing an
                                     extended file description

fileInfoWindowBorder                 For the extended file information box
                                     border

fileInfoWindowTitle                  For the title of the extended file
                                     information box

errorBoxBorder                       Error box border

errorMessage                         Error message color

successMessage                       Success message color

batchDLInfoWindowBorder              Batch download confirm/info window border
                                     color

batchDLInfoWindowTitle               Batch download info window title color

confirmFileActionWindowBorder        Multi-file action confirm window border
                                     color

confirmFileActionWindowWindowTitle   Multi-file action confirm window title
                                     color

fileAreaMenuBorder                   The color of the file area menu border (for
                                     moving a file)

fileNormalBkg                        The file area entry background color for
                                     'normal' colors (for moving a file)

fileAreaNum                          The file library/directory number for
                                     'normal' colors (for moving a file)

fileAreaDesc                         The file library/directory description for
                                     'normal' colors (for moving a file)

fileAreaNumItems                     The number of directories/files for
                                     'normal' colors (for moving a file)

fileAreaMenuHighlightBkg             The file area entry background color for
                                     'highlight' colors (for moving a file)

fileAreaNumHighlight                 The file library/directory number for
                                     'highlight' colors (for moving a file)

fileAreaDescHighlight                The file library/directory description for
                                     'highlight' colors (for moving a file)

fileAreaNumItemsHighlight            The number of directories/files for
                                     'highlight' colors (for moving a file)

Color settings for the traditional (non-lightbar) user interface:

fileAreaMenuBorderTrad               The color of the file area menu border (for
                                     moving a file)

fileNormalBkgTrad                    The file area entry background color for
                                     'normal' colors (for moving a file)

listNumTrad                          The color for file lists in the traditional
                                     interface

fileAreaDescTrad                     The file library/directory description for
                                     'normal' colors (for moving a file)

fileAreaNumItemsTrad                 The number of directories/files for
                                     'normal' colors (for moving a file)

filenameInDesc                       The filename when used in the description
                                     (for instance, if the filename is too long
                                     to fully fit in the lightbar menu or if
                                     the file has no description)


5. Strings used from text.dat
=============================
Digital Distortion File Lister uses the following strings from text.dat (in
Synchronet's ctrl directory):

- DirLibOrAll (622)
- FileSpecStarDotStar (199)
- SearchStringPrompt (76)
- NFilesListed (168): This string will be blanked if the file lister is used as
  a loadable module and the blankNFilesListedStrIfLoadableModule configuration
  setting is true
- EditDescription (254)
- TagFilePrompt (298)
- EditUploader (256)
- EditCreditValue (257)
- EditTimesDownloaded (258)
- EditExtDescriptionQ (259)
- NScanDate (400)
- NScanYear (401)
- NScanMonth (402)
- NScanDay (403)
- NScanHour (404)
- NScanMinute (405)
- NScanPmQ (406)
- FileInfoPrompt (232)
- FileAlreadyInQueue (303)
