/* $Id$ */

/* Synchronet v3.15 update script (to be executed with jsexec) */

const REVISION = "$Revision$".split(' ')[1];

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
//		print("\nDuplicate detected: " +f1);
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

	for (i=1; i<system.lastuser; i++) {
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
	load({}, "sbbsecho_upgrade.js");
}
