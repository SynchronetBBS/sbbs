// $Id$
//****************************************************************************
//	  JavaScript module for performing FTP-based QWKnet call-outs
//    Inspired by exec/qnet-ftp.src
//    Possible due to the load/ftp.js library developed by Deuce
//****************************************************************************

// Usage: ?qnet-ftp <hub-id> <address> <password> [port]
// Example: ?qnet-ftp VERT vert.synchro.net YOURPASS 21

require('ftp.js', 'FTP');

var hubid = argv[0];
var addr = argv[1];
var username = system.qwk_id
var password = argv[2];
var port = argv[3] || 21;

var rep = system.data_dir + hubid + ".rep";
var qwk = hubid + ".qwk";
var qwk_fname = system.data_dir + qwk;

if(file_getcase(qwk_fname)) {
	alert(qwk_fname + " already exists");
	for(var i = 0; ; i++) {
		qwk_fname = system.data_dir + hubid + ".qw" + i;
		if(!file_getcase(qwk_fname))
			break;
		alert(qwk_fname + " already exists");
		if(i == 9)
			exit(1);
	}
}

var ftp;
try {
	ftp = new FTP(addr, username, password, port);
} catch(e) {
	print("FTP Session with " + addr + " failed: " + e);
	exit(1);
}

rep = file_getcase(rep);
if(rep) {
	print("Sending REP Packet: " + rep);
	try {
		ftp.stor(rep, file_getname(rep));
		print("REP packet sent successfully");
		file_remove(rep);
	} catch(e) {
		alert("Upload of " + rep + " failed: " + e);
		exit(1);
	}
}

print("Downloading QWK Packet: " + qwk);
try {
	ftp.retr(qwk, qwk_fname);
	if(file_size(qwk_fname) < 1) {
		alert("Invalid QWK Packet size: " + file_size(qwk_fname));
		file_remove(qwk_fname);
	} else
		print("Downloaded " + file_getname(qwk_fname) + " successfully");
} catch(e) {
	alert("Download of " + qwk + " failed: " + e);
}
ftp.quit();
print("Done.");
