// nntpservice.js

// Synchronet Service for the Network News Transfer Protocol (RFC 977)

// Example configuration (in ctrl/services.cfg):

// NNTP		119	0-unlimited	0		nntpservice.js


const VERSION = "1.00 Alpha";

var debug = false;
var authenticated = false;

// Parse arguments
for(i=0;i<argc;i++)
	if(argv[i].toLowerCase()=="-d")
		debug = true;

// Write a string to the client socket
function write(str)
{
	if(debug && 0)
		log(format("rsp: %s",str));
	client.socket.send(str);
}

function writeln(str)
{
	write(str + "\r\n");
}

var username='';
var msgbase=null;

writeln(format("200 %s News (Synchronet NNTP Service v%s)",system.name,VERSION));

while(client.socket.is_connected) {

	// Get Request
	cmdline = client.socket.recvline(512 /*maxlen*/, 30 /*timeout*/);

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
					log(format("login(%s,%s)",username,cmd[2]));
					if(login(username,cmd[2])) {
						writeln("281 Authentication successful");
						authenticated=true;
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
		case "QUIT":
			writeln("205 closing connection - goodbye!");
			exit();
			break;
	}

	if(!authenticated) {
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
						,msgbase.total_msgs
						,1
						,"y"
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
						selected=msg_area.grp_list[g].sub_list[s].newsgroup;
					}
			if(found)
				writeln(format("211 %u %u %u %s group selected"
					,msgbase.total_msgs	// articles in group
					,1	// first
					,msgbase.total_msgs	// last
					,selected
					));
			else
				writeln("411 no such news group");
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
				hdr=msgbase.get_msg_header(true,i);
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
			if(cmd[1].indexOf('<')>=0)	{ /* message-id */
				hdr=msgbase.get_msg_header(false,Number(cmd[1].slice(1,-1)));
				body=msgbase.get_msg_body(false,Number(cmd[1].slice(1,-1))
					,true /* remove ctrl-a codes */);
			} else {						/* offset */
				hdr=msgbase.get_msg_header(true,Number(cmd[1]));
				body=msgbase.get_msg_body(true,Number(cmd[1])
					,true /* remove ctrl-a codes */);
			}
			
			writeln(format("220 <%u> article retrieved - head and body follow",hdr.number));

			writeln("From: " + hdr.from);
			writeln("Subject: " + hdr.subject);
			writeln("Message-ID: " + hdr.number);
			writeln("Date: " + system.timestr(hdr.when_written_time));
			writeln("References: " + hdr.thread_orig);
			writeln("");
			write(body);
			writeln(".");
			break;
			
		default:
			writeln("500 Syntax error or unknown command");
			break;
	}
}

/* End of nntpservice.js */
