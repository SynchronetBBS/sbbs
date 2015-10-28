// $Id$

// Synchronet BBS List

// This one script replaces (or *will* replace) the functionality of:
// sbl[.exe]        - External online program (door)
// smb2sbl[.exe]    - Imports BBS entries from Synchronet Message Base (e.g. from SYNCDATA echo)
// sbl2smb[.exe]    - Exports BBS entries to Synchronet Message Base (e.g. to SYNCDATA echo)
// sbbslist[.exe]   - Exports BBS entries to HTML and various plain-text formats (e.g. sbbs.lst, sbbsimsg.lst, syncterm.lst)

var REVISION = "$Revision$".split(' ')[1];
var version_notice = "Synchronet BBS List v4(" + REVISION + ")";

load("sbbsdefs.js");
load("sockdefs.js");
load("portdefs.js");

var sbl_dir = "../xtrn/sbl/";
var list_format = 0;
var color_cfg = { 
	header: BLACK | BG_LIGHTGRAY,
	selection: BG_BLUE,
	column: [ WHITE ],
	sorted: BG_RED,
};

var options=load({}, "modopts.js", "sbbslist");
if(!options || !options.sub)
    options.sub="syncdata";
if(options && options.list_format > 0)
	list_format = options.list_format;

var lib = load({}, "sbbslist_lib.js");
load("graphic.js");
var capture = load({}, "termcapture_lib.js");
capture.timeout=15;

// This date format is required for backwards compatibility with SMB2SBL
function date_to_str(date)
{
    return format("%02u/%02u/%02u", date.getUTCMonth()+1, date.getUTCDate(), date.getUTCFullYear()%100);
}

function date_from_str(str)
{
    return new Date(parseInt(str.substr(6,2),10), parseInt(str.substr(0,2),10), parseInt(str.substr(3,2),10));
}

function export_entry(bbs, msgbase)
{
    var i;
    var hdr = { to:'sbl', from:bbs.entry.created.by, subject:bbs.name };

    var body = "";      // This section for SMB2SBL compatibility
    body += "Name:          " + bbs.name + "\r\n";
	if(bbs.first_online)
		body += "Birth:         " + date_to_str(new Date(bbs.first_online)) + "\r\n";
    if(bbs.software.bbs)
        body += "Software:      " + bbs.software.bbs + "\r\n";
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
        switch(bbs.service[i].protocol) {
            case 'modem':
                body += "Number:        " + bbs.service[i].address + "\r\n";
                body += "MinRate:       " + bbs.service[i].min_rate + "\r\n";
                body += "MaxRate:       " + bbs.service[i].max_rate + "\r\n";
                if(bbs.service[i].description)
                    body += "Modem:         " + bbs.service[i].description + "\r\n";
                break;
            case 'telnet':
                body += "Telnet:        " + bbs.service[i].address + "\r\n";
                body += "Port:          " + bbs.service[i].port + "\r\n";
                break;
         }
    }
    for(i in bbs.network) {
        body += "Network:       " + bbs.network[i].name + "\r\n";
        body += "Address:       " + bbs.network[i].address + "\r\n";
    }
    for(i in bbs.terminal.support)
        body += "Terminal:      " + bbs.terminal.support[i].type + "\r\n";
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
    if(bbs.total.xtrns)
        body += "Xtrns:         " + bbs.total.xtrns + "\r\n";
    for(i in bbs.description)
        body += "Desc:          " + bbs.description[i] + "\r\n";

    body += "\r\njson-begin\r\n";
    body += JSON.stringify(bbs, null, 1) + "\r\n";
    body += "json-end\r\n";
    body += "--- " + js.exec_file + " " + REVISION + "\r\n";
//    print(body);

    return msgbase.save_msg(hdr, body);
}

function export_to_msgbase(list, msgbase)
{
    var i;
    var count=0;
    var last_export;

    var ini = new File(msgbase.file + ".ini");
    print("Opening " + ini.name);
    if(ini.open("r")) {
        last_export=ini.iniGetValue("sbbslist","last_export", new Date(0));
        print("last export = " + last_export);
        ini.close();
    } else
        print("Error " + ini.error + " opening " + ini.name);
    /* Fallback to using old SBL export pointer (time_t) storage file/format */
    if(!last_export) {
        var f = new File(sbl_dir + "sbl2smb.dab");
        print("Opening " + f.name);
        if(f.open("rb")) {
            last_export = new Date(f.readBin(4)*1000);
            f.close();
        } else
            print("Error " + f.error + " opening " + f.name);
    }
    print("Exporting entries created/modified/verified since " + last_export.toString());
    for(i in list) {
        if(js.terminated)
            break;
        if(list[i].imported)
            continue;
        if(list[i].entry.created.on < last_export
            && list[i].entry.updated.on < last_export
            && list[i].entry.verified.on < last_export)
            continue;
        if(!export_entry(list[i], msgbase))
            break;
        count++;
    }
    print("Exported " + count + " entries");
    print("Opening " + ini.name);
    if(ini.open(file_exists(ini.name) ? 'r+':'w+')) {
        ini.iniSetValue("sbbslist","last_export",new Date());
        ini.close();
    } else
        print("Error " + ini.error + " opening " + ini.name);
}

function import_entry(bbs, text)
{
    var i;
    var json_begin;
    var json_end;

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

    bbs.sysop = [];
    bbs.network = [];
    bbs.terminal = { nodes:0, support:[] };
    bbs.description = [];
    bbs.service = [];
    bbs.total = {};

    for(i in text) {
        //print(text[i]);
        if(!text[i].length)
            continue;
        var match=text[i].match(/\s*([A-Z\-]+)\:\s*(.*)/i);
        if(!match || match.length < 3) {
            print("No match: " + text[i]);
            continue;
        }
        //print(match[1] + " = " + match[2]);
        switch(match[1].toLowerCase()) {
            case 'birth':
                bbs.first_online = date_from_str(match[2]);
                break;
            case 'software':
                bbs.software = {bbs: match[2]};
                break;
            case 'sysop':
                if(bbs.sysop.length)
                    sysop++;
                bbs.sysop[sysop] = { name: match[2] };
                break;
            case 'e-mail':
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
					bbs.service[number].port = standard_service_port(bbs.service[number].protocol.toLowerCase());
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
                if(bbs.terminal.support.length)
                    terminal++;
                bbs.terminal.support[terminal] = {type: match[2]};
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
            case 'xtrns':
                bbs.total[match[1].toLowerCase()] = parseInt(match[2], 10);
                break;
            case 'nodes':
                bbs.terminal.nodes = parseInt(match[2], 10);
                break;
        }
    }
    return bbs;
}

function import_from_msgbase(list, msgbase)
{
    var i;
    var count=0;
    var import_ptr;
    var highest=0;
    var sbl_crc=crc16_calc("sbl");

    var ini = new File(msgbase.file + ".ini");
    print("Opening " + ini.name);
    if(ini.open("r")) {
        import_ptr=ini.iniGetValue("sbbslist","import_ptr", 0);
        ini.close();
    } else
        print("Error " + ini.error + " opening " + ini.name);
    if(import_ptr==undefined) {
        var f = new File(file_getcase(msgbase.file + ".sbl"));
        if(f.open("rb")) {
            import_ptr = f.readBin(4);
            f.close();
        }
    }
    highest=import_ptr;
    print("import_ptr = " + import_ptr);
    for(i=0; i<msgbase.total_msgs; i++) {
        if(js.terminated)
            break;
        //print(i);
        var idx = msgbase.get_msg_index(/* by_offset: */true, i);
        if(!idx) {
            print("Error " + msgbase.error + " reading index of msg offset " + i);
            continue;
        }
        if(idx.number <= import_ptr)
            continue;
        if(idx.to != sbl_crc) {
            //print(idx.to + " != " + sbl_crc);
            continue;
        }
        if(idx.number > highest)
            highest = idx.number;
        var hdr = msgbase.get_msg_header(/* by_offset: */true, i);
        var l;
        var msg_from = truncsp(hdr.from);
		var msg_from_addr = truncsp(hdr.from_net_addr);
        var bbs_name = truncsp(hdr.subject);
//        print("Searching " + list.length + " entries for BBS: " + bbs_name);
        for(l=0; l<list.length; l++) {
            //print("Comparing " + list[l].name);
            if(list[l].name.toLowerCase() == bbs_name.toLowerCase())
                break;
        }
//        print("l = " + l);
        if(l==undefined)
            l=0;
        if(list.length && list[l]) {
            if(!list[l].entry)
                continue;
            if(list[l].entry.created.by.toLowerCase() != msg_from.toLowerCase()) {
                print(msg_from + " did not create entry: " + bbs_name + " (" + list[l].entry.created.by + " did)");
                continue;
            }
            if(list[l].imported == false) {
                print(msg_from + " attempting to update/over-write local entry: " + bbs_name);
                continue;
            }
            print("Updating existing entry: " + bbs_name + " (by " + list[l].entry.created.by + ")");
        } else {
            print(msg_from + " creating new entry: " + bbs_name);
            list[l] = {name: bbs_name, entry: {created: {by:msg_from, at:msg_from_addr, on:new Date() } } };
        }
        var body = msgbase.get_msg_body(/* by_offset: */true, i
            ,/* strip Ctrl-A */true, /* rfc822-encoded: */false, /* include tails: */false);
        import_entry(list[l], body);
        list[l].imported = true;
//        if(!list[l].birth)
//            list[l].birth=list[l].entry.created.on;
        list[l].entry.updated= { on: new Date(), by:msg_from };
        count++;
    }

    if(ini.open(file_exists(ini.name) ? 'r+':'w+')) {
        print("new import_ptr = " + highest);
        ini.iniSetValue("sbbslist","import_ptr",highest);
        ini.close();
    } else
        print("Error opening/creating " + ini.name);
    print("Imported " + count + " entries");
    if(count)
        return lib.write_list(list);
}

// From sbldefs.h (Do not change, for backwards compatibility):
const sbl_defs = {
    MAX_SYSOPS:     5,
    MAX_NUMBERS:    20,
    MAX_NETS:       10,
    MAX_TERMS:      5,
    DESC_LINES:     5
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
    obj.software = { bbs: truncsp(f.read(16)) };
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
    obj.terminal.support = [];
    for(i=0;i<sbl_defs.MAX_TERMS;i++)
        obj.terminal.support[i] = { type: truncsp(f.read(16)) };
    obj.terminal.support.length = total;
    for(i=0;i<sbl_defs.DESC_LINES;i++)
        obj.description.push(truncsp(f.read(51)));
    for(i in obj.description) {
        if(obj.description[i].length == 0)
            break;
    }
    obj.description.length=i;	// terminate description at first blank line
    obj.terminal.nodes = f.readBin(2);
    obj.total.users = f.readBin(2);
    obj.total.subs = f.readBin(2);
    obj.total.dirs = f.readBin(2);
    obj.total.xtrns = f.readBin(2);
    obj.entry.created.on = new Date(f.readBin(4)*1000);
    var updated = f.readBin(4);
    if(updated)
        obj.entry.updated = { on: new Date(updated*1000) };
    var first_online = f.readBin(4);
    if(first_online)
        obj.first_online = new Date(first_online*1000);
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
				service.protocol = uri[1];
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
        obj.entry.verified = { on: new Date(verified_date*1000), by: truncsp(verified_by) };
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
    if(!dab.open("rb")) {
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
    ];
    var udp_services=[
        "finger",
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
		bbs.entry.autoverify = {attempts:0, successes:0};
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
                bbs.entry.verified = { by: js.exec_file + " " + REVISION, on: new Date() };
                graphic = new Graphic();
                graphic.ANSI = result.preview.join("\r\n");
                bbs.preview = graphic.base64_encode();
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
        else
            print("Failure: " + bbs.entry.autoverify.last_failure.result);
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

/* Supported list formats (the property values for 2nd & 3rd columns) */
const list_formats = [
	[ "sysop_name", "location" ],
	[ "phone_number", "service_address" ],
	[ "since", "bbs_software", "web_site" ],
	[ "description" ],
	[ "networks"],
	[ "nodes", "users", "subs", "dirs", "xtrns", "msgs", "files", "storage"],
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
	printf("%-*s ", lib.max_len.name, bbs.name);

	color = LIGHTMAGENTA;
	if(!js.global.console || console.screen_columns >= 80) {
		for(var i in list_formats[list_format]) {
			var fmt = "%-*.*s";
			var len = Math.max(list_formats[list_format][i].length, lib.max_len[list_formats[list_format][i]]);
			if(i > 0)
				len++;
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

function browse(list)
{
	var top=0;
	var current=0;
	var pagesize=console.screen_rows-3;
	var find;
	var orglist=list.slice();
	var sort;

	if(!js.global.console) {
		alert("This feature requires the Synchronet terminal server (and console object)");
		return false;
	}
	console.line_counter = 0;
	while(!js.terminated) {
//		console.clear(LIGHTGRAY);
		console.home();
		console.current_column = 0;
		console.attributes = color_cfg.header;
		console.print(version_notice);
		if(console.screen_columns >= 80)
			right_justify(format("[%u BBS entries]", list.length));
		console.cleartoeol();
		console.crlf();
		if(list_format >= list_formats.length)
			list_format = 0;
		else if(list_format < 0)
			list_format = list_formats.length-1;

		/* Column headings */
		printf("%-*s", lib.max_len.name, sort=="name" ? "NAME":"name");
		for(var i in list_formats[list_format]) {
			var fmt = "%-*s";
			var heading = list_formats[list_format][i];
			if(i > 0 || lib.max_len[heading] >= heading.length)
				printf(" ");
			if(i==list_formats[list_format].length-1) {
				if(i > 0)
					fmt = "%*s";
			}
			heading = heading.replace("_", " ");
			printf(fmt
				,Math.min(lib.max_len[list_formats[list_format][i]], console.screen_columns - console.current_column -1)
				,sort==list_formats[list_format][i] ? heading.toUpperCase() : heading);
		}
		console.cleartoeol();
		console.crlf();

		/* Bounds checking: */
		if(current < 0) {
			console.beep();
			current = 0;
		} else if(current >= list.length) {
			console.beep();
			current = list.length-1;
		}
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

		console.attributes=LIGHTGRAY;
		if(!options.add_ars || user.compare_ars(options.add_ars))
			console.mnemonics("~Add, ");
		console.mnemonics(format("~Up/~Down, ~Next/~Prev, ~Sort, ~Reverse, ~Find, fmt(~0-~%u), ~Quit, ~More, or [~View] ~?", list_formats.length-1));
		var key = console.getkey(K_UPPER);
		switch(key) {
			case 'A':
				if(options.add_ars && !user.compare_ars(options.add_ars)) {
					console.beep();
					continue;
				}
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
			case 'U':
				current--;
				break;
			case KEY_DOWN:
			case 'D':
				current++;
				break;
			case 'N':
				current += pagesize;
				top += pagesize;
				break;
			case 'P':
				current -= pagesize;
				top -= pagesize;
				break;
			case 'F':
				list = orglist;
				console.clearline();
				console.print("\1n\1y\1hFind: ");
				find=console.getstr(60,K_LINE|K_UPPER|K_NOCRLF);
				console.clearline(LIGHTGRAY);
				if(find && find.length)
					list = lib.find(list, find);
				break;
			case 'R':
				list.reverse();
				break;
			case 'M':	/* More */
				console.clearline();
				console.mnemonics(format("~Download SyncTERM.lst file, ~Sort, ~Format, ~Quit, ~?"));
				var key = console.getkey(K_UPPER);
				switch(key) {
					case 'Q':
						break;
					case 'D':
						console.clear();
						var fname = lib.syncterm_list(list);
						if(!fname)
							alert("Error generating list file");
						else {
							print("Downloading " + file_getname(fname));
							bbs.send_file(fname);
						}
						break;
					case 'S':
					{
						var sort_fields = [ "none", "name" ];
						for(var i in list_formats)
							for(var j in list_formats[i])
								sort_fields.push(list_formats[i][j]);
						for(var i in sort_fields)
							console.uselect(Number(i), "Sort Field", sort_fields[i]);
						var sort_field = console.uselect();
						if(sort_field == 0)
							list = orglist, sort = undefined;
						else if(sort_field > 0)
							lib.sort(list, sort = sort_fields[sort_field]);
						break;
					}
					case 'F':
						for(var i in list_formats)
							console.uselect(Number(i), "List Format", list_formats[i]);
						var choice = console.uselect();
						if(choice >= 0)
							list_format = choice;
						break;
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
					else if(sort_field < 0)
						sort = list_formats[list_format][0];
					else 
						sort = list_formats[list_format][sort_field+1];
				}
				if(sort == undefined)
					list = orglist;
				else
					lib.sort(list, sort);
				break;
				console.clearline();
				console.print("\1n\1y\1hSort: ");
				console.mnemonics("~Name, ~Sysop, ~Address, ~Location, or [None]: ");
				switch(console.getkey(K_UPPER)) {
					case 'N':
						lib.sort(list, "name");
						break;
					case 'S':
						lib.sort(list, "sysop_name");
						break;
					case 'A':
						lib.sort(list, "service_address");
						break;
					case 'L':
						lib.sort(list, "location");
						break;
					default:
						list = orglist;
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
				help();
				break;
			default:
				if(key>='0' && key <='9')
					list_format = parseInt(key);
				break;
		}
	}
}

function view(list, current)
{
	console.line_counter = 0;
	console.clear(LIGHTGRAY);
	while(!js.terminated) {
		var bbs = list[current];
		console.clear();
		console.attributes = BLACK | BG_LIGHTGRAY;
		console.print(version_notice);
		console.cleartoeol();
		console.crlf();

		var fmt = "\1n\1g%11s \1h%-*.*s";
		printf(fmt, "BBS", lib.max_len.name, lib.max_len.name, bbs.name);
		if(bbs.first_online)
			printf("\1n\1g since \1h%s", bbs.first_online.substring(0,10));
		console.crlf();
		for(i in bbs.sysop) {
			printf(fmt, "Operator", lib.max_len.sysop_name, lib.max_len.sysop_name, bbs.sysop[i] ? bbs.sysop[i].name : "");
			if(bbs.sysop[i].email)
				printf("\1n\1g email \1h%s", bbs.sysop[i].email);
			console.crlf();
		}
		if(bbs.location) {
			printf(fmt, "Location", lib.max_len.location, lib.max_len.location, bbs.location);
		}
		if(bbs.software && bbs.software.bbs) {
			printf(fmt, " Software", lib.max_len.bbs_software, lib.max_len.bbs_software, bbs.software.bbs);
		}
		console.crlf();
		if(bbs.web_site) {
			printf(fmt, "Web-site", lib.max_len.web_site, lib.max_len.web_site, bbs.web_site);
			console.crlf();
		}
		for(var i = 0; i < sbl_defs.DESC_LINES; i++) {
			printf(fmt, i ? "" : "Description", 50, 50, bbs.description[i] ? bbs.description[i] : "");
			console.crlf();
		}
		for(i in bbs.service) {
			printf("\1n\1g%11s \1h%s", bbs.service[i].protocol, bbs.service[i].address);
			if(bbs.service[i].protocol != 'modem' && bbs.service[i].port != standard_service_port[bbs.service[i].protocol])
				printf("\1n\1g:%u", bbs.service[i].port);
			if(bbs.service[i].description)
				printf(" %s", bbs.service[i].description);
			if(bbs.service[i].min_rate && bbs.service[i].max_rate)
				printf(" %u-%ubps", bbs.service[i].min_rate, bbs.service[i].max_rate);
			else if(bbs.service[i].min_rate)
				printf(" %u+bps");
			else if(bbs.service[i].max_rate)
				printf(" %ubps");
			console.crlf();
		}
		var networks="";
		for(i in bbs.network) {
			if(i > 0)
				networks += ",";
			networks += bbs.network[i].name;
			if(bbs.network[i].address)
				networks += format("[%s]", bbs.network[i].address);
		}
		if(networks)
			printf("\1n\1g%11s \1h%s\r\n", "Networks", networks);
		if(bbs.entry) {
			if(bbs.entry.created)
				printf("\1n\1g%11s \1h%s\1n\1g on \1h%s \1n\1g%s\r\n", "Created by"
					,bbs.entry.created.by, bbs.entry.created.on.substr(0,10)
					,bbs.imported ? "from msg-network" : "locally");
			if(bbs.entry.updated)
				printf("\1n\1g%11s \1h%s\1n\1g on \1h%s\r\n", "Updated by", bbs.entry.updated.by, bbs.entry.updated.on.substr(0,10));
			if(bbs.entry.verified)
				printf("\1n\1g%11s \1h%s\1n\1g on \1h%s\r\n", "Verified by", bbs.entry.verified.by, bbs.entry.verified.on.substr(0,10));
		}
		
		while(console.line_counter < console.screen_rows - 2)
			console.clearline(), console.crlf();

		if(bbs.preview)
			console.mnemonics("~Preview, ");
		console.mnemonics("~Edit, ~Verify, ~Delete, ~Forward/~Back or ~Quit: ");
		var key = console.getkey(K_UPPER);
		switch(key) {
			case 'P':
				if(!bbs.preview) {
					console.beep();
					break;
				}
				console.clear();
				var graphic=new Graphic();
				graphic.base64_decode(bbs.preview);
				graphic.draw();
				console.getkey();
				console.clear();
				break;
			case 'F':
				if(current < list.length - 1)
					current++;
				else
					console.beep();
				break;
			case 'B':
				if(current)
					current--;
				else
					console.beep();
				break;
			case 'Q':
				return;
		}
	}
}

function main()
{
    var i,j;
    var list;
    var verbose = 0;
    var msgbase;
	var optval={};
	var cmds=[];

    print(version_notice);

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
			default:
				cmds.push(arg);
				break;
        }
    }

    if(!file_exists(lib.list_fname))
        list=upgrade_list(sbl_dir + "sbl.dab");
    else
        list=lib.read_list();

	for(var c in cmds) {
		var cmd = cmds[c].toLowerCase();
		switch(cmd) {
			case "upgrade":
                list=upgrade_list(sbl_dir + "sbl.dab");
                print(list.length + " BBS entries upgraded from " + sbl_dir + "sbl.dab");
				break;
			case "backup":
				file_backup(lib.list_fname);
				break;
			case "import":
			case "export":
				var msgbase = new MsgBase(options.sub);
				print("Opening msgbase " + msgbase.file);
				if(!msgbase.open()) {
					alert("Error " + msgbase.error + " opening msgbase: " + msgbase.file);
					exit(-1);
				}
				if(cmd == "import")
					import_from_msgbase(list, msgbase);
				else
					export_to_msgbase(list, msgbase);
				msgbase.close();
				break;
			case "syncterm":
				print(list.length + " BBS entries exported to: " + lib.syncterm_list(list));
				break;
			case "html":
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
					ibbs.push(format("%-63s %s\t%s\t%s", 
						bbs.entry.autoverify.last_success.service.address, 
						bbs.entry.autoverify.last_success.ip_address,
						bbs.entry.autoverify.last_success.other_services.udp.sort(),
						bbs.entry.autoverify.last_success.other_services.tcp.sort()));
				}
				ibbs = unique_strings(ibbs, /* offset: */64);
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
			case "find":
				print("Searching for " + optval["find"]);
				list = lib.find(list, optval["find"]);
				print(list.length + " entries found");
				break;
			case "verify":
		        verify_list(list);
		        lib.write_list(list);
				break;
			case "browse":
				browse(list);
				break;
			case "save":
				lib.write_list(list);
				murph();
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
				list_format = parseInt(optval["list"]);
				for(i in list) {
					list_bbs_entry(list[i], false);
					writeln();
				}
				break;
			case "add":
				if(lib.system_exists(list, system.name)) {
					alert("System '" + system.name + "' already exists");
					exit(-1);
				}
				var bbs = lib.new_system(system.name, system.nodes, lib.system_stats());
				bbs.sysop.push({ name: system.operator, email: 'sysop@'+system.inet_addr });
				bbs.software = { bbs: "Synchronet " + system.version} ;
				bbs.terminal.support.push("TTY", "ANSI");
				while((str=prompt("Description"))) {
					bbs.description.push(str);
				}
				bbs.service.push({ protocol: 'telnet', address: system.inet_addr, port: standard_service_port['telnet'] });
				lib.add_system(list, bbs, system.operator);
				lib.write_list(list);
				print("System added successfully");
				break;
			case "update":
				var index = lib.system_index(list, system.name);
				if(index < 0) {
					alert("System '" + system.name + "' does not exist");
					exit(-1);
				}
				var bbs=list[i];
				bbs.software = { bbs: "Synchronet " + system.version} ;
				bbs.total = lib.system_stats();
				bbs.terminal.nodes = system.nodes;
				lib.update_system(bbs, js.exec_file + " " + REVISION);
				lib.write_list(list);
				print("System updated successfully");
				break;
		}
    }
}

main();
