/* testbuild.js */

load("sbbsdefs.js");

var date_str = strftime("%b-%d-%y");	/* mmm-dd-yy */

var temp_dir = backslash(system.temp_path) + "sbbs-" + date_str;

if(!file_isdir(temp_dir) && !mkdir(temp_dir)) {
	log(LOG_ERR,"!Failed to create temp directory: " + temp_dir);
	exit(1);
}

putenv("CVSROOT=:pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs");

platform = system.platform.toLowerCase();

var builds
	=	[/* sub-dir */		/* cmd-line */
		["",				"cvs co src-sbbs3"],
		["",				"cvs co lib-" + platform],
		["src/sbbs3",		platform=="win32" ? "make RELEASE=1":"gmake"],
		["src/sbbs3/scfg",	platform=="win32" ? "make RELEASE=1":"gmake"],
	];

/* Platform-specific (or non-ported) projects */
if(platform=="win32") {
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
	if(!chdir(build_dir)) {
		log(LOG_ERR,"!FAILED to chdir to: " + build_dir);
		exit(1);
	}

	var cmd_line = builds[i][1] + " 2> build_output.txt";
	var retval=system.exec(cmd_line);
	if(retval) {
		log(LOG_ERR,"!ERROR " + retval + " executing: " + cmd_line + " in " + sub_dir);
		send_email("build_output.txt", "Build failure in " + sub_dir);
		exit(1);
	}
}

function send_email(fname, subject)
{
	var file = new File(fname);
	if(!file.open("rt")) {
		log("!ERROR " + errno_str + " opening " + fname);
		return(false);
	}
	var msgtxt = lfexpand(file.read(file.length));
	file.close();
	delete file;

	var msgbase = new MsgBase("mail");
	if(msgbase.open()==false) {
		log("!ERROR " + msgbase.last_error);
		return(false);
	}

	var hdr = { 
		to: "sysop", 
		to_ext: 1, 
		from: "testbuild", 
		subject: subject
	};

	if(!msgbase.save_msg(hdr, msgtxt))
		log("!ERROR " + msgbase.last_error + "saving mail message");

	log("E-mail sent.");

	msgbase.close();
}