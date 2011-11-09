(function() {
	writeln("\r\nSynchronet System/Node Statistics Log Viewer v1.02\n");

	var sfile = new File(system.ctrl_dir + "csts.dab");
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

	while(argv.length > 0) {
		switch(argv.shift().toUpperCase()) {
		case "/REVERSE":
		case "/R":
			reverse = true;
			break;
		case "/TOTALS":
		case "/T":
			show_totals = true;
			break;
		}
	}

	if(!file_exists(sfile.name))
		throw(sfile.name + " does not exist");
	if(!sfile.open('r+b'))
		throw("error opening " + sfile.name);

	while(sfile.position <= file_size(sfile.name) - 40) {
		
		var timestamp = sfile.readBin();
		var logons = sfile.readBin();
		var timeon = sfile.readBin();
		var uls = sfile.readBin();
		var ulb = sfile.readBin();
		var dls = sfile.readBin();
		var dlb = sfile.readBin();
		var posts = sfile.readBin();
		var emails = sfile.readBin();
		var fbacks = sfile.readBin();

		if(show_totals) {
			total_logons += logons;
			total_timeon += timeon;
			total_uls += uls;
			total_ulb += uls;
			total_dls += uls;
			total_dlb += uls;
			total_posts += posts;
			total_emails += emails;
			total_fbacks += fbacks;
		}

		list.unshift(
			strftime("%x",timestamp-(24*60*60)) + 
			format(" T:%5lu  L:%3lu  P:%3lu  E:%3lu  F:%3lu  U:%6luk %3lu  D:%6luk %3lu",
			timeon,logons,posts,emails,fbacks,ulb/1024,uls,dlb/1024,dls)
		);
	}

	sfile.close();

	if(reverse) 
		list.reverse();

	for each(var i in list) 
		writeln(i);
		
	if(show_totals) {
		writeln("");
		writeln(format("%-*s: %d",20,"Total Time",total_timeon));
		writeln(format("%-*s: %d",20,"Total Logons",total_logons));
		writeln(format("%-*s: %d - %dk",20,"Total U/L",total_uls, total_ulb));
		writeln(format("%-*s: %d - %dk",20,"Total D/L",total_dls, total_dlb));
		writeln(format("%-*s: %d",20,"Total Posts",total_posts));
		writeln(format("%-*s: %d",20,"Total E-Mails",total_emails));
		writeln(format("%-*s: %d",20,"Total Feedback",total_fbacks));
	}

	writeln("");
})();


