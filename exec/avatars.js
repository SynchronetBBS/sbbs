// $Id$

var REVISION = "$Revision$".split(' ')[1];
load('sbbsdefs.js');
load("lz-string.js");
var lib = load({}, 'avatar_lib.js');

const to_name = 'SBBS Avatars';

function parse_msg(text)
{
	var i;
    var json_begin;
    var json_end;

	// Terminate at tear-line
    text=text.split("\r\n");
    for(i=0; i<text.length; i++) {
        if(text[i]=="---" || text[i].substring(0,4)=="--- ")
            break;
    }
    text.length=i;

	// Parse JSON block
    for(i=0; i<text.length; i++) {
        if(text[i].toLowerCase()=="json-begin")
            json_begin=i+1;
        else if(text[i].toLowerCase()=="json-end")
            json_end=i;
    }
    if(json_begin && json_end > json_begin) {
        text=text.splice(json_begin, json_end-json_begin);
        try {
            if((obj = JSON.parse(text.join(''))) != undefined)
                return obj;
        } catch(e) {
            alert("Error " + e + " parsing JSON");
        }
        return false;
    }
	return false;
}


function import_from_msgbase(msgbase, import_ptr, limit, all)
{
    var i;
    var count=0;
    var highest;
    var avatars_crc=crc16_calc(to_name.toLowerCase());

    var ini = new File(msgbase.file + ".ini");
	if(import_ptr == undefined) {
	    print("Opening " + ini.name);
		if(ini.open("r")) {
			import_ptr=ini.iniGetValue("avatars","import_ptr", 0);
			ini.close();
		} else if(debug)
			print("Error " + ini.error + " opening " + ini.name);
	}
    highest=import_ptr;
    print("import_ptr = " + import_ptr);
    for(i=0; i<msgbase.total_msgs; i++) {
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
        if(idx.to != avatars_crc) {
            continue;
        }
        if(idx.number > highest)
            highest = idx.number;
        var hdr = msgbase.get_msg_header(/* by_offset: */true, i);
		if(hdr.to.toLowerCase() != to_name.toLowerCase())
			continue;
        var l;
        var msg_from = hdr.from;
		if(all != true && !hdr.from_net_type)	// Skip locally posted messages
			continue;

        var body = msgbase.get_msg_body(/* by_offset: */true, i
            ,/* strip Ctrl-A */true, /* rfc822-encoded: */false, /* include tails: */false);
		var avatars = parse_msg(body);
		if(!avatars)
			continue;
		list = decompress_list(avatars);
		import_list(hdr.from_net_addr, avatars);
        count++;
		if(limit && count >= limit)
			break;
    }

    if(ini.open(file_exists(ini.name) ? 'r+':'w+')) {
        print("new import_ptr = " + highest);
        ini.iniSetValue("avatars","import_ptr",highest);
        ini.close();
    } else
        print("Error opening/creating " + ini.name);
    print("Imported " + count + " messages");
}

function decompress_list(list)
{
	var new_list = [];
	for(var i in list)
		new_list[base64_encode(LZString.decompressFromBase64(i.replace(/\s+/g, '')))] = list[i];
	return new_list;
}

function main()
{
	var optval={};
	var cmds=[];
	var i;
	var filename;
	var offset;
	var realnames = false;
	var ptr;
	var limit;

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
			case '-f':
				filename = val;
				break;
			case '-offset':
				offset = val;
				break;
			case '-realnames':
				realnames = true;
				break;
			case "-ptr":
				ptr = val;
				break;
			default:
				if(parseInt(arg) < 0)
					limit = -parseInt(arg);
				else
					cmds.push(arg);
				break;
		}
	}

	for(var c in cmds) {
		var cmd = cmds[c].toLowerCase();
		switch(cmd) {
			case "import":
				if(filename) {
					printf("Importing %s for user #%u\r\n", filename, optval[cmd]);
					var success = lib.import_file(optval[cmd], filename, offset);
					printf("%s\r\n", success ? "Successful" : "FAILED!");
					break;
				}
				var msgbase = new MsgBase(optval[cmd]);
				print("Opening msgbase " + msgbase.file);
				if(!msgbase.open()) {
					alert("Error " + msgbase.error + " opening msgbase: " + msgbase.file);
					exit(-1);
				}
				import_from_msgbase(msgbase, ptr, limit);
				msgbase.close();
				break;
			case "import_msg":
				var file = new File(filename);
				if(!file.open("r")) {
					alert("failure opening " + file.name);
					return 1;
				}
				var msg = file.read();
//				print(msg);
				var list = parse_msg(lfexpand(msg));
				print(JSON.stringify(list, null, 1));
				list = decompress_list(list);
				var success = lib.import_list(optval[cmd], list);
				printf("%s\r\n", success ? "Successful" : "FAILED!");
				break;
			case "export":
				var msgbase = new MsgBase(optval[cmd]);
				print("Opening msgbase " + msgbase.file);
				if(!msgbase.open()) {
					alert("Error " + msgbase.error + " opening msgbase: " + msgbase.file);
					exit(-1);
				}
				var last_user = system.lastuser;
				var list = {};
				var count = 0;
				for(var n = 1; n <= last_user && !js.terminated; n++) {
					if(!file_exists(lib.local_fname(n)))
						continue;
					if(!system.username(n))
						continue;
					var u = new User(n);
					if(u.settings&USER_DELETED)
						continue;
					var avatar = lib.read_local(n);
					if(!avatar)
						continue;
					var data = LZString.compressToBase64(base64_decode(avatar.data));
					if(!list[data])
						list[data] = [];
					list[data].push(u.alias);
					if(realnames)
						list[data].push(u.name);
					count++;
					if(limit && count > limit)
						break;
				}
				for(var i in list)
					list[i].sort();
				var body = "json-begin\r\n";
				body += JSON.stringify(list, null, 1) + "\r\n";
				body += "json-end\r\n";
				body += "--- " + js.exec_file + " " + REVISION + "\r\n";
			    success = msgbase.save_msg({ to:to_name, from:system.operator, subject:system.name + ' Avatars' }, body);
				printf("%s\r\n", success ? "Successful" : "FAILED: " + msgbase.last_error);
				break;
			case "dump":
				var obj = lib.read_local(optval[cmd]);
				print(JSON.stringify(obj));
				break;
		}
	}
}

main();