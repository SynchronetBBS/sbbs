var REVISION = "1.40";

load('sbbsdefs.js');
load("lz-string.js");
var lib = load({}, 'avatar_lib.js');
var SAUCE = load({}, 'sauce_lib.js');
var Graphic = load({}, 'graphic.js');
var fidoaddr = load({}, 'fidoaddr.js');

var export_freq = 7;	// minimum days between exports
var verbosity = 0;

var options=load({}, "modopts.js", "avatars");
if(!options)
	options = {};
if(!options.sub)
    options.sub = load({}, "syncdata.js").find();
if(options && options.export_freq > 0)
	export_freq = options.export_freq;

const user_avatars = 'SBBS User Avatars';
const shared_avatars = 'SBBS Shared Avatars';
const EXCLUDE_FILES = /\.\d+\.bin$/;		// Don't include Pablodraw backups in shared collections

function parse_user_msg(text)
{
	var i;
    var json_begin;
    var json_end;

	// Terminate at tear-line
    text=text.split("\n");
    for(i=0; i<text.length; i++) {
        if(text[i].trimRight()=="---" || text[i].substring(0,4)=="--- ")
            break;
    }
    text.length=i;

	// Parse JSON block
    for(i=0; i<text.length; i++) {
        if(text[i].trimRight().toLowerCase()=="json-begin")
            json_begin=i+1;
        else if(text[i].trimRight().toLowerCase()=="json-end")
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
	alert("invalid or missing JSON block, length: " + text.length
		+ ", begin: " + json_begin + ", end: " + json_end);
	return false;
}

function parse_file_msg(text)
{
	var i;
    var bin_begin;
    var bin_end;

	// Terminate at tear-line
    text=text.split("\n");
    for(i=0; i<text.length; i++) {
        if(text[i].trimRight()=="---" || text[i].substring(0,4)=="--- ")
            break;
    }
    text.length=i;

	// Parse JSON block
    for(i=0; i<text.length; i++) {
        if(text[i].trimRight().toLowerCase()=="bin-lz-begin")
            bin_begin=i+1;
        else if(text[i].trimRight().toLowerCase()=="bin-lz-end")
            bin_end=i;
    }
    if(bin_begin && bin_end > bin_begin) {
        text = text.splice(bin_begin, bin_end-bin_begin);
		return LZString.decompressFromBase64(text.join('').replace(/\s+/g, ''));
    }
	alert("invalid or missing bin-lz block, length: " + text.length
		+ ", begin: " + bin_begin + ", end: " + bin_end);
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
	var fname = lib.netuser_fname(hdr.from_net_addr);
	if(!fname)
		return false;
	var file = new File(fname);
	if(file.open("r")) {
		objs = file.iniGetAllObjects();
		file.close();
	} else if(file_exists(file.name)) {
		alert("Error " + file.error + " opening " + file.name);
		return false;
	}
	for(var i in list) {
		for(var u in list[i]) {
			if(system.findstr(system.ctrl_dir + "twitlist.cfg", list[i][u])) {
				alert("Ignoring twit-listed user: " + list[i][u]);
				continue;
			}
			var index = find_name(objs, list[i][u]);
			if(index >= 0) {
				if(verbosity)
					printf("%s = %s\r\n", objs[index].name, i);
				if(i == "disabled")
					objs.splice(index, 1);	// Remove rather than just mark as disabled
				else if(objs[index].data != i) {
					objs[index].data = i;
					objs[index].updated = new Date();
				}
			} else if(i != "disabled")
				objs.push({ name: list[i][u], data: i, created: new Date() });
		}
	}
	printf("parsed %u avatars", objs.length);
	if(!file.open("w+")) {
		alert("Error opening " + file.name);
		return false;
	}
	var result = file.iniSetAllObjects(objs);
	file.writeln(format(";updated by %s at %s (%s) on %s"
		, hdr.from, hdr.subject, hdr.from_net_addr, new Date()));
	file.close();
	if(verbosity)
		printf("%s written with %u avatars\r\n", file.name, objs.length);

	var bbses = new File(lib.bbsindex_fname());
	if(bbses.open(bbses.exists ? 'r+':'w+')) {
		bbses.iniSetValue(hdr.subject, "netaddr", hdr.from_net_addr);
		bbses.iniSetValue(hdr.subject, "updated", new Date());
		bbses.close();
	}
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
			alert("Avatar " + i + " " + sauce.comment[i] + " (" + list[i].length + " bytes) is invalid");
			return false;
		}
    return true;
}

function import_shared_file(hdr, body)
{
	var data = parse_file_msg(body);
	if(!data)
		return false;

	// If the filename (in the subject) already contains the sender's QWK-ID, skip-it
	var filename = file_getname(hdr.subject);
	var prefix = '';
	if(!fidoaddr.is_valid(hdr.from_net_addr))
		prefix = file_getname(hdr.from_net_addr) + '.';
	var suffix = '.bin';
	if(filename.length > prefix.length + suffix.length
		&& filename.substr(0, prefix.length).toLowerCase() == prefix.toLowerCase())
		filename = filename.slice(prefix.length);
	filename = prefix + filename;
	if(file_getext(filename).toLowerCase() != suffix)
		filename += suffix;

	var fullpath;
	if(fidoaddr.is_valid(hdr.from_net_addr))
		fullpath = format("%sfido/%s.%s", system.data_dir, fidoaddr.to_filename(hdr.from_net_addr), filename);
	else
		fullpath = format("%sqnet/%s", system.data_dir, filename);
	var file = new File(fullpath);
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
    var i = 0;
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
    log(LOG_DEBUG, "import_ptr = " + import_ptr + " last_msg = " + msgbase.last_msg);
	var total_msgs = msgbase.total_msgs;
	if(msgbase.last_msg >= import_ptr)
		i = total_msgs - (msgbase.last_msg - import_ptr);
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
        if(idx.to != users_crc && idx.to != shared_crc)
            continue;
        var hdr = msgbase.get_msg_header(/* by_offset: */true, i);
		if(!hdr)
			continue;
		if(all != true && !hdr.from_net_type)	// Skip locally posted messages
			continue;
		if(system.findstr(system.ctrl_dir + "twitlist.cfg", hdr.from)) {
			alert("Ignoring twit-listed sender: " + hdr.from);
			continue;
		}
		printf("Importing msg #%u from %s: ", hdr.number, hdr.from);
		var success = false;
		var body = msgbase.get_msg_body(/* by_offset: */true, i
			,/* strip Ctrl-A */true, /* rfc822-encoded: */false, /* include tails: */false);
		if(hdr.to.toLowerCase() == user_avatars.toLowerCase()) {
			var l;
			var avatars = parse_user_msg(body);
			if(avatars)
				success = import_netuser_list(hdr, decompress_list(avatars));
		} else {
			// Shared avatars
			success = import_shared_file(hdr, body);
		}
		printf(" %s\r\n", success ? "success" : "FAILED");
		if(success)
			count++;
		if(limit && count >= limit)
			break;
    }

	if(highest != import_ptr) {
		if(ini.open(file_exists(ini.name) ? 'r+':'w+')) {
			print("new import_ptr = " + highest);
			ini.iniSetValue("avatars","import_ptr",highest);
			ini.close();
		} else
			print("Error opening/creating " + ini.name);
	}
    print("Imported " + count + " messages");
}

function decompress_list(list)
{
	var new_list = [];
	for(var i in list) {
		if(i == "disabled") {
			new_list[i] = list[i];
			continue;
		}
		var data = LZString.decompressFromBase64(i.replace(/\s+/g, ''));
		if(lib.is_valid(data))
			new_list[base64_encode(data)] = list[i];
	}
	return new_list;
}

function export_users(msgbase, realnames, all)
{
	var last_user = system.lastuser;
	var list = {};
	var exported = 0;

	for(var n = 1; n <= last_user && !js.terminated; n++) {
		if(!file_exists(lib.localuser_fname(n)))
			continue;
		if(!system.username(n))
			continue;
		var u = new User(n);
		if((u.settings&USER_DELETED)
			|| !u.stats.total_posts			// No need to export avatars for users that have never posted
			|| (u.security.restrictions&(UFLAG_P|UFLAG_N|UFLAG_Q)) // or will never post
			) {
			if(verbosity)
				printf("User #%u hasn't or can't post, skipping\r\n", n);
			continue;
		}
		var avatar = lib.read_localuser(n);
		if(!avatar)
			continue;
		if(avatar.export_count == undefined)
			avatar.export_count = 0;
		var last_exported = 0;
		if(avatar.last_exported)
			last_exported = new Date(avatar.last_exported);
		if(u.stats.laston_date * 1000 < last_exported) {
			if(verbosity)
				printf("User #%u is inactive since: %s\r\n", n,new Date(u.stats.laston_date * 1000));
			continue;	// Don't export avatars of inactive users
		}
		var updated;
		if(avatar.updated)
			updated = new Date(avatar.updated);
		else
			updated = new Date(avatar.created);
		if(all == true
			|| updated > last_exported
			|| new Date() - last_exported >= export_freq * (24*60*60*1000)) {
			if(avatar.disabled == true) {
				if(!last_exported)
					continue;
				printf("Exporting disabled-avatar state for user #%u\r\n", n);
				if(!list.disabled)
					list.disabled = [];
				list.disabled.push(u.alias);
				if(u.name != u.alias) {
					if(realnames)
						list.disabled.push(u.name);
					else if(realnames !== false)
						list.disabled.push("md5:" + md5_calc(u.name));
				}
			} else {
				if(!lib.is_enabled(avatar))	{
					alert("Invalid avatar for user #" + n);
					continue;
				}
				printf("Exporting avatar for user #%u\r\n", n);
				var data = LZString.compressToBase64(base64_decode(avatar.data));
				if(!list[data])
					list[data] = [];
				list[data].push(u.alias);
				if(u.name != u.alias) {
					if(realnames)
						list[data].push(u.name);
					else if(realnames !== false)
						list[data].push("md5:" + md5_calc(u.name));
				}
			}
			avatar.last_exported = new Date();
			avatar.export_count++;
			lib.write_localuser(n, avatar);
			exported++;
		}
	}
	if(!exported) {
		print("No avatars to export");
		return true;	// Nothing to export
	}
	for(var i in list)
		list[i].sort();
	var body = "Timestamp: " + new Date() + "\r\n";
	body += "json-begin\r\n";
	body += lfexpand(JSON.stringify(list, null, 1)) + "\r\n";
	body += "json-end\r\n";
	body += "--- " + js.exec_file + " " + REVISION + "\r\n";
	var result = msgbase.save_msg({ to:user_avatars, from:system.operator, subject:system.name }, body);
	if(!result)
		alert("MsgBase error: " + msgbase.last_error);
	return result;
}

function export_file_to_msgbase(msgbase, filename)
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
	body += lfexpand(JSON.stringify(SAUCE.read(file.name), null, 1)) + "\r\n";
	body += "sauce-json-end\r\n";
	body += "bin-lz-begin\r\n";
	body += data.match(/([\x00-\xff]{1,72})/g).join("\r\n");
	body += "\r\nbin-lz-end\r\n";
	body += "--- " + js.exec_file + " " + REVISION + "\r\n";
	var result = msgbase.save_msg({ to:shared_avatars, from:system.operator, subject:file_getname(filename) }, body);
	if(!result)
		alert("MsgBase error: " + msgbase.last_error);
	return result;
}

function export_file(msgbase, filename)
{
	var last_exported = new Date(0);

    var ini = new File(msgbase.file + ".ini");
	if(ini.open("r")) {
		last_exported=ini.iniGetValue("avatars", file_getname(filename), last_exported);
		ini.close();
	}

	if(file_date(filename) < last_exported.valueOf() / 1000
		&& new Date() - last_exported < export_freq * (24*60*60*1000)) {
		alert("File not updated recently: " + filename);
		return false;
	}

	var success = export_file_to_msgbase(msgbase, filename);

	if(success) {
		if(ini.open(file_exists(ini.name) ? 'r+':'w+')) {
			ini.iniSetValue("avatars", file_getname(filename), new Date());
			ini.close();
		} else
			alert("Error opening/creating " + ini.name);
	}
	return success;
}

function install()
{
	if(!file_exists(lib.local_library() + "*.bin"))
		return "No avatars collections (.bin files) found in " + lib.local_library();
	var f = new File(system.ctrl_dir + "xtrn.ini");
	if(!f.open(f.exists ? 'r+':'w+'))
		return "Failed to open " + f.name;

	var section = "prog:MAIN:AVATCHOO";
	if(!f.iniGetValue(section, "cmd")) {
		printf("Adding external program: Avatar Chooser\r\n");
		f.iniSetObject(section, {
				"name": "Avatar Chooser",
				"ars": "",
				"execution_ars": "ANSI & !GUEST & REST ! Q",
				"type": 0,
				"settings": 1,
				"event": 3,
				"cost": 0,
				"cmd": "?avatar_chooser",
				"clean_cmd": "",
				"startup_dir": "",
				"textra": 0,
				"max_time": 0
				});
	}

	section = "event:AVAT-IN";
	if(!f.iniGetValue(section, "cmd")) {
		printf("Adding timed event: AVAT-IN\r\n");
		f.iniSetObject(section, {
				"cmd": "?avatars import",
				"days": 255,
				"time": 0,
				"node_num": 1,
				"settings": 0,
				"startup_dir": "",
				"freq": 30,		// interval
				"mdays": 0,
				"months": 0
				});
	}

	section = "event:AVAT-OUT";
	if(!f.iniGetValue(section, "cmd")) {
		printf("Adding timed event: AVAT-OUT\r\n");
		f.iniSetObject(section, {
				"cmd": "?avatars export",
				"days": 255,
				"time": 0,
				"node_num": 1,
				"settings": 0,
				"startup_dir": "",
				"freq": 1440,	// interval (once a day)
				"mdays": 0,
				"months": 0
				});
	}
	f.close();

	var ini = new File(file_cfgname(system.ctrl_dir, "modopts.ini"));
	if(!ini.open(file_exists(ini.name) ? 'r+':'w+'))
		return ini.name + " open error " + ini.error;
	printf("Updating %s\r\n", ini.name);
	if (ini.iniGetValue("newuser", "avatar_file") === undefined)
		ini.iniSetValue("newuser", "avatar_file", "silhouettes.bin");
	if (ini.iniGetValue("newuser", "avatar_offset") === undefined)
		ini.iniSetValue("newuser", "avatar_offset", 0);
	if (ini.iniGetValue("logon", "set_avatar") === undefined)
		ini.iniSetValue("logon", "set_avatar", true);
	ini.close();
	return true;
}

function main()
{
	var optval={};
	var cmd;
	var i;
	var offset;
	var realnames;
	var ptr;
	var limit;
	var all;
	var share = false;
	var files = [];
	var usernum;
	var cmds = [];

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
				files.push(val);
				break;
			case '-offset':
				offset = val;
				break;
			case '-realnames':
				realnames = true;
				break;
			case '-aliasonly':
				realnames = false;
				break;
			case "-ptr":
				ptr = val;
				break;
			case "-all":
				all = true;
				break;
			case "-share":
				share = true;
				break;
			case "-user":
				usernum = val;
				break;
			case "-v":
				verbosity++;
				break;
			case "import":
			case "export":
			case "dump":
			case "draw":
			case "show":
			case "verify":
			case "enable":
			case "disable":
			case "remove":
			case "newuser":
			case "install":
			case "normalize":
			case "count":
			case "colls":
			case "msg-default":
			case "msg_default":
				cmds.push(arg);
				break;
			default:
				if(parseInt(arg) < 0)
					limit = -parseInt(arg);
				else if(arg.charAt(0) != '-')
					files.push(arg);
				break;
		}
	}
	var used_dir =  [ lib.local_library(), system.data_dir + 'qnet', system.data_dir + 'fido' ];
	for(var i in used_dir) {
		if(file_isdir(used_dir[i]))
			continue;
		print("Making directory: " + used_dir[i]);
		if(!mkdir(used_dir[i]))
			alert("Failed to make directory: " + used_dir[i]);
	}

	for(var i in files)
		files[i] = lib.fullpath(files[i]);

	if(usernum && !parseInt(usernum))
		usernum = system.matchuser(usernum);
	else if(!usernum && user && user.number)
		usernum = user.number;

	for(var c in cmds) {
		var cmd = cmds[c].toLowerCase();
		switch(cmd) {
			case "import":
				if(files.length && parseInt(optval[cmd])) {
					printf("Importing %s for user #%u\r\n", files[0], optval[cmd]);
					var success = lib.import_file(optval[cmd], files[0], offset);
					printf("%s\r\n", success ? "Successful" : "FAILED!");
					break;
				}
				var msgbase = new MsgBase(optval[cmd] ? optval[cmd] : options.sub);
				print("Opening msgbase " + msgbase.file);
				if(!msgbase.open()) {
					alert("Error " + msgbase.error + " opening msgbase: " + msgbase.file);
					exit(-1);
				}
				import_from_msgbase(msgbase, ptr, limit, all);
				msgbase.close();
				break;
			case "export":
				var msgbase = new MsgBase(optval[cmd] ? optval[cmd] : options.sub);
				print("Opening msgbase " + msgbase.file);
				if(!msgbase.open()) {
					alert("Error " + msgbase.error + " opening msgbase: " + msgbase.file);
					exit(-1);
				}
				var success = true;
				if(!files.length) {
					printf("Exporting user avatars\n");
					success = export_users(msgbase, realnames, all);
				}
				for(var i in files) {
					printf("Exporting avatar collection: %s\n", files[i]);
					if(!valid_shared_file(files[i])) {
						success = false;
						break;
					}
					else {
						if(export_file(msgbase, files[i]))
							printf("Exported avatar collection: %s\n", files[i]);
					}
				}
				if(share) {
					var filespec = lib.local_library() + system.qwk_id + ".*.bin";
					print("Exporting shared avatar collections: " + filespec);
					var share_files = directory(filespec);
					for(var i in share_files) {
						if(share_files[i].search(EXCLUDE_FILES) >= 0) {
							log(LOG_DEBUG, format("Excluding file: %s", file_getname(share_files[i])));
							continue;
						}
						printf("Exporting: %s\r\n", file_getname(share_files[i]));
						if(!valid_shared_file(share_files[i]))
							continue;
						if(export_file(msgbase, share_files[i]))
							printf("Exported shared avatar collection: %s\n", share_files[i]);
					}
				}
				printf("%s\r\n", success ? "Successful" : "FAILED");
				break;
			case "dump":
				if(!usernum)
					usernum = optval[cmd];
				var obj = lib.read_localuser(usernum);
				print(JSON.stringify(obj));
				break;
			case "draw":	// Uses Graphic.draw()
			case "show":	// Uses console.write()
				if(files.length) {
					for(var i in files) {
						for(o=offset ? offset : 0; ; o++) {
							var data = lib.import_file(null, files[i], o);
							if(!data)
								break;
							console.clear();
							if(cmd == "draw") {
								lib.draw_bin(data);
								console.getkey();
							} else {
								lib.show_bin(data);
								console.pause();
							}
							console.attributes = 7;
						}
					}
					break;
				}
				if(!usernum)
					usernum = optval[cmd];
				console.clear();
				var obj = lib[cmd](usernum);
				console.getkey();
				console.clear(LIGHTGRAY);
				break;
			case "verify":
				for(var i in files) {
					printf("%s: ", file_getname(files[i]));
					var success = valid_shared_file(files[i]);
					print(success ? "Successful" : "FAILED");
				}
				break;
			case "remove":
				if(!usernum)
					usernum = optval[cmd];
				if(usernum) {
					printf("Removing user #%u avatar\n", usernum);
					var success = lib.remove_localuser(usernum);
					print(success ? "Successful" : "FAILED");
				}
				break;
			case "enable":
			case "disable":
				if(!usernum)
					usernum = optval[cmd];
				if(usernum) {
					var success = lib.enable_localuser(usernum, cmd == "enable");
					print(success ? "Successful" : "FAILED");
				}
				break;
			case "newuser":
				if(!files.length)
					files.push(optval[cmd]);
				if(!files.length) {
					alert("No file specified");
					break;
				}
				if(!file_exists(files[0])) {
					printf("File does not exist: %s\r\n", files[0]);
					break;
				}
				printf("Importing %s for new users\r\n", files[0]);
				var data = lib.import_file(null, files[0], offset);
				if(!data) {
					alert("Failed");
					break;
				}
				var ini = new File(file_cfgname(system.ctrl_dir, "modopts.ini"));
				if(!ini.open(file_exists(ini.name) ? 'r+':'w+')) {
					alert(ini.name + " open error " + ini.error);
					break;
				}
				var success = ini.iniSetValue("newuser", "avatar", data);
				if(success)
					ini.iniRemoveKey("newuser", "avatar_file");
				printf("%s\r\n", success ? "Successful" : "FAILED!");
				ini.close();
				break;
			case "msg-default":
			case "msg_default":
				if(!files.length)
					files.push(optval[cmd]);
				if(!files.length) {
					alert("No file specified");
					break;
				}
				if(!file_exists(files[0])) {
					printf("File does not exist: %s\r\n", files[0]);
					break;
				}
				printf("Importing %s sub-board default avatar\r\n", files[0]);
				var data = lib.import_file(null, files[0], offset);
				if(!data) {
					alert("Failed");
					break;
				}
				var ini = new File(file_cfgname(system.ctrl_dir, "modopts.ini"));
				if(!ini.open(file_exists(ini.name) ? 'r+':'w+')) {
					alert(ini.name + " open error " + ini.error);
					break;
				}
				var success = ini.iniSetValue("avatars", "msg_default", data);
				printf("%s\r\n", success ? "Successful" : "FAILED!");
				ini.close();
				break;
			case "normalize":
				var graphic = new Graphic(lib.defs.width, lib.defs.height);
				if(files.length) {
					if(!offset)
						offset = 0;
					var filename = lib.fullpath(files[0]);
					try {
						if(!graphic.load(filename, offset))
							break;
					} catch(e) {
						alert(e);
						break;
					}
					if(!lib.is_valid(graphic.BIN)) {
						alert(filename + " is not a valid avatar");
						break;
					}
					var file = new File(filename);
					if(!file.open("r+b")) {
						alert("Failed to open " + file.name);
						break;
					}
					file.position = offset * lib.size;
					file.write(graphic.normalize(optval[cmd]).BIN, lib.size);
					file.close();
					break;
				}
				if(!usernum)
					break;
				var avatar = lib.read_localuser(usernum);
				graphic.base64_decode([avatar.data]);
				lib.update_localuser(usernum, base64_encode(graphic.normalize(optval[cmd]).BIN));
				break;
			case "count":
				if(!files.length) {
					var total = 0;
					files = directory(lib.local_library() + "*.bin");
					for(var i in files) {
						var filename = files[i];
						if(filename.search(EXCLUDE_FILES) >= 0)
							continue;
						printf("%-32s : ", file_getname(filename));
						var sauce = SAUCE.read(filename);
						if(!sauce) {
							printf("no SAUCE\r\n");
							conintue;
						}
						var count = sauce.filesize / lib.size;
						printf("%u\r\n", count);
						total += count;
					}
					printf("%u total\r\n", total);
					break;
				}
			case "colls":
				if(!files.length) {
					var total = 0;
					files = directory(lib.local_library() + "*.bin");
					for(var i in files) {
						var filename = files[i];
						if(filename.search(EXCLUDE_FILES) >= 0)
							continue;
						printf("%-32s : ", file_getname(filename));
						var sauce = SAUCE.read(filename);
						if(!sauce) {
							printf("no SAUCE\r\n");
							conintue;
						}
						var count = sauce.filesize / lib.size;
						printf("%u\r\n", count);
						total += count;
						var f = new File(filename);
						if(!f.open('rb')) {
							alert("Error " + f.error + " opening " + f.name);
							continue;
						}
						var buf = f.read();
						f.close();
						for(var a = 0; a < count; a++) {
							var avatar = buf.substr(a * lib.size, lib.size);
							var b64 = base64_encode(avatar);
							printf("%s #%u: ", file_getname(filename), a+1);
							if(sauce.comment[a])
								print(sauce.comment[a]);
							print(b64);
							print(LZString.compressToBase64(avatar));
						}
					}
					printf("%u total\r\n", total);
					break;
				}
				break;
			case "install":
				var result = install();
				printf("%s\r\n", result == true ? "Successful" : "!FAILED: " + result);
				break;
		}
	}
}

main();
