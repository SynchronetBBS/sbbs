// $Id$

// Synchronet BBS List

// This one script replaces (or *will* replace) the functionality of:
// sbl[.exe]        - External online program (door)
// smb2sbl[.exe]    - Imports BBS entries from Synchronet Message Base (e.g. from SYNCDATA echo)
// sbl2smb[.exe]    - Exports BBS entries to Synchronet Message Base (e.g. to SYNCDATA echo)
// sbbslist[.exe]   - Exports BBS entries to HTML and various plain-text formats (e.g. sbbs.lst, sbbsimsg.lst, syncterm.lst)

var REVISION = "$Revision$".split(' ')[1];
var sbl_dir = "../xtrn/sbl/";

var options={sub:"syncdata"};
opts=load(new Object, "modopts.js", "sbbslist");
if(this.opts && opts.sub)
    options.sub=opts.sub;

var lib = {};
load(lib, "sbbslist_lib.js");

var sort_property = 'name';

function compare(a, b)
{
	if(a[sort_property].toLowerCase()>b[sort_property].toLowerCase()) return 1; 
	if(a[sort_property].toLowerCase()<b[sort_property].toLowerCase()) return -1;
	return 0;
}

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

    body += "\r\n<json>\r\n";
    body += JSON.stringify(bbs, null, 1) + "\r\n";
    body += "</json>\r\n";
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
        if(text[i].toLowerCase()=="<json>")
            json_begin=i+1;
        else if(text[i].toLowerCase()=="</json>")
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
                bbs.service[number] = {address: match[2], protocol: 'telnet', port: 23};
                break;
            case 'minrate':
                var minrate = parseInt(match[2], 10);
                if(minrate == 0xffff) {
                    bbs.service[number].protocol = 'telnet';
                    bbs.service[number].port = 23;
                } else
                    bbs.service[number].minrate = minrate;
                break;
            case 'maxrate':
                if(bbs.service[number].protocol == 'telnet')
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
            list[l] = {name: bbs_name, entry: {created: {by:msg_from, on:new Date() } } };
        }
        var body = msgbase.get_msg_body(/* by_offset: */true, i
            ,/* strip Ctrl-A */true, /* rfc822-encoded: */false, /* include tails: */false);
        import_entry(list[l], body);
        list[l].imported = true;
        if(!list[l].birth)
            list[l].birth=list[l].entry.created.on;
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
    while(--i) {
        if(obj.description[i].length)
            break;
    }
    obj.description.length=i+1;
    obj.terminal.nodes = f.readBin(2);
    obj.total.users = f.readBin(2);
    obj.total.subs = f.readBin(2);
    obj.total.dirs = f.readBin(2);
    obj.total.xtrns = f.readBin(2);
    obj.entry.created.on = new Date(f.readBin(4)*1000);
    var updated = f.readBin(4);
    if(updated)
        obj.entry.updated = { on: new Date(updated*1000) };
    obj.first_online = new Date(f.readBin(4)*1000);
    obj.total.storage = f.readBin(4)*1024*1024;
    obj.total.msgs = f.readBin(4);
    obj.total.files = f.readBin(4);
    obj.imported = Boolean(f.readBin(4));
    for(i=0;i<sbl_defs.MAX_NUMBERS;i++) {
        obj.service[i] = { address: truncsp(f.read(29)),  protocol: 'modem' };
        var location = truncsp(f.read(31));
        var min_rate = f.readBin(2)
        var port = f.readBin(2);
        if(min_rate==0xffff) {
            obj.service[i].protocol = 'telnet';
            obj.service[i].port = port;
        } else {
            obj.service[i].min_rate = min_rate;
            obj.service[i].max_rate = port;
        }
        if(obj.location==undefined || obj.location.length==0)
            obj.location=location;
    }
    obj.service.length = total_numbers;
    var updated_by = truncsp(f.read(26));
    if(obj.entry.updated)
        obj.entry.updated.by = updated_by;
    obj.entry.verified = { on: new Date(f.readBin(4)*1000), by: truncsp(f.read(26)) };
    obj.web_site = truncsp(f.read(61));
    var sysop_email = truncsp(f.read(61));
    if(obj.sysop.length)
        obj.sysop[0].email = sysop_email;
    f.readBin(4);   // 'exported' not used, always zero 
    obj.entry.verification = { count: f.readBin(4), attempts: f.readBin(4) };
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

function main()
{
    var i;
    var list;
    var import_now = false;
    var export_now = false;
    var show_now = false;
    var dump_now = false;
    var sort = false;
    var msgbase;

    print("Synchronet BBS List v4 Rev " + REVISION);

    for(i in argv) {
        switch(argv[i]) {
            case "upgrade":
                upgrade_list(sbl_dir + "sbl.dab");
                exit();
            case "import":
                import_now = true;
                break;
            case "export":
                export_now = true;               
                break;
            case "dump":
                dump_now = true;
            case "show":
                show_now = true;
                break;
            case "backup":
                file_backup(lib.list_fname);
                break;
            case "-sort":
                sort = true;
                if(i+1<argc)
                    sort_property=argv[++i];
                break;
        }
    }

    if(!file_exists(lib.list_fname))
        list=upgrade_list(sbl_dir + "sbl.dab");
    else
        list=lib.read_list();

    if(import_now || export_now) {
        msgbase = new MsgBase(options.sub);
        print("Opening msgbase " + msgbase.file);
        if(!msgbase.open()) {
            alert("Error " + msgbase.error + " opening msgbase: " + msgbase.file);
            exit(-1);
        }
        if(import_now)
            import_from_msgbase(list, msgbase);
        if(export_now)
            export_to_msgbase(list, msgbase);
        msgbase.close();
    }

    if(sort) {
        print("Sorting list by property: " + sort_property);
        list.sort(compare);
    }

    if(show_now) {
        for(i in list) {
            if(dump_now) {
                for(j in list[i])
                    print(j + " : " + list[i][j]);
                continue;
            }
            printf("%-25s %-25s %-25s", list[i].name, list[i].entry.created.by, list[i].sysop[0] ? list[i].sysop[0].name: '');
            if(list[i].entry.updated)
                print(new Date(list[i].entry.updated.on).toISOString());
            else
                print();
        }
        print(list.length + " BBS entries");
    }
}

main();