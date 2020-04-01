// $Id$

// Initial FidoNet setup script - interactive, run via JSexec or ;exec

// usage: init-fidonet.js [zone | othernet-name] [http://path/to/echolist.na]

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

const REVISION = "$Revision$".split(' ')[1];
var netname;
var netdns;
var netzone = parseInt(argv[0], 10);
if(!netzone)
	netname = argv[0];
var echolist_url = argv[1];
// If you want your Othernet listed here, please provide information
// and an http[s] URL to your official EchoList
var network;
var fidoaddr = load({}, 'fidoaddr.js');
print("******************************************************************************");
print("*                            " + js.exec_file + " v" + format("%-30s", REVISION) + " *");
print("*                 " + format("%-58s", "Initializing " + (netname || "FidoNet") + " support in Synchronet") +" *");
print("*                 Use Ctrl-C to abort the process if desired                 *");
print("******************************************************************************");

var network_list = {};
var file = new File("init-fidonet.ini");
if (file.open("r")) {
	var list = file.iniGetSections("zone:", "zone");
	for(var i in list)
		network_list[list[i].substr(5)] = file.iniGetObject(list[i]);
	file.close();
}

function aborted()
{
	if(js.terminated || (js.global.console && console.aborted))
		exit(1);
	return false;
}

function lookup_network(info)
{
	print("Looking up network/zone: " + info);
	var result;
	if(typeof info == "number") {
		result = network_list[info];
		if(result)
			return result;
	}
	
	print("Reading FTN configuration file: sbbsecho.ini");
	var file = new File("sbbsecho.ini");
	if (!file.open("r")) {
		alert("Error " + file.error + " opening " + file.name);
		return false;
	}
	
	if(typeof info == "number") { // zone
		var dns;
		var domain_list = file.iniGetSections("domain:");
		if(domain_list) {
			var zonemap = {};
			for(var i = 0; i < domain_list.length && !result; i++) {
				var section = domain_list[i];
				var netname = section.substr(7)
				var zones = file.iniGetValue(section, "Zones");
				if(!zones)
					continue;
				var dns = file.iniGetValue(section, "DNSSuffix");
				if(typeof zones == 'number') {
					if(info == zones) {
						result = { name: netname, dns: dns};
						break;
					}
					continue;
				}
				zones = zones.split(',');
				for(var j = 0; j < zones.length; j++) {
					if(info == zones[j]) {
						result = { name: netname, dns: dns};
						break;
					}
				}
			}
		}
		file.close();
		return result;
	}
	result = file.iniGetValue("domain:" + info, "Zones", 1);
	file.close();
	return result;
}

if(netzone)
	network = lookup_network(netzone);
else if(netname) {
	netzone = lookup_network(netname);
	network = network_list[netzone];
}

if(!netzone) {
	for(var zone in network_list) {
		var desc = "";
		if(network_list[zone].desc)
			desc = " (" + network_list[zone].desc + ")";
		print(format("%6s %s%s", format("<%u>", zone), network_list[zone].name, desc));
		if(network_list[zone].info)
			print("       " + network_list[zone].info);
		if(network_list[zone].coord || network_list[zone].email) {
			var email = "";
			if(network_list[zone].email)
				email = " <" + network_list[zone].email + ">";
			print("       coordinator: " + (network_list[zone].coord || "") + email);
		}
	}
	var which;
	while((!which || which < 1) && !aborted())
		which = parseInt(prompt("Which"), 10);
	netzone = which;
	network = network_list[which];
}

if(network)
	netname = network.name;
else
	network = {};

if(netzone <= 6)
	netname = "FidoNet";
else {
	while((!netname || netname.indexOf(' ') >= 0 || netname.length > 8 
		|| !confirm("Network name is " + netname)) && !aborted()) {
		var str = prompt("Network name (no spaces or illegal filename chars) [" + netname + "]");
		if(str)
			netname = str;
	}
}
if(netname) {
	print("Network name: " + netname);
	print("Network zone: " + netzone);
	print("Network info: " + network.info);
	print("EchoList URL: " + network.echolist);
	print("Network coordinator: " + network.coord 
		+ (network.email ? (" <" + network.email + ">") : ""));
} else
	alert("Unrecognized network zone: " + netzone);

print("Reading Message Area configuration file: msgs.cnf");
var cnflib = load({}, "cnflib.js");
var msgs_cnf = cnflib.read("msgs.cnf");
if (!msgs_cnf) {
	alert("Failed to read msgs.cnf");
	exit(1);
}

var your = {zone: NaN, net: NaN, node: 9999, point: 0};
for(var i = 0; i < system.fido_addr_list.length; i++) {
	var addr = fidoaddr.parse(system.fido_addr_list[i]);
	if(!addr || addr.zone != netzone)
		continue;
	if(deny("You already have a " + netname + " Address (" + system.fido_addr_list[i] + "), continue"))
		exit(0);
	your = addr;
	break;
}

var hub = {zone: netzone ? netzone : NaN, net: NaN, node: NaN};
if(network.addr)
	hub = fidoaddr.parse(network.addr);
do {
	while((isNaN(hub.zone) || hub.zone < 1) && !aborted())
		hub.zone = parseInt(prompt("Your hub's zone number (e.g. 1 for FidoNet North America)"));
	while((isNaN(hub.net) || hub.net < 1) && !aborted())
		hub.net = parseInt(prompt("Your hub's network number"));
	while((isNaN(hub.node) ||  hub.node < 0) && !aborted())
		hub.node = parseInt(prompt("Your hub's node number"));
} while(!confirm("Your hub's address is " + fidoaddr.to_str(hub)) && !aborted());

if(fidoaddr.to_str(hub) == network.addr)
	hub_name = network.coord;

var hub_name;
while((!hub_name || !confirm("Your hub's sysop's name is " + hub_name)) && !aborted()) {
	hub_name = prompt("Your hub's sysop's name");
}

var hub_host = network.host;
while(!network.dns && (!hub_host
	|| !confirm("Your hub's hostname or IP address is " + hub_host)) && !aborted()) {
	hub_host = prompt("Your hub's hostname or IP address");
}

var hub_port = network.port || 24554;
while(!hub_port || !confirm("Your hub's BinkP/TCP port number is " + hub_port) && !aborted()) {
	hub_port = paseInt(prompt("Your hub's BinkP/TCP port number"));
}

if(!your.zone)
	your.zone = hub.zone;
if(!your.net)
	your.net = hub.net;
while(!confirm("Your node address is " + fidoaddr.to_str(your)) && !aborted()) {
	your.zone = NaN;
	your.net = NaN;
	your.node = NaN;
	while((isNaN(your.zone) || your.zone < 1) && !aborted())
		your.zone = parseInt(prompt("Your zone number (e.g. 1 for FidoNet North America)"));
	while((isNaN(your.net) || your.net < 1) && !aborted())
		your.net = parseInt(prompt("Your network number (i.e. normally the same as your hub)"));
	while((isNaN(your.node) || your.node < 1) && !aborted())
		your.node = parseInt(prompt("Your node number (e.g. 9999 for temporary node)"));
	while((isNaN(your.point)) && !aborted())
		your.point = parseInt(prompt("Your point number (i.e. 0 for a normal node)"));
}
msgs_cnf.fido_addr_list.push(your);

if(!msgs_cnf.fido_default_origin)
	msgs_cnf.fido_default_origin = system.name + " - " + system.inet_addr;
while(!msgs_cnf.fido_default_origin
	|| !confirm("Your origin line is (" +	msgs_cnf.fido_default_origin + ")")) {
	msgs_cnf.fido_default_origin = prompt("Your origin line");
}

var areafixpwd;
while(!areafixpwd && !aborted())
	areafixpwd = prompt("Your AreaFix (a.k.a. Area Manager) Password (case in-sensitive)");
var sessionpwd;
while(!sessionpwd && !aborted())
	sessionpwd = prompt("Your BinkP Session Password (case sensitive)");
var sysop = system.operator;
if(system.stats.total_users) {
	var u = new User(1);
	if(u && u.name)
		sysop = u.name;
}
while((!sysop || !confirm("Your name is " + sysop)) && !aborted())
	sysop = prompt("Your name");

/*******************/
/* UPDATE MSGS.CNF */
/*******************/
if(!msg_area.grp[netname]
	&& confirm("Create " + netname + " message group in SCFG->Message Areas")) {
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

/*********************/
/* DOWNLOAD ECHOLIST */
/*********************/
var echolist_fname = file_getname(network.echolist);
if(network.echolist 
	&& (network.echolist.indexOf("http://") == 0 || network.echolist.indexOf("https://") == 0)
	&& confirm("Download " + netname + " EchoList: " + network.echolist)) {
	var echolist_url = network.echolist;
	load("http.js");
	while(!aborted()) {
		while((!echolist_url || !confirm("Download EchoList from: " + echolist_url)) && !aborted()) {
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
}
while(!file_getcase(echolist_fname)) {
	alert(system.ctrl_dir + echolist_fname + " does not exist");
	if(!confirm("Install " + netname + " EchoList: " + echolist_fname))
		break;
	prompt("Download and extract " + echolist_fname + " now. Enter to continue");
}
echolist_fname = file_getcase(echolist_fname)	
if(echolist_fname && confirm("Import " + netname + " EchoList: " + echolist_fname)) {
	print("Importing " + echolist_fname);
	system.exec(system.exec_dir + "scfg"
		+ " -import=" + echolist_fname 
		+ " -g" + netname
		+ " -faddr=" + fidoaddr.to_str(your));
}


function makepath(path)
{
	if(mkpath(path))
		return true;
	alert("Error " + errno + " (" + errno_str + ") creating " + path);
	return false;
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
	var path = file.iniGetValue(null, "Inbound");
	if(!path)
		path = "../fido/nonsecure";
	while((!path
		|| !confirm("Non-secure inbound directory is " + path)
		|| !makepath(path)) && !aborted())
		path = prompt("Non-secure inbound directory");
	file.iniSetValue(null, "Inbound", path);

	path = file.iniGetValue(null, "SecureInbound");
	if(!path)
		path = "../fido/inbound";
	while((!path
		|| !confirm("Secure inbound directory is " + path)
		|| !makepath(path)) && !aborted())
		path = prompt("Secure inbound directory");
	file.iniSetValue(null, "SecureInbound", path);

	path = file.iniGetValue(null, "Outbound");
	if(!path)
		path = "../fido/outbound";
	while((!path
		|| !confirm("Outbound directory is " + path)
		|| !makepath(path)) && !aborted())
		path = prompt("Outbound directory");
	file.iniSetValue(null, "Outbound", path);

	var binkp = file.iniGetObject("BinkP");
	if(!binkp) binkp = {};
	binkp.sysop = sysop;
	if(!file.iniSetObject("binkp", binkp)) {
		alert("Error" + file.error + " writign to " + file.name);
		exit(1);
	}
	var section = "node:" + fidoaddr.to_str(hub);
	if(!file.iniGetObject(section)
		|| confirm("Overwrite hub [" + section + "] configuration in " + file.name)) {
		if(!file.iniSetObject(section,
			{
				Name: hub_name,
				Comment: network.email,
				AreaFixPwd: areafixpwd,
				SessionPwd: sessionpwd,
				GroupHub: netname,
				BinkpPoll: true,
				BinkpHost: hub_host,
				BinkpPort: hub_port
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
	if(echolist_fname) {
		var section = "echolist:" + echolist_fname;
		if(!file.iniGetObject(section)
			|| confirm("Overwrite [" + section + "] configuration in " + file.name)) {
			if(!file.iniSetObject(section,
				{
					Hub: fidoaddr.to_str(hub),
					Pwd: areafixpwd
				})) {
				alert("Error " + file.error + " writing to " + file.name);
				exit(1);
			}
		}
	}
	file.close();
	print(file.name + " updated successfully.");
}

/******************/
/* INSTALL BINKIT */
/******************/
if(confirm("Install BinkIT")) {
	var result = load({}, "install-binkit.js");
	if (result != true) {
		alert(result);
		exit(1);
	}
}

print("Requesting Synchronet recycle (configuration-reload)");
if(!file_touch(system.ctrl_dir + "recycle"))
	alert("Recycle semaphore file update failure");

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
			from: sysop,
			from_ext: 1,
			subject: areafixpwd
		}, /* body text: */ "%+ALL")) {
		alert("Error saving message: " + msgbase.last_error);
		exit(1);
	}
	msgbase.close();
	print("AreaFix NetMail message created successfully.");
}

/***********************/
/* DISPLAY FINAL NOTES */
/***********************/
print(netname + " initial setup completely successfully.");
print();
if(your.node == 9999) {
	print("You used a temporary node address (" + fidoaddr.to_str(your) +
		").  You will need to update your");
	print("SCFG->Networks->FidoNet->Address once your permanent node address has been");
	print("assigned to you.");
	print();
}
print("See your 'data/sbbsecho.log' file for mail import/export activity.");
print("See your 'data/badareas.lst' file for unrecognized received EchoMail areas.");
print("See your 'data/echostats.ini' file for detailed EchoMail statistics.");
print("See your 'data/binkstats.ini' file for detailed BinkP statistics.");
print("See your 'Events' log output for outbound BinkP connections.");
print("See your 'Services' log output for inbound BinkP connections.");
print("Use exec/echocfg for follow-up FidoNet-related configuration.");
