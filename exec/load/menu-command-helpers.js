load("sbbsdefs.js");

// To-do:
// findFiles Prompt for filespec/search string bbs.list_files(filespec or string, FL_FINDDESC|FL_VIEW)
// select file group & directory

// I'm not sure about the handling of bbs.curgrp vs. msg_area.grp_list index here
var selectMessageGroup = function() {

	console.putmsg(bbs.text(133));
	for(var g = 0; g < msg_area.grp_list.length; g++) {
		console.putmsg(
			format(
				((g == bbs.curgrp) ? " * " : "   ") + bbs.text(134),
				g + 1,
				msg_area.grp_list[g].description
			)
		);
	}
	console.mnemonics(
		format(
			bbs.text(503),
			bbs.curgrp + 1
		)
	);
	var g = console.getkeys("Q", msg_area.grp_list.length);
	if(g != "Q" && g != "")
		bbs.curgrp = parseInt(g) - 1;

}

// Not sure about the handling of bbs.cursb vs sub_list index here
var selectMessageArea = function() {

	console.putmsg(format(bbs.text(125), msg_area.sub[bbs.cursub_code].grp_name));
	for(var s = 0; s < msg_area.grp_list[bbs.curgrp].sub_list.length; s++) {
		var mb = new MsgBase(msg_area.grp_list[bbs.curgrp].sub_list[s].code);
		mb.open();
		var tm = mb.total_msgs;
		mb.close();
		console.putmsg(
			format(
				((s == bbs.cursub) ? " * " : "   ") + bbs.text(126),
				s + 1,
				msg_area.grp_list[bbs.curgrp].sub_list[s].description,
				"",
				tm
			)
		);
	}
	console.mnemonics(
		format(
			bbs.text(503),
			bbs.cursub + 1
		)
	);
	var s = console.getkeys("Q", msg_area.grp_list[bbs.curgrp].sub_list.length);
	if(s != "Q" && s != "")
		bbs.cursub = parseInt(s) - 1;

}

var selectGroupAndArea = function() {
	selectMessageGroup();
	selectMessageArea();
}

var scanSubs = function() {

	console.putmsg(bbs.text(116));
	console.crlf();
	var youOnly = (console.noyes("To you only")) ? 0 : SCAN_TOYOU;
	bbs.scan_subs(SCAN_NEW|youOnly);

}

var sendMail = function() {

	console.putmsg(bbs.text(10));
	console.crlf();
	var nameOrNumber = console.getstr("", 30, K_EDIT|K_LINE);
	if(isNaN(parseInt(nameOrNumber)))
		nameOrNumber = system.matchuser(nameOrNumber);
	if(nameOrNumber < 1) {
		console.crlf();
		console.putmsg(bbs.text(30));
		return;
	}
	bbs.email(nameOrNumber, WM_EMAIL);
	
}

var sendNetMail = function() {

	console.putmsg("Netmail/email address:");
	console.crlf();
	var name = console.getstr("", console.screen_columns - 1, K_EDIT|K_LINE);
	bbs.netmail(name, WM_NONE);
	
}

var findMessages = function() {

	var findInSub = function(sub) {
		subs++;
		if(subjectsOnly) {
			bbs.list_msgs(sub, SCAN_FIND, text);
			return true;
		} else {
			return bbs.scan_msgs(sub, SCAN_FIND, text);
		}
	}

	var findInGroup = function(grp) {
		for(var sub in msg_area.grp_list[grp].sub_list) {
			var ret = findInSub(msg_area.grp_list[grp].sub_list[sub].code);
			if(!ret)
				break;
		}
		return (typeof ret == "undefined") ? true : ret;
	}

	var findInAll = function() {
		for(var grp in msg_area.grp_list) {
			var ret = findInGroup(grp);
			if(!ret)
				break;
		}
		return (typeof ret == "undefined") ? true : ret;
	}

	var main = function() {
		console.mnemonics(bbs.text(621));
		var sga = console.getkeys("SGA");
		console.putmsg(bbs.text(76));
		console.crlf();
		text = console.getstr(
			"",
			console.screen_columns - 1,
			K_EDIT|K_LINE|K_UPPER
		);
		if(text == "")
			return false;
		console.crlf();
		subjectsOnly = console.yesno(bbs.text(625));
		var ret = true;
		switch(sga) {
			case "S":
				ret = findInSub(bbs.cursub_code);
				break;
			case "G":
				ret = findInGroup(bbs.curgrp);
				break;
			case "A":
				ret = findInAll();
				break;
			default:
				break;
		}
		return ret;
	}

	var complete = function(ret) {
		console.putmsg(
			bbs.text(116) +
			((ret) ? format(bbs.text(117), subs) : bbs.text(118))
		);
		console.pause();
	}

	var text = "";
	var subs = 0;
	var subjectsOnly = true;
	complete(main());

}

var findUser = function() {

	console.putmsg("Full or partial username:");
	console.crlf();
	var name = console.getstr("", 30, K_EDIT|K_LINE);
	if(name == "") {
		console.putmsg(bbs.text(30));
		return;
	}
	bbs.finduser(name);

}
