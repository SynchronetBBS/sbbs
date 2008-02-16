// nntpservice.js

// Synchronet Service for the Network News Transfer Protocol (RFC 977)

// $Id$

// Example configuration (in ctrl/services.ini):

// [NNTP]
// Port = 119
// MaxClients = 10
// Options = 0
// Command = nntpservice.js -f

// Available Command-line options:
//
// -d        debug output
// -f        filter bogus client IP addresses
// -na       no anonymous logins (requires user authentication)
// -mail     expose entire mail database as newsgroup to Sysops
// -auto     automatic login based on IP address (no password necessary)
// -nolimit  unlimited message lengths
// -notag    do not append tear/tagline to local messages for Q-rest accounts
// -ascii	 convert ex-ASCII to ASCII

// Tested clients:
//					Microsoft Outlook Express 6
//					Netscape Communicator 4.77
//					Xnews 5.04.25
//					Mozilla 1.1 (Requires -auto, and a prior login via other method)

const REVISION = "$Revision$".split(' ')[1];

var tearline = format("--- Synchronet %s%s-%s NNTP Service %s\r\n"
					  ,system.version,system.revision,system.platform,REVISION);
var tagline	=  format(" *  %s - %s - telnet://%s\r\n"
					  ,system.name,system.location,system.inetaddr);

load("sbbsdefs.js");
load("newsutil.js");

var debug = false;
var no_anonymous = false;
var auto_login = false;
var msgs_read = 0;
var msgs_posted = 0;
var slave = false;
var bogus_cmd_counter = 0;
var max_bogus_cmds = 10;
var filter_bogus_clients = false;
var include_mail = false;
var impose_limit = true;
var sysop_login = false;
var add_tag = true;
var ex_ascii = true;

// Parse arguments
for(i=0;i<argc;i++) {
	if(argv[i].toLowerCase()=="-d")
		debug = true;
	else if(argv[i].toLowerCase()=="-f")
		filter_bogus_clients = true;
	else if(argv[i].toLowerCase()=="-na")
		no_anonymous = true;
	else if(argv[i].toLowerCase()=="-mail")
		include_mail = true;
	else if(argv[i].toLowerCase()=="-nolimit")
		impose_limit = false;
	else if(argv[i].toLowerCase()=="-notag")
		add_tag = false;
	else if(argv[i].toLowerCase()=="-ascii")
		ex_ascii = false;
	else if(argv[i].toLowerCase()=="-auto") {
		no_anonymous = true;
		auto_login = true;
	}
}

// Write a string to the client socket
function write(str)
{
	client.socket.send(str);
}

function writeln(str)
{
	if(debug)
		log("rsp: " + str);
	write(str + "\r\n");
}

// Courtesy of Michael J. Ryan <tracker1@theroughnecks.com>
// updated 2004-04-16 to lookup by reference id on posting newsgroup.
function getReferenceTo(hdr) {
	//Default Response
	var to = "All";
	var newsgroups = hdr.newsgroups.split(',');
	for(n in newsgroups)
    	for(g in msg_area.grp_list)
		    for(s in msg_area.grp_list[g].sub_list)
			    if (msg_area.grp_list[g].sub_list[s].newsgroup.toLowerCase() == newsgroups[n].toLowerCase()) {
				    var mb=new MsgBase(msg_area.grp_list[g].sub_list[s].code);
				    if (mb.open() != true) continue;
				    var hdr2 = mb.get_msg_header(hdr.reply_id);
				    if (hdr2 != null) to = hdr2.from;
				    mb.close();
				    if (to != "All") return to;
			    }

	return to; //no match
}

// Generate an Xref header
function xref(hdr)
{
	return(format("%s %s:%u"
		,system.local_host_name	// "name of the host (with domains omitted)" per RFC1036 sec 2.2.13
		,selected.newsgroup
		,hdr.number));
}

var username='';
var msgbase=null;
var selected=null;
var current_article=0;
var quit=false;

writeln(format("200 %s News (Synchronet %s%s-%s%s NNTP Service %s)"
		,system.name,system.version,system.revision,system.platform,system.beta_version
		,REVISION));

if(!no_anonymous)
	login("guest");	// Login as guest/anonymous by default

while(client.socket.is_connected && !quit) {

	// Get Request
	cmdline = client.socket.recvline(1024 /*maxlen*/, 300 /*timeout*/);

	if(cmdline==null) {
		log(LOG_WARNING,"!TIMEOUT waiting for request");
		break;
	}

	if(cmdline=="") 	/* ignore blank commands */
		continue;

	log((selected==null ? "":("["+selected.newsgroup+"] ")) +format("cmd: %s",cmdline));

	cmd=cmdline.split(' ');

	switch(cmd[0].toUpperCase()) {
		case "AUTHINFO":
			if(cmd[1]==undefined) {
				writeln("500 Syntax error or unknown command");
				break;
			}
			switch(cmd[1].toUpperCase()) {
				case "USER":
					username=cmd.slice(2).join(" ");
					writeln("381 More authentication required");
					break;
				case "PASS":
					logout();
					if(login(username,cmd.slice(2).join(" "))) {
						if(no_anonymous && user.security.restrictions&UFLAG_G) {
							writeln("502 Anonymous/Guest logins disallowed");
							logout();
						} else
							writeln("281 Authentication successful");
					} else
						writeln("502 Authentication failure");
					break;
				default:
					writeln("500 Syntax error or unknown command");
					break;
			}
			continue;
		case "MODE":
			writeln("200 Hello, you can post");
			continue;
		case "DATE":
			d = new Date();
			writeln("111 " +
				format("%u%02u%02u%02u%02u%02u"
					,d.getUTCFullYear()
					,d.getUTCMonth()+1
					,d.getUTCDate()
					,d.getUTCHours()
					,d.getUTCMinutes()
					,d.getUTCSeconds()
				));
			continue;
		case "QUIT":
			writeln("205 closing connection - goodbye!");
			quit=true;
			continue;
	}

	if(!logged_in) {
		if (auto_login) {
			log(LOG_DEBUG,"Autologin Search: Started");
			var oUser = new User(1);
			sUser = false;
			sPassword = ""
			iLastOn = 0;
			for (var i=1; i<=system.stats.total_users; i++) {
				oUser.number = i;
				if (oUser.ip_address == client.ip_address)
					if (!(oUser.security.restrictions&UFLAG_Q)) //don't count qnet users
						if (oUser.stats.laston_date > iLastOn) { //more recent than last match
							iLastOn = oUser.stats.laston_date;
							sUser = oUser.alias;
							sPassword = oUser.security.password;
							log(LOG_DEBUG,"!Autologin Search: Match Found ("+sUser+")");
						}

			}
			oUser = null;
			log(LOG_DEBUG,"Autologin Search: Finished");

			if (sUser)
				login(sUser,sPassword);
		}

		if(!logged_in) {
			writeln("502 Authentication required");
			log(LOG_WARNING,"!Authentication required");
			continue;
		}
	}

	/* These commands require login/authentication */
	switch(cmd[0].toUpperCase()) {

		case "SLAVE":
			slave = true;
			writeln("202 slave status noted");
			break;

		case "LIST":
			if(cmd[1]==undefined || cmd[1].length==0
				|| cmd[1].toUpperCase()=="ACTIVE") {	// RFC 2980 2.1.2
				pattern=cmd[2];
 				writeln("215 list of newsgroups follows");
				if(include_mail && user.security.level == 99 && wildmatch("mail", pattern)) {
					msgbase=new MsgBase("mail");
					if(msgbase.open()==true) {
						writeln(format("mail %u %u n", msgbase.last_msg, msgbase.first_msg));
						msgbase.close();
					}
				}
				for(g in msg_area.grp_list)
					for(s in msg_area.grp_list[g].sub_list) {
						if(!wildmatch(msg_area.grp_list[g].sub_list[s].newsgroup, pattern))
							continue;
						msgbase=new MsgBase(msg_area.grp_list[g].sub_list[s].code);
						if(msgbase.open!=undefined && msgbase.open()==false)
							continue;
						writeln(format("%s %u %u %s"
							,msg_area.grp_list[g].sub_list[s].newsgroup
							,msgbase.last_msg
							,msgbase.first_msg
							,msg_area.grp_list[g].sub_list[s].can_post ? "y" : "n"
							));
						msgbase.close();
					}
				writeln(".");	// end of list
			}
			else if(cmd[1].toUpperCase()=="NEWSGROUPS") {	// RFC 2980 2.1.6
				pattern=cmd[2];
				writeln("215 list of newsgroups and descriptions follows");
				if(include_mail && user.security.level == 99 && wildmatch("mail", pattern))
					writeln("mail complete mail database");
				for(g in msg_area.grp_list) {
					for(s in msg_area.grp_list[g].sub_list) {
						if(!wildmatch(msg_area.grp_list[g].sub_list[s].newsgroup, pattern))
							continue;
						writeln(format("%s %s"
							,msg_area.grp_list[g].sub_list[s].newsgroup
							,msg_area.grp_list[g].sub_list[s].description
							));
					}
				}
				writeln(".");	// end of list
			}
			else if(cmd[1].toUpperCase()=="OVERVIEW.FMT") {	// RFC 2980 2.1.7
				writeln("215 Order of fields in overview database.");
				writeln("Subject:");
				writeln("From:");
				writeln("Date:");
				writeln("Message-ID:");
				writeln("References:");
				writeln("Bytes:");
				writeln("Lines:");
				writeln("Xref:full");
				writeln(".");	// end of list
			}
			else if(cmd[1].toUpperCase()=="EXTENSIONS") {
				writeln("202 Extensions supported:");
				writeln("OVER");
				writeln("HDR");
				writeln("LISTGROUP");
				writeln("XGTITLE");
				writeln(".");	// end of list
			}
			else {
				writeln("500 Syntax error or unknown command");
				log(LOG_NOTICE,"!unsupported LIST argument: " + cmd[1]);
			}
			break;

		case "XGTITLE":
			pattern=cmd[2];
			writeln("282 list of newsgroups follows");
			if(include_mail && user.security.level == 99 && wildmatch("mail", pattern))
				writeln("mail complete mail database");
			for(g in msg_area.grp_list) {
				for(s in msg_area.grp_list[g].sub_list) {
					if(!wildmatch(msg_area.grp_list[g].sub_list[s].newsgroup, pattern))
						continue;
					writeln(format("%s %s"
						,msg_area.grp_list[g].sub_list[s].newsgroup
						,msg_area.grp_list[g].sub_list[s].description
						));
				}
			}
			writeln(".");	// end of list
			break;

		case "NEWGROUPS":
			writeln("231 list of new newsgroups follows");
			writeln(".");	// end of list
			break;

		case "GROUP":
			if(cmd[1]==undefined || cmd[1].length==0) {
				writeln("411 no group specified");
				break;
			}
		case "LISTGROUP":
			found=false;
			if(cmd[1]==undefined || cmd[1].length==0) {
				if(!selected) {
					writeln("412 no newsgroup selected");
					break;
				}
				found=true;
			}
			else if(include_mail && user.security.level==99 && cmd[1].toLowerCase()=="mail") {
				msgbase=new MsgBase("mail");
				if(msgbase.open()==true) {
					selected = { newsgroup: "mail" };
					found=true;
				}
			}
			if(!found) {
				for(g in msg_area.grp_list)
					for(s in msg_area.grp_list[g].sub_list)
						if(msg_area.grp_list[g].sub_list[s].newsgroup.toLowerCase()==cmd[1].toLowerCase()) {
							msgbase=new MsgBase(msg_area.grp_list[g].sub_list[s].code);
							if(msgbase.open!=undefined && msgbase.open()==false)
								continue;
							found=true;
							selected=msg_area.grp_list[g].sub_list[s];
							break;
						}
			}
			if(!found) {
				writeln("411 no such newsgroup");
				log(LOG_NOTICE,"!no such group");
				bogus_cmd_counter++;
				break;
			}

			if(cmd[0].toUpperCase()=="GROUP")
				writeln(format("211 %u %u %d %s group selected"
					,msgbase.total_msgs	// articles in group
					,msgbase.first_msg
					,(msgbase.total_msgs==0) ? (msgbase.first_msg-1):msgbase.last_msg
					,selected.newsgroup
					));
			else {	// LISTGROUP
				writeln("211 list of article numbers follow");
				for(i=0;i<msgbase.total_msgs;i++) {
					idx=msgbase.get_msg_index(/* by_offset */true,i);
					if(idx==null)
						continue;
					if(idx.attr&MSG_DELETE)	/* marked for deletion */
						continue;
					writeln(idx.number);
				}
				writeln(".");
			}
			break;

		case "OVER":
		case "XOVER":
			if(!selected) {
				writeln("412 no newsgroup selected");
				break;
			}
			var first, last;
			if(cmd[1]==undefined || cmd[1].length==0)
				first=last=current_article;
			else if(cmd[1].indexOf('-')>=0)	{ /* range */
				range=cmd[1].split('-');
				first=Number(range[0]);
				last=Number(range[1]);
			} else
				first=last=Number(cmd[1]);
			writeln("224 Overview information follows for articles " + first + " through " + last);
			for(i=first;i<=last;i++) {
				hdr=msgbase.get_msg_header(false,i);
				if(hdr==null)
					continue;
				if(hdr.attr&MSG_DELETE)	/* marked for deletion */
					continue;
				writeln(format("%u\t%s\t%s\t%s\t%s\t%s\t%u\t%u\tXref:%s"
					,i
					,hdr.subject
					,hdr.from
					,hdr.date
					,hdr.id								// message-id
					,hdr.reply_id ? hdr.reply_id : ''	// references
					,hdr.data_length					// byte count
					,Math.round(hdr.data_length/79)+1	// line count
					,xref(hdr)
					));
			}
			writeln(".");	// end of list
			break;

		case "HDR":
		case "XHDR":
			if(cmd[1]==undefined || cmd[2]==undefined) {
				writeln("500 Syntax error or unknown command");
				break;
			}
			if(!selected) {
				writeln("412 no newsgroup selected");
				break;
			}
			writeln("221 Header follows");
			var first, last;
			if(cmd[2].indexOf('-')>=0)	{ /* range */
				range=cmd[2].split('-');
				first=Number(range[0]);
				last=Number(range[1]);
			} else
				first=last=Number(cmd[2]);
			for(i=first;i<=last;i++) {
				hdr=msgbase.get_msg_header(false,i);
				if(hdr==null)
					continue;
				if(hdr.attr&MSG_DELETE)	/* marked for deletion */
					continue;
				var field="";
				switch(cmd[1].toLowerCase()) {	/* header */
					case "to":
						field=hdr.to;
						break;
					case "subject":
						field=hdr.subject;
						break;
					case "from":
						field=hdr.from;
						break;
					case "reply-to":
						field=hdr.replyto;
						break;
					case "date":
						field=hdr.date;
						break;
					case "message-id":
						field=hdr.id;
						break;
					case "references":
						field=hdr.reply_id;
						break;
					case "lines":
						field=Math.round(hdr.data_length/79)+1;
						break;
					case "xref":
						field=xref(hdr);
						break;
					/* FidoNet header fields */
					case "x-ftn-pid":
						field=hdr.ftn_pid;
						break;
					case "x-ftn-area":
						field=hdr.ftn_area;
						break;
					case "x-ftn-flags":
						field=hdr.ftn_flags;
						break;
					case "x-ftn-msgid":
						field=hdr.ftn_msgid;
						break;
					case "x-ftn-reply":
						field=hdr.ftn_reply;
						break;
				}
				if(field==undefined)
					field="";

				writeln(format("%u %s",i,field.toString()));
			}
			writeln(".");	// end of list
			break;

		case "ARTICLE":
		case "HEAD":
		case "BODY":
		case "STAT":
			if(!selected) {
				writeln("412 no newsgroup selected");
				bogus_cmd_counter++;
				break;
			}
			if(cmd[1]==undefined || cmd[1].length==0) {
				writeln("420 no current article has been selected");
				bogus_cmd_counter++;
				break;
			}
			if(cmd[1]!='') {
				if(cmd[1].indexOf('<')>=0)		/* message-id */
					current_article=cmd[1];
				else
					current_article=Number(cmd[1]);
			}
			if(typeof(current_article)=="number" 
				&& (current_article<1 || isNaN(current_article))) {
				writeln("420 no current article has been selected");
				break;
			}

			hdr=null;
			body=null;
			hdr=msgbase.get_msg_header(false,current_article);

			if(hdr==null) {
				writeln("430 no such article found: " + current_article);
				break;
			}

			current_article=hdr.number;

			if(cmd[0].toUpperCase()!="HEAD")
				body=msgbase.get_msg_body(false,current_article
					,true /* remove ctrl-a codes */
					,true /* rfc822 formatted text */);

			// force taglines for QNET Users on local messages
			if(add_tag && user.security.restrictions&UFLAG_Q && !hdr.from_net_type)
				body += "\r\n" + tearline + tagline;

			if(!ex_ascii || (msgbase.cfg && msgbase.cfg.settings&SUB_ASCII)) {
				/* Convert Ex-ASCII chars to approximate ASCII equivalents */
				body = ascii_str(body);
				hdr.subject = ascii_str(hdr.subject);
			}

/* Eliminate dupe loops
			if(user.security.restrictions&UFLAG_Q && hdr!=null)
*/
			if(hdr.attr&MSG_MODERATED && !(hdr.attr&MSG_VALIDATED)) {
				writeln("430 unvalidated message");
				break;
			}
			if(hdr.attr&MSG_DELETE) {
				writeln("430 deleted message");
				break;
			}
			if(hdr.attr&MSG_PRIVATE
				&& hdr.to.toLowerCase()!=user.alias.toLowerCase()
				&& hdr.to.toLowerCase()!=user.name.toLowerCase()) {
				writeln("430 private message");
				break;
			}

			switch(cmd[0].toUpperCase()) {
				case "ARTICLE":
					writeln(format("220 %d %s article retrieved - head and body follow",current_article,hdr.id));
					break;
				case "HEAD":
					writeln(format("221 %d %s article retrieved - header follows",current_article,hdr.id));
					break;
				case "BODY":
					writeln(format("222 %d %s article retrieved - body follows",current_article,hdr.id));
					break;
				case "STAT":
					writeln(format("223 %d %s article retrieved",current_article,hdr.id));
					break;
			}
			if(cmd[0].toUpperCase()=="STAT")
				break;

			if(cmd[0].toUpperCase()!="BODY") {

				if(!hdr.from_net_type)	/* local message */
					writeln(format("From: \"%s\" <%s@%s>"
						,hdr.from
						,hdr.from.replace(/ /g,".").toLowerCase()
						,system.inetaddr));
				else if(!hdr.from_net_addr.length)
					writeln(format("From: %s",hdr.from));
				else if(hdr.from_net_addr.indexOf('@')!=-1)
					writeln(format("From: \"%s\" <%s>"
						,hdr.from
						,hdr.from_net_addr));
				else
					writeln(format("From: \"%s\" <%s@%s>"
						,hdr.from
						,hdr.from.replace(/ /g,".").toLowerCase()
						,hdr.from_net_addr));

				if(hdr.path==undefined)
					hdr.path="not-for-mail";

				if(hdr.newsgroups==undefined)
					hdr.newsgroups = selected.newsgroup;
				else {	/* Tracker1's mod for adding the correct newsgroup name		*/
					var ng_found = false;					/* Requires sbbs v3.13	*/
					var ng_list = hdr.newsgroups.split(','); 
					for(n in ng_list) 
						if(ng_list[n].toLowerCase() == selected.newsgroup.toLowerCase()) {
							ng_found = true; 
							break;  
						}
					if(!ng_found)
						hdr.newsgroups = selected.newsgroup + ',' + hdr.newsgroups;
				}

				if(hdr.from_org==undefined && !hdr.from_net_type)
					hdr.from_org=system.name;

				write_news_header(hdr,writeln);	// from newsutil.js
			}
			if(hdr!=null && body!=null)	/* both, separate with blank line */
				writeln("");
			if(body!=null) {
				write(body);
				msgs_read++;
			}
			writeln(".");
			break;

		case "NEXT":
		case "LAST":
			if(!selected) {
				writeln("412 no newsgroup selected");
				break;
			}
			if(current_article<1) {
				writeln("420 no current article has been selected");
				break;
			}
			if(cmd[0].toUpperCase()=="NEXT")
				current_article++;
			else
				current_article--;
			writeln(format("223 %u %u article retrieved - request text separately"
				,current_article
				,current_article
				));
   			break;

		case "IHAVE":
			if(cmd[1]!=undefined && cmd[1].indexOf('@' + system.inetaddr)!=-1) {	// avoid dupe loop
				writeln("435 that article came from here. Don't send.");
				continue;
			}
		case "POST":
			if(user.security.restrictions&UFLAG_P) {
				writeln("440 posting not allowed");
				break;
			}
			writeln("340 send article to be posted. End with <CR-LF>.<CR-LF>");

			var posted=false;
			var header=true;
			var hfields=[];
			var body="";
			var lines=0;
			while(client.socket.is_connected) {

				line = client.socket.recvline(4096 /*maxlen*/, 300 /*timeout*/);

				if(line==null) {
					log(LOG_NOTICE,"!TIMEOUT waiting for text line");
					break;
				}

				//log(format("msgtxt: %s",line));

				if(line==".") {
					log(format("End of message text (%u chars)",body.length));
					break;
				}
				if(line=="" && header) {
					header=false;
					continue;
				}

				if(!header) {	/* Body text, append to 'body' */
					if(line.charAt(0)=='.')
						line=line.slice(1);		// Skip prepended dots
					body += line;
					body += "\r\n";
					lines++;
					continue;
				}
				log(line);

				if((line.charAt(0)==' ' || line.charAt(0)=='\t') && hfields.length) {
					while(line.charAt(0)==' '	// trim prepended spaces
						|| line.charAt(0)=='\t')	
						line=line.slice(1);
					hfields[hfields.length-1] += line;	// folded header field
				} else
					hfields.push(line);
			}

			if(impose_limit && user.limits!=undefined
				&& lines > user.limits.lines_per_message) {
				log(format("!Message of %u lines exceeds user limit (%u lines)"
					,lines,user.limits.lines_per_message));
				writeln(format("441 posting failed, number of lines (%u) exceeds max (%u)"
					,lines,user.limits.lines_per_message));
				break;
			}

			// Parse the message header
			var hdr={ from: "", subject: "" };
			for(h in hfields)
				parse_news_header(hdr,hfields[h]);	// from newsutil.js

			var newsgroups=hdr.newsgroups.split(',');

			if(hdr.to==undefined && hdr.reply_id!=undefined)
				hdr.to=getReferenceTo(hdr);

			if(hdr.to==undefined && hdr.newsgroups!=undefined)
				hdr.to=hdr.newsgroups;

			if(!(user.security.restrictions&(UFLAG_G|UFLAG_Q))) {	// !Guest and !Network Node
				hdr.from=user.alias;
				hdr.from_ext=user.number;
			}

			if(!(user.security.restrictions&UFLAG_Q))	// Treat this as a local message
				hdr.from_net_type=NET_NONE;

			/* Security logging */
			hdr.from_ip_addr	= client.ip_address;
			hdr.from_host_name	= client.host_name;
			hdr.from_protocol	= client.protocol;
			hdr.from_port		= client.port;

			if(system.trashcan("subject",hdr.subject)) {
				log(format("!BLOCKED subject: %s",hdr.subject));
				var reason = format("Blocked subject from %s (%s): %s"
					,user.alias,hdr.from,hdr.subject);
				system.spamlog("NNTP","BLOCKED"
					,reason,client.host_name,client.ip_address,hdr.to);
				writeln("441 posting failed");
				break;
			}

            for(n in newsgroups)
    			for(g in msg_area.grp_list)
				    for(s in msg_area.grp_list[g].sub_list)
					    if(msg_area.grp_list[g].sub_list[s].newsgroup.toLowerCase()
							==newsgroups[n].toLowerCase()) {
						    if(!msg_area.grp_list[g].sub_list[s].can_post)
							    continue;

						    if(msgbase!=null) {
							    msgbase.close();
							    delete msgbase;
						    }

						    msgbase=new MsgBase(msg_area.grp_list[g].sub_list[s].code);
							if(msgbase.open!=undefined && msgbase.open()==false)
								continue;

							/* NNTP Control Message? */
							if(hdr.control!=undefined) {
								var ctrl_msg = hdr.control.split(/\s+/);
								var target;
								if(ctrl_msg.length) {
									switch(ctrl_msg[0].toLowerCase()) {
										case "cancel":
											target=msgbase.get_msg_header(ctrl_msg[1]);
											if(target==null) {
												log(LOG_NOTICE,"!Invalid Message-ID: " + ctrl_msg[1]);
												break;
											}
											if(logged_in && ((target.from_ext==user.number
												&& msg_area.grp_list[g].sub_list[s].settings&SUB_DEL)
												|| msg_area.grp_list[g].sub_list[s].is_operator)) {
												if(msgbase.remove_msg(ctrl_msg[1])) {
													posted=true;
													log(LOG_NOTICE,"Message deleted: " + ctrl_msg[1]);
												} else
													log(LOG_ERR,"!ERROR " + msgbase.error + 
														" deleting message: " + ctrl_msg[1]);
												continue;
											}
											break;
									}
								}
								log(LOG_WARNING,"!Invalid control message: " + hdr.control);
								break;
							}

						    if(msg_area.grp_list[g].sub_list[s].settings&SUB_NAME
							    && !(user.security.restrictions&(UFLAG_G|UFLAG_Q)))
							    hdr.from=user.name;	// Use real names
							if(msg_area.grp_list[g].sub_list[s].is_moderated)
								hdr.attr|=MSG_MODERATED;
							else
								hdr.attr&=~MSG_MODERATED;

						    if(msgbase.save_msg(hdr,body)) {
							    log(format("%s posted a message (%u chars, %u lines) on %s"
									,user.alias, body.length, lines, newsgroups[n]));
							    posted=true;
								msgs_posted++;
						    } else
							    log(format("!ERROR saving mesage: %s",msgbase.last_error));
					    }
			if(posted)
			    writeln("240 article posted ok");
			else {
				log(LOG_ERR,"!post failure");
				writeln("441 posting failed");
			}
   			break;

		default:
			writeln("500 Syntax error or unknown command");
			log(LOG_NOTICE,"!unknown command");
			break;
	}

	if(user.security.restrictions&UFLAG_G	/* Only guest/anonymous logins can be "bogus" */
		&& bogus_cmd_counter >= max_bogus_cmds) {
		log(format("!TOO MANY BOGUS COMMANDS (%u)", bogus_cmd_counter));
		if(filter_bogus_clients) {
			log(LOG_NOTICE,"!FILTERING CLIENT'S IP ADDRESS: " + client.ip_address);
			system.filter_ip("NNTP","- TOO MANY BOGUS COMMANDS (Example: " + cmdline +")"
				, client.host_name, client.ip_address, client.user_name);
		}
		break;
	}
}

// Log statistics

if(msgs_read)
	log(format("%u messages read",msgs_read));
if(msgs_posted)
	log(format("%u messages posted",msgs_posted));

/* End of nntpservice.js */

