// $Id: hexdump.js,v 1.2 2018/03/22 23:03:45 rswindell Exp $

var hexdump = load({}, "hexdump_lib.js");

function main()
{
	var i;

	for(i=0; i < argv.length; i++) {
		var file = new File(argv[i]);
		if(!file.open("rb"))
			alert(file.name + " open error " + file.error);
		else {
			var output = hexdump.file(file);
			print(output.join('\n'));
			file.close();
		}
	}
}

main();