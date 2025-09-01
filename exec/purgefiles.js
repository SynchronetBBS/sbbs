// Utility for deleting old files (e.g. log files) based on age/date

const default_max_age=30;	// days

var testing = false;
var verbose = false;
var max_age = default_max_age;
var pattern;

for (var argn = 0; argn < argc; ++argn) {
	if (argv[argn] == '-t')
		testing = true;
	else if (argv[argn] == '-v')
		verbose = true;
	else if (parseInt(argv[argn]) > 0)
		max_age = parseInt(argv[argn]);
	else if (argv[argn][0] == '-')
		break;
	else
		pattern = argv[argn];
}

if(!pattern) {
	print("\nusage: purgfiles.js [max_age_in_days] [-options] <directory/pattern>");
	print();
	print("default max_age_in_days = " + default_max_age);
	print();
	print("options:");
	print(" -t  testing only (no file deletion)");
	print(" -v  verbose output");
	exit();
}

printf("Getting directory of %s", pattern);
files = directory(pattern);
printf("\n%lu files\n",files.length);
if (verbose)
	print(JSON.stringify(files, null, 4));

if (files.length < 1)
	exit(0);
purged_files=0;
now = time();
for(i in files) {
	t=file_date(files[i]);
	age=(now-t)/(24*60*60);
	if(age <= max_age)
		continue;
	print(system.datestr(t) + " " + files[i]);
	if (testing)
		continue;
	if(!file_remove(files[i])) {
		log(LOG_ERR,"!Error " + errno + " removing " + files[i]);
		continue;
	}
	purged_files++;
}

printf("%lu files purged.\n",purged_files);
