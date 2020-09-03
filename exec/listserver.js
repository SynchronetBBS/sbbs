// listserver.js

// Mailing List Server module for Synchronet v3.12

// $Id: listserver.js,v 1.46 2018/01/09 06:26:53 rswindell Exp $

load("sbbsdefs.js");
load("mailproc_util.js");	// import parse_msg_header() and get_msg_body()

const REVISION = "$Revision: 1.46 $".split(' ')[1];
const user_list_ext = ".list.sub";

log(LOG_INFO,"ListServer " + REVISION);

//function log(level, msg)
//{
//js.global.log(level,"ListServer " + msg);
//}

js.auto_terminate=false;

var ini_fname = file_cfgname(system.ctrl_dir, "listserver.ini");

ini_file = new File(ini_fname);
if(!ini_file.open("r")) {
	log(LOG_ERR,format("!ERROR %d opening ini_file: %s"
		,ini_file.error, ini_fname));
	exit();
}
listserver_address=ini_file.iniGetValue(null,"address","listserver@"+system.inet_addr);
listserver_name=ini_file.iniGetValue(null,"name","Synchronet ListServer");
use_sender_address=ini_file.iniGetValue(null,"use_sender_address",true);
var body_cmd = true;
if((subj_cmd = ini_file.iniGetValue(null, "SubjectCommand", false)) == true)
    body_cmd = ini_file.iniGetValue(null, "BodyCommand", true);
disabled=ini_file.iniGetValue(null,"disabled",false);
list_array=ini_file.iniGetAllObjects("name");
ini_file.close();
if(!list_array.length) {
	log(LOG_ERR,"!No lists defined in " + ini_fname);
	exit();
}

if(disabled) {
	log(LOG_WARNING,"!Disabled in " + ini_fname);
	exit();
}

for(var l in list_array) {
	
	var list = list_array[l];

	/* Set default list addresses */
	if(!list.address)
		list.address = format("%s@%s", list.name, system.inet_addr);
	if(!msg_area.sub[list.sub.toLowerCase()]) {
		log(LOG_WARNING,"!Unrecognized sub-board internal code: '" + list.sub + "'");
		list.disabled=true;
		continue;
	}
	if(!list.description)
		list.description = msg_area.sub[list.sub.toLowerCase()].description;
	if(list.confirm==undefined)
		list.confirm=true;
        if(list.use_sender_address==undefined)
                list.use_sender_address=use_sender_address;

	var msgbase = new MsgBase(list.sub);
	if(msgbase.open()==false) {
		log(LOG_ERR,format("%s !ERROR %d (%s) opening msgbase: %s"
			,list.name.toUpperCase(), msgsbase.status, msgbase.error, list.sub));
		continue;
	}
	list.msgbase_file = msgbase.file;

	/* Create the user list file if it doesn't exist */
	var user_fname = list.msgbase_file + user_list_ext;
	if(!file_exists(user_fname))
		file_touch(user_fname);
}

mailbase = new MsgBase("mail");
if(mailbase.open()==false) {
	log(LOG_ERR,format("!ERROR %d (%s) opening mail database"
		,mailbase.status, mailbase.error));
	exit();
}

/* Inbound message from SMTP Server? */
if(js.global.recipient_list_filename!=undefined) {	

	log(LOG_DEBUG,"reverse_path = " + reverse_path);
	if(reverse_path=='' || reverse_path=='<>') {
		log(LOG_WARNING,"No reverse path");
		exit();
	}
	if(reverse_path=='<' + listserver_address + '>') {
		log(LOG_WARNING,"Invalid reverse path (loop?)");
		exit();
	}
	var error_file = new File(processing_error_filename);
	if(!error_file.open("w")) {
		log(LOG_ERR,format("!ERROR %d opening processing error file: %s"
			,error_file.error, processing_error_filename));
		exit();
	}

	var rcptlst_file = new File(recipient_list_filename);
	if(!rcptlst_file.open("r")) {
		error_file.writeln(log(LOG_ERR,format("!ERROR %d opening recipient list: %s"
			,rcptlst_file.error, recipient_list_filename)));
		exit();
	}
	var rcpt_list=rcptlst_file.iniGetAllObjects("number");
	rcptlst_file.close();

	var msgtxt_file = new File(message_text_filename);
	if(!msgtxt_file.open("r")) {
		error_file.writeln(log(LOG_ERR,format("!ERROR %d opening message text: %s"
			,msgtxt_file.error, message_text_filename)));
		exit();
	}
	var msgtxt = msgtxt_file.readAll()
	msgtxt_file.close();
	file_remove(message_text_filename);

	var header = parse_msg_header(msgtxt);
	header = convert_msg_header(header);

	if(header.from_net_addr == listserver_address) {
		error_file.writeln(log(LOG_ERR,format("refusing to process message from %s (loop?)"
			,header.from_net_addr)));
		exit();
	}

	var body = get_msg_body(msgtxt);

	var r;
	var handled=false;

	/* contribution to mailing list? */
	for(r=0;r<rcpt_list.length;r++) {
		var l;
		var recipient;
		for(l=0;l<list_array.length;l++) {
			var list = list_array[l];
/** DEBUG
			for(var p in list)
				log(LOG_DEBUG,"list_array["+l+"]."+p+" = "+list[p]);
**/
			recipient=rcpt_list[r].To;
			if(!recipient)	// for pre v3.15 compatibility
				recipient=rcpt_list[r].Recipient;
			if(recipient.toLowerCase()==list.address.toLowerCase()
				&& !list.disabled
				&& !list.readonly)
				break;
		}
		if(l<list_array.length) {	/* match found */
			log(LOG_INFO,format("Contribution message from %s to %s: %s"
				,header.from, recipient, header.subject));
			handled=true;
			if(!process_contribution(header, body, list))
				break;
		}
	}
	if(handled)
		exit();


	log(LOG_INFO,format("Control message from %s to %s: %s"
		,header.from, header.to, header.subject));

	file_remove(recipient_list_filename);

	if(subj_cmd) {
        if(!body_cmd)
            body.length = 0;
        body.unshift(header.subject);	/* Process the subject as a command */
	}

	var response = process_control_msg(body);
	var resp_hdr = {};

	resp_hdr.subject		= listserver_name + " Response";
	resp_hdr.to				= header.from;
	resp_hdr.to_net_addr	= header.from_net_addr;
	resp_hdr.to_net_type	= NET_INTERNET;
	resp_hdr.from			= listserver_name;
	resp_hdr.from_net_addr	= listserver_address;
	resp_hdr.from_net_type	= NET_INTERNET;
	resp_hdr.from_agent		= AGENT_PROCESS;
	resp_hdr.reply_id		= header.id;
	resp_hdr.attr			= MSG_NOREPLY;

	/* Write response to message */
	if(mailbase.save_msg(resp_hdr, response.body.join('\r\n')))
		log(LOG_INFO,"Response to control message created");
	else
		log(LOG_ERR,format("!ERROR %d (%s) saving response message to mail msgbase"
			,msgbase.status, msgbase.error));

	exit();
}

for(var l in list_array) {

	if(js.terminated) {
		log(LOG_WARNING,"Terminated");
		break;
	}

	var list = list_array[l];

	if(list.disabled) {
		log(LOG_INFO,format("%s: disabled", list.name.toUpperCase()));
		continue;
	}

	log(LOG_INFO,format("Processing list: %s", list.name.toUpperCase()));
	msgbase = new MsgBase(list.sub);
	if(msgbase.open()==false) {
		log(LOG_ERR,format("%s !ERROR %d (%s) opening msgbase: %s"
			,list.name.toUpperCase(), msgbase.status, msgbase.error, list.sub));
		delete msgbase;
		continue;
	}

	/* Get subscriber list */
	var user_list = get_user_list(list);
	if(!user_list.length) {
		log(LOG_INFO,format("%s has no subscribers", list.name.toUpperCase()));
		delete msgbase;
		continue;
	}

	var ptr=NaN;
	/* Get export message pointer */
	/* Old way: */
	ptr_fname = list.msgbase_file + ".list.ptr";
	ptr_file = new File(ptr_fname);
	if(ptr_file.open("r")) {
		ptr = Number(ptr_file.readln());
		ptr_file.close();
	}
	delete ptr_file;
	/* New way: */
	ini_fname = list.msgbase_file + ".ini";
	ini_file = new File(ini_fname);
	if(ini_file.open("r")) {
		ptr=ini_file.iniGetValue("ListServer","export_ptr",ptr);
		ini_file.close();
	}

	var last_msg = msgbase.last_msg;

	log(LOG_DEBUG,format("%s pointer read: %u"
		,list.name.toUpperCase(), ptr));

	if(isNaN(ptr))
		ptr = msgbase.last_msg+1;		// export none
	else if(ptr < msgbase.first_msg)
		ptr = msgbase.first_msg;		// export all
	else
		ptr++;							// export next

	for(;ptr<=last_msg && !js.terminated; ptr++) {
		hdr = msgbase.get_msg_header(
			/* retrieve by offset? */	false,
			/* message number */		ptr
			);
		if(hdr == null) {
			/**
			log(LOG_WARNING,format("%s !ERROR %d (%s) getting msg header #%lu"
				,list.name.toUpperCase(), msgbase.status, msgbase.error, ptr));
			**/
			continue;
		}
		if(hdr.attr&(MSG_DELETE|MSG_PRIVATE))	{ /* marked for deletion */
			log(LOG_NOTICE,format("%s Skipping %s message #%lu from %s: %s"
				,list.name.toUpperCase(), hdr.attr&MSG_DELETE ? "deleted":"private"
				,ptr, hdr.from, hdr.subject));
			continue;
		}
		if(hdr.attr&MSG_MODERATED && !(hdr.attr&MSG_VALIDATED)) {
			log(LOG_NOTICE,format("%s Stopping at unvalidated moderated message #%lu from %s: %s"
				,list.name.toUpperCase(), ptr, hdr.from, hdr.subject));
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
			log(LOG_ERR,format("%s !ERROR %d (%s) reading text of message #%lu"
				,list.name.toUpperCase(), msgbase.status, msgbase.error, ptr));
			continue;
		}

		rcpt_list = new Array();
		for(u in user_list) {
			if(js.terminated)
				break;
			if(user_list[u].disabled || !user_list[u].address)
				continue;
			if(!user_list[u].name)
				user_list[u].name = user_list[u].address;
			log(LOG_DEBUG,format("%s Enqueing message #%lu for %s <%s>"
				,list.name.toUpperCase(), ptr, user_list[u].name, user_list[u].address));
			rcpt_list.push(	{	to:				user_list[u].name,
								to_net_addr:	user_list[u].address, 
								to_net_type:	NET_INTERNET 
							} );
		}
		if(js.terminated) {
			ptr--;
			break;
		}
		if(rcpt_list.length < 1) {
			log(LOG_NOTICE,format("%s No active subscriptions", list.name));
			continue;
		}

		log(LOG_INFO,format("%s Sending message #%lu from %s to %lu recipients: %s"
			,list.name.toUpperCase(), ptr, hdr.from, rcpt_list.length, hdr.subject));

		hdr.replyto_net_type = NET_INTERNET;
		hdr.replyto_net_addr = list.address;
		hdr.from_agent = AGENT_PROCESS;
		hdr.reverse_path = listserver_address;
   		// Create RFC2919 'List-ID' header
		hdr.field_list = [ { type: RFC822HEADER, data: 'List-ID: '     + list.description +" <"+ list.name+'.'+system.inet_addr +">"} ];
        // Create RFC2369 Mailing List headers
        if(subj_cmd || body_cmd) {
            var mailto_verb = subj_cmd ? "subject":"body";
            hdr.field_list.push({ type: RFC822HEADER, data: 'List-Help: '   + "<mailto:"+listserver_address+"?"+mailto_verb+"=help>"});
            hdr.field_list.push({ type: RFC822HEADER, data: 'List-Unsubscribe: '   + "<mailto:"+listserver_address+"?"+mailto_verb+"=unsubscribe "+list.name+">"});
        }
        if(!list.disabled && !list.readonly)
            hdr.field_list.push({ type: RFC822HEADER, data: 'List-Post: '   + "<mailto:"+list.address+">"});
		if(list.subject_mod==true && hdr.subject.indexOf("[" + list.name + "]")==-1)
			hdr.subject = "[" + list.name + "] " + hdr.subject;
		if(!mailbase.save_msg(hdr,body,rcpt_list))
			log(LOG_ERR,format("%s !ERROR %d (%s) saving mail message"
				,list.name.toUpperCase(), mailbase.status, mailbase.error));
	}

	if(ptr > last_msg)
		ptr = last_msg;

	if(!ini_file.open(ini_file.exists ? "r+":"w+"))
		printf("!ERROR %d creating/opening %s\r\n",errno,ini_file.name);
	else {
		ini_file.iniSetValue("ListServer", "export_ptr", ptr);
		ini_file.close();
		log(LOG_DEBUG,format("%s pointer written: %u"
			,list.name.toUpperCase(), ptr));
		if(file_exists(ptr_fname))
			file_remove(ptr_fname);
	}
	delete ini_file;
}

/* Process bounced messages here: */
{
	log(LOG_INFO,"Scanning for bounced e-mail messages");
	var total_msgs = mailbase.total_msgs;
	var num;

	for(num=0;num<=total_msgs && !js.terminated; num++) {
		hdr = mailbase.get_msg_header(
			/* retrieve by offset? */	true,
			/* message number */		num
			);
		if(hdr == null)
			continue;
		if(hdr.attr&MSG_DELETE)/* marked for deletion */
			continue;
		if(hdr.to_agent != AGENT_PROCESS)
			continue;
		for(var l in list_array) {

			var list = list_array[l];

			if(list.disabled)
				continue;
	
		}

		if(hdr.to == listserver_name || hdr.to_net_addr == listserver_address
			|| hdr.subject == "Delivery failure: Synchronet ListServer Response") {
			log(LOG_INFO,format("Deleting bounce message #%u to %s", hdr.number, hdr.to));
			if(!mailbase.remove_msg(/* by offset: */ true, num))
				log(LOG_ERR,format("!ERROR %d (%s) removing message #%u to %s"
					,mailbase.status, mailbase.error, hdr.number, hdr.to));
		}
	}
	log(LOG_INFO,"Scanned for bounced e-mail messages");
}

/* clean-up */
mailbase.close();

/* End of Main */

/* Handle Mailing List Control Messages (e.g. subscribe/unsubscribe) here */
function process_control_msg(cmd_list)
{
	var response = { body: new Array(), subject: "" };
	
	response.body.push(listserver_name + " " +REVISION+ " Response:\r\n");

	for(var c in cmd_list) {
		var cmd=cmd_list[c];
		if(!cmd || !cmd.length)
			continue;
		if(cmd=="---")	/* don't parse past tear line */
			break;
		var token=cmd.split(/\s+/);
		cmd=token[0];
		var listname=token[1];
		var address=token[2];
		switch(cmd.toLowerCase()) {
			case "lists":
				response.body.push("List of mailing lists:");
				for(var l in list_array) {
					var list = list_array[l];
					if(!list.disabled) {
						response.body.push("    "+list.name.toUpperCase());
						response.body.push("                                "+list.description);
                        }
				}
				break;
            case "who":
                if(!user.compare_ars("SYSOP"))
                    break;
			case "subscribe":
			case "unsubscribe":
				if(!listname || !listname.length) {
					response.body.push("!List name not specified");
					break;
				}
                var found=false;
				for(var l in list_array) {
					var list = list_array[l];
					if(list.disabled || list.closed)
						continue;
					if(list.name.toLowerCase()==listname.toLowerCase()
						|| list.address.toLowerCase()==listname.toLowerCase()
                        || listname=='*') {
						response.body.push(subscription_control(cmd, list, address));
                        found=true;
					}
				}
                if(!found)
    				response.body.push("!List not found: " + listname);
				break;
			default:
				response.body.push("!Bad command: " + cmd);
			case "help":
				response.body.push("Available commands:");
				response.body.push("    lists");
                response.body.push("                                Request a list of mailing lists");
				response.body.push("    subscribe <list-name>" + (use_sender_address ? "":" [address]"));
                response.body.push("                                Subscribe to specified mailing list (or * for all)");
				response.body.push("    unsubscribe <list-name>" + (use_sender_address ? "":" [address]"));
                response.body.push("                                Unsubscribe from specified mailing list (or * for all)");
				response.body.push("    help");
                response.body.push("                                This help message");
				response.body.push("    end");
                response.body.push("                                End of command list (same as '---')");
                if(user.compare_ars("SYSOP")) {
				    response.body.push("    who <list-name>");
                    response.body.push("                                Request a list of subscribers to specified mailing list (or * for all)");
                }
                response.body.push("");
                if(subj_cmd)
                    response.body.push("A single command may be specified in the message subject.");
                else
                    response.body.push("Commands may not be specified in the message subject.");
                if(body_cmd)
                    response.body.push("One or more commands may be specified in the message body.");
			case "end":
				return(response);
		}
	}

	return(response);
}

function open_user_list(list, mode)
{
	var user_fname = list.msgbase_file + user_list_ext;
	var user_file = new File(user_fname);
	if(!user_file.open(mode)) {
		log(LOG_ERR,format("%s !ERROR %d opening file: %s"
			,list.name.toUpperCase(), user_file.error, user_fname));
		return(null);
	}

	return(user_file);
}

function read_user_list(user_file)
{
	return user_file.iniGetAllObjects("address");
}

function get_user_list(list)
{
	var user_list = new Array();
	if((user_file = open_user_list(list,"r")) != null) {
		user_list = read_user_list(user_file);
		user_file.close();
	}
	return user_list;
}

function find_user(user_list, address)
{
	for(var u in user_list)
		if(user_list[u].address.toLowerCase()==address.toLowerCase())
			return(u);
	return(-1);
}
function remove_user(user_list, address)
{
	var u=find_user(user_list, address);
	if(u==-1)
		return(false);
	user_list.splice(u,1);
	return(true);
}

function write_user_list(user_list, user_file)
{
	user_file.rewind();
	user_file.length = 0;
	for(var u in user_list) {
		user_file.writeln("[" + user_list[u].address + "]");
		for(var p in user_list[u])
			if(p!="address")
				user_file.writeln(format("%-25s",p) + " = " + user_list[u][p]);
		user_file.writeln();
	}
}

function subscription_control(cmd, list, address)
{
	if(!address || list.use_sender_address)
		address=sender_address;

	log(LOG_INFO,format("%s Subscription control command (%s) from %s"
		,list.name.toUpperCase(),cmd,address));

	/* Get subscriber list */
	if((user_file=open_user_list(list,"r+"))==null)
		return log(LOG_ERR,format("%s !ERROR opening subscriber list",list.name));

	user_list = read_user_list(user_file);
	
	switch(cmd.toLowerCase()) {
		case "unsubscribe":
			if(remove_user(user_list, address)) {
				write_user_list(user_list, user_file);
				return log(LOG_INFO,format("%s %s unsubscribed successfully"
					,list.name.toUpperCase(), address));
			}
			return log(LOG_WARNING,format("%s !subscriber not found: %s"
				,list.name.toUpperCase(), address));
		case "subscribe":
			if(find_user(user_list, address)!=-1)
				return log(LOG_WARNING,format("%s !%s is already subscribed"
					,list.name.toUpperCase(), address));
			var now=time();
			user_list.push({ 
				 name:					sender_name 
				,address:				address
				,created:				system.timestr(now)
				,last_activity:			system.timestr(now)
				,last_activity_time:	format("%08lx",now)
				});
			write_user_list(user_list, user_file);
			return log(LOG_INFO,format("%s %s subscription successful"
				,list.name.toUpperCase(), address));
        case "who":
        {
            var response = list.name.toUpperCase() + " has "+user_list.length+" subscribers:\n";
            for(u in user_list)
                response += user_list[u].address + ' "' + user_list[u].name + '" since ' + user_list[u].created + '\n';
            return response;
        }
	}
}

/* Handle Mailing List Contributions here */
function process_contribution(header, body, list)
{
	// ToDo: apply filtering here?

	var user_list = get_user_list(list);

	// verify author/sender is a list subscriber here

	if(sender_address!=list.submitter
        && find_user(user_list, sender_address)==-1) {
		error_file.writeln(log(LOG_WARNING,format("%s !ERROR %s is not a subscriber"
			,list.name.toUpperCase(), sender_address)));
//		error_file.writeln();
//		error_file.writeln("To subscribe to this list, send an e-mail to " 
//			+ listserver_address);
//		error_file.writeln("with \"subscribe " + list.name + "\" in the message body.");
		return(false);
	}

	var msgbase=new MsgBase(list.sub);
	if(!msgbase.open()) {
		error_file.writeln(log(LOG_ERR,format("%s !ERROR %d (%s) opening msgbase: %s"
			,list.name.toUpperCase(), msgbase.status, msgbase.error, list.sub)));
		return(false);
	}

	if(msg_area.sub[list.sub.toLowerCase()].is_moderated)
		header.attr |= MSG_MODERATED;

	// Remove [list.name] from imported subject
	header.subject=header.subject.replace(RegExp("\\["+list.name+"\\]\\s*"), "");

	if(!msgbase.save_msg(header, body.join('\r\n'))) {
		log(LOG_ERR,format("%s !ERROR %d (%s) saving message to sub: %s"
			,list.name.toUpperCase(), msgbase.status, msgbase.error, list.sub));
		return(false);
	}

	return(true);
}
