// $Id: logonlist_lib.js,v 1.2 2019/08/16 03:57:28 rswindell Exp $

// Library for dealing with the "logon list" (data/logon.jsonl)

"use strict";

function filename(days_ago)
{
	if(days_ago > 0)
		return system.data_dir + format("logon.%u.jsonl", days_ago - 1);
	return system.data_dir + "logon.jsonl";
}

function add(obj)
{
	if(!obj)
		obj = {};
	if(obj.time === undefined)
		obj.time = time();
	if(obj.user === undefined) {
		obj.user = JSON.parse(JSON.stringify(user));
		obj.user.limits = undefined;
		obj.user.security = undefined;
	}
	if(js.global.bbs !== undefined) {
		if(obj.node === undefined)
			obj.node = bbs.node_num;
		if(obj.total === undefined)
			obj.total = system.stats.total_logons;
	}
	if(obj.terminal === undefined
		&& js.global.console !== undefined) {
		obj.terminal =  { 
			desc: console.terminal, 
			type: console.type,
			charset: console.charset,
			support: console.term_supports(),
			columns: console.screen_columns,
			rows: console.screen_rows,
		};
	}
	
	if(!this.json_lines)
		this.json_lines = load({}, "json_lines.js");

	return json_lines.add(filename(), obj);
}

function get(num, days_ago)
{
	if(!this.json_lines)
		this.json_lines = load({}, "json_lines.js");
	
	return json_lines.get(filename(days_ago), num);
}

function maint(backup_level)
{
	return file_backup(filename(), backup_level, /* rename: */true);
}

this;