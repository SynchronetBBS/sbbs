// $Id$

/* This is a script that can sort a file.  This script is meant to be run with jsexec
 * (on the OS command prompt, not via the Synchronet terminal server).  One purpose
 * of this script is to sort dictionary files when they're created or when words
 * are added to existing dictionary files.
 *
 * The command-line syntax of this script is:
 * jsexec sort_file.js <inputFilename> <outputFilename> [convertToLowercase]
 * Where <inputFilename> is the name of the input file and <outputFilename> is the
 * name of the output file (the sorted input).  convertToLowercase is optional, and
 * specifies whether or not to convert all entries in the file to lower-case
 * (which is required for case-insensitive word matching via SlyEdit, for instance).
 * The values for convertToLowercase can be true or false (really, anything besides
 * true will be interpreted as false).
 */

load("sbbsdefs.js");

if (argc < 2)
{
	write("\r\nUsage: sort_file.js <inputFilename> <outputFilename> [convertToLowercase]\r\n\r\n");
	exit(1);
}

write("\r\n");
var inputFilename = argv[0];
var outputFilename = argv[1];
var convertToLowercase = false;
if (argc >= 3)
	convertToLowercase = (argv[2].toLowerCase() == "true");
write("Input filename: " + inputFilename + "\r\n");
write("Output filename: " + outputFilename + "\r\n");
write("Convert to lowercase: " + convertToLowercase + "\r\n");
write("Sorting...\r\n");
var succeeded = sortFile(inputFilename, outputFilename, convertToLowercase);
if (succeeded)
	write("Sort successful.\r\n");
else
	write("Sort failed!\r\n");

write("\r\n");


//////////////////////////////////
// Functions
function sortFile(pInputFilename, pOutputFilename, pToLowercase)
{
	var succeeded = true;

	var inFile = new File(pInputFilename);
	var fileLines = [];
	if (inFile.open("r"))
	{
		var fileLine;
		while (!inFile.eof)
		{
			// Read the next line from the file.
			fileLine = inFile.readln(2048);
			// fileLine should be a string, but I've seen some cases
			// where for some reason it isn't.  If it's not a string,
			// then continue onto the next line.
			if (typeof(fileLine) == "string")
				fileLines.push(pToLowercase ? fileLine.toLowerCase() : fileLine);
		}
		inFile.close();
	}
	else
		succeeded = false;
	if (succeeded)
	{
		fileLines.sort();
		var outFile = new File(pOutputFilename);
		if (outFile.open("w"))
		{
			for (var i = 0; i < fileLines.length; ++i)
				outFile.writeln(fileLines[i]);
			outFile.close();
		}
		else
			succeeded = false;
	}

	return succeeded;
}