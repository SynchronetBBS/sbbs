/* md5sum.js */

/* Calculate and displays MD5 digest (in hex) of specified file or string */

/* $Id: md5sum.js,v 1.1 2004/11/17 11:16:50 rswindell Exp $ */

var filename;
var binary=true;

for(i=0;i<argc;i++) {
	if(argv[i].substring(0,2)=="-s") {
		print(md5_calc(argv[i].substr(2),true));
		exit();
	}
	else if(argv[i]=="-b" || argv[i]=="--binary")
		binary=true;
	else if(argv[i]=="-t" || argv[i]=="--text")
		binary=false;
	else
		filename = argv[i];
}

if(!filename) {
	alert("no filename specified");
	exit();
}

file = new File(filename);
print("opening ", filename);
if(!file.open(binary ? "rb" : "r")) {
	alert("!Error " + file.error + " opening " + filename);
	exit();
}
print(file.md5_hex, " ", binary ? "*":" ", filename);
file.close();
