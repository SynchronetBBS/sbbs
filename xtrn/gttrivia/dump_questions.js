// This is a script that reads a .qa file & dumps the questions within.
// This is meant to be run on the server with jsexec.

"use strict";

var requireFnExists = (typeof(require) === "function");
if (requireFnExists)
	require("gttrivia_common.js", "parseQAFile");
else
	load("gttrivia_common.js");


if (argc == 0)
{
	writeln("");
	printUsage();
	exit(0);
}

// Parse command-line arguments
var gOpts = parseCmdLineArgs(argv);
if (gOpts.exitNow)
	exit(0);


// Make sure the file exists & we know where it is
if (/^\.[\\\/]/.test(gOpts.qaFilename))
	gOpts.qaFilename = js.exec_dir + gOpts.qaFilename.substring(2);
if (!file_exists(gOpts.qaFilename))
{
	var fullPath = js.exec_dir + "qa/" + file_getname(gOpts.qaFilename);
	if (file_exists(fullPath))
		gOpts.qaFilename = fullPath;
	else
	{
		var filenameInQaDir = fullpath("qa/" + file_getname(gOpts.qaFilename));
		if (file_exists(filenameInQaDir))
			gOpts.qaFilename = filenameInQaDir;
	}
}
if (!file_exists(gOpts.qaFilename))
{
	writeln("* The specified file doesn't exist");
	writeln("");
	exit(0);
}


// Do the dump
writeln("Question file: " + file_getname(gOpts.qaFilename));
writeln("Full path:");
writeln(gOpts.qaFilename);
writeln("");

var questions = parseQAFile(gOpts.qaFilename);
printf("There are %d questions.\n\n", questions.length);
if (gOpts.onlyCountQuestions)
	exit(0);
for (var questionIdx = 0; questionIdx < questions.length; ++questionIdx)
{
	var questionObj = questions[questionIdx];
	printf("- Question %d of %d:\n", questionIdx+1, questions.length);
	printf("Question:\n%s\n", questionObj.question);
	if (Array.isArray(questionObj.answer))
	{
		printf("%d %s:\n", questionObj.answer.length, questionObj.answer.length > 1 ? "answers" : "answer");
		for (var i = 0; i < questionObj.answer.length; ++i)
			printf(" %s\n", questionObj.answer[i]);
	}
	else if (typeof(questionObj.answer === "string"))
		printf("Answer:\n %s\n", questionObj.answer);
	else
		writeln("* The answer is invalid data!\n");
	printf("# points: %d\n", questionObj.numPoints);
	if (questionObj.answerFacts.length > 0)
	{
		printf("%d %s:\n", questionObj.answerFacts.length, questionObj.answerFacts.length > 1 ? "facts" : "fact");
		for (var i = 0; i < questionObj.answerFacts.length; ++i)
			printf(" %s\n", questionObj.answerFacts[i]);
	}

	writeln("");
}




///////////////////////////////////////////////////////////
// Functions

// Parses command-line arguments.
function parseCmdLineArgs(pArgv)
{
	var retObj = {
		exitNow: false,
		onlyCountQuestions: false,
		qaFilename: ""
	};

	if (pArgv.length == 0)
	{
		retObj.exitNow = true;
		return retObj;
	}

	for (var i = 0; i < pArgv.length; ++i)
	{
		if (pArgv[i].length == 0)
			continue;
		if (pArgv[i].indexOf("-") == 0)
		{
			if (pArgv[i] == "-cq" || pArgv[i] == "-cq")
				retObj.onlyCountQuestions = true;
		}
		else
			retObj.qaFilename = argv[i];
	}

	if (retObj.qaFilename.length == 0)
	{
		retObj.exitNow = true;
		writeln("");
		writeln("* The filename is not specified.");
		writeln("");
		printUsage()
	}

	return retObj;
}

// Prints the script's command-line usage'
function printUsage()
{
	writeln("usage: dump_questions.js [<switches>...] <qa_filename>");
	writeln("");
	writeln("<Switches>");
	writeln("  -cq: Only count questions");
	writeln("");
	writeln("<qa_filename> is the name of a .qa (question) file.")
	writeln("");
}
