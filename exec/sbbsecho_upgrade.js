// $Id$

// SBBSecho upgrade from v2.x to v3.x (run with jsexec)

// Converts ctrl/sbbsecho.cfg (or other file if specified)
// to ctrl/sbbsecho.ini

var debug =  false;

function newnode()
{
	return { echolists: [] };
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
var line_num;
var bool_opts = [];
var value_opts = [];
var nodelist = [];
var echolist = [];
var packer = [];
for(line_num in cfg) {
	var line = cfg[line_num].trimLeft();
	if(debug) print(line);
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
					bool_opts.push(key);
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
					nodelist[word[i]] = newnode();
				nodelist[word[i]][key] = word[1];
			}
			break;
		case "pktpwd":
			if(!nodelist[word[1]])
				nodelist[word[1]] = newnode();
			nodelist[word[1]][key] = word[2];
			break;
		case "send_notify":
		case "passive":
		case "hold":
		case "crash":
		case "direct":
			for(var i = 1; i < word.length; i++) {
				if(!nodelist[word[i]])
					nodelist[word[i]] = newnode();
				nodelist[word[i]][key] = true;
			}
			break;
		case "areafix":
			if(!nodelist[word[1]])
				nodelist[word[1]] = newnode();
			nodelist[word[1]].areafix_pwd = word[2];
			nodelist[word[1]].echolists = word.slice(3);
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
		print("bool " + bool_opts[i]);

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

file.writeln("check_path = " + !Boolean(bool_opts["nopathcheck"])), delete bool_opts["nopathcheck"];
file.writeln("fwd_circular = " + !Boolean(bool_opts["nocircularfwd"])), delete bool_opts["nocircularfwd"];
for(var i in bool_opts)
	file.writeln(bool_opts[i] + " = true");
file.writeln("notify_user = " + parseInt(value_opts["notify"])), delete value_opts["notify"];
file.writeln("zone_blind = " + Boolean(value_opts["zone_blind"]));
if(parseInt(value_opts["zone_blind"]))
	file.writeln("zone_blind_threshold = " + parseInt(value_opts["zone_blind"])), delete value_opts["zone_blind"];
for(var i in value_opts)
	file.writeln(i + " = " + value_opts[i]);

for(var i in nodelist) {
	file.writeln();
	file.writeln("[node:" + i + "]");
	file.writeln("\tcomment = ");
	if(nodelist[i].pkttype)
		file.writeln("\tpacket_type = " + nodelist[i].pkttype);
	file.writeln("\tpacket_pwd = " + (nodelist[i].pktpwd || ""));
	file.writeln("\tareafix_pwd = " + (nodelist[i].areafix_pwd || ""));
	file.writeln("\tinbox = ");
	file.writeln("\toutbox = ");
	file.writeln("\tnotify = " + Boolean(nodelist[i].send_notify));
	file.writeln("\tpassive = " + Boolean(nodelist[i].passive));
	file.writeln("\tdirect = " + Boolean(nodelist[i].direct));
	file.writeln("\tstatus = " + (nodelist[i].crash ? "crash" : nodelist[i].hold ? "hold" : "normal"));
	file.writeln("\tpacker = " + (nodelist[i].usepacker || "none"));
	file.writeln("\techolists = " + nodelist[i].echolists.join(","));
}
for(var i in packer) {
	file.writeln();
	file.writeln("[packer:" + packer[i].name + "]");
	file.writeln("\tsig_offset = " + packer[i].offset);
	file.writeln("\tsig = " + packer[i].sig);
	file.writeln("\tpack = " + packer[i].pack);
	file.writeln("\tunpack = " + packer[i].unpack);
}
for(var i in echolist) {
	var elist = echolist[i];
	var hub = { addr: '', pwd: '' };
	var forward;
	if(elist[0].toUpperCase()=="FORWARD") {
		elist.shift();
		hub = { addr: elist.shift(), pwd: elist.shift() };
		forward = true;
	} else if(elist[0].toUpperCase()=="HUB")
		hub = { addr: elist.shift(), pwd: '' };

	file.writeln();
	file.writeln("[echolist:" + elist.shift() + "]");
	file.writeln("\tflags = " + elist.join(","));
	file.writeln("\thub = " + hub.addr);
	file.writeln("\tpwd = " + hub.pwd);
	file.writeln("\tfwd = " + Boolean(forward));
}

file.close();
print(cfgfile + " successfully converted to " + inifile);