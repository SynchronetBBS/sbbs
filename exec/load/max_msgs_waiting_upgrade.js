// Carries the mail server's Max Messages Waiting setting of Synchronet
// v3.21 and older (MaxMsgsWaiting in the [Mail] section of sbbs.ini) over
// to the message options of v3.22 and later (max_msgs_waiting in the
// [mail] section of ctrl/msgs.ini). Called by update.js:
//
// require("max_msgs_waiting_upgrade.js", "upgrade_max_msgs_waiting");
// var result = upgrade_max_msgs_waiting(system.ctrl_dir [, dry_run]);
//
// The sbbs.ini read is the one this host's servers use (file_cfgname():
// sbbs.<hostname>.ini when there is one). It is not changed: a server
// still running an older binary reads the old key. Until msgs.ini has
// the new key, the configuration loader reads the old one itself
// (sbbs_ini_max_msgs_waiting() in src/sbbs3/scfglib1.c), so the limit is
// the same before and after this runs.
//
// Nothing is done when msgs.ini already has the key (so running this
// twice changes nothing, and a value saved by SCFG is never replaced),
// when sbbs.ini or its key does not exist, or when the old value is the
// default (100), which needs no key. A missing msgs.ini is an error and
// nothing is created.
//
// Returns an object: { status, value }, where status is "none",
// "converted" (value is the number carried over, or that would be when
// dry_run is true) or "error" (with a message property).

"use strict";

// As MAX_MSGS_WAITING_DEFAULT in src/sbbs3/sbbsdefs.h
var MAX_MSGS_WAITING_DEFAULT = 100;

function upgrade_max_msgs_waiting(ctrl_dir, dry_run)
{
	var ini_path = ctrl_dir + "msgs.ini";
	var old_path = file_cfgname(ctrl_dir, "sbbs.ini");
	var result = { status: "none" };

	if (!file_exists(old_path))
		return result;
	var old = new File(old_path);
	if (!old.open("r")) {
		result.status = "error";
		result.message = "Error " + old.error + " opening " + old_path;
		return result;
	}
	// A number default: the value is read as the servers read it
	var value = old.iniGetValue("Mail", "MaxMsgsWaiting", MAX_MSGS_WAITING_DEFAULT);
	old.close();
	if (value == MAX_MSGS_WAITING_DEFAULT)
		return result;

	if (!file_exists(ini_path)) {
		result.status = "error";
		result.message = ini_path + " does not exist";
		return result;
	}
	var ini = new File(ini_path);
	if (!ini.open(dry_run ? "r" : "r+")) {
		result.status = "error";
		result.message = "Error " + ini.error + " opening " + ini_path;
		return result;
	}
	if (ini.iniGetValue("mail", "max_msgs_waiting") !== undefined) {
		ini.close();
		return result;
	}
	result.status = "converted";
	result.value = value;
	if (!dry_run && !ini.iniSetValue("mail", "max_msgs_waiting", value)) {
		result.status = "error";
		result.message = "Error " + ini.error + " writing " + ini_path;
	}
	ini.close();
	return result;
}

this;
