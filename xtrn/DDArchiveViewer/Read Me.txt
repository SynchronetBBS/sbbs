                    Digital Distortion Archive Viewer
                              Version 1.03
                        Release date: 2009-12-20

                                  by

                             Eric Oulashin
                     Sysop of Digital Distortion BBS
               BBS internet address: digitaldistortionbbs.com
                                     digdist.bbsindex.com
                     Email: eric.oulashin@gmail.com



This file describes the Digital Distortion Archive Viewer.

Contents
========
1. Disclaimer
2. Introduction
3. Archive File Software
4. Installation and Setup
5. Main configuration file
6. Archive file type configuration file


1. Disclaimer
=============
I can only guarantee that the contents of the Digital Distortion Archive
Viewer script and accompanying files take up space on your computer.  I
have tested this script with my BBS in Windows; it has not been tested
with Linux.  However, it should operate just as well in Linux as in
Windows.


2. Introduction
===============
Digital Distortion Archive Viewer is a script for Synchronet that allows
the user to list files inside of an archive (zip, rar, etc.), view files
inside the archive (including text files and other archive files), and
download files from inside the archive.  The list of files for an archive
can be navigated backward and forward, with either a lightbar interface
or a more traditional interface.

In the file list, it's possible to have filenames that are too long to fit
on the screen.  Filenames that are too long will be truncated, so for very
long filenames, it's normal to not see the entire filename in the list.

The way this script views archive and text files is by executing external
command lines and then displaying the output.  For archive files, both
a VIEW and EXTRACT command line can be specified; for text files, only a
VIEW command line makes sense.  To list the contents of an archive, the
EXTRACT command is executed to extract the archive to a temporary directory
(DDArcViewerTemp in the current node directory), and then the files are
listed.  Files are listed in alphabetical order.  The user can then navigate
the list and view/download any of the files.  If the temporary directory
can't be created for some reason, the script will fall back to the VIEW
command to view the contents of the archive.  In that case, the archive file
contents will simply be displayed to the user, without the abilities to
navigate the list and view/download files inside the archive.

For text files, the VIEW command will be used to display its contents to
the user.

Multiple archive file formats and text file formats can be specified and
configured via a configuration file.  This way, sysops can add more file
formats and configure them as needed for their system.

Additionally, a separate configuration file specifies behavior options
and colors for the script, including the interface to use (lightbar or
traditional), input timeout, and colors for the file list.

As mentioned already, users can download files from within an archive.
Files within an archive can be downoaded individually from within the
archive viewer, but the files cannot be added to the user's download
queue.  The reason is that Synchronet won't add files to the user's
download queue unless the file is in Synchronet's file database.


3. Archive File Software
========================
Digital Distortion Archive Viewer comes with configurations to handle
ZIP, 7Z (7-Zip), RAR, ARJ, TAR, GZ, TGZ, and TAR.GZ archives, and many
text file formats.

The file format configuration file included with this script includes
VIEW and EXTRACT command lines for various archivers for both Windows
and Linux.  In order for this script to work properly, you will need
to make sure you have the appropriate archiver software installed on
your system, and you will need to edit the DDArcViewerFileTypes.cfg
file (using a text editor) and make sure you have the VIEW and
EXTRACT command lines set properly for your archiver software.
The included DDArcViewerFileTypes.cfg file has command lines for
Windows and Linux, and the Linux command lines are commented out.
For information on that configuration file, see section 6: Archive
file type configuration file.

The following archive contains Win32 command-line archivers for popular
archive file formats:
http://digdist.bbsindex.com/miscFilesForDL/Win32CmdLineCompressionTools.zip
The archivers included in that archive handle the most popular file formats
(ZIP, 7Z (7-Zip), RAR, ARJ, TAR, GZ, TGZ, and TAR.GZ), and they are set up in
DDArcViewerFileTypes.cfg.  If your BBS is running in Windows, the included
configuration file should work for you - But note that you will need to edit
the DDArcViewerFileTypes.cfg file and change the paths to the .exe files
according to where you copied them to your system.  If you are running Linux,
that .cfg file also has the Linux command lines as comments).

If you copy the archivers to a directory that is not in your system path, you
will need to edit the DDArcViewerFileTypes.cfg file to include the full paths
with the archive executables.

The configuration file DDArcViewerFileTypes.cfg includes a setup
for using 7-Zip to extract ISO (CD/DVD image) files; however, in
testing, it seemed that 7-Zip can only extract or see one file in
an ISO image.

For Linux, the following is a list of Linux archivers that this
script is configured for and how you can acquire them if you don't
have them already:
------------------
ZIP, 7Z, GZ, TGZ, TAR:
- Install p7zip using your distro's package manager, or download it via
the web.  A download page is available here:
http://www.7-zip.org/download.html

RAR:
- Install unrar using your distro's package manager, or download it via
the web.  Or, download RARLab's version:
http://www.rarlab.com/download.htm

ARJ:
- Source code for open-Source ARJ (and instructions) are available here:
http://linux.softpedia.com/get/System/Archiving/Arj-12097.shtml
This is the download link from that page:
http://linux.softpedia.com/progDownload/Arj-Download-12097.html
Download the source, and follow the page's instructions to build that on
your Linux system.  After compiling, the executable file will be located
in linux-gnu/en/rs/arj .  Place the executable file (arj) in a directory
in your path (i.e., /usr/local/bin or /usr/bin).
Instructions for building the ARJ source code, from the above web page,
are as follows:
cd gnu;autoconf;./configure;cd ..;make prepare;make

Notes:

1. GNU make must be used (on FreeBSD systems it's called "gmake").
2. You have to run autoconf prior to running configure - as there will be different configure scripts for UNIX-like systems and OS/2 EMX.
3. On OS/2 EMX, autoconf v 2.57 is confirmed to work.
4. You can finalize the build process with "make install" (to perform a local installation) or "make package" (to create a self-extracting distribution kit).

ARJ is a CPU-intensive program. If you wish to configure ARJ for a higher performance on a specific CPU, try the following, assuming that you have a newer version of GCC, e.g. 3.2.1:

./configure CFLAGS="-march=i386 -mcpu=athlon-xp"

where "-mcpu" designates the type of your CPU, run "man gcc" for a list of CPU types.


4. Installation and Setup
=========================
Step 1: Copy the script files, configuration files, & archivers to your system
------------------------------------------------------------------------------
Digital Distortion Archive Viewer consists of the following 4 files, which
you will need to place in your sbbs/exec directory (or another directory of
your choice):
 1. DDArcViewer.js
 2. DDArcViewerCleanup.js
 3. DDArcViewer.cfg
 4. DDArcViewerFileTypes.cfg
These files must be all together in the same directory.


For sysops running their BBS in Windows, the following archiver programs
in the Win32Archivers directory will need to be placed in a directory
(preferably a directory that's included in the system's path):
 1. 7za.exe
 2. ARJ32.EXE
 3. Rar.exe
 4. unzip.exe

For sysops running the Linux version of Synchronet, you will need to acquire
the appropriate archivers as described in the previous section.

Step 2: Edit the configuration files
------------------------------------
At a minimum, you will need to edit DDArcViewerFileTypes.cfg to make sure
that the archive command lines are correct for your system and that the text
view commands are correct.  If you're running your BBS in Linux, you will
first need to comment the Windows command lines and uncomment the Linux
command lines.  See section 6: Archive file type configuration file.

If desired, you can also edit DDArcViewer.cfg to change script options
such as the interface type (lightbar/traditional) and colors.  See section
5: Main configuration file.

Step 3: Set up the command lines to view archive files in Synchronet's
configuration program (SCFG)
----------------------------------------------------------------------
1. Run Synchronet's configuration program (SCFG)
2. From the main menu, choose "File Options".
3. From the menu that appears, choose "Viewable Files..."
4. For each archive format you want users to be able to view, you will need an
entry in this menu.  Some archives (i.e. ZIP) might already be in there.  In that
case, you can simply scroll to it and press Enter to select it.  If an archive
format is not in the list, you can press INS to insert an entry.  If you press
INS, the line below (or above) will be copied and inserted as a new line.
When editing a viewable file type, you will need to enter the file extension
and the command line used to view it.  The file extension is fairly
self-explanatory.  For the command line, use the following (this assumes
DDArcViewer.js was copied to your sbbs\exec diretory):
?DDArcViewer.js %s
If you copied DDArcViewer.js and its other files to another directory, you
will need to provide the path to DDArcViewer.js (i.e.,
?/sbbs/ArcViewer/DDArcViewer.js %s).  As an example, your Viewable File Type
window should look similar to the following:
       +[¦][?]--------------------------------------+
       ¦             Viewable File Type             ¦
       ¦--------------------------------------------¦
       ¦ ¦File Extension        ZIP                 ¦
       ¦ ¦Command Line          ?DDArcViewer.js %f  ¦
       ¦ ¦Access Requirements                       ¦
       +--------------------------------------------+



Step 4: Create/update your logout script to handle temporary directory cleanup
------------------------------------------------------------------------------
If you have not already done so, you will need to create a logout script for your
BBS.  This is important, because when a user logs off or disconnects, you will
need to have the archive viewer's temporary directory removed so that it does not
interfere with the next user's use of the archive viewer.

If you are not already using a logout script, follow these steps:
1. Create a file in your sbbs\exec directory called logout.js
2. Add this line to it (assuming that the archive viewer scripts are in sbbs\exec):
load("DDArcViewerCleanup.js");
3. Add your logout script to Synchronet's configuration:
   A. Run Synchronet's configuration program (SCFG)
   B. From the main menu, choose "System".
   C. From that menu, choose "Loadable Modules..."
   D. Arrow down to highlight "Logout Event", and press Enter.  When it
      prompts you, type in "logout" (without the quotes) and press Enter.
   E. Escape back to the main menu, saving changes when it asks you to do
      so.  Then, exit out of SCFG.

If you already have a logout script (using JavaScript), you just need to add the
following line to it:
load("DDArcViewerCleanup.js");


Step 5: If you use a JavaScript command shell, update it
--------------------------------------------------------
When using a JavaSCript command shell, I have noticed that when Synchronet
executes some Baja scripts, sometimes there is text leftover in the command
string, causing your command shell to pick up extra commands that that user
did not type.  That can happen when using the Baja module included with this
archive; therefore, if you use a JavaScript command shell, you should update
it to perform this action before inputting a command from the user:
bbs.command_str = "";
That blanks out the command string, preventing extra leftover characters
from triggering actions in your command shell.


5. Main configuration file
==========================
If you want to change the default beavior and colors, you can edit the
DDArcViewer.cfg file, which is a plain text file.  The configuration file
has two sections: A behavior section (denoted by [BEHAVIOR]) and a colors
section (denoted by [COLORS]).  For each setting or color, the syntax is as
folows:

setting=value

where "setting" is the behavior setting or color, and "value" is the corresponding
value for the setting/color.  The colors are Synchronet color codes.

Also, comments are allowed in the configuration file.  Comments begin with a
semicolon (;).

Behavior section
----------------
Setting                               Description
-------                               -----------
interfaceStyle                        String: The user interface to use.  Valid values
                                      are "Traditional" (traditional user interface
                                      with user prompted for input at the end of each
                                      screenful) and "Lightbar" (use the lightbar user
                                      interface).

inputTimeoutMS                        The user input timeout, in milliseconds.
                                      In the default configuration file, this is set at
                                      300000 (5 minutes).

maxArcFileSize                        Maximum archive file size.  By default, this value
                                      is in bytes; however, kilobytes, megabytes, or
                                      gigabytes can be specified by putting one of the
                                      following characters at the end:
                                       K: Kilobytes
                                       M: Megabytes
                                       G: Gigabytes
                                      For example, 1000 would mean 1000 bytes, but
                                      700M would mean 700 megabytes.
                                      If this is 0, then no maximum file size will
                                      be used.

maxTextFileSize                       Maximum text file size.  By default, this value
                                      is in bytes; however, kilobytes, megabytes, or
                                      gigabytes can be specified by putting one of the
                                      following characters at the end:
                                       K: Kilobytes
                                       M: Megabytes
                                       G: Gigabytes
                                      For example, 1000 would mean 1000 bytes, but
                                      5M would mean 5 megabytes.
                                      If this is 0, then no maximum file size will
                                      be used.

Colors section
--------------
Color setting                        Used for
-------------                        --------
archiveFilenameHdrText               The text displayed before the filename above the
                                     list.

archiveFilename                      The archive filename


headerLine                           The column headers above the file list

headerSeparatorLine                  The line between the header line and the file list

fileNums                             The file numbers

fileSize                             The file size

fileDate                             The file date

fileTime                             The file time

filename                             The filename

subdir                               Subdirectories

highlightedFile                      Selected files in lightbar mode



6. Archive file type configuration file
=======================================
The configuration file DDArcViewerFileTypes.cfg defines the files that
you want to be viewable by the archive viewer.  Viewable file types are
specified by their extension in square brackets.  Each archive file type
must have a view command and an extract command.  Text files must have only
a view command and an IsText=Yes setting.  The general format for each file
type is as follows:

[EXTENSION]
VIEW=command
EXTRACT=command
IsText=No     (or Yes)

By default, the IsText option is No, so IsText can be left out for archive
files.  VIEW and EXTRACT specify command lines for viewing and extracting the
file, respectively.  The EXTRACT setting can be left out for text files, since
text files will not be extracted.  Archive files require both the VIEW and
EXTRACT settings.  As an example, the following settings can be used for zip
files and for txt files in Windows:

[ZIP]
VIEW=unzip.exe -l %FILENAME%
EXTRACT=unzip.exe -qq -o %FILENAME% %FILESPEC% -d %TO_DIR%

[TXT]
IsText=Yes
VIEW=type %FILENAME%

Note that for each command, the following pseudonyms are used:
%FILENAME% : The name of the archive file or text file.
%FILESPEC% : This would specify the file(s) to be extracted from the archive. 
             The script actually will totally remove this from the command; it
             is not used.  It's currently here for possible future use.
%TO_DIR%   : The directory to which the archive file will be extracted.

Using the above example configuration for zip files, if the user (on node 1)
wants to view D:\Files\someArchive.zip, and the temp directory is
D:\sbbs\node1\DDArcViewerTemp, the extract command will be translated to the
following:
unzip.exe -qq -o D:\Files\someArchive.zip -d D:\sbbs\node1\DDArcViewerTemp