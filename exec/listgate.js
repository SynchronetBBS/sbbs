// listgate.js

// Synchronet Mailing List Gateway Module

// This module will scan one or more message bases (listed in ctrl/listgate.ini)
// and export any new messages to the mail database to be sent to one or more
// list-server e-mail addresses.

// $Id: listgate.js,v 1.4 2016/10/14 23:11:24 rswindell Exp $

// Configuration file (in ctrl/listgate.ini) format:

// [subcode]
// from = <fromaddr> 
// to = <toaddr> [toaddr] [...]

const REVISION = "$Revision: 1.4 $".split(' ')[1];

log(LOG_INFO,format("Synchronet Mailing List Gateway %s session started\r\n", REVISION));

var tearline = format("--- Synchronet %s%s-%s ListGate %s\r\n"
					  ,system.version,system.revision,system.platform,REVISION);
var tagline	=  format(" *  %s - %s - telnet://%s\r\n"
					  ,system.name,system.location,system.inetaddr);

var ini_fname = file_cfgname(system.ctrl_dir, "listgate.ini");

var debug		= false;
var reset_ptrs	= false;
var update_ptrs = false;

load("sbbsdefs.js");

if(this.js==undefined) 		// v3.10?
	js = { terminated: false };

for(i=0;i<argc;i++) {
	if(argv[i].toLowerCase()=="-d")			// debug
		debug = true;
	else if(argv[i].toLowerCase()=="-u")	// update export pointers (export none)
		update_ptrs = true;
	else if(argv[i].toLowerCase()=="-r")	// reset export pointers (export all)
		reset_ptrs = true;
}

/******************************/
/* Read/Parse the Config File */
/******************************/

ini_file = new File(ini_fname);
if(!ini_file.open("r")) {
	log(LOG_ERR,format("!Error %d opening %s\r\n",errno,ini_fname));
	delete ini_file;
	exit();
}
var disabled=ini_file.iniGetValue(null,"disabled",false);
var area = ini_file.iniGetAllObjects("sub");
delete ini_file;

if(disabled) {
	log(LOG_WARNING,"Disabled in " + ini_fname);
	exit();
}

/*******************/
/* Export Messages */
/*******************/
var exported=0;

mailbase = new MsgBase("mail");
if(mailbase.open!=undefined && mailbase.open()==false) {
	log(LOG_ERR,format("!ERROR %s opening mail base\r\n",mailbase.last_error));
	exit();
}

log(LOG_INFO,format("Scanning %lu message bases...\r\n",area.length));
for(i in area) {
	
	if(js.terminated)
		break;

	sub = area[i].sub;

	from = area[i].from;
	
	if(!from)
		from = sub;

	if(from.indexOf('@')<0)	// automaticalliy append system's e-mail host/domain name
		from+=('@'+system.inet_addr);

	msgbase = new MsgBase(sub);
	if(msgbase.open!=undefined && msgbase.open()==false) {
		log(LOG_ERR,format("!ERROR %s opening msgbase: %s\r\n",msgbase.last_error,sub));
		delete msgbase;
		continue;
	}

	log(LOG_INFO,format("sub: %s\r\n",sub));

	/*********************/
	/* Read Pointer File */
	/*********************/

	/* Get export message pointer */
	ptr_fname = msgbase.file + ".listgate.ptr";
	if(!file_exists(ptr_fname))
		file_touch(ptr_fname);
	ptr_file = new File(ptr_fname);
	if(!ptr_file.open("r+")) {
		log(LOG_ERR,format("%s !ERROR %d opening/creating file: %s"
			,list.name, ptr_file.error, ptr_fname));
		delete msgbase;
		continue;
	}

	var last_msg = msgbase.last_msg;
	var ptr = Number(ptr_file.readln());

	log(LOG_DEBUG,format("%s pointer read: %u", sub, ptr));

	if(reset_ptrs)
		ptr = 0;
	else if(update_ptrs)
		ptr = last_msg;

	if(ptr < msgbase.first_msg)
		ptr = msgbase.first_msg;
	else
		ptr++;

	/*************************/
	/* EXPORT Local Messages */
	/*************************/
	for(;ptr<=last_msg && !js.terminated;ptr++) {
		if(this.console!=undefined)
			console.line_counter = 0;
		hdr = msgbase.get_msg_header(
			/* retrieve by offset? */	false,
			/* message number */		ptr,
			/* regenerate msg-id? */	false
			);
		if(hdr == null)
			continue;
		if(hdr.attr&MSG_DELETE)	/* marked for deletion */
			continue;
		if(hdr.attr&MSG_MODERATED && !(hdr.attr&MSG_VALIDATED))
			continue;
		if(hdr.attr&MSG_PRIVATE)/* no private messages on list */
			continue;
		if(hdr.reverse_path)	/* no dupe loop */
			continue;
		if(hdr.from_net_type	/* don't gate messages between net types */
			&& msgbase.cfg!=undefined && !(msgbase.cfg.settings&SUB_GATE))
			continue;

		body = msgbase.get_msg_body(
				 false	/* retrieve by offset */
				,ptr	/* message number */
				,true	/* remove ctrl-a codes */
				,true	/* rfc822 formatted text */
				,true	/* include tails */
				);
		if(body == null) {
			log(LOG_ERR,format("!FAILED to read message number %ld\r\n",ptr));
			continue;
		}
		if(msgbase.cfg!=undefined && msgbase.cfg.settings&SUB_ASCII) {
			/* Convert Ex-ASCII chars to approximate ASCII equivalents */
			body = ascii_str(body);
			hdr.subject = ascii_str(hdr.subject);
		}
		body += tearline;
		body += tagline;

		/* we're not replying a message that exists in the mail database */
		/* so any thread message numbers will be invalid */
		delete hdr.thread_orig;	
		delete hdr.thread_next;
		delete hdr.thread_first;

		var listservers = new Array(area[i].to);
		while(listservers.length) {	/* For each list server... */

			listserv=listservers.shift();
	
			/* Address message to list server e-mail address */
		    hdr.to_net_addr = listserv; 
		    hdr.to_net_type = NET_INTERNET;

			hdr.from_net_addr = from;
			hdr.from_net_type = NET_INTERNET;

			/* Copy to message base */
			if(mailbase.save_msg(hdr,body)) {
				log(LOG_INFO,format("Exported message %lu to list server: %s\r\n",ptr,listserv));
				exported++;
			} else
				log(LOG_ERR,format("!ERROR %s exporting message %lu to list server: %s\r\n"
					,mailbase.error, ptr, listserv));
		}
	}
	if(ptr > last_msg)
		ptr = last_msg;

	/* Save Pointers */
	ptr_file.rewind();
	ptr_file.length=0;		// truncate
	ptr_file.writeln(ptr);
	ptr_file.close();

	delete ptr_file;
	delete msgbase;
}

delete mailbase;

printf("Synchronet Mailing List Gateway %s session complete (%lu exported)\r\n"
	   ,REVISION, exported);

/* End of listgate.js */