// $Id$

var REVISION = "$Revision$".split(' ')[1];
load('sbbsdefs.js');
load("lz-string.js");
var lib = load({}, 'avatar_lib.js');
var SAUCE = load({}, 'sauce_lib.js');

const user_avatars = 'SBBS User Avatars';
const shared_avatars = 'SBBS Shared Avatars';

function parse_user_msg(text)
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

function parse_file_msg(text)
{
	var i;
    var bin_begin;
    var bin_end;

	// Terminate at tear-line
    text=text.split("\r\n");
    for(i=0; i<text.length; i++) {
        if(text[i]=="---" || text[i].substring(0,4)=="--- ")
            break;
    }
    text.length=i;

	// Parse JSON block
    for(i=0; i<text.length; i++) {
        if(text[i].toLowerCase()=="bin-lz-begin")
            bin_begin=i+1;
        else if(text[i].toLowerCase()=="bin-lz-end")
            bin_end=i;
    }
    if(bin_begin && bin_end > bin_begin) {
        text = text.splice(bin_begin, bin_end-bin_begin);
		return LZString.decompressFromBase64(text.join('').replace(/\s+/g, ''));
    }
	return false;
}


function find_name(objs, name)
{
	for(var i=0; i < objs.length; i++)
		if(objs[i].name.toLowerCase() == name.toLowerCase())
			return i;
	return -1;
}

function import_netuser_list(hdr, list)
{
	var objs = [];
	var file = new File(lib.netuser_fname(hdr.from_net_addr));
	if(file.open("r")) {
		objs = file.iniGetAllObjects();
		file.close();
	}
	for(var i in list) {
		for(var u in list[i]) {
			var index = find_name(objs, list[i][u]);
			if(index >= 0) {
				if(objs[index].data != i) {
					objs[index].data = i;
					objs[index].updated = new Date();
				}
			} else
				objs.push({ name: list[i][u], data: i, created: new Date() });
		}
	}
	if(!file.open("w"))
		return false;
	file.writeln(format(";from %s at %s (%s) on %s"
		, hdr.from, hdr.subj, hdr.from_net_addr, new Date().toISOString()));
	var result = file.iniSetAllObjects(objs);
	file.close();
	return result;
}

function valid_shared_file(filename)
{
	if(!file_exists(filename)) {
		alert(filename + " does not exist");
		return false;
	}
    var sauce = SAUCE.read(filename);
    if(!sauce) {
        alert(filename + " has no SAUCE!");
        return false;
    }
    if(sauce.datatype != SAUCE.defs.datatype.bin
		|| sauce.cols != lib.defs.width
		|| sauce.filesize < lib.size
		|| (sauce.filesize%lib.size) != 0) {
        alert(format("%s has invalid SAUCE! (datatype=%u cols=%u size=%u)"
			,filename, sauce.datatype, sauce.cols, sauce.filesize));
        return false;
    }
	for(var i in sauce.comment) {
		if(strip_ctrl(sauce.comment[i]) != sauce.comment[i]) {
			alert(format("%s has invalid SAUCE comment [%u]!", filename, i));
			return false;
		}
	}
	var file = new File(filename);
	if(!file.open("rb"))
		return false;
	var data = file.read(sauce.filesize);
	file.close();
	var list = data.match(new RegExp('([\x00-\xff]{1,' + lib.size + '})', 'g'));
	printf("%u avatars\r\n", list.length);
//	print(JSON.stringify(list, null, 4));
	for(var i in list)
		if(!lib.is_valid(list[i])) {
			alert("Avatar " + i + " (" + list[i].length + " bytes) is invalid");
			return false;
		}
    return true;
}

function import_shared_file(hdr, body)
{
	var data = parse_file_msg(body);
	if(!data)
		return false;

	var filename = format("%s.%s", hdr.from_net_addr, file_getname(hdr.subject));
	if(file_getext(filename).toLowerCase() != '.bin')
		filename += '.bin';

	var file = new File(format("%sqnet/%s", system.data_dir, filename));
	if(!file.open("wb")) {
		alert("ERROR " + file.error + " opening " + file.name);
		return false;
	}
	file.write(data);
	file.close();
	print(file.name + " created successfully");
	if(!valid_shared_file(file.name))
	    return false;
	var new_path = format("%s%s", lib.local_library(), filename);
	var result = file_copy(file.name, new_path);
	if(!result)
		alert("ERROR copying " + file.name + " to " + new_path);
	return result;
}

function import_from_msgbase(msgbase, import_ptr, limit, all)
{
    var i;
    var count=0;
    var highest;
    var users_crc=crc16_calc(user_avatars.toLowerCase());
	var shared_crc=crc16_calc(shared_avatars.toLowerCase());

    var ini = new File(msgbase.file + ".ini");
	if(import_ptr == undefined) {
	    print("Opening " + ini.name);
		if(ini.open("r")) {
			import_ptr=ini.iniGetValue("avatars","import_ptr", 0);
			ini.close();
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
//            print("Error " + msgbase.error + " reading index of msg offset " + i);
            continue;
        }
        if(idx.number <= import_ptr)
            continue;
        if(idx.to != users_crc && idx.to != shared_crc) {
            continue;
        }
        if(idx.number > highest)
            highest = idx.number;
        var hdr = msgbase.get_msg_header(/* by_offset: */true, i);
		if(all != true && !hdr.from_net_type)	// Skip locally posted messages
			continue;
		printf("Importing msg #%u from %s: ", hdr.number, hdr.from);
		var success = false;
		var body = msgbase.get_msg_body(/* by_offset: */true, i
			,/* strip Ctrl-A */true, /* rfc822-encoded: */false, /* include tails: */false);
		if(hdr.to.toLowerCase() == user_avatars.toLowerCase()) {
			var l;
			var avatars = parse_user_msg(body);
			if(!avatars)
				continue;
			success = import_netuser_list(hdr, decompress_list(avatars));
		} else {
			// Shared avatars
			success = import_shared_file(hdr, body);
		}
		printf("%s\r\n", success ? "success" : "FAILED");
		if(success)
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
	for(var i in list) {
		var data = LZString.decompressFromBase64(i.replace(/\s+/g, ''));
		if(lib.is_valid(data))
			new_list[base64_encode(data)] = list[i];
	}
	return new_list;
}

function export_users(msgbase, realnames)
{
	var last_user = system.lastuser;
	var list = {};
	
	for(var n = 1; n <= last_user && !js.terminated; n++) {
		if(!file_exists(lib.localuser_fname(n)))
			continue;
		if(!system.username(n))
			continue;
		var u = new User(n);
		if(u.settings&USER_DELETED)
			continue;
		var avatar = lib.read_localuser(n);
		if(!avatar)
			continue;
		var data = LZString.compressToBase64(base64_decode(avatar.data));
		if(!list[data])
			list[data] = [];
		list[data].push(u.alias);
		if(realnames)
			list[data].push(u.name);
	}
	if(!list.length)
		return true;	// Nothing to export
	for(var i in list)
		list[i].sort();
	var body = "json-begin\r\n";
	body += JSON.stringify(list, null, 1) + "\r\n";
	body += "json-end\r\n";
	body += "--- " + js.exec_file + " " + REVISION + "\r\n";
	return msgbase.save_msg({ to:user_avatars, from:system.operator, subject:system.name }, body);
}

function export_file(msgbase, filename)
{
	var file = new File(filename);
	if(!file.open("rb")) {
		alert("Error " + file.error + " opening " + filename);
		return false;
	}
	var data = file.read();
	file.close();
	data = LZString.compressToBase64(data);
	var body = "";
	body += "sauce-json-begin\r\n";
	body += JSON.stringify(SAUCE.read(file.name), null, 1) + "\r\n";
	body += "sauce-json-end\r\n";
	body += "bin-lz-begin\r\n";
	body += data.match(/([\x00-\xff]{1,72})/g).join("\r\n");
	body += "\r\nbin-lz-end\r\n";
	body += "--- " + js.exec_file + " " + REVISION + "\r\n";
	return msgbase.save_msg({ to:shared_avatars, from:system.operator, subject:file_getname(filename) }, body);
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
	var all;
	var users = false;

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
			case '-file':
				filename = val;
				break;
			case '-offset':
				offset = val;
				break;
			case '-users':
				users = true;
				break;
			case '-realnames':
				realnames = true;
				break;
			case "-ptr":
				ptr = val;
				break;
			case "-all":
				all = true;
				break;
			default:
				if(parseInt(arg) < 0)
					limit = -parseInt(arg);
				else
					cmds.push(arg);
				break;
		}
	}
	mkdir(lib.local_library());
	for(var c in cmds) {
		var cmd = cmds[c].toLowerCase();
		switch(cmd) {
			case "import":
				if(filename && parseInt(optval[cmd])) {
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
				import_from_msgbase(msgbase, ptr, limit, all);
				msgbase.close();
				break;
			case "export":
				var msgbase = new MsgBase(optval[cmd]);
				print("Opening msgbase " + msgbase.file);
				if(!msgbase.open()) {
					alert("Error " + msgbase.error + " opening msgbase: " + msgbase.file);
					exit(-1);
				}
				var success = true;
				if(users) {
					printf("Exporting user avatars\n");
					success = export_users(msgbase, realnames);
				}
				if(success && filename) {
				    printf("Exporting avatar file: %s\n", filename);
				    if(!valid_shared_file(filename))
				        success = false;
                    else
					    success = export_file(msgbase, filename);
				}
				printf("%s\r\n", success ? "Successful" : "FAILED: " + msgbase.last_error);
				break;
			case "dump":
				var usernum = optval[cmd];
				if(!usernum)
					usernum = user.number;
				var obj = lib.read_localuser(usernum);
				print(JSON.stringify(obj));
				break;
			case "draw":	// Uses Graphic.draw()
				var usernum = optval[cmd];
				if(!usernum)
					usernum = user.number;
				console.clear();
				var obj = lib.draw(usernum);
				break;
			case "show":	// Uses console.write()
				var usernum = optval[cmd];
				if(!usernum)
					usernum = user.number;
				var obj = lib.show(usernum);
				break;
			case "verify":
				if(filename) {
				    var success = valid_shared_file(filename);
					print(success ? "Successful" : "FAILED");
				}
				break;
			case "remove":
				var usernum = parseInt(optval[cmd]);
				if(usernum) {
					printf("Removing user #%u avatar\n", usernum);
					var success = lib.remove_localuser(usernum);
					print(success ? "Successful" : "FAILED");
				}
				break;
			case "enable":
			case "disable":
				var usernum = parseInt(optval[cmd]);
				if(usernum) {
					var success = lib.enable_localuser(usernum, cmd == "enable");
					print(success ? "Successful" : "FAILED");
				}
				break;
		}
	}
}

main();