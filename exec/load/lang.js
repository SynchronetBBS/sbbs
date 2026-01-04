// Language support load library

"use strict";

function count()
{
	return directory(system.ctrl_dir + "text.*.ini").length + 1;
}

function list()
{
	var result = [""];
	var list = directory(system.ctrl_dir + "text.*.ini");
	const prefix_len = (system.ctrl_dir + "text.").length;
	for (var i in list) {
		result.push(list[i].slice(prefix_len, -4));
	}
	return result;
}

function desc_list()
{
	var result = [bbs.text(bbs.text.LANG, /* default: */true)];
	var list = directory(system.ctrl_dir + "text.*.ini");
	for (var i in list) {
		var f = new File(list[i]);
		if (!f.open("r"))
			continue;
		result.push(f.iniGetValue(null, "LANG", ""));
		f.close();
	}
	return result;
}

function select(usr)
{
	if (usr === undefined)
		usr = user;
	var lang = list();
	if (lang.length < 1)
		return;
	var i;
	for (i = 0; i < lang.length; ++i) {
		if (usr.lang == lang[i])
			break;
	}
	if (i >= lang.length)
		i = 0;
	var desc = desc_list();
	for (var j = 0; j < desc.length; ++j) {
		console.uselect(j, bbs.text(bbs.text.Language), desc[j]);
	}
	if ((j = console.uselect(i)) >= 0)
		usr.lang = lang[j];
	if (user.number === usr.number)
		bbs.load_user_text();
}

this;
