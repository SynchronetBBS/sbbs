// $Id: install-binkit.js,v 1.2 2020/03/26 06:18:33 rswindell Exp $	

// Installs BinkIT (BinkP FidoNet Mailer) timed events and service
// Enables SBBSecho timed events (FIDOIN and FIDOOUT)
// Migrated from exec/binkit.js

"use strict";

// returns true on success, an error string on failure
function install_binkit()
{	
	require("sbbsdefs.js", 'EVENT_DISABLED');
	var cnflib = load({}, "cnflib.js");
	var xtrn_cnf = cnflib.read("xtrn.cnf");
	if (!xtrn_cnf)
		return "Failed to read xtrn.cnf";

	var changed = false;
	if (!xtrn_area.event["binkout"]) {
		print("Adding timed event: BINKOUT");
		xtrn_cnf.event.push( {
				"code": "BINKOUT",
				"cmd": "?binkit",
				"days": 255,
				"time": 0,
				"node_num": 1,
				"settings": 0,
				"startup_dir": "",
				"freq": 0,
				"mdays": 0,
				"months": 0
				});
		changed = true;
	}

	if (!xtrn_area.event["binkpoll"]) {
		print("Adding timed event: BINKPOLL");
		xtrn_cnf.event.push( {
				"code": "BINKPOLL",
				"cmd": "?binkit -p",
				"days": 255,
				"time": 0,
				"node_num": 1,
				"settings": 0,
				"startup_dir": "",
				"freq": 60,
				"mdays": 0,
				"months": 0
				});
		changed = true;
	}

	const timed_events = ["FIDOIN", "FIDOOUT", "BINKOUT", "BINKPOLL"];
	for(var i in xtrn_cnf.event) {
		if(timed_events.indexOf(xtrn_cnf.event[i].code) < 0)
			continue;
		if (xtrn_cnf.event[i].settings & EVENT_DISABLED) {
			print("Enabling timed event: " + xtrn_cnf.event[i].code);
			xtrn_cnf.event[i].settings &= ~EVENT_DISABLED;
			changed = true;
		}
	}

	if (changed && !cnflib.write("xtrn.cnf", undefined, xtrn_cnf))
		return "Failed to write xtrn.cnf";

	var ini = new File(file_cfgname(system.ctrl_dir, "sbbsecho.ini"));
	if (!ini.open(file_exists(ini.name) ? 'r+':'w+'))
		return ini.name + " open error " + ini.error;
	print("Updating " + ini.name);
	ini.iniSetValue(null, "BinkleyStyleOutbound", true);
	ini.iniSetValue(null, "OutgoingSemaphore", "../data/binkout.now");
	var links = ini.iniGetAllObjects("addr", "node:");
	for (var i in links) {
		if (links[i].addr.toUpperCase().indexOf('ALL') >= 0)	// Don't include wildcard links
			continue;
		print("Updating " + ini.name + " [node:" + links[i].addr + "]");
		if(links[i].SessionPwd === undefined) {
			var password = links[i].PacketPwd ? links[i].PacketPwd : links[i].AreaFixPwd;
			ini.iniSetValue("node:"+links[i].addr, "SessionPwd", password === undefined ? '' : password);
		}
		ini.iniSetValue("node:"+links[i].addr, "BinkpPoll", links[i].GroupHub ? true : false);
	}
	ini.close();

	ini = new File(file_cfgname(system.ctrl_dir, "services.ini"));
	if (!ini.open(file_exists(ini.name) ? 'r+':'w+'))
		return ini.name + " open error " + ini.error;
	if(!ini.iniGetObject("BINKIT")) {
		var section = "BINKP";
		print("Updating " + ini.name + " [" + section + "]");
		ini.iniSetValue(section, "Enabled", true);
		ini.iniSetValue(section, "Command", "binkit.js");
		ini.iniSetValue(section, "Port", 24554);
	}
	ini.close();

	print("BinkIT installed successfully");
	return true;
}

install_binkit();
