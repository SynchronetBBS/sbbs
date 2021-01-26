// File transfer protocol driver for the Synchronet Terminal Server
// Sends file(s) via E-Mail attachment(s)

// Make sure SCFG->Networks->Internet E-mail->Allow Sending of E-mail and
// Allow File Attachments are set to "Yes".

// Install using 'jsexec emailfiles.js -install'

// Or manually in SCFG->File Options->File Transfer Protocols:

// Mnemonic (Command Key)        E
// Protocol Name                 E-mail Attachment
// Download Command Line         ?emailfiles %f
// Batch Download Command Line   ?emailfiles +%f
// Native Executable/Script      Yes
// Supports DSZLOG               Yes
// Socket I/O                    No

// Your Synchronet Mail Server (SendMail thread) must be operational for this
// module to work as expected.

// ctrl/modopts.ini settings:
// [emailfiles]
// maxfiles = 10
// maxfilesize = 10M
// maxpending = 100M
// prompt
// badaddr
// msgbody
// success

require("sbbsdefs.js", "K_EDIT");
require('smbdefs.js', 'NET_INTERNET');
require("text.js", "InvalidNetMailAddr");

"use strict";

if(argc < 1) {
	alert("No filenames given");
	exit(1);
}

var options = load({}, "modopts.js", "emailfiles");
if(!options)
	options = {};
if(!options.maxfiles)
	options.maxfiles = 10;
if(!options.maxfilesize)
	options.maxfilesize = 10 * 1024 * 1024;
if(!options.maxpending)
	options.maxpending = 100 * 1024 * 1024;

if(argv[0] === '-install') {
	var cnflib = load({}, "cnflib.js");
	var file_cnf = cnflib.read("file.cnf");
	if(!file_cnf) {
		alert("Failed to read file.cnf");
		exit(-1);
	}
	file_cnf.prot.push({
		  key: 'E'
		, name: 'E-mail Attachment'
		, dlcmd: '?emailfiles %f'
		, batdlcmd: '?emailfiles +%f'
		, ars: 'REST NOT M'
		, settings: PROT_NATIVE | PROT_DSZLOG
		});

	if(!cnflib.write("file.cnf", undefined, file_cnf)) {
		alert("Failed to write file.cnf");
		exit(-1);
	}
	exit(0);
}

function sendfiles()
{
	var dir = system.data_dir + format("file/%04u.out/", user.number);
	if(!mkpath(dir)) {
		alert("Error " + errno_str + " making directory: " + dir);
		return errno || 1;
	}

	var logfile = new File(system.node_dir + "PROTOCOL.LOG");
	if(!logfile.open("w")) {
		alert("Error " + logfile.error + " opening " + logfile.name);
		return errno || 1;
	}

	var msgbase = new MsgBase('mail');
	if(!msgbase.open()) {
		alert("Error " + msgbase.error + " opening mail base");
		return msgbase.status || 1;
	}
	var diskusage = load({}, "diskusage.js");
	var list = [];
	for(var i = 0; i < argc; i++) {
		if(argv[i][0] == '+') {
			var f = new File(argv[i].slice(1));
			if(f.open("r")) {
				list = list.concat(f.readAll());
				f.close();
			} else
				alert("Error " + f.error + " opening " + f.name);
		} else
			list.push(argv[i]);
	}
	var files_sent = 0;
	for(var i = 0; i < list.length; i++) {
		if(!user.is_sysop && files_sent >= options.maxfiles) {
			alert(format("Maximum files (%u) sent", options.maxfiles));
			break;
		}
		var fpath = list[i];
		var fname = file_getname(fpath);
		if(!file_exists(fpath)) {
			alert(format("File (%s) does not exist", fname));
			continue;
		}
		var size = file_size(fpath);
		if(!user.is_sysop) {
			if(size > options.maxfilesize) {
				alert(format("File (%s) size (%u) is larger than maximum: %u bytes"
					, fname
					, size
					, options.maxfilesize));
				continue;
			}
			var pending = diskusage.get(dir + '*');
			if(pending + size > options.maxpending) {
				alert(format("Bytes pending (%u) at maximum: %u bytes"
					, pending, options.maxpending));
				continue;
			}
		}
		if(!file_copy(fpath, dir + fname)) {
			alert("Error " + errno_str + " copying file: " + fname);
			continue;
		}
		var hdr = { subject: fname
			, from: user.alias
			, from_net_addr: user.email
			, from_ext: user.number
			, to: user.name
			, to_net_type: NET_INTERNET
			, to_net_addr: address
			, attr: MSG_NOREPLY // Suppress bounce messages
			, netattr: MSG_KILLSENT
			, auxattr: MSG_FILEATTACH
		};
		var msgbody = format(options.msgbody || "Your requested file (%u of %u) is attached." +
			"\r\n\r\nRequested from %s by %s via %s port %u."
			,i + 1, list.length, system.name, client.ip_address, client.protocol, client.port);
		if(!msgbase.save_msg(hdr, msgbody)) {
			alert("Error " + msgbase.error + " saving msg");
			continue;
		}
		console.print(format(options.success || "Successfully attached: %s", fname));
		console.crlf();
		logfile.writeln(format("S %u infinite infinite 0 cps 0 errors 0 infinite %s -1"
			,size, fpath));
		files_sent++;
	}
	logfile.close();
	msgbase.close();
	return files_sent > 0 ? 0 : 1;
}

var result = 1;	// error
if(!user.is_sysop && !(msg_area.inet_netmail_settings & NMAIL_ALLOW))
	alert(bbs.text(NoNetMailAllowed));
else if(!user.is_sysop && !(msg_area.inet_netmail_settings & NMAIL_FILE))
	alert(bbs.text(EmailFilesNotAllowed));
else {
	if(options.prompt === false)
		address = user.netmail;
	else {
		console.print(options.prompt || "\x01h\x01yE-mail address: ");
		if(!bbs.mods.emailfiles_address)
			bbs.mods.emailfiles_address = user.netmail;
		var address = console.getstr(bbs.mods.emailfiles_address, 60, K_EDIT | K_AUTODEL | K_LINE);
	}
	if(console.aborted || netaddr_type(address) != NET_INTERNET || address.indexOf('@' + system.inet_addr) >= 0)
		alert(options.badaddr || "Unsupported e-mail address");
	else {
		bbs.mods.emailfiles_address = address;
		result = sendfiles();
	}
}
// The BBS flushes I/O buffers when returning from file transfers:
while(bbs.online && console.output_buffer_level && !js.terminated) {
	sleep(100);
}
exit(result);
