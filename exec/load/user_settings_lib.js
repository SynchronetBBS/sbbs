// This is a library of functions to allow the user to change their settings.
//
// *** NOTE - This will only work on Synchronet v3.20 or above!

"use strict";

require("gettext.js", "gettext");


// Prompts the user for their preferred download protocol & auto-hangup setting
//
// Parameters:
//  pUserObj: Optional - A User object representing the current user
//  pFileCfg: Optional - A cfglib object with file.ini loaded
function prompt_user_for_download_protocol(pUserObj, pFileCfg)
{
	var thisuser;
	if (typeof(pUserObj) === "object" && pUserObj.hasOwnProperty("download_protocol"))
		thisuser = pUserObj;
	else
		thisuser = new User(user.number);

	var file_cfg;
	if (typeof(pFileCfg) === "object" && pFileCfg.hasOwnProperty("protocol"))
		file_cfg = pFileCfg;
	else
	{
		var cfglib = load({}, "cfglib.js");
		file_cfg = cfglib.read("file.ini");
	}

	var c = 0;
	var keylist = 'Q';
	console.newline();
	console.print(gettext("Choose a default file transfer protocol (or [ENTER] for None):"));
	console.newline(2);
	for (var code in file_cfg.protocol)
	{
		if (!thisuser.compare_ars(file_cfg.protocol[code].ars) || file_cfg.protocol[code].dlcmd.length === 0)
			continue;
		console.putmsg(format(bbs.text(bbs.text.TransferProtLstFmt), String(file_cfg.protocol[code].key), file_cfg.protocol[code].name));

		keylist += String(file_cfg.protocol[code].key);
		if (c % 2 === 1)
			console.newline();
		++c;
	}
	console.mnemonics(bbs.text(bbs.text.ProtocolOrQuit));
	var kp = console.getkeys(keylist);
	if (kp !== 'Q' && !console.aborted)
	{
		thisuser.download_protocol = kp;
		if (kp && console.yesno(bbs.text(bbs.text.HangUpAfterXferQ)))
			thisuser.settings |= USER_AUTOHANG;
		else
			thisuser.settings &= ~USER_AUTOHANG;
	}
}

this;
