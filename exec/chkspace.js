// chkspace.js

// Requires minimum free disk space (in megabytes) as first argument

// $Id$

// Example: "?chkspace 100"

minspace = file_area.min_diskspace*2;
freespace = system.freediskspacek;	// new property in v3.10L

if(argc)
	minspace=Number(argv[0])*1024;	// convert megabytes to kilobytes

if(freespace==-1 || freespace >= minspace)
	exit();	// everything's fine

log("!Low disk space: " + freespace + " kilobytes");

msgbase = new MsgBase("mail");
if(msgbase.open!=undefined && msgbase.open()==false) {
	log("!ERROR " + msgbase.last_error);
	exit();
}

hdr = { to: 'sysop', to_ext: '1', from: 'chkspace', subject: 'Low disk space notification' }

if(!msgbase.save_msg(hdr, "WARNING: Only " + freespace + " kilobytes of free disk space on " 
	+ system.timestr()))
	log("!Error " + msgbase.last_error + "saving mail message");

log("E-mailed low disk space notification to sysop");

msgbase.close();