// listserv.js

// Mailing List Server module for Synchronet v3.11c

load("sbbsdefs.js");

const REVISION = "$Revision$".split(' ')[1];

log(LOG_INFO,"Synchronet ListServ " + REVISION);

js.auto_terminate=false;

var ini_fname = system.ctrl_dir + "listserv.ini";

ini_file = new File(ini_fname);
if(!ini_file.open("r")) {
	log(LOG_ERR,format("!ERROR %s opening ini_file: %s"
		,ini_file.error, ini_fname));
	exit();
}
list_array=ini_file.iniGetAllObjects("name","list:");
ini_file.close();
if(!list_array.length) {
	log(LOG_ERR,"!No lists defined in " + ini_fname);
	exit();
}

if(this.message_text_filename!=undefined) {	/* Subscription control */

	exit();
}

mailbase = new MsgBase("mail");
if(mailbase.open()==false) {
	log(LOG_ERR,"!ERROR " + mailbase.error + " opening mail database");
	exit();
}

for(l in list_array) {

	if(js.terminated) {
		log(LOG_WARNING,"Terminated");
		break;
	}

	if(list_array[l].disabled)
		continue;

	list_name = list_array[l].name;

	msgbase = new MsgBase(list_array[l].sub);
	if(msgbase.open()==false) {
		log(LOG_ERR,format("%s !ERROR %s opening msgbase: %s"
			,list_name, msgbase.error, list_array[l].sub));
		delete msgbase;
		continue;
	}

	/* Get user (subscriber) list */
	user_fname = msgbase.file + ".listserv.users";
	user_file = new File(user_fname);
	if(!user_file.open("r")) {
		log(LOG_ERR,format("%s !ERROR %s opening file: %s"
			,list_name, user_file.error, user_fname));
		delete msgbase;
		continue;
	}
	user_list = user_file.iniGetAllObjects();

/***
	if(!user_list.length) {
		log(LOG_NOTICE,"No subscribers to list: " + list_name);
		delete msgbase;
		continue;
	}
***/

	/* Get export message pointer */
	ptr_fname = msgbase.file + ".listserv.ptr";
	ptr_file = new File(ptr_fname);
	if(!ptr_file.open("w+")) {
		log(LOG_ERR,format("%s !ERROR %s opening/creating file: %s"
			,list_name, ptr_file.error, ptr_fname));
		delete msgbase;
		continue;
	}

	last_msg=msgbase.last_msg;
	ptr = Number(ptr_file.readln());

	if(ptr < msgbase.first_msg)
		ptr = msgbase.first_msg;
	else
		ptr++;

	for(;ptr<=last_msg && !js.terminated; ptr++) {
		hdr = msgbase.get_msg_header(
			/* retrieve by offset? */	false,
			/* message number */		ptr,
			/* regenerate msg-id? */	false
			);
		if(hdr == null) {
			/**
			log(LOG_WARNING,format("%s !ERROR %s getting msg header #%lu"
				,list_name, msgbase.error, ptr));
			**/
			continue;
		}
		if(hdr.attr&(MSG_DELETE|MSG_PRIVATE))	{ /* marked for deletion */
			log(LOG_NOTICE,format("%s Skipping %s message #%lu from %s: %s"
				,list_name, hdr.attr&MSG_DELETE ? "deleted":"private"
				,ptr, hdr.from, hdr.subject));
			continue;
		}
		if(hdr.attr&MSG_MODERATED && !(hdr.attr&MSG_VALIDATED)) {
			log(LOG_NOTICE,format("%s Stopping at unvalidated moderated message #%lu from %s: %s"
				,list_name, ptr, hdr.from, hdr.subject));
			ptr--;
			break;
		}

		body = msgbase.get_msg_body(
				 false	/* retrieve by offset */
				,ptr	/* message number */
				,true	/* remove ctrl-a codes */
				,false	/* rfc822 formatted text */
				,true	/* include tails */
				);
		if(body == null) {
			log(LOG_ERR,format("%s !ERROR %s reading text of message #%lu"
				,list_name, msgbase.error, ptr));
			continue;
		}

		rcpt_list = new Array();
		for(u in user_list) {
			if(js.terminated)
				break;
			if(user_list[u].disabled || !user_list[u].addr)
				continue;
			log(LOG_DEBUG,format("%s Enqueing message #%lu for %s <%s>"
				,list_name, ptr, user_list[u].name, user_list[u].addr));
			rcpt_list.push(	{	to:				user_list[u].name,
								to_net_addr:	user_list[u].addr, 
								to_net_type:	NET_INTERNET 
							} );
		}
		if(js.terminated) {
			ptr--;
			break;
		}
		if(rcpt_list.length < 1) {
			log(LOG_NOTICE,format("%s No active subscriptions", list_name));
			continue;
		}

		log(LOG_INFO,format("%s Sending message #%lu from %s to %lu recipients: %s"
			,list_name, ptr, hdr.from, rcpt_list.length, hdr.subject));

		if(!mailbase.save_msg(hdr,body,rcpt_list))
			log(LOG_ERR,format("%s !ERROR %s saving mail message"
				,list_name, mailbase.error));
	}

	if(ptr > last_msg)
		ptr = last_msg;

	ptr_file.rewind();
	ptr_file.length=0;		// truncate
	ptr_file.writeln(ptr);
	ptr_file.close();
}

/* clean-up */
mailbase.close();