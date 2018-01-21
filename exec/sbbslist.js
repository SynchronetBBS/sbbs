// $Id$

// Synchronet BBS List

// This one script replaces (or *will* replace) the functionality of:
// sbl[.exe]        - External online program (door)
// smb2sbl[.exe]    - Imports BBS entries from Synchronet Message Base (e.g. from SYNCDATA echo)
// sbl2smb[.exe]    - Exports BBS entries to Synchronet Message Base (e.g. to SYNCDATA echo)
// sbbslist[.exe]   - Exports BBS entries to HTML and various plain-text formats (e.g. sbbs.lst, sbbsimsg.lst, syncterm.lst)

// TODO: Daily maintenance, warning local creators and purging old unverified entries

var REVISION = "$Revision$".split(' ')[1];
var version_notice = "Synchronet BBS List v4(" + REVISION + ")";

load("sbbsdefs.js");
load("sockdefs.js");
load("portdefs.js");
load("hexdump_lib.js");

var sbl_dir = "../xtrn/sbl/";
var list_format = 0;
var export_freq = 7;	// minimum days between exports
var color_cfg = {
	header: BLACK | BG_LIGHTGRAY,
	selection: BG_BLUE,
	column: [ WHITE ],
	sorted: BG_RED,
};
var cmd_prompt_fmt = "\1n\1c\xfe \1h%s \1n\1c\xfe ";
if(js.global.console==undefined || !console.term_supports(USER_ANSI))
	cmd_prompt_fmt = "%s: ";

var debug = false;

var options=load({}, "modopts.js", "sbbslist");
if(!options)
	options = {};
if(!options.sub)
    options.sub = load({}, "syncdata.js").find();
if(options && options.format > 0)
	list_format = options.format;
if(options && options.export_freq > 0)
	export_freq = options.export_freq;

var lib = load({}, "sbbslist_lib.js");
var capture = load({}, "termcapture_lib.js");
capture.timeout=15;
capture.poll_timeout=10;
var avatar_lib = load({}, "avatar_lib.js");

function objcopy(obj)
{
	return JSON.parse(JSON.stringify(obj));
}

// This date format is required for backwards compatibility with SMB2SBL
function date_to_str(date)
{
    return format("%02u/%02u/%02u", date.getUTCMonth()+1, date.getUTCDate(), date.getUTCFullYear()%100);
}

function date_from_str(str)
{
	var a = str.split("/");
	var month = parseInt(a[0]);
	var day = parseInt(a[1]);
	var year = parseInt(a[2]);
	year += 1900;
	if(year < 1970)
		year += 100;
    return new Date(year, month - 1, day);
}

function export_entry(bbs, msgbase)
{
    var i;
    var hdr = { to:'SBL', from:bbs.entry.created.by, subject:bbs.name };

    var body = "";      // This section for SMB2SBL compatibility
    body += "Name:          " + bbs.name + "\r\n";
	if(bbs.first_online)
		body += "Birth:         " + date_to_str(new Date(bbs.first_online)) + "\r\n";
    if(bbs.software)
        body += "Software:      " + bbs.software + "\r\n";
    for(i in bbs.sysop) {
        body += "Sysop:         " + bbs.sysop[i].name + "\r\n";
        if(i==0 && bbs.sysop[i].email)
            body += "e-mail:        " + bbs.sysop[i].email + "\r\n";
    }
    if(bbs.web_site)
        body += "Web-site:      " + bbs.web_site + "\r\n";
    if(bbs.location)
        body += "Location:      " + bbs.location + "\r\n";
    for(i in bbs.service) {
        switch(bbs.service[i].protocol.toLowerCase()) {
            case 'modem':
                body += "Number:        " + bbs.service[i].address + "\r\n";
                body += "MinRate:       " + bbs.service[i].min_rate + "\r\n";
                body += "MaxRate:       " + bbs.service[i].max_rate + "\r\n";
                if(bbs.service[i].description)
                    body += "Modem:         " + bbs.service[i].description + "\r\n";
                break;
            case 'telnet':
                body += "Telnet:        " + bbs.service[i].address + "\r\n";
				if(bbs.service[i].port 
					&& bbs.service[i].port != standard_service_port["telnet"])
					body += "Port:          " + bbs.service[i].port + "\r\n";
                break;
         }
    }
    for(i in bbs.network) {
        body += "Network:       " + bbs.network[i].name + "\r\n";
        body += "Address:       " + bbs.network[i].address + "\r\n";
    }
    for(i in bbs.terminal.types)
        body += "Terminal:      " + bbs.terminal.types[i] + "\r\n";
    if(bbs.terminal.nodes)
        body += "Nodes:         " + bbs.terminal.nodes + "\r\n";
    if(bbs.total.storage)
        body += "Megs:          " + bbs.total.storage / (1024*1024) + "\r\n";
    if(bbs.total.msgs)
        body += "Msgs:          " + bbs.total.msgs + "\r\n";
    if(bbs.total.files)
        body += "Files:         " + bbs.total.files + "\r\n";
    if(bbs.total.users)
        body += "Users:         " + bbs.total.users + "\r\n";
    if(bbs.total.subs)
        body += "Subs:          " + bbs.total.subs + "\r\n";
    if(bbs.total.dirs)
        body += "Dirs:          " + bbs.total.dirs + "\r\n";
    if(bbs.total.doors)
        body += "Xtrns:         " + bbs.total.doors + "\r\n";
    for(i in bbs.description)
        body += "Desc:          " + bbs.description[i] + "\r\n";

	delete bbs.entry;
    body += "\r\njson-begin\r\n";
    body += lfexpand(JSON.stringify(bbs, null, 1)) + "\r\n";
    body += "json-end\r\n";
    body += "--- " + js.exec_file + " " + REVISION + "\r\n";
//    print(body);

    return msgbase.save_msg(hdr, body);
}

function export_to_msgbase(list, msgbase, limit, all)
{
    var i;
    var count=0;
	var errors=0;

    for(i in list) {
        if(js.terminated)
            break;
		if(all != true) {
			if(list[i].imported)
				continue;
			var last_export = 0;
			if(list[i].entry.exported)
				last_export = new Date(list[i].entry.exported.on).valueOf();
			var created = 0;
			if(list[i].entry.created)
				created = new Date(list[i].entry.created.on).valueOf();
			var updated = 0;
			if(list[i].entry.updated)
				updated = new Date(list[i].entry.updated.on).valueOf();
			var verified = 0;
			if(list[i].entry.verified)
				verified = new Date(list[i].entry.verified.on).valueOf();
			if(created <= last_export
				&& updated <= last_export
				&& verified <= last_export)
				continue;
			if((new Date().valueOf() - last_export) < export_freq * (24*60*60*1000))
				continue;
		}
        if(!export_entry(objcopy(list[i]), msgbase)) {
			alert("MsgBase error: " + msgbase.last_error);
			errors++;
		}
		else {
			if(!list[i].entry.exported)
				list[i].entry.exported = { on: null, by: null, to: null, count: 1 };
			else
				list[i].entry.exported.count++;
			list[i].entry.exported.on = new Date().toISOString();
			list[i].entry.exported.by = js.exec_file + " " + REVISION;
			list[i].entry.exported.to = file_getname(msgbase.file);
			count++;
		}
		if(limit && count >= limit)
			break;
    }
    print("Exported " + count + " entries (" + errors + " errors)");
	if(count)
		lib.write_list(list);
}

function import_entry(name, text)
{
    var i;
    var json_begin;
    var json_end;
	var bbs = lib.new_system(name, /* nodes: */0);

    text=text.split("\r\n");
    for(i=0; i<text.length; i++) {
        if(text[i]=="---" || text[i].substring(0,4)=="--- ")
            break;
    }
    text.length=i;

    for(i=0; i<text.length; i++) {
        if(text[i].toLowerCase()=="json-begin")
            json_begin=i+1;
        else if(text[i].toLowerCase()=="json-end")
            json_end=i;
    }
    if(json_begin && json_end > json_begin) {
        text=text.splice(json_begin, json_end-json_begin);

        try {
            if((bbs = JSON.parse(text.join(' '))) != undefined)
                return bbs;
        } catch(e) {
            alert("Error " + e + " parsing JSON");
        }
        return bbs;
    }

    /* Parse the old SBL2SMB syntax: */

    var sysop=0;
    var network=0;
    var terminal=0;
    var desc=0;
    var number=0;

    for(i in text) {
        //print(text[i]);
		var line = truncsp(text[i]);
        if(!line.length)
            continue;
        var match=line.match(/\s*([A-Z\-]+)\:\s*(.*)/i);
        if(!match || match.length < 3) {
			print("No match: " + line);
            continue;
        }
        if(debug) print(match[1] + " = " + match[2]);
        switch(match[1].toLowerCase()) {
            case 'birth':
		if(match[2] && match[2].length)
                	bbs.first_online = date_from_str(match[2]);
                break;
            case 'software':
                bbs.software = match[2];
                break;
            case 'sysop':
                if(bbs.sysop.length)
                    sysop++;
                bbs.sysop[sysop] = { name: match[2] };
                break;
            case 'e-mail':
                if(bbs.sysop.length)
                    bbs.sysop[0].email = match[2];
                break;
            case 'web-site':
                bbs.web_site = match[2];
                break;
            case 'number':
                if(bbs.service.length)
                    number++;
                bbs.service[number] = {address: match[2], protocol: 'modem'};
                break;
            case 'telnet':  /* SBL2SMB never actually generated this line though SMB2SBL supported it */
                if(bbs.service.length)
                    number++;
                bbs.service[number] = {address: match[2], protocol: 'telnet', port: standard_service_port["telnet"]};
                break;
            case 'minrate':
                var minrate = parseInt(match[2], 10);
                if(minrate == 0xffff) {
					bbs.service[number].protocol = 'telnet';
					var uri=bbs.service[number].address.match(/^(\S+)\:\/\/(\S+)/);
					if(uri) {
						bbs.service[number].protocol = uri[1];
						bbs.service[number].address = uri[2];
					}
					bbs.service[number].port = standard_service_port[bbs.service[number].protocol.toLowerCase()];
                } else
                    bbs.service[number].minrate = minrate;
                break;
            case 'maxrate':
                if(bbs.service[number].protocol != 'modem')
                    bbs.service[number].port = parseInt(match[2], 10);
                else
                    bbs.service[number].maxrate = parseInt(match[2], 10);
                break;
            case 'location':
                bbs.location = match[2];
                break;
            case 'port':    /* SBL2SMB never actually generated this line though SMB2SBL supported it */
                bbs.service[number].port = parseInt(match[2], 10);
                break;
            case 'network':
                if(bbs.network.length)
                    network++;
                bbs.network[network] = {name: match[2]};
                break;
            case 'address':
                bbs.network[network].address = match[2];
                break;
            case 'terminal':
                if(bbs.terminal.types.length)
                    terminal++;
                bbs.terminal.types[terminal] = match[2];
                break;
            case 'desc':
                if(bbs.description.length)
                    desc++;
                bbs.description[desc] = match[2];
                break;
            case 'megs':
                bbs.total.storage = parseInt(match[2], 10)*1024*1024;
                break;
            case 'msgs':
            case 'files':
            case 'users':
            case 'subs':
            case 'dirs':
				bbs.total[match[1].toLowerCase()] = parseInt(match[2], 10);
                break;
            case 'xtrns':
                bbs.total.doors = parseInt(match[2], 10);
				break;
            case 'nodes':
                bbs.terminal.nodes = parseInt(match[2], 10);
                break;
        }
    }
	if(debug) print(JSON.stringify(bbs, null, 1));
    return bbs;
}

function import_from_msgbase(list, msgbase, import_ptr, limit, all)
{
    var i=0;
    var count=0;
    var highest;
    var sbl_crc=crc16_calc("sbl");

    var ini = new File(msgbase.file + ".ini");
	if(import_ptr == undefined) {
		if(ini.open("r")) {
			import_ptr=ini.iniGetValue("sbbslist","import_ptr", 0);
			ini.close();
		} else if(debug)
			print("Error " + ini.error + " opening " + ini.name);
		if(import_ptr==undefined) {
			var f = new File(file_getcase(msgbase.file + ".sbl"));
			if(f.open("rb")) {
				import_ptr = f.readBin(4);
				f.close();
			}
		}
	}
    highest=import_ptr;
    log(LOG_DEBUG, "import_ptr = " + import_ptr + " last_msg = " + msgbase.last_msg);
	if(msgbase.last_msg >= import_ptr)
		i = total_msgs - (msgbase.last_msg - import_ptr);
	var total_msgs = msgbase.total_msgs;
    for(; i<total_msgs; i++) {
        if(js.terminated)
            break;
        //print(i);
        var idx = msgbase.get_msg_index(/* by_offset: */true, i);
        if(!idx) {
//            print("Error " + msgbase.error + " reading index of msg offset " + i);
            continue;
        }
        if(idx.number <= import_ptr)
            continue;
        if(idx.number > highest)
            highest = idx.number;
        if(idx.to != sbl_crc)
            continue;
        var hdr = msgbase.get_msg_header(/* by_offset: */true, i);
		if(!hdr.to || hdr.to.toLowerCase() != "sbl")
			continue;
        var l;
        var msg_from = hdr.from;
		if(all != true && !hdr.from_net_type)	// Skip locally posted messages
			continue;
		if(hdr.from_net_addr && hdr.from_net_addr.length)
			msg_from += "@" + hdr.from_net_addr;
        var bbs_name = truncsp(hdr.subject);
		printf("Msg #%u from %s: ", i+1, msg_from);
//        print("Searching " + list.length + " entries for BBS: " + bbs_name);
        for(l=0; l<list.length; l++) {
            //print("Comparing " + list[l].name);
            if(list[l].name.toLowerCase() == bbs_name.toLowerCase())
                break;
        }
//        print("l = " + l);
        if(l==undefined)
            l=0;
		var entry = {};	// We preserve this object (and the BBS name) in the system's entry
        if(list.length && list[l]) {
            if(!list[l].entry)
                continue;
            if(!list[l].imported && hdr.from_net_type) {
                print(msg_from + " attempted to update/over-write local entry: " + bbs_name);
                continue;
            }
			entry = list[l].entry;
            if(entry.created.by.toLowerCase() != hdr.from.toLowerCase()
				|| (entry.created.at && entry.created.at != hdr.from_net_addr)) {
                print(msg_from  + " did not create entry: " 
					+ bbs_name + " (" + entry.created.by + "@" + entry.created.at + " did)");
                continue;
            }
            print("Updating existing entry: " + bbs_name + " (by " + entry.created.by + ")");
        } else {
            print(msg_from + " creating new entry: " + bbs_name);
            entry = { created: { on:new Date().toISOString(), by:hdr.from, at:hdr.from_net_addr } };
        }
        var body = msgbase.get_msg_body(/* by_offset: */true, i
            ,/* strip Ctrl-A */true, /* rfc822-encoded: */false, /* include tails: */false);
		var bbs = import_entry(bbs_name, body);
		if(bbs.name != bbs_name) {
			alert("Message body contained different BBS name (" + bbs.name + ") than subject: " + bbs_name);
			continue;
		}
		if(hdr.from_net_type)
			bbs.imported = true;
		bbs.entry = entry;
		bbs.entry.updated= { on: new Date().toISOString(), by:hdr.from, at:hdr.from_net_addr };
		if(!lib.check_entry(bbs))
			continue;
		list[l] = bbs;
//        if(!list[l].birth)
//            list[l].birth=list[l].entry.created.on;
        count++;
		if(limit && count >= limit)
			break;
    }

    if(ini.open(file_exists(ini.name) ? 'r+':'w+')) {
        ini.iniSetValue("sbbslist","import_ptr",highest);
        ini.close();
    } else
        print("Error opening/creating " + ini.name);
    print("Imported " + count + " messages");
    if(count)
        return lib.write_list(list);
}

// From sbldefs.h (Do not change, for backwards compatibility):
const sbl_defs = {
    MAX_SYSOPS:     5,
    MAX_NUMBERS:    20,
    MAX_NETS:       10,
    MAX_TERMS:      5,
    DESC_LINES:     5,
	DESC_LINE_LEN:	50,
};

// Reads a single BBS entry from SBL v3.x "sbl.dab" file
function read_dab_entry(f)
{
    // These sbl.dab magic numbers come from sbldefs.h (now deprecated)
    var i;
    var total;
    var obj = { name: '', entry:{}, sysop:[], service:[], terminal:{}, network:[], description:[], total:{} };

    obj.name = truncsp(f.read(26));
    if(f.eof)
        return null;
    obj.entry.created = { on:null, by:truncsp(f.read(26)) };
    obj.software = truncsp(f.read(16));
    total = f.readBin(1);
    for(i=0;i<sbl_defs.MAX_SYSOPS;i++)
        obj.sysop[i] = { name: truncsp(f.read(26)) };
    obj.sysop.length = total;
    total_numbers = f.readBin(1);
    total = f.readBin(1);
    for(i=0;i<sbl_defs.MAX_NETS;i++) {
        obj.network[i] = {};
        obj.network[i].name = truncsp(f.read(16));
    }
    for(i=0;i<sbl_defs.MAX_NETS;i++)
        obj.network[i].address = truncsp(f.read(26));
    obj.network.length = total;
    total = f.readBin(1);
    obj.terminal.types = [];
    for(i=0;i<sbl_defs.MAX_TERMS;i++)
        obj.terminal.types[i] = truncsp(f.read(16));
    obj.terminal.types.length = total;
    for(i=0;i<sbl_defs.DESC_LINES;i++)
        obj.description.push(truncsp(f.read(51)));
    for(i=0;i<sbl_defs.DESC_LINES;i++)
        if(obj.description[i].length == 0)
            break;
	obj.description.length=i;	// terminate description at first blank line
    obj.terminal.nodes = f.readBin(2);
    obj.total.users = f.readBin(2);
    obj.total.subs = f.readBin(2);
    obj.total.dirs = f.readBin(2);
    obj.total.doors = f.readBin(2);
    obj.entry.created.on = new Date(f.readBin(4)*1000).toISOString();
    var updated = f.readBin(4);
    if(updated)
        obj.entry.updated = { on: new Date(updated*1000).toISOString() };
    var first_online = f.readBin(4);
    if(first_online)
        obj.first_online = new Date(first_online*1000).toISOString();
    obj.total.storage = f.readBin(4)*1024*1024;
    obj.total.msgs = f.readBin(4);
    obj.total.files = f.readBin(4);
    obj.imported = Boolean(f.readBin(4));
    for(i=0;i<sbl_defs.MAX_NUMBERS;i++) {
        var service = { address: truncsp(f.read(13)), description: truncsp(f.read(16)), protocol: 'modem' };
        var location = truncsp(f.read(31));
        var min_rate = f.readBin(2)
        var port = f.readBin(2);
        if(min_rate==0xffff) {
			service.address = service.address + service.description;
			service.description = undefined;
			service.protocol = 'telnet';
			var uri=service.address.match(/^(\S+)\:\/\/(\S+)/);
			if(uri) {
				service.protocol = uri[1].toLowerCase();
				service.address = uri[2];
			}
            service.port = port;
        } else {
			// Only the first 12 chars of the modem "address" (number) are valid
			//service.address = service.address.substr(0, 12);
            service.min_rate = min_rate;
            service.max_rate = port;
        }
        if(obj.service.indexOf(service) < 0)
            obj.service.push(service);
        if(obj.location==undefined || obj.location.length==0)
            obj.location=location;
    }
    obj.service.length = total_numbers;
    var updated_by = truncsp(f.read(26));
    if(obj.entry.updated)
        obj.entry.updated.by = updated_by;
    var verified_date = f.readBin(4);
    var verified_by = f.read(26);
    if(verified_date)
        obj.entry.verified = { on: new Date(verified_date*1000).toISOString(), by: truncsp(verified_by), count: 1 };
    obj.web_site = truncsp(f.read(61));
    var sysop_email = truncsp(f.read(61));
    if(obj.sysop.length)
        obj.sysop[0].email = sysop_email;
    f.readBin(4);   // 'exported' not used, always zero
    obj.entry.autoverify = { successes: f.readBin(4), attempts: f.readBin(4) };
    f.read(310);    // unused padding

    return obj;
}

// Upgrades from SBL v3.x (native/binary-data) to v4.x (JavaScript/JSON-data)
function upgrade_list(sbl_dab)
{
    var dab = new File(sbl_dab);
    print("Upgrading from: " + sbl_dab);
    if(!dab.open("rb", /* shareable: */true)) {
        alert("Error " + dab.error + " opening " + dab.name);
        exit();
    }

    var list=[];

    while(!dab.eof) {
        if(js.terminated)
            break;
        var bbs = read_dab_entry(dab);
        if(bbs==null || !bbs.name.length)
            continue;
//        print(bbs.name);
        list.push(bbs);
    }
    dab.close();

    lib.write_list(list);

    return list;
}

function capture_preview(bbs, addr)
{
    for(var i in bbs.service) {
        if(js.terminated)
            break;
		var service = bbs.service[i];
        var protocol = service.protocol.toLowerCase();
        if(protocol != "telnet" && protocol != "rlogin")
            continue;
	    capture.protocol = protocol;
		if(addr)
			capture.address = addr;
		else
			capture.address = service.address;
	    capture.port = service.port;
		var result = capture.capture();
        if(result == false)
            return capture.error;
        bbs.preview = lib.encode_preview(result.preview);
        return true;
	}

    return false;
}

function verify_terminal_service(service)
{
    print("Verifying terminal service: " + service.protocol);
    capture.protocol = service.protocol.toLowerCase();
    capture.address = service.address;
    capture.port = service.port;

    return capture.capture();
}

// Perform a limited TCP port scan to test for common "other services" on standard ports
// Results are used for instant message list and other stuff
// Terminal services (e.g. telnet, rlogin, ssh) are purposely excluded
function verify_services(address, timeout)
{
    var i;
    var tcp_services=[
        "ftp",
        "msp",
        "nntp",
		"smtp",
//		"submission",
		"pop3",
		"imap",
		"irc",
    ];
    var udp_services=[
        "systat",
    ];

    var verified={ tcp: [], udp: []};

    var udp_socket = new Socket(SOCK_DGRAM);

    for(i in udp_services) {
        var service = udp_services[i];
        printf("Verifying %-10s UDP connection at %s\r\n", service, address);
        if(!udp_socket.sendto("\r\n", address, standard_service_port[service]))
            log(LOG_ERR,format("FAILED Send to %s UDP service at %s", service, address));
    }

    for(i in tcp_services) {
        if(js.terminated)
            break;
        var service = tcp_services[i]
        printf("Verifying %-10s TCP connection at %s ", service, address);
        var socket = new Socket();
        if(socket.connect(address, standard_service_port[service], timeout)) {
            print("Succeeded");
            verified.tcp.push(service);
            socket.close();
        } else
            print("Failed");
    }
    print("Waiting for UDP replies");
    while(verified.udp.length < udp_services.length && udp_socket.poll(1)) {
        if(js.terminated)
            break;
		var msg=udp_socket.recvfrom(32*1024);
        if(msg==null)
            log(LOG_ERR, "FAILED (UDP recv)");
        else {
            log(LOG_DEBUG, format("UDP message (%u bytes) from %s port %u", msg.data.length, msg.ip_address, msg.port));
            if(msg.ip_address != address)
                continue;
            for(i in udp_services) {
                var service = udp_services[i];
                if(standard_service_port[service] == msg.port) {
                    print("Valid UDP reply for " + service);
                    verified.udp.push(service);
                }
            }
        }
    }
    print(format("Successfully verified %u TCP services and %u UDP services", verified.tcp.length, verified.udp.length));
    return verified;
}

function verify_bbs(bbs)
{
    var i;
    var error="N/A";

	if(!bbs.entry.autoverify)
		bbs.entry.autoverify = {attempts:0, successes:0, last_failure: {result:'none'}};
    bbs.entry.autoverify.success=false;
    for(i in bbs.service) {
        if(js.terminated)
            break;
        var protocol = bbs.service[i].protocol.toLowerCase();
        if(protocol != "telnet" && protocol != "rlogin")
            continue;
        bbs.entry.autoverify.attempts++;
        var result = verify_terminal_service(bbs.service[i]);
        var failure = {
            on: new Date(),
            result: capture.error,
            service: bbs.service[i],
            ip_address: result.ip_address,
        };
        if(result == false) {
            print(capture.error);
            bbs.entry.autoverify.last_failure = failure;
        } else {
            print(result.stopcause);
            if(result.hello.length && result.hello[0].indexOf("Synchronet BBS") == 0) {
                bbs.entry.autoverify.success=true;
                bbs.entry.autoverify.successes++;
                bbs.entry.autoverify.last_success = {
                    on: new Date(),
                    result: result.hello[0],
                    stopcause: result.stopcause,
                    service: bbs.service[i],
                    ip_address: result.ip_address,
                    other_services: verify_services(result.ip_address, 5)
                };
                lib.verify_system(bbs, js.exec_file + " " + REVISION);
				bbs.preview = lib.encode_preview(result.preview);
                return true;
            }
            log(LOG_DEBUG,"Non-Synchronet identification: " + result.hello[0]);
            failure.result = "non-Synchronet";
            bbs.entry.autoverify.last_failure = failure;
        }
    }
    return false;
}

function verify_list(list)
{
    var i;

    js.auto_terminate=false;
    for(i in list) {
        var bbs=list[i];
        printf("Verifying BBS %u of %u: %s\n", Number(i)+1, list.length, bbs.name);
        if(verify_bbs(bbs))
            print("Success: " + bbs.entry.autoverify.last_success.result);
        else if(bbs.entry.autoverify.last_failure)
            print("Failure: " + bbs.entry.autoverify.last_failure.result);
		lib.replace(bbs);
        if(js.terminated)
            break;
    }
}

function unique_strings(a, offset)
{
    var seen = {};
    return a.filter(function(item) {
        return seen.hasOwnProperty(item.substring(offset)) ? false : (seen[item.substring(offset)] = true);
    });
}

function console_color(arg, selected)
{
	if(selected)
		arg |= BG_BLUE;
	if(js.global.console != undefined)
		console.attributes = arg;
}

function console_beep()
{
	if(js.global.console != undefined && options.beep !== false)
		console.beep();
}

/* Supported list formats (the property values for 2nd & 3rd columns) */
const list_formats = [
	[ "sysops", "location" ],
	[ "phone_number", "service_address" ],
	[ "since", "software", "web_site" ],
	[ "description" ],
	[ "networks"],
	[ "nodes", "users", "subs", "dirs", "doors", "msgs", "files", "storage"],
	[ "protocols"],
	[ "created_by", "created_on" ],
	[ "updated_by", "updated_on" ],
	[ "verified_by", "verified_on" ],
];

function list_bbs_entry(bbs, selected, sort)
{
	var sysop="";
	var color = color_cfg.column[0];

	if(sort=="name")
		color |= color_cfg.sorted;
	console_color(color, selected);
	printf("%-*s%c", lib.max_len.name, bbs.name, selected ? '<' : ' ');

	color = LIGHTMAGENTA;
	if(!js.global.console || console.screen_columns >= 80) {
		for(var i in list_formats[list_format]) {
			var fmt = "%-*.*s";
			var len = Math.max(list_formats[list_format][i].length, lib.max_len[list_formats[list_format][i]]);
			if(i > 0)
				len++;
			if(js.global.console)
				len = Math.min(len, console.screen_columns - console.current_column -1);
			if(color > WHITE)
				color = DARKGRAY;
			if(color_cfg.column[i+1] != undefined)
				color = color_cfg.column[i+1];
			if(list_formats[list_format][i] == sort)
				color |= color_cfg.sorted;
			console_color(color++, selected);
			if(i > 0 && i == list_formats[list_format].length-1)
				fmt = "%*.*s";
			printf(fmt, len, len, lib.property_value(bbs, list_formats[list_format][i]));
		}
	}
}

function right_justify(text)
{
	console.print(format("%*s", console.screen_columns - console.current_column - 1, text));
}

function help()
{
	console.clear();
	console.printfile(system.text_dir + "sbbslist.hlp");
	console.pause();
	console.clear();
}

function test_port(port)
{
	sock = new Socket();
	success = sock.connect(system.host_name,port);
	sock.close();

	return(success);
}

var common_bbs_services=[
    "telnet",
    "rlogin",
	"ssh",
    "ftp", 
    "nntp",
	"smtp",
	"binkp",
	"irc",
];

function this_bbs()
{
	var bbs = lib.new_system(system.name, system.nodes, lib.system_stats());
	bbs.sysop.push({ name: system.operator, email: system.operator.replace(' ', '.') +'@'+ system.inet_addr });
	bbs.software = "Synchronet";
	bbs.location = system.location;
	bbs.web_site = system.inet_addr;
	bbs.terminal.types.push("TTY", "ANSI");
	if(msg_area.grp["DOVE-Net"])
		bbs.network.push({ name: "DOVE-Net", address: system.qwk_id});
	if(msg_area.grp["FidoNet"] || msg_area.grp["Fidonet"])
		bbs.network.push({ name: "FidoNet", address: system.fido_addr_list[0] });
	print("Testing common BBS service ports");
    var ports = [];
    for(var i in common_bbs_services) {
		var prot = common_bbs_services[i];
        if(ports.indexOf(standard_service_port[prot]) >= 0) // Already tested this port
            continue;
		printf("%s ", prot);
        ports.push(standard_service_port[prot]);
		if(test_port(standard_service_port[prot]))
			bbs.service.push({ protocol: prot, address: system.inet_addr, port: standard_service_port[prot] });
    }
	print();
	if(!bbs.service.length)
		bbs.service.push({ protocol: 'telnet', address: system.inet_addr, port: standard_service_port['telnet'] });

	return bbs;
}

function getstr(prmpt, maxlen, mode)
{
	if(!js.global.console)
		return prompt(prmpt);
	printf("\1n\1y\1h%s\1w: ", prmpt);
	return console.getstr(maxlen, mode !==undefined ? mode : K_LINE);
}

function get_description()
{
	var description = [];
	while(description.length < sbl_defs.DESC_LINES
		&& (str=getstr("Description [line " + (description.length + 1) + " of " + sbl_defs.DESC_LINES + "]"
			, sbl_defs.DESC_LINE_LEN, description.length < sbl_defs.DESC_LINES -1 ? K_WRAP : 0))) {
		description.push(str);
	}
	return description;
}

// Return true if there's a message for the user to read
function add_entry(list)
{
	console.clear();
	if(options.add_ars && !user.compare_ars(options.add_ars)) {
		console_beep();
		alert("Sorry, you cannot do that");
		return true;
	}
	var bbs_name = getstr("BBS Name", lib.max_len.name);
	if(!bbs_name)
		return false;
	if(lib.system_exists(list, bbs_name)) {
		alert("System '" + bbs_name + "' already exists");
		return true;
	}
	var bbs = lib.new_system(bbs_name, /* nodes: */1);
	bbs.sysop.push({ name: user.alias, email: user.email });
	bbs.location = user.location;
	bbs.software = getstr("BBS Software", lib.max_len.software);
	bbs.web_site = getstr("Web-Site (address/URL)", lib.max_len.web_site);
	bbs.terminal.types.push("TTY", "ANSI");
	// Automatically test ports here?
	var host_name = getstr("Terminal server host name or IP address", lib.max_len.service_address);
	bbs.service.push({ protocol: 'telnet', address: host_name, port: standard_service_port['telnet'] });
	bbs.description = get_description();
	if(!bbs.description.length) {
		alert("You need to provide a description");
		return true;
	}
	bbs.first_online = new Date().toISOString();
	if(!edit(bbs)) {
		alert("Edit aborted");
		return true;
	}
	lib.add_system(list, bbs, user.alias);
	lib.append(bbs);
	print("System added successfully");
	return true;
}

function browse(list)
{
	var top=0;
	var current=0;
	var pagesize=console.screen_rows-3;
	var find;
	var orglist=list.slice();
	var sort;
	var reversed = false;

	if(!js.global.console) {
		alert("This feature requires the Synchronet terminal server (and console object)");
		return false;
	}

	js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
	console.ctrlkey_passthru|=(1<<16);      // Disable Ctrl-P handling in sbbs

	if(user.number == 1 && !lib.system_exists(list, system.name) 
		&& !console.noyes(system.name + " is not listed. Add it")) {
		var bbs = this_bbs();
		bbs.description = get_description();
		lib.add_system(list, bbs, user.alias);
		lib.append(bbs);
		print("System added successfully");
	}

	console.line_counter = 0;
	while(!js.terminated) {
//		console.clear(LIGHTGRAY);
		console.home();
		console.current_column = 0;
		console.attributes = color_cfg.header;
		console.print(version_notice);
		if(0)
			console.print(format(" sort='%s'", sort));

		/* Bounds checking: */
		if(current < 0) {
			console_beep();
			current = 0;
		} else if(current >= list.length) {
			console_beep();
			current = list.length-1;
		}

		if(console.screen_columns >= 80)
			right_justify(format("[BBS entry %u of %u]", current + 1, list.length));
		console.cleartoeol();
		console.crlf();
		if(list_format >= list_formats.length)
			list_format = 0;
		else if(list_format < 0)
			list_format = list_formats.length-1;

		/* Column headings */
		printf("%-*s", lib.max_len.name, sort=="name" ? "NAME":"Name");
		for(var i in list_formats[list_format]) {
			var fmt = "%-*s";
			var heading = list_formats[list_format][i];
			if(i > 0 || lib.max_len[heading] >= heading.length)
				printf(" ");
			if(i==list_formats[list_format].length-1) {
				if(i > 0)
					fmt = "%*s";
			}
			heading = heading.replace("_", " ").capitalize();
			printf(fmt
				,Math.min(lib.max_len[list_formats[list_format][i]], console.screen_columns - console.current_column -1)
				,sort==list_formats[list_format][i] ? heading.toUpperCase() : heading);
		}
		console.cleartoeol();
		console.crlf();

		if(top > current)
			top = current;
		else if(top < 0)
			top = 0;
		if(top && top+pagesize > list.length)
			top = list.length-pagesize;
		if(top + pagesize  <= current)
			top = (current+1) - pagesize;

		for(var i=top; i-top < pagesize; i++) {
			console.line_counter = 0;
			if(list[i]) {
				list_bbs_entry(list[i], i==current, sort);
				console.cleartoeol(i==current ? color_cfg.selection : LIGHTGRAY);
			} else
				console.clearline(LIGHTGRAY);
			console.crlf();
		}

//		console.attributes=LIGHTGRAY;
		console.print(format(cmd_prompt_fmt, "List"));
		console.mnemonics(format("~Sort, ~Reverse, ~GoTo, ~Find, fmt:0-%u, ~More, ~Quit, or [View] ~?"
			,list_formats.length-1));
		console.cleartoeol();
		var key = console.getkey(0);
		switch(key.toUpperCase()) {
			case KEY_INSERT:
			case 'A':
				if(add_entry(orglist))
					console.pause();
				list = orglist.slice();
				continue;
			case 'V':
			case '\r':
				view(list, current);
				continue;
			case 'Q':
				return;
			case KEY_HOME:
				current=top=0;
				continue;
			case KEY_END:
				current=list.length-1;
				continue;
			case KEY_UP:
				current--;
				break;
			case KEY_DOWN:
				current++;
				break;
			case KEY_PAGEDN:
			case 'N':
				current += pagesize;
				top += pagesize;
				break;
			case KEY_PAGEUP:
			case 'P':
				current -= pagesize;
				top -= pagesize;
				break;
			case 'F':
				list = orglist.slice();
				console.clearline();
				console.print("\1n\1y\1hFind: ");
				find=console.getstr(60,K_LINE|K_UPPER|K_NOCRLF);
				console.clearline(LIGHTGRAY);
				if(find && find.length) {
					var found = lib.find(list.slice(), find);
					if(found.length)
						list = found;
					else
						console_beep();
				}
				break;
			case 'G':
				console.clearline();
				console.print("\1n\1y\1hGo to BBS Name or Address: ");
				var name=console.getstr(lib.max_len.name,K_LINE|K_UPPER|K_NOCRLF);
				console.clearline(LIGHTGRAY);
				var index = lib.system_index(list, name);
				if(index >= 0)
					current = index;
				else {
					for(index = 0; index < list.length; index++) {
						var n;
						for(n = 0; n < list[index].network.length; n++) {
							if(list[index].network[n].address.toUpperCase() == name)
								break;
						}
						if(n < list[index].network.length)
							break;
						for(n = 0; n < list[index].service.length; n++) {
							if(list[index].service[n].address.toUpperCase() == name)
								break;
						}
						if(n < list[index].service.length)
							break;
					}
					if(index < list.length)
						current = index;
					else
						console_beep();
				}
				break;
			case 'R':
				list.reverse();
				reversed = !reversed;
				break;
			case 'M':	/* More */
				console.clearline();
				console.print(format(cmd_prompt_fmt, "More"));
				var prompt = "";
				if(!options.add_ars || user.compare_ars(options.add_ars))
					prompt += "~Add, ";
				if(can_edit(list[current]))
					prompt += "~Edit, ~Remove, ";
				console.mnemonics(prompt + "~Download, ~Sort, ~Format, ~Quit, ~Help ~?");
				var key = console.getkey(K_UPPER);
				switch(key) {
					case KEY_INSERT:
					case 'A':
						if(add_entry(orglist))
							console.pause();
						list = orglist.slice();
						continue;
					case 'E':
						console.clear();
						/* Edit a clone, not the original (allowing the user to abort the edit) */
						var bbs = edit(objcopy(list[current]));
						if(!bbs) {
							alert("Edit aborted");
							console.pause();
							continue;
						}
						list[current] = bbs;
						lib.replace(bbs);
						console.pause();
						continue;
					case KEY_DEL:
					case 'R':
						if(!can_edit(list[current])) {
							console_beep();
							continue;
						}
						list = orglist.slice();
						console.clear();
						var entry_name = list[current].name;
						if(console.noyes("Remove BBS entry: " + entry_name))
							break;
						if(lib.remove(list[current])) {
							list.splice(current, 1);
							alert(entry_name + " removed successfully");
						}
						orglist = list.slice();
						console.pause();
						break;
					case 'Q':
						break;
					case 'D':
						console.clear();
						console.uselect(1, "Download List Format", "SyncTERM");
						console.uselect(2, "", "JSON");
						switch(console.uselect()) {
							case 1:
								printf("\r\nGenerating list file... ");
								var fname = lib.syncterm_list(list, system.temp_dir);
								if(!fname) {
									alert("Error generating list file");
									console.pause();
								} else {
									print("\rDownloading " + file_getname(fname));
									js.global.bbs.send_file(fname);
								}
								break;
							case 2:
								printf("\r\nGenerating list file... ");
								var fname = system.temp_dir + file_getname(lib.list_fname);
								if(!file_copy(lib.list_fname, fname)) {
									alert("File copy failure: " + fname);
									console.pause();
								} else {
									print("\rDownloading " + file_getname(fname));
									js.global.bbs.send_file(fname);
								}
								break;
						}
						break;
					case 'J':
						console.clear();
						console.write(lfexpand(JSON.stringify(list, null, 4)));
						console.crlf();
						console.pause();
						break;
					case 'S':
					{
						var sort_fields = [ "none", "name" ];
						for(var i in list_formats)
							for(var j in list_formats[i])
								sort_fields.push(list_formats[i][j]);
						for(var i in sort_fields)
							console.uselect(Number(i), "Sort Field", sort_fields[i].replace('_', ' ').capitalize());
						var sort_field = console.uselect();
						if(sort_field == 0)
							list = orglist.slice(), sort = undefined;
						else if(sort_field > 0)
							lib.sort(list, sort = sort_fields[sort_field]);
						if(reversed)
							list.reverse();
						break;
					}
					case 'F':
					{
						var formats = objcopy(list_formats);
						for(var i in formats) {
							formats[i].forEach(function(v, i, a) { a[i] = v.replace('_', ' ').capitalize() });
							console.uselect(Number(i), "List Format", formats[i].join(", "));
						}
						var choice = console.uselect();
						if(choice >= 0)
							list_format = choice;
						break;
					}
					case 'H':
					case '?':
						help();
						break;
				}
				break;
			case 'S':
				if(sort == undefined)
					sort = "name";
				else {
					var sort_field = list_formats[list_format].indexOf(sort);
					if(sort_field >= list_formats[list_format].length)
						sort = undefined;
					else if(key == 'S')	{ /* capital 'S', move backwards through sort fields */
						if(sort == "name")
							sort = undefined;
						else if(sort_field)
							sort = list_formats[list_format][sort_field-1];
						else
							sort = "name";
					} else {
						if(sort_field < 0)
							sort = list_formats[list_format][0];
						else
							sort = list_formats[list_format][sort_field+1];
					}
				}
				if(sort == undefined)
					list = orglist.slice();
				else
					lib.sort(list, sort);
				if(reversed)
					list.reverse();
				break;
				console.clearline();
				console.print("\1n\1y\1hSort: ");
				console.mnemonics("~Name, ~Sysop, ~Address, ~Location, or [None]: ");
				switch(console.getkey(K_UPPER)) {
					case 'N':
						lib.sort(list, "name");
						break;
					case 'S':
						lib.sort(list, "sysops");
						break;
					case 'A':
						lib.sort(list, "service_address");
						break;
					case 'L':
						lib.sort(list, "location");
						break;
					default:
						list = orglist.slice();
						break;
				}
				break;
			case KEY_LEFT:
				list_format--;
				break;
			case KEY_RIGHT:
				list_format++;
				break;
			case '?':
			case 'H':
				help();
				break;
			default:
				if(key>='0' && key <='9')
					list_format = parseInt(key);
				break;
		}
	}
}

function print_additional_services(bbs, addr, start)
{
	for(var i=start; i < bbs.service.length; i++) {
        if(JSON.stringify(bbs.service[i]).toLowerCase() == JSON.stringify(bbs.service[i-1]).toLowerCase())
            continue;
		if(!bbs.service[i] || bbs.service[i].address != addr)
			continue;
		printf(", %s", bbs.service[i].protocol);
		if(bbs.service[i].port && bbs.service[i].port != standard_service_port[bbs.service[i].protocol.toLowerCase()])
			printf(":%u", bbs.service[i].port);
	}
}  

function edit_json(bbs)
{
	var bbs_name = bbs.name;
	var f = new File(system.temp_dir + "json.txt");
	if(!f.open("w"))
		return false;
	f.write(lfexpand(JSON.stringify(bbs, null, 4)));
	f.close();
	if(!console.editfile(f.name))
		return false;
	if(!f.open("r"))
		return false;
	var buf = f.read();
	f.close();
	try {
		bbs=JSON.parse(buf);
	} catch(e) {
		alert("JSON.parse exception: " + e);
		return false;
	}
	if(bbs.name != bbs_name) {
		alert("Cannot change BBS name");
		return false;
	}
	return bbs;
}

function is_nav_key(key)
{
	switch(key) {
		case KEY_UP:
		case KEY_LEFT:
		case KEY_PAGEUP:
		case KEY_DOWN:
		case KEY_RIGHT:
		case KEY_PAGEDN:
			return true;
	}
	return false;
}

function show_bbs_avatar(bbs)
{
	if(!bbs.imported) {
		if(bbs.entry && bbs.entry.created
			&& avatar_lib.draw(bbs.entry.created.by, null, null, /* above */true, /* right */true))
			return;
		if(bbs.name == system.name) {
			avatar_lib.draw(1, null, null, /* above */true, /* right */true);
			return;
		}
	}
	for(var i in bbs.sysop) {
		if(avatar_lib.draw(/* usernum */null, bbs.sysop[i].name, bbs.name, /* above */true, /* right */ true))
			return;
		for(var n in bbs.network) {
			if(!bbs.network[n].address)
				continue;
			if(avatar_lib.draw(/* usernum */null, bbs.sysop[i].name, bbs.network[n].address, /* above */true, /* right */true))
				return;
		}
	}
	if(bbs.entry) {
		if(bbs.entry.created
			&& avatar_lib.draw(/* usernum */null, bbs.entry.created.by, bbs.entry.created.at, /* above */true, /* right */true))
			return;
		if(bbs.entry.updated
			&& avatar_lib.draw(/* usernum */null, bbs.entry.updated.by, bbs.entry.updated.at, /* above */true, /* right */true))
			return;
	}
}

function view(list, current)
{
	console.line_counter = 0;
	console.clear(LIGHTGRAY);
	while(!js.terminated) {

		/* Bounds checking: */
		if(current < 0) {
			console_beep();
			current = 0;
		} else if(current >= list.length) {
			console_beep();
			current = list.length-1;
		}

		var bbs = list[current];
		if(!bbs)
			return;
		console.clear();
		console.attributes = BLACK | BG_LIGHTGRAY;
		console.print(version_notice);
		if(console.screen_columns >= 80)
			right_justify(format("[BBS entry %u of %u]", current + 1, list.length));
		console.cleartoeol();
		console.crlf();
		
		var prefix = "\1n\1c%11s \1h";
		var fmt = prefix + "%-*.*s";
		printf("\1n  ");
		printf(fmt, "Name\1w", lib.max_len.name, lib.max_len.name, bbs.name);
		if(bbs.first_online)
			printf("\1n\1c since \1h%s", bbs.first_online.substring(0,10));
		if(bbs.software) {
			console.attributes = LIGHTGRAY;
			right_justify(bbs.software);
		}
		for(i in bbs.sysop) {
			console.crlf();
			printf(prefix, "Operator");
			printf(bbs.sysop[i] ? bbs.sysop[i].name : "");
			if(bbs.sysop[i].email)
				printf(" \1n\1c<\1h%s\1n\1c>", bbs.sysop[i].email);
		}
		console.crlf();
		if(bbs.location)
			printf(fmt, "Location", lib.max_len.location, lib.max_len.location, bbs.location);
		console.attributes = LIGHTGRAY;
		right_justify(format("%u nodes", bbs.terminal.nodes));
		console.crlf();

		if(bbs.web_site)
			printf(fmt, "Web-site", lib.max_len.web_site, lib.max_len.web_site, bbs.web_site);
		console.attributes = LIGHTGRAY;
		right_justify(format("%u users", bbs.total.users));
		console.crlf();

		var prefix = "\1n\1c%11s \1g\1h";
		var fmt = prefix + "%-*.*s";
		var total = ["doors", "subs", "dirs", "msgs", "files" ];
		for(var i = 0; i < sbl_defs.DESC_LINES; i++) {
			if(bbs.description[i])
				printf(fmt, i ? "" : "Description"
					, sbl_defs.DESC_LINE_LEN, sbl_defs.DESC_LINE_LEN, bbs.description[i] ? bbs.description[i] : "");
			console.attributes = LIGHTGRAY;
			right_justify(format("%u %-5s", bbs.total[total[i]], total[i]));
			console.crlf();
		}

		var listed_hosts = [];
		for(i=0; i < bbs.service.length; i++) {
			if(i && JSON.stringify(bbs.service[i]).toLowerCase() == JSON.stringify(bbs.service[i-1]).toLowerCase())
				continue;
			if(listed_hosts.indexOf(bbs.service[i].address) >= 0)	// Already listed
				continue;
			printf("\1n\1c%11s \1h%s\1n\1c", bbs.service[i].protocol, bbs.service[i].address);
			if(bbs.service[i].protocol != 'modem' 
				&& bbs.service[i].port
				&& bbs.service[i].port != standard_service_port[bbs.service[i].protocol.toLowerCase()])
				printf(":%u", bbs.service[i].port);
			if(bbs.service[i].description)
				printf(" %s", bbs.service[i].description);
			if(bbs.service[i].min_rate && bbs.service[i].max_rate)
				printf(" %u-%ubps", bbs.service[i].min_rate, bbs.service[i].max_rate);
			else if(bbs.service[i].min_rate)
				printf(" %u+bps", bbs.service[i].min_rate);
			else if(bbs.service[i].max_rate)
				printf(" %ubps", bbs.service[i].max_rate);
			print_additional_services(bbs, bbs.service[i].address, i+1);
			listed_hosts.push(bbs.service[i].address);
			console.crlf();
		}
		var networks="";
		for(i in bbs.network) {
			if(i > 0)
				networks += ", ";
			networks += bbs.network[i].name;
			if(bbs.network[i].address)
				networks += format("[%s]", bbs.network[i].address);
		}
		if(networks)
			printf("\1n\1c%11s \1h%s\r\n", "Networks"
				,lfexpand(word_wrap(networks, console.screen_columns - 13).replace(/\n/g,'\n            ')).trimRight());
		if(bbs.entry) {
			if(bbs.entry.created && bbs.entry.created.on) {
				var created_by = bbs.entry.created.by;
				if(bbs.entry.created.at)
					created_by += ("@" + bbs.entry.created.at);
				printf("\1n\1c%11s \1m\1h%s\1n\1m on \1h%s \1n\1m%s\r\n", "Created by"
					,created_by, new Date(bbs.entry.created.on).toDateString().substr(4)
					,bbs.imported ? "via msg-network" : "locally");
			}
			if(bbs.entry.updated && bbs.entry.updated.on) {
				var updated_by = bbs.entry.updated.by;
				if(bbs.entry.updated.at)
					updated_by += ("@" + bbs.entry.updated.at);
				printf("\1n\1c%11s \1m\1h%s\1n\1m on \1h%s\r\n", "Updated by"
					, updated_by, new Date(bbs.entry.updated.on).toDateString().substr(4));
			}
			if(bbs.entry.verified && bbs.entry.verified.on) {
				var verified_by = bbs.entry.verified.by;
				if(bbs.entry.verified.at)	// Not actually supported
					verified_by += ("@" + bbs.entry.verified.at);
				printf("\1n\1c%11s \1m\1h%s\1n\1m on \1h%s\r\n", "Verified by"
					, verified_by, new Date(bbs.entry.verified.on).toDateString().substr(4));
			}
		}
		if(console.term_supports(USER_ANSI)) {
			console.gotoxy(1, console.screen_rows);
			show_bbs_avatar(bbs);
		} else
			console.clearline(), console.crlf();

		console.print(format(cmd_prompt_fmt, "Detail"));
		var prompt="";
		if(options.live_preview || bbs.preview) {
			prompt += "preview:";
			if(options.live_preview)
				prompt+= "~Live";
			if(bbs.preview) {
				if(options.live_preview)
					prompt += "/";
				prompt += "~Cap";
			}
			prompt += ", ";
		}
		if(can_edit(bbs))
			prompt += "~Edit, ~Remove, ";
		if(user.is_sysop)
			prompt += "~JSON, ";
		console.mnemonics(prompt + "~Verify or ~Quit: ");
		var key = console.getkey(K_UPPER);
		switch(key) {
			case 'C':
				while(js.global.bbs.online) {
					if(!bbs.preview) {
						console_beep();
						break;
					}
					console.clear();
					lib.draw_preview(bbs);
					key = console.getkey();
					console.clear();
					if(!is_nav_key(key))
						break;
					do {
						switch(key) {
							case KEY_UP:
							case KEY_LEFT:
							case KEY_PAGEUP:
								current--;
								break;
							case KEY_DOWN:
							case KEY_RIGHT:
							case KEY_PAGEDN:
								current++;
								break;
						}
						if(current >= list.length)
							break;
						bbs = list[current];
					} while(!bbs.preview);
				}
				break;
			case 'L':
				{
					console.clear();
					printf("\1n\1hConnecting to \1y%s \1w\1i...", bbs.name);
					var copy = objcopy(bbs);
					var result = capture_preview(copy);
					if(result == true) {
						console.clear();
						lib.draw_preview(copy);
						console.getkey();
					} else {
						console.crlf();
						alert("Result: " + result);
						console.pause();
					}
					console.clear();
				}
				break;
			case KEY_HOME:
				current=top=0;
				continue;
			case KEY_END:
				current=list.length-1;
				continue;
			case 'B':
			case KEY_UP:
			case KEY_LEFT:
			case KEY_PAGEUP:
				current--;
				break;
			case 'F':
			case KEY_DOWN:
			case KEY_RIGHT:
			case KEY_PAGEDN:
				current++;
				break;
			case 'E':
				console.clear();
				/* Edit a clone, not the original (allowing the user to abort the edit) */
				var edited = edit(objcopy(bbs));
				if(!edited) {
					alert("Edit aborted");
					console.pause();
					continue;
				}
				list[current] = edited;
				lib.replace(edited);
				break;
			case KEY_DEL:
			case 'R':
				if(!can_edit(list[current])) {
					console_beep();
					continue;
				}
				console.clear();
				var entry_name = list[current].name;
				if(console.noyes("Remove BBS entry: " + entry_name))
					break;
				if(lib.remove(list[current])) {
					list.splice(current, 1);
					alert(entry_name + " removed successfully");
				}
				console.pause();
				break;
			case 'J':
				if(user.is_sysop) {
					var edited = edit_json(objcopy(bbs));
					if(edited) {
						list[current] = edited;
						if(lib.replace(edited))
							alert(edited.name + " updated successfully");
					} else
						alert("Edit aborted");
					console.pause();
				}
				break;
			case 'V':
				lib.verify_system(list[current], user.alias);
				lib.replace(list[current]);
				break;
			case 'H':
			case '?':
				help();
				break;
			case 'Q':
				return;
		}
	}
}

String.prototype.capitalize = function(){
   return this.replace( /(^|\s)([a-z])/g , function(m,p1,p2){ return p1+p2.toUpperCase(); } );
  };

function edit_field(obj, field, max_len)
{
	printf("\1h\1y" + field.capitalize().replace('_', '-') + ": ");
	field = field.toLowerCase().replace('-', '_');
	if(max_len == undefined)
		max_len = lib.max_len[field];
	if(max_len == undefined)
		max_len = 3;
	var str = console.getstr(obj[field], max_len, K_EDIT|K_LINE|K_AUTODEL);
	if(str !== false)
		obj[field] = str;
}

function can_edit(bbs)
{
	if(bbs.imported) {
		return "Cannot edit imported entries";
	}
	if(bbs.entry.created
		&& bbs.entry.created.by
		&& bbs.entry.created.by.toLowerCase() != user.alias.toLowerCase()) {
		return "Sorry, this entry was created by: " + bbs.entry.created.by;
	}
	return true;
}

function edit_object(title, obj, props, max_lens, prop_descs)
{
	while(bbs.online) {
		console.clear(LIGHTGRAY);
		print("Editing " + title + ": " + obj[props[0]]);
		console.crlf();
		for(var i in props)
			print(format("%2u %-12s : %s", Number(i)+1, props[i].capitalize(), obj[props[i]]));
		console.mnemonics("\r\nWhich or ~Quit: ");
		var which = console.getnum(props.length);
		if(which < 1)
			break;
		var i = Number(which) - 1;
		printf("\r\n%s\r\n", prop_descs[i]);
		edit_field(obj, props[i], max_lens[i]);
	}
}

function edit_array(title, arr, props, max_lens, prop_descs, max_array_len)
{
	while(bbs.online) {
		console.clear(LIGHTGRAY);
		print("Editing " + title + " List");
		console.crlf();
		for(var i in arr)
			print(format("%2u %-*s %s", Number(i)+1, max_lens[0], arr[i][props[0]], arr[i][props[1]]));

		var keys = "Q";
		var prompt = "\r\nWhich";
		if(arr.length < max_array_len) {
			prompt += ", ~Add", keys += 'A';
			if(arr.length > 0)
				prompt += ", ~Insert", keys += 'I';
		}
		if(arr.length > 0)
			prompt += ", ~Delete", keys += 'D';
		prompt += " or ~Quit: ";
		console.mnemonics(prompt);
		var which = console.getkeys(keys, arr.length);
		if(which == 'A' || which == 'I') {
			print("\r\nAdding " + title);
			if(which == 'I') {
				printf("Before which: ");
				which = console.getnum(arr.length);
				if(which < 1)
					continue;
			}
			var obj = {};
			for(var i in props) {
				printf("\r\n%s\r\n", prop_descs[i]);
				do {
					obj[props[i]] = getstr(props[i].capitalize(), max_lens[i]);
				} while(i==0 && !obj[props[i]]);
			}
			if(which == 'A')
				arr.push(obj);
			else
				arr.splice(which - 1, 0, obj);
			continue;
		}
		if(which == 'D') {
			if(arr.length == 1)
				which = 1;
			else {
				printf("Delete which: ");
				which = console.getnum(arr.length);
			}
			if(which > 0)
				arr.splice(which - 1, 1);
			continue;
		}
		if(which == false || which == 'Q' || which < 1)
			return false;
		edit_object(title, arr[Number(which)-1], props, max_lens, prop_descs);
	}
}

function edit(bbs)
{
	var allowed = can_edit(bbs);
	if(allowed != true) {
		alert(allowed);
		return false;
	}
	if(!bbs.first_online)
			bbs.first_online="";
	while(js.global.bbs.online) {
		console.clear();
		printf("\1nEditing BBS entry: \1h%s\r\n", bbs.name);
		console.crlf();
		var opts = 1;
		var optlen = 15;
		var optmax = console.screen_columns - 30;
		var sysop = "";
		if(bbs.sysop.length)
			sysop = format("%s <%s>", bbs.sysop[0].name, bbs.sysop[0].email);
		printf("\1n%2u \1h%-*s : %s\r\n"	, opts++, optlen, "Since", bbs.first_online.substring(0,10));
		printf("\1n%2u \1h%-*s : %.*s\r\n"	, opts++, optlen, "Sysop", optmax, sysop);
		printf("\1n%2u \1h%-*s : %.*s\r\n"	, opts++, optlen, "Location", optmax, bbs.location);
		printf("\1n%2u \1h%-*s : %.*s\r\n"	, opts++, optlen, "Web-site", optmax, bbs.web_site);
		printf("\1n%2u \1h%-*s : %.*s\r\n"	, opts++, optlen, "Software", optmax, bbs.software);
		printf("\1n%2u \1h%-*s : %.*s\r\n"	, opts++, optlen, "Description", optmax, bbs.description.join(" "));
		printf("\1n%2u \1h%-*s : %u\r\n"	, opts++, optlen, "Terminal Nodes", bbs.terminal.nodes);
		var term_types = "";
		if(bbs.terminal.types)
			term_types = bbs.terminal.types.join(", ");
		printf("\1n%2u \1h%-*s : %.*s\r\n"	, opts++, optlen, "Terminal Types", optmax, term_types);
		printf("\1n%2u \1hTotals...\r\n"	, opts++);
		printf("\1n%2u \1hNetworks...\r\n"	, opts++);
		printf("\1n%2u \1hServices...\r\n"	, opts++);

		console.mnemonics("\r\nWhich, ~Save or ~Abort: ");
		var which = console.getkeys("SQA", opts);
		if(!which || which == 'A')
			return false;
		if(which == 'S')
			break;
		switch(which) {
			case 1:
				printf("\1n\1h\1ySince (\1cYYYY\1y-\1cMM\1y-\1cDD\1y)\1w: ");
				var value = console.gettemplate("NNNN-NN-NN", bbs.first_online.substring(0,10), K_LINE|K_EDIT);
				if(value)
					bbs.first_online = new Date(value).toISOString();
				break;
			case 2:
				edit_array("Operator"
					,bbs.sysop, [ "name", "email" ]
					,[ lib.max_len.sysop_name, lib.max_len.email_addr ]
					,[ "Sysop's name or alias", "Sysop's e-Mail address (optional)" ]
					,sbl_defs.MAX_SYSOPS);
				break;
			case 3:
				edit_field(bbs, "Location");
				break;
			case 4:
				edit_field(bbs, "Web-site");
				break;
			case 5:
				edit_field(bbs, "Software");
				break;
			case 6:
				console.clear();
				if(!bbs.description.length) {
					bbs.description = get_description();
					break;
				}
				for(var i in bbs.description)
					print(bbs.description[i]);
				console.home();
				for(var i=0; i < sbl_defs.DESC_LINES && !console.aborted; i++)
					bbs.description[i] = console.getstr(bbs.description[i], sbl_defs.DESC_LINE_LEN, K_EDIT|K_MSG);
				break;
			case 7:
				printf("\1n\1h\1yTerminal server nodes\1w: ");
				var nodes = console.getstr(String(bbs.terminal.nodes), 3, K_NUMBER|K_EDIT|K_AUTODEL);
				if(nodes > 0)
					bbs.terminal.nodes = nodes;
				break;
			case 8:
				printf("\1n\1h\1yTerminal types supported\1w: ");
				var terms = bbs.terminal.types.join(", ");
				terms = console.getstr(terms, 40, K_EDIT|K_LINE|K_UPPER);
				if(terms)
					bbs.terminal.types = terms.split(/\s*,\s*/, sbl_defs.MAX_TERMS);
				break;
			case 9:
				while(js.global.bbs.online)
				{
					console.clear();
					printf("\1nEditing BBS totals for: \1h%s\r\n\r\n", bbs.name);
					opts = 1;
					optlen = 8;
					var prop = [];
					for(var i in bbs.total) {
						prop[opts] = i;
						printf("\1n%2u \1h%-*s : ", opts++, optlen, i.capitalize());
						print(bbs.total[i]);
					}
					console.mnemonics("\r\nWhich or ~Quit: ");
					var which = console.getnum(opts);
					if(which < 1)
						break;
					printf("\1h\1yTotal %s\1w: ", prop[which]);
					var newval = console.getstr(20);
					if(newval !== false && newval.length)
						bbs.total[prop[which]] = newval;
				}
				break;
			case 10:
				edit_array("Network"
					,bbs.network
					,[ "name", "address" ]
					,[ lib.max_len.network_name, lib.max_len.network_address ]
					,[ "Network name" ,"Network node address (e.g. QWK-ID or FTN node address)" ]
					,sbl_defs.MAX_NETS);
				break;
			case 11:
				edit_array("Service"
					,bbs.service
					,[ "address", "protocol", "port", "min_rate", "max_rate", "description" ]
					,[ lib.max_len.service_address, 15, 5, 5, 5, 15 ]
					,[ "Terminal service host name, IP address (excluding port number), or phone number" 
					  ,"TCP protocol name (e.g. telnet, ssh, rlogin) or 'modem' for traditional dial-up"
					  ,"TCP Port number, if non-standard"
					  ,"Minimum connection bit-rate (for dial-up modem)"
					  ,"Maximum connection bit-rate (for dial-up modem)"
					  ,"Service/modem description (optional)"
					 ]
					,sbl_defs.MAX_NUMBERS);
				break;

		}
	}
	lib.update_system(bbs, user.alias);
	return bbs;
}

function find_code(objs, code)
{
	for(var i=0; i < objs.length; i++)
		if(objs[i].code.toLowerCase() == code.toLowerCase())
			return i;
	return -1;
}

function remove_from_list(list, value)
{
	var index = find_code(list, value);
	if(index < 0)
		return null;
	return list.splice(index, 1);
}

function replace_in_list(list, value, obj)
{
	var index = find_code(list, value);
	if(index < 0)
		list.push(obj);
	else
		list[index] = obj;
}

function install()
{
	var sbbslist_cfg = {
		"sec": 0,
		"name": "Synchronet BBS List",
		"code": "SBBSLIST",
		"arstr": "",
		"run_arstr": "",
		"type": 0,
		"misc": 1,
		"event": 0,
		"cost": 0,
		"cmd": "?sbbslist.js browse",
		"clean": "",
		"path": "",
		"textra": 0,
		"maxtime": 0
		};
	var smb2sbl_cfg = {
		"code": "SMB2SBL",
		"cmd": "?sbbslist import",
		"days": 255,
		"time": 0,
		"node": 1,
		"misc": 0,
		"dir": "",
		"freq": 360,
		"mdays": 0,
		"months": 0
		};
	var sbl2smb_cfg = {
        "code": "SBL2SMB",
        "cmd": "?sbbslist export",
        "days": 255,
        "time": 0,
        "node": 1,
        "misc": 0,
        "dir": "",
        "freq": 360,
        "mdays": 0,
        "months": 0
		};
	var sblupdate_cfg = {
        "code": "SBLUPDAT",
        "cmd": "?sbbslist update -preview",
        "days": 255,
        "time": 0,
        "node": 1,
        "misc": 0,
        "dir": "",
        "freq": 0,
        "mdays": 2,
        "months": 0
		};
	var sblmaint_cfg = {
        "code": "SBLMAINT",
        "cmd": "?sbbslist maint",
        "days": 255,
        "time": 0,
        "node": 1,
        "misc": 0,
        "dir": "",
        "freq": 0,
        "mdays": 0,
        "months": 0
		};
	var cnflib = load({}, "cnflib.js");
	var xtrn_cnf = cnflib.read("xtrn.cnf");
	if(!xtrn_cnf)
		return "Failed to read xtrn.cnf";

	remove_from_list(xtrn_cnf.xtrn, "sbl");
	printf("Adding external program: SBBSLIST\r\n");
	replace_in_list(xtrn_cnf.xtrn, "sbbslist", sbbslist_cfg);

	printf("Adding timed event: SMB2SBL\r\n");
	replace_in_list(xtrn_cnf.event, "smb2sbl", smb2sbl_cfg);

	printf("Adding timed event: SBL2SMB\r\n");
	replace_in_list(xtrn_cnf.event, "sbl2smb", sbl2smb_cfg);

	printf("Adding timed event: SBLUPDAT\r\n");
	replace_in_list(xtrn_cnf.event, "sblupdat", sblupdate_cfg);

	printf("Adding timed event: SBLMAINT\r\n");
	replace_in_list(xtrn_cnf.event, "sblmaint", sblmaint_cfg);

	if(!cnflib.write("xtrn.cnf", undefined, xtrn_cnf))
		return "Failed to write xtrn.cnf";

	return true;
}


function main()
{
    var i,j;
    var list;
    var verbose = 0;
	var quiet = false;
    var msgbase;
	var optval={};
	var cmds=[];
	var count=0;
	var exclude = [];
	var filter;
	var all;
	var ptr;
	var limit;
	var addr;
	var preview;

    for(i in argv) {
		var arg = argv[i];
		var val = ""
		var eq = arg.indexOf('=');

		if(eq > 0) {
			val = arg.substring(eq+1);
			arg = arg.substring(0,eq);
		}
		optval[arg] = val;

        switch(arg) {
            case "-v":
                verbose++;
                break;
			case "-quiet":
				quiet = true;
				break;
			case "-exclude":
				exclude.push(val);
				break;
			case "-format":
				if(val === undefined || val === '?' || !val.length) {
					print("Supported list formats:");
					for(var i in list_formats)
						print(format("%u = %s", i, list_formats[i].join(", ")));
				}
				list_format = parseInt(val);
				break;
			case "-filter":
				filter = val;
				break;
			case "-all":
				all = true;
				ptr = 0;
				break;
			case "-ptr":
				ptr = val;
				break;
			case "-addr":
				addr = val;
				break;
			case "-preview":
				if(val)
					addr = val;
				preview = true;
				break;
			case "-sort":
				options.sort = val;
				break;
			case "-reverse":
				options.reverse = !options.reverse;
				break;
			case "-debug":
				debug = true;
				break;
			default:
				if(parseInt(arg) < 0)
					limit = -parseInt(arg);
				else
					cmds.push(arg);
				break;
        }
    }

	if(!quiet)
		print(version_notice);

    if(!file_exists(lib.list_fname))
        list=upgrade_list(sbl_dir + "sbl.dab");
    else
        list=lib.read_list();
	if(options && options.sort)
		lib.sort(list, options.sort);
	if(options && options.reverse)
		list.reverse();

	for(var c in cmds) {
		var cmd = cmds[c].toLowerCase();
		switch(cmd) {
			case "count":
				print(list.length + " BBS entries in " + lib.list_fname);
				break;
			case "upgrade":
                list=upgrade_list(sbl_dir + "sbl.dab");
                print(list.length + " BBS entries upgraded from " + sbl_dir + "sbl.dab");
				break;
			case "backup":
				file_backup(lib.list_fname, limit ? limit : options.backup_level);
				break;
			case "import":
			case "export":
				var msgbase = new MsgBase(optval[cmd] ? optval[cmd] : options.sub);
				print("Opening msgbase " + msgbase.file);
				if(!msgbase.open()) {
					alert("Error " + msgbase.error + " opening msgbase: " + msgbase.file);
					exit(-1);
				}
				if(cmd == "import")
					import_from_msgbase(list, msgbase, ptr, limit, all);
				else
					export_to_msgbase(list, msgbase, limit, all);
				msgbase.close();
				break;
			case "syncterm":
				print(list.length + " BBS entries exported to: " + lib.syncterm_list(list, system.data_dir));
				break;
			case "html":
				file_backup("sbbslist.html", limit ? limit : options.backup_level);
				var f = new File("sbbslist.html");
				if(!f.open("wb")) {
					log(LOG_ERR,"Error opening " + f.name);
					exit();
				}
				load(f, "sbbslist_html.js", lib, list);
				f.close();
				print(list.length + " BBS entries exported to: " + f.name);
				break;
			case "imsglist":
				var ibbs = [];
				for(i in list) {
					var bbs = list[i];
					if(!bbs.entry.autoverify.success)
						continue;
					if(!lib.imsg_capable_system(bbs))
						continue;
					ibbs.push(format("%-63s\t%s\t%s",
						bbs.entry.autoverify.last_success.service.address,
						bbs.entry.autoverify.last_success.ip_address,
						bbs.name));
				}
				ibbs = unique_strings(ibbs, /* offset: */64);
				file_backup("sbbsimsg.lst", limit ? limit : options.backup_level);
				var f = new File("sbbsimsg.lst");
				if(!f.open("w")) {
					log(LOG_ERR,"Error opening " + f.name);
					exit();
				}
				f.writeAll(ibbs);
				f.close();
				print(ibbs.length + " BBS entries exported to: " + f.name);
				break;
			case "sort":
				print("Sorting list by property: " + optval["sort"]);
				lib.sort(list, optval["sort"]);
				break;
			case "randomize":
				list.sort(function () { return 0.5 - Math.random() } );
				break;
			case "find":
				print("Searching for " + optval["find"]);
				list = lib.find(list, optval["find"]);
				print(list.length + " entries found");
				break;
			case "verify":
				if(optval[cmd]) {
					var index = lib.system_index(list, optval[cmd]);
					if(index < 0)
						alert(optval[cmd] + " not found");
					else {
						verify_bbs(list[index]);
						lib.replace(list[index]);
					}
				} else {
					verify_list(list);
				}
				break;
			case "browse":	// BBS-only
				browse(list);
				break;
			case "save":
				lib.write_list(list);
				break;
			case "show":
				for(i in list) {
					if(verbose) {
						for(j in list[i])
							print(j + " : " + JSON.stringify(list[i][j]));
						continue;
					}
					printf("%-25s %-25s %-25s", list[i].name, list[i].entry.created.by, list[i].sysop[0] ? list[i].sysop[0].name: '');
					if(list[i].entry.updated)
						print(new Date(list[i].entry.updated.on.substr(0,10)).toISOString());
					else
						print();
				}
				print(list.length + " BBS entries");
				break;
			case "list":
				for(i in list) {
					if(exclude.indexOf(list[i].name) >= 0)
						continue;
					if(filter) {
						with(list[i]) {
							if(!eval(filter))
								continue;
						}
					}
					list_bbs_entry(list[i], false);
					writeln();
					count++;
					if(optval["list"] && count >= optval["list"])
						break;
					if(limit && count >= limit)
						break;
				}
				break;
			case "dump":
				if(optval[cmd]) {
					var index = lib.system_index(list, optval[cmd]);
					if(index < 0)
						alert(optval[cmd] + " not found");
					else {
						print(JSON.stringify(list[index], null, 1));
						hexdump("compressed preview", base64_decode(list[index].preview.join("")));
					}
					break;
				}
				for(i in list) {
					if(exclude.indexOf(list[i].name) >= 0)
						continue;
					if(filter) {
						with(list[i]) {
							if(!eval(filter))
								continue;
						}
					}
					print(JSON.stringify(list[i], null, 1));
					count++;
					if(limit && count >= limit)
						break;
				}
				break;
			case "add":
				if(lib.system_exists(list, system.name)) {
					alert("System '" + system.name + "' already exists");
					exit(-1);
				}
				var bbs = this_bbs();
				bbs.description = get_description();
				lib.add_system(list, bbs, system.operator);
				lib.append(bbs);
				print(system.name + " added successfully");
				break;
			case "remove":
				if(optval[cmd]) {
					var index = lib.system_index(list, optval[cmd]);
					if(index < 0)
						alert(optval[cmd] + " not found");
					else
						if(lib.remove(list[index]))
							print(list[index].name + " removed successfully");
					break;
				}
				alert("usage: remove=bbs_name");
				break;
			case "update":
				var index = lib.system_index(list, system.name);
				if(index < 0) {
					alert("System '" + system.name + "' does not exist");
					exit(-1);
				}
				var bbs=list[index];
				bbs.software = "Synchronet";
				bbs.total = lib.system_stats();
				bbs.terminal.nodes = system.nodes;
				if(preview) {
					log("Capturing preview from: " + bbs.name);
					var result = capture_preview(bbs, addr);
					if(result != true)
						alert(result);
				}
				lib.update_system(bbs, js.exec_file + " " + REVISION);
				if(debug) print(JSON.stringify(bbs, null, 1));
				if(lib.replace(bbs))
					print(system.name + " updated successfully");
				break;
			case "preview":
				var index = lib.system_index(list, optval[cmd]);
				if(index < 0) {
					alert("System '" + system.name + "' does not exist");
					exit(-1);
				}
				var result = capture_preview(list[index], addr);
				if(result == true) {
					if(js.global.console) {
						lib.draw_preview(list[index].preview);
					}
				} else
					print(result);
				break;
			case "dedupe":
				print(list.length + " BBS entries before de-duplication");
				var list = lib.remove_dupes(list);
				print(list.length + " BBS entries after de-duplication");
				lib.write_list(list);
				break;
			case "check":
				for(var i=0; i < list.length; i++) {
					var entry = list[i];
					if(!lib.check_entry(entry)) {
						if(debug) print(JSON.stringify(entry, null, 1));
					}
				}
				break;
			case "maint":
				alert("Not yet implemented");
				break;
			case "install":
				var result = install();
				printf("%s\r\n", result == true ? "Successful" : "!FAILED: " + result);
				break;
		}
    }
}

main();
