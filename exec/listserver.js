// listserver.js

// Mailing List Server module for Synchronet v3.11c

load("sbbsdefs.js");

const REVISION = "$Revision$".split(' ')[1];

log(LOG_INFO,"Synchronet ListServer " + REVISION);

js.auto_terminate=false;

var ini_fname = system.ctrl_dir + "listserver.ini";

ini_file = new File(ini_fname);
if(!ini_file.open("r")) {
	log(LOG_ERR,format("!ERROR %s opening ini_file: %s"
		,ini_file.error, ini_fname));
	exit();
}
name_list=ini_file.iniGetValue("config","names",new Array());
list_array=ini_file.iniGetAllObjects("name","list:");
ini_file.close();
if(!list_array.length) {
	log(LOG_ERR,"!No lists defined in " + ini_fname);
	exit();
}

if(this.recipient_list_filename!=undefined) {	/* Subscription control */

	var error_file = new File(processing_error_filename);
	if(!error_file.open("w")) {
		log(LOG_ERR,format("!ERROR %s opening processing error file: %s"
			,error_file.error, processing_error_filename));
		exit();
	}

	var rcptlst_file = new File(recipient_list_filename);
	if(!rcptlst_file.open("r")) {
		error_file.writeln(log(LOG_ERR,format("!ERROR %s opening recipient list: %s"
			,rcptlst_file.error, recipient_list_filename)));
		exit();
	}
	var rcpt_list=rcptlst_file.iniGetAllObjects("number");
	rcptlst_file.close();

	var msgtxt_file = new File(message_text_filename);
	if(!msgtxt_file.open("r")) {
		error_file.writeln(log(LOG_ERR,format("!ERROR %s opening recipient list: %s"
			,msgtxt_file.error, message_text_filename)));
		exit();
	}

	msgtxt = msgtxt_file.readAll()
	msgtxt_file.close();

	load("mailproc_util.js");	// import parse_msg_header() and get_msg_body()

	var header = parse_msg_header(msgtxt);
	var body = get_msg_body(msgtxt);

	/* control message for list server? */
	for(r=0;r<rcpt_list.length;r++) {
		for(n=0;n<name_list.length;n++) {
			if(rcpt_list[r].Recipient.search(new RegExp(name_list[n],"i"))!=-1)
				break;
		}
		if(n<name_list.length)	/* match found */
			break;
	}
	if(r<rcpt_list.length) { 

		log(LOG_INFO,format("ListServer Control message from %s to %s: %s"
			,header.from, header.to, header.subject));

		process_control_msg(header, body); 

		exit();
	}

	/* contribution to mailing list? */
	var contribution=false;
	for(r=0;r<rcpt_list.length;r++) {
		for(l=0;l<list_array.length;l++) {
			if(rcpt_list[r].Recipient.match(/(\S+)@/)[1].toLowerCase()==list_array[l].name
				&& !list_array[l].disabled
				&& !list_array[l].readonly)
				break;
		}
		if(l<list_array.length) {	/* match found */
			log(LOG_INFO,format("ListServer Contribution message from %s to %s: %s"
				,header.from, rcpt_list[r].Recipient, header.subject));

			if(!process_contribution(header, body, list_array[l]))
				break;
		}
	}

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
	user_fname = msgbase.file + ".list.users";
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
	ptr_fname = msgbase.file + ".list.ptr";
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

/* End of Main */

/* Handle Mailing List Control Messages (e.g. subscribe/unsubscribe) here */
function process_control_msg(header, body)
{
}

/* Handle Mailing List Contributions here */
function process_contribution(header, body, list)
{
	// ToDo: apply filtering here?

	var msgbase=new MsgBase(list.sub);

	// ToDo: verify author/sender is a list subscriber here

	if(!msgbase.open()) {
		error_file.writeln(log(LOG_ERR,format("%s !ERROR %s opening msgbase: %s"
			,list.name, msgbase.error, list.sub)));
		return(false);
	}

	// ToDo: Split header.from into separate name/address fields here

	header.id = header["message-id"]; // Convert to Synchronet-compatible

	if(!msgbase.save_msg(header, body.join('\r\n'))) {
		log(LOG_ERR,format("%s !ERROR %s saving message to sub: %s"
			,list.name, msgbase.error, list.sub));
		return(false);
	}

	return(true);
}