/* testbuild.js */

/* JSexec script for periodic Synchronet test builds */

/* $Id$ */

load("sbbsdefs.js");

var keep = false;

for(i=0;i<argc;i++)
	if(argv[i]=="-k")
		keep=true;

var date_str = strftime("%b-%d-%y");	/* mmm-dd-yy */

var temp_dir = backslash(system.temp_path) + "sbbs-" + date_str;

if(!file_isdir(temp_dir) && !mkdir(temp_dir)) {
	log(LOG_ERR,"!Failed to create temp directory: " + temp_dir);
	exit(1);
}
log(LOG_INFO,"Using temp directory: " + temp_dir);

putenv("CVSROOT=:pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs");

var platform = system.platform.toLowerCase();
var make = (platform=="win32" ? "make":"gmake");
var msdev = '"C:\\Program Files\\Microsoft Visual Studio\\Common\\MSDev98\\Bin\\msdev"';
var build_output = "build_output.txt";
var archive;
var archive_cmd;
var cleanup;

if(platform=="win32") {
	archive="sbbs_src.zip";
	archive_cmd="pkzip25 -exclude=*output.txt -add -dir -max " + archive;
	cleanup="rmdir /s /q ";
} else {
	archive="sbbs_src.tgz";
	archive_cmd="tar --exclude=*output.txt -czvf " + archive + " *";
	cleanup="rm -r -f "
}

var builds
	=	[/* sub-dir */		/* cmd-line */						/* redirect */
		[""					,"cvs co src-sbbs3"					,"2> " + build_output],
		[""					,"cvs co lib-"+platform+".debug"	,"2> " + build_output],
		[""					,"cvs co lib-"+platform+".release"	,"2> " + build_output],
		[""					,archive_cmd						,"2> " + build_output],
		["src/sbbs3"		,make + " DEBUG=1"					,"2> " + build_output],
		["src/sbbs3"		,make + " RELEASE=1"				,"2> " + build_output],
		["src/sbbs3/scfg"	,make + " DEBUG=1"					,"2> " + build_output],
		["src/sbbs3/scfg"	,make + " RELEASE=1"				,"2> " + build_output],
	];

/* Platform-specific (or non-ported) projects */
if(platform=="win32") {
	/* Requires C++Builder */
	builds.push(["src/sbbs3/ctrl"		,"makelibs.bat"			,"> " + build_output]);
	builds.push(["src/sbbs3/ctrl"		,"bpr2mak sbbsctrl.bpr & make -f sbbsctrl.mak"
																,"> " + build_output]);
	builds.push(["src/sbbs3/chat"		,"bpr2mak chat.bpr     & make -f chat.mak"
																,"> " + build_output]);
	/* Requires Visual C++ */
	builds.push(["src/sbbs3"			,"msdev sbbs3.dsw /MAKE ALL"
																,"/OUT "+ build_output]);
} else {	/* Unix */
	builds.push(["src/sbbs3/install"	,"gmake"				,"2> " + build_output]);
	builds.push(["src/sbbs3/umonitor"	,"gmake"				,"2> " + build_output]);
	builds.push(["src/sbbs3/uedit"		,"gmake"				,"2> " + build_output]);
}

var win32_dist
	= [ "README.TXT",
		"FILE_ID.DIZ",
		"src/sbbs3/msvc.win32.exe.release/*.exe",
		"src/sbbs3/msvc.win32.dll.release/*.dll",
		"src/sbbs3/scfg/msvc.win32.exe.release/*.exe",
		"src/sbbs3/scfg/msvc.win32.exe.release/scfghelp.*",
		"src/sbbs3/chat/chat.exe",
		"src/sbbs3/ctrl/sbbsctrl.exe"
	];

chdir(temp_dir);

var system_description=system.local_host_name + " - " + system.os_version;

var file = new File("README.TXT");
if(file.open("wt")) {
	file.writeln(format("Synchronet-%s C/C++ Source Code Archive (%s)\n"
		,system.platform, system.datestr()));
	file.writeln("This archive contains a snap-shot of all the source code and library files");
	file.writeln("necessary for a successful " + system.platform 
		+ " build of the following Synchronet projects");
	file.writeln("as of " + system.datestr() + ":");
	file.writeln();
	file.writeln(format("%-20s %s", "Project Directory", "Build Command"));
	for(i in builds) {
		if(builds[i][0].length)
			file.writeln(format("%-20s %s", builds[i][0], builds[i][1]));
	}
	file.writeln();
	file.writeln("Builds verified on " + system.timestr());
	file.writeln(system_description);
	file.writeln();
	file.writeln("For more details, see http://synchro.net/docs/source.html");
	if(platform!="win32")
		file.writeln("and http://synchro.net/docs/sbbsunix.txt");
	file.close();
}

var file = new File("FILE_ID.DIZ");
if(file.open("wt")) {
	file.writeln(format("Synchronet-%s BBS Software",system.platform));
	file.writeln(format("C/C++ source code archive (%s)",system.datestr()));
	if(platform=="win32")
		file.writeln("Unzip *with* directories!");
	file.writeln("http://www.synchro.net");
	file.close();
}

var start = time();

for(i in builds) {
	var sub_dir = builds[i][0];
	var build_dir = temp_dir + "/" + sub_dir;
	var subject = system.platform + " build failure in " + sub_dir;

	log(LOG_DEBUG,"Build " + i + " of " + builds.length);
	if(sub_dir.length)
		log(LOG_INFO, "Build sub-directory: " + sub_dir);
	if(!chdir(build_dir)) {
		semd_email(subject, log(LOG_ERR,"!FAILED to chdir to: " + build_dir));
		bail(1);
	}

	builds[i].start = time();

	var cmd_line = builds[i][1];
	if(builds[i][2])
		cmd_line += " " + builds[i][2];
	log(LOG_INFO, "Executing: " + cmd_line);
	var retval=system.exec(cmd_line.replace(/msdev/, msdev));
	log(LOG_INFO, "Done: " + cmd_line);
	if(retval) {
		send_email(subject, 
			log(LOG_ERR,"!ERROR " + retval + " executing: '" + cmd_line + "' in " + sub_dir) 
			+ "\n\n" + file_contents(build_output));
		bail(1);
	}

	builds[i].end = time();
}

var body = "System: " + system_description + "\n\n";

for(i in builds) {
	body += elapsed_time(builds[i].end-builds[i].start) + " - ";
	body += builds[i][0] + "\t" + builds[i][1];
	body += "\n";
}

body += "-----\n";
body += elapsed_time(time()-start) + " - total\n";

send_email(system.platform + " builds successful", lfexpand(body));

chdir(temp_dir);
var dest = file_area.dir["sbbs"].path+archive;
log(LOG_INFO,format("Copying %s to %s",archive,dest));
if(!file_copy(archive,dest))
	log(LOG_ERR,format("!ERROR copying %s to %s",archive,dest));

if(platform=="win32") {

	var file = new File("README.TXT");
	if(file.open("wt")) {
		file.writeln(format("Synchronet-Win32 Version 3 Development Executable Archive (%s)\n"
			,system.datestr()));
		file.writeln("This archive contains a snap-shot of Synchronet-Win32 executable files");
		file.writeln("created on " + system.timestr());
		file.writeln();
		file.writeln("The files in this archive are not necessarily well-tested, DO NOT");
		file.writeln("constitute an official Synchronet release, and are NOT SUPPORTED!");
		file.writeln();
		file.writeln("USE THESE FILES AT YOUR OWN RISK");
		file.writeln();
		file.writeln("BACKUP YOUR WORKING EXECUTABLE FILES (i.e. *.exe and *.dll)");
		file.writeln("BEFORE over-writing them with the files in this archive!");
		file.close();
	}

	var file = new File("FILE_ID.DIZ");
	if(file.open("wt")) {
		file.writeln(format("Synchronet-%s BBS Software",system.platform));
		file.writeln(format("Development Executable Archive (%s)",system.datestr()));
		file.writeln("Snapshot for experimental purposes only!");
		file.writeln("http://www.synchro.net");
		file.close();
	}

	archive = "sbbs_dev.zip";

	var cmd_line = "pkzip25 -add " + archive 
		+ " -exclude=makehelp.exe -exclude=v4upgrade.exe " + win32_dist.join(" ");
	log(LOG_INFO, "Executing: " + cmd_line);
	system.exec(cmd_line);

	dest = file_area.dir["sbbs"].path+archive;	

	log(LOG_INFO,format("Copying %s to %s",archive,dest));
	if(!file_copy(archive,dest))
		log(LOG_ERR,format("!ERROR copying %s to %s",archive,dest));
}

bail(0);
/* end */

function bail(code)
{
	if(cleanup && !keep) {
		chdir(temp_dir + "/..");
		log(LOG_INFO, "Executing: " + cleanup + temp_dir);
		var retval=system.exec(cleanup + temp_dir);
		if(retval)
			log(LOG_ERR,format("!ERROR %d executing %s", retval, cleanup + temp_dir));
	}
	exit(code);
}

function elapsed_time(t)
{
	return format("%02u:%02u", t/60, t%60);
}

function file_contents(fname)
{
	var file = new File(fname);
	if(!file.open("rt")) {
		return log(LOG_ERR, "!ERROR " + errno_str + " opening " + fname);
	}
	var msgtxt = lfexpand(file.read(file.length));
	file.close();
	delete file;

	return(msgtxt);
}

function send_email(subject, body)
{
	var msgbase = new MsgBase("mail");
	if(msgbase.open()==false) {
		log(LOG_ERR,"!ERROR " + msgbase.last_error);
		return(false);
	}

	var hdr = { 
		from: "Synchronet testbuild.js", 
		subject: subject
	};

	var rcpt_list = [
		{to: "Rob Swindell", to_ext: 1},
		{to: "Stephen Hurd", to_net_addr: "sysop@nix.synchro.net", to_net_type: NET_INTERNET }
		];

	if(!msgbase.save_msg(hdr, body, rcpt_list))
		log(LOG_ERR, "!ERROR " + msgbase.last_error + " saving mail message");
	else
		log(LOG_INFO, "E-mail sent.");

	msgbase.close();
}
