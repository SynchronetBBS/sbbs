// $Id$

load("hexdump_lib.js");

function main()
{
	var i;

	for(i=0; i < argv.length; i++) {
		var file = new File(argv[i]);
		if(!file.open("rb"))
			alert(file.name + " open error " + file.error);
		else {
			hexdump_file(file);
			file.close();
		}
	}
}

main();