                        Digital Distortion File Lister
                                 Version 2.02
                           Release date: 2022-02-13

                                     by

                                Eric Oulashin
                          Sysop of Digital Distortion
                  BBS internet address: digitaldistortionbbs.com
                     Alternate address: digdist.bbsindex.com
                        Email: eric.oulashin@gmail.com



This file describes the Digital Distortion File Lister.

Contents
========
1. Disclaimer
2. Introduction
3. Installation & Setup
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
This release is version 2.00 because I had previously released a message lister
mod for Synchronet which was just a list header and a command bar to display
under the list, and it still used Synchronet's stock file list.  Now that
Synchronet provides a JavaScript interface to its filebases (as of version
3.19), more customization is possible with JavaScript.

Digital Distortion File Lister is a script for Synchronet that provides an
enhanced user interface (for ANSI terminals) for listing files in the user's
current file directory.  This file lister uses a lightbar interface to list the
files, a 'command bar' at the bottom of the screen allowing the user to use the
left & right arrow keys or the first character of the action to select an
action.  The file lister also uses message boxes to display information.

If the user's terminal does not support ANSI, the file lister will run the
stock Synchronet file lister interface instead.

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

Command shell setup
-------------------
Running the file lister involves simply having the BBS run a command to run
ddfilelister.js.  No command-line parameters are required.  The command to run
ddfilelister.js can be placed in a command shell for a command key, or in
SCFG > External Programs > Online Programs (Doors) in one of your program
groups, and then you could have your command shell run the lister via the
internal program code you configured in SCFG.

Installing into a command shell is described in the subsection "Installing
into a command shell".

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
or sbbs/ctrl directory so that they'll be more difficutl to accidentally
override if you update your xtrn/DDMsgReader from the Synchronet CVS
repository, where this reader's files are checked in.

The configuration settings are described in the sections below:

Main configuration file (DDMsgReader.cfg)
-----------------------------------------
Setting                               Description
-------                               -----------
sortOrder                             String: The file sort order to use.
                                      Valid values are:
                                      NATURAL: Natural sort order (same as DATE_A)
                                      NAME_AI: Filename ascending, case insensitive sort order
                                      NAME_DI: Filename descending, case insensitive sort order
                                      NAME_AS: Filename ascending, case sensitive sort order
                                      NAME_DS: Filename descending, case sensitive sort order
                                      DATE_A: Import date/time ascending sort order
                                      DATE_D: Import date/time descending sort order


pauseAfterViewingFile                 Whether or not to pause after viewing a
                                      file

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
                                     (selected) file menu itme

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

fileAreaMenuHighlightBkg              The file area entry background color for
                                      'highlight' colors (for moving a file)

fileAreaNumHighlight                  The file library/directory number for
                                      'highlight' colors (for moving a file)

fileAreaDescHighlight                 The file library/directory description for
                                      'highlight' colors (for moving a file)

fileAreaNumItemsHighlight            The number of directories/files for
                                     'highlight' colors (for moving a file)


5. Strings used from text.dat
=============================
Digital Distortion File Lister uses the following strings from text.dat (in
Synchronet's ctrl directory):

- DirLibOrAll (622)
- FileSpecStarDotStar (199)
- SearchStringPrompt (76)
