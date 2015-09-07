/* This is an archive viewer for Synchronet.  This script lets
 * the user view the contents of an archive with a friendly
 * user interface.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 * Date       User              Description
 * 2009-08-29 Eric Oulashin     Started
 * 2009-08-31 Eric Oulashin     Re-did the configuration file.
 *                              Started re-arranging the code with a
 *                              plan to extract archives, rather than
 *                              using their view command in order to
 *                              view inside of archives.  Started
 *                              working on some functions to do this
 *                              so that it can be done recursively
 *                              (to handle subdirectories).
 * 2009-09-01 Eric Oulashin     Worked on the lightbar interface, and
 *                              on the script in general.
 * 2009-09-02 Eric Oulashin     Worked on file downloading, subdirectory
 *                              viewing, and other enhancements.  Started
 *                              working more on the traditional interface:
 *                              added writeCmdPrompt_Traditional(), etc.
 * 2009-09-03 Eric Oulashin     Updated the file size width to be 6 instead
 *                              of 10.
 *                              Made the file # show up in both the lightbar
 *                              and traditional interface (for consistency).
 *                              Added file # entry to the lightbar interface.
 * 2009-09-04 Eric Oulashin     Worked on functionality enhancements and
 *                              bug fixes.
 * 2009-09-05 Eric Oulashin     Worked on optimizing when file path slashes
 *                              are fixed.  Increased the size column from 7
 *                              to 8 chars.
 * 2009-09-07 Eric Oulashin     Fixed a bug in viewing text files - Now it
 *                              always pauses after the last screenful.
 *                              Also added a maxFileSize option to specify
 *                              the maximum size of a file that can be read.
 * 2009-09-08 Eric Oulashin     Updated the configuration file to include
 *                              maxArcFileSize and maxTextFileSize, to
 *                              specify the maximum archive file size and
 *                              maximum text file size.  Added sizeStrToBytes()
 *                              to help convert the number values read from
 *                              the config file to bytes.
 * 2009-09-10 Eric Oulashin     Version 1.00
 *                              First general public release
 * 2009-12-15 Eric Oulashin     Version 1.01
 *                              Updated the getFilenameExtension() function to
 *                              not use file_getext().  Added some unit test
 *                              functions.
 * 2009-12-15 Eric Oulashin     Version Pre1.02
 *                              Updated to store the name of the file to view
 *                              in a variable called fileToView.  Also added
 *                              some code to read the name of the file to view
 *                              from DDArcViewerFilename.txt in the node
 *                              directory if the filename is not specified on
 *                              the command line.
 *                              This method enables this script to view files
 *                              files with spaces in its path/filename, avoiding
 *                              the issue that strings with spaces are always
 *                              split up into separate command-line arguments.
 * 2009-12-18 Eric Oulashin     Version 1.02
 *                              Decided to release this version.
 * 2009-12-20 Eric Oulashin     Version 1.03
 *                              Made use of Tracker1's fixArgs() function to
 *                              combine command-line arguments that were
 *                              originally split due to spaces in the arguments.
 *                              This relies on the arguments being passed with
 *                              double-quotes around them, as in other
 *                              programming languages.
 */

/* Command-line arguments:
 1 (argv[0]): The name of the file to view
*/

load("sbbsdefs.js");

// Temporary (for testing) - Printing out command-line arguments
//console.print("argv[0]:" + argv[0] + ":\r\n\1p");
//console.print("argv: " + argv + "\r\n\1p");
// End Temporary

// Unit tests
/*
test_fixPathSlashes();
test_getFilenameExtension();
exit(0);
*/
// End Unit tests

// Determine the script's execution directory.
// This code is a trick that was created by Deuce, suggested by Rob
// Swindell as a way to detect which directory the script was executed
// in.  I've shortened the code a little.
// Note: gStartupPath will include the trailing slash.
var gStartupPath = '.';
try { throw dig.dist(dist); } catch(e) { gStartupPath = e.fileName; }
gStartupPath = backslash(gStartupPath.replace(/[\/\\][^\/\\]*$/,''));

// We need the deltree() and withoutTrailingSlash() functions
// from the cleanup script.
load(gStartupPath + "DDArcViewerCleanup.js");


// Version information
var gDDArcViewerVersion = "1.03";
var gDDArcViewerVerDate = "2009-12-20";
var gDDArcViewerProgName = "Digital Distortion Archive Viewer";


// Keyboard keys
var CTRL_M = "\x0d";
var KEY_ENTER = CTRL_M;
var ESC_KEY = "\x1b";

// Characters for display
var UP_ARROW_DISPLAY = "";
var DOWN_ARROW_DISPLAY = "";
var UPPER_LEFT_SINGLE = "Ú";
var HORIZONTAL_SINGLE = "Ä";
var UPPER_RIGHT_SINGLE = "¿";
var VERTICAL_SINGLE = "³";
var LOWER_LEFT_SINGLE = "À";
var LOWER_RIGHT_SINGLE = "Ù";


// Determine which slash character to use for paths, depending
// on the OS.
var gPathSlash = (/^WIN/.test(system.platform.toUpperCase())) ? "\\" : "/";


// gViewableFileTypes will contain ViewableFile objects representing files
// that can be viewed.  The following are the properties in this object:
//  Filename extension (in all upper-case)
//  
// properties/indexes will be the filename extensions
// (strings), and the values will be the OS command to view the file.
var gViewableFileTypes = new Object();

// General configuration
var gGenConfig = new Object();
// Set the general configuration defaults
gGenConfig.interfaceStyle = "Lightbar";
gGenConfig.inputTimeoutMS = 300000;
gGenConfig.maxArcFileSize = 1073741824; // 1.0GB
gGenConfig.maxTextFileSize = 5242880;   // 5.0MB
gGenConfig.colors = new Object();
gGenConfig.colors.archiveFilenameHdrText = "nwh";
gGenConfig.colors.archiveFilename = "ngh";
gGenConfig.colors.headerLine = "nyh";
gGenConfig.colors.headerSeparatorLine = "nkh";
gGenConfig.colors.fileNums = "nmh";
gGenConfig.colors.fileSize = "nw";
gGenConfig.colors.fileDate = "ng";
gGenConfig.colors.fileTime = "nr";
gGenConfig.colors.filename = "nc";
gGenConfig.colors.subdir = "ngh";
gGenConfig.colors.highlightedFile = "n4wh";


// If the filename was specified on the command line, then use that
// for the filename.  Otherwise, read the name of the file to view
// from DDArcViewerFilename.txt in the node directory.
var fileToView = "";
if (argv.length > 0)
{
   if (typeof(argv[0]) == "string")
   {
      // Make sure the arguments are correct (in case they have spaces),
      // then use the first one.
      var fixedArgs = fixArgs(argv);
      if ((typeof(fixedArgs[0]) == "string") && (fixedArgs[0].length > 0))
         fileToView = fixedArgs[0];
      else
      {
         console.print("nchError: ncBlank filename argument given.\r\np");
         exit(-2);
      }
   }
   else
   {
      console.print("nchError: ncUnknown command-line argument specified.\r\np");
      exit(-1);
   }
}
else
{
   // Read the filename from DDArcViewerFilename.txt in the node directory.
   // This is a workaround for file/directory names with spaces in
   // them, which would get separated into separate command-line
   // arguments for JavaScript scripts.
   var fileToView = "";
   var filenameFileFilename = system.node_dir + "DDArcViewerFilename.txt";
   var filenameFile = new File(filenameFileFilename);
   if (filenameFile.open("r"))
   {
      if (!filenameFile.eof)
         fileToView = filenameFile.readln(2048);
      filenameFile.close();
   }
}

// A filename must be specified as the first argument, so give an error and return
// if not.
//if ((argv[0] == undefined) || (argv[0] == null) || (typeof(argv[0]) != "string"))
if (fileToView.length == 0)
{
   console.print("nchError: ncNo filename specified to view.\r\np");
   exit(1);
}

// Read the configuration files.
var configFileRead = ReadConfig(gStartupPath);
// If the configuration file wasn't read, then output an error and exit.
if (!configFileRead)
{
   console.print("nchError: ncThis viewer was unable to read its configuration\r\n");
   console.print("file.\r\np");
   exit(2);
}


// Now, the fun begins..

// gRootWorkDir will containing the name of the root work directory.  This
// is where we'll extract the archive.
const gRootWorkDir = fixPathSlashes(system.node_dir + "DDArcViewerTemp/");
// Create the work directory (making sure it's gone first)
deltree(gRootWorkDir);
var gRootWorkDirExists = mkdir(gRootWorkDir);
// If the work dir was not created, then output an error and exit.
if (!gRootWorkDirExists)
{
   console.print("nchError: ncCould not create the temporary work directory.\r\np");
   exit(6);
}

// Store the path of the batch download directory for the node.
const gBatchDLDir = system.node_dir + "DDArcViewer_BatchDL/";

const gANSISupported = console.term_supports(USER_ANSI);
var useLightbar = (gANSISupported && (gGenConfig.interfaceStyle == "Lightbar"));

// Write a note in the log that the user is viewing the archive.
var gTopLevelFilename = fixPathSlashes(fileToView);
bbs.log_str(user.alias + " is viewing a file: " + gTopLevelFilename);
// View the file
var gListFilesFunction = useLightbar ? listFiles_Lightbar : listFiles_Traditional;
var retObj = viewFile(gTopLevelFilename, gRootWorkDir, gListFilesFunction);

// Clean up (remove the root work directory, etc.)
console.print("ncViewer cleanupi...n\r\n");
console.line_counter = 0; // To prevent pausing
deltree(gRootWorkDir);

// If the batch download directory exists and is empty, then remove it.
if (directory(gBatchDLDir + "*").length == 0)
   deltree(gBatchDLDir);

exit(retObj.errorCode);
// End of script execution.

//////////////////////////////////////////////////////////////////////////////////
// Object stuff

// Constructor for the ViewableFile object, which contains information
// about a viewable file.
//
// Parameters:
//  pExtension: The filename extension
//  pViewCmd: The OS command to view it
//
// The ViewableFile object contains the following properties:
//  extension: The filename extension
//  viewCmd: The OS command to view it
//  pExtractCmd: The OS command to extract it
function ViewableFile(pExtension, pViewCmd, pExtractCmd)
{ 
   this.extension = "";      // The archive filename extension
   this.viewCmd = "";        // The command to view the archive
   this.extractCmd = "";     // The command to extract the archive

   // If isText is true, then the file is a text file; otherwise,
   // treat it as a compressed archive.
   this.isText = false;

   // If pExtension, pViewCmd, or pExtractCmd are valid, then use them to set
   // the object parameters.
   if ((pExtension != null) && (pExtension != undefined) && (typeof(pExtension) == "string"))
      this.extension = pExtension;
   if ((pViewCmd != null) && (pViewCmd != undefined) && (typeof(pViewCmd) == "string"))
      this.viewCmd = pViewCmd;
   if ((pExtractCmd != null) && (pExtractCmd != undefined) && (typeof(pExtractCmd) == "string"))
      this.extractCmd = pExtractCmd;
}


//////////////////////////////////////////////////////////////////////////////////
// Functions

// Reads the configuration files and populates the global configuration objects
// with the configuration settings.  Returns whether or not configuration
// settings were read.  Also populates gGenConfig with the general
// configuration options.
//
// Parameters:
//  pCfgFilePath: The path from which to load the configuration file.
//
// Return value: Boolean - Whether or not configuration settings were read.
function ReadConfig(pCfgFilePath)
{
   // Read the file type settings
   var fileTypeSettingsRead = false;
   var fileTypeCfgFile = new File(pCfgFilePath + "DDArcViewerFileTypes.cfg");
   if (fileTypeCfgFile.open("r"))
   {
      if (fileTypeCfgFile.length > 0)
      {
         fileTypeSettingsRead = true;
         // Read each line from the config file and set the
         // various options.
         var pos = 0;               // Index of = in the file lines
         var fileLine = "";
         var filenameExt = "";      // Archive filename extension
         var option = "";           // Configuration option
         var optionValue = "";      // Configuration option value
         var optionValueUpper;      // Upper-cased configuration option value
         var viewableFile = null;   // Will be used to create & store viewable archive options
         while (!fileTypeCfgFile.eof)
         {
            // Read the line from the config file, look for a =, and
            // if found, read the option & value and set them
            // in cfgObj.
            fileLine = fileTypeCfgFile.readln(1024);

            // fileLine should be a string, but I've seen some cases
            // where it isn't, so check its type.
            if (typeof(fileLine) != "string")
               continue;

            // If the line is blank or starts with with a semicolon
            // (the comment character), then skip it.
            if ((fileLine.length == 0) || (fileLine.substr(0, 1) == ";"))
               continue;

            // Look for a file extension in square brackets ([ and ]).
            // If found, then set filenameExt and continue onto the next line.
            // Note: This regular expression allows whitespace around the [...].
            if (/^\s*\[.*\]\s*$/.test(fileLine))
            {
               var startIndex = fileLine.indexOf("[") + 1;
               var endIndex = fileLine.lastIndexOf("]");
               var ext = fileLine.substr(startIndex, endIndex-startIndex).toUpperCase();
               // If the filename extension is different than the last one
               // we've seen, then:
               // 1. If viewableFile is not null, then add it to gViewableFileTypes.
               // 2. Create a new one (referenced as viewableFile).
               if (ext != filenameExt)
               {
                  if ((viewableFile != null) && (viewableFile != undefined) &&
                      (filenameExt.length > 0))
                  {
                     gViewableFileTypes[filenameExt] = viewableFile;
                  }
                  filenameExt = ext;
                  viewableFile = new ViewableFile();
                  viewableFile.extension = ext;
               }
               continue;
            }

            // If filenameExt is blank, then continue onto the next line.
            if (filenameExt.length == 0)
               continue;

            // If we're here, then filenameExt is set, and this is a valid
            // line to process.
            // Look for an = in the line, and if found, split into
            // option & value.
            pos = fileLine.indexOf("=");
            if (pos > -1)
            {
               // Extract the option & value, trimming leading & trailing spaces.
               option = trimSpaces(fileLine.substr(0, pos), true, false, true).toUpperCase();
               optionValue = trimSpaces(fileLine.substr(pos+1), true, false, true);

               if (option == "VIEW")
                  viewableFile.viewCmd = optionValue;
               else if (option == "EXTRACT")
                  viewableFile.extractCmd = optionValue;
               else if (option == "ISTEXT")
                  viewableFile.isText = (optionValue.toUpperCase() == "YES");
            }
         }
      }

      fileTypeCfgFile.close();
   }

   // Read the general program configuration
   var genSettingsRead = false;
   var genCfgFile = new File(pCfgFilePath + "DDArcViewer.cfg");
   if (genCfgFile.open("r"))
   {
      if (genCfgFile.length > 0)
      {
         genSettingsRead = true;
         var settingsMode = "";
         var fileLine = null;     // A line read from the file
         var equalsPos = 0;       // Position of a = in the line
         var commentPos = 0;      // Position of the start of a comment
         var setting = null;      // A setting name (string)
         var settingUpper = null; // Upper-case setting name
         var value = null;        // A value for a setting (string)
         while (!genCfgFile.eof)
         {
            // Read the next line from the config file.
            fileLine = genCfgFile.readln(1024);

            // fileLine should be a string, but I've seen some cases
            // where it isn't, so check its type.
            if (typeof(fileLine) != "string")
               continue;

            // If the line starts with with a semicolon (the comment
            // character) or is blank, then skip it.
            if ((fileLine.substr(0, 1) == ";") || (fileLine.length == 0))
               continue;

            // If in the "behavior" section, then set the behavior-related variables.
            if (fileLine.toUpperCase() == "[BEHAVIOR]")
            {
               settingsMode = "behavior";
               continue;
            }
            else if (fileLine.toUpperCase() == "[COLORS]")
            {
               settingsMode = "colors";
               continue;
            }

            // If settingsMode is blank, then skip this line.
            if (settingsMode.length == 0)
               continue;

            // If the line has a semicolon anywhere in it, then remove
            // everything from the semicolon onward.
            commentPos = fileLine.indexOf(";");
            if (commentPos > -1)
               fileLine = fileLine.substr(0, commentPos);

            // Look for an equals sign, and if found, separate the line
            // into the setting name (before the =) and the value (after the
            // equals sign).
            equalsPos = fileLine.indexOf("=");
            if (equalsPos > 0)
            {
               // Read the setting & value, and trim leading & trailing spaces.
               setting = trimSpaces(fileLine.substr(0, equalsPos), true, false, true);
               settingUpper = setting.toUpperCase();
               value = trimSpaces(fileLine.substr(equalsPos+1), true, false, true).toUpperCase();

               if (settingsMode == "behavior")
               {
                  // Skip this one if the value is blank.
                  if (value.length == 0)
                     continue;

                  // Set the appropriate value in the settings object.
                  if (settingUpper == "INTERFACESTYLE")
                  {
                     // Ensure that the first character is uppercase and the
                     // rest is lower-case.
                     if ((value == "LIGHTBAR") || (value == "TRADITIONAL"))
                     {
                        gGenConfig.interfaceStyle = value.substr(0, 1).toUpperCase()
                                                  + value.substr(1).toLowerCase();
                     }
                  }
                  else if (settingUpper == "INPUTTIMEOUTMS")
                     gGenConfig.inputTimeoutMS = +value;
                  else if (settingUpper == "MAXARCFILESIZE")
                     gGenConfig.maxArcFileSize = sizeStrToBytes(value);
                  else if (settingUpper == "MAXTEXTFILESIZE")
                     gGenConfig.maxTextFileSize = sizeStrToBytes(value);
               }
               else if (settingsMode == "colors")
                  gGenConfig.colors[setting] = value;
            }
         }

         genCfgFile.close();
      }
   }

   return (fileTypeSettingsRead && genSettingsRead);
}
// Helper for ReadConfig(): Converts a size string from the config file (i.e.,
// for max file size) to the number of bytes.
//
// Parameters:
//  pSizeStr: A max file size value from the config file.  May contain a K, M,
//            or G at the end for Kilobytes, Megabytes, or Gigabytes.
function sizeStrToBytes(pSizeStr)
{
   if ((pSizeStr == null) || (pSizeStr == undefined))
      return 0;
   if (typeof(pSizeStr) != "string")
      return 0;
   if (pSizeStr.length == 0)
      return 0;

   var numBytes = 0;
   // Look for a K, M, or G at the end of the pSizeStr.  If found,
   // that signifies kilobytes, megabytes, or gigabytes, respectively.
   var lastChar = pSizeStr.charAt(pSizeStr.length-1).toUpperCase();
   if (lastChar == "K") // Kilobytes
      numBytes = +(pSizeStr.substr(0, pSizeStr.length-1)) * 1024;
   else if (lastChar == "M") // Megabytes
      numBytes = +(pSizeStr.substr(0, pSizeStr.length-1)) * 1048576;
   else if (lastChar == "G") // Gigabytes
      numBytes = +(pSizeStr.substr(0, pSizeStr.length-1)) * 1073741824;
   else
      numBytes = +pSizeStr;
   if (isNaN(numBytes))
      numBytes = 0;
   numBytes = Math.floor(numBytes);
   return numBytes;
}


// Extracts an archive to a directory.  Returns an array containing the
// output of the extraction command.
//
// Parameters:
//  pArchiveFilename: The name of the archive file to extract
//  pExtractCmd: The command to extract the file (from the configuration)
//  pDestDir: The directory to which to extract the file
//
// Return value: An array containing the output of the extraction command.
function extractArchive(pArchiveFilename, pExtractCmd, pDestDir)
{
   // Make sure all the slashes in the filename and the destination directory
   // are correct for the platform.
   var filename = fixPathSlashes(pArchiveFilename);
   var destDir = fixPathSlashes(pDestDir);

   // Build the extract command, and then execute it.
   // Note: This puts double-quotes around the filename and destination dir
   // to deal with spaces.
   var command = pExtractCmd.replace("%FILENAME%", "\"" + filename + "\"");
   // I was originally going to replace %FILESPEC% with * (to extract all files), but
   // that doesn't work on *nix because * expands to all files in the current dir.
   // And if the filespec isn't specified, it extracts all files anyway.
   //command = command.replace("%FILESPEC%", "*"); // Extract all files
   command = command.replace("%FILESPEC%", ""); // Extract all files
   command = command.replace("%TO_DIR%", "\"" + withoutTrailingSlash(destDir) + "\"");
   return (execCmdWithOutput(command));
}

// Views a file.
//
// Parameters:
//  pFilename: The name of the file to view
//  pWorkDir: The directory in which to place extracted files
//  pListFilesFunction: A pointer to the file listing function.  This
//                      would point to either listFiles_Lightbar() or
//                      listFiles_Traditional().
//
// Return value: An object containing the following properties:
//   errorCode: An code representing an error.  0 = no error.
function viewFile(pFilename, pWorkDir, pListFilesFunction)
{
   var retObj = new Object();
   retObj.errorCode = 0;

   // Determine the filename's extension
   var filenameExt = getFilenameExtension(pFilename);
   if (filenameExt.length == 0)
   {
      console.print("nchError: ncThe filename has no extension.\r\np");
      retObj.errorCode = 3;
      return(retObj);
   }
   // If the filename extension is unknown, then output an error and exit.
   if ((gViewableFileTypes[filenameExt] == undefined) || (gViewableFileTypes[filenameExt] == null))
   {
      console.print("nchError: ncThe filename extension (" + filenameExt + ") is unknown.\r\np");
      retObj.errorCode = 4;
      return(retObj);
   }

   // Get the filename without the full path in front.
   var justFilename = getFilenameFromPath(pFilename);

   // If the file is a text file, then view it using the VIEW command.
   // Otherwise, treat it as an archive: Extract it and list the files.
   if (gViewableFileTypes[filenameExt].isText)
   {
      // If file size limit is in effect and the file is too big, then refuse
      // to view it.
      if ((gGenConfig.maxTextFileSize > 0) && (file_size(pFilename) > gGenConfig.maxTextFileSize))
      {
         //console.print("Here 1\r\n"); // Temporary
         console.print("nchError: ncThe file is too big to view.\r\np");
         retObj.errorCode = 8;
         return(retObj);
      }
      viewUsingVIEWCmd(pFilename, filenameExt);
   }
   else
   {
      // The file is an archive file.
      // If file size limit is in effect and the file is too big, then refuse
      // to view it.
      if ((gGenConfig.maxArcFileSize > 0) && (file_size(pFilename) > gGenConfig.maxArcFileSize))
      {
         //console.print("Here 2\r\n"); // Temporary
         //console.print("File size: " + file_size(pFilename) + ", limit: " + gGenConfig.maxarcFileSize + "\r\n"); // Temporary
         console.print("nchError: ncThe file is too big to view.\r\np");
         retObj.errorCode = 8;
         return(retObj);
      }

      // If there is no extract command specified for the archive type, then
      // output an error and exit.
      if (gViewableFileTypes[filenameExt].extractCmd.length == 0)
      {
         console.print("nchError: ncNo extract command is defined for " +
                       filenameExt + " files.\r\np");
         retObj.errorCode = 5;
         return(retObj);
      }

      // Create a subdirectory within pWorkDir where the file will be extracted.
      var workDir = pWorkDir + justFilename + "_Temp/";
      deltree(workDir);
      var workDirExists = mkdir(workDir);
      // If the work directory was created, then extract the archive and list
      // its files.  Otherwise, fall back to the view command to view it.
      if (workDirExists)
      {
         // Extract the archive into the work directory.
         console.print("ncExtracting " + justFilename + "i...n\r\n");
         console.line_counter = 0; // To prevent pausing
         extractArchive(pFilename, gViewableFileTypes[filenameExt].extractCmd, workDir);
   
         // List the files.
         var screenRow = pListFilesFunction(workDir, justFilename);
   
         // Remove the work directory and all of its contents (and tell
         // the user what we're doing).
         console.print("n");
         console.gotoxy(1, screenRow);
         console.print("yhPerforming cleanup for " + justFilename + "i...n\r\n");
         console.line_counter = 0; // To prevent pausing
         deltree(workDir);
      }
      else
      {
         console.clear("n");
         console.print("nchNote: ncCould not create the temporary work directory for\r\n" +
                       justFilename + ", so only a basic view is provided.\r\np");
         viewUsingVIEWCmd(pFilename, filenameExt);
         retObj.errorCode = 7;
      }
   }

   return(retObj);
}

// Views a text file using its VIEW command.
//
// Parameters:
//  pFilename: The full path & name of the file
//  pFilenameExt: The filename's extension
function viewUsingVIEWCmd(pFilename, pFilenameExt)
{
   // Make sure all the slashes in the filename are correct for the platform.
   //var filename = fixPathSlashes(pFilename);

   // Get the view command with %FILENAME% replaced by the actual filename.
   var viewCmd = gViewableFileTypes[pFilenameExt].viewCmd.replace("%FILENAME%", pFilename);
   // Execute the view command, get its output, and display the output.
   var outputArray = execCmdWithOutput(viewCmd);
   if (outputArray.length > 0)
   {
      console.print("n\r\n");
      var numLinesOutput = 1;     // To test for outputting a pageful of lines
      var pageSize = console.screen_rows - 1;

      for (var i = 0; i < outputArray.length; ++i)
      {
         write_raw(outputArray[i]);
         // Update numLinesOutput based on how many screen lines were written,
         // based on the width of the line in the array.
         if (outputArray[i].length <= console.screen_columns)
            ++numLinesOutput;
         else if (outputArray[i].length > console.screen_columns)
            numLinesOutput += Math.ceil(outputArray[i].length / console.screen_columns);
         if (outputArray[i].length != console.screen_columns)
            console.crlf();
         // Prevent automatic screen pausing.  And if we've written a
         // screenful of lines, then prompt the user whether to continue or quit.
         console.line_counter = 0;
         if (numLinesOutput == pageSize)
         {
            numLinesOutput = 0;
            // Prompt the user to continue.  If they say yes, then
            // reset the color back to normal.  Otherwise, break from
            // the input loop.
            if (console.yesno("cContinue"))
               console.print("n");
            else
               break;
         }
      }
      // Edge case: If the text file is smaller than a screenful or if we've
      // written less than a screenful, then pause.
      if ((outputArray.length < pageSize) || (numLinesOutput < pageSize))
         console.pause();
   }
   else
      console.print("ncThe file is empty.");
}

// Shows the user the list of files in a given directory (traditional
// interface).
//
// Parameters:
//  pDir: The directory to list
//  pArchiveFilename: The name of the archive that contained the files, without the full path
//
// Return value: The screen row that's one past the last file listed.
function listFiles_Traditional(pDir, pArchiveFilename)
{
   var onePastLastFileRow = console.screen_rows;

   // Get a list of files in the directory
   var files = directory(pDir + "*");
   if (files.length > 0)
   {
      // This object will keep track of which filenames have had their
      // slashes fixed.
      var fileSlashesFixed = new Object();

      // Screen display and array index variables
      const listTopRow = 4;
      const listBottomRow = console.screen_rows - 1;
      const numPageLines = listBottomRow - listTopRow + 1; // # of lines in a page
      var topFileIndex = 0; // The index of the array entry at the top of the screen
      // Calculate the index of the filename that appears at the top of the
      // last page.
      var lastPageTopIndex = numPageLines;
      while (lastPageTopIndex < files.length)
         lastPageTopIndex += numPageLines;
      lastPageTopIndex -= numPageLines;
      // lastPageRow will store the row number of the last file displayed on the screen.
      var lastPageRow = listBottomRow;

      // This function goes to the next page.  Returns whether or not
      // to redraw (i.e., we won't want to redraw if there is no next
      // page).
      function goToNextPage()
      {
         var redraw = false;
         var nextPageTopIndex = topFileIndex + numPageLines;
         if (nextPageTopIndex < files.length)
         {
            topFileIndex = nextPageTopIndex;
            selectedIndex = topFileIndex;
            redraw = true;
         }
         return redraw;
      }
      // This function goes to the previous page.  Returns whether or
      // not to redraw (i.e., we won't want to redraw if there is no
      // next page).
      function goToPrevPage()
      {
         var redraw = false;
         var prevPageTopIndex = topFileIndex - numPageLines;
         if (prevPageTopIndex >= 0)
         {
            topFileIndex = prevPageTopIndex;
            selectedIndex = topFileIndex;
            redraw = true;
         }
         return redraw;
      }
      // This function calculates lastPageRow based on topFileIndex.
      function calcLastPageRow()
      {
         lastPageRow = listBottomRow;
         if (files.length-topFileIndex < numPageLines)
            lastPageRow = listTopRow + (files.length-topFileIndex) - 1;
         onePastLastFileRow = lastPageRow + 1;
      }


      // Start listing the file information.

      // Output the file list headers.
      console.clear("n");
      console.gotoxy(1, 1);
      writeFileListHeader(pArchiveFilename);

      // Input loop
      var userInput = "";
      var redrawFileInfo = true; // Whether or not to update the file info on the screen
      var continueOn = true;
      while (continueOn)
      {
         // List the screenful of files if redrawFileInfo is true.
         if (redrawFileInfo)
            listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow, -1, true, false);
         redrawFileInfo = true;
         // Calculate the screen row number of the last file displayed on the screen, and
         // place the cursor one line below it.
         calcLastPageRow();
         if (gANSISupported)
            console.gotoxy(1, lastPageRow+1);
         writeCmdPrompt_Traditional();
         // Get a keypress from the user, with a timeout.
         userInput = console.inkey(K_UPPER|K_NOCRLF|K_NOSPIN, gGenConfig.inputTimeoutMS);
         // If the user's terminal doesn't support ANSI, and if the user didn't
         // enter a number, then output a newline so that the text looks okay.
         if (!gANSISupported && userInput.match(/[^0-9]/))
            console.crlf();
         // If userInput is blank, that means the timeout was reached.
         if (userInput.length == 0)
         {
            // Display a message and exit.
            console.print("n\r\nyhInput timeout reached.");
            mswait(2500);
            continueOn = false;
         }
         // Numeric digit: The start of a file number
         else if (userInput.match(/[0-9]/))
         {
            // Put the user's input back in the input buffer to
            // be used for getting the rest of the message number.
            console.ungetstr(userInput);
            var retObj = doFileNumInput(files, pDir, listFiles_Traditional);
            // doFileNumInput() fixes file path slashes for the chosen file,
            // so if the user chose a file, then update fileSlashesFixed
            // for the chosen array index.
            if (retObj.arrayIndex > -1)
               fileSlashesFixed[retObj.arrayIndex] = true;
            // Refresh the headers on the screen.
            if (gANSISupported)
            {
               console.clear("n");
               console.gotoxy(1, 1);
               writeFileListHeader(pArchiveFilename);
            }
         }
         // N: Next page
         else if ((userInput == "N") || (userInput == KEY_ENTER))
         {
            redrawFileInfo = (goToNextPage() || !gANSISupported);
            calcLastPageRow();
            // If there is less than a pageful of file entries on this
            // new page, then we need to erase the bottom row of the
            // screen where the input prompt line was.
            if (gANSISupported && (lastPageRow < listBottomRow))
            {
               console.gotoxy(1, console.screen_rows);
               console.clearline("n");
            }
            else
               console.crlf();
         }
         // P: Previous page
         else if (userInput == "P")
            redrawFileInfo = goToPrevPage();
         // F: first page
         else if (userInput == "F")
         {
            // Set topFileIndex to 0 only if it's > 0.  Otherwise,
            // we don't want to redraw the screen.
            if (topFileIndex > 0)
               topFileIndex = 0;
            else
            {
               if (gANSISupported)
                  redrawFileInfo = false;
            }
         }
         // L: Last page
         else if (userInput == "L")
         {
            // Set topFileIndex for the last page only if we're not
            // already there.  Otherwise, we don't want to redraw
            // the screen.
            if (topFileIndex < lastPageTopIndex)
            {
               topFileIndex = lastPageTopIndex;
               calcLastPageRow();
               // If there is less than a pageful of file entries on this
               // new page, then we need to erase the bottom row of the
               // screen where the input prompt line was.
               if (gANSISupported && (lastPageRow < listBottomRow))
               {
                  console.gotoxy(1, console.screen_rows);
                  console.clearline("n");
               }
            }
            else
               redrawFileInfo = false;
         }
         // D: Download a file
         else if (userInput == "D")
         {
            if (gANSISupported)
               console.crlf();
            // Prompt the user for a file number.
            console.print("ncFile #: h");
            userInput = console.getnum(files.length);
            if (userInput > 0)
            {
               var index = userInput - 1;
               // If the entry is a directory, then output an error.
               // Otherwise, let the user download the file.
               if (file_isdir(files[index]))
               {
                  console.print("nwhCan't download an entire directory.");
                  mswait(1500);
               }
               else
               {
                  // Confirm with the user whether to download the file
                  var filename = getFilenameFromPath(files[index]);
                  var confirmText = "ncDownload h"
                                  + filename.substr(0, console.screen_columns-12) + "n";
                  if (console.yesno(confirmText))
                  {
                     bbs.log_str(user.alias + " is downloading a file from an archive: " +
                                 files[index]);
                     bbs.send_file(files[index]);
                  }
               }
            }
   
            // Refresh the screen by outputting the file list headers.
            if (gANSISupported)
            {
               console.clear("n");
               console.gotoxy(1, 1);
               writeFileListHeader(pArchiveFilename);
            }
         }
         // V: View a file
         else if (userInput == "V")
         {
            if (gANSISupported)
               console.crlf();
            // Prompt the user for a file number.
            console.print("ncFile #: h");
            userInput = console.getnum(files.length);
            if (userInput > 0)
            {
               // If this file hasn't had its slashes fixed, then fix them.
               var index = (+userInput) - 1;
               if (!fileSlashesFixed[index])
               {
                  files[index] = fixPathSlashes(files[index]);
                  fileSlashesFixed[index] = true;
               }

               // Confirm with the user whether to view the file.
               var filename = getFilenameFromPath(files[index]);
               var confirmText = "ncView h"
                               + filename.substr(0, console.screen_columns-15) + "n";
               if (console.yesno(confirmText))
               {
                  // If the entry is a directory, then call this function again
                  // to look in there.  Otherwise, call viewFile() to view it.
                  if (file_isdir(files[index]))
                     listFiles_Traditional(files[index], filename);
                  else
                  {
                     bbs.log_str(user.alias + " is viewing a file inside an archive: " + files[index]);
                     viewFile(files[index], pDir, listFiles_Traditional);
                  }
               }
            }
   
            // Refresh the screen by outputting the file list headers.
            if (gANSISupported)
            {
               console.clear("n");
               console.gotoxy(1, 1);
               writeFileListHeader(pArchiveFilename);
            }
         }
         // Q or ESC key: Quit
         else if ((userInput == "Q") || (userInput == ESC_KEY))
         {
            continueOn = false;
            break;
         }
         // ?: Display help
         else if (userInput == "?")
         {
            showHelpScreen(false);
            // Refresh the screen by outputting the file list headers.
            if (gANSISupported)
            {
               console.clear("n");
               console.gotoxy(1, 1);
               writeFileListHeader(pArchiveFilename);
            }
         }
      }
   }
   else
      console.print("ncCould not extract/view the file, or it is empty.\r\np");

   return onePastLastFileRow;
}

// Writes the header lines for the file list.
//
// Parameters:
//  pFilename: The name of the file/dir being displayed.
function writeFileListHeader(pFilename)
{
   // Construct the header text variables only once (for speed).
   if (writeFileListHeader.topHelp2 == undefined)
   {
      writeFileListHeader.topHelp2 = gGenConfig.colors.headerLine
                                   + "   #     Size    Date    Time  Filename\r\n";
   }
   if (writeFileListHeader.topHelp3 == undefined)
   {
      writeFileListHeader.topHelp3 = gGenConfig.colors.headerSeparatorLine
                                   + "ÄÄÄÄ ÄÄÄÄÄÄÄÄ ÄÄÄÄÄÄÄÄÄÄ ÄÄÄÄÄ ";
      // Add line characters to the end of the screen.
      for (var x = 30; x < console.screen_columns - 2; ++x)
         writeFileListHeader.topHelp3 += "Ä";
      writeFileListHeader.topHelp3 += "\r\n";
   }

   // Write the header lines.
   printf("%s%s %s%s\r\n", gGenConfig.colors.archiveFilenameHdrText, "List of:",
          gGenConfig.colors.archiveFilename,
          pFilename.substr(0, console.screen_columns-9));
   console.print(writeFileListHeader.topHelp2);
   console.print(writeFileListHeader.topHelp3);
}

// Performs file number input and viewing/sending a file to the user based
// on the file number (1-based).
//
// Parameters:
//  pFileArray: The array of filenames
//  pDir: The directory where the files are
//  pListFilesFunction: A pointer to the file listing function.  This
//                      would point to either listFiles_Lightbar() or
//                      listFiles_Traditional().
//  pFileNum: Optional - A file number (1-based).  If this is specified,
//            the user won't be prompted for the file number.
//
// Return value: An object containing the following properties:
//               fileNum: The user's selected file number (1-based)
//               arrayIndex: The file array index (0-based)
function doFileNumInput(pFileArray, pDir, pListFilesFunction, pFileNum)
{
   // Create the return object
   var retObj = new Object();
   retObj.fileNum = (pFileNum == undefined ? -1 : pFileNum);
   retObj.arrayIndex = (pFileNum == undefined ? -1 : pFileNum-1);

   // Create a persistent object to keep track of the indexes of the files
   // in the array that have had their slashes fixed.
   if (doFileNumInput.fileSlashesFixed == undefined)
      doFileNumInput.fileSlashesFixed = new Object();

   var userInput = 0;
   // If pFileNum wasn't passed in, then prompt the user for the file
   // number.  Otherwise, use pFileNum.
   if ((pFileNum == null) || (pFileNum == undefined))
      userInput = console.getnum(pFileArray.length);
   else
      userInput = pFileNum;
   if (userInput > 0)
   {
      retObj.fileNum = userInput;
      retObj.arrayIndex = userInput - 1;

      // If this file hasn't had its slashes fixed, then fix them.
      if (!doFileNumInput.fileSlashesFixed[retObj.arrayIndex])
      {
         pFileArray[retObj.arrayIndex] = fixPathSlashes(pFileArray[retObj.arrayIndex]);
         doFileNumInput.fileSlashesFixed[retObj.arrayIndex] = true;
      }

      var justFilename = getFilenameFromPath(pFileArray[retObj.arrayIndex]);
      // If the file is a directory, don't prompt the user; just view it.
      // Otherwise, prompt the user to view, download, or cancel.
      if (file_isdir(pFileArray[retObj.arrayIndex]))
         userInput = "V";
      else
      {
         console.print("nch" +
                       justFilename.substr(0, console.screen_columns-1));
         console.crlf();
         console.print("y(cVy)nciewbh, " +
                       "y(cDy)ncownloadbh, or " +
                       "y(cCy)ncancel: h");
         userInput = console.getkeys("VDC").toString();
      }
      if (userInput == "V")
      {
         // View the file
         // If the entry is a directory, then call this function again
         // to look in there.  Otherwise, call viewFile() to view it.
         if (file_isdir(pFileArray[retObj.arrayIndex]))
            pListFilesFunction(pFileArray[retObj.arrayIndex], justFilename);
         else
         {
            bbs.log_str(user.alias + " is viewing a file inside an archive: " +
                        pFileArray[retObj.arrayIndex]);
            viewFile(pFileArray[retObj.arrayIndex], pDir, pListFilesFunction);
         }
      }
      else if (userInput == "D")
      {
         // Download the file
         // If the entry is a directory, then output an error.
         // Otherwise, let the user download the file.
         if (file_isdir(pFileArray[retObj.arrayIndex]))
         {
            console.print("nwhCan't download an entire directory.");
            mswait(1500);
         }
         else
         {
            bbs.log_str(user.alias + " is downloading a file from an archive: " +
                        pFileArray[retObj.arrayIndex]);
            bbs.send_file(pFileArray[retObj.arrayIndex]);
         }
      }
   }

   return retObj;
}

// Displays the command prompt for the traditional interface.
function writeCmdPrompt_Traditional()
{
   // Construct the prompt text only once (for speed).
   if (writeCmdPrompt_Traditional.text == undefined)
   {
      writeCmdPrompt_Traditional.text = "nyh(cNy)ncextbh, "
            + "yh(cPy)ncrevbh, yh(cFy)ncirstbh, "
            + "yh(cLy)ncastbh, yh(cVy)nciewbh, "
            + "yh(cDy)ncLbh, yh(cQy)ncuitbh, "
            + "c#b, or c?nc: h";
   }
   console.print(writeCmdPrompt_Traditional.text);
}

// Shows the user the list of files in a given directory (lightbar
// interface).
//
// Parameters:
//  pDir: The directory to list
//  pArchiveFilename: The name of the archive that contained the files, without the full path
//
// Return value: The screen row that's one past the last file listed.
function listFiles_Lightbar(pDir, pArchiveFilename)
{
   var onePastLastFileRow = console.screen_rows;

   // Get a list of files in the directory
   var files = directory(pDir + "*");
   if (files.length > 0)
   {
      // This object will keep track of which filenames have had their
      // slashes fixed.
      var fileSlashesFixed = new Object();

      // Screen row and array index variables
      const listTopRow = 4;
      const listBottomRow = console.screen_rows - 1;
      const numPageLines = listBottomRow - listTopRow + 1; // # of lines in a page
      var topFileIndex = 0;  // Index of the file appearing at the top of the screen
      var selectedIndex = 0; // The index of the currently selected file
      onePastLastFileRow = listTopRow + (files.length-topFileIndex);

      // Set up a cursor position object, which will start at the top row.
      var curpos = new Object();
      curpos.x = 1;
      curpos.y = listTopRow;

      // Calculate the index of the filename that appears at the top of the
      // last page.
      var lastPageTopIndex = numPageLines;
      while (lastPageTopIndex < files.length)
         lastPageTopIndex += numPageLines;
      lastPageTopIndex -= numPageLines;

      // This function goes to the next page.  Returns whether or not
      // to redraw (i.e., we won't want to redraw if there is no next
      // page).
      function goToNextPage()
      {
         var redraw = false;
         var nextPageTopIndex = topFileIndex + numPageLines;
         if (nextPageTopIndex < files.length)
         {
            topFileIndex = nextPageTopIndex;
            selectedIndex = topFileIndex;
            curpos.y = listTopRow;
            onePastLastFileRow = listTopRow + (files.length-topFileIndex) + 1;
            redraw = true;
         }
         return redraw;
      }
      // This function goes to the previous page.  Returns whether or
      // not to redraw (i.e., we won't want to redraw if there is no
      // next page).
      function goToPrevPage()
      {
         var redraw = false;
         var prevPageTopIndex = topFileIndex - numPageLines;
         if (prevPageTopIndex >= 0)
         {
            topFileIndex = prevPageTopIndex;
            selectedIndex = topFileIndex;
            curpos.y = listTopRow;
            redraw = true;
         }
         return redraw;
      }

      // Draw the header lines and the help line at the bottom
      drawTopAndBottom_Lightbar(pArchiveFilename);

      // Now, output the first page of information, move the cursor up to the top,
      // and start the input loop.
      listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow, topFileIndex,
                              false, true);
      var userInput = "";
      var continueOn = true;
      while (continueOn)
      {
         console.gotoxy(curpos); // Make sure the cursor is still in the right place
         // Get a key from the user, with a timeout.
         userInput = console.inkey(K_UPPER|K_NOCRLF|K_NOSPIN, gGenConfig.inputTimeoutMS);
         // If userInput is blank, that means the timeout was reached.
         if (userInput.length == 0)
         {
            // Display a message and exit.
            console.gotoxy(1, console.screen_rows);
            console.clearline("n");
            console.print("nyhInput timeout reached.");
            mswait(2500);
            continueOn = false;
         }
         // Up arrow: Move up one file
         else if (userInput == KEY_UP)
         {
            // Go to the previous file, if possible.
            if (selectedIndex > 0)
            {
               if (curpos.y > listTopRow)
               {
                  // Draw the current file information in normal colors.
                  console.gotoxy(1, curpos.y);
                  writeFileInfo(files[selectedIndex], false, selectedIndex+1);
                  // Go to the next file, position the cursor where it should be,
                  // and write the file information in the highlight color.
                  --selectedIndex;
                  --curpos.y;
                  console.gotoxy(1, curpos.y);
                  writeFileInfo(files[selectedIndex], true, selectedIndex+1);
               }
               else
               {
                  // Go to the previous page, if possible.
                  // Note: curpos is updated by goToPrevPage(), so we don't have to
                  // update it here.
                  if (goToPrevPage())
                  {
                     // Move the selection to the bottom of the list, then refresh the screen.
                     selectedIndex = topFileIndex + numPageLines - 1;
                     curpos.y = listBottomRow;
                     listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                             selectedIndex, true, true);
                  }
               }
            }
         }
         // Down arrow: Move down one file
         else if (userInput == KEY_DOWN)
         {
            // Go to the next file, if possible.
            if (selectedIndex < files.length-1)
            {
               if (curpos.y < listBottomRow)
               {
                  // Draw the current file information in normal colors.
                  console.gotoxy(1, curpos.y);
                  writeFileInfo(files[selectedIndex], false, selectedIndex+1);
                  // Go to the next file, position the cursor where it should be,
                  // and write the file information in the highlight color.
                  ++selectedIndex;
                  ++curpos.y;
                  console.gotoxy(1, curpos.y);
                  writeFileInfo(files[selectedIndex], true, selectedIndex+1);
               }
               else
               {
                  // Go to the next page, if possible.
                  // Note: curpos is updated by goToNextPage(), so we don't have to
                  // update it here.
                  if (goToNextPage())
                  {
                     listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                             selectedIndex, true, true);
                  }
               }
            }
         }
         // Home key: Go to the first file shown on the screen
         else if (userInput == KEY_HOME)
         {
            if ((selectedIndex > topFileIndex) && (curpos.y > listTopRow))
            {
               // Draw the current file information in normal colors.
               console.gotoxy(1, curpos.y);
               writeFileInfo(files[selectedIndex], false, selectedIndex+1);
               // Go to the top index on the screen, and draw that file's
               // information in the highlight color.
               selectedIndex = topFileIndex;
               curpos.y = listTopRow;
               console.gotoxy(1, curpos.y);
               writeFileInfo(files[selectedIndex], true, selectedIndex+1);
            }
         }
         // End key: Go to the last file shown on the screen
         else if (userInput == KEY_END)
         {
            // Calculate the bottom file index and bottom screen row so that
            // we don't go out of the array bounds.
            var bottomFileIndex = topFileIndex + (numPageLines-1);
            var bottomScreenRow = listBottomRow;
            if (bottomFileIndex >= files.length)
            {
               bottomFileIndex = files.length-1;
               bottomScreenRow = listTopRow + (bottomFileIndex-topFileIndex);
            }
            if ((selectedIndex < bottomFileIndex) && (curpos.y < listBottomRow))
            {
               // Draw the current file information in normal colors.
               console.gotoxy(1, curpos.y);
               writeFileInfo(files[selectedIndex], false, selectedIndex+1);
               // Go to the top index on the screen, and draw that file's
               // information in the highlight color.
               selectedIndex = bottomFileIndex;
               curpos.y = bottomScreenRow;
               console.gotoxy(1, curpos.y);
               writeFileInfo(files[selectedIndex], true, selectedIndex+1);
            }
         }
         // Enter: Allow the user to view/download the highlighted file
         else if (userInput == KEY_ENTER)
         {
            // Place the cursor at the desired location and do file # input.
            console.gotoxy(1, console.screen_rows);
            console.clearline("n");
            var retObj = doFileNumInput(files, pDir, listFiles_Lightbar, selectedIndex+1);
            // doFileNumInput() fixes file path slashes for the chosen file,
            // so if the user chose a file, then update fileSlashesFixed
            // for the chosen array index.
            if (retObj.arrayIndex > -1)
               fileSlashesFixed[retObj.arrayIndex] = true;
            // Re-draw the screen so that everything looks correct.
            drawTopAndBottom_Lightbar(pArchiveFilename);
            listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                    selectedIndex, true, true);
         }
         // Note: I was hoping to let users add files to their batch download
         // queue, but it looks like files have to be in Synchronet's file
         // database in order for that to work.  :(  So I've commented out
         // the code I wrote for adding to the batch download queue.
         /*
         // B: Add the selected file to the user's batch download queue
         else if (userInput == "B")
         {
            // If this file hasn't had its slashes fixed, then fix them.
            if (!fileSlashesFixed[selectedIndex])
            {
               files[selectedIndex] = fixPathSlashes(files[selectedIndex]);
               fileSlashesFixed[selectedIndex] = true;
            }
            // Go to the last line on the screen (for prompting/messages)
            console.gotoxy(1, console.screen_rows);
            console.clearline("n");
            // If the entry is a directory, then report an error.  Otherwise,
            // go to the bottom and confirm with the user if they really
            // want to download the file.
            if (file_isdir(files[selectedIndex]))
            {
               console.print("nwhCan't download an entire directory.");
               mswait(1500);
               // Re-draw the bottom help line
               console.gotoxy(1, console.screen_rows);
               displayBottomHelpLine_Lightbar();
            }
            else
            {
               var justFilename = getFilenameFromPath(files[selectedIndex]);
               var filenameWidth = console.screen_columns - 23;
               if (console.yesno("ncAdd h" +
                                 justFilename.substr(0, filenameWidth) +
                                 "nc to batch DL queue"))
               {
                  // Create the temporary batch download directory and copy
                  // the selected file to it.  If that succeeded, then add
                  // the file to the user's download queue.
                  mkdir(gBatchDLDir);
                  var DLFilename = gBatchDLDir + justFilename;
                  if (file_copy(files[selectedIndex], DLFilename))
                  {
                     // Add the file to the DL queue.  If failed, show an error.
                     console.gotoxy(1, console.screen_rows);
                     console.clearline("n");
                     if (addFileToBatchQueue(DLFilename))
                        console.print("ncFile added to batch DL queue.");
                     else
                     {
                        console.print("nyhUnable to add to batch DL queue!");
                        file_remove(DLFilename);
                     }
                     mswait(1500);
                  }
                  else
                  {
                     // Couldn't copy the file to the temporary DL queue directory.
                     console.gotoxy(1, console.screen_rows);
                     console.clearline("n");
                     console.print("nyhUnable to copy to queue directory!");
                     mswait(1500);
                  }
               }

               // Re-draw the screen so that everything looks correct.
               drawTopAndBottom_Lightbar(pArchiveFilename);
               listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                       selectedIndex, true, true);
            }
         }
         */
         // D: Download the selected file
         else if (userInput == "D")
         {
            // Go to the last line on the screen (for prompting/messages)
            console.gotoxy(1, console.screen_rows);
            console.clearline("n");
            // If the entry is a directory, then report an error.  Otherwise,
            // go to the bottom and confirm with the user if they really
            // want to download the file.
            if (file_isdir(files[selectedIndex]))
            {
               console.print("nwhCan't download an entire directory.");
               mswait(1500);
               // Re-draw the bottom help line
               console.gotoxy(1, console.screen_rows);
               displayBottomHelpLine_Lightbar();
            }
            else
            {
               var filenameWidth = console.screen_columns - 24;
               if (console.yesno("ncDownload h" +
                                 getFilenameFromPath(files[selectedIndex]).substr(0, filenameWidth) +
                                 "nc: Are you sure"))
               {
                  bbs.log_str(user.alias + " is downloading a file from an archive: " +
                              files[selectedIndex]);
                  bbs.send_file(files[selectedIndex]);
               }

               // Re-draw the screen so that everything looks correct.
               drawTopAndBottom_Lightbar(pArchiveFilename);
               listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                       selectedIndex, true, true);
            }
         }
         // Q or ESC key: Quit
         else if ((userInput == "Q") || (userInput == ESC_KEY))
         {
            continueOn = false;
            break;
         }
         // ?: Display help
         else if (userInput == "?")
         {
            showHelpScreen(true);
            // Re-draw the screen so that everything looks correct.
            drawTopAndBottom_Lightbar(pArchiveFilename);
            listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                    selectedIndex, true, true);
         }
         // V: View the current file
         else if (userInput == "V")
         {
            // If this file hasn't had its slashes fixed, then fix them.
            if (!fileSlashesFixed[selectedIndex])
            {
               files[selectedIndex] = fixPathSlashes(files[selectedIndex]);
               fileSlashesFixed[selectedIndex] = true;
            }

            var filename = getFilenameFromPath(files[selectedIndex]);
            // If the file is a directory, then just view it.  Otherwise,
            // go to the bottom and confirm with the user if they really
            // want to view the file.
            var viewIt = false;
            if (file_isdir(files[selectedIndex]))
               viewIt = true;
            else
            {
               console.gotoxy(1, console.screen_rows);
               console.clearline("n");
               var confirmText = "ncView h"
                                + filename.substr(0, console.screen_columns-15) + "n"
               viewIt = console.yesno(confirmText);
            }
            if (viewIt)
            {
               // If the entry is a directory, then call this function again
               // to look in there.  Otherwise, call viewFile() to view it.
               if (file_isdir(files[selectedIndex]))
                  listFiles_Lightbar(files[selectedIndex], filename);
               else
               {
                  bbs.log_str(user.alias + " is viewing a file inside an archive: " +
                              files[selectedIndex]);
                  viewFile(files[selectedIndex], pDir, listFiles_Lightbar);
               }
            }

            // Re-draw the screen so that everything looks correct.
            drawTopAndBottom_Lightbar(pArchiveFilename);
            listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                    selectedIndex, true, true);
         }
         // N: Next page
         else if (userInput == "N")
         {
            // Note: curpos is updated by goToNextPage(), so we don't have to
            // update it here.
            if (goToNextPage())
            {
               listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                       selectedIndex, true, true);
            }
         }
         // P: Previous page
         else if (userInput == "P")
         {
            // Note: curpos is updated by goToNextPage(), so we don't have to
            // update it here.
            if (goToPrevPage())
            {
               listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                       selectedIndex, true, true);
            }
         }
         // F: First page
         else if (userInput == "F")
         {
            // If we're not already on the first page, then go there.
            if (topFileIndex > 0)
            {
               topFileIndex = 0;
               selectedIndex = 0;
               console.y = listTopRow;
               listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                       selectedIndex, true, true);
            }
         }
         // L: Last page
         else if (userInput == "L")
         {
            // If we're not already on the last page, then go there.
            if (topFileIndex < lastPageTopIndex)
            {
               topFileIndex = lastPageTopIndex;
               selectedIndex = lastPageTopIndex;
               console.y = listTopRow;
               listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                       selectedIndex, true, true);
            }
         }
         // Numeric digit: The start of a file number
         else if (userInput.match(/[0-9]/))
         {
            // Put the user's input back in the input buffer to
            // be used for getting the rest of the message number.
            console.ungetstr(userInput);
            console.gotoxy(1, console.screen_rows);
            console.clearline("n");
            console.print("cFile #: h");
            var retObj = doFileNumInput(files, pDir, listFiles_Lightbar);
            // doFileNumInput() fixes file path slashes for the chosen file,
            // so if the user chose a file, then update fileSlashesFixed
            // for the chosen array index.
            if (retObj.arrayIndex > -1)
               fileSlashesFixed[retObj.arrayIndex] = true;
            // Refresh the screen.
            drawTopAndBottom_Lightbar(pArchiveFilename);
            listFiles_ShowScreenful(files, topFileIndex, listTopRow, listBottomRow,
                                    selectedIndex, true, true);
         }
      }
   }
   else
      console.print("ncCould not extract/view the file, or it is empty.\r\np");

   return onePastLastFileRow;
}

// Displays the help line at the bottom of the screen (for lightbar mode).
function displayBottomHelpLine_Lightbar()
{
   // Construct the help text variable only once (for speed).
   if (displayBottomHelpLine_Lightbar.bottomHelp == undefined)
   {
      displayBottomHelpLine_Lightbar.bottomHelp = "n7r"
            + UP_ARROW_DISPLAY + "b, r" + DOWN_ARROW_DISPLAY
            + "b, rHOMEb, rENDb, rENTERb, rNm)bext, "
            + "rPm)brev, rFm)birst, rLm)bast, "
            + "rVm)biew, rDm)bL, rQm)buit, "
            + "r#b, r?   ";
   }

   // Go to the last row on the screen and display the help line
   console.gotoxy(1, console.screen_rows);
   console.print(displayBottomHelpLine_Lightbar.bottomHelp);
}

// Writes the top and bottom lines for the file list in lightbar mode.
//
// Parameters:
//  pFilename: The name of the file/dir being displayed.
function drawTopAndBottom_Lightbar(pFilename)
{
   // Clear the screen, display the help line at the bottom of the screen,
   // and output the file list headers.
   console.clear("n");
   displayBottomHelpLine_Lightbar();
   console.gotoxy(1, 1);
   writeFileListHeader(pFilename);
}

// Displays a screenful of file information.
//
// Parameters:
//  pArray: The array of files in the directory
//  pStartIndex: The index in the array to start at
//  pStartLine: The line on the screen to start displaying at
//  pEndLine: The last line on the screen to display at
//  pHighlightIndex: The index of the entry to show in highlight colors. Optional.
//  pClearToEOS: Whether or not to clear to the end of the screen, if the list doesn't
//               fill up the screen.  Optional; defaults to false.
//  pLightbar: Whether or not we're in lightbar mode.  If so, this will list
//             the file without the file number.
function listFiles_ShowScreenful(pArray, pStartIndex, pStartLine, pEndLine, pHighlightIndex,
                                  pClearToEOS, pLightbar)
{
   // Parameter validation
   if ((pStartIndex < 0) || (pStartIndex >= pArray.length))
      return;
   if ((pStartLine < 1) || (pStartLine > console.screen_rows))
      return;
   if (pEndLine < pStartLine)
      pEndLine = pStartLine + 1;
   if (pEndLine > console.screen_rows-1)
      pEndLine = console.screen_rows-1;

   // Set local versions of the optional variables.
   var highlightIndex = -1;
   if ((pHighlightIndex != null) && (pHighlightIndex != undefined))
      highlightIndex = pHighlightIndex;
   // Note: Only allow clearing to the end of the screenful if the
   // user's terminal supports ANSI.
   var clearToEOS = false;
   if ((pClearToEOS != null) && (pClearToEOS != undefined))
      clearToEOS = pClearToEOS && gANSISupported;

   // Figure out the last index in the array that can be displayed
   var onePastLastIndex = pStartIndex + (pEndLine - pStartLine) + 1;
   if (onePastLastIndex > pArray.length)
      onePastLastIndex = pArray.length;

   // Display the file information
   var screenLine = pStartLine;
   console.gotoxy(1, screenLine);
   for (var i = pStartIndex; i < onePastLastIndex; ++i)
   {
      // If the screen line is beyond the last line we want to display
      // at, then stop listing the file.
      if (screenLine > pEndLine)
         break;

      writeFileInfo(pArray[i], (highlightIndex == i), i+1);
      console.crlf();

      ++screenLine;
   }
   // Edge case: If we haven't reached the last screen line yet and
   // clearToEOS is true, then blank out the rest of the lines to the
   // end of the "screen".
   if (clearToEOS && (screenLine < pEndLine))
   {
      console.print("n");
      const onePastLastLine = console.screen_rows;
      const clearWidth = console.screen_columns - 1;
      const formatStr = "%" + clearWidth + "s";
      for (var line = screenLine; line < onePastLastLine; ++line)
      {
         console.gotoxy(1, line);
         printf(formatStr, "");
      }
   }
}

// Writes information about a file on the screen.
//
// Parameters:
//  pFilename: The full path & name of the file
//  pHighlight: Whether or not to use highlight colors.
//  pFileNum: The file number (1-based)
function writeFileInfo(pFilename, pHighlight, pFileNum)
{
   // Create the format strings (but only once, for speed)
   // Format widths
   var sizeWidth = 8;
   var dateWidth = 10;
   var timeWidth = 5;
   // Filename width
   if (writeFileInfo.filenameWidth == undefined)
   {
      writeFileInfo.filenameWidth = console.screen_columns - sizeWidth - dateWidth
                                  - timeWidth - 9;
   }
   if (writeFileInfo.formatStr == undefined)
   {
      writeFileInfo.formatStr = "n" + gGenConfig.colors.fileNums + "%4d "
                  + gGenConfig.colors.fileSize + "%" + sizeWidth + "s "
                  + gGenConfig.colors.fileDate + "%-" + dateWidth + "s "
                  + gGenConfig.colors.fileTime + "%-" + timeWidth
                  + "s " + gGenConfig.colors.filename + "%-" + writeFileInfo.filenameWidth
                  + "s";
   }
   if (writeFileInfo.hiFormatStr == undefined)
   {
      writeFileInfo.hiFormatStr = "n" + gGenConfig.colors.highlightedFile + "%4d %"
                  + sizeWidth + "s %-" + dateWidth + "s %-" + timeWidth + "s %-"
                  + writeFileInfo.filenameWidth + "s";
   }

   // Get just the filename from the end of pFilename.
   var filename = getFilenameFromPath(pFilename);
   // If the entry is a directory, then display it as such.
   // Otherwise, output the file information.
   if (file_isdir(pFilename))
   {
      printf(pHighlight ? writeFileInfo.hiFormatStr : writeFileInfo.formatStr,
             pFileNum,                            // File number column
             "<DIR>",                             // File size column
             "",                                  // File date column
             "",                                  // File time column
             filename.substr(0, writeFileInfo.filenameWidth)); // Directory name
   }
   else
   {
      var fileDate = file_date(pFilename);
      var fileSizeBytes = file_size(pFilename);
      // Display the file size in gigabytes, megabytes, or kilobytes, if
      // it's large enough.
      var fileSizeStr = "";
      if (fileSizeBytes >= 1073741824)   // Gigabyte size
         fileSizeStr = format("%6.2fG", (fileSizeBytes / 1073741824));
      else if (fileSizeBytes >= 1048576) // Megabyte size
         fileSizeStr = format("%6.2fM", (fileSizeBytes / 1048576));
      else if (fileSizeBytes >= 1024)    // Kilobyte size
         fileSizeStr = format("%6.2fK", (fileSizeBytes / 1024));
      else // Use the byte size
         fileSizeStr = format("%d", fileSizeBytes).substr(0, 10);

      // Display the file information.
      printf(pHighlight ? writeFileInfo.hiFormatStr : writeFileInfo.formatStr,
             pFileNum,                            // File number
             fileSizeStr,                         // File size
             strftime("%Y-%m-%d", fileDate),      // File date
             strftime("%H:%M", fileDate),         // File time
             filename.substr(0, writeFileInfo.filenameWidth)); // Filename
   }
}

// This function executes an OS command and returns its output as an
// array of strings.  The reason this function was written is that
// system.popen() is only functional in UNIX.
//
// Parameters:
//  pCommand: The command to execute
//
// Return value: An array of strings containing the program's output.
function execCmdWithOutput(pCommand)
{
   var cmdOutput = new Array();

   if ((pCommand == undefined) || (pCommand == null) || (typeof(pCommand) != "string"))
      return cmdOutput;

   // Execute the command and redirect the output to a file in the
   // node's directory.  If system.exec() returns 0, that means success.
   const tempFilename = system.node_dir + "DDArcViewerOutput.temp";
   if (system.exec(pCommand + " >" + tempFilename) == 0)
   {
      // Read the temporary file and populate cmdOutput with its
      // contents.
      var tempFile = new File(tempFilename);
      if (tempFile.open("r"))
      {
         if (tempFile.length > 0)
         {
            var fileLine = "";
            while (!tempFile.eof)
            {
               fileLine = tempFile.readln(2048);

               // fileLine should be a string, but I've seen some cases
               // where it isn't, so check its type.
               if (typeof(fileLine) != "string")
                  continue;

               cmdOutput.push(fileLine);
            }
         }
      }
      tempFile.close();
   }

   // Remove the temporary file, if it exists.
   if (file_exists(tempFilename))
      file_remove(tempFilename);

   return cmdOutput;
}

// Removes multiple, leading, and/or trailing spaces
// The search & replace regular expressions used in this
// function came from the following URL:
//  http://qodo.co.uk/blog/javascript-trim-leading-and-trailing-spaces
//
// Parameters:
//  pString: The string to trim
//  pLeading: Whether or not to trim leading spaces (optional, defaults to true)
//  pMultiple: Whether or not to trim multiple spaces (optional, defaults to true)
//  pTrailing: Whether or not to trim trailing spaces (optional, defaults to true)
function trimSpaces(pString, pLeading, pMultiple, pTrailing)
{
	var leading = true;
	var multiple = true;
	var trailing = true;
	if (typeof(pLeading) != "undefined")
		leading = pLeading;
	if (typeof(pMultiple) != "undefined")
		multiple = pMultiple;
	if (typeof(pTrailing) != "undefined")
		trailing = pTrailing;
		
	// To remove both leading & trailing spaces:
	//pString = pString.replace(/(^\s*)|(\s*$)/gi,"");

	if (leading)
		pString = pString.replace(/(^\s*)/gi,"");
	if (multiple)
		pString = pString.replace(/[ ]{2,}/gi," ");
	if (trailing)
		pString = pString.replace(/(\s*$)/gi,"");

	return pString;
}

// Returns a filename's extension.  Always returns a string.
//
// Parameters:
//  pFilename: The name of a file
//
// Return value: The filename's extension, or blank if there is none.
function getFilenameExtension(pFilename)
{
   const filenameUpper = pFilename.toUpperCase();
   var filenameExt = "";
   // Special case for .tar.gz - Report tar.gz as an extension
   // rather than just gz
   if (/.TAR.GZ$/.test(filenameUpper))
      filenameExt = "TAR.GZ";
   else
   {
      // Look for the last period in filenameUpper
      var dotIndex = filenameUpper.lastIndexOf(".");
      if (dotIndex > -1)
         filenameExt = filenameUpper.substr(dotIndex+1);
   }
   return filenameExt;
}

// Unit test function for getFilenameExtension()
function test_getFilenameExtension()
{
   console.crlf();
   if (getFilenameExtension("file.zip") != "ZIP")
      console.print("nyhtest_getFilenameExtension(): Test 1 failed\r\n");

   if (getFilenameExtension("file.7z") != "7Z")
      console.print("nyhtest_getFilenameExtension(): Test 2 failed\r\n");

   if (getFilenameExtension("file.tar.gz") != "TAR.GZ")
      console.print("nyhtest_getFilenameExtension(): Test 3 failed\r\n");

   if (getFilenameExtension("file") != "")
      console.print("nyhtest_getFilenameExtension(): Test 4 failed\r\n");

   if (getFilenameExtension("file.") != "")
      console.print("nyhtest_getFilenameExtension(): Test 5 failed\r\n");
}

// This function returns just the filename from the end of a full path, regardless
// of whether it has a trailing slash.
//
// Parameters:
//  pFilename: The full path & filename
//
// Return value: Just the filename from the end of the path
function getFilenameFromPath(pFilename)
{
   var filename = pFilename;
   if (filename.length > 0)
   {
      // If the filename has a trailing slash, remove it.  Then,
      // use file_getname() to get the filename from the end.
      if ((/\/$/.test(filename)) || (/\\$/.test(filename)))
         filename = filename.substr(0, filename.length-1);
      filename = file_getname(filename);
   }
   return filename;
}

// Given a full path & filename, this function returns just the path portion,
// with a trailing slash.
//
// Parameters:
//  pFilename: A filename with the full path
//
// Return value: Just the path portion of the filename.
function getPathFromFilename(pFilename)
{
   // Make sure pFilename is valid
   if ((pFilename == null) || (pFilename == undefined))
      return "";
   if (typeof(pFilename) != "string")
      return "";
   if (pFilename.length == 0)
      return "";

   // Make sure the filename has the correct slashes for
   // the platform.
   var filename = fixPathSlashes(pFilename);

   // If pFilename is actually a directory, then just return it.
   if (file_isdir(filename))
   {
      // Make sure it has a trailing slash that's appropriate
      // for the OS.
      var lastChar = filename.charAt(filename.length-1);
      if (lastChar != gPathSlash)
         filename += gPathSlash;
      return filename;
   }

   // Find the index of the last slash and use that to extract the path.
   var path = "";
   var lastSlashIndex = filename.lastIndexOf(gPathSlash);
   if (lastSlashIndex > 0)
      path = filename.substr(0, lastSlashIndex);

   // If we extracted the path, make sure it ends with a slash.
   if (path.length > 0)
   {
      var lastChar = path.charAt(path.length-1);
      if (lastChar != gPathSlash)
         path += gPathSlash;
   }

   return path;
}

// Fixes all slashes in a given path to be the appropriate slash
// character for the OS.  Returns a new string with the fixed version.
//
// Parameters:
//  pPath: A path to fix
//
// Return value: The fixed version of pPath
function fixPathSlashes(pPath)
{
   // Make sure pPath is valid.
   if ((pPath == null) || (pPath == undefined))
      return "";
   if (typeof(pPath) != "string")
      return "";
   if (pPath.length == 0)
      return "";

   // Create a variable to store whether or not we're in Windows,
   // but only once (for speed).
   if (fixPathSlashes.inWin == undefined)
      fixPathSlashes.inWin = /^WIN/.test(system.platform.toUpperCase());

   // Fix the slashes and return the fixed version.
   //return(fixPathSlashes.inWin ? pPath.replace("/", "\\") : pPath.replace("\\", "/"));
   var path = pPath;
   if (fixPathSlashes.inWin) // Windows
   {
      while (path.indexOf("/") > -1)
         path = path.replace("/", "\\");
   }
   else // *nix
   {
      while (path.indexOf("\\") > -1)
         path = path.replace("\\", "/");
   }
   return path;
}

// Unit test function for fixPathSlashes
function test_fixPathSlashes()
{
   console.crlf();
   // Windows tests
   if (/^WIN/.test(system.platform.toUpperCase()))
   {
      if (fixPathSlashes("file.zip") != "file.zip")
         console.print("nyhtest_fixPathSlashes(): Test 1 failed\r\n");
   
      if (fixPathSlashes("D:\\path\\file.zip") != "D:\\path\\file.zip")
         console.print("nyhtest_fixPathSlashes(): Test 2 failed\r\n");

      if (fixPathSlashes("D:/path/file.zip") != "D:\\path\\file.zip")
         console.print("nyhtest_fixPathSlashes(): Test 3 failed\r\n");

      if (fixPathSlashes("D:/path\\file.zip") != "D:\\path\\file.zip")
         console.print("nyhtest_fixPathSlashes(): Test 4 failed\r\n");

      if (fixPathSlashes("\\path\\file.zip") != "\\path\\file.zip")
         console.print("nyhtest_fixPathSlashes(): Test 5 failed\r\n");

      if (fixPathSlashes("/path/file.zip") != "\\path\\file.zip")
         console.print("nyhtest_fixPathSlashes(): Test 6 failed\r\n");

      if (fixPathSlashes("/path\\file.zip") != "\\path\\file.zip")
         console.print("nyhtest_fixPathSlashes(): Test 6 failed\r\n");

      if (fixPathSlashes("\\path/file.zip") != "\\path\\file.zip")
         console.print("nyhtest_fixPathSlashes(): Test 5 failed\r\n");
   }
   // *nix tests
   else
   {
   }
}

// Adds a file to the user's batch download queue.  Returns true on
// success or false on failure.
// Note: Despite my hopes, this doesn't work, because a file needs
// to be in Synchronet's file database in order for it to be added
// to the user's download queue!
//
// Parameters:
//  pFilename: The file to add to the user's batch download queue
//
// Return value: true on success, or false on failure
function addFileToBatchQueue(pFilename)
{
   var retval = false;
   // Create the batch list file
   var listFilename = system.node_dir + "DDArcViewer_DLList.txt";
   var listFile = new File(listFilename);
   if (listFile.open("w"))
   {
      // Write to the list file and close it.
      retval = listFile.write(pFilename);
      listFile.close();

      // If we were able to write the filename to the list file, then
      // add the list to the user's download queue.
      if (retval)
         retval = bbs.batch_add_list(listFilename);

      // Delete the list file
      file_remove(listFilename);
   }
   return retval;
}

// Shows the help screen.
//
// Parameters:
//  pLightbarMode: Whether or not the script is using the lightbar interface
function showHelpScreen(pLightbarMode)
{
   // Construct the program name header text lines
   if (showHelpScreen.progInfoHeader == undefined)
   {
      var width = gDDArcViewerProgName.length + 2;
      showHelpScreen.progInfoHeader = new Array();
      // Upper & lower border lines
      showHelpScreen.progInfoHeader[0] = "ch" + UPPER_LEFT_SINGLE;
      showHelpScreen.progInfoHeader[2] = LOWER_LEFT_SINGLE;
      for (var i = 0; i < width; ++i)
      {
         showHelpScreen.progInfoHeader[0] += HORIZONTAL_SINGLE;
         showHelpScreen.progInfoHeader[2] += HORIZONTAL_SINGLE;
      }
      showHelpScreen.progInfoHeader[0] += UPPER_RIGHT_SINGLE;
      showHelpScreen.progInfoHeader[2] += LOWER_RIGHT_SINGLE;
      // Middle section with the program name
      showHelpScreen.progInfoHeader[1] = VERTICAL_SINGLE + "4yh "
                                       + gDDArcViewerProgName + " nch" + VERTICAL_SINGLE;
      // Version & author information
      showHelpScreen.progInfoHeader[3] = "ncVersion g" + gDDArcViewerVersion
                                       + " wh(b" + gDDArcViewerVerDate + "w)";
      showHelpScreen.progInfoHeader[4] = "ncby Eric Oulashin h(ncsysop of Digital Distortion BBSh)";
   }

   // Display the header lines
   console.clear("n");
   for (var i = 0; i < showHelpScreen.progInfoHeader.length; ++i)
      console.center(showHelpScreen.progInfoHeader[i]);

   // Display the program help
   console.crlf();
   console.print("nc" + gDDArcViewerProgName + " lets you list the files inside an archive.\r\n");
   console.print("You can also view or download the files from the archive.");
   console.crlf();
   console.crlf();
   console.print("The following is a list of the command keys:\r\n");
   console.print("khÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\r\n");
   var formatStr = "nch%5sg: nc%s\r\n";
   if (pLightbarMode)
   {
      printf(formatStr, "HOME", "Go to the first file on the page");
      printf(formatStr, "END", "Go to the last file on the page");
      printf(formatStr, UP_ARROW_DISPLAY, "Move up one file");
      printf(formatStr, DOWN_ARROW_DISPLAY, "Move down one file");
   }
   printf(formatStr, "N", "Next page");
   printf(formatStr, "P", "Previous page");
   printf(formatStr, "F", "First page");
   printf(formatStr, "L", "Last page");
   if (pLightbarMode)
   {
      printf(formatStr, "Enter", "Select the currently highlighted file to view/download");
      printf(formatStr, "V", "View the currently highlighted file");
      printf(formatStr, "D", "Download the currently highlighted file");
   }
   else
   {
      printf(formatStr, "V", "View a file (you will be prompted for the file number)");
      printf(formatStr, "D", "Download a file (you will be prompted for the file number)");
   }
   printf(formatStr, "Q/ESC", "Quit");
   console.crlf();
   console.print("ncYou can also choose a file to view/download by typing its number.");
   console.crlf();
   if (!gANSISupported)
      console.crlf();

   // If the user doesn't have their screen pause setting turned on, then
   // pause now.
   if ((user.settings & USER_PAUSE) == 0)
      console.pause();
}

// This function fixes an array of command-line arguments so that
// arguments with spaces in them are a single argument.  This function
// was written by Tracker1 of The Roughnecks BBS - He posted the code
// in the DOVE-Net Synchronet Discussion sub-board on December 20, 2009.
function fixArgs(input)
{
   var patt1 = /\"[^\"]*\"|\S+/g;
   var patt2 = /^\"?([^\"]*)\"?$/;
   return input.join(' ').match(patt1).map(function(item)
   {
     return item.replace(patt2, "$1")
   });
}