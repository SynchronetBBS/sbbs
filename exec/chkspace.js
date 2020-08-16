// chkspace.js

// Requires minimum free disk space (in megabytes) as first argument

// $Id: chkspace.js,v 1.8 2004/11/18 21:28:45 rswindell Exp $

// Example: "?chkspace [dir1] [dir2] [minfreespace]"

load("sbbsdefs.js");

var minspace = file_area.min_diskspace*2;	// default to twice the min allowed for uploads
var dirs = new Array();

for(i=0;i<argc;i++)
	if(parseInt(argv[i]))
		minspace=parseInt(argv[i])*1024;	// convert megabytes to kilobytes
	else
		dirs.push(argv[i]);

if(!dirs.length)
	dirs.push(system.temp_dir);	// default to temp dir if none specified

msgbase = new MsgBase("mail");
if(msgbase.open()==false) {
	log(LOG_ERR,"!ERROR " + msgbase.last_error);
	exit();
}

for(i in dirs) {

	var freespace = dir_freespace(dirs[i],1024);

	if(freespace==-1 || freespace >= minspace)
		continue;	// everything's fine

	log(LOG_WARNING,"!Low disk space: " + freespace + " kilobytes on " + dirs[i]);

	hdr = { to: 'sysop', to_ext: '1', from: 'chkspace', subject: 'Low disk space notification' }

	if(!msgbase.save_msg(hdr, "WARNING: Only " + freespace + " kilobytes of free disk space in " 
		+ dirs[i] + " on " + system.timestr()))
		log(LOG_ERR,"!Error " + msgbase.last_error + "saving mail message");

	log(LOG_INFO,"E-mailed low disk space notification to sysop");
}

msgbase.close();