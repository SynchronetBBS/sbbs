// chkspace.js

// Requires minimum free disk space (in megabytes) as first argument

// $Id: chkspace.js,v 1.8 2004/11/18 21:28:45 rswindell Exp $

// Example: "?chkspace [dir1] [dir2] [minfreespace]"

load("sbbsdefs.js");
load("file_size.js");

var minspace = file_area.min_diskspace*2;	// default to twice the min allowed for uploads
var dirs = new Array();

for(i=0;i<argc;i++)
	if(parseInt(argv[i]))
		minspace=parseInt(argv[i])*1024;	// convert megabytes to kilobytes
	else
		dirs.push(argv[i]);

if(!dirs.length)
	dirs.push(system.temp_dir);	// default to temp dir if none specified

var msgbase;
for(i in dirs) {

	var freespace = dir_freespace(dirs[i]);

	if(freespace >= minspace) {
		continue;	// everything's fine
	}

	if(!msgbase) {
		msgbase = new MsgBase("mail");
		if(msgbase.open()==false) {
			log(LOG_ERR,"!ERROR " + msgbase.last_error);
			exit();
		}
	}
	log(LOG_WARNING,"!Low disk space: " + file_size_str(freespace, 1, 1) + " bytes on " + dirs[i]);

	hdr = { to: 'sysop', to_ext: '1', from: 'chkspace', subject: 'Low disk space notification' }

	if(!msgbase.save_msg(hdr, "WARNING: Only " + file_size_str(freespace, 1, 1) + " bytes of free disk space in "
		+ dirs[i] + " on " + system.timestr()))
		log(LOG_ERR,"!Error " + msgbase.last_error + "saving mail message");

	log(LOG_INFO,"E-mailed low disk space notification to sysop");
}

if(msgbase)
	msgbase.close();
