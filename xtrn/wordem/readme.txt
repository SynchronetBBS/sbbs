Wordem
InterBBS Scrabble for Synchronet BBS 3.16+
echicken -at- bbs.electronicchicken.com

- This game is incomplete and is a work in progress.
- Currently, only randomly-paired two-player games are supported.
- Play & scoring are based on the Scrabble rules, with some minor deviations.

To Do:
------

- Notification to players on game completion
- Title screen
- Better menu / lobby screen
- Help / rules screen
- Recent games / high scores screen
- Non-random (invite a friend) matches
- Games with more than two players

Installation:
-------------

Configure a new external program as follows:

Wordem                      
------------------------------------------------------
 ¦Name                       Wordem                   
 ¦Internal Code              WORDEM                   
 ¦Start-up Directory         ../xtrn/wordem           
 ¦Command Line               *../xtrn/wordem/wordem.js
 ¦Clean-up Command Line                               
 ¦Execution Cost             None                     
 ¦Access Requirements                                 
 ¦Execution Requirements                              
 ¦Multiple Concurrent Users  Yes                      
 ¦Intercept I/O              No                       
 ¦Native Executable          No                       
 ¦Use Shell to Execute       No                       
 ¦Modify User Data           No                       
 ¦Execute on Event           No                       
 ¦Pause After Execution      No                       
 ¦BBS Drop File Type         None                     
 ¦Place Drop File In         Node Directory           

I recommend that you edit the server.ini file in this directory to read as
follows:

host=bbs.electronicchicken.com
port=10088

Hosting your own database (optional):
-------------------------------------

Disregard this section if you edited your server.ini file as suggested above.

If you choose to leave the server.ini file as is, you will need to host your
own Wordem database on a local JSON server.  If so, you must ensure that your
JSON server is configured in ctrl/services.ini:

[JSON]
Port=10088
Options=STATIC | LOOP
Command=json-service.js

You will also need to add a Wordem module to your JSON service, by placing the
following in ctrl/json-service.ini:

[wordem]
dir=../xtrn/wordem/