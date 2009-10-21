/*******************************************************************************
FILE: emailval.js v0.2
BY  : Michael J. Ryan (http://tracker1.info/)
ON  : 2005-02-14

TABS: 4 character tabstops.

--------------------------------------------------------------------------------
This is a simple telnet email validation script.

Installation instructions for emailval.js (requires SBBS 3.12a+)

STEP 1:
First, make sure that you have two Security Levels setup in order to use the
Email validation system.. one setting for new users (unvalidated) and another
that is intended for validated users.

STEP 2:
Edit mailval.js in notepad and edit the following values to match your
pre-validation and post-validation security levels.
        LEVEL_BEFORE_VALIDATION and LEVEL_AFTER_VALIDATION

STEP 3:
add the following to the top of logon.js
    load("emailval.js");

or, if using logon.bin/src
    EXEC "?emailval.js"

That's it!
*******************************************************************************/

//should match USER LEVEL settings for you bbs
//	modify the next two lines to match your bbs settings
var	LEVEL_BEFORE_VALIDATION = 50;
var	LEVEL_AFTER_VALIDATION = 60;


//other constants, shouldn't need changing.
var cValChars='ACDEFHJKLMNPQRTUVWXY23456789!@#$%&*';
var cPrevalText = "telvalcode";
var cValCodeLen = 16;

//include SBBS Definition constants
load("sbbsdefs.js"); 

//gets the validation code in use, if any, otherwise sets/returns a new code.
function GetValidationCode() {
	var val = user.comment;
	if (val.indexOf(cPrevalText) == -1)
		return SetValidationCode(); //return a new code.
	
	//parse/return the existing code.
	return val.substr(val.indexOf(cPrevalText) + cPrevalText.length + 1, cValCodeLen);
}

//sets/returns a new validation code.
function SetValidationCode() {
	var val='';

	for (var i=0;i<cValCodeLen;i++)
		val+=cValChars.substr(parseInt(Math.random() * cValChars.length), 1);
		
	user.comment = cPrevalText + ":" + val;
	return val;
}

function SendValidationEmail() {
	var val = GetValidationCode(); //reuses the last code emailed, if available.
	
	/* Generate and send email */
	var hdrs = new Object();
	hdrs.to=user.name;
	hdrs.to_net_type=netaddr_type(user.netmail);
	if(hdrs.to_net_type!=NET_NONE) {
		hdrs.to_net_addr=user.netmail;
	} else {
		console.print("\r\n\1n\1h\1rCannot send validation code to new email address!\1n \r\n");
		console.pause();
		return;
	}
	hdrs.from=system.name;
	hdrs.from_net_addr='sysop@'+system.inet_addr;
	hdrs.from_net_type=NET_INTERNET;
	hdrs.subject="Email validation for " + system.name;
	var msgbase = new MsgBase("mail");
	if(msgbase.open!=undefined && msgbase.open()==false) {
		console.print("\r\n\1n\1h\1rERROR: \1y" + msgbase.last_error + "\1n \r\n");
		console.pause();
		msgbase.close();
		return;
	}
	var msg="CODE: " + val + "\n\n";
	msg += "You may now return to " + system.name + " and validate your account.";

	if(!msgbase.save_msg(hdrs,msg))  {
		console.print("\r\n\1n\1h\1rERROR: \1y" + msgbase.last_error + "\1n \r\n");
		console.pause();
		msgbase.close();
		return;
	}

	msgbase.close();
	console.print("\1n \r\n\1h\1bValidation code sent to \1n\1h" + user.netmail + "\1h\1b.\1n \r\n");
}

function ChangeEmailAddress() {
	var val = SetValidationCode(); //resets the last code used.

	console.print("\1n \r\b\1h\1bEnter your email address below.\r\n\1n\1b:\1n ");
	user.netmail = console.getstr(user.netmail, 60, K_EDIT | K_LINE);
	console.print("\r\n\1n \1\n");
}

function EnterValidationCode() {
	var val = GetValidationCode(); //SHOULD get the validation code sent.

	console.print("\1n \r\b\1h\1bEnter the validation code you were sent below.\r\n\1n\1b:\1n ");
	var valu = console.getstr("", cValCodeLen, K_EDIT | K_UPPER | K_LINE);
	if (val.toUpperCase() == valu.toUpperCase()) {
		console.print("\r\n\1n \r\n\1hValidated!\1n \r\n\r\n");
		user.security.level = LEVEL_AFTER_VALIDATION;
		user.comment = cPrevalText + ":" + val + " validated on " + (new Date());
	} else {
		console.print("\r\n\1n \r\n\1h\1rCode doesn't match!\1n \r\n");
	}
}

function HangupNow() {
	console.clear();
	console.print("\1nGoodbye.\r\n");
	bbs.hangup();
}

function CheckValidation() {
	while (bbs.online && user.security.level == LEVEL_BEFORE_VALIDATION) {
		console.clear();
		
		//NOTE: could use bbs.menu("FILENAME") to display an ansi here.
		console.print("\1h\1bTelnet validation for \1h\1w" + user.alias + " #" + user.number + "\r\n\r\n");
		console.print("\1nYou have created an account with an email address that must be validated.\r\n")
		console.print("\1n\1b[\1h\1wS\1n\1b] \1h\1bSend validation code to\1n\1c " + user.netmail + "\r\n");
		console.print("\1n\1b[\1h\1wV\1n\1b] \1h\1bValidate your account\r\n");
		console.print("\1n\1b[\1h\1wE\1n\1b] \1h\1bEdit/Update email address\r\n");
		console.print("\1n\1b[\1h\1wH\1n\1b] \1h\1bHangup\r\n");
		
		//display prompt.
		console.print("\r\n\1h\1yYour selection? ");
		var o = console.getkey().toUpperCase();
		
		console.print("\1n\1h\1c" + o + "\r\n");
		switch (o) {
			case "S":
				SendValidationEmail();
				break;
			case "V":
				EnterValidationCode();
				break;
			case "E":
				ChangeEmailAddress();
				break;
			case "H":
				HangupNow();
				return;
				break;
			default:
				console.print("\1h\1yInvalid Selection.\r\n");
		}
	}
}
CheckValidation();