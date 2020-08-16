// $Id: sbbsecho_upgrade.js,v 1.14 2019/01/18 23:03:01 rswindell Exp $

// SBBSecho upgrade from v2.x to v3.x (run with jsexec)

// Converts ctrl/sbbsecho.cfg (or other file if specified)
// to ctrl/sbbsecho.ini

const REVISION = "$Revision: 1.14 $".split(' ')[1];

var debug =  false;

function newnode(addr)
{
	if(debug) print("New node: " + addr);
	return { keys: [] };
}

var cfgfile = system.ctrl_dir + "sbbsecho.cfg";
if(argc)
	cfgfile = argv[0];

var file = new File(cfgfile);
if(!file.open("r")) {
	alert(format("Error %d opening %s\n", errno, cfgfile));
	exit();
}
var cfg = file.readAll(4096);
file.close();

var line_num;
var bool_opts = [];
var value_opts = [];
var nodelist = [];
var echolist = [];
var packer = [];
for(line_num in cfg) {
	var line = cfg[line_num].trimLeft().trimRight();
	if(debug) print('"' + line + '"');
	if(line.charAt(0)==';')
		continue;
	var word = line.split(/\s+/);
	var key = word[0].toLowerCase();
	switch(key) {
		default:
			switch(word.length) {
				case 2:
					value_opts[key] = word[1];
					break;
				case 1:
					bool_opts[key] = true;
					break;
				default:
					alert("Unrecognized cfg line: " + line);
					break;
			}
			break;
		case "pkttype":
		case "usepacker":
		case "route_to":
			for(var i = 2; i < word.length; i++) {
				if(!nodelist[word[i]])
					nodelist[word[i]] = newnode(word[i]);
				nodelist[word[i]][key] = word[1];
			}
			break;
		case "pktpwd":
			if(!nodelist[word[1]])
				nodelist[word[1]] = newnode(word[1]);
			nodelist[word[1]][key] = word[2];
			break;
		case "send_notify":
		case "passive":
		case "hold":
		case "crash":
		case "direct":
			for(var i = 1; i < word.length; i++) {
				if(!nodelist[word[i]])
					nodelist[word[i]] = newnode(word[i]);
				nodelist[word[i]][key] = true;
			}
			break;
		case "areafix":
			if(!nodelist[word[1]])
				nodelist[word[1]] = newnode(word[1]);
			nodelist[word[1]].areafix_pwd = word[2];
			nodelist[word[1]].keys = word.slice(3);
			break;
		case "echolist":
			echolist.push(word.slice(1));
			break;
		case "packer":
			packer.push({ name: word[1], offset: word[2], sig: word[3]});
			break;
		case "pack":
		case "unpack":
			packer[packer.length-1][key] = word.slice(1).join(' ');
			break;
		case "end":
		case "regnum":
		case "store_seenby":
		case "store_path":
		case "store_kludge":
		case "noswap":
			/* these are ignored (deprecated, no longer used) */
			break;
	}
}

if (debug) {
	for(var i in bool_opts)
		print("bool " + i);

	for(var i in value_opts)
		print("value " + i + " = " + value_opts[i]);

	for(var i in nodelist) {
		print(i);
		print(JSON.stringify(nodelist[i]));
	}

	for(var i in echolist)
		print("echolist: " + JSON.stringify(echolist[i]));

	for(var i in packer)
		print("packer: " + JSON.stringify(packer[i]));
}

var inifile = system.ctrl_dir + "sbbsecho.ini";
if(argc > 1)
	inifile = argv[1];

var file = new File(inifile);
if(!file.open("w")) {
	alert(format("Error %d opening %s\n", errno, inifile));
	exit();
}

file.writeln("; Converted from " + cfgfile + " using sbbsecho_upgrade.js " + REVISION);
file.writeln("CheckPathsForDupes = " + !Boolean(bool_opts["nopathcheck"])), delete bool_opts["nopathcheck"];
delete bool_opts["nocircularfwd"];
file.writeln("KillEmptyNetmail = " + Boolean(bool_opts["kill_empty"])), delete bool_opts["kill_empty"];
file.writeln("AreaAddFromEcholistsOnly = " + Boolean(bool_opts["elist_only"])), delete bool_opts["elist_only"];
file.writeln("SecureEchomail = " + Boolean(bool_opts["secure_echomail"])), delete bool_opts["secure_echomail"];
file.writeln("BinkleyStyleOutbound = " + Boolean(bool_opts["flo_mailer"])), delete bool_opts["flo_mailer"];
file.writeln("TruncateBundles = " + Boolean(bool_opts["trunc_bundles"])), delete bool_opts["trunc_bundles"];
file.writeln("FuzzyNetmailZones = " + Boolean(bool_opts["fuzzy_zone"])), delete bool_opts["fuzzy_zone"];
file.writeln("StripLineFeeds = " + Boolean(bool_opts["strip_lf"])), delete bool_opts["strip_lf"];
file.writeln("ConvertTearLines = " + Boolean(bool_opts["convert_tear"])), delete bool_opts["convert_tear"];
for(var i in bool_opts)
	if(i) file.writeln(i + " = true");
file.writeln();
delete value_opts["notify"];
if(value_opts["sysop_alias"])
	file.writeln("SysopAliasList = " + value_opts["sysop_alias"]), delete value_opts["sysop_alias"];
file.writeln("ZoneBlind = " + Boolean(value_opts["zone_blind"]));
if(parseInt(value_opts["zone_blind"]))
	file.writeln("ZoneBlindThreshold = " + parseInt(value_opts["zone_blind"])), delete value_opts["zone_blind"];
if(value_opts["log"])
	delete value_opts["log"];
if(value_opts["log_level"])
	file.writeln("LogLevel = " + value_opts["log_level"]), delete value_opts["log_level"];
if(!value_opts["inbound"] && value_opts["outbound"]) {
	var inbound = value_opts["outbound"].replace("out","in");
	alert("Setting non-secure inbound dir to best guess (no longer using SCFG setting): " + inbound);
	file.writeln("inbound = " + inbound);
	if(!file_isdir(inbound))
		alert("Non-secure inbound directory (" + inbound + ") does not appear to exist.");
}
if(value_opts["secure_inbound"])
	file.writeln("SecureInbound = " + value_opts["secure_inbound"]), delete value_opts["secure_inbound"];
if(value_opts["arcsize"])
	file.writeln("BundleSize = " + value_opts["arcsize"]), delete value_opts["arcsize"];
if(value_opts["pktsize"])
	file.writeln("PacketSize = " + value_opts["pktsize"]), delete value_opts["pktsize"];

for(var i in value_opts)
	if(i) file.writeln(i + " = " + value_opts[i]);

for(var i in nodelist) {
	file.writeln();
	file.writeln("[node:" + i + "]");
	file.writeln("\tComment = ");
	if(nodelist[i].pkttype)
		file.writeln("\tPacketType = " + nodelist[i].pkttype);
	file.writeln("\tPacketPwd = " + (nodelist[i].pktpwd || ""));
	file.writeln("\tAreafixPwd = " + (nodelist[i].areafix_pwd || ""));
	file.writeln("\tNotify = " + Boolean(nodelist[i].send_notify));
	file.writeln("\tPassive = " + Boolean(nodelist[i].passive));
	file.writeln("\tDirect = " + Boolean(nodelist[i].direct));
	file.writeln("\tStatus = " + (nodelist[i].crash ? "crash" : nodelist[i].hold ? "hold" : "normal"));
	file.writeln("\tArchive = " + (nodelist[i].usepacker || "none"));
	file.writeln("\tKeys = " + nodelist[i].keys.join(","));
	if(nodelist[i].route_to)
		file.writeln("\tRoute = " + nodelist[i].route_to);
}
for(var i in packer) {
	file.writeln();
	file.writeln("[archive:" + packer[i].name + "]");
	file.writeln("\tSigOffset = " + packer[i].offset);
	file.writeln("\tSig = " + packer[i].sig);
	file.writeln("\tPack = " + packer[i].pack);
	file.writeln("\tUnpack = " + packer[i].unpack);
}
for(var i in echolist) {
	var elist = echolist[i];
	var hub = { addr: '', pwd: '' };
	var forward;
	if(elist[0].toUpperCase()=="FORWARD") {
		elist.shift();
		hub = { addr: elist.shift(), pwd: elist.shift() };
		forward = true;
	} else if(elist[0].toUpperCase()=="HUB") {
		elist.shift();
		hub = { addr: elist.shift(), pwd: '' };
	}

	file.writeln();
	file.writeln("[echolist:" + elist.shift() + "]");
	file.writeln("\tKeys = " + elist.join(","));
	file.writeln("\tHub = " + hub.addr);
	file.writeln("\tPwd = " + hub.pwd);
	file.writeln("\tFwd = " + Boolean(forward));
}

file.close();
print(Object.keys(nodelist).length + " Nodes");
print(packer.length + " Packers");
print(Object.keys(echolist).length + " EchoLists");
print(cfgfile + " successfully converted to " + inifile);
if(file_getext(cfgfile).toLowerCase() == ".cfg") {
	var oldcfg = cfgfile + ".old";
	if(file_rename(cfgfile,oldcfg))
		print(cfgfile + " renamed to " + oldcfg);
	else
		alert("Error renaming " + cfgfile + " to " + oldcfg);
}
