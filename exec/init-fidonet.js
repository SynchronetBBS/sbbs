// $Id$

// Initial FidoNet setup script - interactive, run via JSexec

/* Prompt for FidoNet node address (support points and othernets)
 * Prompt for hub address
 * Prompt for areafix password
 * Prompt for BinkP session password
 * Prompt for Origin Line
 * Create FidoNet msg group
 * Prompt for backbone.na file location (to download from)
 * Download backbone.na (using http-get) to ctrl dir
 * Import backbone.na in SCFG
 * Ask for default route to be created (e.g. 1:ALL)
 * Update sbbsecho.ini (add uplink, enabled BinkpPoll)
 * Run binkit.js install
 * Send an AreaFix %+ALL netmail to the hub
 * Generate a report with all details, mention log/lst/ini files
 */

"use strict";

var netname = "FidoNet";
var fidoaddr = load({}, 'fidoaddr.js');
print("Initializing " + netname + " support in Synchronet");
print("Use Ctrl-C to abort the process, if desired");

print("Reading Message Area configuration file: msgs.cnf");
var cnflib = load({}, "cnflib.js");
var msgs_cnf = cnflib.read("msgs.cnf");
if (!msgs_cnf) {
	alert("Failed to read msgs.cnf");
	exit(1);
}

if(system.fido_addr_list.length
	&& deny("You already have a " + netname + " Address (" + system.fido_addr_list[0] + "), continue"))
	exit(0);

var hub = {zone: NaN, net: NaN, node: NaN};
do {
	while(isNaN(hub.zone) || hub.zone < 1)
		hub.zone = parseInt(prompt("Your hub/uplink's zone number (e.g. 1 for FidoNet North America)"));
	while(isNaN(hub.net) || hub.net < 1)
		hub.net = parseInt(prompt("Your hub/uplink's network number"));
	while(isNaN(hub.node) ||  hub.node < 0)
		hub.node = parseInt(prompt("Your hub/uplink's node number"));
} while(!confirm("Your hub's address is: " + fidoaddr.to_str(hub)));

if(hub.zone > 6) {
	do {
		var str = prompt("Network name (no spaces or illegal filename chars) [" + netname + "]");
		if(str)
			netname = str;
	} while(!confirm("Network name is " + netname));
}

var your = {zone: hub.zone, net: hub.net, node: 9999, point: 0};
if(system.fido_addr_list.length)
	your = fidoaddr.parse(system.fido_addr_list[0]);
while(!confirm("Your node address is: " + fidoaddr.to_str(your))) {
	your.zone = NaN;
	your.net = NaN;
	your.node = NaN;
	while(isNaN(your.zone) || your.zone < 1)
		your.zone = parseInt(prompt("Your zone number (e.g. 1 for FidoNet North America)"));
	while(isNaN(your.net) || your.net < 1)
		your.net = parseInt(prompt("Your network number (i.e. normally the same as your hub)"));
	while(isNaN(your.node) || your.node < 1)
		your.node = parseInt(prompt("Your node number (e.g. 9999 for temporary node)"));
	while(isNaN(your.point))
		your.point = parseInt(prompt("Your point number (i.e. 0 for a normal node)"));
}
msgs_cnf.fido_addr_list.push(your);

var areafixpwd;
while(!areafixpwd)
	areafixpwd = prompt("Your AreaFix (a.k.a. Area Manager) Password (case in-sensitive)");
var sessionpwd;
while(!sessionpwd)
	sessionpwd = prompt("Your BinkP Session Password (case sensitive)");
if(!msgs_cnf.fido_default_origin)
	msgs_cnf.fido_default_origin = system.name + " - " + system.inet_addr;
if(!confirm("Your origin line (" +	msgs_cnf.fido_default_origin + ")")) {
	msgs_cnf.fido_default_origin = prompt("Your origin line");
}

/*******************/
/* UPDATE MSGS.CNF */
/*******************/
if(!msg_area.grp[netname.toLowerCase()]
	&& confirm("Create " + netname + " message group")) {
	print("Adding Message Group: " + netname);
	msgs_cnf.grp.push( {
			"name": netname,
			"description": netname,
			"ars": "",
			"code_prefix": netname.toUpperCase() + "_"
			});
}
if(confirm("Update Message Area configuration file: msgs.cnf")) {
	if(!cnflib.write("msgs.cnf", undefined, msgs_cnf)) {
		alert("Failed to write msgs.cnf");
		exit(1);
	}
	print("msgs.cnf updated successfully.");
}

/***********************/
/* UPDATE SBBSECHO.INI */
/***********************/
if(confirm("Update FidoNet configuration file: sbbsecho.ini")) {
	var file = new File("sbbsecho.ini");
	if(!file.open("r+")) {
		alert("Error " + file.error + " opening " + file.name);
		exit(1);
	}
	var section = "node:" + fidoaddr.to_str(hub);
	if(!file.iniGetObject(section)
		|| confirm("Overwrite hub [" + section + "] configuration in " + file.name)) {
		if(!file.iniSetObject(section,
			{
				AreaFixPwd: areafixpwd,
				SessionPwd: sessionpwd,
				GroupHub: netname,
				BinkpPoll: true
			})) {
			alert("Error " + file.error + " writing to " + file.name);
			exit(1);
		}
	}
	var section = "node:" + hub.zone + ":ALL";
	if(confirm("Route all zone " + hub.zone + " netmail through your hub")) {
		if(!file.iniSetObject(section,
			{
				Comment: "Everyone in zone " + hub.zone,
				Route: fidoaddr.to_str(hub)
			})) {
			alert("Error " + file.error + " writing to " + file.name);
			exit(1);
		}
	}
	file.close();
	print(file.name + " updated successfully.");
}

/*********************/
/* DOWNLOAD ECHOLIST */
/*********************/
if(confirm("Download and install " + netname + " EchoList")) {
	var echolist_fname = "echolist.txt";
	var echolist_url = "http://www.filegate.net/backbone/BACKBONE.NA";
	load("http.js");
	while(!js.terminated) {
		while(!echolist_url	|| !confirm("Download EchoList from " + echolist_url)) {
			echolist_url = prompt("Echolist URL");
		}
		var file = new File(echolist_fname);
		if(!file.open("w")) {
			alert("Error " + file.error + " opening " + file.name);
			exit(1);
		}
		var http_request = new HTTPRequest();
		var contents = http_request.Get(echolist_url);
		if(http_request.response_code == 200) {
			print("Downloaded " + echolist_url + " to " + file.name);
			file.write(contents);
			file.close();
			break;
		}
		alert("Error " + http_request.respons_code + " downloading " + echolist_url);
	}

	if(confirm("Import " + echolist_fname)) {
		print("Importing " + echolist_fname);
		system.exec(system.exec_dir + "scfg -import=" + echolist_fname + " -g" + netname);
	}
}

/******************/
/* INSTALL BINKIT */
/******************/
if(confirm("Install BinkIT")) {
	system.exec(system.exec_dir + "jsexec binkit install");
}

/************************/
/* SEND AREAFIX NETMAIL */
/************************/
if(confirm("Create an AreaFix request to link ALL EchoMail areas with "
	+ fidoaddr.to_str(hub))) {
	var msgbase = new MsgBase("mail");
	if(msgbase.open() == false) {
		alert("Error opening mail base: " + msgbase.last_error);
		exit(1);
	}
	if(!msgbase.save_msg({
			to: "areafix",
			to_net_addr: fidoaddr.to_str(hub),
			from: system.operator,
			from_ext: 1,
			subject: areafixpwd
		}, /* body text: */ "%+ALL")) {
		alert("Error saving message: " + msgbase.last_error);
		exit(1);
	}
	msgbase.close();
	file_touch(system.data_dir + "fidoout.now");
	print("AreaFix NetMail message created successfully.");
}

/***********************/
/* DISPLAY FINAL NOTES */
/***********************/
print(netname + " initial setup completely successfully.");
print();
if(your.node == 9999) {
	print("You used a temporary (e.g. /9999) node address. You will need to update your");
	print("SCFG->Networks->FidoNet->Address once your permanent node address has been");
	print("assigned to you.");
	print();
}
print("See your 'data/sbbsecho.log' file for mail import/export activity.");
print("See your 'data/badareas.lst' file for unrecognized received EchoMail areas.");
print("See your 'data/echostats.ini' file for detailed EchoMail statistics.");
print("See your 'Events' log output for outbound BinkP connections.");
print("See your 'Services' log output for inbound BinkP connections.");
print("Use exec/echocfg for follow-up FidoNet-related configuration.");
