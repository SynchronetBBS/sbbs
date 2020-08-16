/* $Id: popen.js,v 1.1 2011/11/12 03:46:20 rswindell Exp $ */

/* Tests the File.popen() method, pass the program name/command-line you want to execute */

var f = new File(argv[0]);
if(!f.popen()) {
	alert("Error " + f.error);
	exit();
}
print(f.name + " opened successfully");
var ln;
while(ln=f.readln())
	print(ln);

f.close();