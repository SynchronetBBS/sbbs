// str_cmds.js

// Global String Command Module for Synchronet

// $Id$

// @format.tab-size 4, @format.use-tabs true

// This is NOT a command shell, DO NOT add to SCFG->Command Shells

// This module is loaded from command shells with the load() function

// It contains mostly sysop commands (i.e. ;ERR)

// The command string must be the first argument string
// when this module is loaded.

load("sbbsdefs.js");
load("nodedefs.js");
load("text.js");

if(argc>0) {
	str_cmds(argv.join(" "));
}

// For testing...
//str_cmds(console.getstr("",60));

function str_cmds(str)
{
	var file;	// File
	var word;	// First word of args
	var i;		// Temp integer
	var j;		// Temp integer
	var k;		// Temp integer
	var l;		// Temp integer
	var m;		// Temp integer
	var a;		// Temp array
	var s;		// Temp string

	// Remove any trailing spaces

	str=truncsp(str);
	// Upper-Case first word
	str=str.replace(/^\s*([^\s]*)/,
		function(st, p, oset, s) {
			word=p.toUpperCase();
			return(word);
		}
	);

	log(str);
	if(user.compare_ars("SYSOP")) {
		// Change node action to "sysop activities"
		bbs.node_action=NODE_SYSP;
		//sync

		// ######################## SYSOP Functions ##############################

		if(str=="ERR") {
			if(file_exists(system.logs_dir+"error.log")) {
				write(bbs.text(ErrorLogHdr));
				console.printfile(system.logs_dir+"error.log");
				if(!console.noyes(bbs.text(DeleteErrorLogQ)))
					file_remove(system.logs_dir+"error.log");
			}
			else {
				write(bbs.text(NoErrorLogExists));
			}
			for(i=0;i<system.nodes;i++) {
				if(system.node_list[i].errors)
					break;
			}
			if(i<system.nodes) {
				if(!console.noyes(bbs.text(ClearErrCounter))) {
					for(i=0;i<system.nodes; i++) {
						system.node_list[i].errors=0;
					}
				}
			}
			return;
		}

		if(str=="GURU") {
			if(file_exists(system.logs_dir+"guru.log")) {
				console.printfile(system.logs_dir+"guru.log");
				console.crlf();
				if(!console.noyes(bbs.text(DeleteGuruLogQ)))
					file_remove(system.logs_dir+"guru.log");
			}
		}

		if(str=="CHUSER") {
			// Prompts for syspass
			bbs.change_user();
			return;
		}

		if(str=="ANSCAP") {
			bbs.sys_status^=SS_ANSCAP;
			printf(bbs.text(ANSICaptureIsNow),bbs.sys_status&SS_ANSCAP?bbs.text(ON):bbs.text(OFF));
			return;
		}

		if(str=="LIST") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				console.printfile(get_arg(str));
				return;
			}
		}

		if(str=="EDIT") {
			if(bbs.check_syspass()) {
				write(bbs.text(Filename));
				if((str=console.getstr("",60))!=null) {
					editfile(str);
				}
			}
		}

		if(str=="LOG") {
			if(bbs.check_syspass()) {
				str=system.logs_dir+strftime("logs/%m%d%y.log",time());
				console.printfile(str);
			}
			return;
		}

		if(str=="YLOG") {
			if(bbs.check_syspass()) {
				str=system.logs_dir+strftime("logs/%m%d%y.log",time()-24*60*60);
				console.printfile(str);
			}
			return;
		}

		if(str=="SS") {
			bbs.sys_stats();
			return;
		}

		if(word=="NS") {
			str=str.substr(2);
			str=str.replace(/^\s+/,"");
			bbs.node_stats(parseInt(str));
			return;
		}

		if(word=="EXEC") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				bbs.exec(get_arg(str),EX_OUTR|EX_INR|EX_OUTL);
			}
			return;
		}

		if(word=="NEXEC") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				bbs.exec(get_arg(str),EX_OUTR|EX_INR|EX_OUTL|EX_NATIVE);
			}
			return;
		}

		if(word=="FOSSIL") {
			if(bbs.check_syspass()) {
				str=str.substr(6);
				bbs.exec(get_arg(str));
			}
			return;
		}

		if(word=="CALL") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				file=new File(system.data_dir+"qnet/"+get_arg(str)+".now");
				if(file.open("w"))
					file.close();
			}
			return;
		}

		if(word=="NODE") {
			bbs.exec(system.exec_dir+str.toLowerCase(), EX_OUTR|EX_INR|EX_OUTL|EX_NATIVE);
			return;
		}

		if(word=="DOWN") {
			str=str.substr(4);
			i=parseInt(get_arg(str));
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				if(system.node_list[i].status==NODE_WFC)
					system.node_list[i].status=NODE_OFFLINE;
				else
					system.node_list[i].misc^=NODE_DOWN;
				display_node(i+1);
			}
			return;
		}

		if(word=="RERUN") {
			str=str.substr(5);
			i=parseInt(get_arg(str));
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_RRUN;
				display_node(i+1);
			}
			return;
		}

		if(str=="SLOG") {
			bbs.exec(system.exec_dir+"slog /p",EX_OUTR|EX_INR|EX_OUTL|EX_NATIVE);
			return;
		}

		if(str=="NLOG") {
			bbs.exec(system.exec_dir+"slog "+system.node_dir+" /p",EX_OUTR|EX_INR|EX_OUTL|EX_NATIVE);
			return;
		}

		if(word=="NLOG") {
			str=str.substr(5);
			bbs.exec(system.exec_dir+"slog "+system.node_dir+"../node"+get_arg(str)+" /p",EX_OUTR|EX_INR|EX_OUTL|EX_NATIVE);
			return;
		}

		if(word=="UEDIT") {
			// Prompts for syspass
			str=str.substr(5);
			bbs.edit_user(str);
			return;
		}

		if(str=="MAIL") {
			bbs.read_mail(MAIL_ALL);
			return;
		}

		if(str=="BULKMAIL") {
			write("\r\nEnter ARS matches to send mail to or [CR] to send ");
			write("by name/number\r\nARS to match: ");
			str=console.getstr("", 40);
			if(str!=null)
				bbs.bulk_mail(str);
			return;
		}

		if(str=="DOS") {	// DOS/Windows shell
			if(bbs.check_syspass()) {
				bbs.exec("command.com",EX_OUTR|EX_INR|EX_OUTL|EX_NATIVE);
			}
			return;
		}

		// ToDo %y doesn't do the trick here baby.
		// Isn't there a JS thing that tells me this one?
		if(str=="SHELL") {	// Unix shell (-i for interactive)
			if(bbs.check_syspass()) {
				if(system.platform != 'Win32')
					bbs.exec(bbs.cmdstr("%y -i"),EX_OUTR|EX_INR|EX_OUTL|EX_NATIVE);
				else
					bbs.exec(bbs.cmdstr("%y"),EX_OUTR|EX_INR|EX_OUTL|EX_NATIVE);
			}
			return;
		}

		if(word=="SPY") {
			if(bbs.check_syspass()) {
				str=str.substr(3);
				writeln("");
				bbs.spy(parseInt(get_arg(str)));
				write("\1n\r\nSpy session complete.\r\n");
			}
			return;
		}

		if(str=="DIR") {
			// Dir of current lib:
			if(bbs.check_syspass()) {
				var files=0;
				var bytes=0;
				var dirs=0;
				str=file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].path;
				write("\r\nDirectory of: "+str+"\r\n\r\n");
				a=directory(str+"*",GLOB_NOSORT);
				for(i=0; i<a.length; i++) {
					j=file_date(a[i]);
					if(system.settings & SYS_EURODATE)
						write(strftime("%d/%m/%Y  %I:%M %p    ",j).toUpperCase());
					else
						write(strftime("%m/%d/%Y  %I:%M %p    ",j).toUpperCase());
					if(file_isdir(a[i])) {
						write("<DIR>          ");
						dirs++;
					}
					else {
						j=file_size(a[i]);
						write(add_commas(j,14)+" ");
						files++;
						bytes+=j;
					}
					write(file_getname(a[i]));
					console.crlf();
				}
                write(add_commas(files,16)+" File(s)");
                writeln(add_commas(bytes,15)+" bytes");
				write(add_commas(dirs,16)+" Dir(s)");
				writeln(add_commas(dir_freespace(str),16)+" bytes free");
			}
			return;
		}

		if(word=="LOAD") {
			str=str.substr(4);
			bbs.load_text(get_arg(str));
			return;
		}

		if(word=="DIR") {
			if(bbs.check_syspass()) {
				var files=0;
				var bytes=0;
				var dirs=0;
				str=str.substr(3);
				str=get_arg(str);
				str=backslash(str);
				write("\r\nDirectory of: "+str+"\r\n\r\n");
				a=directory(str+"*",GLOB_NOSORT);
				for(i=0; i<a.length; i++) {
					j=file_date(a[i]);
					write(system.datestr(j)+"  ");
					write(strftime("%I:%M %p    ",j).toUpperCase());
					if(file_isdir(a[i])) {
						write("<DIR>          ");
						dirs++;
					}
					else {
						j=file_size(a[i]);
						write(add_commas(j,14)+" ");
						files++;
						bytes+=j;
					}
					write(file_getname(a[i]));
					console.crlf();
				}
                                write(add_commas(files,16)+" File(s)");
                                writeln(add_commas(bytes,15)+" bytes");
				write(add_commas(dirs,16)+" Dir(s)");
				writeln(add_commas(dir_freespace(str),16)+" bytes free");
			}
			return;
		}

		if(word=="UPLOAD") {
			// ToDo This apparently is broken.
			str=str.substr(7);
			bbs.bulk_upload(str);
			return;
		}

		if(word=="ALTUL") {
			str=str.substr(6);
			bbs.alt_ul_dir=(str+0);
			printf(bbs.text(AltULPathIsNow),bbs.alt_ul_dir?bbs.alt_ul_dir:bbs.text(OFF));
			return;
		}

		if(word=="RESORT") {
			for(i=0;i<system.nodes;i++) {
				if(i!=bbs.node_num-1) {
					if(system.node_list[i].stats==NODE_INUSE
							|| system.node_list[i].stats==NODE_QUIET)
						break;
				}
			}
			if(i<system.nodes) {
				write(bbs.text(ResortWarning));
				return;
			}
			if(str.search(/^ALL$/i)!=-1) {
				for(i=0;i<file_area.lib_list.length;i++) {
					for(j=0;j<file_area.lib_list[i].dir_list.length;j++) {
						bbs.resort_dir(file_area.lib_list[i].dir_list[j].number);
					}
				}
			}
			else if(str.search(/^LIB$/i)!=-1) {
				for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++) {
					bbs.resort_dir(file_area.lib_list[bbs.curlib].dir_list[j].number);
				}
			}
			else {
				bbs.resort_dir(undefined);
			}
			str=str.substr(7);
			return;
		}

		if(word=="OLDUL" || word=="OLD" || word=="OFFLINE" || word=="CLOSE") {
			str=str.replace(/^[A-Z]*\s/,"");
			if(file_area.lib_list.length<1)
				return;
			s=bbs.get_filespec();
			if(s==null)
				return;
			s=s.replace(/^(.*)(\..*)?$/,
				function(s, p1, p2, oset, s) {
					if(p2==undefined)
						return(format("%-8.8s    ",p1));
					return(format("%-8.8s%-4.4s",p1,p2));
				}
			);
			write("\r\nSearching ");
			if(str.toUpperCase()=="ALL")
				write("all libraries");
			else if(str.toUpperCase()=="LIB")
				write("library");
			else
				write("directory");
			write(" for files ");
			if(word=="OLDUL") {
				printf("uploaded before %s\r\n", system.timestr(bbs.new_file_time));
				m=FI_OLDUL;
			}
			else if(word=="OLD") {
				printf("not downloaded since %s\r\n", system.timestr(bbs.new_file_time));
				m=FI_OLD;
			}
			else if(word=="OFFLINE") {
				write("not online...\r\n");
				m=FI_OFFLINE;
			}
			else {
				write("currently open...\r\n");
				m=FI_CLOSE;
			}
			k=0;
			if(str.toUpperCase()=="ALL") {
				for(i=0;i<file_area.lib_list.length;i++) {
					for(j=0;j<file_area.lib_list[i].dir_list.length;j++) {
						/* ToDo... there's an offline check in here */
						/* if(cfg.lib[i]->offline_dir==usrdir[i][j])
                               continue; */
						l=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number,s,m);
						if(l==-1)
							return;
						k+=l;
					}
				}
			}
			else if(str.toUpperCase()=="LIB") {
				for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++) {
					/* ToDo... there's an offliune check in here */
					/* if(cfg.lib[usrlib[curlib]]->offline_dir==usrdir[curlib][i])
                           continue; */

					l=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[j].number,s,m);
					if(l==-1)
						return;
					k+=l;
				}
			}
			else {
				l=bbs.list_file_info(undefined,s,m);
				if(l==-1)
					return;
				k+=l;
			}
			if(k>1)
				printf(bbs.text(NFilesListed),k);
			return;
		}

		if(word=="GET") {
			str=str.substr(3);
			str=str.replace(/^\s+/,"");
			if(str=="") {
				write("\r\nPath and filename: ");
				str=console.getstr("",60);
				if(str=="")
					return;
			}
			if(!file_exists(str)) {
				write(bbs.text(FileNotFound));
				return;
			}
			if(!bbs.check_syspass())
				return;

			// ToDo "GET" not implemented
			writeln("\r\nFILE_GET not implemented!");
			return;
		}

		if(word=="PUT") {
			str=str.substr(3);
			str=str.replace(/^\s+/,"");
			if(str=="") {
				write("\r\nPath and filename: ");
				str=console.getstr("",60);
				if(str=="")
					return;
			}
			if(!file_exists(str)) {
				write(bbs.text(FileNotFound));
				return;
			}
			if(!bbs.check_syspass())
				return;

			// ToDo "PUT" not implemented
			writeln("\r\nPUT not implemented!");
			return;
		}
	}

//############################# Exemption Functions #############################


//# Quiet Node
	if(user.compare_ars("exempt Q")) {
		if(str=="QUIET") {
			if(user.compare_ars("QUIET"))
				system.node_list[bbs.node_num-1].status=NODE_INUSE;
			else
				system.node_list[bbs.node_num-1].status=NODE_QUIET;
			display_node(bbs.node_num);
			return;
		}

		if(str=="ANON") {
			bbs.node_settings ^= NODE_ANON;
			display_node(bbs.node_num);
			return;
		}
	}

// Lock Node
	if(user.compare_ars("exempt N")) {
		if(word=="LOCK") {
			str=str.substr(4);
			i=parseInt(get_arg(str));
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_LOCK;
				display_node(i+1);
			}
			return;
		}
	}
			
// Interrupt Node
	if(user.compare_ars("exempt I")) {
		if(word=="INTR") {
			str=str.substr(4);
			i=parseInt(get_arg(str));
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_INTR;
				display_node(i+1);
			}
			return;
		}
	}

// Chat
	if(user.compare_ars("exempt C")) {
		if(str=="CHAT") {
			bbs.page_sysop();
			return;
		}
	}

	if(str=="POFF") {
		bbs.node_settings ^= NODE_POFF;
	}
}

//### Generic routine to ask user for parameter if one wasn't specified ###

function get_arg(str)
{
	str=str.replace(/^\s+/,"");
	if(str=="") {
		write("Parameter(s): ");
		str=console.getstr();
	}

	return(str);
}

function display_node(node)
{
	var hour;
	var mer;

	printf("Node %2d: ",node);
	switch(system.node_list[node-1].status) {
		case NODE_WFC:
			write("Waiting for call");
			break;
		case NODE_OFFLINE:
			write("Offline");
			break;
		case NODE_NETTING:
			write("Networking");
			break;
		case NODE_LOGON:
			write("At logon prompt");
			break;
		case NODE_EVENT_WAITING:
			write("Waiting for all nodes to become inactive");
			break;
		case NODE_EVENT_LIMBO:
			write("Waiting for node %d to finish external event",system.node_list[node-1].aux);
			break;
		case NODE_EVENT_RUNNING:
			write("Running external event");
			break;
		case NODE_NEWUSER:
			write("New user");
			write(" applying for access ");
			if(!system.node_list[node-1].connection)
				write("locally");
			else if(system.node_list[node-1].connection==0xffff)
				write("via telnet");
			else
				printf("at %ubps",system.node_list[node-1].connection);
			break;
		case NODE_QUIET:
		case NODE_INUSE:
			printf("User #%d",system.node_list[node-1].useron);
			write(" ");
			switch(system.node_list[node-1].action) {
				case NODE_MAIN:
					write("at main menu");
					break;
				case NODE_RMSG:
					write("reading messages");
					break;
				case NODE_RMAL:
					write("reading mail");
					break;
				case NODE_RSML:
					write("reading sent mail");
					break;
				case NODE_RTXT:
					write("reading text files");
					break;
				case NODE_PMSG:
					write("posting message");
					break;
				case NODE_SMAL:
					write("sending mail");
					break;
				case NODE_AMSG:
					write("posting auto-message");
					break;
				case NODE_XTRN:
					if(!system.node_list[node-1].aux)
						write("at external program menu");
					else
						write("running external program #%d",system.node_list[node-1].aux);
					break;
				case NODE_DFLT:
					write("changing defaults");
					break;
				case NODE_XFER:
					write("at transfer menu");
					break;
				case NODE_RFSD:
					printf("retrieving from device #%d",system.node_list[node-1].aux);
					break;
				case NODE_DLNG:
					write("downloading");
					break;
				case NODE_ULNG:
					write("uploading");
					break;
				case NODE_BXFR:
					write("transferring bidirectional");
					break;
				case NODE_LFIL:
					write("listing files");
					break;
				case NODE_LOGN:
					write("logging on");
					break;
				case NODE_LCHT:
					write("in local chat with sysop");
					break;
				case NODE_MCHT:
					if(system.node_list[node-1].aux) {
						printf("in multinode chat channel %d",system.node_list[node-1].aux&0xff);
						if(system.node_list[node-1].aux&0x1f00) { /* password */
							write('*');
							printf(" %s",unpackchatpass(node));
						}
					}
					else
						write("in multinode global chat channel");
					break;
				case NODE_PAGE:
					printf("paging node %u for private chat",system.node_list[node-1].aux);
					break;
				case NODE_PCHT:
					printf("in private chat with node %u",system.node_list[node-1].aux);
					break;
				case NODE_GCHT:
					write("chatting with The Guru");
					break;
				case NODE_CHAT:
					write("in chat section");
					break;
				case NODE_TQWK:
					write("transferring QWK packet");
					break;
				case NODE_SYSP:
					write("performing sysop activities");
					break;
				default:
					write(system.node_list[node-1].action);
					break;
			}
			if(!system.node_list[node-1].connection)
				write(" locally");
			else if(system.node_list[node-1].connection==0xffff)
				write(" via telnet");
			else
				printf(" at %ubps",system.node_list[node-1].connection);
			if(system.node_list[node-1].action==NODE_DLNG) {
				if((system.node_list[node-1].aux/60)>=12) {
					if(system.node_list[node-1].aux/60==12)
						hour=12;
					else
						hour=(system.node_list[node-1].aux/60)-12;
					mer="pm";
				}
				else {
					if((system.node_list[node-1].aux/60)==0)    /* 12 midnite */
						hour=12;
					else hour=system.node_list[node-1].aux/60;
						mer="am";
				}
				printf(" ETA %02d:%02d %s"
					,hour,system.node_list[node-1].aux-((system.node_list[node-1].aux/60)*60),mer);
			}
			break;
	}
	if(system.node_list[node-1].misc&(NODE_LOCK|NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG)) {
		write(" (");
		if(system.node_list[node-1].misc&NODE_AOFF)
			write('A');
		if(system.node_list[node-1].misc&NODE_LOCK)
			write('L');
		if(system.node_list[node-1].misc&(NODE_MSGW|NODE_NMSG))
			write('M');
		if(system.node_list[node-1].misc&NODE_POFF)
			write('P');
		write(')');
	}
	if(((system.node_list[node-1].misc
		&(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN))
		|| system.node_list[node-1].status==NODE_QUIET)) {
		write(" [");
		if(system.node_list[node-1].misc&NODE_ANON)
			write('A');
		if(system.node_list[node-1].misc&NODE_INTR)
			write('I');
		if(system.node_list[node-1].misc&NODE_RRUN)
			write('R');
		if(system.node_list[node-1].misc&NODE_UDAT)
			write('U');
		if(system.node_list[node-1].status==NODE_QUIET)
			write('Q');
		if(system.node_list[node-1].misc&NODE_EVENT)
			write('E');
		if(system.node_list[node-1].misc&NODE_DOWN)
			write('D');
		if(system.node_list[node-1].misc&NODE_LCHAT)
			write('C');
		write(']'); }
	if(system.node_list[node-1].errors)
		printf(" %d error%s",system.node_list[node-1].errors, system.node_list[node-1].errors>1 ? 's' : '' );
	printf("\n");
}

function add_commas(val, pad)
{
	var s;

	s=val.toString();
	s=s.replace(/([0-9]+)([0-9]{3})$/,"$1,$2");
	while(s.search(/[0-9]{4}/)!=-1)
		s=s.replace(/([0-9]+)([0-9]{3}),/g,"$1,$2,");
	while(s.length < pad)
		s=" "+s;
	return(s);
}

function unpackchatpass(node)
{
    var bits;
    var i;
	var pass;

	pchars=new Array();
    pchars[0]=(system.node_list[node-1].aux&0x1f00)>>8;
    pchars[1]=(((system.node_list[node-1].aux&0xe000)>>13)|((system.node_list[node-1].extaux&0x3)<<3));
    bits=2;
    for(i=2;i<8;i++) {
        pchars[i]=((system.node_list[node-1].extaux>>bits)&0x1f);
        bits+=5;
	}
    pchars[8]=0;
    for(i=0;i<8;i++) {
		if(pchars[8])
			pass+=String.fromCharCode(pchars[i]+64);
		else
			break;
	}
    return(pass);
}

function get_lib_index(lib)
{
	var i;

	for(i=0;i<file_area.lib_list.length;i++) {
		if(file_area.lib_list[i].number==lib)
			return(i);
	}
	return(-1);
}

