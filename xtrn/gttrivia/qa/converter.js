require("sbbsdefs.js", "K_NOCRLF");


var opts = parseCmdLine(argv);

print("");

if (opts.inputFilename.length == 0)
{
	print("No input filename was specified.");
	print("");
	exit(1);
}
if (!file_exists(opts.inputFilename))
{
	print("Specified file does not exist:");
	print(opts.inputFilename);
	print("");
	exit(2);
}

print("Input filename:" + opts.inputFilename + ":");
print("");


var outputFilename = opts.inputFilename + "-converted.txt";

print("");
print("Converting " + opts.inputFilename);
print("Output: " + outputFilename);
print("");

var inFile = new File(opts.inputFilename);
var outFile = new File(outputFilename);
if (inFile.open("r"))
{
	if (outFile.open("w"))
	{
		var question = "";
		var answer = "";
		var questionLimit = 0; // Temporary
		var numWritten = 0; // Temporary
		while (!inFile.eof /*&& (questionLimit > 0 && numWritten < questionLimit)*/)
		{
			var fileLine = inFile.readln(4096);
			// I've seen some cases where readln() doesn't return a string
			if (typeof(fileLine) !== "string")
				continue;
			fileLine = fileLine.trim();
			if (fileLine.length == 0)
				continue;

			/*
			//print(fileLine); // Temporary
			if (fileLine.indexOf("Trivia Question:") == 0)
			{
				question = fileLine.substr(16).trim();
				//print("- Here 1 - question:" + question + ":"); // Temporary
			}
			else if (fileLine.indexOf("Answer:") == 0)
			{
				answer = fileLine.substr(7).trim();
				//print("- Here 2 - answer:" + answer + ":"); // Temporary
			}
			else // Assume it's part of the question
			{
				question += " " + fileLine;
				//print("- Here 3 - question:" + question + ":"); // Temporary
			}
			*/
			//print(fileLine); // Temporary
			if (fileLine.indexOf("Answer:") == 0)
			{
				answer = fileLine.substr(7).trim();
				//print("- Here 2 - answer:" + answer + ":"); // Temporary
			}
			else
			{
				question = fileLine;
				// Search for and remove the number (and period) at the beginning of the question
				var dotIdx = question.indexOf(".");
				if (dotIdx >= 0)
					question = question.substr(dotIdx+1).trim();
				//print("- Here 3 - question:" + question + ":"); // Temporary
			}

			/*
			// Temporary
			print("");
			print("Question length: " + question.length);
			print("Answer length: " + answer.length);
			print("");
			// End Temporary
			*/
			
			if (question.length > 0 && answer.length > 0)
			{
				/*
				// Temporary
				print("- Writing:");
				print(question);
				print(answer);
				print("");
				// End Temporary
				*/
				outFile.writeln(question);
				outFile.writeln(answer);
				outFile.writeln("10");
				outFile.writeln("");
				//++numWritten; // Temporary

				question = "";
				answer = "";
				//numPoints = -1;
			}


			/*
			if (question.length == 0)
				question = fileLine;
			else if (qnswer.length == 0)
				qnswer = fileLine;
			//else if (theNumPoints < 0)
			//	theNumPoints = +(fileLine);
			*/
		}
		outFile.close();
		print("Done.");
		print("");
	}
	else
		print("* Failed to open " + outputFilename + " for writing!");
	inFile.close();
}
else
{
	print("* Failed to open " + opts.inputFilename + " for reading!");
	exit(3);
}




////////////////////////////////////////////////////////////////////
// Functions


// QA object constructor
function QA(pQuestion, pAnswer, pNumPoints)
{
	this.question = pQuestion;
	this.answer = pAnswer;
	this.numPoints = pNumPoints;
}

// Parses command line options
function parseCmdLine(argv)
{
	var retObj = {
		inputFilename: ""
	};

	if (!Array.isArray(argv))
		return retObj;

	for (var i = 0; i < argv.length; ++i)
	{
		if (argv[i].length == 0) continue;
		if (argv[i].charAt(0) == "-")
		{
			if (i >= argv.length - 1) continue;
			var paramNameUpper = argv[i].substr(1).toUpperCase();
			if (paramNameUpper == "INPUTFILENAME" || paramNameUpper == "INPUT_FILENAME")
				retObj.inputFilename = argv[i+1];
			++i; // To avoid analyzing the next parameter, since the next one is the value for this one
		}
		else
		{
			if (i == 0)
				retObj.inputFilename = argv[i];
		}
	}

	return retObj;
}