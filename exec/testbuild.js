/* testbuild.js */

/* JSexec script for nightly Synchronet test builds */

/* $Id: testbuild.js,v 1.34 2020/08/01 22:09:03 rswindell Exp $ */

require("smbdefs.js", 'SMB_PRIORITY_HIGHEST');

var keep = false;
var cov_token;
for(i=0;i<argc;i++) {
	if(argv[i]=="-k")
		keep=true;
	else if(argv[i].indexOf('-cov-token=') == 0) {
		cov_token = argv[i].slice(11);
	}
}
var date_str = strftime("%b-%d-%Y");	/* mmm-dd-yyyy */

var temp_dir = backslash(system.temp_path) + "sbbs-" + date_str;

log(LOG_INFO,"Using temp directory: " + temp_dir);

var build_output = "build_output.txt";
var repo = "git@gitlab.synchro.net:/sbbs/sbbs";
var cmd_line = "git clone " + repo + " " + temp_dir + " 2> " + build_output;

log(LOG_INFO, "Executing: " + cmd_line);
var retval=system.exec(cmd_line);
log(LOG_INFO, "Done: " + cmd_line);
if(retval) {
	print("errno: " + errno);
	send_email(subject,
		log(LOG_ERR,"!ERROR " + retval + " executing: '" + cmd_line)
			+ "\n\n" + file_contents(build_output)
		,SMB_PRIORITY_HIGHEST);
	bail(1);
}

var platform = system.platform.toLowerCase();
if(system.architecture=="x64")
	platform += "-x64";
var archive;
var archive_cmd;
var cleanup;
var exclude_dirs = [
	"node1",
	"ctrl",
	"docs",
	"exec",
	"install",
	"text",
	"web",
	"webv4",
	"xtrn",
	"src/crt",
	"src/doors",
	"src/odoors",
	"src/sbbs2",
	"src/syncterm",
	"src/ZuulTerm"
	];

if(platform=="win32") {
	archive="sbbs_src.zip";
	archive_cmd="pkzip25 -exclude=*output.txt " +
		" -exclude=.gitignore" +
		" -exclude=" + exclude_dirs.join(" -exclude=") +
		" -exclude=" + exclude_dirs.join("/* -exclude=") + "/*" +
		" -exclude=3rdp/build" +
		" -exclude=3rdp/build/*" +
		" -exclude=3rdp/dist" +
		" -exclude=3rdp/dist/*" +
		" -add -dir -max " + archive;
	cleanup="rmdir /s /q ";
} else {
	archive="sbbs_src.tgz";
	archive_cmd="tar --exclude=*output.txt --exclude=" + exclude_dirs.join(" --exclude=") +
		" --exclude=3rdp/win32.release" +
		" --exclude-vcs" +
		" --exclude-vcs-ignores" +
		" --dereference" +
		" -czvf " + archive + " *";
	cleanup="rm -r -f "
}

var builds
	=	[/* sub-dir */		/* cmd-line */							/* redirect */
		[""					,archive_cmd							,"2> " + build_output],
	];

/* Platform-specific (or non-ported) projects */
if(platform=="win32") {
	/* Requires Visual C++ 2019 */
	builds.push(["src/sbbs3"			,'release.bat /v:m /p:WarningLevel=0'
																,"> " + build_output]);
	/* Requires C++Builder */
	builds.push(["src/xpdev"			,"make"
																,"> " + build_output]);
	builds.push(["src/sbbs3/ctrl"		,"makelibs.bat ..\\msvc.win32.dll.release"
																,"> " + build_output]);
	builds.push(["src/sbbs3/ctrl"		,"bpr2mak sbbsctrl.bpr & make -f sbbsctrl.mak"
																,"> " + build_output]);
	builds.push(["src/sbbs3/chat"		,"bpr2mak chat.bpr     & make -f chat.mak"
																,"> " + build_output]);
	builds.push(["src/sbbs3/useredit"	,"build.bat"
																,"> " + build_output]);
} else {	/* Unix */
	builds.unshift(["src/sbbs3"			,"make git_branch.h git_hash.h"]);
	builds.push(["src/sbbs3"			,"cov-build --dir ../../cov-int make RELEASE=1 all" ,"2> " + build_output]);
	builds.push(["src/sbbs3"			,"make RELEASE=1 gtkutils"	,"2> " + build_output]);
}

var win32_dist
	= [ "README.TXT",
		"FILE_ID.DIZ",
		"src/sbbs3/msvc.win32.exe.release/*.exe",
		"src/sbbs3/msvc.win32.dll.release/*.dll",
		"src/sbbs3/scfg/msvc.win32.exe.release/*.exe",
		"src/sbbs3/scfg/msvc.win32.exe.release/scfghelp.*",
		"src/sbbs3/chat/chat.exe",
		"src/sbbs3/ctrl/sbbsctrl.exe",
		"src/sbbs3/useredit/useredit.exe",
		"3rdp/win32.release/mozjs/bin/*.dll",
		"3rdp/win32.release/nspr/bin/*.dll",
		"3rdp/win32.release/cryptlib/bin/*.dll",
		"3rdp/win32.release/libarchive/bin/*.dll",
		"3rdp/win32.release/zlib/bin/*.dll",
		"s:/sbbs/exec/dosxtrn.exe"
	];

var nix_dist
	= [ "README.TXT",
		"FILE_ID.DIZ",
		"src/sbbs3/gcc.*.exe.release/*",
		"src/sbbs3/gcc.*.lib.release/*",
		"src/sbbs3/*/gcc.*.exe.release/*",
	];

chdir(temp_dir);
var system_description=system.local_host_name + " - " + system.os_version;

var file = new File("README.TXT");
if(file.open("wt")) {
	file.writeln(format("Synchronet-%s C/C++ Source Code Archive (%s)\n"
		,system.platform, date_str));
	file.writeln("This archive contains a snap-shot of all the source code and library files");
	file.writeln("necessary for a successful " + system.platform 
		+ " build of the following Synchronet projects");
	file.writeln("as of " + new Date().toUTCString() + ":");
	file.writeln();
	file.writeln(format("%-20s %s", "Project Directory", "Build Command"));
	for(i in builds) {
		if(builds[i][0].length)
			file.writeln(format("%-20s %s", builds[i][0], builds[i][1]));
	}
	file.writeln();
	file.writeln("Builds verified on " + system.timestr() + " " + system.zonestr());
	file.writeln(system_description);
	file.writeln();
	file.writeln("For more details, see http://wiki.synchro.net/dev:source");
	file.writeln("and http://wiki.synchro.net/install:dev");
	file.writeln();
	file.write("git commit: " );
	file.close();
	system.exec("git rev-parse HEAD >> " + file.name);
}

var file = new File("FILE_ID.DIZ");
if(file.open("wt")) {
	file.writeln(format("Synchronet-%s (%s) BBS Software",system.platform, system.architecture));
	file.writeln(format("C/C++ source code archive (%s)", date_str));
	if(platform=="win32")
		file.writeln("Unzip *with* directories!");
	file.writeln("http://www.synchro.net");
	file.close();
}

var start = time();

for(var i = 0; i < builds.length; i++) {
	var sub_dir = builds[i][0];
	var build_dir = temp_dir + "/" + sub_dir;
	var subject = system.platform + " build failure in " + sub_dir;

	log(LOG_DEBUG,"Build " + (i+1) + " of " + builds.length);
	if(sub_dir.length)
		log(LOG_INFO, "Build sub-directory: " + sub_dir);
	if(!chdir(build_dir)) {
		print("errno: " + errno);
		send_email(subject, log(LOG_ERR,"!FAILED to chdir to: " + build_dir), SMB_PRIORITY_HIGHEST);
		bail(1);
	}

	builds[i].start = time();

	var cmd_line = builds[i][1];
	if(builds[i][2])
		cmd_line += " " + builds[i][2];
	log(LOG_INFO, "Executing: " + cmd_line);
	var retval=system.exec(cmd_line);
	log(LOG_INFO, "Done: " + cmd_line);
	if(retval) {
		print("errno: " + errno);
		send_email(subject,
			log(LOG_ERR,"!ERROR " + retval + " executing: '" + cmd_line + "' in " + sub_dir) 
				+ "\n\n" + file_contents(build_output)
			,SMB_PRIORITY_HIGHEST);
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

send_email(system.platform + " builds successful in " + elapsed_time(time() - start), lfexpand(body));

chdir(temp_dir);

system.exec("git checkout -b dailybuild_" + platform);
system.exec("git merge master");
system.exec("git push --set-upstream origin dailybuild_" + platform);

var dest = file_area.dir["sbbs"].path+archive;
log(LOG_INFO,format("Copying %s to %s",archive,dest));
if(!file_copy(archive,dest))
	log(LOG_ERR,format("!ERROR copying %s to %s",archive,dest));

var file = new File("README.TXT");
if(file.open("wt")) {
	file.writeln(format("Synchronet-%s (%s) Version 3 Development Executable Archive (%s)\n"
		,system.platform,system.architecture, date_str));
	file.writeln(format("This archive contains a snap-shot of Synchronet-%s executable files"
		,system.platform));
	file.writeln("created on " + new Date().toUTCString());
	file.writeln();
	file.writeln("The files in this archive are not necessarily well-tested, DO NOT");
	file.writeln("constitute an official Synchronet release, and are NOT SUPPORTED!");
	file.writeln();
	file.writeln("USE THESE FILES AT YOUR OWN RISK");
	file.writeln();
	file.writeln("BACKUP YOUR WORKING EXECUTABLE FILES");
	file.writeln("BEFORE over-writing them with the files in this archive!");
	file.writeln();
	file.write("git commit: " );
	file.close();
	system.exec("git rev-parse HEAD >> " + file.name);
}

var file = new File("FILE_ID.DIZ");
if(file.open("wt")) {
	file.writeln(format("Synchronet-%s (%s) BBS Software",system.platform, system.architecture));
	file.writeln(format("Development Executable Archive (%s)", date_str));
	file.writeln("Snapshot for experimental purposes only!");
	file.writeln("http://www.synchro.net");
	file.close();
}

var cmd_line;
if(platform=="win32") {
	archive = "sbbs_dev.zip";
	cmd_line = "pkzip25 -add " + archive 
		+ " -exclude=v4upgrade.exe " + win32_dist.join(" ");
} else {
	cmd_line = 'tar czvf sbbs-cov.tgz cov-int && ' +
			'curl --form token=' + cov_token + ' ' +
			'--form email=rob@synchro.net ' +
			'--form file=@sbbs-cov.tgz ' +
			'--form version=' + system.version + system.revision + ' ' +
			'--form description="Synchronet for ' + system.platform + '" ' +
			'https://scan.coverity.com/builds?project=Synchronet';
	log(LOG_INFO, "Executing: " + cmd_line);
	system.exec(cmd_line);
	archive = "sbbs_dev.tgz";
	cmd_line = "pax -s :.*/::p -wzf " + archive + " " + nix_dist.join(" ");
}

log(LOG_INFO, "Executing: " + cmd_line);
system.exec(cmd_line);

dest = file_area.dir["sbbs"].path+archive;

log(LOG_INFO,format("Copying %s to %s",archive,dest));
if(!file_copy(archive,dest))
	log(LOG_ERR,format("!ERROR copying %s to %s",archive,dest));

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

function send_email(subject, body, priority)
{
	var msgbase = new MsgBase("mail");
	if(msgbase.open()==false) {
		log(LOG_ERR,"!ERROR " + msgbase.last_error);
		return(false);
	}

	var hdr = {
		from: "Synchronet testbuild.js",
		subject: subject || (system.platform + " build failure"),
		priority: priority
	};

	var rcpt_list = [
		{to: "Rob Swindell", to_ext: 1},
		{to: "Stephen Hurd", to_net_addr: "deuce@synchro.net", to_net_type: NET_INTERNET }
		];

	if(!msgbase.save_msg(hdr, body, rcpt_list))
		log(LOG_ERR, "!ERROR " + msgbase.last_error + " saving mail message");
	else
		log(LOG_INFO, "E-mail sent.");

	msgbase.close();
}
