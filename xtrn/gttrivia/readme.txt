                               Good Time Trivia
                                 Version 1.06
                           Release date: 2026-02-04

                                     by

                                Eric Oulashin
                                 AKA Nightfox
                          Sysop of Digital Distortion
                  BBS internet address: digitaldistortionbbs.com
                     Alternate address: digdist.bbsindex.com
                        Email: eric.oulashin@gmail.com



This file describes Good Time Trivia.

Contents
========
1. Disclaimer
2. Introduction
3. Installation & Setup
   - Installation in SCFG
4. Configuration file
5. Optional: Configuring your BBS to host player scores
   - Only do this if you would prefer to host scores on your BBS rather than
     using the inter-BBS scores hosted on Digital Distortion
6. dump_questions.js


1. Disclaimer
=============
I cannot guarantee that this script is 100% free of bugs.  However, I have
tested it, and I used it often during development, so I have some confidence
that there are no serious issues with it (at least, none that I have seen).


2. Introduction
===============
Good Time Trivia is a trivia door game written for Synchronet, in JavaScript
(so it will run anywhere Synchronet runs).  Good Time Trivia has a question-
and-answer format and can give the user multiple tries to answer each question,
with clues provided after incorrect answers (the clue will start off with a
totally masked answer, and then reveal one letter at a time for each incorrect
answer given).

Multiple trivia categories are supported, and the user can choose one when
starting the game.  Each category of questions is contained in its own text
file.  If there is only one category, then the game will auto-select that
category when a user enters the game.

Player scores are saved to a file called scores.json in the same directory as
the script.  The scores file is in JSON format.

In the main menu, there will be an extra option for the sysop to clear the high
scores.  This will delete the scores.json file.

This is currently a single-player game, but multiple users on different nodes
can play it simultaneously.

Answer matching: When a user answers a question, the game can allow non-exact
answer matching in some circumstances, to account for typos and spelling
mistakes.  If the answer is a single word up to 12 characters, the game will
require an exact match for the answer; otherwise, the game will check for a
Levenshtein distance up to 2 in order to determine if the user entered the
correct answer.  The Levenshtein distance checking can be adjusted by changing
the value of the MAX_LEVENSHTEIN_DISTANCE variable in gttrivia.js; however,
increasing it too much would mean allowing wrong answers as correct in some
cases.
For more information on Levenshtein distances:
https://www.cuelogic.com/blog/the-levenshtein-algorithm


Shared game scores on a server BBS
----------------------------------
The game can be configured to share its local game scores, to be stored on a
remote BBS.  The scores can be shared either by directly contacting the remote
BBS (via the JSON DB service) and/or by posting the local game scores in one or
more (networked) message sub-boards (both configurable in gttrivia.ini).  The
option to post scores in a message sub-board is a backup in case the remote BBS
is down and cannot be contacted directly.  You may also opt to just have Good
Time Trivia post scores in a message sub-board and not configure a remote BBS
server.

Digital Distortion (BBS) is set up to host scores for this game, and the
default settings in the REMOTE_SERVER section of gttrivia.ini point to
Digital Distortion, to use the direct-connect JSON DB method to update remote
scores.

Digital Distortion is also set up to look for scores for this game in the
Dove-Net Synchronet Data message area, as well as FSXNet Inter-BBS Data.

If your BBS has the Dove-Net message sub-boards, you could configure Good Time
Trivia to post scores in the Dove-Net Synchronet Data sub-board (using the
internal code configured on your BBS).  If there are other BBSes besides Digital
Distortion hosting scores, the host BBS would also need to have Dove-Net and
have an event configured to periodically run Good Time Trivia to poll Dove-Net
Synchronet Data and read the game scores.

By default, the game is set up to post scores to Digital Distortion, so you may
choose to view the scores there or host scores yourself. See section 4
(Configuration file) for more information on the options to send scores to a
remote BBS.

When configured to send user scores to a remote BBS and to write scores to a
message sub-board, that happens whenever a user stops playing a game.  The logic
for sending the scores is as follows:
- Try to post the scores to the remote BBS
- If that fails, then post the scores in the configured message sub-board, if
  there is one configured.
That logic should ensure that the scores get posted.  The remote BBS should then
be configured to have their JSON-DB service running and/or have Good Time Trivia
periodically scan the same (networked) message sub-board to read the scores.


3. Installation & Setup
=======================
Aside from readme.txt and revision_history.txt, Good Time Trivia is comprised
of the following files and directories:

1. gttrivia.js            The Good Time Trivia script (this is the script to
                          run)

2. gttrivia.example.ini   The configuration file (in INI format). This is an
                          example configuration file; to make your own changes
                          to your configuration, it is recommended that you
                          copy this file to gttrivia.ini and make your changes
                          in gttrivia.ini.

3. gttrivia.asc           The logo/startup screen to be shown to the user.
                          This is in Synchronet attribute code format.

4. install-xtrn.ini       A configuration file for automated installation into
                          Synchronet's external programs section; for use with
                          install-xtrn.js.  This is optional.

5. dump_questions.js      This is a script that will load a .qa file and write
                          out all the questions & their information. This is
                          meant to be run on the server with jsexec. This is
                          mainly for debugging & informational purposes. See
                          section "6. dump_questions.js" for more information
                          on using this script.

6. qa                     This is a subdirectory that contains the trivia
                          question & answer files.  Each file contains a
                          collection of questions, answers, and number of
                          points for each question.  Each filename must have
                          the general format of category_name.qa, where
                          category_name is the name of the category of
                          questions (underscores are required between each
                          word).  The filename extension must be .qa .

7. server                 This directory contains a couple scripts that are
                          used if you enable the gttrivia JSON database (for
                          hosting game scores on a Synchronet BBS so that scores
                          from players on multiple BBSes can be hosted and
                          displayed).  As of this writing, I host scores for
                          Good Time Trivia on my BBS (Digital Distortion), so
                          unless you really want to do your own score hosting,
                          you can have your installation of the game read
                          inter-BBS scores from Digital Distortion.


The trivia category files (in the qa directory, with filenames ending in .qa)
are plain text files.
Optionally, a QA file may have a section of metadata specified in JSON
(JavaScript Object Notation) format that can contain some information about the
question category. The information can have the following properties:
category_name: The name of the category (if different from simple parsing of
               the filename)
ARS: Optional - An ARS security string to restrict access to the question set.
     This overrides any setting for the question set in the [CATEGORY_ARS]
     section in gttrivia.ini.

This JSON must be between two lines:
-- QA metadata begin
-- QA metadata end

For example, for a test category you might want to restrict to only the sysop:
-- QA metadata begin
{
    "category_name": "Test category (not meant for use)",
	"ARS": "SYSOP"
}
-- QA metadata end



The questions and answers inside the QA files contain questions, their answers,
and their number of points.  For eqch question in a category file, the main
format is 3 lines:
Question
Answer
Number of points

A blank line after each set of 3 lines is optional.  For example, a question
in one of the files might be as follows (this is a simple example):
What color is the sky?
Blue
5

Text within the question can be emphasized with ** characters around it. For
example:
What color is the **sky**?
In that question, the word "sky" will be emphasized when printed (and the **
characters around it will not be printed).  The colors/attributes for emphasized
text in questions can be set using the questionEmphasizedText setting in the
color settings.

Alternately, the answer can be specified via JSON (JavaScript Object Notation)
with multiple acceptable answers, and optionally a fact about the answer. When
JSON is specified for the answer, there need to be two text lines to specify
that the answer is JSON:
-- Answer metadata begin
-- Answer metadata end

One example where this can be used is to specify an answer that is a number and
you want to allow both spelling out the number and the number itself. Also, in
some cases it can be good to specify the spelled-out version first (if it's
short) since that will be used for the clue. For example:

How many sides does a square have?
-- Answer metadata begin
{
    "answers": ["Four", "4"]
}
-- Answer metadata end
5


An example of a question specifying an answer with a fact specified (note that
it's a string in an array with the property name "answerFacts"):
Who's picture is on the US $50 bill?
-- Answer metadata begin
{
    "answers": ["Ulysses Grant", "Ulysses S. Grant", "Ulysses S Grant"],
	"answerFacts": ["The US capitol building is on the other side of the $50 bill"]
}
-- Answer metadata end
5

Since "answerFacts" is an array, it can contain multiple facts:
Who's picture is on the US $50 bill?
-- Answer metadata begin
{
    "answers": ["Ulysses Grant", "Ulysses S. Grant", "Ulysses S Grant"],
	"answerFacts": ["The US capitol building is on the other side of the $50 bill",
	                "The numeral "50" in the lower right corner shifts from copper to green as the angle changes."]
}
-- Answer metadata end
5

A property with the name "answerFact" (not plural) is also supported, and this can be a
string or an array of strings:
Who's picture is on the US $50 bill?
-- Answer metadata begin
{
    "answers": ["Ulysses Grant", "Ulysses S. Grant", "Ulysses S Grant"],
	"answerFact": "The US capitol building is on the other side of the $50 bill"
}
-- Answer metadata end
5

NOTE: The questions and answers must be specified in exactly either of the two
above formats, or else the questions and answers will not be read correctly.

Also, there is a script in the qa directory called converter.js.  This is a
rough script that can be modified to convert a list of trivia questions into
the format that can be used by this door.  It is not a generic script; if you
find a list of questions & answers you might want to add to the game, and you
have some programming knowledge, you could modify this script to convert the
list of questions & answers into the format required by this door.


The configuration file, gttrivia.ini, is a plain text file, in INI format.
There are 2 sections:
[BEHAVIOR] - This section contains behavioral options for the game
[COLORS] - This section specifies colors for various text/items
[CATEGORY_ARS] - This section can specify a lit of trivia categories (filenames
                 without the .qa extension) and an ARS string to restrict
                 access to some trivia categories, if desired


The .js script, .ini file, and qa subdirectory can be placed together in any
directory.  When the game reads the configuration file & theme file, the game
will first look in your sbbs/mods directory, then sbbs/ctrl, then in the same
directory where the .js script is located.  So, if you desire, you could place
gttrivia.js in sbbs/exec and the .ini file sin your sbbs/ctrl directory, for
example.  Note, though, that the qa directory must be in the same directory as
gttrivia.js.


Installation in SCFG
--------------------
This is an example of adding the game in one of your external programs sections
in SCFG (note that the 'Native Executable/Script value doesn't matter for JS
scripts):
╔═════════════════════════════════════════════════════[< >]╗
║                     Good Time Trivia                     ║
╠══════════════════════════════════════════════════════════╣
║ │Name                       Good Time Trivia             ║
║ │Internal Code              GTTRIVIA                     ║
║ │Start-up Directory         ../xtrn/gttrivia             ║
║ │Command Line               ?gttrivia.js                 ║
║ │Clean-up Command Line                                   ║
║ │Execution Cost             None                         ║
║ │Access Requirements                                     ║
║ │Execution Requirements                                  ║
║ │Multiple Concurrent Users  Yes                          ║
║ │I/O Method                 FOSSIL or UART               ║
║ │Native Executable/Script   Yes                          ║
║ │Use Shell or New Context   No                           ║
║ │Modify User Data           No                           ║
║ │Execute on Event           No                           ║
║ │Pause After Execution      No                           ║
║ │Disable Local Display      No                           ║
║ │BBS Drop File Type         None                         ║
╚══════════════════════════════════════════════════════════╝


That is all you need to do to get Good Time Trivia running.


Optional
--------
As mentioned in the introduction, server scores can be sent to a remote BBS so
that scores from players on multiple BBSes can be viewed. Normally, if Good Time
Trivia is unable to connect to the remote BBS directly, it will fall back to
posting scores in a networked message sub-board (if configured) as a backup
option. That happens automatically after a user finishes playing a game, but
Good Time Trivia can also (optionally) be configured to post game scores in a
sub-board as a timed event by running gttrivia.js with the
-post_scores_to_subboard command-line argument.


4. Configuration File
=====================
gttrivia.ini is the configuration file for the door game.  There are 3 sections:
[BEHAVIOR], [COLORS], and [CATEGORY_ARS].  The settings are described below:

[BEHAVIOR] section
------------------
Setting                           Description
-------                           -----------
numQuestionsPerPlay               The maximum number of trivia questions
                                  allowed each time a user plays a game

numTriesPerQuestion               The maximum number of times a user is
                                  allowed to try to answer a question. After
                                  the user gives a wrong answer the first
                                  time, Good Time Trivia will start
                                  showing clues.

maxNumPlayerScoresToDisplay       The maximum number of player scores to display
                                  in the list of high scores

scoresMsgSubBoardsForPosting      This can be used to specify a comma-separated
                                  list of internal sub-board codes for the game
                                  to post user scores in, if you want your user
                                  scores to be shared. In case the remote BBS
                                  in the REMOTE_SERVER setting can't be reached
                                  or there is no remote BBS configured, the game
                                  will post scores in the sub-board(s) specified
                                  here. You can specify more than one sub-board
                                  code in case there are multiple BBSes that
                                  host scores for this game.
                                  Note that this setting is empty by default,
                                  because internal sub-board codes are probably
                                  different on each BBS. Digital Distortion is
                                  set up to host scores for this game, and if
                                  your BBS is connected to Dove-Net, it is
                                  recommended to use your BBS's internal code
                                  code for the Dove-Net Synchronet Data
                                  sub-board here. FSXNet also has an InterBBS
                                  Data area that might be used for conveying
                                  game scores to a host BBS.

[COLORS] section
----------------
In this section, the color codes are simply specified by a string of color
(attribute) code characters (i.e., YH for yellow and high).  See this page for
Synchronet attribute codes:
http://wiki.synchro.net/custom:ctrl-a_codes

Setting                           Element applied to
-------                           -------------------
error                             Errors
triviaCategoryHdr                 Text for the trivia category
triviaCategoryListNumbers         Text for the numbers in the category list
triviaCategoryListSeparator       Separator text after the numbers in the
                                  category list
triviaCategoryName                Category names in the category list
categoryNumPrompt                 Prompt text for the trivia category number
                                  input
categoryNumPromptSeparator        Separator text after the prompt for the
                                  category number input
categoryNumInput                  User input for the category number
questionHdr                       Header text for the trivia questions
questionHdrNum                    Numbers in the trivia question header text
question                          Trivia question
questionEmphasizedText            Emphasized text (i.e., **emphasized**) in
                                  trivia questions
answerPrompt                      Prompt text for trivia answers
answerPromptSep                   Separator text in trivia answer prompt
answerInput                       User input for trivia answers
userScore                         Outputted user score
scoreSoFarText                    "Your score so far" text
clueHdr                           Header text for clues
clue                              Clue text
answerAfterIncorrect              The answer printed after incorrect response
answerFact                        Fact displayed after an answer (not all
                                  questions will have one)

[CATEGORY_ARS] section
----------------------
In this section, the format is section_name=ARS string
section_name must match the part of a filename in the qa directory without the
filename extension.  The ARS string is a string that Synchronet uses to describe
access requirement(s).  The ARS strings in this section can be used to set up
access requirements for certain trivia categories (i.e., if there are any
categories that you may want age-restricted, for instance).  See the following
web page for documentation on Synchronet's ARS strings:
http://wiki.synchro.net/access:requirements

[REMOTE_SERVER] section
-----------------------
This section is used for specifying a remote server to connect to.  Currently,
this is used for writing & reading player scores on the remote system.  Inter-
BBS scores (retrieved from the remote system) can be optionally viewed when a
user is viewing scores.

Setting                           Description
-------                           -----------
server                            The server hostname/IP address.  The default
                                  value can be used if you want to use Digital
                                  Distortion BBS.

port                              The port number to use to connect to the
                                  remote host.  The default value is set for
                                  Digital Distortion.

[SERVER] section
----------------
This section is only used if you decide to host scores for Good Time Trivia.

Setting                           Description
-------                           -----------
deleteScoresOlderThanDays         The number of days to keep old player scores.
                                  The background service will remove player
                                  scores older than this number of days.

scoresMsgSubBoardsForReading      This can be used to specify a comma-separated
                                  list of internal sub-board codes for the game
                                  to read user scores from, for client BBSes
                                  that post their game scores there. See section
                                  5 (Optional: Configuring your BBS to host
                                  player scores) for information on adding an
                                  event in SCFG to periodically read scores
                                  from the sub-board(s) if you want to host game
                                  scores on your BBS. By default, the game is
                                  set up to post scores to Digital Distortion,
                                  so you may choose to view the scores there or
                                  host scores yourself.


5. Optional: Configuring your BBS to host player scores
=======================================================
You should only do this if you would prefer to host scores on your BBS rather
than using the inter-BBS scores hosted on Digital Distortion.

If you want to host player scores for Good Time Trivia, first ensure you have a
section in your ctrl/services.ini configured to run json-service.js (usually
with a name of JSON, or JSON in the name):

[JSON]
Port=10088
Options=STATIC | LOOP
Command=json-service.js

Note that those settings configure it to run on port 10088.  Also, you might
already have a similar section in your services.ini if you currently host any
JSON databases.

Then, open your ctrl/json-service.ini and add these lines to enable the gttrivia
JSON dtaabase:

[gttrivia]
dir=../xtrn/gttrivia/server/

It would then probably be a good idea to stop and re-start your Synchronet BBS
in order for it to recognize that you have a new JSON database configured.


Periodic reading of scores from a message sub-board
---------------------------------------------------
BBSes with the game installed could configure their game to post scores in a
(networked) message sub-board.  If you decide to host game scores on your BBS,
it's also a good idea to configure your BBS to read scores from a networked
message sub-board (which would need to be the same one that BBSes post in). For
instance, you could set it up to read scores from Dove-Net Synchronet Data,
FSXNet InterBBS Data, or perhaps another networked sub-board that is meant to
carry BBS data.

To specify which sub-board(s) to read scores from, you can specify those as a
comma-separated list of internal sub-board codes using the
scoresMsgSubBoardsForReading setting under the [SERVER] section of
gttrivia.ini.

Then, in SCFG, you will need to configure an event to run periodically to run
gttrivia.js to read those message sub-boards for game scores. You can do that
in SCFG > External Programs > Timed Events. Set it up to run gttrivia.js with
the command-line parameter -read_scores_from_subboard
Add an event as follows (internal code can be what you want; GTRIVSIM is short
for Good Time Trivia scores import):

╔═══════════════════════════════════════════════════════════════[< >]╗
║                        GTRIVSIM Timed Event                        ║
╠════════════════════════════════════════════════════════════════════╣
║ │Internal Code                   GTRIVSIM                          ║
║ │Start-up Directory              ../xtrn/gttrivia                  ║
║ │Command Line                    ?gttrivia.js -read_scores_from_subboard
║ │Enabled                         Yes                               ║
║ │Execution Node                  12                                ║
║ │Execution Months                Any                               ║
║ │Execution Days of Month         Any                               ║
║ │Execution Days of Week          All                               ║
║ │Execution Frequency             96 times a day                    ║
║ │Requires Exclusive Execution    No                                ║
║ │Force Users Off-line For Event  No                                ║
║ │Native Executable/Script        Yes                               ║
║ │Use Shell or New Context        No                                ║
║ │Background Execution            No                                ║
║ │Always Run After Init/Re-init   No                                ║
║ │Error Log Level                 Error                             ║
╚════════════════════════════════════════════════════════════════════╝

The number of times per day is up to you, but it would probably be beneficial
for this to happen frequently so that scores are kept up to date. In the above
example, 96 times per day would mean it would run every 15 minutes.


6. dump_questions.js
====================
dump_questions.js is a utility script, meant to be run on your BBS machine using
jsexec, which will read one of the .qa files and write out all the questions and
their information.

The general syntax of running this script is as follows:

jsexec dump_questions.js <qa_filename>

You can also leave off the .js filename extension:

jsexec dump_questions <qa_filename>

<qa_filename> is the name of one of the .qa files.  Assuming you run the script
from the gttrivia directory, you can specify just the name of one of the .qa
files (leaving out the full path), and the script will look in the qa directory
if the .qa file doesn't exist in the current directory.

For exmple, if you run it with just "general.qa", it will look for qa/general.qa
and read it:

jsexec dump_questions.js general.qa

If running in the gttrivia directory, you can also specify the qa directory if
you want to:

Linux:
jsexec dump_questions.js ./qa/general.qa

Windows:
jsexec dump_questions.js qa\general.qa


You can also change directory to the qa directory and run it from there,
specifying one of the filenames in there:

cd qa
jsexec ../dump_questions.js general.qa

In Windows, you may need to use a \ rather than a /.

