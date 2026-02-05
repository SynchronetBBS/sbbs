// This file has common functions for gttrivia.js and dump_questions.js.

"use strict";

// Gets the list of the Q&A files in the qa subdirectory in the .js script's directory.
//
// Return value: An array of objects with the following properties:
//               sectionName: The name of the trivia section
//               filename: The fully-pathed filename of the file containing the questions, answers, and # points for the trivia section
function getQACategoriesAndFilenames()
{
	var sectionsAndFilenames = [];
	var QAFilenames = directory(gScriptExecDir + "qa/*.qa");
	for (var i = 0; i < QAFilenames.length; ++i)
	{
		// Get the section name - Start by removing the .qa filename extension
		var filenameExtension = file_getext(QAFilenames[i]);
		// sectionName is the filename without the extension, but the section name can also be specified by
		// JSON metadata in the Q&A file
		var sectionName = file_getname(QAFilenames[i]);
		var charIdx = sectionName.lastIndexOf(".");
		if (charIdx > -1)
			sectionName = sectionName.substring(0, charIdx);

		// Open the file to see if it has a JSON metadata section specifying a section name, etc.
		// Note: If its metadata has an "ars" setting, then we'll use that instead of
		// any ARS setting that may be in gttrivia.ini for this section.
		var sectionARS = null;
		var QAFile = new File(QAFilenames[i]);
		if (QAFile.open("r"))
		{
			var fileMetadataStr = "";
			var readingFileMetadata = false;
			var haveSeenAllFileMetadata = false;
			while (!QAFile.eof && !haveSeenAllFileMetadata)
			{
				var fileLine = QAFile.readln(2048);
				// I've seen some cases where readln() doesn't return a string
				if (typeof(fileLine) !== "string")
					continue;
				fileLine = fileLine.trim();
				if (fileLine.length == 0)
					continue;

				//-- QA metadata begin" and "-- QA metadata end"
				if (fileLine === "-- QA metadata begin")
				{
					fileMetadataStr = "";
					readingFileMetadata = true;
					continue;
				}
				else if (fileLine === "-- QA metadata end")
				{
					readingFileMetadata = false;
					haveSeenAllFileMetadata = true;
					continue;
				}
				else if (readingFileMetadata)
					fileMetadataStr += fileLine + " ";
			}
			QAFile.close();

			// If we've read all the file metadata lines, then parse it & use the metadata.
			if (haveSeenAllFileMetadata && fileMetadataStr.length > 0)
			{
				try
				{
					var fileMetadataObj = JSON.parse(fileMetadataStr);
					if (typeof(fileMetadataObj) === "object")
					{
						if (fileMetadataObj.hasOwnProperty("category_name") && typeof(fileMetadataObj.category_name) === "string")
							sectionName = fileMetadataObj.category_name;
						if (fileMetadataObj.hasOwnProperty("ARS") && typeof(fileMetadataObj.ARS) === "string")
							sectionARS = fileMetadataObj.ARS;
					}
				}
				catch (error) {}
			}
		}

		// See if there is an ARS string for this in the configuration, and if so,
		// only add it if the ARS string passes for the user.
		var addThisSection = true;
		if (typeof(sectionARS) === "string")
			addThisSection = bbs.compare_ars(sectionARS);
		else if (gSettings.category_ars.hasOwnProperty(sectionName))
			addThisSection = bbs.compare_ars(gSettings.category_ars[sectionName]);
		// Add this section/category, if allowed
		if (addThisSection)
		{
			// Capitalize the first letters of the words in the section name, and replace underscores with spaces
			var wordsArr = sectionName.split("_");
			for (var wordI = 0; wordI < wordsArr.length; ++wordI)
				wordsArr[wordI] = wordsArr[wordI].charAt(0).toUpperCase() + wordsArr[wordI].substr(1);
			sectionName = wordsArr.join(" ");
			sectionsAndFilenames.push({
				sectionName: sectionName,
				filename: QAFilenames[i]
			});
		}
	}
	return sectionsAndFilenames;
}

// TODO: Put the next 3 functions into a common JS library for use with this and
// qa_edit.js?
// Parses a Q&A file with questions and answers
//
// Parameters:
//  pQAFilenameFullPath: The full path & filename of the trivia Q&A file to read
//
// Return value: An array of objects containing the following properties:
//               question: The trivia question
//               answer: The answer to the trivia question
//               numPoints: The number of points to award for the correct answer
function parseQAFile(pQAFilenameFullPath)
{
	if (!file_exists(pQAFilenameFullPath))
		return [];

	// Open the file and parse it.  Each line has:
	// question,answer,points
	var QA_Array = [];
	var QAFile = new File(pQAFilenameFullPath);
	if (QAFile.open("r"))
	{
		var inFileMetadata = false; // Whether or not we're in the file metadata question (we'll skip all of this for the questions & answers)
		var theQuestion = "";
		var theAnswer = "";
		var theNumPoints = -1;
		var readingAnswerMetadata = false;
		var doneReadintAnswerMetadata = false; // For immediately after done reading answer JSON
		var answerIsJSON = false;
		var component = "Q"; // Q/A/P for question, answer, points
		while (!QAFile.eof)
		{
			var fileLine = QAFile.readln(2048);
			// I've seen some cases where readln() doesn't return a string
			if (typeof(fileLine) !== "string")
				continue;
			fileLine = fileLine.trim();
			if (fileLine.length == 0)
				continue;

			// Skip any file metadata lines (those are read in getQACategoriesAndFilenames())
			if (fileLine === "-- QA metadata begin")
			{
				inFileMetadata = true;
				continue;
			}
			else if (fileLine === "-- QA metadata end")
			{
				inFileMetadata = false;
				continue;
			}
			if (inFileMetadata)
				continue;

			if (theQuestion.length > 0 && theAnswer.length > 0 && theNumPoints > -1)
			{
				addQAToArray(QA_Array, theQuestion, theAnswer, theNumPoints, answerIsJSON);
				theQuestion = "";
				theAnswer = "";
				theNumPoints = -1;
				readingAnswerMetadata = false;
				doneReadintAnswerMetadata = false;
				answerIsJSON = false;
				component = "Q";
			}

			if (component === "Q")
			{
				theQuestion = fileLine;
				component = "A"; // Next, set answer
			}
			else if (component === "A")
			{
				// Possible JSON for multiple answers
				if (fileLine === "-- Answer metadata begin")
				{
					readingAnswerMetadata = true;
					answerIsJSON = true;
					theAnswer = "";
					continue;
				}
				else if (fileLine === "-- Answer metadata end")
				{
					readingAnswerMetadata = false;
					doneReadintAnswerMetadata = true;
				}
				if (readingAnswerMetadata)
					theAnswer += fileLine + " ";
				else
				{
					if (doneReadintAnswerMetadata)
						doneReadintAnswerMetadata = false;
					else
						theAnswer = fileLine;
					component = "P"; // Next, set points
				}
			}
			else if (component === "P")
			{
				theNumPoints = +(fileLine);
				if (theNumPoints < 1)
					theNumPoints = 10;
				component = "Q"; // Next, set question
			}

			// Older: Each line in the format question,answer,numPoints
			/*
			// Questions might have commas in them, so we can't just blindly split on commas like this:
			//var lineParts = fileLine.split(",");
			// Find the first & last indexes of a comma in the line
			var lastCommaIdx = fileLine.lastIndexOf(",");
			var firstCommaIdx = (lastCommaIdx > -1 ? fileLine.lastIndexOf(",", lastCommaIdx-1) : -1);
			if (firstCommaIdx > -1 && lastCommaIdx > -1)
			{
				QA_Array.push({
					question: fileLine.substring(0, firstCommaIdx),
					answer: fileLine.substring(firstCommaIdx+1, lastCommaIdx),
					numPoints: +(fileLine.substring(lastCommaIdx+1))
				});
			}
			*/
		}
		QAFile.close();
		// Ensure we've added the last question & answer, if there is one
		if (theQuestion.length > 0 && theAnswer.length > 0 && theNumPoints > -1)
			addQAToArray(QA_Array, theQuestion, theAnswer, theNumPoints, answerIsJSON);
	}
	return QA_Array;
}
// QA object constructor
function QA(pQuestion, pAnswer, pNumPoints, pAnswerFact)
{
	this.question = pQuestion;
	this.answer = pAnswer;
	this.numPoints = pNumPoints;
	this.answerFacts = [];
	if (this.numPoints < 1)
		this.numPoints = 10;
	if (typeof(pAnswerFact) === "string")
	{
		if (pAnswerFact.length > 0)
			this.answerFacts.push(pAnswerFact);
	}
	else if (Array.isArray(pAnswerFact))
		this.answerFacts = pAnswerFact;
}
// Helper for parseQAFile(): Adds a question, answer, and # points to the Q&A array
//
// Parameters:
//  QA_Array (INOUT): The array to add the Q/A/Point sets to
//  theQuestion: The question (string)
//  theAnswer: The answer (string)
//  theNumPoints: The number of points to award (number)
//  answerIsJSON: Boolean - Whether or not theAnswer is in JSON format or not (if JSON, it contains
//                multiple possible answers)
function addQAToArray(QA_Array, theQuestion, theAnswer, theNumPoints, answerIsJSON)
{
	// If the answer is a JSON object, then there may be multiple acceptable answers specified
	var addAnswer = true;
	var answerFact = null;
	if (answerIsJSON)
	{
		try
		{
			var answerObj = JSON.parse(theAnswer);
			if (typeof(answerObj) === "object")
			{
				if (answerObj.hasOwnProperty("answers") && Array.isArray(answerObj.answers) && answerObj.answers.length > 0)
				{
					// Make sure all answers in the array are non-zero length
					theAnswer = [];
					for (var i = 0; i < answerObj.answers.length; ++i)
					{
						if (answerObj.answers[i].length > 0)
							theAnswer.push(answerObj.answers[i]);
					}
					// theAnswer is an array
					addAnswer = (theAnswer.length > 0);
				}
				else if (answerObj.hasOwnProperty("answer") && typeof(answerObj.answer) === "string" && answerObj.answer.length > 0)
					theAnswer = answerObj.answer;
				else
					addAnswer = false;

				// Answer fact(s)
				if (answerObj.hasOwnProperty("answerFacts") && Array.isArray(answerObj.answerFacts))
					answerFact = answerObj.answerFacts;
				else if (answerObj.hasOwnProperty("answerFact") &&
					(typeof(answerObj.answerFact) === "string" || Array.isArray(answerObj.answerFact)) &&
					answerObj.answerFact.length > 0)
				{
					answerFact = answerObj.answerFact;
				}
			}
			else
				addAnswer = false;
		}
		catch (error)
		{
			addAnswer = false;
		}
	}
	if (addAnswer) // Note: theAnswer is converted to an object if it's JSON
		QA_Array.push(new QA(theQuestion, theAnswer, +theNumPoints, answerFact));
}
