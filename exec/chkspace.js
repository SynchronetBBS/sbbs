// chkspace.js

// Requires minimum free disk space (in megabytes) as first argument

// Example: "?chkspace 100"

freespace = system.freediskspace;

if(freespace==-1 || freespace > Number(argv[0])*1024*1024)
	exit();	// everything's fine

log("!Low disk space: " + freespace + " bytes");

msgbase = new MsgBase("mail");
if(msgbase.open!=undefined && msgbase.open()==false) {
	log("!ERROR " + msgbase.last_error);
	exit();
}

hdr = { to: 'sysop', to_ext: '1', from: 'chkspace', subject: 'Low disk space notification' }

if(!msgbase.save_msg(hdr, "Only " + freespace + " bytes of free disk space!"))
	log("!Error " + msgbase.last_error + "saving mail message");

log("E-mailed low disk space notification to sysop");

msgbase.close();