// Sanity-check a Synchronet BBS installation
// @format.tab-size 4

"use strict";
const REVISION = '1.20';
require("http.js", 'HTTPRequest');
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
				output.push("Duplicate " + desc +" internal code in " + grp_list[g].name + ": " + code);
			else {
				if(file_getname(code) != code)
					output.push("Invalid " + desc + " internal code in " + grp_list[g].name + ": " + code);
				list.push(code);
			}
		}
	}
	return output;
}

var tests = {

	check_version: function(options)
	{
		var url = "https://gitlab.synchro.net/main/sbbs/-/raw/master/install/sbbs.json";
		var http_request = new HTTPRequest();
		try {
			var contents = http_request.Get(url);
		} catch(e) {
			return format("http_request.Get(%s) error: ", url) + e.message;
		}
		try {
			var obj = JSON.parse(contents);
		} catch(e) {
			return format("%s: '%s'", e.message, lfexpand(contents));
		}
		if(obj.valid === true) {
			if(obj.release.version_num > system.version_num)
				return format("Synchronet %s (latest release by %s) is newer than %s"
					,obj.release.version, obj.release.by, system.full_version);
			if(obj.release.version_num < system.version_num)
				return format("Synchronet %s (latest release by %s) is older than %s"
					,obj.release.version, obj.release.by, system.full_version);
		}
		return true;
	},

	check_sysop: function(options)
	{
		var output = [];
		var usr = new User(1);
		if(!usr.is_sysop)
			output.push("User #1    is not a sysop");
		if(usr.security.restrictions & UFLAG_G)
			output.push("User #1    should not have the (G)uest restriction");
		if(usr.security.restrictions & UFLAG_L)
			output.push("User #1    should not have the (L)ogon Once/Day restriction");
		if(!(usr.security.exemptions & UFLAG_T))
			output.push("User #1    should have the (T)ime exemption");
		if(!(usr.security.exemptions & UFLAG_L))
			output.push("User #1    should have the (L)ogon exemption");
		if(system.operator.toLowerCase() != usr.alias.toLowerCase()
			&& system.operator.toLowerCase() != usr.name.toLowerCase())
			output.push(format("User #%-4u name (%s) and alias (%s) does not match system operator (%s)"
							,1, usr.name, usr.alias, system.operator));
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
			if(system.version_num > 32100 && !system.check_name(usr.alias, /* unique: */false))
				output.push(format("User #%-4u has a disallowed alias%s"
					, usr.number
					, options.verbose ? (': ' + usr.alias) : ''));
			if(usr.security.restrictions & UFLAG_G)
				continue;
			if(typeof system.check_realname === "function" && !system.check_realname(usr.name))
				output.push(format("User #%-4u has a disallowed name%s"
					, usr.number
					, options.verbose ? (': ' + usr.name) : ''));
		}
		return output;
	},

	check_user_passwords: function(options)
	{
		var output = [];
		var usr = new User;
		var lastuser = system.lastuser;
		var password_list = {};
		for(var u = 1; u <= lastuser; u++) {
			usr.number = u;
			if(usr == null)
				continue;
			if(usr.settings & (USER_DELETED|USER_INACTIVE))
				continue;
			var password = usr.security.password;
			if(password == '') {
				if(!(usr.security.restrictions & UFLAG_G))
					output.push(format("User #%-4u has no password", usr.number));
				continue;
			}
			if(!password_list[password])
				password_list[password] = [];
			password_list[password].push(u);
			if(usr.alias.toLowerCase() == password.toLowerCase()
				|| usr.name.toLowerCase() == password.toLowerCase()
				|| usr.handle.toLowerCase() == password.toLowerCase())
				output.push(format("User #%-4u has an obvious password (their name or alias)"
					, usr.number));
			if(password.length < system.min_password_length)
				output.push(format("User #%-4u has a password length (%u) < the minimum: %u"
					, usr.number
					, password.length
					, system.min_password_length));
			else if(system.max_password_length 
				&& password.length > system.max_password_length)
				output.push(format("User #%-4u has a password length (%u) > the maximum: %u"
					, usr.number
					, password.length
					, system.max_password_length));
			else if(typeof system.check_password === "function" && !system.check_password(password))
				output.push(format("User #%-4u has a low quality password%s"
					, usr.number
					, options.verbose ? (': ' + password) : ''));
			if(!system.trashcan("password", password))
				continue;
			output.push(format("User #%-4u has a disallowed password%s"
				, usr.number
				, options.verbose ? (': ' + password) : ''));
		}
		for(var p in password_list) {
			if(password_list[p].length > 2)
				output.push("Password "
					+ ( options.verbose ? (p  + ' '): '')
					+ "shared between " + password_list[p].length + " users: " + password_list[p]);
		}
		return output;
	},

	check_qnet_tags: function(options)
	{
		var output = [];
		var long = 0;
		var short = 0;
		var dflt = 0;
		const maxlen = 79 - " * Synchronet * ".length;
		for(var s in msg_area.sub) {
			var sub = msg_area.sub[s];
			if(!(sub.settings & SUB_QNET))
				continue;
			if(sub.qwknet_tagline == "My Brand-New BBS")
				dflt++;
			var len = strip_ctrl(sub.qwknet_tagline).length;
			if(js.global.console)
				len = console.strlen(sub.qwknet_tagline);
			if(len > maxlen) {
				if(options.verbose)
					alert(format("QWK-networked sub (%s) has a long tagline", s));
				long++;
			}
			if(len < 1) {
				if(options.verbose)
					alert(format("QWK-networked sub (%s) has a short tagline", s));
				short++;
			}
		}
		if(long)
			output.push(long 
				+ " msg sub-boards have QWKnet taglines exceeding " 
				+ maxlen + " printed characters");
		if(short)
			output.push(short
				+ " msg sub-boards have QWKnet taglines with no content");
		if(dflt)
			output.push(dflt
				+ " msg sub-boards have the default QWKnet tagline");
		return output;
	},

	check_fido_origlines: function(options)
	{
		var output = [];
		var long = 0;
		var short = 0;
		const maxlen = 79 - " * Origin: ".length;
		for(var s in msg_area.sub) {
			var sub = msg_area.sub[s];
			if(!(sub.settings & SUB_FIDO))
				continue;
			var len = sub.fidonet_origin.length;
			if(len > maxlen) {
				if(options.verbose)
					alert(format("Fido-networked sub (%s) has a long origin line", s));
				long++;
			}
			if(len < 1) {
				if(options.verbose)
					alert(format("Fido-networked sub (%s) has a short origin line", s));
				short++;
			}
		}
		if(long)
			output.push(long 
				+ " msg sub-boards have FidoNet origin lines exceeding "
				+ maxlen + " printed characters");
		if(short)
			output.push(short
				+ " msg sub-boards have FidoNet origin lines with no content");
		return output;
	},
	
	check_bbs_list: function(options)
	{
		var lib = load({}, "sbbslist_lib.js");
		var list = lib.read_list();
		if(!lib.system_exists(list, system.name))
			return system.name + " is not listed in " + lib.list_fname;
		var finger_host = "vert.synchro.net";
		var finger_query = "?bbs:" + system.name;
		log(LOG_DEBUG, format("Sending finger query (%s) to %s", finger_query, finger_host));
		var finger_result = load({}, "finger_lib.js").request(finger_host, finger_query);
		if(typeof finger_result == 'string')
			return finger_result;
		var finger_obj;
		try {
			finger_obj = JSON.parse(finger_result.join(''));
		} catch(e) {
			return finger_result.join('');
		}
		var bbs = list[lib.system_index(list, system.name)];
		bbs.entry = undefined;
		bbs.total = undefined;
		bbs.preview = undefined;
		bbs.imported = undefined;
		finger_obj.entry = undefined;
		finger_obj.total = undefined;
		finger_obj.preview = undefined;
		finger_obj.imported = undefined;
		if(JSON.stringify(finger_obj) != JSON.stringify(bbs)) {
			if(options.verbose) {
				print(finger_host + ":");
				print(JSON.stringify(finger_obj, null, 4));
				print("local:");
				print(JSON.stringify(bbs, null, 4));
			}
			return format("'%s' BBS entry on %s is different than local", system.name, finger_host);
		}
	},
	
	check_syncdata: function(options)
	{
		if(!load({}, "syncdata.js").find())
			return "SYNCDATA sub-board could not be found in message area configuration";
	},
	
	check_imsg_list: function(options)
	{
		var lib = load({}, "sbbsimsg_lib.js");
		var list = lib.read_sys_list(/* include_self: */true);
		if(!lib.find_name(list, system.name))
			return system.name + " is not listed in " + lib.filename;
	},
	
	check_dove_net: function(options)
	{
		const TOTAL_DOVENET_CONFERENCES = js.global.jsexec_revision ? 23 : 22;
		var output = [];
		var grp = msg_area.grp["DOVE-Net"];
		if(!grp)
			return;
		if(grp.sub_list.length != TOTAL_DOVENET_CONFERENCES)
			output.push(format("DOVE-Net: %u sub-boards configured (instead of the expected: %u)"
				,grp.sub_list.length, TOTAL_DOVENET_CONFERENCES));
		for(var s in grp.sub_list) {
			var sub = grp.sub_list[s];
			if(sub.settings & SUB_GATE)
				output.push(format("DOVE-Net: %-16s is configured to Gate Between Net Types", sub.code));
			if(sub.settings & SUB_NOVOTING)
				output.push(format("DOVE-Net: %-16s is NOT configured to allow voting", sub.code));
			if(!(sub.settings & SUB_QNET))
				output.push(format("DOVE-Net: %-16s is NOT configured for QWK Networking", sub.code));
		}
		return output;
	},
	
	check_sub_cfgs: function(options)
	{
		var output = [];
		for(var sub in msg_area.sub) {
			var msgbase = new MsgBase(sub);
			if(!msgbase.open()) {
				if(options.verbose)
					alert(format("Error (%s) opening sub: %s", msgbase.error, sub));
				continue;
			}
			if(msgbase.max_crcs != msgbase.cfg.max_crcs)
				output.push(format("MsgBase: %-16s max_crcs status (%d) does not match sub-board configuration: %d",
					sub, msgbase.max_crcs, msgbase.cfg.max_crcs));
			if(msgbase.max_msgs != msgbase.cfg.max_msgs)
				output.push(format("MsgBase: %-16s max_msgs status (%d) does not match sub-board configuration: %d",
					sub, msgbase.max_msgs, msgbase.cfg.max_msgs));
			if(msgbase.max_age != msgbase.cfg.max_age)
				output.push(format("MsgBase: %-16s max_age status (%d) does not match sub-board configuration: %d",
					sub, msgbase.max_age, msgbase.cfg.max_age));
			msgbase.close();
		}
		return output;
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
	
	check_sockopts_ini: function(options)
	{
		var output = [];
		var file = new File(file_cfgname(system.ctrl_dir, "sockopts.ini"));
		if(file.open("r")) {
			if(file.iniGetValue(null, "SNDBUF"))
				output.push(file.name + " has SNDBUF set");
			if(file.iniGetValue(null, "RCVBUF"))
				output.push(file.name + " has RCVBUF set");
			if(file.iniGetValue("tcp", "SNDBUF"))
				output.push(file.name + " has [tcp] SNDBUF set");
			if(file.iniGetValue("tcp", "RCVBUF"))
				output.push(file.name + " has [tcp] RCVBUF set");
			file.close();
		}
		return output;
	}
};

print('Synchronet Check Setup v' + REVISION);

var options = { verbose: argv.indexOf('-v') >= 0 || argv.indexOf('-V') >= 0};

var issues = 0;
for(var i in tests) {
	print('Invoking: ' + i);
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
if(issues)
	alert(issues + " issues discovered");
else
	writeln("No issues discovered");
