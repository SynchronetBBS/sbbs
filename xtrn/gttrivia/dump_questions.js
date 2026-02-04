// This is a script that reads a .qa file & dumps the questions within.
// This is meant to be run on the server with jsexec.

"use strict";

var requireFnExists = (typeof(require) === "function");
if (requireFnExists)
	require("gttrivia_common.js", "parseQAFile");
else
	load("gttrivia_common.js");


writeln("");

if (argc == 0)
{
	writeln("Usage: dump_questions.js <qa_filename>");
	writeln("");
	exit(0);
}

var qaFilename = argv[0];
if (/^\.[\\\/]/.test(qaFilename))
	qaFilename = js.exec_dir + qaFilename.substring(2);
if (!file_exists(qaFilename))
{
	var fullPath = js.exec_dir + "qa/" + file_getname(qaFilename);
	if (file_exists(fullPath))
		qaFilename = fullPath;
	else
	{
		var filenameInQaDir = fullpath("qa/" + file_getname(qaFilename));
		if (file_exists(filenameInQaDir))
			qaFilename = filenameInQaDir;
	}
}
if (!file_exists(qaFilename))
{
	writeln("* The specified file doesn't exist");
	writeln("");
	exit(0);
}

writeln("Question file: " + file_getname(qaFilename));
writeln("Full path:");
writeln(qaFilename);
writeln("");

var questions = parseQAFile(qaFilename);
printf("There are %d questions.\n\n", questions.length);
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
