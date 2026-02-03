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
 * Date       Author            Description
 * 2009-12-25-
 * 2009-12-28 Eric Oulashin     Initial development
 * 2009-12-29 Eric Oulashin     Version 1.00
 *                              Initial public release
 * 2022-06-08 Eric Oulashin     Version 1.01
                                Made fixes to get the scanner functionality working properly in Linux
 * 2022-06-11 Eric Oulashin     Version 1.02
 *                              Improved file/dir permissions more: Set file permissions after extracting
 *                              an archive so that they're all readable.
 * 2022-06-11 Eric Oulashin     Version 1.03
 *                              Removed the chmod stuff, as it is actually not needed.
 * 2023-08-06 Eric Oulashin     Version 1.04
 *                              Now uses Synchronet's built-in archiver (added in Synchronet 3.19),
 *                              if available, to extract archives.
 * 2023-08-07 Eric Oulashin     Version 1.05
 *                              Internal refactor of how the configuration files are read
 * 2023-08-08 Eric Oulashin     Version 1.06
 *                              When a virus scan fails, the scan output is written to the system
 *                              log (as a warning) rather than to the user's console session.
 * 2025-08-11 Eric Oulashin     Version 1.07
 *                              Supports .example.cfg configuration filenames. Also allows
 *                              reading the configuration file from sbbs/mods or sbbs/ctrl.
 * 2026-02-02 Eric Oulashin     Version 1.08
 *                              More robust filename extension matching
 */

/* Command-line arguments:
 1 (argv[0]): The name of the file to scan
*/

"use strict";

load("sbbsdefs.js");

// Require version 3.14 or newer of Synchronet (for file_chmod())
if (system.version_num < 31400)
{
	console.print("\x01nDigital Distortion Upload Processor requires Synchronet 3.14 or newer.\r\n");
	exit(1);
}


load(js.exec_dir + "ddup_cleanup.js");

// Version information
var gDDUPVersion = "1.06";
var gDDUPVerDate = "2026-02-02";

// Store whether or not this is running in Windows
var gRunningInWindows = /^WIN/.test(system.platform.toUpperCase());


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
			console.print("\x01n\x01y\x01hError: \x01n\x01cBlank filename argument given.\r\n\x01p");
			exit(-2);
		}
	}
	else
	{
		console.print("\x01n\x01y\x01hError: \x01n\x01cUnknown command-line argument specified.\r\n\x01p");
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
	console.print("\x01n\x01y\x01hError: \x01n\x01cNo filename specified to process.\r\n\x01p");
	exit(1);
}

// Create the global configuration objects.
var gGenCfg = {
	scanCmd: "",
	skipScanIfSysop: false,
	pauseAtEnd: false
};
var gFileTypeCfg = {};

// Read the configuration files to populate the global configuration object.
var configFileRead = ReadConfigFile();
// If the configuration files weren't read, then output an error and exit.
if (!configFileRead)
{
	console.print("\x01n\x01y\x01hError: \x01n\x01cUpload processor is unable to read its\r\n");
	console.print("configuration files.\r\n\x01p");
	exit(2);
}
// Exit if there is no scan command.
if (gGenCfg.scanCmd.length == 0)
{
	console.print("\x01n\x01y\x01hWarning: \x01n\x01cNo scan command configured for the upload processor.\r\n");
	exit(0);
}

// Global variables
// Strings for the OK and failure symbols
var gOKStr = "\x01n\x01k\x01h[\x01n\x01g\xFB\x01k\x01h]\x01n";
var gOKStrWithNewline = gOKStr + "\r\n";
var gFailStr = "\x01n\x01k\x01h[\x01rX\x01k]\x01n";
var gFailStrWithNewline = gFailStr + "\r\n";
// Stuff for the printf formatting string for the status messages
var gStatusTextLen = 79 - console.strlen(gOKStr); // gOKStr and gFailStr should have the same length
var gStatusPrintfStr = "\x01n%s%-" + gStatusTextLen + "s\x01n"; // For a color and the status text

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
	console.print("\x01n\r\n\x01c\x01hD\x01n\x01cigital \x01hD\x01n\x01cistortion \x01hU\x01n\x01cpload \x01hP\x01n\x01crocessor \x01w\x01hv\x01n\x01g" +
	              gDDUPVersion);
	// Originally I had this script output the version date, but now I'm not sure
	// if I want to do that..
	//console.print(" \x01w\x01h(\x01b" + gDDUPVerDate + "\x01w)");
	console.attributes = "N";
	console.crlf();

	// Process the file
	var exitCode = processFile(gFileToScan);
	// Depending on the exit code, display a success or failure message.
	console.crlf();
	if (exitCode == 0)
		console.print(gOKStr + " \x01n\x01b\x01hScan successful - The file passed.\r\n");
	else
		console.print(gFailStr + " \x01n\x01y\x01hScan failed!\r\n");

	// If the option to pause at the end is enabled, then prompt the user for
	// a keypress.
	if (gGenCfg.pauseAtEnd)
	{
		console.print("\x01n\x01w\x01hPress any key to continue:\x01n");
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
	if ((pExtension != null) && (typeof(pExtension) == "string"))
		this.extension = pExtension;
	if ((pExtractCmd != null) && (typeof(pExtractCmd) == "string"))
		this.extractCmd = pExtractCmd;
	if ((pScanOption != null) && (typeof(pScanOption) == "string"))
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
	console.print("\x01n\x01w\x01hScanning \x01b" + justFilename);
	console.print("\x01n\r\n\x01b\x01" + "7                             File Scan Status                                  \x01n\r\n");

	// If the skipScanIfSysop option is enabled and the user is a sysop,
	// then assume the file is good.
	if (gGenCfg.skipScanIfSysop && user.compare_ars("SYSOP"))
	{
		printf(gStatusPrintfStr, "\x01g\x01h", "Auto-approving the file (you're a sysop)");
		console.print(gOKStrWithNewline);
		return 0;
	}

	var retval = 0;

	// Look for the file extension in gFileTypeCfg to get the file scan settings.
	// If the file extension is not there, then go ahead and scan it (to be on the
	// safe side).
	var filenameExtension = getFilenameExtension(pFilename, gFileTypeCfg);
	if (gFileTypeCfg.hasOwnProperty(filenameExtension) && typeof(gFileTypeCfg[filenameExtension]) === "object")
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
					console.print("\x01n\x01y\x01hWarning: \x01n\x01w\x01h Unable to create the work dir.\x01n\r\n");
					retval = -1;
				}
				
				// If all is okay, then create the directory in the temporary work dir.
				var workDir = baseWorkDir + "/" + justFilename + "_temp";
				if (retval == 0)
				{
					deltree(workDir + "/");
					if (!mkdir(workDir))
					{
						console.print("\x01n\x01y\x01hWarning: \x01n\x01w\x01h Unable to create a dir in the temporary work dir.\x01n\r\n");
						retval = -1;
					}
				}

				// If all is okay, we can now process the file.
				if (retval == 0)
				{
					// Extract the file to the work directory
					printf(gStatusPrintfStr, "\x01m\x01h", "Extracting the file...");
					console.crlf();
					var errorStr = extractFileToDir(pFilename, workDir);
					if (errorStr.length == 0)
					{
						// Scan the files in the work directory.
						printf(gStatusPrintfStr, "\x01r", "Scanning files inside the archive for viruses...");
						var retObj = scanFilesInDir(workDir);
						retval = retObj.returnCode;
						if (retObj.returnCode == 0)
							console.print(gOKStrWithNewline);
						else
						{
							console.print(gFailStrWithNewline);
							console.print("\x01n\x01y\x01hVirus scan failed.\x01n\r\n");
							log(LOG_WARNING, format("File (%s) uploaded by %s failed virus scan:", pFilename, user.alias));
							for (var index = 0; index < retObj.cmdOutput.length; ++index)
								log(LOG_WARNING, retObj.cmdOutput[index]);
						}
					}
					else
					{
						console.print(gFailStrWithNewline);
						log(LOG_ERR, "File extract error: " + errorStr);
						console.print("\x01n\x01y\x01hWarning: \x01n\x01w\x01h Unable to extract to work dir.\x01n\r\n");
						retval = -2;
					}
				}
				// Remove the work directory.
				deltree(baseWorkDir + "/");
			}
			else
			{
				// The file has no extract command, so just scan it.
				printf(gStatusPrintfStr, "\x01b\x01h", "Scanning...");
				var scanCmd = gGenCfg.scanCmd.replace("%FILESPEC%", "\"" + fixPathSlashes(pFilename) + "\"");
				// Run the scan command and capture its output, in case the scan fails.
				var retObj = runExternalCmdWithOutput(scanCmd);
				retval = retObj.returnCode;
				if (retObj.returnCode == 0)
					console.print(gOKStrWithNewline);
				else
				{
					console.print(gFailStrWithNewline);
					console.print("\x01n\x01y\x01hVirus scan failed.\x01n\r\n");
					log(LOG_WARNING, format("File (%s) uploaded by %s failed virus scan:", pFilename, user.alias));
					for (var index = 0; index < retObj.cmdOutput.length; ++index)
						log(LOG_WARNING, retObj.cmdOutput[index]);
				}
			}
		}
		else if (gFileTypeCfg[filenameExtension].scanOption == "always fail")
			exitCode = 10;
	}
	else
	{
		// There's nothing configured for the file's extension, so just scan it.
		printf(gStatusPrintfStr, "\x01r", "Scanning...");
		var scanCmd = gGenCfg.scanCmd.replace("%FILESPEC%", "\"" + fixPathSlashes(pFilename) + "\"");
		var retObj = runExternalCmdWithOutput(scanCmd);
		retval = retObj.returnCode;
		if (retObj.returnCode == 0)
			console.print(gOKStrWithNewline);
		else
		{
			console.print(gFailStrWithNewline);
			console.print("\x01n\x01y\x01hVirus scan failed.\x01n\r\n");
			log(LOG_WARNING, format("File (%s) uploaded by %s failed virus scan:", pFilename, user.alias));
			for (var index = 0; index < retObj.cmdOutput.length; ++index)
				log(LOG_WARNING, retObj.cmdOutput[index]);
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
	var retObj = {
		cmdOutput: [],
		returnCode: 0
	};

	// If pDir is unspecified, then just return.
	if (typeof(pDir) != "string")
	{
		retObj.returnCode = -1;
		return retObj;
	}
	if (pDir.length == 0)
	{
		retObj.returnCode = -2;
		return retObj;
	}
	// Also, just return if gGenCfg.scanCmd is blank.
	if (gGenCfg.scanCmd.length == 0)
	{
		retObj.returnCode = -3;
		return retObj;
	}

	// If the filename has a trailing slash, remove it.
	if ((/\/$/.test(pDir)) || (/\\$/.test(pDir)))
		pDir = pDir.substr(0, pDir.length-1);

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
			retObj = {
				returnCode: 0,
				cmdOutput = []
			};
		}
		*/
	}
	else
	{
		// gGenCfg.scanCmd doesn't contain %FILESPEC%, so set up
		// retObj with a non-zero return code (for failure)
		retObj.returnCode = -4;
		retObj.cmdOutput.push("The virus scanner is not set up correctly.");
	}

	return retObj;
}

// Reads the configuration file and returns an object containing the
// configuration settings.
//
// Return value: Boolean - Whether or not the configuration was read.
function ReadConfigFile()
{
	// Read the file type settings.
	var fileTypeSettingsRead = false;
	var cfgFilename = GetFullyPathedCfgFilename("ddup_file_types.cfg");
	if (cfgFilename.length > 0)
	{
		var fileTypeCfgFile = new File(cfgFilename);
		if (fileTypeCfgFile.open("r"))
		{
			if (fileTypeCfgFile.length > 0)
			{
				var allFileTypeCfg = fileTypeCfgFile.iniGetAllObjects();
				fileTypeSettingsRead = true;
				for (var i = 0; i < allFileTypeCfg.length; ++i)
				{
					var filenameExt = allFileTypeCfg[i].name; // Filename extension
					var scannableFile = new ScannableFile(filenameExt, "", "scan");
					for (var prop in allFileTypeCfg[i])
					{
						var propUpper = prop.toUpperCase();
						if (propUpper === "EXTRACT")
							scannableFile.extractCmd = allFileTypeCfg[i][prop];
						else if (propUpper === "SCANOPTION")
							scannableFile.scanOption = allFileTypeCfg[i][prop];
					}
					gFileTypeCfg[filenameExt] = scannableFile;
				}
			}
			fileTypeCfgFile.close();
		}
	}

	// Read the general configuration
	var genSettingsRead = false;
	cfgFilename = GetFullyPathedCfgFilename("ddup.cfg");
	if (cfgFilename.length > 0)
	{
		var genCfgFile = new File(cfgFilename);
		if (genCfgFile.open("r"))
		{
			if (genCfgFile.length > 0)
			{
				var settingsObj = genCfgFile.iniGetObject();
				genSettingsRead = true;
				for (var prop in settingsObj)
				{
					// Set the appropriate value in the settings object.
					var settingUpper = prop.toUpperCase();
					if (settingUpper == "SCANCMD")
						gGenCfg.scanCmd = settingsObj[prop];
					else if (settingUpper == "SKIPSCANIFSYSOP")
					{
						if (typeof(settingsObj[prop]) === "string")
							gGenCfg.skipScanIfSysop = (settingsObj[prop].toUpperCase() == "YES");
						else if (typeof(settingsObj[prop]) === "boolean")
							gGenCfg.skipScanIfSysop = settingsObj[prop];
					}
					else if (settingUpper == "PAUSEATEND")
					{
						if (typeof(settingsObj[prop]) === "string")
						{
							var valueUpper = settingsObj[prop].toUpperCase();
							gGenCfg.pauseAtEnd = (valueUpper == "YES" || valueUpper == "TRUE");
						}
						else if (typeof(settingsObj[prop]) === "boolean")
							gGenCfg.pauseAtEnd = settingsObj[prop];
					}
				}
			}
		}
	}

	return (fileTypeSettingsRead && genSettingsRead);
}

// Given a filename (without full path), this returns a fully-pathed configuration filename.
// This checks the mods directory, then the ctrl directory, then the same directory as the
// script. If not found, then this will look for an .example.cfg in the same directory as
// the script.  If none found, this will return an empty string.
//
// Parameters:
//  pFilenameWithoutFullPath: A filename (not fully-pathed)
//
// Return value: A fully-pathed filename containing the directory it's found in. If not found,
//               the return value will be an empty string.
function GetFullyPathedCfgFilename(pFilenameWithoutFullPath)
{
	var cfgFilename = file_cfgname(system.mods_dir, pFilenameWithoutFullPath);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(system.ctrl_dir, pFilenameWithoutFullPath);
	if (!file_exists(cfgFilename))
		cfgFilename = file_cfgname(js.exec_dir, pFilenameWithoutFullPath);
	// If the configuration file hasn't been found, look to see if there's a .example.cfg file
	// available in the same directory as the script
	if (!file_exists(cfgFilename))
	{
		var exampleFileName = "";
		var dotIdx = pFilenameWithoutFullPath.lastIndexOf(".");
		if (dotIdx > -1)
			exampleFileName = pFilenameWithoutFullPath.substring(0, dotIdx) + ".example" + pFilenameWithoutFullPath.substring(dotIdx);
		else
			exampleFileName = pFilenameWithoutFullPath + ".example";
		exampleFileName = file_cfgname(js.exec_dir, exampleFileName);
		if (file_exists(exampleFileName))
			cfgFilename = exampleFileName;
		else
			cfgFilename = "";
	}
	return cfgFilename;
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
//  pFileTypeCfg: The file type configuration object (extensions can be matched with this object)
//
// Return value: The filename's extension, or blank if there is none.
function getFilenameExtension(pFilename, pFileTypeCfg)
{
	const filenameUpper = pFilename.toUpperCase();
	var filenameExt = "";
	if (typeof(pFileTypeCfg) === "object")
	{
		var dotIdx = filenameUpper.indexOf(".");
		var continueOn = true;
		while (dotIdx > -1 && continueOn)
		{
			var nextIdx = dotIdx + 1;
			var ext = filenameUpper.substring(nextIdx);
			if (pFileTypeCfg.hasOwnProperty(ext))
			{
				filenameExt = filenameUpper.substring(nextIdx);
				continueOn = false;
			}
			else
				dotIdx = filenameUpper.indexOf(".", nextIdx);
		}
	}
	else
	{
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

   var pathSlash = (gRunningInWindows ? "\\" : "/");

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

   // Fix the slashes and return the fixed version.
   //return(gRunningInWindows ? pPath.replace("/", "\\") : pPath.replace("\\", "/"));
   var path = pPath;
   if (gRunningInWindows) // Windows
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

	var filenameExt = getFilenameExtension(pFilename, gFileTypeCfg);
	// Return with errors if there are problems.
	if (filenameExt.length == 0)
		return ("Can't extract (no file extension).");
	if (typeof(gFileTypeCfg[filenameExt]) == "undefined")
		return ("Can't extract " + getFilenameFromPath(pFilename) + " (I don't know how).");
	if (gFileTypeCfg[filenameExt].extractCmd == "")
		return ("Can't extract " + getFilenameFromPath(pFilename) + " (I don't know how).");

	var retval = "";

	// Extract the file to the work directory.
	// If the Archive class is available (added in Synchronet 3.19), then
	// use it to extract the archive.  Otherwise, use the configured external
	// archiver command to extract it.
	var builtInExtractSucceeded = false;
	if (typeof(Archive) === "function")
	{
		var arcFilenameFixed = fixPathSlashes(pFilename);
		var arcFile = new Archive(arcFilenameFixed);
		try
		{
			// Extract with path information (trust the archive that any
			// filename characters are okay).  If we don't extract with
			// path information, then Synchronet may reject some filenames
			// due to certain characters in the filename.
			var numFilesExtracted = arcFile.extract(pWorkDir, true);
			builtInExtractSucceeded = true;
		}
		catch (e)
		{
			// Synchronet's internal archiver was unable to extract it.
			log(LOG_ERR, "DD Upload Processor: Synchronet internal archiver failed to extract " + arcFilenameFixed + ": " + e);
		}
	}
	if (!builtInExtractSucceeded)
	{
		var extractCmd = gFileTypeCfg[filenameExt].extractCmd.replace("%FILENAME%", "\"" + fixPathSlashes(pFilename) + "\"");
		extractCmd = extractCmd.replace("%FILESPEC% ", "");
		extractCmd = extractCmd.replace("%TO_DIR%", "\"" + fixPathSlashes(pWorkDir) + "\"");
		var retCode = system.exec(extractCmd);
		if (retCode != 0)
			return ("Extract failed with exit code " + retCode);
	}
	//   For each file in the work directory:
	//     If the file has an extract command
	//        Extract it to a subdir in the temp dir
	//        Delete the archive
	var files = directory(pWorkDir + "/*");
	for (var i in files)
	{
		// If the file has an extract command, then extract it to a
		// temp directory in the work directory.
		filenameExt = getFilenameExtension(files[i], gFileTypeCfg);
		if ((typeof(gFileTypeCfg[filenameExt]) != "undefined") && ((gFileTypeCfg[filenameExt].extractCmd != "")))
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
	var retObj = null; // The return object
	var wroteScriptFile = false; // Whether or not we were able to write the script file

	// In the node directory, write a batch file (if in Windows) or a *nix shell
	// script (if not in Windows) containing the command to run.
	var scriptFilename = "";
	if (gRunningInWindows)
	{
		// Write a Windows batch file to run the command
		scriptFilename = fixPathSlashes(system.node_dir + "DDUP_ScanCmd.bat");
		//console.print(":" + scriptFilename + ":\r\n\x01p"); // Temporary (for debugging)
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
		retObj = {
			cmdOutput: [],
			returnCode: -1
		};
	}

	return retObj;
}
