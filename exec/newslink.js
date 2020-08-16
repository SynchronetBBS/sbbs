// newslink.js

// Synchronet Newsgroup Link/Gateway Module

// $Id: newslink.js,v 1.113 2020/04/29 18:59:24 rswindell Exp $

// Configuration file (in ctrl/newslink.cfg) format:

// ;this line is a comment
// server	servername
// port		TCP port number (defaults to 119)
// user		username (optional)
// pass		password (optional)
// area		subboard (internal code) newsgroup flags [attachment_dir]
// ...

// Defined area flags:
// x		do not add tearlines & taglines to exported messages
// n		do not add "From Newsgroup" text to imported messages
// t		do not add tearline to imported messages
// a		convert extended-ASCII chars to ASCII on imported messages
// r		remove "Newsgroups:" header field from imported messages
// o		over-write "Newsgroups:" header field for exported messages
// b        decode binary attachments
// i		import all (not just new articles)
// s		no subject filtering
// m		Moderate imported messages

const REVISION = "$Revision: 1.113 $".split(' ')[1];

printf("Synchronet NewsLink %s session started\r\n", REVISION);

var tearline = format("--- Synchronet %s%s-%s NewsLink %s\r\n"
					  ,system.version,system.revision,system.platform,REVISION);
var tagline	=  format(" *  %s - %s - telnet://%s\r\n"
					  ,system.name,system.location,system.inetaddr);
var antispam = format(".remove-%s-this"
					  ,random(50000).toString(36));

var cfg_fname = system.ctrl_dir + "newslink.cfg";

var global_flags = "";	// global flags  - area flags applied to all areas

load("sbbsdefs.js");
load("newsutil.js");	// write_news_header() and parse_news_header()

var debug = false;
var slave = false;
var reader_mode = false;
var reset_import_ptrs = false;		// Reset import pointers, import all messages
var update_import_ptrs = false;		// Update import pointers, don't import anything
var reset_export_ptrs = false;		// Reset export pointers, export all messages
var update_export_ptrs = false;		// Update export pointers, don't export anything
var email_addresses = true;			// Include e-mail addresses in headers
var import_amount = 0;				// Import a fixed number of messages per group
var lines_per_yield = 5;			// Release time-slices ever x number of lines
var yield_length = 1;				// Length of yield (in milliseconds)
var max_newsgroups_per_article = 5;	// Used for spam-detection
var unmangle = false;
var use_xover = true;

// Parse arguments
for(i=0;i<argc;i++) {
	if(argv[i].toLowerCase()=="-d")			// debug
		debug = true;
	else if(argv[i].toLowerCase()=="-ri")	// reset import pointers (import all)
		reset_import_ptrs = true;
	else if(argv[i].toLowerCase()=="-ui")	// update import pointers (import none)
		update_import_ptrs = true;
	else if(argv[i].toLowerCase()=="-re")	// reset export pointers (export all)
		reset_export_ptrs = true;
	else if(argv[i].toLowerCase()=="-ue")	// update export pointers (export none)
		update_export_ptrs = true;
	else if(argv[i].toLowerCase()=="-ne")	// no e-mail addresses
		email_addresses = false;
	else if(argv[i].toLowerCase()=="-nm")	// no mangling of e-mail addresses
		antispam = "";
	else if(argv[i].toLowerCase()=="-um")	// un-mangle e-mail addresses when importing
		unmangle = true;
	else if(argv[i].toLowerCase()=="-nx")	// no xover command when importing
		use_xover = false;
	else if(argv[i].toLowerCase()=="-ix") 	// import a fixed number of messages
	{
		import_amount = parseInt(argv[i+1]);
		if(import_amount)
			i++;
		else
			import_amount = 500;			// default 500
	}
	else
		cfg_fname = argv[i];
}

if(this.js==undefined) 		// v3.10?
	js = { terminated: false };
else
	js.auto_terminate = false;

// Write a string to the server socket
function write(str)
{
	socket.send(str);
}

function writeln(str)
{
	if(debug)
		printf("cmd: %s\r\n",str);
	write(str + "\r\n");
}

// RFC977 and RFC3977 clearly state 512-bytes per response line,
// but "Timelord" reported a problem with some news server sending
// > 512-byte response lines for the XOVER command
function readln()
{
	rsp = socket.recvline(4096);
	if(debug)
		printf("rsp: %s\r\n",rsp);
	return(rsp);
}

function unique_fname(dir,fname)
{
	var new_fname=fname;
	var file_num=0;
	var ext;
	while(file_exists(dir + new_fname) && file_num<1000) { 
		// generate unique name, if necessary
		ext=fname.lastIndexOf('.');
		if(ext<0)
			ext="";
		else
			ext=fname.slice(ext);
		// Convert filename.ext to filename.<article>.ext
		new_fname=format("%.*s.%lu%s",fname.length-ext.length,fname,file_num++,ext);
	}
	return(dir + new_fname);
}

// Remove NewsLink anti-spam meature from e-mail address
function unmangle_addr(addr)
{
	return addr.replace(/\.remove-\S+-this/,"");
}

var yield_calls=0;
function maybe_yield()
{
	yield_calls++;
	if(lines_per_yield && (yield_calls%lines_per_yield)==0) {
		sleep(yield_length);
	}
}

var host;
var port=119;
var username;
var password;
var interface_ip_address=0;
var port_set=false;
var tls=false;
var no_path=false;
var area = {};

if(this.server!=undefined)
	interface_ip_address=server.interface_ip_address;

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
	if(line == null)
		continue;
	line = truncsp(line);
	if(line==null || line[0] == ';' || !line.length || line==undefined)
		continue;
	str=line.split(/\s+/);
	switch(str[0].toLowerCase()) {
		case "disabled":
			printf("!NewsLink DISABLED in %s\r\n",cfg_fname);
			delete cfg_file;
			exit();
		case "server":
			host=str[1];
			break;
		case "tls":
			tls=true;
			if(!port_set)
				port=563;
			break;
		case "port":
			port=parseInt(str[1]);
			port_set=true;
			break;
		case "interface":
			interface_ip_address=str[1];
			break;
		case "user":
			username=str[1];
			break;
		case "pass":
			password=str[1];
			break;
		case "flags":
			global_flags=str[1];
			break;
		case "area":
			area[str[1]] = { newsgroup: str[2], flags: str[3], attachment_dir: str[4] };
			break;
		case "auto_areas":
		{
			for(var s in msg_area.sub)
				if((msg_area.sub[s].settings & SUB_INET) && !area[s])
					area[s] = {};
			break;
		}
		case "debug":
			debug=true;
			break;
		case "unmangle":
			unmangle=true;
			break;
		case "no_xover":
			use_xover=false;
			break;
		case "no_path":
			no_path=true;
			break;
		case "slave":
			slave=true;
			break;
		case "reader_mode":
			reader_mode = true;
			break;
		case "tagline":
			str.shift();			// Remove first element (keyword)
			tagline=str.join(' ');		// Combine remaining elements (tagline)
			tagline+="\r\n";
			break;
		case "lines_per_yield":
			lines_per_yield=parseInt(str[1]);
			break;
		case "yield_length":
			yield_length=parseInt(str[1]);
			break;
		case "max_newsgroups_per_article":
			max_newsgroups_per_article=parseInt(str[1]);
			break;
		default:
			print("!UNRECOGNIZED configuration keyword: " + str[0]);
			break;
	}
}
cfg_file.close();
delete cfg_file;

printf("server: %s\r\n",host);
if(debug) {
	printf("username: %s\r\n",username);
	printf("password: %s\r\n",password);
}
printf("%ld areas\r\n",area.length);

if(host==undefined || !host.length) {
	print("!No news server specified");
	exit();
}

printf("Connecting to %s port %d ...\r\n",host,port);
socket = new Socket();
//socket.debug=true;
socket.bind(0,interface_ip_address);
if(!socket.connect(host,port)) {
	printf("!Error %d connecting to %s port %d\r\n"
		,socket.last_error,host,port);
	delete socket;
	exit();
}
print("Connected");
if(tls) {
	print("Negotiating TLS");
	socket.ssl_session=true;
}
readln();

if(reader_mode) {
	writeln("MODE READER");
	readln();
}

if(username!=undefined && username.length) {
	print("Authenticating...");
	writeln(format("AUTHINFO USER %s",username));
	readln();
	if(password!=undefined && password.length) {
		writeln(format("AUTHINFO PASS %s",password));
		rsp = readln();
		if(rsp==null || rsp[0]!='2') {
			printf("!Authentication FAILURE: %s\r\n", rsp);
			delete socket;
			exit();
		}
	}
	print("Authenticated");
}

if(slave) {
	writeln("slave");
	readln();
}

var stop_semaphore=system.data_dir+"newslink.stop";
file_remove(stop_semaphore);

function stop_sem_signaled()
{
	if(file_exists(stop_semaphore)) {
		log(stop_semaphore + " signaled");
		return(true);
	}
	return(false);
}

/******************************/
/* Export and Import Messages */
/******************************/
var exported=0;
var imported=0;
var subimported=0;
var subpending=0;
var twitlist_fname = system.ctrl_dir + "twitlist.cfg";
var use_twitlist = file_exists(twitlist_fname);

function article_listed(list, article)
{
	var i;

	for(i in list)
		if(list[i]==article)
			return(list.splice(i,1));

	return(false);
}

function save_ptr(ini_file, name, value)
{
	/* Save Pointers */
	if(!ini_file.open(ini_file.exists ? "r+":"w+"))
		printf("!ERROR %d creating/opening %s\r\n",errno,ini_file.name);
	else {
		ini_file.iniSetValue("NewsLink", name, value);
		ini_file.close();
	}
}

printf("Scanning %lu message bases...\r\n",area.length);
for(sub in area) {

	if(!socket.is_connected)
		break;

	if(js.terminated || stop_sem_signaled())
		break;

	newsgroup = area[sub].newsgroup;
	flags = area[sub].flags;
	if(flags==undefined)
		flags="";
	flags += global_flags;
	flags = flags.toLowerCase();

	msgbase = new MsgBase(sub);
	if(msgbase.open!=undefined && msgbase.open()==false) {
		printf("!ERROR %s opening msgbase: %s\r\n",msgbase.last_error,sub);
		delete msgbase;
		continue;
	}

	// Use default newsgroup name if not configured
	if(newsgroup==undefined || newsgroup.length<2)
		newsgroup=msgbase.cfg.newsgroup;

	attachment_dir=area[sub].attachment_dir;
	if(attachment_dir==undefined)
		attachment_dir=msgbase.cfg.data_dir+"attach";
	mkdir(attachment_dir);   
	if(attachment_dir.substr(-1)!='/')
		attachment_dir+="/";   

	printf("sub: %s, newsgroup: %s\r\n",sub,newsgroup);

	/*********************/
	/* Read Pointer File */
	/*********************/
	export_ptr = 0;
	import_ptr = NaN;			// Set to highest possible message number (by default)
	if(flags.indexOf('i')>=0)	// import all
		import_ptr = 0;

	ini_fname = msgbase.file + ".ini";
	ini_file = new File(ini_fname);
	if(ini_file.open("r")) {
		export_ptr=ini_file.iniGetValue("NewsLink","export_ptr",export_ptr);
		printf("%s.ini export ptr: %ld\r\n",sub,export_ptr);
		import_ptr=ini_file.iniGetValue("NewsLink","import_ptr",import_ptr);
		printf("%s.ini import ptr: %ld\r\n",sub,import_ptr);
		ini_file.close();
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
	if(debug)
		print("exporting local messages");
	last_msg=msgbase.last_msg;
	for(;socket.is_connected 
		&& ptr<=last_msg 
		&& !js.terminated
		&& !stop_sem_signaled()
		;ptr++) {
		if(this.console!=undefined)
			console.line_counter = 0;
		hdr = msgbase.get_msg_header(
			/* retrieve by offset? */	false,
			/* message number */		ptr
			);
		if(hdr == null) {
			alert("Failed to read msg header #" + ptr);
			continue;
		}
		if(!hdr.id) {
			alert("Message #" + ptr + " is missing a Message-ID header field");
			continue;
		}
		if(hdr.attr&MSG_DELETE)	{ /* marked for deletion */
			if(debug)
				printf("skipping deleting msg #" + ptr);
			continue;
		}
		if(hdr.attr&MSG_MODERATED && !(hdr.attr&MSG_VALIDATED)) {
			print("Stopping at unvalidated moderated message: " + ptr);
			ptr--;
			break;
		}
		if(hdr.attr&MSG_PRIVATE) { /* no private messages on NNTP */
			if(debug)
				printf("skipping deleted msg #" + ptr);
			continue;
		}
		if(hdr.from_net_type==NET_INTERNET)	{ /* no dupe loop */
			if(debug)
				printf("skipping msg from the Internet #" + ptr);
			continue;
		}
		if(hdr.from_net_type	/* don't gate messages between net types */
			&& msgbase.cfg!=undefined && !(msgbase.cfg.settings&SUB_GATE)) {
			if(debug)
				printf("skipping (not gating) networked message #" + ptr);
			continue;
		}

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
		if(flags.indexOf('x')==-1) {
			body += tearline;
			body += tagline;
		}
		if(0) 
			writeln(format("IHAVE %s",hdr.id));
		else
			writeln("POST");

		rsp = readln();
		if(rsp==null || rsp[0]!='3') {
			printf("!POST FAILURE: %s\r\n",rsp);
			break;
		}

		if(!email_addresses)
			writeln(format("From: %s@%s",hdr.from,newsgroup));
		else if(hdr.from.indexOf('@')!=-1)
			writeln(format("From: %s%s",hdr.from,antispam));
		else if(hdr.from_net_type && hdr.from_net_addr!=null) {
			if(hdr.from_net_addr.indexOf('@')!=-1)
				writeln(format("From: \"%s\" <%s%s>"
					,hdr.from
					,hdr.from_net_addr,antispam));
			else
				writeln(format("From: \"%s\" <%s@%s%s>"
					,hdr.from
					,hdr.from.replace(/ /g,".").toLowerCase()
					,hdr.from_net_addr,antispam));
		}
		else
			writeln(format("From: \"%s\" <%s@%s%s>"
				,hdr.from
				,hdr.from.replace(/ /g,".").toLowerCase()
				,system.inetaddr,antispam));

		if(hdr.newsgroups==undefined || flags.indexOf('o')>=0)
			hdr.newsgroups=newsgroup;

		if(hdr.from_org==undefined && !hdr.from_net_type)
			hdr.from_org=system.name;

		write_news_header(hdr,writeln); // from newsutil.js

		writeln("X-Gateway: "
			+ system.inetaddr
			+ " [Synchronet "
			+ system.version + system.revision 
			+ "-" + system.platform + system.beta_version
			+ " NewsLink " + REVISION
			+ "]"
			);

		writeln("");
		if(hdr.to.toLowerCase()!="all") {
			writeln("  To: " + hdr.to);
		}
		write(body);
		writeln(".");
		rsp = readln();
		if(rsp==null || rsp[0]!='2') {
			printf("!POST FAILURE: %s\r\n",rsp);
			break;
		}
		printf("Exported message %lu to newsgroup: %s\r\n",ptr,newsgroup);
		exported++;
	}
	if(ptr > msgbase.last_msg)
		ptr = msgbase.last_msg;
	export_ptr = ptr;
	save_ptr(ini_file, "export_ptr", export_ptr);

	/***************************/
	/* IMPORT Network Messages */
	/***************************/	

	writeln(format("GROUP %s",newsgroup));
	rsp = readln();
	if(rsp==null || rsp[0]!='2') {
		printf("!GROUP %s FAILURE: %s\r\n",newsgroup,rsp);
		delete ini_file;
		delete msgbase;
		continue;
	}
	str = rsp.split(' ');

	first_msg = parseInt(str[2]);
	last_msg = parseInt(str[3]);

	if(reset_import_ptrs)
		ptr = 0;
	else if(update_import_ptrs)
		ptr = last_msg;
	else if(import_amount)
		ptr = last_msg - import_amount;
	else
		ptr = import_ptr;

	printf("%s import ptr: %ld, first_msg: %ld, last_msg: %ld\r\n"
		,newsgroup,ptr,first_msg,last_msg);

	if(ptr < first_msg)
		ptr = first_msg;
	else {
		if(ptr > last_msg || isNaN(ptr))
			ptr = last_msg;
		ptr++;
	}

	delete article_list;
	subpending=0;
	if(use_xover && ptr<=last_msg) {
		writeln(format("XOVER %u-%u", ptr, last_msg));
		if(parseInt(readln())==224) {
			printf("Getting headers for articles %u through %u\r\n", ptr, last_msg);
			article_list = new Array();
			while((rsp=readln())!='.' && socket.is_connected && !js.terminated) {
				if(rsp)
					article_list.push(parseInt(rsp));
				maybe_yield();
			}
			printf("%u new articles\r\n", article_list.length);
			subpending=article_list.length;
		}
	}

	subimported=0;
	for(;socket.is_connected 
		&& ptr<=last_msg 
		&& !js.terminated
		&& !stop_sem_signaled()
		;ptr++) {
		if(this.console!=undefined)
			console.line_counter = 0;

		if(this.article_list!=undefined && !article_listed(article_list,ptr)) {
			subpending--;
			continue;
		}

		printf("Retrieving article: %u\r\n", ptr);
		writeln(format("ARTICLE %lu",ptr));
		rsp = readln();
		if(rsp==null || rsp[0]!='2') {
			printf("!ARTICLE %lu ERROR: %s\r\n",ptr,rsp);
			subpending--;
			continue;
		}
		body="";
		header=true;
		var line;
		var line_counter=0;
		var recv_lines=0;
        var file=undefined;   
		var hfields=[];
        var md5; 
		while(socket.is_connected && !js.terminated) {

			maybe_yield();

			line = readln();

			if(line==null) {
				print("!TIMEOUT waiting for text line\r\n");
				break;
			}

			recv_lines++;

			if(debug)
				printf("msgtxt: %s\r\n",line);

			if(line==".") {
				if(debug)
					print("End of message text");
				break;
			}
			if(line=="" && header) {
				header=false;
				continue;
			}

			if(header) {
				if((line.charAt(0)==' ' || line.charAt(0)=='\t') && hfields.length) {
					while(line.charAt(0)==' '	// trim prepended spaces
						|| line.charAt(0)=='\t')	
						line=line.slice(1);
					hfields[hfields.length-1] += line;	// folded header field
				} else
					hfields.push(line);
				continue;
			}

			/* Body text, append to 'body' */
			if(line.charAt(0)=='.')
				line=line.slice(1);		// Skip prepended dots

			body += line+"\r\n";
			line_counter++;

            if(flags.indexOf('b')>=0) {        // decode attachments
				if(file==undefined
					&& line.substr(0,8)=="=ybegin " 
					&& line.indexOf(" line=")>0
					&& (size=line.indexOf(" size="))>0
					&& (name=line.indexOf(" name="))>0) {

					printf("yenc header: %s\r\n",line);

					size=parseInt(line.slice(size+6),10);
					fname=file_getname(line.slice(name+6));

					if((total=line.indexOf(" total="))>0)
						total=parseInt(line.slice(total+7),10);

					if(total>1)	/* multipart */
						continue;

					/* part? */
					if((part=line.indexOf(" part="))>0)
						part=parseInt(line.slice(part+6),10);

					if(part>1) /* multipart */
						continue;

					if(part>0) {

						line = readln();

						body += line+"\r\n";

						printf("ypart header: %s\r\n",line);

						if(line.substr(0,7)!="=ypart "
							|| (begin=line.indexOf(" begin="))<1
							|| (end=line.indexOf(" end="))<1) {
							printf("!yEnc MALFORMED ypart line: %s\r\n",line);
							continue;
						}
						if(begin!=1 || end!=size)	/* multipart */
							continue;
					}

					fname=unique_fname(attachment_dir,fname);
                    file=new File(fname);   
                    file.yenc=true;   
                    if(file.open("w+b"))   
                        printf("Receiving/decoding attachment: %s\r\n",file.name);
                    else   
                        printf("!ERROR %s opening %s\r\n",errno_str,file.name);
                    continue;   
				}	

                if(file==undefined && line.substr(0,6)=="begin ") {   
					 // Parse uuencode header   
					 arg=line.split(/\s+/);   
					 arg.splice(0,2);        // strip "begin 666 "   
					 fname=file_getname(arg.join(" "));   
					 fname=unique_fname(attachment_dir,fname);    
                     file=new File(fname);   
                     file.uue=true;   
                     if(file.open("w+b"))   
                         printf("Receiving/decoding attachment: %s\r\n",file.name);   
                     else   
                         printf("!ERROR %s opening %s\r\n",errno_str,file.name);
                     continue;   
                 }    
                 if(file!=undefined && file.is_open==true) {   
                    if(file.uue && line=="end") {   
                         md5=file.md5_hex;   
                         file.close();
						 continue;
					}
					if(file.yenc && line.substr(0,6)=="=yend "
						&& (part_size=line.indexOf(" size="))>0) {
						part_size=parseInt(line.slice(part_size+6),10);
						if(part_size!=size)	{ /* multi-part, ignore */
							file.remove();
							delete file;
							continue;
						}
						if((crc32=line.indexOf(" pcrc32="))>0)
							crc32=parseInt(line.slice(crc32+8),16);
						else if((crc32=line.indexOf(" crc32="))>0)
							crc32=parseInt(line.slice(crc32+7),16);
						else
							crc32=undefined;

						if(crc32!=undefined) {
							file_crc32=file.crc32;
							if(crc32!=file_crc32) {
								printf("!yEnc CRC-32 mismatch, actual: %08lX, trailer: %s\r\n"
									,file_crc32,line);
								file.remove();
								delete file;
								continue;
							}
						}

						md5=file.md5_hex;
						file.close();
						continue;
					}

					if(!file.write(line))   
						printf("!ERROR decoding/writing: %s\r\n",line);   
				}
			}
			//print(line);
		}

		if(debug)
			print("Parsing message header");
		// Parse the message header
		var hdr={ from: "", to: newsgroup, subject: "", id: "" };
		for(h in hfields)
			parse_news_header(hdr,hfields[h]);	// from newsutil.js

		if(hdr.extra_headers!=undefined
            && hdr.extra_headers["nntp-posting-host"]!=undefined 
			&& (system.trashcan("ip", hdr.extra_headers["nntp-posting-host"]) 
				|| system.trashcan("ip-silent", hdr.extra_headers["nntp-posting-host"]))) {
			print("!Filtered IP address in NNTP-Posting-Host header field: " + hdr.extra_headers["nntp-posting-host"]);
			subpending--;
			continue;
		}

        if(file!=undefined) {   
			if(debug)
				print("Parsing attachment");

            if(file.is_open==true) { /* Incomplete attachment? */
				print("!Incomplete attachment: " + file_getname(file.name));
                file.remove();
            } else {
				/* Search for duplicate MD5 */
				print("Searching for duplicate file");
				var duplicate=false;
				md5_file=new File(attachment_dir + "md5.lst");
				if(md5_file.open("r")) {
					while(!md5_file.eof && !duplicate) {
						str=md5_file.readln();
						if(str==null)
							break;
						if(str.substr(0,32)==md5)
							duplicate=true;
					}
					md5_file.close();
				}
				if(duplicate) {   
					printf("Duplicate MD5 digest found: %s\r\n",str);   
					if(file_remove(file.name))   
						printf("Duplicate file removed: %s\r\n",file.name);   
					else   
						printf("!ERROR removing duplicate file: %s\r\n",file.name);   
					subpending--;
					continue;   
				}
				/* Append MD5 to history file */
				if(md5_file.open("a")) {
					md5_file.printf("%s %s\n",md5,file_getname(file.name));
					md5_file.close();
				} else
					printf("!ERROR %d (%s) creating/appending %s\r\n"
						,errno, errno_str, md5_file.name);
				subpending--;
				continue;
			}
        } 

		if(debug)
			print("Decoding body text");
		
		body=decode_news_body(hdr, body);

		if(truncsp(body).length==0) {
			printf("Message %lu not imported (blank)\r\n",ptr);
			subpending--;
			continue;
		}

		if(max_newsgroups_per_article && hdr.newsgroups!=undefined) {
			var ngarray=hdr.newsgroups.split(',');
			if(ngarray.length>max_newsgroups_per_article) {
				var reason = format("Too many newsgroups (%d): %s",ngarray.length, hdr.newsgroups);
				printf("!%s\r\n",reason);
				system.spamlog("NNTP","NOT IMPORTED",reason,hdr.from,host,hdr.to);
				subpending--;
				continue;
			}
		}

		if(hdr.to==newsgroup && hdr.newsgroups!=undefined)
			hdr.to=hdr.newsgroups;

		// Duplicate/looped message detection here
		if(hdr.id.indexOf('@' + system.inetaddr)!=-1) {
			subpending--;
			continue;
		}
		if(hdr.path
			&& hdr.path.indexOf(system.inetaddr)!=-1) {
			subpending--;
			continue;
		}
        if(no_path && hdr.path)
            delete hdr.path;

		if(hdr.gateway
			&& hdr.gateway.indexOf(system.inetaddr)!=-1) {
			subpending--;
			continue;
		}

		if(flags.indexOf('s')==-1 && system.trashcan("subject",hdr.subject)) {
			var reason = format("Blocked subject (%s)",hdr.subject);
			printf("!%s\r\n",reason);
			system.spamlog("NNTP","NOT IMPORTED",reason,hdr.from,host,hdr.to);
			subpending--;
			continue;
		}
		if(use_twitlist 
			&& (system.findstr(twitlist_fname,hdr.from) 
				|| system.findstr(twitlist_fname,hdr.to))) {
			printf("Filtering message from %s to %s\r\n", hdr.from, hdr.to);
			subpending--;
			continue;
		}

		if(flags.indexOf('n')==-1)
			body=format("\1n\1b\1hFrom Newsgroup\1n\1b: \1h\1c%s\1n\r\n\r\n",newsgroup) + body;

		if(flags.indexOf('a')>=0) {	// import ASCII only (convert ex-ASCII to ASCII)
			body = ascii_str(body);
			hdr.subject = ascii_str(hdr.subject);
		}
		if(flags.indexOf('r')>=0) 	// remove "Newsgroups:" header field
			delete hdr.newsgroups;

		if(hdr.from_net_addr && unmangle)
			hdr.from_net_addr = unmangle_addr(hdr.from_net_addr);

		hdr.from_net_type=NET_INTERNET;
//		hdr.from_net_addr=hdr.from;
		if(flags.indexOf('t')==-1)
			body += tearline;
		if(flags.indexOf('m')>=0)
			hdr.attr |= MSG_MODERATED;

		if(debug)
			print("Saving message");

		if(msgbase.save_msg(hdr,body)) {
			imported++;
			subimported++;
			printf("Message %lu imported into %s (%lu of %lu total) %lu lines\r\n"
				,ptr, sub, subimported, subpending, line_counter);
		} else {
			printf("!IMPORT %lu ERROR: %s\r\n", ptr, msgbase.last_error);
			subpending--;
		}
	}

	ptr--;	/* point to last requested article number */
	if(ptr > last_msg)
		ptr = last_msg;
	import_ptr = ptr;

	save_ptr(ini_file, "import_ptr", import_ptr);
	delete ini_file;
	msgbase.close();
	delete msgbase;

//	if(flags.indexOf('b')>=0)	// binary newsgroup
//		load("binarydecoder.js",sub);
}

if(!socket.is_connected)
	print("!DISCONNECTED BY SERVER");
else {
	writeln("quit");
	readln();
}

delete socket;

printf("Synchronet NewsLink %s session complete (%lu exported, %lu imported)\r\n"
	   ,REVISION, exported, imported);

/* End of newslink.js */
