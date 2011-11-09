writeln("\r\nSynchronet System/Node Statistics Log Viewer v1.02\n");

var reverse = false;
var pause = false;

while(argv.length > 0) {
	switch(argv.shift().toUpperCase()) {
	case "/REVERSE":
	case "/R":
		reverse = true;
		break;
	case "/PAUSE":
	case "/P":
		pause = true;
		break;
	}
}


var sfile = new File(system.ctrl_dir + "csts.dab");
var list = [];

if(!file_exists(sfile.name))
	throw(sfile.name + " does not exist");
if(!sfile.open('r+'))
	throw("error opening " + sfile.name);

while(sfile.position <= file_size(sfile.name) - 40) {
	
	timestamp = sfile.readBin();
	logons = sfile.readBin();
	timeon = sfile.readBin();
	uls = sfile.readBin();
	ulb = sfile.readBin();
	dls = sfile.readBin();
	dlb = sfile.readBin();
	posts = sfile.readBin();
	emails = sfile.readBin();
	fbacks = sfile.readBin();

	yesterday = timestamp-(24*60*60);	/* 1 day less than stamp */
	
	list.unshift(
		strftime("%x",yesterday) + 
		format(" T:%5lu  L:%3lu  P:%3lu  E:%3lu  F:%3lu  U:%6luk %3lu  D:%6luk %3lu",
		timeon,logons,posts,emails,fbacks,ulb/1024,uls,dlb/1024,dls)
	);
}

if(reverse) 
	list.reverse();
	
for each(var i in list)
	print(i);
	
sfile.close();


