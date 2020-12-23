// $Id: init-fidonet.js,v 1.29 2020/05/12 17:23:30 rswindell Exp $

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

const REVISION = "$Revision: 1.30 $".split(' ')[1];
require('sbbsdefs.js', 'SUB_NAME');
const temp_node = 9999;
var netname;
var netdns;
var netzone = parseInt(argv[0], 10);
if(!netzone)
	netname = argv[0];
var echolist_url = argv[1];
// If you want your Othernet listed in init-fidonet.ini, please provide information
// and an http[s] URL to your official EchoList
var network;
var fidoaddr = load({}, 'fidoaddr.js');
print("******************************************************************************");
print("*                            " + js.exec_file + " v" + format("%-30s", REVISION) + " *");
print("*                 " + format("%-58s", "Initializing " + (netname || "FidoNet") + " support in Synchronet") +" *");
print("*                 Use Ctrl-C to abort the process if desired                 *");
print("******************************************************************************");

var network_list = {};
var file = new File(js.exec_dir + "init-fidonet.ini");
if(file.open("r")) {
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

function exclude_strings(list, patterns, flags)
{
	patterns = [].concat(patterns);
	if(flags === undefined)
		flags = 'i';
	return list.reduce(function (a, c) {
		var matched = patterns.some(function (e) {
			if(typeof e == 'string')
				e = new RegExp(e, flags);
			return c.match(e);
		});
		if(!matched)
			a.push(c);
		return a;
	}, []);
}

function file_contents(filename, dflt, maxlinelen)
{
	var file = new File(filename);
	if(!file.open("r"))
		return dflt;
	var lines = file.readAll(maxlinelen || 1000);
	file.close();
	return lines || dflt;
}

function remove_lines_from_file(filename, patterns, maxlinelen)
{
	var file = new File(filename);
	if(!file.open("r"))
		return "Error " + file.error + " opening " + file.name;
	var lines = file.readAll(maxlinelen || 1000);
	file.close();
	var file = new File(filename);
	if(!file.open("w"))
		return "Error " + file.error + " opening " + file.name;
	var result = file.writeAll(exclude_strings(lines, patterns));
	file.close();
	return result;
}

function remove_prefix_from_title(filename, prefix)
{
	var file = new File(filename);
	if(!file.open("r"))
		return "Error " + file.error + " opening " + file.name;
	var lines = file.readAll(1000);
	file.close();
	var longest = 0;
	var list = [];
	for(var i = 0; i < lines.length; i++) {
		var a = lines[i].split(/\s+/);
		var tag = a.shift();
		if(tag.length > longest)
			longest = tag.length;
		var title = a.join(' ');
		if(title.toLowerCase().indexOf(prefix.toLowerCase()) == 0)
			title = title.substr(prefix.length);
		list.push({ tag: tag, title: title});
	}
	var file = new File(filename);
	if(!file.open("w"))
		return "Error " + file.error + " opening " + file.name;
	for(var i in list)
		file.writeln(format("%-*s %s", longest, list[i].tag, list[i].title));
	file.close();
	return true;
}

function find_sys_addr(addr)
{
	for(var i = 0; i < system.fido_addr_list.length; i++) {
		if(system.fido_addr_list[i] == addr)
			return true;
	}
	return false;
}

function send_app_netmail(destaddr)
{
	var hdr = {
		to: link.Name,
		to_net_addr: destaddr,
		from: sysop,
		from_ext: 1,
		subject: netname + " node number request"
	}
	print("Message text:");
	var body_text = "Hello, this is " + sysop + " from " + system.location + "\r\n";
	body_text += "and I am requesting a node number for zone " + netzone + " in " + netname + ".\r\n";
	body_text += "\r\n";
	body_text += "My system is " + system.name + " at " + system.inet_addr + ".\r\n";
	body_text += "\r\n";
	body_text += "I am using Synchronet-" + system.platform + " v" + system.full_version
		+ " with SBBSecho and BinkIT.\r\n";
	body_text += "\r\n";
	body_text += "My requested AreaFix password is: '" + link.AreaFixPwd + "'\r\n";
	body_text += "My requested BinkP Session password is: '" + link.SessionPwd + "'\r\n";
	if(link.TicFilePwd)
		body_text += "My requested TIC Password is: '" + link.TicFilePwd  + "'\r\n";
	body_text += "\r\n";
	body_text += "I will be using 'Type-2+' (FSC-39) packets with no password.\r\n";
	body_text += "Uncompressed or PKZIP-archived EchoMail bundles will work fine.\r\n";
	print(body_text);
	while(confirm("Add more text") && !aborted()) {
		body_text += "\r\n";
		var str;
		while((str=prompt("[done]")) && !aborted()) body_text += str + "\r\n";
	}
	if(aborted() || !confirm("Send now"))
		return false;
	var msgbase = new MsgBase("mail");
	if(msgbase.open() == false)
		return "Error opening mail base: " + msgbase.last_error;
	if(!msgbase.save_msg(hdr, body_text)) {
		msgbase.close();
		return "Error saving message: " + msgbase.last_error;
	}
	msgbase.close();
	print("Application NetMail message created successfully for " + destaddr);
	return true;
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

	var file = new File("sbbsecho.ini");
	if(!file.open("r")) {
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

function get_domain(zone)
{
	var file = new File("sbbsecho.ini");
	if(!file.open("r")) {
		alert("Error " + file.error + " opening " + file.name);
		return false;
	}

	var domain_list = file.iniGetSections("domain:");
	if(domain_list) {
		var zonemap = {};
		for(var i = 0; i < domain_list.length && !result; i++) {
			var section = domain_list[i];
			var netname = section.substr(7)
			var zones = file.iniGetValue(section, "Zones");
			if(!zones)
				continue;
			if(typeof zones == 'number') {
				if(zone == zones) {
					return netname;
				}
				continue;
			}
			zones = zones.split(',');
			for(var j = 0; j < zones.length; j++) {
				if(zone == zones[j]) {
					return netname;
				}
			}
		}
		file.close();
		return result;
	}
	return "";
}

function get_linked_node(addr, domain)
{
	var file = new File("sbbsecho.ini");
	if(!file.open("r"))
		return false;
	var result = false;
	if(domain)
		result = file.iniGetObject("node:" + addr + "@" + domain);
	if (!result)
		result = file.iniGetObject("node:" + addr);
	file.close();
	return result;
}

function get_binkp_sysop()
{
	var file = new File("sbbsecho.ini");
	if(!file.open("r"))
		return false;
	var result = file.iniGetValue("Binkp", "Sysop");
	file.close();
	return result;
}

function update_sbbsecho_ini(hub, link, domain, echolist_fname, areamgr)
{
	function makepath(path)
	{
		if(mkpath(path))
			return true;
		alert("Error " + errno + " (" + errno_str + ") creating " + path);
		return false;
	}

	var file = new File("sbbsecho.ini");
	if(!file.open("r+")) {
		return "Error " + file.error + " opening " + file.name;
	}
	var path = file.iniGetValue(null, "Inbound");
	if(!path)
		path = "../fido/nonsecure";
	while((!path
		|| !confirm("Non-secure inbound directory is '" + path + "'")
		|| !makepath(path)) && !aborted())
		path = prompt("Non-secure inbound directory");
	file.iniSetValue(null, "Inbound", path);

	path = file.iniGetValue(null, "SecureInbound");
	if(!path)
		path = "../fido/inbound";
	while((!path
		|| !confirm("Secure inbound directory is '" + path + "'")
		|| !makepath(path)) && !aborted())
		path = prompt("Secure inbound directory");
	file.iniSetValue(null, "SecureInbound", path);

	path = file.iniGetValue(null, "Outbound");
	if(!path)
		path = "../fido/outbound";
	while((!path
		|| !confirm("Outbound directory is '" + path + "'")
		|| !makepath(path)) && !aborted())
		path = prompt("Outbound directory");
	file.iniSetValue(null, "Outbound", path);

	var binkp = file.iniGetObject("BinkP");
	if(!binkp) binkp = {};
	binkp.sysop = sysop;
	if(!file.iniSetObject("binkp", binkp)) {
		return "Error" + file.error + " writing to " + file.name;
	}

	var prefnode;
	var section = "node:" + fidoaddr.to_str(hub);

	if(domain) {
		if(file.iniGetObject(section)) {
			if(confirm("Migrate " + section + " to " + section + "@" + domain)) {
				if(!file.iniSetObject(section + "@" + domain, link)) {
					return "Error " + file.error + " writing to " + file.name;
				} else {
					file.iniRemoveSection(section);
				}
			}
		} else {
			if(!file.iniGetObject(section) || confirm("Overwrite hub [" + section + "@" + domain + "] configuration in " + file.name)) {
				if(!file.iniSetObject(section + "@" + domain, link)) {
					return "Error " + file.error + " writing to " + file.name;
				}
			}
		}
	} else {
		if(!file.iniGetObject(section) || confirm("Overwrite hub [" + section + "] configuration in " + file.name)) {
			if(!file.iniSetObject(section, link)) {
				return "Error " + file.error + " writing to " + file.name;
			}
		}
	}

	var section = "node:" + hub.zone + ":ALL";
	if(confirm("Route all zone " + hub.zone + " netmail through your hub")) {
		if(!file.iniSetObject(section,
			{
				Comment: "Everyone in zone " + hub.zone,
				Route: fidoaddr.to_str(hub)
			})) {
			return "Error " + file.error + " writing to " + file.name;
		}
	}
	if(echolist_fname) {
		var section = "echolist:" + echolist_fname;
		if(!file.iniGetObject(section)
			|| confirm("Overwrite [" + section + "] configuration in " + file.name)) {
			if(!file.iniSetObject(section,
				{
					Hub: fidoaddr.to_str(hub),
					Pwd: link.AreaFixPwd,
					AreaMgr: areamgr || "AreaFix",
					Fwd: false
				})) {
				return "Error " + file.error + " writing to " + file.name;
			}
		}
	}
	file.close();
	print(file.name + " updated successfully.");
	return true;
}

/****************************/
/* DETERMINE NETWORK (ZONE) */
/****************************/
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
		if(network_list[zone].coord || network_list[zone].email || network_list[zone].fido) {
			var email = "";
			if(network_list[zone].email)
				email = " <" + network_list[zone].email + ">";
			if(network_list[zone].fido)
				email += " " + network_list[zone].fido;
			// removed because screen is scrolling so much with so many networks
			//print("       coordinator: " + (network_list[zone].coord || "") + email);
		}
	}
	var which;
	while((!which || which < 1) && !aborted()) {
		var str = prompt("Which or [Q]uit");
		if(str && str.toUpperCase() == 'Q')
			exit(0);
		which = parseInt(str, 10);
	}
	netzone = which;
	network = network_list[which];
}

if(network)
	netname = network.name;
else
	network = {};


var domain = get_domain(netzone);

if(netzone <= 6)
	netname = "FidoNet";
else {
	while((!netname || netname.indexOf(' ') >= 0 || netname.length > 8
		|| !confirm("Network name is '" + netname + "'")) && !aborted()) {
		var str = prompt("Network name (no spaces or illegal filename chars) [" + netname + "]");
		if(str)
			netname = str;
	}
}
if(netname) {
	print("Network name: " + netname);
	print("Network zone: " + netzone);
	print("Network info: " + network.info);
	if(domain)
		print("Network domain: " + domain);
	if (network.pack) {
		print("Network pack: " + network.pack);
	}
	print("Network coordinator: " + network.coord
		+ (network.email ? (" <" + network.email + ">") : "")
		+ (network.fido ? (" " + network.fido) : ""));
	if(network.also)
		print("        also: " + network.also);
	print("EchoList: " + file_getname(network.echolist));
} else
	alert("Unrecognized network zone: " + netzone);

print("Reading Message Area configuration file: msgs.cnf");
var cnflib = load({}, "cnflib.js");
var msgs_cnf = cnflib.read("msgs.cnf");
if(!msgs_cnf) {
	alert("Failed to read msgs.cnf");
	exit(1);
}

var your = {zone: NaN, net: NaN, node: temp_node, point: 0};
for(var i = 0; i < system.fido_addr_list.length; i++) {
	var addr = fidoaddr.parse(system.fido_addr_list[i]);
	if(!addr || addr.zone != netzone)
		continue;
	if(deny("You already have a " + netname + " Address (" + system.fido_addr_list[i] + "), continue"))
		exit(0);
	your = addr;
	break;
}

/*********************************/
/* DETERMINE YOUR HUB'S ADDRESSS */
/*********************************/
var hub = {zone: netzone ? netzone : NaN, net: NaN, node: NaN};
if(network.addr)
	hub = fidoaddr.parse(network.addr);
while(((isNaN(hub.zone) || hub.zone < 1)
	|| (isNaN(hub.net)  || hub.net < 1)
	|| (isNaN(hub.node) ||  hub.node < 0)
	|| !confirm("Your hub's address is " + fidoaddr.to_str(hub))) && !aborted()) {
	hub = fidoaddr.parse(prompt("Your hub's address (zone:net/node)"));
}

var link = get_linked_node(fidoaddr.to_str(hub), domain);
if(!link)
	link = {};

if(!link.Name && fidoaddr.to_str(hub) == network.addr)
	link.Name = network.coord;

while((!link.Name || !confirm("Your hub's sysop's name is '" + link.Name + "'")) && !aborted()) {
	link.Name = prompt("Your hub's sysop's name");
}

if(!link.BinkpHost)
	link.BinkpHost = network.host;
while(!network.dns && (!link.BinkpHost
	|| !confirm("Your hub's hostname or IP address is " + link.BinkpHost)) && !aborted()) {
	link.BinkpHost = prompt("Your hub's hostname or IP address");
}

if(!link.BinkpPort)
	link.BinkpPort = network.port || 24554;
while(!link.BinkpPort || !confirm("Your hub's BinkP/TCP port number is " + link.BinkpPort) && !aborted()) {
	link.BinkpPort = parseInt(prompt("Your hub's BinkP/TCP port number"));
}

if(!link.Comment)
	link.Comment = network.email;

if(!link.GroupHub)
	link.GroupHub = netname;

if(link.BinkpPoll === undefined)
	link.BinkpPoll = true;

/***************************/
/* DETERMINE YOUR ADDRESSS */
/***************************/
if(!your.zone)
	your.zone = hub.zone;
if(!your.net)
	your.net = hub.net;
while(!confirm("Your node address is " + fidoaddr.to_str(your)) && !aborted()) {
	your.zone = NaN;
	your.net = NaN;
	your.node = NaN;
	while((isNaN(your.zone) || your.zone < 1) && !aborted())
		your.zone = parseInt(prompt("Your zone number (e.g. " + hub.zone + ")"));
	while((isNaN(your.net) || your.net < 1) && !aborted())
		your.net = parseInt(prompt("Your network number (e.g. " + hub.net + ")"));
	while((isNaN(your.node) || your.node < 1) && !aborted())
		your.node = parseInt(prompt("Your node number (e.g. " + temp_node + " for temporary node)"));
	while((isNaN(your.point)) && !aborted())
		your.point = parseInt(prompt("Your point number (i.e. 0 for a normal node)"));
}

/* Get/Confirm Sysop Name */
var sysop = system.operator;
if(system.stats.total_users) {
	var u = new User(1);
	if(u && u.name)
		sysop = u.name;
}
sysop = get_binkp_sysop() || sysop;
while((!sysop || !confirm("Your name is '" + sysop + "'")) && !aborted())
	sysop = prompt("Your name");

/* Get/Confirm passwords */
while((!link.AreaFixPwd || !confirm("Your AreaFix Password is '" + link.AreaFixPwd + "'")) && !aborted())
	link.AreaFixPwd = prompt("Your AreaFix (a.k.a. Area Manager) Password (case in-sensitive)");
while((!link.SessionPwd || !confirm("Your BinkP Session Password is '" + link.SessionPwd + "'")) && !aborted())
	link.SessionPwd = prompt("Your BinkP Session Password (case sensitive)");
while(((!link.TicFilePwd && (link.TicFilePwd !== "")) || !confirm("Your TIC File Password is '" + (link.TicFilePwd ? link.TicFilePwd : "(not set)") + "'")) && !aborted())
	link.TicFilePwd = prompt("Your TIC File Password (case sensitive) (optional)");

/***********************************************/
/* SEND NODE NUMBER REQUEST NETMAIL (Internet) */
/***********************************************/
if(your.node === temp_node && network.email && network.email.indexOf('@') > 0
	&& confirm("Send a node number application to " + network.email)) {
	var result = send_app_netmail(network.email);
	if(typeof result !== 'boolean') {
		alert(result);
		exit(1);
	}
	if(aborted())
		exit(0);
	if(confirm("Come back when you have your permanently-assigned node address")) {
		if(confirm("Save changes to FidoNet configuration file: sbbsecho.ini")) {
			var result = update_sbbsecho_ini(hub, link, domain);
			if (result != true) {
				alert(result);
				exit(1);
			}
		}
		exit(0);
	}
}

if(!find_sys_addr(fidoaddr.to_str(your))
	&& confirm("Add node address " + fidoaddr.to_str(your) + " to your configuration"))
	msgs_cnf.fido_addr_list.push(your);

if(!msgs_cnf.fido_default_origin)
	msgs_cnf.fido_default_origin = system.name + " - " + system.inet_addr;
while((!msgs_cnf.fido_default_origin
	|| !confirm("Your origin line is '" +	msgs_cnf.fido_default_origin + "'")) && !aborted()) {
	msgs_cnf.fido_default_origin = prompt("Your origin line");
}

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
		"code_prefix": network.areatag_prefix === undefined
			? (netname.toUpperCase() + "_") : network.areatag_prefix
	});
}
if(confirm("Save Changes to Message Area configuration file: msgs.cnf")) {
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
load("http.js");
if(network.echolist
	&& (network.echolist.indexOf("http://") == 0 || network.echolist.indexOf("https://") == 0)
	&& confirm("Download " + netname + " EchoList: " + file_getname(network.echolist))) {
	var echolist_url = network.echolist;

	while(!aborted()) {
		while((!echolist_url || !confirm("Download from: " + echolist_url)) && !aborted()) {
			echolist_url = prompt("Echolist URL");
		}
		var file = new File(echolist_fname);
		if(!file.open("w")) {
			alert("Error " + file.error + " opening " + file.name);
			exit(1);
		}
		var http_request = new HTTPRequest();
		try {
			var contents = http_request.Get(echolist_url);
		} catch(e) {
			alert(e);
			file.close();
			file_remove(file.name);
			if(!confirm("Try again"))
				break;
			continue;
		}
		if(http_request.response_code == http_request.status.ok) {
			print("Downloaded " + echolist_url + " to " + system.ctrl_dir + file.name);
			file.write(contents);
			file.close();
			break;
		}
		file.close();
		file_remove(file.name);
		alert("Error " + http_request.response_code + " downloading " + echolist_url);
		if(!confirm("Try again"))
			break;
	}
} else if (network.pack
	&& (network.pack.indexOf("http://") == 0 || network.pack.indexOf("https://") == 0)
	&& confirm("Download " + netname + " Info Pack: " + network.pack)) {
	while(!aborted()) {
		var packdlfilename = file_getname(network.pack)
		var file = new File(packdlfilename);
		if(!file.open("w")) {
			alert("Error " + file.error + " opening " + file.name);
			exit(1);
		}
		var http_request = new HTTPRequest();
		try {
			var contents = http_request.Get(network.pack);
		} catch(e) {
			alert(e);
			file.close();
			file_remove(system.ctrl_dir + file.name);
			if(!confirm("Try again"))
				break;
			continue;
		}
		if(http_request.response_code == http_request.status.ok) {
			print("Downloaded " + network.pack + " to " + system.ctrl_dir + file.name);
			file.write(contents);
			file.close();

			// try to extract
			var prefix = "";
			if(system.platform == "Win32")
				prefix = system.exec_dir;
			if (system.exec(prefix + "unzip -CLjo " + file_getname(network.pack) + " " + echolist_fname) !== 0) {
				print("Please extract " + network.echolist + " from " + file.name + " into " + system.ctrl_dir);
			}

			break;
		}
		file.close();
		file_remove(file.name);
		alert("Error " + http_request.response_code + " downloading " + network.pack);
		if(!confirm("Try again"))
			break;
	}
}
while(echolist_fname && !file_getcase(echolist_fname) && !aborted()) {
	alert(system.ctrl_dir + echolist_fname + " does not exist");
	if ((network.echolist.indexOf("http://") == -1) && (network.echolist.indexOf("https://") == -1)
		&& (network.pack)) {
		if (!confirm("Please extract the " + echolist_fname + " file from the pack " + file_getname(network.pack) + " into " + system.ctrl_dir + ". Continue?")) {
			break;
		}
	} else {
		if (!confirm("Please place " + echolist_fname + " into " + system.ctrl_dir + ". Continue?")) {
			break;
		}
	}

	prompt(echolist_fname + " not found. Please put file into ctrl dir and press Enter.");
}
echolist_fname = file_getcase(system.ctrl_dir + echolist_fname)
if(echolist_fname && file_size(echolist_fname) > 0) {
	if(network.areatag_exclude) {
		print("Removing " + network.areatag_exclude + " from " + echolist_fname);
		var result = remove_lines_from_file(echolist_fname, network.areatag_exclude.split(','));
		if(result !== true)
			alert(result);
	}
	if(network.areatitle_prefix) {
		print("Removing " + network.areatitle_prefix + " Title Prefixes from " + echolist_fname);
		var result = remove_prefix_from_title(echolist_fname, network.areatitle_prefix);
		if(result !== true)
			alert(result);
	}
	if(confirm("Import EchoList (" + echolist_fname + ") into Message Group: " + netname)) {
		print("Importing " + echolist_fname);
		var misc = 0;
		if(!network.handles)
			misc |= SUB_NAME;
		system.exec(system.exec_dir + "scfg"
			+ " -import=" + echolist_fname
			+ " -g" + netname
			+ " -faddr=" + fidoaddr.to_str(your)
			+ " -misc=" + misc
		);
	}
}

/***********************/
/* UPDATE SBBSECHO.INI */
/***********************/
if(confirm("Save changes to FidoNet configuration file: sbbsecho.ini")) {
	var result = update_sbbsecho_ini(hub, link, domain, echolist_fname, network.areamgr);
	if (result != true) {
		alert(result);
		exit(1);
	}
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

/******************************************/
/* SEND NODE NUMBER REQUEST NETMAIL (FTN) */
/******************************************/
if(your.node === temp_node) {
	if(confirm("Send a node number application to "	+ fidoaddr.to_str(hub))) {
		var result = send_app_netmail(fidoaddr.to_str(hub));
		if(typeof result !== 'boolean') {
			alert(result);
			exit(1);
		}
		if(aborted() || confirm("Come back when you have a permanently-assigned node address"))
			exit(0);
	} else if(network.fido && confirm("Send a node number application to " + network.fido)) {
		var result = send_app_netmail(network.fido);
		if(typeof result !== 'boolean') {
			alert(result);
			exit(1);
		}
		if(aborted() || confirm("Come back when you have a permanently-assigned node address"))
			exit(0);
	}
}

/************************/
/* SEND AREAFIX NETMAIL */
/************************/
if(your.node !== temp_node
	&& confirm("Send an AreaFix request to link EchoMail areas with "
		+ fidoaddr.to_str(hub))) {
	var msgbase = new MsgBase("mail");
	if(msgbase.open() == false) {
		alert("Error opening mail base: " + msgbase.last_error);
		exit(1);
	}
	var lines = file_contents(echolist_fname, ["%+ALL"]);
	for(var i in lines) {
		lines[i] = lines[i].split(/\s+/)[0];
	}
	if(!msgbase.save_msg({
		to: network.areamgr || "AreaFix",
		to_net_addr: fidoaddr.to_str(hub),
		from: sysop,
		from_ext: 1,
		subject: link.AreaFixPwd
	}, /* body text: */ lines.join('\r\n'))) {
		alert("Error saving message: " + msgbase.last_error);
		exit(1);
	}
	msgbase.close();
	print("AreaFix NetMail message created successfully.");
}

/**************************************************************/
/* Add links to logs/stats files to "Operator" G-file section */
/**************************************************************/
function add_gfile(fname, desc, tail)
{
	function file_append(fname, text)
	{
		var file = new File(fname);
		if(!file.open("at")) {
			alert("Error " + file.error + " opening " + file.name);
			return false;
		}
		file.writeAll(text);
		file.close();
		return true;
	}
	var file = new File(system.data_dir + "text/operator.ini");
	if(!file.open(file.exists ? 'r+':'w+')) {
		alert("Error " + file.error + " opening " + file.name);
		return false;
	}
	var section = "%j" + fname;
	if(file.iniGetObject(section) == null && confirm("Add " + desc)) {
		if(file.iniSetObject(section, { desc: "Fido: " + desc, tail: tail }))
			print("Added " + fname + " to " + file.name);
	}
	file.close();
}
print("Updating the 'Operator' Text File Section:");
add_gfile("sbbsecho.log", "NetMail and EchoMail import/export activity log", 500);
add_gfile("badareas.lst", "Unrecognized received EchoMail area list");
add_gfile("echostats.ini", "Detailed EchoMail statistics");
add_gfile("binkstats.ini", "Detailed BinkP (mail/file transfer) statistics");

/***********************/
/* DISPLAY FINAL NOTES */
/***********************/
print(netname + " initial setup completely successfully.");
print();
if(your.node == temp_node) {
	print("You used a temporary node address (" + fidoaddr.to_str(your) +
		"). You will need to update your");
	print("SCFG->Networks->FidoNet->Address once your permanent node address has been");
	print("assigned to you.");
	print();
}

print("See your 'Events' log output for outbound BinkP connections.");
print("See your 'Services' log output for inbound BinkP connections.");
print("Use exec/echocfg for follow-up FidoNet-related configuration.");
if(!this.jsexec_revision) {
	print();
	print("It appears you have run this script from the BBS. You must log-off now for the");
	print("server to recycle and configuration changes to take effect.");
}
exit(0);
