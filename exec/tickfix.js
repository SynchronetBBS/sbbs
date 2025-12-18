// TickFix: A remote Area Manager (ala FileFix) for TickIT
// Requires SBBSecho v3.11 or later
// Install using "jsexec tickfix -install"

const REVISION = "1.8";
require('smbdefs.js', 'NET_FIDO');
var fidoaddr = load({}, 'fidoaddr.js');

// Parse tickfix.ini
var f = new File(file_cfgname(system.ctrl_dir, "tickfix.ini"));
f.open("r");
var AreaMgr = f.iniGetValue(null, "AreaMgr", ["FileFix"]);
var backup_level = f.iniGetValue(null, "Backups", 10);
f.close();
for(var i in AreaMgr)
	AreaMgr[i] = AreaMgr[i].toLowerCase();

const help_text = 
	"TickFix " + REVISION + " Help\r\n" +
	"\r\n" +
	"In the body of the message, one or more:\r\n" +
	"\r\n" +
	"+<tag>     Connect an area\n" +
	"+ALL       Connect all areas\n" +
	"-<tag>     Disconnect an area\n" +
	"-ALL       Disconnect all areas\n" +
	"%HELP      This message\r\n" +
	"%LIST      List of all available areas (tags)\r\n" +
	"%QUERY     List of linked / connected areas\r\n" +
	"%UNLINKED  List of unlinked areas\r\n" +
	"[---]      Everything below the tear line is ignored"
	;
	
if(argv.indexOf("-install") >= 0)
{
	print("Installing TickFix");
	
	var cfglib = load({}, "cfglib.js");
	var xtrn_cfg = cfglib.read("xtrn.ini");
	if(!xtrn_cfg)
		js.report_error("Failed to read xtrn.ini", /* fatal */true);

	var changed = false;
	if(!xtrn_area.event["tickfix"]) {
		print("Adding timed event: TICKFIX");
		xtrn_cfg.event.push( {
				"code": "TICKFIX",
				"cmd": "?tickfix",
				"days": 0,
				"time": 0,
				"node_num": 1,
				"settings": 0,
				"startup_dir": "",
				"freq": 30,		// interval
				"mdays": 0,
				"months": 0
				});
		changed = true;
	}
	if(changed && !cfglib.write("xtrn.ini", undefined, xtrn_cfg))
		js.report_error("Failed to write xtrn.ini", /* fatal */true);

	var f = new File(file_cfgname(system.ctrl_dir, "sbbsecho.ini"));
	file_backup(f.name, backup_level);
	if(!f.open(f.exists ? 'r+':'w+'))
		js.report_error(f.error + " opening " + f.name, /* fatal */true);
	var section = "robot:filefix";
	if(!f.iniGetObject(section)) {
		log("Adding [" + section + "] in " + f.name);
		f.iniSetValue(section, "SemFile", "../data/tickfix.now");
		f.iniSetValue(section, "attr", "0x0020");
	}
	f.close();
	exit(0);
}

function delete_msg(hdr)
{
	hdr.attr |= MSG_DELETE;
	if(!msgbase.put_msg_header(true, i, hdr))
		js.report_error(msgbase.last_error + " deleting msg");
}
function reply_to_msg(re, text)
{
	log(LOG_INFO, "Replying to " + re.from_net_addr);
	var hdr = {
		from: "TickFix",
		from_net_type: re.to_net_type,
		from_net_addr: re.to_net_addr,
		to: re.from,
		to_net_type: re.from_net_type,
		to_net_addr: re.from_net_addr,
		subject: "Area Manager Response",
		ftn_reply: re.ftn_msgid,
		ftn_pid: "TickFix " + REVISION,
	};
	text += "\r\n--- " + hdr.ftn_pid;
	if(!msgbase.save_msg(hdr, text))
		js.report_error(msgbase.last_error + " saving response msg to " + msgbase.file);
	log("Response to " + hdr.to_net_addr + " saved successfully");
	if(argv.indexOf("-debug") < 0)
		delete_msg(re);
}

function area_description(tag)
{
	var desc = area_desc[tag.toLowerCase()];
	if(desc)
		return format("%-25s %s", tag, desc);
	return tag;
}

function handle_command(hdr, cmd)
{
	var me = hdr.to_net_addr;
	var you = hdr.from_net_addr;
	cmd = cmd.trimRight();
	switch(cmd.toLowerCase()) {
		case "%help":
			return help_text;
		case "%list":
			var response = [];
			for(var a in area_list)
				response.push(area_description(area_list[a]));
			return format("%u file areas are available from %s to %s:\r\n\r\n"
				,response.length, me, you) + response.join("\r\n");
		case "%query":
			var response = [];
			for(var a in area_map) {
				if(area_map[a].indexOf(you) >= 0)
					response.push(area_description(a.toUpperCase()));
			}
			return format("%u file areas are connected from %s to %s:\r\n\r\n"
				,response.length, me, you) + response.join("\r\n");
		case "%unlinked":
			var response = [];
			for(var a in area_map) {
				if(area_map[a].indexOf(you) < 0)
					response.push(area_description(a.toUpperCase()));
			}
			return format("%u available file areas on %s are not connected to %s:\r\n\r\n"
				,response.length, me, you) + response.join("\r\n");
		default:
			var tag = cmd.toLowerCase();
			if(tag[0] == '-') {	// Remove
				tag = tag.substring(1);
				if(tag == "all") {
					for(var a in area_map) {
						var i = area_map[a].indexOf(you);
						if(i >= 0)
							area_map[a].splice(i, 1);
					}
					return you + " is now disconnected from all file areas on " + me;
				}
				if(!area_map[tag])
					return "Unrecognized area tag: " + tag.toUpperCase();
				var i = area_map[tag].indexOf(you);
				if(i < 0)
					return you + " is not connected to area: " + tag.toUpperCase();
				area_map[tag].splice(i, 1);
				return you + " is now disconnected from area: " + tag.toUpperCase();
			}
			if(tag[0] == '+')
				tag = tag.substring(1);
			if(tag == "all") {
				for(var a in area_map) {
					if(area_map[a].indexOf(you) < 0)
						area_map[a].push(you);
				}
				return you + " is now connected to all available file areas on " + me;
			}
			if(area_map[tag]) { // Add
				if(area_map[tag].indexOf(you) >= 0)
					return you + " is already connected to area: " + tag.toUpperCase();
				area_map[tag].push(you);
				return you + " is now connected to area: " + tag.toUpperCase();
			}
			return "!Unrecognized area-tag or command on " + me + ": '" + cmd + "'";
	}
}

var tickit = new File(file_cfgname(system.ctrl_dir, "tickit.ini"));
if(!tickit.open(tickit.exists ? 'r+':'w+'))
	js.report_error(tickit.error + " opening " + tickit.name, /* fatal */true);
var area_list = tickit.iniGetSections();
var area_map = {};
var area_desc = {};
for(var i = 0; i < area_list.length; i++) {
	var tag = area_list[i].toLowerCase();
	area_map[tag] = tickit.iniGetValue(tag, "links", []);
	var dir_code = tickit.iniGetValue(tag, "dir");
	if(dir_code && file_area.dir[dir_code])
		area_desc[tag] = file_area.dir[dir_code].description;
}
var orig_map = JSON.stringify(area_map);

var f = new File(file_cfgname(system.ctrl_dir, "sbbsecho.ini"));
if(!f.open("r"))
	js.report_error(f.error + " opening " + f.name, /* fatal */true);

var node_map = {};
var node_list = f.iniGetAllObjects("addr", "node:", /* lowercase: */true);
for(var i = 0 ; i < node_list.length; i++) {
	// Normalize node address (e.g. strip .0 and @domain) here
	node_map[fidoaddr.to_str(fidoaddr.parse(node_list[i].addr))] = node_list[i];
}
f.close();

var msgbase = new MsgBase("mail");
if(!msgbase.open())
	js.report_error(msgbase.last_error + " opening " + msgbase.file, /* fatal */true);

var idx_list = msgbase.get_index();
for(var i = 0; i < idx_list.length; i++) {
	var idx = idx_list[i];
	if((idx.attr&MSG_DELETE) || idx.to != 0)
		continue;
	var hdr = msgbase.get_msg_header(true, i, /* expand_fields: */false);
	if(AreaMgr.indexOf[hdr.to.toLowerCase()] < 0)
		continue;
	if(hdr.from_net_type != NET_FIDO || hdr.to_net_type != NET_FIDO)
		continue;
	if(system.fido_addr_list.indexOf(hdr.to_net_addr) < 0)
		continue;
	if(!node_map[hdr.from_net_addr]) {
		reply_to_msg(hdr, "Unknown node address: " + hdr.from_net_addr);
		continue;
	}
	if(!node_map[hdr.from_net_addr].areafix) {
		reply_to_msg(hdr, "AreaFix not enabled for node: " + hdr.from_net_addr);
		continue;
	}
	if(!node_map[hdr.from_net_addr].areafixpwd) {
		reply_to_msg(hdr, "No AreaFix password for node: " + hdr.from_net_addr);
		continue;
	}
	if(hdr.subject.toLowerCase() != String(node_map[hdr.from_net_addr].areafixpwd).toLowerCase()) {
		reply_to_msg(hdr, "Wrong AreaFix password for node: " + hdr.from_net_addr);
		continue;
	}
	log("Successful authentication of: " + hdr.from_net_addr);
	var text = msgbase.get_msg_body(true, i
		, /* strip ctrl-a */true
		, /* dot-stuffing */false
		, /* include-tails */false);
	if(!text) {
		reply_to_msg(hdr, "No body text for Area Management Request from node: " + hdr.from_net_addr);
		continue;
	}
	var response = [];
	var line = text.split('\r\n');
	for(var l = 0; l < line.length; l++) {
		if(line[l])
			response.push(handle_command(hdr, line[l]));
	}
	reply_to_msg(hdr, response.join('\r\n'));
}
msgbase.close();

if(JSON.stringify(area_map) != orig_map) {
	log(LOG_INFO, tickit.name + " modified");
	file_backup(tickit.name, backup_level);
	for(var i = 0; i < area_list.length; i++) {
		var tag = area_list[i].toLowerCase();
		tickit.iniSetValue(tag, "links", area_map[tag]);
	}
}
tickit.close();
