/* $Id: update.js,v 1.10 2020/05/05 01:09:27 rswindell Exp $ */

/* Synchronet v3.15 update script (to be executed with jsexec) */

const REVISION = "$Revision: 1.10 $".split(' ')[1];

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

	for (i=1; i < last_user; i++) {
		u = new User(i);
//		print("User: "+i+" Note: "+u.note+" IP: "+u.ip_address);
		if (u.ip_address.length == 0 && u.note.length > 0) {
			print("\nMoving IP from note to ip_address for "+u.alias+" (#"+i+")");
			u.ip_address = u.note;
			updated++;
		}
	}
	return updated;
}

function install_logonlist()
{
	var cnflib = load({}, "cnflib.js");
	var main_cnf = cnflib.read("main.cnf");
	if(!main_cnf)
		return "!Failed to read main.cnf";
	if(main_cnf.sys_daily)
		return format("System daily event already set to: '%s'", main_cnf.sys_daily);
	main_cnf.sys_daily = maint_event;
	if(!cnflib.write("main.cnf", undefined, main_cnf))
		return "!Failed to write main.cnf";
	return "Successful";
}

function base_filename(fullname)
{
	var ext = file_getext(fullname);

	if(!ext)
		return fullname;

	return fullname.slice(0, -ext.length);
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

printf("Synchronet update.js revision %u\n", REVISION);
printf("Updating exec directory: ");
printf("%s\n", update_exec_dir() ? "Success" : "FAILURE");
printf("Updating ip_address field: ");
printf("%d records updated\n", move_laston_address());

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

if(!xtrn_area.prog["avatchoo"] && !xtrn_area.event["avat-out"]) {
	print("Installing Avatars feature");
	if(!test)
		js.exec("avatars.js", {}, "install");
}

print("Installing Logon List module: " + install_logonlist());

print("Updating [General] Text File Section indexes");
print(update_gfile_indexes() + " indexes updated.");

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