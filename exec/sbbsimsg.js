// sbbsimsg.js

load("sbbsdefs.js");
load("nodedefs.js");

// Global vars
var imsg_user;
var last_user=0;

print("\1n\1hSynchronet \1cInstant Message \1wModule \1nv1.00 Alpha\r\n");

// Parse arguments
for(i=0;i<argc;i++)
	if(argv[i].toLowerCase()=="-l") {
		list_users(true);
		exit();
	}

// Read the list of systems into list array
fname = system.ctrl_dir + "sbbsimsg.lst";

f = new File(fname);
if(!f.open("r")) {
	alert("Error opening " + fname);
	exit();
}

list = f.readAll();
f.close();
for(line=0;;) {
	if(list[line]==null)
		break;
	while(list[line].charAt(0)==' ')		// skip prepended spaces
		list[line]=list[line].slice(1);
	if(list[line].charAt(0)==';' ||			// comment
		list[line]==system.inetaddr) {		// local system
		list.splice(line,1);				// ignore
		continue;
	}
	sp=list[line].indexOf(' ');
	tab=list[line].indexOf('\t');
	if(tab>0 && tab<sp)
		sp=tab;
	if(sp>0)								// truncate at first space or tab
		list[line]=list[line].slice(0,sp);
	line++;
}


// Truncate space off end of string
function truncsp(str)
{
	var len;

	while((len=str.length)!=0 && str.charAt(len-1)==' ')
		str=str.slice(0,-1);

	return(str);
}

function list_users(show)
{
	imsg_user = new Array();

	for(i=0;list[i]!=null;i++) {

		if(show) 
			printf("\1n\1h%-25.25s\1n ",list[i]);

		sock = new Socket();
		if(!sock.connect(list[i],79)) {
			log(format("!Finger connection to %s FAILED with error %d"
				,list[i],sock.last_error));
			alert("system not available");
			list.splice(i,1);
			i--;
			continue;
		}
		sock.send("\r\n");	// Get list of active users
		var response=new Array();
		while(bbs.online && sock.is_connected) {
			str=sock.readline();
			if(str==null)
				break;
			response.push(truncsp(str));
		}
		sock.close();


		// Skip header
		while(response.length && response[0].charAt(0)!='-')
			response.shift();
		if(response.length && response[0].charAt(0)=='-')
			response.shift();	// Delete the separator line
		while(response.length && !response[0].length)
			response.shift();	// Delete any blank lines

		if(!response.length) {
			if(show)
				print();
			continue;
		}

		if(show) {
			str = format("%lu user%s",response.length,response.length==1 ? "":"s");
			printf("\1g\1h%-40s Age Sex\r\n",str);
		}

		for(j in response) {
			if(response[j]=="")
				continue;

			if(show)
				print(format("\1h\1y%.25s\1n\1g %.48s"
					,response[j],response[j].slice(26)));
			var u = new Object;
			u.host = list[i];
			u.name = format("%.25s",response[j]);
			u.name = truncsp(u.name);
			imsg_user.push(u);
		}
	}
}

function send_msg(dest, msg)
{

	if((hp = dest.indexOf('@'))==-1) {
		alert("Invalid user");
		exit();
	}
	host = dest.slice(hp+1);

	printf("\1h\1ySending...\r\1w");
	sock = new Socket();
	//sock.debug = true;
	do {
		if(!sock.connect(host,25)) {
			alert("Connection to " + host + " failed with error " + sock.last_error);
			break;
		}

		if(Number((rsp=sock.recvline()).slice(0,3))!=220) {
			alert("Invalid connection response:\r\n" + rsp);
			break;
		}
		sock.send("HELO "+system.inetaddr+"\r\n");
		if(Number((rsp=sock.recvline()).slice(0,3))!=250) {
			alert("Invalid HELO response: " + rsp);
			break;
		}
		sock.send("SOML FROM: "+user.email+"\r\n");
		if(Number((rsp=sock.recvline()).slice(0,3))!=250) {
			alert("Invalid SOML response: " + rsp);
			break;
		}
		sock.send("RCPT TO: "+dest+"\r\n");
		if(Number((rsp=sock.recvline()).slice(0,3))!=250) {
			alert("Invalid RCPT TO response: " + rsp);
			break;
		}
		sock.send("DATA\r\n");
		if(Number((rsp=sock.recvline()).slice(0,3))!=354) {
			alert("Invalid DATA response: " + rsp);
			break;
		}
		sock.send(msg);
		sock.send("\r\n.\r\n");
		if(Number((rsp=sock.recvline()).slice(0,3))!=250) {
			alert("Invalid end of message response: " + rsp);
			break;
		}
		sock.send("QUIT\r\n");
		print("Message delivered successfully.");

	} while(0);

	sock.close();
}

function getmsg()
{
	var lines=0;
	var msg="";
	const max_lines = 5;

	printf("\1n\1g\1h%lu\1n\1g lines maximum (blank line ends)\r\n",max_lines);
	while(bbs.online && lines<max_lines) {
		console.print("\1n: \1h");
		mode=0;
		if(lines+1<max_lines)
			mode|=K_WRAP;
		str=console.getstr(76, mode);
		if(str=="")
			break;
		msg+=str;
		msg+="\r\n";
		lines++;
	}

	if(!lines || !bbs.online || bbs.sys_status&SS_ABORT)
		return("");

	return(msg);
}

list_users(true);	// Needed to initialize user[]
console.crlf();

var key;
while(bbs.online) {
	console.line_counter=0;	// defeat pause
	console.print("\1n\1h\1bInter-BBS: ");
	console.mnemonics("~Telegram, ~Message, ~List, or ~Quit: ");
	bbs.sys_status&=~SS_ABORT;
	while(bbs.online && !(bbs.sys_status&SS_ABORT)) {
		key=console.inkey(K_UPPER);
		if(key=='Q' || key=='L' || key=='T' || key=='M' || key=='\r')
			break;
		if(system.node_list[bbs.node_num-1].misc&(NODE_MSGW|NODE_NMSG)) {
			console.line_counter=0;	// defeat pause
			console.saveline();
			console.crlf();
			bbs.nodesync();
			console.crlf();
			console.restoreline();
		}
	}
//	printf("key=%s\r\n",key);
	switch(key) {
		case 'L':
			print("\1h\1cList\r\n");
			list_users(true);
			console.crlf();
			break;
		case 'T':
			printf("\1h\1cTelegram\r\n\r\n");
			printf("\1n\1h\1y(user@hostname): \1w");
			if(imsg_user.length)
				dest=format("%s@%s",imsg_user[last_user].name,imsg_user[last_user].host);
			else
				dest="";
			dest=console.getstr(dest,64,K_EDIT|K_AUTODEL);
			if(dest==null || dest=='' || bbs.sys_status&SS_ABORT)
				break;
			if((msg=getmsg())=='')
				break;
			send_msg(dest,msg);
			console.crlf();
			break;
		case 'M':
			print("\1h\1cMessage\r\n");
			if(!imsg_user.length) {
				alert("No users!\r\n");
				break;
			}
			done=false;
			while(bbs.online && !done) {
				printf("\r\1n\1h\x11\1n-[\1hQ\1nuit]-\1h\x10 \1y%-25s \1c%s\1>"
					,imsg_user[last_user].name,imsg_user[last_user].host);
				switch(console.getkey(K_UPPER|K_NOECHO)) {
					case '+':
					case '>':
					case ']':
					case '\x06':	/* right arrow */
					case 'N':
					case '\n':		/* dn arrrow */
						last_user++;
						if(last_user>=imsg_user.length)
							last_user=0;
						break;

					case '-':
					case '<':
					case '[':
					case '\x1d':	/* left arrow */
					case 'P':
					case '\x1e':	/* up arrow */
						last_user--;
						if(last_user<0)
							last_user=imsg_user.length-1;
						break;
					case '\x1b':	/* ESC */
					case 'Q':
						printf("\r\1>");
						done=true;
						break;
					case '\r':
						done=true;
						dest=format("%s@%s"
							,imsg_user[last_user].name,imsg_user[last_user].host);
						printf("\r\1n\1cSending message to \1h%s\1>\r\n",dest);
						if((msg=getmsg())=='')
							break;
						send_msg(dest,msg);
						console.crlf();
						break;
				}
			}
			break;
		default:
			print("\1h\1cQuit");
			exit();
			break;
	}
}