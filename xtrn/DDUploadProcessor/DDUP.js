/* Name: Digital Distortion Upload Processor
 *
 * Description: This is a script for Synchronet that scans
 * uploaded files with a virus scanner.  Compressed archives are
 * unpacked so that the files inside can be scanned by the virus
 * scanner.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 * Date       User              Description
 * 2009-12-25-
 * 2009-12-28 Eric Oulashin     Initial development
 * 2009-12-29 Eric Oulashin     Version 1.00
 *                              Initial public release
 */

/* Command-line arguments:
 1 (argv[0]): The name of the file to scan
*/

load("sbbsdefs.js");

// Determine the script's execution directory.
// This code is a trick that was created by Deuce, suggested by Rob
// Swindell as a way to detect which directory the script was executed
// in.  I've shortened the code a little.
// Note: gStartupPath will include the trailing slash.
var gStartupPath = '.';
try { throw dig.dist(dist); } catch(e) { gStartupPath = e.fileName; }
gStartupPath = backslash(gStartupPath.replace(/[\/\\][^\/\\]*$/,''));

load(gStartupPath + "DDUP_Cleanup.js");

// Version information
var gDDUPVersion = "1.00";
var gDDUPVerDate = "2009-12-29";


// If the filename was specified on the command line, then use that
// for the filename.  Otherwise, read the name of the file to view
// from DDArcViewerFilename.txt in the node directory.
var gFileToScan = "";
if (argv.length > 0)
{
   if (typeof(argv[0]) == "string")
   {
      // Make sure the arguments are correct (in case they have spaces),
      // then use the first one.
      var fixedArgs = fixArgs(argv);
      if ((typeof(fixedArgs[0]) == "string") && (fixedArgs[0].length > 0))
         gFileToScan = fixedArgs[0];
      else
      {
         console.print("nyhError: ncBlank filename argument given.\r\np");
         exit(-2);
      }
   }
   else
   {
      console.print("nyhError: ncUnknown command-line argument specified.\r\np");
      exit(-1);
   }
}
else
{
   // Read the filename from DDArcViewerFilename.txt in the node directory.
   // This is a workaround for file/directory names with spaces in
   // them, which would get separated into separate command-line
   // arguments for JavaScript scripts.
   var filenameFileFilename = system.node_dir + "DDArcViewerFilename.txt";
   var filenameFile = new File(filenameFileFilename);
   if (filenameFile.open("r"))
   {
      if (!filenameFile.eof)
         gFileToScan = filenameFile.readln(2048);
      filenameFile.close();
   }
}

// Make sure the slashes in the filename are correct for the platform.
if (gFileToScan.length > 0)
   gFileToScan = fixPathSlashes(gFileToScan);

// A filename must be specified as the first argument, so give an error and return
// if not.
if (gFileToScan.length == 0)
{
   console.print("nyhError: ncNo filename specified to process.\r\np");
   exit(1);
}

// Create the global configuration objects.
var gGenCfg = new Object();
gGenCfg.scanCmd = "";
gGenCfg.skipScanIfSysop = false;
gGenCfg.pauseAtEnd = false;
var gFileTypeCfg = new Object();

// Read the configuration files to populate the global configuration object.
var configFileRead = ReadConfigFile(gStartupPath);
// If the configuration files weren't read, then output an error and exit.
if (!configFileRead)
{
   console.print("nyhError: ncUpload processor is unable to read its\r\n");
   console.print("configuration files.\r\np");
   exit(2);
}
// Exit if there is no scan command.
if (gGenCfg.scanCmd.length == 0)
{
   console.print("nyhWarning: ncNo scan command configured for the upload processor.\r\n");
   exit(0);
}

// Global variables
// Strings for the OK and failure symbols
var gOKStr = "nkh[ngûkh]n";
var gOKStrWithNewline = gOKStr + "\r\n";
var gFailStr = "nkh[rXk]n";
var gFailStrWithNewline = gFailStr + "\r\n";
// Stuff for the printf formatting string for the status messages
var gStatusTextLen = 79 - console.strlen(gOKStr); // gOKStr and gFailStr should have the same length
var gStatusPrintfStr = "n%s%-" + gStatusTextLen + "sn"; // For a color and the status text

// Now, scan the file and return the appropriate return code.
exit(main());
// End of script execution.


// This is the "main" function that contains the main code
// for the script.
//
// Return value: An integer to return upon script exit.
function main()
{
   // Output the program name & version information
   console.print("n\r\nchDncigital hDncistortion hUncpload hPncrocessor whvng" +
                 gDDUPVersion);
   // Originally I had this script output the version date, but now I'm not sure
   // if I want to do that..
   //console.print(" wh(b" + gDDUPVerDate + "w)");
   console.print("n");
   console.crlf();

   // Process the file
   var exitCode = processFile(gFileToScan);
   // Depending on the exit code, display a success or failure message.
   console.crlf();
   if (exitCode == 0)
      console.print(gOKStr + " nbhScan successful - The file passed.\r\n");
   else
      console.print(gFailStr + " nyhScan failed!\r\n");

   // If the option to pause at the end is enabled, then prompt the user for
   // a keypress.
   if (gGenCfg.pauseAtEnd)
   {
      console.print("nwhPress any key to continue:n");
      console.getkey(K_NOECHO);
   }

   return exitCode;
}


//////////////////////////////////////////////////////////////////////////////////
// Object stuff

// Constructor for the ScannableFile object, which contains information
// about a viewable file.
//
// Parameters:
//  pExtension: The filename extension
//  pViewCmd: The OS command to view it
//
// The ScannableFile object contains the following properties:
//  extension: The filename extension
//  pExtractCmd: The OS command to extract it (if applicable)
//  pScanOption: A string containing a scan option.  The following are valid:
//               "scan": Always scan the file using the scan commdn
//               "always pass": Don't scan the file, and assume it's good
//               "always fail": Don't scan the file, and assume it's bad
function ScannableFile(pExtension, pExtractCmd, pScanOption)
{ 
   this.extension = "";      // The archive filename extension
   this.extractCmd = "";     // The command to extract the archive (if applicable)
   this.scanOption = "scan"; // The scan option ("scan", "always pass", "always fail")

   // If the parameters are valid, then use them to set the object properties.
   if ((pExtension != null) && (pExtension != undefined) && (typeof(pExtension) == "string"))
      this.extension = pExtension;
   if ((pExtractCmd != null) && (pExtractCmd != undefined) && (typeof(pExtractCmd) == "string"))
      this.extractCmd = pExtractCmd;
   if ((pScanOption != null) && (pScanOption != undefined) && (typeof(pScanOption) == "string"))
      this.scanOption = pScanOption;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Functions

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

// Scans a file.
//
// Parameters:
//  pFilename: The name of the file to scan
//
// Return value: A return code from scanning the file.  0 means success;
//               non-zero means failure.
function processFile(pFilename)
{
   // Display the program header stuff - The name of the file being scanned
   // and the status header line
   var justFilename = getFilenameFromPath(pFilename);
   console.print("nwhScanning b" + justFilename.substr(0, 70));
   console.print("n\r\nb7                             File Scan Status                                  n\r\n");

   // If the skipScanIfSysop option is enabled and the user is a sysop,
   // then assume the file is good.
   if (gGenCfg.skipScanIfSysop && user.compare_ars("SYSOP"))
   {
      printf(gStatusPrintfStr, "gh", "Auto-approving the file (you're a sysop)");
      console.print(gOKStrWithNewline);
      return 0;
   }

   var retval = 0;

   // Look for the file extension in gFileTypeCfg to get the file scan settings.
   // If the file extension is not there, then go ahead and scan it (to be on the
   // safe side).
   var filenameExtension = getFilenameExtension(pFilename);
   if (typeof(gFileTypeCfg[filenameExtension]) != "undefined")
   {
      if (gFileTypeCfg[filenameExtension].scanOption == "scan")
      {
         // - If the file has an extract command, then:
         //   Extract the file to a temporary directory in the node dir
         //   For each file in the directory:
         //     If it's a subdir
         //        Recurse into it
         //     else
         //        Scan it for viruses
         //        If non-zero retval
         //           Return with error code
         var filespec = pFilename;
         if (gFileTypeCfg[filenameExtension].extractCmd.length > 0)
         {
            // Create the base work directory for this script in the node dir.
            // And just in case that dir already exists, remove it before
            // creating it.
            var baseWorkDir = system.node_dir + "DDUploadProcessor_Temp";
            deltree(baseWorkDir + "/");
            if (!mkdir(baseWorkDir))
            {
               console.print("nyhWarning: nwh Unable to create the work dir.n\r\n");
               retval = -1;
            }
            
            // If all is okay, then create the directory in the temporary work dir.
            var workDir = baseWorkDir + "/" + justFilename + "_temp";
            if (retval == 0)
            {
               deltree(workDir + "/");
               if (!mkdir(workDir))
               {
                  console.print("nyhWarning: nwh Unable to create a dir in the temporary work dir.n\r\n");
                  retval = -1;
               }
            }

            // If all is okay, we can now process the file.
            if (retval == 0)
            {
               // Extract the file to the work directory
               printf(gStatusPrintfStr, "mh", "Extracting the file...");
               var errorStr = extractFileToDir(pFilename, workDir);
               if (errorStr.length == 0)
               {
                  console.print(gOKStrWithNewline);
                  // Scan the files in the work directory.
                  printf(gStatusPrintfStr, "r", "Scanning files inside the archive for viruses...");
                  var retObj = scanFilesInDir(workDir);
                  retval = retObj.returnCode;
                  if (retObj.returnCode == 0)
                     console.print(gOKStrWithNewline);
                  else
                  {
                     console.print(gFailStrWithNewline);
                     console.print("nyhVirus scan failed.  Scan output:n\r\n");
                     for (var index = 0; index < retObj.cmdOutput.length; ++index)
                     {
                        console.print(retObj.cmdOutput[index]);
                        console.crlf();
                     }
                  }
               }
               else
               {
                  console.print(gFailStrWithNewline);
                  // Scan the files in the work directory.
                  console.print("nyhWarning: nwh Unable to extract to work dir.n\r\n");
                  retval = -2;
               }
            }
            // Remove the work directory.
            deltree(baseWorkDir + "/");
         }
         else
         {
            // The file has no extract command, so just scan it.
            printf(gStatusPrintfStr, "bh", "Scanning...");
            var scanCmd = gGenCfg.scanCmd.replace("%FILESPEC%", "\"" + fixPathSlashes(pFilename) + "\"");
            // Run the scan command and capture its output, in case the scan fails.
            var retObj = runExternalCmdWithOutput(scanCmd);
            retval = retObj.returnCode;
            if (retObj.returnCode == 0)
               console.print(gOKStrWithNewline);
            else
            {
               console.print(gFailStrWithNewline);
               console.print("nyhVirus scan failed.  Scan output:n\r\n");
               for (var index = 0; index < retObj.cmdOutput.length; ++index)
               {
                  console.print(retObj.cmdOutput[index]);
                  console.crlf();
               }
            }
         }
      }
      else if (gFileTypeCfg[filenameExtension].scanOption == "always fail")
         exitCode = 10;
   }
   else
   {
      // There's nothing configured for the file's extension, so just scan it.
      printf(gStatusPrintfStr, "r", "Scanning...");
      var scanCmd = gGenCfg.scanCmd.replace("%FILESPEC%", "\"" + fixPathSlashes(pFilename) + "\"");
      var retObj = runExternalCmdWithOutput(scanCmd);
      retval = retObj.returnCode;
      if (retObj.returnCode == 0)
         console.print(gOKStrWithNewline);
      else
      {
         console.print(gFailStrWithNewline);
         console.print("nyhVirus scan failed.  Scan output:n\r\n");
         for (var index = 0; index < retObj.cmdOutput.length; ++index)
         {
            console.print(retObj.cmdOutput[index]);
            console.crlf();
         }
      }
   }

   return retval;
}

// Recursively scans the files in a directory using the scan command in
// gGencfg.
//
// Parameters:
//  pDir: The directory to scan
//
// Return value: 0 on success, or non-zero on error.
// Return value: An object containing the following properties:
//               returnCode: The return code of the last scan command called in the
//                           OS (0 is good, non-zero is failure).
//               cmdOutput: An array of strings containing the output from the last
//                          file scan.
function scanFilesInDir(pDir)
{
   // If pDir is unspecified, then just return.
   if (typeof(pDir) != "string")
   {
      var retObj = new Object();
      retObj.cmdOutput = new Array();
      retObj.returnCode = -1;
      return retObj;
   }
   if (pDir.length == 0)
   {
      var retObj = new Object();
      retObj.cmdOutput = new Array();
      retObj.returnCode = -2;
      return retObj;
   }
   // Also, just return if gGenCfg.scanCmd is blank.
   if (gGenCfg.scanCmd.length == 0)
   {
      var retObj = new Object();
      retObj.cmdOutput = new Array();
      retObj.returnCode = -3;
      return retObj;
   }

   // If the filename has a trailing slash, remove it.
   if ((/\/$/.test(pDir)) || (/\\$/.test(pDir)))
      pDir = pDir.substr(0, pDir.length-1);

   var retObj = null;  // Will be used to capture the return from the scan commands

   // If the virus scan command contains %FILESPEC%, then
   // replace %FILESPEC% with pDir and run the scan command.
   if (gGenCfg.scanCmd.indexOf("%FILESPEC%") > -1)
   {
      var scanCmd = gGenCfg.scanCmd.replace("%FILESPEC%", "\"" + fixPathSlashes(pDir) + "\"");
      retObj = runExternalCmdWithOutput(scanCmd);

      // This is old code, for scanning each file individually (slow):
      /*
      // Get a list of the files, and scan them.
      var files = directory(pDir + "/*");
      if (files.length > 0)
      {
         var scanCmd = null; // Will be used for the scan commands (string)
         var counter = 0;    // Loop variable
         for (var i in files)
         {
            // If the file is a directory, then recurse into it.  Otherwise,
            // scan the file using the configured scan command.
            if (file_isdir(files[i]))
               retObj = scanFilesInDir(files[i]);
            else
            {
               scanCmd = gGenCfg.scanCmd.replace("%FILESPEC%", "\"" + fixPathSlashes(files[i]) + "\"");
               // Run the scan command and capture its output, in case the scan fails.
               retObj = runExternalCmdWithOutput(scanCmd);
            }
   
            // If there's a problem, then stop going through the list of files.
            if (retObj.returnCode != 0)
               break;
         }
      }
      else
      {
         // There are no files.  So create retObj with default settings
         // for a good result.
         retObj = new Object();
         retObj.returnCode = 0;
         retObj.cmdOutput = new Array();
      }
      */
   }
   else
   {
      // gGenCfg.scanCmd doesn't contain %FILESPEC%, so set up
      // retObj with a non-zero return code (for failure)
      retObj = new Object();
      retObj.returnCode = -4;
      retObj.cmdOutput = new Array();
      retObj.cmdOutput.push("The virus scanner is not set up correctly.");
   }

   return retObj;
}

// Reads the configuration file and returns an object containing the
// configuration settings.
//
// Parameters:
//  pCfgFilePath: The path from which to load the configuration file.
//
// Return value: Boolean - Whether or not the configuration was read.
function ReadConfigFile(pCfgFilePath)
{
   // Read the file type settings.
   var fileTypeSettingsRead = false;
   var fileTypeCfgFile = new File(pCfgFilePath + "DDUPFileTypes.cfg");
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
         var scannableFile = null;  // Will be used to create & store scannable file options
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
               // 1. If scannableFile is not null, then add it to gScannableFileTypes.
               // 2. Create a new one (referenced as scannableFile).
               if (ext != filenameExt)
               {
                  if ((scannableFile != null) && (scannableFile != undefined) &&
                      (filenameExt.length > 0))
                  {
                     gFileTypeCfg[filenameExt] = scannableFile;
                  }
                  filenameExt = ext;
                  scannableFile = new ScannableFile(ext, "", "scan");
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

               if (option == "EXTRACT")
                  scannableFile.extractCmd = optionValue;
               else if (option == "SCANOPTION")
                  scannableFile.scanOption = optionValue;
            }
         }
      }

      fileTypeCfgFile.close();
   }

   // Read the general program configuration
   var genSettingsRead = false;
   var genCfgFile = new File(pCfgFilePath + "DDUP.cfg");
   if (genCfgFile.open("r"))
   {
      if (genCfgFile.length > 0)
      {
         genSettingsRead = true;
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
               value = trimSpaces(fileLine.substr(equalsPos+1), true, false, true);

               // Skip this one if the value is blank.
               if (value.length == 0)
                  continue;

               // Set the appropriate value in the settings object.
               if (settingUpper == "SCANCMD")
                  gGenCfg.scanCmd = value;
               else if (settingUpper == "SKIPSCANIFSYSOP")
                  gGenCfg.skipScanIfSysop = (value.toUpperCase() == "YES");
               else if (settingUpper == "PAUSEATEND")
                  gGenCfg.pauseAtEnd = (value.toUpperCase() == "YES");
            }
         }

         genCfgFile.close();
      }
   }

   return (fileTypeSettingsRead && genSettingsRead);
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

   // Determine which slash character to use for paths, depending
   // on the OS.
   if (getPathFromFilename.inWin == undefined)
      getPathFromFilename.inWin = /^WIN/.test(system.platform.toUpperCase());
   var pathSlash = (getPathFromFilename.inWin ? "\\" : "/");

   // Make sure the filename has the correct slashes for
   // the platform.
   var filename = fixPathSlashes(pFilename);

   // If pFilename is actually a directory, then just return it.
   if (file_isdir(filename))
   {
      // Make sure it has a trailing slash that's appropriate
      // for the OS.
      var lastChar = filename.charAt(filename.length-1);
      if ((lastChar != "/") || (lastChar == "\\"))
         filename += pathSlash;
      return filename;
   }

   // Find the index of the last slash and use that to extract the path.
   var path = "";
   var lastSlashIndex = filename.lastIndexOf(pathSlash);
   if (lastSlashIndex > 0)
      path = filename.substr(0, lastSlashIndex);

   // If we extracted the path, make sure it ends with a slash.
   if (path.length > 0)
   {
      var lastChar = path.charAt(path.length-1);
      if (lastChar != pathSlash)
         path += pathSlash;
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

// This function extracts a file to a directory.
//
// Parameters:
//  pFilename: The name of the file to extract
//  pWorkDir: The directory to extract the file into.  This directory must
//            exist before calling this function.
//
// Return value: A blank string on success, or an error message on failure.
function extractFileToDir(pFilename, pWorkDir)
{
   // If pFilename doesn't exist, then return with an error.
   if (typeof(pFilename) != "string")
      return ("Invalid filename specified.");
   if (pFilename.length == 0)
      return ("No filename specified.");
   if (!file_exists(pFilename))
      return ("The specified file does not exist.");

   // If pWorkDir is blank, then return with an error.
   if (typeof(pWorkDir) != "string")
      return ("Unknown argument specified for the work directory.");
   if (pWorkDir.length == 0)
      return ("No work directory specified.");

   // If pWorkDir ends with a slash, remove it.
   if ((/\/$/.test(pWorkDir)) || (/\\$/.test(pWorkDir)))
      pWorkDir = pWorkDir.substr(0, pWorkDir.length-1);

   // If the work directory doesn't exist, then return with
   // an error.
   // Note: file_exists() doesn't seem to work properly with directories.
   //if (!file_exists(pWorkDir))
   //   return ("The work directory doesn't exist.");

   var filenameExt = getFilenameExtension(pFilename);
   // Return with errors if there are problems.
   if (filenameExt.length == 0)
      return ("Can't extract (no file extension).");
   if (typeof(gFileTypeCfg[filenameExt]) == "undefined")
      return ("Can't extract " + getFilenameFromPath(pFilename) + " (I don't know how).");
   if (gFileTypeCfg[filenameExt].extractCmd == "")
      return ("Can't extract " + getFilenameFromPath(pFilename) + " (I don't know how).");

   var retval = "";

   // Extract the file to the work directory.
   var extractCmd = gFileTypeCfg[filenameExt].extractCmd.replace("%FILENAME%", "\"" + fixPathSlashes(pFilename) + "\"");
   extractCmd = extractCmd.replace("%FILESPEC% ", "");
   extractCmd = extractCmd.replace("%TO_DIR%", "\"" + fixPathSlashes(pWorkDir) + "\"");
   var retCode = system.exec(extractCmd);
   if (retCode != 0)
      return ("Extract failed with exit code " + retCode);
   //   For each file in the work directory:
   //     If the file has an extract command
   //        Extract it to a subdir in the temp dir
   //        Delete the archive
   var files = directory(pWorkDir + "/*");
   for (var i in files)
   {
      // If the file has an extract command, then extract it to a
      // temp directory in the work directory.
      filenameExt = getFilenameExtension(files[i]);
      if ((typeof(gFileTypeCfg[filenameExt]) != "undefined") &&
          ((gFileTypeCfg[filenameExt].extractCmd != "")))
      {
         // Create the temp directory and extract the file there.
         var workDir = pWorkDir + "/" + getFilenameFromPath(files[i] + "_temp");
         if (mkdir(workDir))
            retval = extractFileToDir(files[i], workDir);
         else
            retval = "Unable to create a temporary directory.";

         // If there was no problem, then delete the archive file.  Otherwise,
         // stop going through the list of files.
         if (retval.length == 0)
            file_remove(files[i]);
         else
            break;
      }
   }

   return retval;
}

// This function executes an OS command and returns its output as an
// array of strings.  The reason this function was written is that
// system.popen() is only functional in UNIX.
//
// Parameters:
//  pCommand: The command to execute
//
// Return value: An object containing the following properties:
//               returnCode: The return code of the OS command
//               cmdOutput: An array of strings containing the program's output.
function execCmdWithOutput(pCommand)
{
   var retObj = new Object();
   retObj.returnCode = 0;
   retObj.cmdOutput = new Array();

   if ((pCommand == undefined) || (pCommand == null) || (typeof(pCommand) != "string"))
      return retObj;

   // Execute the command and redirect the output to a file in the
   // node's directory.  system.exec() returns the return code that the
   // command returns; generally, 0 means success and non-zero means
   // failure (or an error of some sort).
   const tempFilename = system.node_dir + "DDUPCommandOutput_temp.txt";
   retObj.returnCode = system.exec(pCommand + " >" + tempFilename + " 2>&1");
   // Read the temporary file and populate retObj.cmdOutput with its
   // contents.
   var tempFile = new File(tempFilename);
   if (tempFile.open("r"))
   {
      if (tempFile.length > 0)
      {
         var fileLine = null;
         while (!tempFile.eof)
         {
            fileLine = tempFile.readln(2048);

            // fileLine should be a string, but I've seen some cases
            // where it isn't, so check its type.
            if (typeof(fileLine) != "string")
               continue;

            retObj.cmdOutput.push(fileLine);
         }
      }

      tempFile.close();
   }

   // Remove the temporary file, if it exists.
   if (file_exists(tempFilename))
      file_remove(tempFilename);

   return retObj;
}

// Runs an external command.  This function was written because
// I want to be able to handle executable files with spaces in
// their name/path (system.exec() doesn't handle said spaces).
//
// Parameters:
//  pCommand: The command to execute
//
// Return value: An object containing the following properties:
//               returnCode: The return code of the OS command
//               cmdOutput: An array of strings containing the program's output.
function runExternalCmdWithOutput(pCommand)
{
   // Determine whether or not we're in Windows.
   if (runExternalCmdWithOutput.inWin == undefined)
      runExternalCmdWithOutput.inWin = /^WIN/.test(system.platform.toUpperCase());

   var retObj = null; // The return object
   var wroteScriptFile = false; // Whether or not we were able to write the script file

   // In the node directory, write a batch file (if in Windows) or a *nix shell
   // script (if not in Windows) containing the command to run.
   var scriptFilename = "";
   if (runExternalCmdWithOutput.inWin)
   {
      // Write a Windows batch file to run the command
      scriptFilename = fixPathSlashes(system.node_dir + "DDUP_ScanCmd.bat");
      //console.print(":" + scriptFilename + ":\r\n\1p"); // Temporary (for debugging)
      var scriptFile = new File(scriptFilename);
      if (scriptFile.open("w"))
      {
         scriptFile.writeln("@echo off");
         scriptFile.writeln(pCommand);
         scriptFile.close();
         wroteScriptFile = true;
         retObj = execCmdWithOutput(scriptFilename);
      }
   }
   else
   {
      // Write a *nix shell script to run the command
      scriptFilename = system.node_dir + "DDUP_ScanCmd.sh";
      var scriptFile = new File(scriptFilename);
      if (scriptFile.open("w"))
      {
         scriptFile.writeln("#!/bin/bash"); // Hopefully /bin/bash is valid on the system!
         scriptFile.writeln(pCommand);
         scriptFile.close();
         wroteScriptFile = true;
         system.exec("chmod ugo+x " + scriptFilename);
         retObj = execCmdWithOutput("bash " + scriptFilename);
      }
   }

   // Remove the script file, if it exists
   if (file_exists(scriptFilename))
      file_remove(scriptFilename);

   // If we were unable to write the script file, then create retObj with
   // a returnCode indicating failure.
   if (!wroteScriptFile)
   {
      // Could not open the script file for writing
      retObj = new Object();
      retObj.cmdOutput = new Array();
      retObj.returnCode = -1;
   }

   return retObj;
}