/*******************************************************************************
Originally based on:
FILE: emailval.js v0.2
BY  : Michael J. Ryan (http://tracker1.info/)
ON  : 2005-02-14

TABS: 4 character tabstops.

--------------------------------------------------------------------------------
This is a simple email validation script.

Installation instructions for emailval.js (requires SBBS 3.12a+)

STEP 1:
First, make sure that you have two Security Levels setup in order to use the
Email validation system.. one setting for new users (unvalidated) and another
that is intended for validated users.

STEP 2:
Edit ctrl/modopts.ini in a text editor and edit the following values in the
[emailval] section (create if it necessary) to match your pre-validation and
post-validation security levels.
        level_before_validation (default: 50)
		level_after_validation (default: 60)
		flags1_after_validation (default: no change)
		flags2_after_validation (default: no change)
		flags3_after_validation (default: no change)
		flags4_after_validation (default: no change)
		exemptions_after_validation (default: no change)
		restrictions_after_validation (default: no change)
		expiration_after_validation (default: false)
		expiration_days_after_validation (default: no change)
		valid_chars=ACDEFHJKLMNPQRTUVWXY23456789!@#$%&*
		code_length=16

Note: the flags, exemptions, and restrictions .ini values support 'A' through
      'Z' with the optional '+' (add) and '-' (remove) modifiers.
	      e.g. "+A-B" to add the A flag and remove the B flag
	      e.g. "AB" to change the flag set to just "AB"
	  Numeric values are supported for assignment (not modification).
	      e.g. 0 = no flags, 1 = A, 2 = B, 4 = C, 8 = D, etc.

STEP 3:
add the following to the [logon] section of your ctrl/modopts.ini file:
    email_validation = true

or, if using logon.bin/src
    EXEC "?emailval.js"

That's it!
*******************************************************************************/

// Configure in ctrl/modopts/emailval.ini

const module = "emailval";
var prompts = bbs.mods.prompts || load(bbs.mods.prompts = {}, "user_info_prompts.js");
var userprops = bbs.mods.userprops || load(bbs.mods.userprops = {}, "userprops.js");
var options = load("modopts.js", module);
if(!options)
	options = {};
if(options.level_before_validation === undefined)
	options.level_before_validation = 50;
if(options.level_after_validation === undefined)
	options.level_after_validation = 60;
options.valid_chars = load("modopts.js", module, "valid_chars", 'ACDEFHJKLMNPQRTUVWXY23456789!@#$%&*');

prompts.operation = "";

var cValCodeLen = options.code_length || 16;

require("sbbsdefs.js", 'NET_NONE'); 

//gets the validation code in use, if any, otherwise sets/returns a new code.
function GetValidationCode() {
	var val = userprops.get(module, "code");
	if (!val)
		return SetValidationCode(); //return a new code.
	
	// return the existing code.
	return val;
}

//sets/returns a new validation code.
function SetValidationCode() {
	var val='';

	for (var i=0;i<cValCodeLen;i++)
		val+=options.valid_chars.substr(parseInt(Math.random() * options.valid_chars.length), 1);
		
	userprops.set(module, "code", val);
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
	userprops.set(module, "sent", new Date());
	userprops.set(module, "to", user.netmail);
}

function ChangeEmailAddress() {
	var val = SetValidationCode(); //resets the last code used.

	while (bbs.online && !js.terminated) {
		prompts.get_netmail(user);
		if (system.check_netmail_addr(user.netmail))
			break;
		alert("Sorry, that is an unsupported netmail address");
	}
}

function EnterValidationCode() {
	var val = GetValidationCode(); //SHOULD get the validation code sent.

	console.print(options.entercode || "\1n \r\b\1h\1bEnter the validation code you were sent below.\r\n\1n\1b:\1n ");
	var valu = console.getstr("", cValCodeLen, K_EDIT | K_UPPER | K_LINE);
	if (val.toUpperCase() == valu) {
		console.newline();
		console.print(options.validated || "\1hValidated!\1n");
		if(user.security.level < options.level_after_validation)
			user.security.level = options.level_after_validation;
		if(options.flags1_after_validation)
			user.security.flags1 = options.flags1_after_validation;
		if(options.flags2_after_validation)
			user.security.flags2 = options.flags2_after_validation;
		if(options.flags3_after_validation)
			user.security.flags3 = options.flags3_after_validation;
		if(options.flags4_after_validation)
			user.security.flags4 = options.flags4_after_validation;
		if(options.exemptions_after_validation)
			user.security.exemptions = options.exemptions_after_validation;
		if(options.restrictions_after_validation)
			user.security.restrictions = options.restrictions_after_validation;
		if(options.expiration_after_validation == true) {
			if(options.expiration_days_after_validation)
				user.security.expiration_date = time() + options.expiration_days_after_validation * 24 * 60 * 60;
		} else
			user.security.expiration_date = 0;
		user.adjust_credits(options.credits_added_after_validation || 0);
		user.adjust_minutes(options.minutes_added_after_validation || 0);
		
		user.comment = "Email validated on " + (new Date());
		userprops.set(module, "validated", new Date());
	} else {
		console.print(options.nomatch || "\r\n\1n \r\n\1h\1rCode doesn't match!\1n \r\n");
	}
}

function HangupNow() {
	console.clear();
	console.print(options.goodbye || "\1nGoodbye.");
	console.newline();
	bbs.hangup();
}

function CheckValidation() {
	while (bbs.online
		&& ((user.security.level == options.level_before_validation && !userprops.get(module, "validated"))
			|| argv.indexOf("-test") >= 0)) {
		console.clear();
		
		var keys ="EH";
		if (!bbs.menu("emailval", P_NOERROR)) {
			console.print("\1h\1bEmail validation for \1h\1w" + user.alias + " #" + user.number + "\r\n\r\n");
			console.print("\1nYou have created an account with an email address that must be validated.\r\n");
		}
		if (!system.check_netmail_addr(user.netmail)) {
			console.print(format(bbs.text(bbs.text.InvalidNetMailAddr), user.netmail));
			console.newline();
		} else {
			console.newline();
			console.print("\1n\1b[\1h\1wS\1n\1b] \1h\1bSend validation code to\1n\1c " + user.netmail + "\r\n");
			keys += "S";
		}
		if (userprops.get(module, "sent")) {
			console.print("\1n\1b[\1h\1wV\1n\1b] \1h\1bValidate your account\r\n");
			keys += "V";
		}
		console.print("\1n\1b[\1h\1wE\1n\1b] \1h\1bEdit/Update email address\r\n");
		console.print("\1n\1b[\1h\1wH\1n\1b] \1h\1bHangup\r\n");
		
		console.newline();
		console.print(options.prompt || "\1h\1yYour selection? \1w");
		switch (console.getkeys(keys, 0)) {
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
		}
	}
}
CheckValidation();