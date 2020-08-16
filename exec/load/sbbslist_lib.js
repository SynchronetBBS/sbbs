// $Id: sbbslist_lib.js,v 1.24 2020/04/22 00:54:58 rswindell Exp $
// @format.tab-size 4

// Synchronet BBS List (SBL) v4 Library

load("synchronet-json.js"); // For compatibility with older verisons of Synchronet/JavaScript-C without the JSON object
load("graphic.js");
load("lz-string.js");

var list_fname = system.data_dir + 'sbbslist.json';

var sort_property = 'name';

// These max lengths are derived from the bbs_t structure definition in xtrn/sbl/sbldefs.h:
const max_len = {
	name:				25,		/* Synchronet allows 40, I think this restricted by 25-char QWK msg subjs in sbldefs.h */
	phone_number:		25,		/* only the first 12 chars are backwards compatible with SBL v3 */
	location:			30,
	sysop_name:			25,
	sysops:				30,
	network_name:		15,
	network_address:	25,
	created_by:			25,
	updated_by:			25,
	verified_by:		25,
	created_on:			28,
	updated_on:			28,
	verified_on:		28,
	service_address:	35,		// Increased from 28
	service_description:15,
	software:			15,
	since:				4,		/* just the year portion of the first_online date */
	nodes:				4,
	users:				5,
	subs:				5,
	dirs:				5,
	doors:				5,		// was xtrns
	msgs:				8,
	files:				7,
	storage:			7,
	min_rate:			5,
	max_rate:			5,
	port:				5,
	protocol:			15,
	protocols:			60,
	tcp_services:		33,
	udp_services:		20,
	web_site:			40,		// Decreased from 60
	email_addr:			40,		// Decreased from 60
	networks:			60,
	description:		250,
};

// Some of these max values are hard-technical limits (e.g. port), some are legacy
// SBL limits (e.g. 16-bit min/max_rate) and some are just limited for cosmetic reasons
const max_val = {
	port:				65535,
	nodes:				255,
	users:				9999,
	subs:				99999,
	dirs:				99999,
	doors:				99999,
	msgs:				99999999,
	files:				9999999,
	min_rate:			65535,
	max_rate:			65535,
};

// Services supported by BBS clients (e.g. SyncTERM)
const common_bbs_services = [
    "telnet",
    "rlogin",
	"ssh",
	"modem"
];

function compare(a, b)
{
	var val1="";
	var val2="";

	val1 = property_sort_value(a, sort_property);
	val2 = property_sort_value(b, sort_property);

	if(typeof(val1) == "string")
		val1=val1.toLowerCase();
	if(typeof(val2) == "string")
		val2=val2.toLowerCase();
	else { /* Sort numbers backwards on purpose */
		var tmp = val1;
		val1 = val2;
		val2 = tmp;
	}
	if(val1>val2) return 1;
	if(val1<val2) return -1;
	return 0;
}

function verify_compare(a, b)
{
    var diff;

    if(a.entry.verified && a.entry.verified.on && b.entry.verified && b.entry.verified.on)
        diff = new Date(b.entry.verified.on.substr(0,10)).getTime() - new Date(a.entry.verified.on.substr(0,10)).getTime();

    if(diff) return diff;

    if(a.entry.autoverify !=undefined && b.entry.autoverify != undefined) {
        diff = b.entry.autoverify.successes - a.entry.autoverify.successes;

        if(diff) return diff;

        diff = a.entry.autoverify.attempts - b.entry.autoverify.attempts;
    }

    return diff;
}

function active_compare(a, b)
{
	return last_active(b) - last_active(a);
}

function date_string(date)
{
	return new Date(date).toString().substr(4,11);
}

/* Some properties are nested within arrays of objects, lets simplify those properties here */
function property_value(bbs, property)
{
	var value="";

	switch(property) {
		case "service_address":
			if(bbs.service) {
				for(var i in bbs.service) {
					if(bbs.service[i].protocol != 'modem') {
						value = bbs.service[i].address;
						break;
					}
				}
			}
			break;
		case "phone_number":
			if(bbs.service) {
				for(var i in bbs.service) {
					if(bbs.service[i].protocol == 'modem') {
						value = bbs.service[i].address;
						break;
					}
				}
			}
			break;
		case "sysops":
			for(var i in bbs.sysop) {
				if(value.length) value += ", ";
				value += bbs.sysop[i].name;
			}
			break;
		case "created_by":
			value = bbs.entry.created.by;
			break;
		case "created_on":
			value = date_string(bbs.entry.created.on);
			break;
		case "updated_by":
			if(bbs.entry.updated)
				value = bbs.entry.updated.by;
			break;
		case "updated_on":
			if(bbs.entry.updated)
				value = date_string(bbs.entry.updated.on);
			break;
		case "verified_by":
			if(bbs.entry.verified)
				value = bbs.entry.verified.by;
			break;
		case "verified_on":
			if(bbs.entry.verified)
				value = date_string(bbs.entry.verified.on);
			break;
		case "since":
			if(bbs.first_online)
				value = bbs.first_online.substring(0,4);
			break;
		case "nodes":
			if(bbs.terminal && bbs.terminal.nodes !== undefined)
				value = bbs.terminal.nodes;
			break;
		case "users":
			if(bbs.total && bbs.total.users !== undefined)
				value = bbs.total.users;
			break;
		case "subs":
			if(bbs.total && bbs.total.subs !== undefined)
				value = bbs.total.subs;
			break;
		case "dirs":
			if(bbs.total && bbs.total.dirs !== undefined)
				value = bbs.total.dirs;
			break;
		case "doors":
			if(bbs.total && bbs.total.doors !== undefined)
				value = bbs.total.doors;
			break;
		case "msgs":
			if(bbs.total && bbs.total.msgs !== undefined) {
				value = bbs.total.msgs;
				if(value >= 1000000)
					value = Math.ceil(value / 1000000) + "M";
			}
			break;
		case "files":
			if(bbs.total && bbs.total.files !== undefined) {
				value = bbs.total.files;
				if(value >= 1000000)
					value = Math.ceil(value / 1000000) + "M";
			}
			break;
		case "storage":
			if(bbs.total && bbs.total.storage && !isNaN(bbs.total.storage)) {
				if(bbs.total.storage > 1024*1024*1024*1024)
					value = Math.ceil(bbs.total.storage / (1024*1024*1024*1024)) + "TB";
				else if(bbs.total.storage > 1024*1024*1024)
					value = Math.ceil(bbs.total.storage / (1024*1024*1024)) + "GB";
				else if(bbs.total.storage > 1024*1024)
					value = Math.ceil(bbs.total.storage / (1024*1024)) + "MB";
				else
					value = Math.ceil(bbs.total.storage / 1024) + "KB";
			}
			break;				
		case "description":
			value = bbs.description.join(" ");
			break;
		case "networks":
			if(bbs.network && bbs.network.length) {
				var networks = [];
				for(var i in bbs.network) {
					if(networks.indexOf(bbs.network[i].name) < 0)
						networks.push(bbs.network[i].name);
				}
				for(var i in networks) {
					if(i > 0) value += ", ";
					value += networks[i];
				}
			}
			break;
		case "protocols":
		{
			var protocols = [];
			if(bbs.service && bbs.service.length) {
				for(var i in bbs.service) {
					if(bbs.service[i].protocol == 'modem')
						continue;
					if(protocols.indexOf(bbs.service[i].protocol) < 0)
						protocols.push(bbs.service[i].protocol);
				}
			}
			if(bbs.entry 
				&& bbs.entry.autoverify 
				&& bbs.entry.autoverify.last_success
				&& bbs.entry.autoverify.last_success.other_services) {
				protocols = protocols.concat(bbs.entry.autoverify.last_success.other_services.tcp);
			}
			value = protocols.join();
			break;
		}
		case "tcp_services":
			if(bbs.entry 
				&& bbs.entry.autoverify 
				&& bbs.entry.autoverify.last_success
				&& bbs.entry.autoverify.last_success.other_services) {
				value = bbs.entry.autoverify.last_success.other_services.tcp;
			}
			break;
		case "udp_services":
			if(bbs.entry 
				&& bbs.entry.autoverify 
				&& bbs.entry.autoverify.last_success
				&& bbs.entry.autoverify.last_success.other_services) {
				value = bbs.entry.autoverify.last_success.other_services.udp;
			}
			break;
		default:
			if(bbs[property])
				value = bbs[property];
			break;
	}
	return value;
}

function numeric_sort_value(value)
{
	value = parseInt(value, 10);
	if(isNaN(value))
		return -1;
	return value;
}

/* Some properties are nested within arrays of objects, lets simplify those properties here */
function property_sort_value(bbs, property)
{
	var value="";

	switch(property) {
		case "name":
			if(bbs.name.toLowerCase().slice(0, 4) == "the ")
				return bbs.name.slice(4);
			return bbs.name;
		case "storage":
			if(bbs.total)
				return numeric_sort_value(bbs.total.storage);
			break;
		case "files":
			if(bbs.total)
				return numeric_sort_value(bbs.total.files);
			brak;
		case "msgs":
			if(bbs.total)
				return numeric_sort_value(bbs.total.msgs);
			break;
		case "dirs":
			if(bbs.total)
				return numeric_sort_value(bbs.total.dirs);
			break;
		case "subs":
			if(bbs.total)
				return numeric_sort_value(bbs.total.subs);
			break;
		case "users":
			if(bbs.total)
				return numeric_sort_value(bbs.total.users);
			break;
		case "doors":
			if(bbs.total)
				return numeric_sort_value(bbs.total.doors);
			break;
		case "nodes":
			if(bbs.terminal)
				return numeric_sort_value(bbs.terminal.nodes);
			break;
		case "created_on":
			return new Date(bbs.entry.created.on).valueOf();
		case "updated_on":
			if(bbs.entry.updated)
				return new Date(bbs.entry.updated.on).valueOf();
			break;
		case "verified_on":
			if(bbs.entry.verified)
				return new Date(bbs.entry.verified.on).valueOf();
			break;
	}
	return property_value(bbs, property);
}

function sort(list, property)
{
	if(property=="verify")
		list.sort(verify_compare);
	else if(property=="active")
		list.sort(active_compare);
	else {	
		if(property)
			sort_property=property;
		list.sort(compare);
	}
}

function find(list, text)
{
	var new_list=[];
	var text=text.toUpperCase();

	for(var i in list)
		if(JSON.stringify(list[i]).toUpperCase().indexOf(text) >= 0)
			new_list.push(list[i]);
	return new_list;
}

function find_value(list, prop, value)
{
	for(var i = 0; i < list.length; i++) {
		if(list[i][prop] == value)
			return i;
	}
	return -1;
}

// Parse the list from the JSON database file
function parse(f)
{
    var buf = f.read();
	truncsp(buf);
    var list = [];
	if(buf.length) {
		try {
			list=JSON.parse(buf);
		} catch(e) {
			log(LOG_ERR, "SBBSLIST: JSON.parse exception: " + e);
		}
	}
	return list;
}

function lock_fname()
{
	return list_fname + '.lock';
}

function lock(op, timeout /* in seconds */, max)
{
	var start = time();
	if(timeout === undefined)
		timeout = 60;
	if(max === undefined)
		max = 24 * 60 * 60;

	while(!file_mutex(lock_fname(), op, max)) {
		if(time() - start >= timeout) {
        		log(LOG_ERR, "SBBSLIST: Timeout locking " + list_fname + " for " + op);
			return false;
		}
		sleep(1000);
	}
	return true;
}

function unlock()
{
	return file_remove(lock_fname());
}

function read_list()
{
	if(!lock("read"))
		return false;
    var f = new File(list_fname);
    log(LOG_DEBUG, "Opening list file: " + f.name);
    if(!f.open("r")) {
        log(LOG_ERR, "SBBSLIST: Error " + f.error + " opening " + f.name);
		unlock();
        return [];
    }
	var list = parse(f);
    f.close();
	unlock();
    return list;
}

function write_list(list)
{
	if(!lock("write"))
		return false;
    var out = new File(list_fname);
    log(LOG_DEBUG, "SBBSLIST: Opening / creating list file: " + list_fname);
    if(!out.open("w+")) {
        log(LOG_ERR, "SBBSLIST: Error " + out.error + " creating " + out.name);
		unlock();
        return false;
    }
    log(LOG_DEBUG, "SBBSLIST: Writing list file: " + out.name + " (" + list.length + " BBS entries)");
    out.write(JSON.stringify(list, null, 4));
    out.close();
	unlock();
    return true;
}

function append(bbs)
{
	if(!lock("append"))
		return false;
    var f = new File(list_fname);
    log(LOG_DEBUG, "SBBSLIST: Opening / creating list file: " + list_fname);
    if(!f.open(f.exists ? 'r+':'w+')) {
        log(LOG_ERR, "SBBSLIST: Error " + f.error + " creating " + f.name);
		unlock();
        return false;
    }
	var list = parse(f);
	list.push(bbs);
    log(LOG_DEBUG, "SBBSLIST: Writing list file: " + f.name + " (" + list.length + " BBS entries)");
	f.truncate();
    f.write(JSON.stringify(list, null, 4));
    f.close();
	unlock();
    return true;
}

function remove(bbs)
{
	if(!lock("remove"))
		return false;
    var f = new File(list_fname);
    log(LOG_INFO, "SBBSLIST: Opening / creating list file: " + list_fname);
    if(!f.open('r+')) {
        log(LOG_ERR, "SBBSLIST: Error " + f.error + " opening " + f.name);
		unlock();
        return false;
    }
    var list = parse(f);
	var index = find_value(list, "name", bbs.name); 
	if(index < 0 ) {
		f.close();
		unlock();
		return false;
	}
	list.splice(index, 1);
    log(LOG_INFO, "SBBSLIST: Writing list file: " + f.name + " (" + list.length + " BBS entries)");
	f.truncate();
    f.write(JSON.stringify(list, null, 4));
    f.close();
	unlock();
    return true;
}

function replace(bbs)
{
	if(!lock("replace"))
		return false;
    var f = new File(list_fname);
    log(LOG_INFO, "SBBSLIST: Opening / creating list file: " + list_fname);
    if(!f.open('r+')) {
        log(LOG_ERR, "SBBSLIST: Error " + f.error + " opening " + f.name);
		unlock();
        return false;
    }
    var list = parse(f);
	var index = find_value(list, "name", bbs.name); 
	if(index < 0 ) {
		f.close();
		unlock();
		return false;
	}
	list[index] = bbs;
    log(LOG_INFO, "SBBSLIST: Writing list file: " + f.name + " (" + list.length + " BBS entries)");
	f.truncate();
    f.write(JSON.stringify(list, null, 4));
    f.close();
	unlock();
    return true;
}

function remove_dupes(list, verbose)
{
    var new_list=[];
    var names=[];
    var address=[];
    var i;

    for(i in list) {
		var bbs = list[i];
		var name = bbs.name.toLowerCase();
		if(name.slice(-4) == " bbs")
			name = name.slice(0, -4);
		if(name.slice(0, 4) == "the ")
			name = name.slice(4);
		if(names.indexOf(name) >= 0)
			continue;
		if(bbs.service.length
			&& address.indexOf(bbs.service[0].address.toLowerCase()) >= 0) {
			if(verbose)
				print(format("%-25s : Duplicated name", bbs.name));
			continue;
		}
		names.push(name);
		address.push(bbs.service[0].address.toLowerCase());
		new_list.push(bbs);
	}
    return new_list;
}

function last_active(bbs)
{
	var active = 0;
	if(bbs.entry.created)
		active = new Date(bbs.entry.created.on);
	var updated = 0;
	if(bbs.entry.updated)
		updated = new Date(bbs.entry.updated.on);
	if(updated > active)
		active = updated;
	var verified = 0;
	if(bbs.entry.verified)
		verified = new Date(bbs.entry.verified.on);
	if(verified > active)
		active = verified;
	return active;
}

function remove_inactive(list, max, verbose)
{
    var new_list=[];
    var i;

    for(i in list) {
		var bbs = list[i];
		var active = last_active(bbs);
		var diff = new Date().valueOf() - active.valueOf();
		var days = diff / (24 * 60 * 60 * 1000);
		if(days > max) {
			if(verbose)
				print(format("%-25s : Inactive since %.10s (%d days)"
					, bbs.name, active.toISOString(), days));
			continue;
		}
		new_list.push(list[i]);
	}
	return new_list;
}

function imsg_capable_system(bbs)
{
	if(bbs.imsg_capable == true)
		return true;
	if(!bbs.entry.autoverify || !bbs.entry.autoverify.last_success)
		return false;
	var services = bbs.entry.autoverify.last_success.other_services;
	if(services.udp.indexOf("systat")<0)
		return false;
	if(services.tcp.indexOf("msp")<0)
		return false;
	return true;
}

function system_index(list, name)
{
    name = name.toLowerCase();
    for(var i=0; i < list.length; i++)
        if(list[i].name.toLowerCase() == name)
            return i;

    return -1;
}


function system_exists(list, name)
{
    return system_index(list, name) >= 0;
}

function new_system(name, nodes, stats)
{
	if(!stats)
		stats = { users:1, doors:0, dirs:0, subs:0, files:0, msgs:0, storage:0 };
	return {
		name: name, 
		entry:{ created:{}, updated:{} }, 
		sysop:[], 
		service:[], 
		terminal:{ nodes: nodes, types:[] }, 
		network:[], 
		description:[],
		total: stats
	};
}

function check_entry(bbs)
{
	if(!bbs.name || !bbs.name.length || bbs.name.length > max_len.name) {
		log("Problem with BBS name: " + bbs.name);
		return false;
	}
	if(!bbs.service || !bbs.service.length) {
		log(bbs.name + " has no services");
		return false;
	}
/** this is valid in SBL
	if(!bbs.sysop || !bbs.sysop.length || !bbs.sysop[0].name || !bbs.sysop[0].name.length) {
		log(bbs.name + " has no sysop");
		return false;
	}
**/
	if(!bbs.entry || !bbs.entry.created || !bbs.entry.created.by) {
		log(bbs.name + " has no entry.created.by property");
		return false;
	}
	return true;	// All is good
}

function system_stats()
{
	return {
		users: system.stats.total_users, 
		subs: Object.keys(msg_area.sub).length, 
		dirs: Object.keys(file_area.dir).length, 
		doors: Object.keys(xtrn_area.prog).length, 
		storage:disk_size(system.data_dir, 1024*1024)*1024*1024, 
		msgs: system.stats.total_messages, 
		files: system.stats.total_files
	};
}

function add_system(list, bbs, by)
{
	bbs.entry.created = { on: new Date().toISOString(), by: by };
	if(!bbs.first_online)
		bbs.first_online = new Date().toISOString();
	list.push(bbs);
}

function update_system(bbs, by)
{
	var update_count = 0;
	if(bbs.entry.updated)
		update_count = Number(bbs.entry.updated.count);
	bbs.entry.updated = { on: new Date().toISOString(), by: by, count: update_count + 1 };
}

function verify_system(bbs, by)
{
	var verify_count = 0;
	if(bbs.entry.verified)
		verify_count = Number(bbs.entry.verified.count);
	bbs.entry.verified = { on: new Date().toISOString(), by: by, count: verify_count + 1 };
}

function syncterm_list(list, dir)
{
    var f = new File(dir + "syncterm.lst");
    if(!f.open("w")) {
		return false;
    }
	f.writeln(format("; Exported from %s on %s", system.name, new Date().toString()));
	f.writeln();
	var sections = [];
    for(i in list) {
        for(j in list[i].service) {
            if(!list[i].service[j].protocol)
                continue;
			if(common_bbs_services.indexOf(list[i].service[j].protocol.toLowerCase()) < 0)
				continue;
			var section;
            if(j > 0)
                section = format("%-23.23s %6.6s", list[i].name, list[i].service[j].protocol);
            else
                section = list[i].name;
			if(sections.indexOf(section) >= 0) {	// duplicate
				if(list[i].service[j].description)
					section = format("%-20.20s%10.10s", list[i].name, list[i].service[j].description);
				else if(list[i].service[j].port)
					section = format("%-24.24s %5u", list[i].name, list[i].service[j].port);
				if(sections.indexOf(section) >= 0)	// duplicate
					continue;
			}
			sections.push(section);
			sections.push(format("%-23.23s %6.6s", list[i].name, list[i].service[j].protocol));
			f.writeln(format("[%s]", section));
            f.writeln(format("\tConnectionType=%s", list[i].service[j].protocol));
            f.writeln(format("\tAddress=%s", list[i].service[j].address));
			if(list[i].service[j].port)
				f.writeln(format("\tPort=%s", list[i].service[j].port));	
            f.writeln();
        }
    }
    f.close();
	return f.name;
}

function share_list(list, qwk)
{
	var new_list = [];
	for(i in list) {
		var bbs = list[i];
		delete bbs.entry.autoverify;
		delete bbs.entry.exported;
		if(qwk) {
			if(bbs.entry.created.at && isNaN(parseInt(bbs.entry.created.at, 10)))
				bbs.entry.created.at = system.qwk_id + "/" + bbs.entry.created.at;
			else
				bbs.entry.created.at = system.qwk_id;
			if(bbs.entry.updated.at && isNaN(parseInt(bbs.entry.updated.at, 10)))
				bbs.entry.updated.at = system.qwk_id + "/" + bbs.entry.updated.at;
			else
				bbs.entry.updated.at = system.qwk_id;
		} else {
			if(isNaN(parseInt(bbs.entry.created.at, 10)))
				bbs.entry.created.at = system.fido_addr_list[0];
			if(isNaN(parseInt(bbs.entry.updated.at, 10)))
				bbs.entry.updated.at = system.fido_addr_list[0];
		}
		bbs.imported = true;
		new_list.push(bbs);
	}
	return new_list;
}

const base64_max_line_len = 72;

function compress_preview(bin)
{
	var compressed = LZString.compressToBase64(bin);
	return ["!LZ"].concat(compressed.match(new RegExp('([\x00-\xff]{1,' + base64_max_line_len + '})', 'g')));
}

function encode_preview(ansi_capture)
{
	var graphic = new Graphic();
	graphic.ANSI = ansi_capture.join("\r\n");
	return compress_preview(graphic.BIN);
}

function decode_preview(preview)
{
	if(preview[0] == "!LZ")
		return LZString.decompressFromBase64(preview.slice(1).join(""));
	else
		return base64_decode(preview.join(""));
}

function draw_preview(bbs)
{
	if(!bbs.preview)
		return false;
	var graphic = new Graphic();
	var bin = decode_preview(bbs.preview);
	if(!bin || !bin.length)
		return false;
	graphic.BIN = bin;
	return graphic.draw();
}

/* Leave as last line for convenient load() usage: */
this;
