                   Digital Distortion Upload Processor
                              Version 1.08
                        Release date: 2026-02-02

                                  by

                             Eric Oulashin
                     Sysop of Digital Distortion BBS
                 BBS internet address: digdist.synchro.net
                       AKA digitaldistortionbbs.com
                     Email: eric.oulashin@gmail.com



This file describes the Digital Distortion Upload Processor.

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
The only guarantee that I can make about Digital Distortion Upload Processor
is that it will take up space on your computer.  I have tested this with
the Windows verison of Synchronet and with the Windows version of AVG Free
(virus scanner) on my BBS, running in Windows 2000; this script has not
been tested with Linux Synchronet platforms.  I created this script because
I felt that it would be useful and am providing it to the Synchronet BBS
community in case other Synchronet sysops might find it useful.


2. Introduction
===============
Digital Distortion Upload Processor is a script makes use of a virus scanner
to scan uploaded files, with the added ability to extract compressed files
in order to scan the files inside the compressed file.

File formats can be specified and configured via a configuration file,
including extraction commands for compressed files. Synchronet's built-in
archive support (added in Synchronet 3.19) will be used, if available, to
extract archive files. Otherwise, extract commands for archive file types can
be configured in the archive file type configuration file (ddup_file_types.cfg).
In addition, the virus scan command can be configured in the main configuration
file, which should allow for the use of any virus scanner, as long as it is a
command-line scanner (no GUI) and is able to take a subdirectory as a
command-line parameter.

Compressed (archive) files will be extracted to a temporary directory in the
node directory after they are uploaded.  Furthermore, compressed files
within the compressed file will be extracted to subdirectories within that
subdirectory, and any compressed files inside those compressed files will
be extracted, etc..  This way, all files inside of the archive can be
scanned by the virus scanner.

The temporary directory created in the node directory has the following
name:
DDUploadProcessor_Temp
Additionally, the following temporary files are created in the node
directory:
DDUPCommandOutput_temp.txt
DDUP_ScanCmd.bat (for Win32 systems) or DDUP_ScanCmd.sh (for *nix systems)

The temporary files and temporary directory will be removed when the
script finishes; however, in cases where they aren't removed (i.e., if the
user disconnects during the scan), a cleanup script is also included, which
can be executed in your logon and logoff scripts to ensure  that the node
directory does not contain the temporary files.

Detection of viruses will reject the uploaded file.  Also, failure to
extract an archive (and thus, inability to scan for viruses) will cause the
uploaded file to be rejected.


3. Archive File Software
========================
Digital Distortion Upload Processor will (try to) use Synchronet's built-in
archive support (if available) to extract archive files. If that fails, it will
fall back on a configured command-line to extract the archive (if configured).
Synchronet's built-in archive support will take precedence over any configured
extraction command (that is, if Synchronet itself is able to extract the
archive, any configured extraction command for that archive file type will not
be uesd).

Configuration settings to use various archivers to handle extraction of ZIP, 7Z
(7-Zip), RAR, ARJ, MSI, TAR, GZ, TGZ, and TAR.GZ archives.  If you want to use
other archiver programs, you will need to update the configuration to modify the
commands used.

The file format configuration file included with this script includes
extraction command lines (specified by an EXTRACT setting) for various
archivers for both Windows and Linux.  In order for this script to work
properly, you will need to make sure you have the appropriate archiver
software installed on your system, and you will need to edit the
ddup_file_types.cfg file (using a text editor) and make sure you have
EXTRACT command lines set properly for your system.  For information on
that configuration file, see section 6: Archive file type configuration
file.

Extractor notes:
ddup_file_types.cfg includes a setup for using 7-Zip to extract ISO (CD/DVD
image) files; however, in testing, it seemed that 7-Zip can only extract
or see one file in an ISO image.
ddup_file_types.cfg also includes a setup for extracting MSI (Microsoft
Installer) files.

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
Step 1: Install a virus scanner
-------------------------------
You will need to download and install a virus scanner on your BBS system.
The one that I set up this script for is AVG Free version 9 for Windows,
which is available at the following web page:
http://free.avg.com/us-en/download?prd=afg

Step 2: Install archiver programs for any archive formats you want to support
-----------------------------------------------------------------------------
You will need to acquire the appropriate archiver programs for any archive
formats you want to use.  These will need to be command-line archiver tools.
Command-line archiver tools are available for many archive formats. For Windows,
you can download them as needed, and for Linux, they may be available via your
package manager or as source that you can download and build for your system.

Step 3: Copy the script files, configuration files, & archivers to your system
------------------------------------------------------------------------------
Digital Distortion Upload Processor consists of the following files, which
you will need to place in your sbbs/exec directory (or another directory of
your choice):
 1. ddup.js
 2. ddup.cfg
 3. ddup_cleanup.js
 4. ddup_file_types.cfg


Step 4: Edit the configuration files
------------------------------------
You will need to edit ddup_file_types.cfg to make sure that the EXTRACT
command lines are correct for your system.  If you're running your BBS
in Linux, you will first need to comment the Windows command lines and
uncomment the Linux command lines.  See section 6: Archive file type
configuration file.

You will also need to edit ddup.cfg and change the scanCmd option, which
specifies the command line to use to scan files for viruses.  In this
command line, the text string %FILESPEC% will be replaced with the name
of the file or directory to be scanned; thus, the virus scanner you use
should be able to take the name of an individual file or a directory as
a parameter, and it should also be a command-line scanner (capable of
taking command-line parameters).

Special note about the scanner command line
-------------------------------------------
It should be noted that the scanner command line specified in ddup.cfg
will be written to a temporary batch file (on Win32 systems) or a shell
script (on *nix systems), which is then run in order to scan the file(s).
The reason for this is that if there are any spaces in the file or
directory names used in the scanner command, the command line doesn't seem
to be passed to the operating system correctly by Synchronet's JavaScript
object model.


Step 5: Set up Digital Distortion Upload Processor for Testable Files
in Synchronet's configuration program (SCFG)
----------------------------------------------------------------------
1. Run Synchronet's configuration program (SCFG)
2. From the main menu, choose "File Options".
3. From the menu that appears, choose "Testable Files..."
4. For each file type that you want to be able to test, you will need
an entry in this table.  Some archive file types (i.e., ZIP) might
already be in there.  In that case, simply scroll down to it and press
Enter to select it.  If the desired file type is not in the list, then
press the INS key to insert an entry or scroll down to the first blank
line and press Enter to insert an entry there, then press Enter to
select and edit it.
The Command Line setting for the file type should look similar to
this (assuming the upload processor files were copied to your
sbbs\exec directory):
?ddup.js %f
If you copied the upload processor files to another directory, you
will need to provide the path to ddup.js; for example:
?/sbbs/dd_upload_processor/ddup.js %f
As an example, your Testable File Type window should look similar
to the following:
+[X][?]--------------------------------------------------------+
|                      Testable File Type                      |
|--------------------------------------------------------------|
| |File Extension        ZIP                                   |
| |Command Line          ?ddup.js %f                           |
| |Working String        Scanning archive file for viruses...  |
| |Access Requirements                                         |
+--------------------------------------------------------------+


Step 6: Create/update your logout and scripts to handle temporary file cleanup
------------------------------------------------------------------------------
If you have not already done so, you will need to create logout and login
scripts for your BBS; there, you will want to load ddup_cleanup.js - That will
help to ensure that temporary files created by the upload processor will be
removed to prevent extra space on your hard drive being wasted.

If you are not already using a logout script, follow these steps:
1. Create a file in your sbbs\exec directory called logout.js (or another name
of your choosing)
2. Add this line to it (assuming that the upload processor scripts are in
sbbs\exec):
load("ddup_cleanup.js");
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
load("ddup_cleanup.js");

It is recommended that you do the same thing with your logout script.


5. Main configuration file
==========================
ddup.example.cfg is an example configuration file for the upload processor.
This file can be edited with a text editor. In order to to customize your
configuration, copy ddup.example.cfg to ddup.cfg and edit ddup.cfg. ddup.cfg
can be in the same directory, xtrn/dd_upload_processor, in sbbs/mods, or in
sbbs/ctrl.

The syntax for each setting is as follows:
setting=value
where "setting" is the setting name, "value" is the corresponding value for
the setting.
Also, comments are allowed in the configuration file.  Comments begin with a
semicolon (;).

The following are the settings used in this configuration file:
Setting                               Description
-------                               -----------
scanCmd                               The command line to use for the virus scanner.
                                      In this command line, the text string
                                      %FILESPEC% will be replaced with the
                                      file/directory to be scanned.

pauseAtEnd                            Specifies whether or not to pause for user
                                      input when the scan is done.  Valid values
                                      are yes and no.

skipScanIfSysop                       Specifies whether or not to skip scanning
                                      for the sysop(s).  Valid values are yes
                                      and no.



6. Archive file type configuration file
=======================================
The configuration file ddup_file_types.example.cfg is an example configuration
file that defines options for various file types, including the extract command
(for archive files) and whether or not you want the upload processor to scan it.
File types are specified by their filename extension in square brackets.
Extractable files must have an EXTRACT option, which specifies the command line
for extracting the file.

Another option that can be specified in this file is scanOption, which
specifies whether or not you want the upload processor to scan the file
(or files in the archive) with the virus scanner.

Similarly to ddup.example.cfg, in order to customize settings in this file,
copy it to ddup_file_types.cfg (in the same directory, xtrn/dd_upload_processor,
in sbbs/mods, or in sbbs/ctrl) and edit ddup_file_types.cfg with a text editor.

The general format for each file type is as follows:

[EXTENSION]
EXTRACT=command
scanOption=scan (or always pass, or always fail)

By default, the scanOption setting will be "scan", and by default, the extract
command is blank (not set).

The valid values for the scanOption setting are as follows:
scan: Scan the file using the virus scanner
always pass: Always assume the file is good
always fail: Always assume the file is bad

As an example, the following settings can be used for zip files in Windows:

[ZIP]
scanOption=scan
EXTRACT=\BBS\sbbs\exec\unzip.exe -qq -o %FILENAME% %FILESPEC% -d %TO_DIR%

Note that for the extract command, the following pseudonyms are used:
%FILENAME% : The name of the archive file or text file.
%FILESPEC% : This would specify the file(s) to be extracted from the archive. 
             The script actually will totally remove this from the command; it
             is not used.  It's currently here for possible future use.
%TO_DIR%   : The directory to which the archive file will be extracted.

Using the above example configuration for zip files, if the user (on node 1)
uploads D:\Files\someArchive.zip and your Synchronet installation is located
in D:\sbbs, the temp directory is D:\sbbs\node1\DDUploadProcessor_Temp, and
the extract command will be translated to the following:
unzip.exe -qq -o D:\Files\someArchive.zip -d D:\sbbs\node1\DDUploadProcessor_Temp

