load("file_size.js");

function isoTime(val)
{
	var str = format("%.4s-%.2s-%.2sT00:00", val.substring(0,4), val.substring(4,6), val.substring(6));
	var d = new Date(str);
	return d.valueOf() / 1000;
}

(function() {
	writeln("\r\nSynchronet System/Node Statistics Log Viewer\n");

	var sfile = new File(system.ctrl_dir + "csts.tab");
	var list = [];
	var reverse = false;
	var show_totals = false;

	var total_logons = 0;
	var total_timeon = 0;
	var total_uls = 0;
	var total_ulb = 0;
	var total_dls = 0;
	var total_dlb = 0;
	var total_posts = 0;
	var total_emails = 0;
	var total_fbacks = 0;
	var total_nusers = 0;

	while(argv.length > 0) {
		switch(argv.shift().toUpperCase()) {
		case "/REVERSE":
		case "/R":
		case "-R":
			reverse = true;
			break;
		case "/TOTALS":
		case "/T":
		case "-T":
			show_totals = true;
			break;
		}
	}

	if(!file_exists(sfile.name)) {
		writeln("* " + sfile.name + " does not exist");
		return false;
	}
	if(!sfile.open('r')) {
		writeln("* error opening " + sfile.name);
		return false;
	}

	var lines = sfile.readAll();
	sfile.close();

	for(var l = 1; l < lines.length; l++) {
		var line = lines[l];
		var field = line.split('\t');
		var timestamp = isoTime(field[0]);
		var logons = parseInt(field[1], 10);
		var timeon = parseInt(field[2], 10);
		var uls = parseInt(field[3], 10);
		var ulb = parseInt(field[4], 10);
		var dls = parseInt(field[5], 10);
		var dlb = parseInt(field[6], 10);
		var posts = parseInt(field[7], 10);
		var emails = parseInt(field[8], 10);
		var fbacks = parseInt(field[9], 10);
		var nusers = parseInt(field[10], 10);

		if(show_totals) {
			total_logons += logons;
			total_timeon += timeon;
			total_uls += uls;
			total_ulb += ulb;
			total_dls += dls;
			total_dlb += dlb;
			total_posts += posts;
			total_emails += emails;
			total_fbacks += fbacks;
			total_nusers += nusers;
		}

		list.unshift(
			strftime("%x",timestamp-(24*60*60)) +
			format(" T:%4lu  L:%3lu  P:%3lu  E:%3lu  F:%3lu  U:%5s %5lu  D:%5s %5lu  N:%2u",
				timeon,logons,posts,emails,fbacks,file_size_str(ulb, 1, 0),uls,file_size_str(dlb, 1, 0),dls,nusers)
		);
	}

	if(reverse)
		list.reverse();

	for each(var i in list)
		writeln(i);

	if(show_totals) {
		writeln("");
		writeln(format("%-*s: %d",20,"Total Time",total_timeon));
		writeln(format("%-*s: %d",20,"Total Logons",total_logons));
		writeln(format("%-*s: %d - %s",20,"Total U/L",total_uls, file_size_str(total_ulb, 1, 0)));
		writeln(format("%-*s: %d - %s",20,"Total D/L",total_dls, file_size_str(total_dlb, 1, 0)));
		writeln(format("%-*s: %d",20,"Total Posts",total_posts));
		writeln(format("%-*s: %d",20,"Total E-Mails",total_emails));
		writeln(format("%-*s: %d",20,"Total Feedback",total_fbacks));
		writeln(format("%-*s: %u",20,"Total New users", total_nusers));
	}

	writeln("");
})();


