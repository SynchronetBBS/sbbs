// $Id: qnet-ftp.js,v 1.4 2020/05/22 06:58:34 rswindell Exp $
//****************************************************************************
//	  JavaScript module for performing FTP-based QWKnet call-outs
//    Inspired by exec/qnet-ftp.src
//    Possible due to the load/ftp.js library developed by Deuce
//****************************************************************************

// Usage: ?qnet-ftp <hub-id> [address] [password] [port]
// Example: ?qnet-ftp VERT vert.synchro.net YOURPASS 21

const REVISION = "$Revision: 1.4 $".split(' ')[1];

require('ftp.js', 'FTP');

var hubid = argv[0];
var options = load({}, "modopts.js", "qnet-ftp:"+hubid);
if(!options)
	options = load({}, "modopts.js", "qnet-ftp");
if(!options)
	options = {};

var username = options.username || system.qwk_id;
var addr = options.address || argv[1];
var password = options.password || argv[2];
var port = options.port || argv[3] || 21;

var rep = system.data_dir + hubid + ".rep";
var qwk = hubid + ".qwk";
var qwk_fname = system.data_dir + qwk;

log(LOG_DEBUG, js.exec_file + " v" + REVISION);

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
	ftp = new FTP(addr, username, password, port, options.dataport, options.bindhost);
} catch(e) {
	print("FTP Session with " + addr + " failed: " + e);
	exit(1);
}

log(LOG_DEBUG, "Connected to " + addr + " via " + ftp.revision);

if(options.passive === false)
	ftp.passive = false;

rep = file_getcase(rep);
if(rep) {
	print("Sending REP Packet: " + rep + format(" (%1.1fKB)", file_size(rep) / 1024.0));
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
		print("Downloaded " + file_getname(qwk_fname) 
			+ format(" (%1.1fKB)", file_size(qwk_fname) / 1024.0) + " successfully");
} catch(e) {
	alert("Download of " + qwk + " failed: " + e);
}
ftp.quit();
print("Done.");
