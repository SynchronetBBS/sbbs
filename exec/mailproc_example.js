// mailproc_example.js

// Example SMTP "Mail Processor" module
// Requires Synchronet Mail Server 1.298 or later

// $Id$

// Set to false at any time to indicate a processing failure
var success=true;

// These lines open the processing error output file as a new File object.
// If there are any processing errors (e.g. filtered context, blocked sender),
// you can reject the message by simply writing some text to 'errfile'.
var errfile = new File(processing_error_filename);
if(!errfile.open("w"))
	exit();

// These lines read the recipient list into a new 'recipient' object array.
var rcptlst = new File(recipient_list_filename);
if(!rcptlst.open("r"))
	exit();
var recipient=rcptlst.iniGetAllObjects("number");
// At this point we can access the list of recipients very easily
// using the 'recipient' object array.

// These lines open the message text file in append mode (writing to the end)
var msgtxt = new File(message_text_filename);
if(!msgtxt.open("a"))	// Change thie mode to "r+" for "read/update" access
	exit();

// This an example of modifying the message text.
// In this case, we're adding some text (a dump of the recipient object array)
// to the end of the message.
msgtxt.writeln("\r\nHello from mailproc_example.js\r\n");
msgtxt.writeln("Array of recipient objects (message envelopes):\r\n");

// Dump recipient object array
for(i in recipient)			// For each recipient object...
	for(j in recipient[i])	// For each property...			
		msgtxt.writeln("recipient[" +i+ "]." +j+ "=" + recipient[i][j]);

// If there were any processing errors... reject the message
if(!success)
	errfile.writeln("A mail processing error occurred. Message rejected.");

// End of mailproc_example.js