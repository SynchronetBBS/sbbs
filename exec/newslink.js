// newslink.js

// Synchronet Newsgroup Link/Gateway Module

// Configuration file (in ctrl/newslink.cfg) format:

// ;this line is a comment
// server username password
// subboard	newsgroup
// subboard newsgroup
// ...

log("Synchronet NewsLink v1.00 Alpha");

var cfg_fname = system.ctrl_dir + "newslink.cfg";

load("sbbsdefs.js");

var debug = false;

// Parse arguments
for(i=0;i<argc;i++)
	if(argv[i].toLowerCase()=="-d")
		debug = true;
	else
		cfg_fname = argv[i];

// Write a string to the server socket
function write(str)
{
	socket.send(str);
}

function writeln(str)
{
	if(debug)
		log(format("rsp: %s",str));
	write(str + "\r\n");
}

var msgbase=null;
var selected=null;
var current_article=0;

cfg_file = new File(cfg_fname);
if(!cfg_file.open("r")) {
	printf("!Error %d opening %s",errno,cfg_fname);
	delete cfg_file;
	exit();
}

while(!cfg_file.eof) {
	line = cfg_file.readln();
	if(line[0] == ';' || !line.length)
		continue;
	break;
}
str = line.split(' ');
server = str[0];
printf("server: %s",server);
username = str[1];
printf("username: %s",username);
password = str[2];
printf("password: %s",password);

socket = new Socket();
socket.debug=true;
if(!socket.connect(server,119)) {
	printf("Error %d opening %s",socket.last_error,cfg_fname);
	delete socket;
	delete cfg_file;
	exit();
}

printf("rsp: %s",socket.readln());
writeln(format("AUTHINFO USER %s",username));
printf("rsp: %s",socket.readln());
writeln(format("AUTHINFO PASS %s",password));
rsp = socket.readln();
printf("rsp: %s",rsp);
if(rsp==null || rsp[0]!='2') {
	printf("!Authentication failure: %s", rsp);
	delete socket;
	delete cfg_file;
	exit();
}

while(!cfg_file.eof) {
	line = cfg_file.readln();
	if(line == null)
		break;
	if(line[0] == ';' || !line.length)
		continue;

	str = line.split(' ');
	sub = str[0];
	newsgroup = str[1];
	msgbase = new MsgBase(sub);
	if(msgbase == null) {
		printf("!ERROR opening msgbase: %s",sub);
		continue;
	}

	ptr_fname = msgbase.file + ".snl";
	ptr_file = new File(ptr_fname);
	if(!ptr_file.open("w+b")) {
		printf("!ERROR %d opening %s",errno,ptr_fname);
		delete msgbase;
		continue;
	}
	/* Open Pointer File */
	ptr = ptr_file.readBin();
	if(ptr < msgbase.first_msg)
		ptr = msgbase.first_msg;

	printf("%s ptr: %ld",sub,ptr);

	/*************************/
	/* EXPORT Local Messages */
	/*************************/
	for(;ptr<=msgbase.last_msg;ptr++) {
		hdr = msgbase.get_msg_header(false,ptr);
		if(hdr == null)
			continue;
		if(hdr.attr&MSG_DELETE)	/* marked for deletion */
			continue;
		if(hdr.attr&MSG_MODERATED && !(hdr.attr&MSG_VALIDATED))
			continue;
		if(hdr.attr&MSG_PRIVATE)/* no private messages on NNTP */
			continue;
		if(hdr.from_net_type==NET_INTERNET)	/* no dupe loop */
			continue;

		body = msgbase.get_msg_body(false, ptr
				,true /* remove ctrl-a codes */);
		if(body == null) {
			printf("!FAILED to read message number %ld",ptr);
			continue;
		}

		writeln("POST");
		rsp = socket.readln();
		printf("rsp: %s",rsp);
		if(rsp[0]!='3') {
			printf("!POST failure: %s",rsp);
			break;
		}

		if(hdr.from_net_type)
			writeln(format("From: \"%s\" <%s@%s>"
				,hdr.from,hdr.from,hdr.from_net_addr));
		else if(hdr.from.indexOf(' ')>0)
			writeln(format("From: \"%s\"@%s"
				,hdr.from,system.inetaddr));
		else
			writeln(format("From: %s@%s"
				,hdr.from,system.inetaddr));
		writeln("Subject: " + hdr.subject);
		writeln("Message-ID: " + hdr.id);
		writeln("Date: " + system.timestr(hdr.when_written_time));
		writeln("References: " + hdr.reply_id);
		writeln("Newsgroups: " + newsgroup);
		writeln("");
		write(body);
		writeln(".");
		rsp = socket.readln();
		printf("rsp: %s",rsp);
		if(rsp[0]!='2') {
			printf("!POST failure: %s",rsp);
			break;
		}
		printf("Posted message %lu to newsgroup: %s",ptr,newsgroup);
	}

	/* Save Export Pointer */
	ptr_file.position=0;
	ptr_file.writeBin(ptr);
	/* Read Import pointer */
	ptr = ptr_file.readBin();

	writeln(format("GROUP %s",newsgroup));
	rsp = socket.readln();
	printf("rsp: %s",rsp);
	if(rsp[0]!='2') {
		printf("!GROUP %s failure: %s",newsgroup,rsp);
		delete ptr_file;
		delete msgbase;
		continue;
	}
	str = rsp.split(' ');
	last_msg = str[3];

	/***************************/
	/* IMPORT Network Messages */
	/***************************/	
	for(;ptr<=last_msg;ptr++) {
		writeln(format("ARTICLE %lu",ptr));
		rsp = socket.readln();
		printf("rsp: %s",rsp);
		if(rsp[0]!='2') {
			printf("!ARTICLE %lu failure: %s",ptr,rsp);
			continue;
		}
		header=true;
		var hdr=new Object();
		while(socket.is_connected) {

			line = socket.recvline(512 /*maxlen*/, 300 /*timeout*/);

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
					hdr.from=data;
					break;
				case "subject":
					hdr.subject=data;
					break;
				/* TODO: Parse date field */
			}
		}
		hdr.from_net_type=NET_INTERNET;
		hdr.from_net_addr=hdr.from;
		if(msgbase.save_msg(hdr,body)) 
			printf("Message %lu imported into %s",ptr,sub);
	}

	/* Save Export Pointer */
	ptr_file.position=4;
	ptr_file.writeBin(ptr);
	delete ptr_file;

	delete msgbase;
}

delete socket;
delete cfg_file;

/* End of newslink.js */
