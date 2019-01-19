// $Id$

// Sanity-check a Synchronet BBS installation

"use strict";
require("sbbsdefs.js", 'USER_DELETED');

function check_codes(desc, grp_list, sub_list)
{
	var list = [];
	var output = [];
	
	for(var g in grp_list) {
		for(var s in grp_list[g][sub_list]) {
			var sub = grp_list[g][sub_list][s];
			var code = sub.code.toUpperCase();
			if(list.indexOf(code) >= 0)
				output.push("Duplicate " + desc +" internal code: " + code);
			else {
				if(file_getname(code) != code)
					output.push("Invalid " + desc + " internal code: " + code);
				list.push(code);
			}
		}
	}
	return output;
}

var tests = {

	check_sysop: function(options)
	{
		var output = [];
		var usr = new User(1);
		if(!usr.is_sysop)
			output.push("User #1    is not a sysop");
		if(usr.security.restrictions & UFLAG_G)
			output.push("User #1    should not have the (G)uest restriction");
		if(system.operator.toLowerCase() != usr.alias.toLowerCase())
			output.push(format("User #%-4u alias (%s) does not match system operator (%s)"
							,1, usr.alias, system.operator));
		return output;
	},
	
	check_guest: function(options)
	{
		var guests = 0;
		var usr = new User;
		var lastuser = system.lastuser;
		for(var u = 1; u <= lastuser; u++) {
			usr.number = u;
			if(usr == null)
				continue;
			if(usr.settings & (USER_DELETED|USER_INACTIVE))
				continue;
			if(!(usr.security.restrictions & UFLAG_G))
				continue;
			guests++;
			if(guests > 1 && options.verbose)
				writeln(format("User %-4u (%s) is a (G)uest-restricted account"
					,usr.number, usr.alias));
		}
		if(guests != 1)
			return format("%u guest accounts found, recommended number: 1", guests);
	},
	
	check_user_names: function(options)
	{
		var output = [];
		var usr = new User;
		var lastuser = system.lastuser;
		for(var u = 1; u <= lastuser; u++) {
			usr.number = u;
			if(usr == null)
				continue;
			if(usr.settings & (USER_DELETED|USER_INACTIVE))
				continue;
			if(!system.trashcan("name", usr.alias))
				continue;
			output.push(format("User #%-4u has a disallowed alias%s"
				, usr.number
				, options.verbose ? (': ' + usr.alias) : ''));
		}
		return output;
	},
	
	check_user_passwords: function(options)
	{
		var output = [];
		var usr = new User;
		var lastuser = system.lastuser;
		for(var u = 1; u <= lastuser; u++) {
			usr.number = u;
			if(usr == null)
				continue;
			if(usr.settings & (USER_DELETED|USER_INACTIVE))
				continue;
			if(!system.trashcan("password", usr.security.password))
				continue;
			output.push(format("User #%-4u has a disallowed password%s"
				, usr.number
				, options.verbose ? (': ' + usr.security.password) : ''));
		}
		return output;
	},

	check_qnet_tags: function(options)
	{
		var output = [];
		var long = 0;
		const maxlen = 79 - " * Synchronet * ".length;
		for(var s in msg_area.sub) {
			var sub = msg_area.sub[s];
			if(!(sub.settings & SUB_QNET))
				continue;
			var len = strip_ctrl(sub.fidonet_origin).length;
			if(js.global.console)
				len = console.strlen(sub.fidonet_origin);
			if(len > maxlen)
				long++;
		}
		if(long)
			output.push(long 
				+ " msg sub-boards have QWKnet taglines exceeding " 
				+ maxlen + " printed characters");
		return output;
	},

	check_fido_origlines: function(options)
	{
		var output = [];
		var long = 0;
		const maxlen = 79 - " * Origin: ".length;
		for(var s in msg_area.sub) {
			var sub = msg_area.sub[s];
			if(!(sub.settings & SUB_FIDO))
				continue;
			var len = sub.fidonet_origin.length;
			if(len > maxlen)
				long++;
		}
		if(long)
			output.push(long 
				+ " msg sub-boards have FidoNet origin lines exceeding "
				+ maxlen + " printed characters");
		return output;
	},
	
	check_bbs_list: function(options)
	{
		var lib = load({}, "sbbslist_lib.js");
		var list = lib.read_list();
		if(!lib.system_exists(list, system.name))
			return system.name + " is not listed in " + lib.list_fname;
	},
	
	check_sub_codes: function(options)
	{
		return check_codes("msg sub-board", msg_area.grp_list, 'sub_list');
	},
	
	check_dir_codes: function(options)
	{
		return check_codes("file directory", file_area.lib_list, 'dir_list');
	},

	check_xtrn_codes: function(options)
	{
		return check_codes("external program (door)", xtrn_area.sec_list, 'prog_list');
	},

};

var options = { verbose: argv.indexOf('-v') >= 0 };

var issues = 0;
for(var i in tests) {
	print(i);
	var result;
	switch(typeof (result = tests[i](options))) {
		case 'string':
			alert(result), issues++;
			break;
		case 'object':
			for(var i in result)
				alert(result[i]), issues++;
			break;
	}
}
writeln(issues + " issues discovered");