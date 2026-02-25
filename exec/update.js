/* Synchronet v3.15+ update script (to be executed with jsexec) */

const REVISION = "3.0";

var test = argv.indexOf("-test") >= 0;

function update_exec_dir()
{
	var files;
	var fi,f1,f2;

	files=directory(system.exec_dir + "load/*.js");

	for(fi in files) {
		f1 = files[fi];
		f2 = system.exec_dir + file_getname(f1);
		if(!file_exists(f2))
			continue;
		print("\nDuplicate detected: " +f1);
		if(test)
			continue;
		if(file_compare(f1, f2)) {
			if(!file_remove(f2)) {
				printf("!Error %u removing %s\n", errno, f2);
				return false;
			}
		} else if(!file_backup(f2, /* levels: */100, /* rename: */true)) {
				printf("!Error backing-up %s\n", f2);
				return false;
		}
	}
	return true;
}

function move_laston_address()
{
	var i;
	var u;
	var updated = 0;
	var last_user = system.lastuser;

	if(test)
		return 0;

	for (i=1; i <= last_user; i++) {
		u = new User(i);
//		print("User: "+i+" Note: "+u.note+" IP: "+u.ip_address);
		if (u.ip_address.length == 0 && u.note.length > 0) {
			if(!updated)
				print();
			print("Moving IP from note to ip_address for "+u.alias+" (#"+i+")");
			u.ip_address = u.note;
			updated++;
		}
	}
	return updated;
}

function update_birthdates()
{
	var i;
	var u;
	var updated = 0;
	var last_user = system.lastuser;

	if(test)
		return 0;

	for (i=1; i <= last_user; i++) {
		u = new User(i);
		if (u.birthdate.charAt(2) == '/') {
			if(!updated)
				print();
			print("Updating birthdate format of "+u.alias+" (#"+i+")");
			var year = u.birthyear;
			var month = u.birthmonth;
			var day = u.birthday;
			u.birthyear = year;
			u.birthmonth = month;
			u.birthday = day;
			updated++;
		}
	}
	return updated;
}

function install_logonlist()
{
	const cmdline = "?logonlist -m";
	var f = new File(system.ctrl_dir + "main.ini");
	if(!f.open(f.exists ? 'r+':'w+'))
		return "!Failed to open " + f.name;
	var cmd = f.iniGetValue("daily_event", "cmd");
	if(cmd) {
		f.close();
		return cmd == cmdline ? "Already installed" : format("System daily event already set to: '%s'", cmd);
	}
	var result = f.iniSetValue("daily_event", "cmd", cmdline);
	f.close();
	if(!result)
		return "!Failed to write main.ini";
	return "Successful";
}

function install_trashman()
{
	const cmdline = "%!trashman%. %z*.can %kspamblock.cfg";
	const section = "monthly_event";
	var f = new File(system.ctrl_dir + "main.ini");
	if(!f.open(f.exists ? 'r+':'w+'))
		return "!Failed to open " + f.name;
	var cmd = f.iniGetValue(section, "cmd");
	if(cmd) {
		f.close();
		return cmd == cmdline ? "Already installed" : format("System monthly event already set to: '%s'", cmd);
	}
	var result = f.iniSetValue(section, "cmd", cmdline);
	result = result && f.iniSetValue(section, "settings", "0x4001");
	f.close();
	if(!result)
		return "!Failed to write main.ini";
	return "Successful";
}

function base_filename(fullname)
{
	var ext = file_getext(fullname);

	if(!ext)
		return fullname;

	return fullname.slice(0, -ext.length);
}

function upgrade_to_v319()
{
	print("Checking for v3.19 file bases");
	var upgraded = true;
	for(var d in file_area.dir) {
		upgraded = false;
		dir_idx = directory(file_area.dir[d].data_dir + "*.sid");
		if(dir_idx && dir_idx.length) {
			upgraded = true;
			break;
		}
	}
	if(!upgraded) {
		var cmdline = system.exec_dir + "upgrade_to_v319";
		print("No v3.19 file bases found, running " + cmdline);
		system.exec(cmdline);
	}
}

function update_gfile_indexes()
{
	var count = 0;
	var ixt_files = directory(system.data_dir + "text/*.ixt");
	for(var i in ixt_files) {
		var file = new File(ixt_files[i]);
		var ini_file = base_filename(file.name) + ".ini";
		if(file_exists(ini_file))
			continue;
		print("Upgrading " + file.name);
		if(!file.open("r")) {
			alert("Error " + file.error + " opening " + file.name);
			continue;
		}
		var list = [];
		while(!file.eof) {
			var path = file.readln();
			if(!path)
				break;
			list.push({ name: path, desc: file.readln() });
		}
		file.close();
		file = new File(ini_file);
		printf("       to %-30s ", file.name);
		if(!file.open("w+")) {
			alert("Error " + file.error + " creating " + file.name);
			continue;
		}
		file.writeln("; Migrated from " + ixt_files[i] 
			+ " by " + js.exec_file + " " + REVISION
			+ " on " + new Date().toLocaleString());
		file.iniSetAllObjects(list);
		file.close();
		print("Success");
		count++;
	}
	return count;
}

function upgrade_to_v320()
{
	print("Checking for v3.20 config files");
	var upgraded = true;
	["main.cnf", "msgs.cnf", "file.cnf", "xtrn.cnf", "chat.cnf"].forEach(function(f) {
		if(file_date(system.ctrl_dir + f) > file_date(system.ctrl_dir + f.replace('.cnf', '.ini')))
			upgraded = false;
	});
	if(!upgraded) {
		js.exec("upgrade_to_v320.js", {});
		alert("Configuration files upgraded, you need to run 'jsexec update.js' again");
		exit(0);
	}

	if(file_date(system.ctrl_dir + "attr.cfg") > file_date(system.ctrl_dir + "attr.ini")) {
		print("attr.cfg -> attr.ini");
		var f = new File(system.ctrl_dir + "attr.cfg");
		if(!f.open("r"))
			alert("Error " + f.error + " opening " + f.name);
		else {
			var list = f.readAll();
			f.close();
			f = new File(system.ctrl_dir + "attr.ini");
			if(!f.open("w"))
				alert("Error " + f.error + " creating " + f.name);
			else {
				f.writeln("mnehigh           = " + list.shift());
				f.writeln("mnelow            = " + list.shift());
				f.writeln("mnecmd            = " + list.shift());
				f.writeln("inputline         = " + list.shift());
				f.writeln("error             = " + list.shift());
				f.writeln("nodenum           = " + list.shift());
				f.writeln("nodeuser          = " + list.shift());
				f.writeln("nodestatus        = " + list.shift());
				f.writeln("filename          = " + list.shift());
				f.writeln("filecdt           = " + list.shift());
				f.writeln("filedesc          = " + list.shift());
				f.writeln("filelisthdrbox    = " + list.shift());
				f.writeln("filelistline      = " + list.shift());
				f.writeln("chatlocal         = " + list.shift());
				f.writeln("chatremote        = " + list.shift());
				f.writeln("multichat         = " + list.shift());
				f.writeln("external          = " + list.shift());
				f.writeln("votes_full        = " + list.shift());
				f.writeln("votes_empty       = " + list.shift());
				f.writeln("progress_complete = " + list.shift());
				f.writeln("progress_empty    = " + list.shift());
				f.close();
			}
		}
	}

	print("Checking for v3.20 user base");
	if(file_exists(system.data_dir + 'user/user.dat') && !file_exists(system.data_dir + 'user/user.tab')) {
		var cmdline = system.exec_dir + "upgrade_to_v320";
		print("No v3.20 user base found, running " + cmdline);
		system.exec(cmdline);
	}

	print("Installing Logon List Daily Event: " + install_logonlist());
	print("Installing Trashman Monthly Event: " + install_trashman());

	print("Updating [General] Text File Section indexes");
	print(update_gfile_indexes() + " indexes updated.");
}

printf("Synchronet update.js revision %s\n", REVISION);
printf("Updating exec directory: ");
printf("%s\n", update_exec_dir() ? "Success" : "FAILURE");
printf("Updating users ip_address field: ");
printf("%d records updated\n", move_laston_address());
printf("Updating users birthdate field: ");
printf("%d records updated\n", update_birthdates());

upgrade_to_v320();
upgrade_to_v319();

var sbbsecho_cfg = system.ctrl_dir + "sbbsecho.cfg";
var sbbsecho_ini = system.ctrl_dir + "sbbsecho.ini";
if(file_exists(sbbsecho_cfg) && !file_exists(sbbsecho_ini)) {
	printf("Converting %s to %s: ", sbbsecho_cfg, sbbsecho_ini);
	if(!test)
		js.exec("sbbsecho_upgrade.js", {});
}

var binkit_ini = system.ctrl_dir + "binkit.ini";
if(file_exists(binkit_ini) && file_exists(sbbsecho_ini)) {
	printf("Merging %s with %s: ", binkit_ini, sbbsecho_ini);
	if(!test)
		js.exec("binkit.js", {}, "upgrade");
}

if(!file_exists(system.data_dir + "sbbslist.json")) {
	print("Installing SBBSLIST v4 (replacing SBL v3)");
	if(!test)
		js.exec("sbbslist.js", {}, "install");
}

if (argv.indexOf("-noavatars") < 0) {
	if(!xtrn_area.prog["avatchoo"] && !xtrn_area.event["avat-out"]) {
		print("Installing Avatars feature");
		if(!test)
			js.exec("avatars.js", {}, "install");
	}
}

var src = system.exec_dir + "jsexec.ini";
var dst = system.ctrl_dir + "jsexec.ini";
if(file_exists(src) && !file_exists(dst)) {
	print("Moving " + src + " to " + dst);
	if(!file_rename(src, dst))
		alert("Could not move '" + src + "' to '" + dst + "'");
}

print("Updating (compiling) Baja modules");
var src_files = directory(system.exec_dir + "*.src");
for(var i in src_files) {
	var bin = src_files[i].slice(0, -4) + ".bin";
	if(file_date(src_files[i]) < file_date(bin))
		continue;
	print("Building " + bin);
	if(!test)
		system.exec(system.exec_dir + "baja " + src_files[i]);
}

