// Library for dealing with the "logon list" (data/logon.jsonl)

"use strict";

function filename(days_ago)
{
	if(days_ago > 0)
		return system.data_dir + format("logon.%u.jsonl", days_ago - 1);
	return system.data_dir + "logon.jsonl";
}

const exclude_list = [
	'limits',
	'security',
	'is_guest',
	'is_sysop',
	'is_active',
	'batch_upload_list',
	'batch_download_list',
	'birthyear',
	'birthmonth',
	'birthday',
];

function add(obj)
{
	if(!obj)
		obj = {};
	if(obj.time === undefined)
		obj.time = time();
	if(obj.protocol === undefined)
		obj.protocol = client.protocol;
	if(obj.user === undefined) {
		obj.user = JSON.parse(JSON.stringify(user));
		for (var i in exclude_list)
			obj.user[exclude_list[i]] = undefined;
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
	
	var result = json_lines.get(filename(days_ago), num);
	if(days_ago !== undefined || typeof result !== 'object')
		return result;
	if(num < 0)
		num += result.length;
	else
		num -= result.length;
	for(days_ago = 1; num != 0; days_ago++) {
		var more = json_lines.get(filename(days_ago), num);
		if(typeof more !== 'object')
			break;
		result.unshift.apply(result, more);
		if(num < 0)
			num += more.length;
		else
			num -= more.length;
	}
	return result;
}

function maint(backup_level)
{
	file_touch(filename());
	return file_backup(filename(), backup_level, /* rename: */true);
}

this;
