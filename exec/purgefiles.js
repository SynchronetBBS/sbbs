// purgefiles.js

const default_max_age=30;	// days

argn=0;
if((max_age=parseInt(argv[argn]))>0)
	argn++;
else
	max_age=default_max_age;

if(argc==argn) {
	printf("\nusage: purgfiles.js [max_age_in_days] <directory/pattern>\n");
	printf("\ndefault max_age_in_days = %lu\n",default_max_age);
	exit();
}


printf("Getting directory of %s", argv[argn]);
files = directory(argv[argn]);
printf("\n%lu files\n",files.length);

printf("\nPurging files aged more than %lu days\n",max_age);
purged_files=0;
now = time();
for(i in files) {
	t=file_date(files[i]);
	age=(now-t)/(24*60*60);
	if(age <= max_age)
		continue;
	print(system.datestr(t) + " " + files[i]);
	if(!file_remove(files[i])) {
		log(LOG_ERR,"!Error " + errno + " removing " + files[i]);
		continue;
	}
	purged_files++;
}

printf("%lu files purged.\n",purged_files);