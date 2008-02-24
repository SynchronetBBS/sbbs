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

if(argc>0)
	str_cmds(argv.join(" "));	// use command-line arguments if supplied
else if(bbs.command_str && bbs.command_str != '')
	str_cmds(bbs.command_str);	// otherwise, use command shell 'str' var, if supported (v3.13b)

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
	if(str=="HELP")
		write("\r\nAvailable STR commands (prefix with a semi-colon)\r\n\r\n");

	if(user.compare_ars("SYSOP")) {
		// Change node action to "sysop activities"
		bbs.node_action=NODE_SYSP;
		//sync

		// ######################## SYSOP Functions ##############################
		if(str=="HELP") {
			writeln("ERR\tDisplay currrent error log and opptionally delete it as well as");
			writeln("\toptionally clearing all nodes error counters.");
		}
		if(str=="ERR") {
			var errlog=system.logs_dir+"error.log";
			if(file_exists(errlog)) {
				write(bbs.text(ErrorLogHdr));
				console.printfile(errlog);
				if(!console.noyes(bbs.text(DeleteErrorLogQ)))
					file_remove(errlog);
			}
			else {
				write(format(bbs.text(FileDoesNotExist),errlog));
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

		if(str=="HELP")
			writeln("GURU\tDisplay and optionally clear current guru log.");
		if(str=="GURU") {
			if(file_exists(system.logs_dir+"guru.log")) {
				console.printfile(system.logs_dir+"guru.log");
				console.crlf();
				if(!console.noyes(bbs.text(DeleteGuruLogQ)))
					file_remove(system.logs_dir+"guru.log");
			}
		}

		if(str=="HELP")
			writeln("CHUSER\tBecome a different user.");
		if(str=="CHUSER") {
			// Prompts for syspass
			bbs.change_user();
			return;
		}

		if(str=="HELP")
			writeln("ANSCAP\tToggle ANSI capture.");
		if(str=="ANSCAP") {
			bbs.sys_status^=SS_ANSCAP;
			printf(bbs.text(ANSICaptureIsNow),bbs.sys_status&SS_ANSCAP?bbs.text(ON):bbs.text(OFF));
			return;
		}

		if(str=="HELP") {
			writeln("LIST <filename>");
			writeln("\tDisplays a file.");
		}
		if(str=="LIST") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				console.printfile(get_arg(str));
				return;
			}
		}

		if(str=="HELP")
			writeln("EDIT\tEdits a specified file using your message editor.");
		if(str=="EDIT") {
			if(bbs.check_syspass()) {
				write(bbs.text(Filename));
				if((str=console.getstr("",60))!=null) {
					console.editfile(str);
				}
			}
		}

		if(str=="HELP")
			writeln("LOG\tDisplays todays activity log");
		if(str=="LOG") {
			if(bbs.check_syspass()) {
				str=system.logs_dir+strftime("logs/%m%d%y.log",time());
				console.printfile(str);
			}
			return;
		}

		if(str=="HELP")
			writeln("YLOG\tDisplays yesterdays activity log.");
		if(str=="YLOG") {
			if(bbs.check_syspass()) {
				str=system.logs_dir+strftime("logs/%m%d%y.log",time()-24*60*60);
				console.printfile(str);
			}
			return;
		}

		if(str=="HELP")
			writeln("SS\tDisplays current system stats");
		if(str=="SS") {
			bbs.sys_stats();
			return;
		}

		if(str=="HELP")
			writeln("NS <#>\tDisplays the current node stats for node #.");
		if(word=="NS") {
			str=str.substr(2);
			str=str.replace(/^\s+/,"");
			bbs.node_stats(parseInt(str));
			return;
		}

		if(str=="HELP") {
			writeln("EXEC [command]");
			writeln("\texecutes command (or prompts for it) with I/O redirected.");
		}
		if(word=="EXEC") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				bbs.exec(get_arg(str),EX_OUTR|EX_INR);
			}
			return;
		}

		if(str=="HELP") {
			writeln("NEXEC [command]");
			writeln("\texecutes command (or prompts for it) with I/O redirected, and assuming");
			writeln("\tit's a native binary.");
		}
		if(word=="NEXEC") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				bbs.exec(get_arg(str),EX_OUTR|EX_INR|EX_NATIVE);
			}
			return;
		}

		if(str=="HELP") {
			writeln("FOSSIL [command]");
			writeln("\texecutes command (or prompts for it) with I/O redirected, and assuming");
			writeln("\tthe internal FOSSIL driver will be used.");
		}
		if(word=="FOSSIL") {
			if(bbs.check_syspass()) {
				str=str.substr(6);
				bbs.exec(get_arg(str));
			}
			return;
		}

		if(str=="HELP") {
			writeln("CALL <HubID>");
			writeln("\tforces callout to HubID");
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

		if(str=="HELP") {
			writeln("NODE [parameters]");
			writeln("\texecutes the node utility with the passed parameters.");
		}
		if(word=="NODE") {
			bbs.exec(system.exec_dir+str.toLowerCase(), EX_OUTR|EX_INR|EX_NATIVE);
			return;
		}

		if(str=="HELP") {
			writeln("DOWN [#]");
			writeln("\tdowns node #.  If # is omitted, downs the current node.");
		}
		if(word=="DOWN") {
			str=str.substr(4);
			i=parseInt(get_arg(str));
			if(!i) i=bbs.node_num;
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

		if(str=="HELP") {
			writeln("RERUN [#]");
			writeln("\tMarks node # for rerun.  If # is omitted, reruns the current node.");
		}
		if(word=="RERUN") {
			str=str.substr(5);
			i=parseInt(get_arg(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_RRUN;
				display_node(i+1);
			}
			return;
		}

		if(str=="HELP")
			writeln("SLOG\tExecutes the slog utility to display system statistics.");
		if(str=="SLOG") {
			bbs.exec(system.exec_dir+"slog /p",EX_OUTR|EX_INR|EX_NATIVE);
			return;
		}

		if(str=="HELP") {
			writeln("NLOG [#]");
			writeln("\tExecutes the slog utility to display node stats for the specified node.");
			writeln("\tIf # is omitted, uses the current node.");
		}
		if(str=="NLOG") {
			bbs.exec(system.exec_dir+"slog "+system.node_dir+" /p",EX_OUTR|EX_INR|EX_NATIVE);
			return;
		}
		if(word=="NLOG") {
			str=str.substr(5);
			bbs.exec(system.exec_dir+"slog "+system.node_dir+"../node"+get_arg(str)+" /p",EX_OUTR|EX_INR|EX_NATIVE);
			return;
		}

		if(str=="HELP") {
			writeln("UEDIT [Number or Name]");
			writeln("\tEdits specified user or starts at user #1");
		}
		if(word=="UEDIT") {
			// Prompts for syspass
			str=str.substr(5);
			if(str.length)
				bbs.edit_user(bbs.finduser(get_arg(str)));
			else
				bbs.edit_user();
			return;
		}

		if(str=="HELP")
			writeln("MAIL\tRead all mail currently in the mail base");
		if(str=="MAIL") {
			bbs.read_mail(MAIL_ALL);
			return;
		}

		if(str=="HELP") {
			writeln("BULKMAIL");
			writeln("\tSends a mail to all users which match a specified ARS.");
		}
		if(str=="BULKMAIL") {
			write("\r\nEnter ARS matches to send mail to or [CR] to send ");
			write("by name/number\r\nARS to match: ");
			str=console.getstr("", 40);
			if(str!=null)
				bbs.bulk_mail(str);
			return;
		}

		if(str=="HELP") {
			writeln("DOS\tExecutes the DOS shell (command.com) with I/O redirected.");
		}
		if(str=="DOS") {	// DOS/Windows shell
			if(bbs.check_syspass()) {
				bbs.exec("command.com",EX_OUTR|EX_INR);
			}
			return;
		}

		if(str=="HELP")
			writeln("SHELL\tExecutes the native shell (COMSPEC or SHELL env variable).");
		if(str=="SHELL") {	// Unix shell (-i for interactive)
			if(bbs.check_syspass()) {
				if(system.platform != 'Win32')
					bbs.exec(system.cmd_shell+" -i"	,EX_OUTR|EX_INR|EX_NATIVE);
				else
					bbs.exec(system.cmd_shell		,EX_OUTR|EX_INR|EX_NATIVE);
			}
			return;
		}

		if(str=="HELP") {
			writeln("SPY <#>");
			writeln("\tSpys on node #.");
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

		if(str=="HELP") {
			writeln("DIR [path]");
			writeln("\tDisplays a full directory of specified path or the current file area if");
			writeln("\tnot specified");
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

		if(str=="HELP") {
			writeln("LOAD [filespec]");
			writeln("\tLoads the text.dat from the specified filespec.");
		}
		if(word=="LOAD") {
			str=str.substr(4);
			bbs.load_text(get_arg(str));
			return;
		}

		if(str=="HELP") {
			writeln("UPLOAD [areaspec]");
			writeln("\tPerforms a bulk upload in areaspec where area spec is ALL, LIB, or");
			writeln("\tomitted.");
			writeln("\tIf areaspec is ALL performs the bulk upload in all file areas.");
			writeln("\tIf areaspec is LIB, does the same in all areas of the current lib.");
		}
		if(word=="UPLOAD") {
			str=str.substr(7);
			if(str.toUpperCase()=="ALL") {
				for(i=0; i<file_area.lib_list.length; i++) {
					for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
						if(file_area.lib_list[i].offline_dir != undefined
							&& file_area.lib_list[i].offline_dir == file_area.lib_list[i].dir[j])
							continue;
						bbs.bulk_upload(file_area.lib_list[i].dir_list[j].number);
					}
				}
				return;
			}
			if(str.toUpperCase()=="LIB") {
				for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
					if(file_area.lib_list[bbs.curlib].offline_dir != undefined
						&& file_area.lib_list[bbs.curlib].offline_dir == file_area.lib_list[bbs.curlib].dir[j])
						continue;
					bbs.bulk_upload(file_area.lib_list[bbs.curlib].dir_list[i].number);
				}
				return;
			}
			bbs.bulk_upload();
			return;
		}

		if(str=="HELP") {
			writeln("ALTUL [path]");
			writeln("\tSets the ALT upload path to <path>.  If path is omitted, turns off the");
			writeln("\talt upload path.");
		}
		if(word=="ALTUL") {
			str=str.substr(6);
			bbs.alt_ul_dir=(str+0);
			printf(bbs.text(AltULPathIsNow),bbs.alt_ul_dir?bbs.alt_ul_dir:bbs.text(OFF));
			return;
		}

		if(str=="HELP") {
			writeln("RESORT [ALL|LIB|blank]");
			writeln("\tResorts the specified file areas.");
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

		if(str=="HELP") {
			writeln("OLDUL [ALL|LIB|blank]");
			writeln("\tLists all files uploaded before your last scan time.");
			writeln("OLD [ALL|LIB|blank]");
			writeln("\tLists all files not downloaded since your last scan time.");
			writeln("OFFLINE [ALL|LIB|blank]");
			writeln("\tLists all offline files.");
			writeln("CLOSE [ALL|LIB|blank]");
			writeln("\tLists all files currently open.");
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
						if(file_area.lib_list[i].offline_dir != undefined
							&& file_area.lib_list[i].offline_dir == file_area.lib_list[i].dir[j])
							continue;
						l=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number,s,m);
						if(l==-1)
							return;
						k+=l;
					}
				}
			}
			else if(str.toUpperCase()=="LIB") {
				for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++) {
					if(file_area.lib_list[bbs.curlib].offline_dir != undefined
						&& file_area.lib_list[bbs.curlib].offline_dir == file_area.lib_list[bbs.curlib].dir[j])
						continue;
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

		if(str=="HELP") {
			writeln("GET [path]");
			writeln("\tDownload the specified file");
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

			bbs.send_file(str);
			return;
		}

		if(str=="HELP") {
			writeln("PUT [path]");
			writeln("\tUpload the specified file");
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
			if(!bbs.check_syspass())
				return;

			bbs.receive_file(str);
			return;
		}
	}

//############################# Exemption Functions #############################


//# Quiet Node
	if(user.compare_ars("exempt Q")) {
		if(str=="HELP")
			writeln("QUIET\tToggles quiet setting (you are not lised as online).");
		if(str=="QUIET") {
			if(system.node_list[bbs.node_num-1].status==NODE_QUIET)
				system.node_list[bbs.node_num-1].status=NODE_INUSE;
			else
				system.node_list[bbs.node_num-1].status=NODE_QUIET;
			display_node(bbs.node_num);
			return;
		}

		if(str=="HELP") {
			writeln("ANON\tToggles anonymous setting (the node is listed online, but you are not");
			writeln("\tmentioned).");
		}
		if(str=="ANON") {
			bbs.node_settings ^= NODE_ANON;
			display_node(bbs.node_num);
			return;
		}
	}

// Lock Node
	if(user.compare_ars("exempt N")) {
		if(str=="HELP") {
			writeln("LOCK [#]");
			writeln("\tLocks the specified node, or the current node if none specified.");
		}
		if(word=="LOCK") {
			str=str.substr(4);
			i=parseInt(get_arg(str));
			if(!i) i=bbs.node_num;
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
		if(str=="HELP") {
			writeln("INTR [#]");
			writeln("\tInterrupts the specified node, or the current node if none specified.");
		}
		if(word=="INTR") {
			str=str.substr(4);
			i=parseInt(get_arg(str));
			if(!i) i=bbs.node_num;
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
		if(str=="HELP")
			writeln("CHAT\tPages the sysop");
		if(str=="CHAT") {
			bbs.page_sysop();
			return;
		}
	}

	if(str=="HELP")
		writeln("POFF\tToggles if other users can page you for this session.");
	if(str=="POFF") {
		bbs.node_settings ^= NODE_POFF;
		write("Paging is ");
		if(bbs.node_settings & NODE_POFF)
			writeln("OFF");
		else
			writeln("ON");
	}

// Edit .plan
	if(user.compare_ars("rest not G")) {
		if(str=="HELP")
			writeln("PLAN\tEdits or deletes your .plan file (displayed when somebody fingers you).");
		if(str=="PLAN") {
			var plan=format("%suser/%04d.plan",system.data_dir,user.number);
			if(file_exists(plan)) {
				if(console.yesno("Display current .plan"))
					console.printfile(plan);
				if(!console.noyes("Delete current .plan"))
					file_remove(plan);
			}
			if(console.yesno("Edit/Create .plan")) {
				console.editfile(plan);
				if(file_exists(plan))
					console.printfile(plan);
			}
		}
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

function display_node(node_num)
{
	var n=node_num-1;

	printf("Node %2d: ",node_num);
	var node = system.node_list[node_num-1];

	if(node.status==NODE_QUIET || node.status==NODE_INUSE) {
		write(system.username(node.useron)
			+ " "
			+ format(NodeAction[node.action],system.node_list[n].aux));
	} else
		printf(NodeStatus[node.status],node.aux);

	if(node.misc&(NODE_LOCK|NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG)) {
		write(" (");
		if(node.misc&NODE_AOFF)
			write('A');
		if(node.misc&NODE_LOCK)
			write('L');
		if(node.misc&(NODE_MSGW|NODE_NMSG))
			write('M');
		if(node.misc&NODE_POFF)
			write('P');
		write(')');
	}
	if(((node.misc
		&(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN))
		|| node.status==NODE_QUIET)) {
		write(" [");
		if(node.misc&NODE_ANON)
			write('A');
		if(node.misc&NODE_INTR)
			write('I');
		if(node.misc&NODE_RRUN)
			write('R');
		if(node.misc&NODE_UDAT)
			write('U');
		if(node.status==NODE_QUIET)
			write('Q');
		if(node.misc&NODE_EVENT)
			write('E');
		if(node.misc&NODE_DOWN)
			write('D');
		if(node.misc&NODE_LCHAT)
			write('C');
		write(']'); }
	if(node.errors)
		printf(" %d error%s",node.errors, node.errors>1 ? 's' : '' );
	printf("\r\n");
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

function get_lib_index(lib)
{
	var i;

	for(i=0;i<file_area.lib_list.length;i++) {
		if(file_area.lib_list[i].number==lib)
			return(i);
	}
	return(-1);
}

