/* testbuild.js */

/* JSexec script for periodic Synchronet test builds */

load("sbbsdefs.js");

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

var builds
	=	[/* sub-dir */		/* cmd-line */
		["",				"cvs co src-sbbs3"],
		["",				"cvs co lib-" + platform + ".debug"],
		["",				"cvs co lib-" + platform + ".release"],
		["src/sbbs3",		make + " DEBUG=1"],
		["src/sbbs3",		make + " RELEASE=1"],
		["src/sbbs3/scfg",	make + " DEBUG=1"],
		["src/sbbs3/scfg",	make + " RELEASE=1"],
	];

/* Platform-specific (or non-ported) projects */
if(platform=="win32") {
	/* Requires Visual C++ */
	builds.push(["src/sbbs3",			msdev + " sbbs3.dsw /MAKE ALL /OUT "+ build_output]);
	/* Requires C++Builder */
	builds.push(["src/sbbs3/chat",		"build"]);
	builds.push(["src/sbbs3/ctrl",		"build"]);
} else {	/* Unix */
	builds.push(["src/sbbs3/install",	"gmake"]);
	builds.push(["src/sbbs3/umonitor",	"gmake"]);
	builds.push(["src/sbbs3/uedit",		"gmake"]);
}

for(i in builds) {
	var sub_dir = builds[i][0];
	var build_dir = temp_dir + "/" + sub_dir;
	var subject = "Build failure in " + sub_dir;

	if(sub_dir.length)
		log(LOG_INFO, "Build sub-directory: " + sub_dir);
	if(!chdir(build_dir)) {
		semd_email(subject, log(LOG_ERR,"!FAILED to chdir to: " + build_dir));
		exit(1);
	}

	var cmd_line = builds[i][1];
	if(cmd_line.indexOf(build_output)<0)
		cmd_line += " 2> " + build_output;
	log(LOG_INFO, "Executing: " + cmd_line);
	var retval=system.exec(cmd_line);
	if(retval) {
		log(LOG_ERR,"!ERROR " + retval + " executing: '" + cmd_line + "' in " + sub_dir);
		send_email(subject, file_contents(build_output));
		exit(1);
	}
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
		log("!ERROR " + msgbase.last_error);
		return(false);
	}

	var hdr = { 
		to: "sysop", 
		to_ext: 1, 
		from: "Synchronet testbuild.js", 
		subject: subject
	};

	if(!msgbase.save_msg(hdr, body))
		log(LOG_ERR, "!ERROR " + msgbase.last_error + "saving mail message");
	else
		log(LOG_INFO, "E-mail sent.");

	msgbase.close();
}