// nntpservice.js

// Synchronet Service for the Network News Transfer Protocol (RFC 977)

// Example configuration (in ctrl/services.cfg):

// NNTP		119	0-unlimited	0		nntpservice.js

load("sbbsdefs.js");

const VERSION = "1.00 Alpha";

var debug = false;
var no_anonymous = false;

// Parse arguments
for(i=0;i<argc;i++)
	if(argv[i].toLowerCase()=="-d")
		debug = true;
	else if(argv[i].toLowerCase()=="-na")
		no_anonymous = true;

// Write a string to the client socket
function write(str)
{
	if(0 && debug)
		log(format("rsp: %s",str));
	client.socket.send(str);
}

function writeln(str)
{
	write(str + "\r\n");
}

var username='';
var msgbase=null;
var selected=null;
var current_article=0;

writeln(format("200 %s News (Synchronet NNTP Service v%s)",system.name,VERSION));

if(!no_anonymous)	
	login("guest");	// Login as guest/anonymous by default

while(client.socket.is_connected) {

	// Get Request
	cmdline = client.socket.recvline(512 /*maxlen*/, 300 /*timeout*/);

	if(cmdline==null) {
		log("!TIMEOUT waiting for request");
		exit();
	}

	if(cmdline=="") 	/* ignore blank commands */
		continue;

	log(format("cmd: %s",cmdline));

	cmd=cmdline.split(' ');

	switch(cmd[0].toUpperCase()) {
		case "AUTHINFO":
			switch(cmd[1].toUpperCase()) {
				case "USER":
					username='';
					for(i=2;cmd[i]!=undefined;i++) {
						if(i>2)
							username+=' ';
						username+=cmd[i];
					}
					writeln("381 More authentication required");
					break;
				case "PASS":
					if(login(username,cmd[2])) 
						writeln("281 Authentication successful");
					else
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
		case "QUIT":
			writeln("205 closing connection - goodbye!");
			exit();
			break;
	}

	if(!logged_in) {
		writeln("502 Authentication required");
		continue;
	}

	/* These commands require login/authentication */
	switch(cmd[0].toUpperCase()) {

		case "LIST":
			writeln("215 list of newsgroups follows");
			for(g in msg_area.grp_list) 
				for(s in msg_area.grp_list[g].sub_list) {
					msgbase=new MsgBase(msg_area.grp_list[g].sub_list[s].code);
					writeln(format("%s %u %u %s"
						,msg_area.grp_list[g].sub_list[s].newsgroup
						,msgbase.last_msg
						,msgbase.first_msg
						,msg_area.grp_list[g].sub_list[s].can_post ? "y" : "n"
						));
					msgbase.close();
				}
			writeln(".");	// end of list
			break;

		case "GROUP":
			found=false;
			for(g in msg_area.grp_list) 
				for(s in msg_area.grp_list[g].sub_list) 
					if(msg_area.grp_list[g].sub_list[s].newsgroup.toLowerCase()==cmd[1].toLowerCase()) {
						found=true;
						msgbase=new MsgBase(msg_area.grp_list[g].sub_list[s].code);
						selected=msg_area.grp_list[g].sub_list[s];
					}
			if(found)
				writeln(format("211 %u %u %u %s group selected"
					,msgbase.total_msgs	// articles in group
					,msgbase.first_msg
					,msgbase.last_msg
					,selected.newsgroup
					));
			else
				writeln("411 no such news group");
			break;

		case "XOVER":
			if(msgbase==null) {
				writeln("412 no news group selected");
				break;
			}
			writeln("224 Overview information follows");
			var first, last;
			if(cmd[1].indexOf('-')>=0)	{ /* range */
				range=cmd[1].split('-');
				first=Number(range[0]);
				last=Number(range[1]);
			} else
				first=last=Number(cmd[1]);
			for(i=first;i<=last;i++) {
				hdr=msgbase.get_msg_header(false,i);
				if(hdr==null)
					continue;
				writeln(format("%u\t%s\t%s\t%s\t%u\t%u\t%u\t%u"
					,i
					,hdr.subject
					,hdr.from
					,system.timestr(hdr.when_written_time)
					,hdr.number			// message-id
					,hdr.thread_orig	// references
					,hdr.data_length	// byte count
					,Math.round(hdr.data_length/79)	// line count
					));
			}
			writeln(".");	// end of list
			break;

		case "XHDR":
			if(msgbase==null) {
				writeln("412 no news group selected");
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
				var field="";
				switch(cmd[1].toLowerCase()) {	/* header */
					case "subject":
						field=hdr.subject;
						break;
					case "from":
						field=hdr.from;
						break;
					case "reply-to":
						field=hdr.reply_to;
						break;
					case "date":
						field=system.timestr(hdr.when_written_time);
						break;
					case "message-id":
						field=hdr.number;
						break;
					case "references":
						field=hdr.thread_orig;
						break;
					case "lines":
						field=Math.round(hdr.data_length/79);
						break;
				}

				writeln(format("%u %s",i,field.toString()));
			}
			writeln(".");	// end of list
			break;

		case "ARTICLE":
		case "HEAD":
		case "BODY":
			if(cmd[1]!='') {
				if(cmd[1].indexOf('<')>=0)		/* message-id */
					current_article=Number(cmd[1].slice(1,-1));
				else 
					current_article=Number(cmd[1]);
			}
			if(current_article<1) {
				writeln("420 no current article has been selected");
				break;
			}

			hdr=null;
			body=null;
			hdr=msgbase.get_msg_header(false,current_article);
			if(cmd[0].toUpperCase()!="HEAD") 
				body=msgbase.get_msg_body(false,current_article
					,true /* remove ctrl-a codes */);

/* Eliminate dupe loops 
			if(user.security.restrictions&UFLAG_Q && hdr!=null) 
*/
			if(hdr==null) {
				writeln("430 no such arctile found");
				break;
			}
			if(hdr.attr&MSG_MODERATED && !(hdr.attr&MSG_VALIDATED)) {
				writeln("430 unvalidated message");
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
					writeln(format("220 <%u> article retrieved - head and body follow",hdr.number));
					break;
				case "HEAD":
					writeln(format("221 <%u> article retrieved - header follows",hdr.number));
					break;
				case "BODY":
					writeln(format("222 <%u> article retrieved - body follows",current_article));
					break;
			}

			if(cmd[0].toUpperCase()!="BODY") {
				if(hdr.from_net_type)
					writeln(format("From: \"%s\" <%s@%s>"
						,hdr.from,hdr.from,hdr.from_net_addr));
				else
					writeln("From: " + hdr.from);
				writeln("Subject: " + hdr.subject);
				writeln("Message-ID: " + hdr.number);
				writeln("Date: " + system.timestr(hdr.when_written_time));
				writeln("References: " + hdr.thread_orig);
				writeln("Newsgroups: " + selected.newsgroup);
			}
			if(hdr!=null && body!=null)	/* both, separate with blank line */
				writeln("");
			if(body!=null) 
				write(body);
			writeln(".");
			break;

		case "NEXT":
		case "LAST":
			if(msgbase==null) {
				writeln("412 no news group selected");
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

		case "POST":
/**
			if(!selected.can_post) {
				writeln("440 posting not allowed");
				break;
			}
**/
			writeln("340 send article to be posted. End with <CR-LF>.<CR-LF>");

			var hdr=new Object();
			if(!(user.security.restrictions&(UFLAG_G|UFLAG_Q))) {	// !Guest and !Network Node
				hdr.from=user.alias;
				hdr.from_ext=user.number;
			}

			var posted=false;
			var header=true;
			var body="";
			var newsgroups="";
			while(client.socket.is_connected) {

				line = client.socket.recvline(512 /*maxlen*/, 300 /*timeout*/);

				if(line==null) {
					log("!TIMEOUT waiting for text line");
					break;
				}

				//log(format("msgtxt: %s",line));

				if(line==".") {
					log("End of message text");
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
					continue;
				}
				log(line);

				/* Parse header lines */
				if((sp=line.indexOf(':'))==-1)
					continue;

				data=line.slice(sp+1);
				while(data.charAt(0)==' ')	// skip prepended spaces
					data=data.slice(1);

				line=line.substr(0,sp);
				while(line.charAt(0)==' ')	// skip prepended spaces
					line=line.slice(1);

				switch(line.toLowerCase()) {
					case "from":
						if(user.security.restrictions&(UFLAG_G|UFLAG_Q)) // Guest or Network Node
							hdr.from=data;
						break;
					case "subject":
						hdr.subject=data;
						break;
					case "newsgroups":
						newsgroups=data;
						break;
				}
			}

			for(g in msg_area.grp_list) 
				for(s in msg_area.grp_list[g].sub_list) 
					if(msg_area.grp_list[g].sub_list[s].newsgroup.toLowerCase()==newsgroups.toLowerCase()) {
						if(!msg_area.grp_list[g].sub_list[s].can_post)
							continue;

						if(msgbase!=null) {
							msgbase.close();
							delete msgbase;
						}

						msgbase=new MsgBase(msg_area.grp_list[g].sub_list[s].code);
						if(msgbase.save_msg(hdr,body)) {
							log(format("%s posted a message on %s",user.alias,newsgroups));
							writeln("240 article posted ok");
							posted=true;
						} else 
							log(format("!ERROR saving mesage: %s",msgbase.last_error));
					}
			if(!posted) {
				log("post failure");
				writeln("441 posting failed");
			}
   			break;

		default:
			writeln("500 Syntax error or unknown command");
			break;
	}
}

/* End of nntpservice.js */
