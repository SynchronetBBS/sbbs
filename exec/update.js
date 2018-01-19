/* $Id$ */

/* Synchronet v3.15 update script (to be executed with jsexec) */

const REVISION = "$Revision$".split(' ')[1];

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
		load({}, "sbbsecho_upgrade.js");
}

if(!file_exists(system.data_dir + "sbbslist.json")) {
	print("Installing SBBSLIST v4 (replacing SBL v3)");
	if(!test)
		load({}, "sbbslist.js", "install");
}

if(!xtrn_area.prog["avatchoo"] && !xtrn_area.event["avat-out"]) {
	print("Installing Avatars feature");
	if(!test)
		load({}, "avatars.js", "install");
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