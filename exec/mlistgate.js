// mlistgate.js

// Synchronet Mailing List Gateway Module

// This module will scan one or more message bases (listed in ctrl/mlistgate.cfg)
// and export any new messages to the mail database to be sent to one or more
// list-server e-mail addresses.

// $Id$

// Configuration file (in ctrl/mlistgate.cfg) format:

// <subcode> <fromaddr> <toaddr> [toaddr] [...]

const REVISION = "$Revision$".split(' ')[1];

printf("Synchronet Mailing List Gateway %s session started\r\n", REVISION);

var tearline = format("--- Synchronet %s%s-%s MListGate %s\r\n"
					  ,system.version,system.revision,system.platform,REVISION);
var tagline	=  format(" *  %s - %s - telnet://%s\r\n"
					  ,system.name,system.location,system.inetaddr);

var cfg_fname = system.ctrl_dir + "mlistgate.cfg";

var reset_export_ptrs = false;
var update_export_ptrs = false;

load("sbbsdefs.js");

if(this.js==undefined) 		// v3.10?
	js = { terminated: false };

area = new Array();

/******************************/
/* Read/Parse the Config File */
/******************************/

cfg_file = new File(cfg_fname);
if(!cfg_file.open("r")) {
	printf("!Error %d opening %s\r\n",errno,cfg_fname);
	delete cfg_file;
	exit();
}

while(!cfg_file.eof) {
	line = cfg_file.readln();
	if(line==null || line[0] == ';' || !line.length || line==undefined)
		continue;
	line = truncsp(line);
	str=line.split(/\s+/);
	area.push(str);
}
delete cfg_file;

/******************/
/* Expor Messages */
/******************/
var exported=0;

mailbase = new MsgBase("mail");
if(mailbase.open!=undefined && mailbase.open()==false) {
	printf("!ERROR %s opening mail base\r\n",mailbase.last_error);
	exit();
}

printf("Scanning %lu message bases...\r\n",area.length);
for(i in area) {
	
	if(js.terminated)
		break;

	sub = area[i].shift();

	from = area[i].shift();

	msgbase = new MsgBase(sub);
	if(msgbase.open!=undefined && msgbase.open()==false) {
		printf("!ERROR %s opening msgbase: %s\r\n",msgbase.last_error,sub);
		delete msgbase;
		continue;
	}

	printf("sub: %s\r\n",sub);

	/*********************/
	/* Read Pointer File */
	/*********************/
	export_ptr = 0;
	ptr_fname = msgbase.file + ".mlg";
	ptr_file = new File(ptr_fname);
	if(ptr_file.open("rb")) {
		export_ptr = ptr_file.readBin();
		printf("%s export ptr: %ld\r\n",sub,export_ptr);
		ptr_file.close();
	}

	if(reset_export_ptrs)
		ptr = 0;
	else if(update_export_ptrs)
		ptr = msgbase.last_msg;
	else 
		ptr = export_ptr;

	if(ptr < msgbase.first_msg)
		ptr = msgbase.first_msg;
	else
		ptr++;

	/*************************/
	/* EXPORT Local Messages */
	/*************************/
	last_msg=msgbase.last_msg;
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
			printf("!FAILED to read message number %ld\r\n",ptr);
			continue;
		}
		if(msgbase.cfg!=undefined && msgbase.cfg.settings&SUB_ASCII) {
			/* Convert Ex-ASCII chars to approximate ASCII equivalents */
			body = ascii_str(body);
			hdr.subject = ascii_str(hdr.subject);
		}
		body += tearline;
		body += tagline;

		while(area[i].length) {	/* For each list server... */

			listserv=area[i].shift();
	
			/* Address message to list server e-mail address */
		    hdr.to_net_addr = listserv; 
		    hdr.to_net_type = NET_INTERNET;

			hdr.from_net_addr = from;
			hdr.from_net_type = NET_INTERNET;

			/* Copy to message base */
			mailbase.save_msg(hdr,body);
			printf("Exported message %lu to list server: %s\r\n",ptr,listserv);
			exported++;
		}
	}
	if(ptr > msgbase.last_msg)
		ptr = msgbase.last_msg;
	export_ptr = ptr;

	/* Save Pointers */
	if(!ptr_file.open("wb"))
		printf("!ERROR %d creating/opening %s\r\n",errno,ptr_fname);
	else {
		ptr_file.writeBin(export_ptr);
		ptr_file.close();
	}
	delete ptr_file;
	delete msgbase;

}

delete mailbase;

printf("Synchronet Mailing List Gateway %s session complete (%lu exported)\r\n"
	   ,REVISION, exported);

/* End of mlistgate.js */