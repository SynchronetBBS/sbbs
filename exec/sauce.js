// $Id$
// vi: tabstop=4

var lib = load({}, "sauce_lib.js");

function main()
{
	var i = 0;
	var verbosity = 0;
	var files = [];
	const view=0, add=1, edit=2, remove=3;
	var cmd = view;

	for(i=0; i < argv.length; i++) {
		switch(argv[i]) {
			case '-v':
				verbosity++;
				break;
			case '-e':
				cmd = edit;
				break;
			case '-a':
				cmd = add;
				break;
			case '-r':
				cmd = remove;
				break;
			default:
				files.push(argv[i]);
				break;
		}
	}

	for(i=0; i < files.length; i++) {
		var file = new File(files[i]);
		if(!file.open("rb"))
			alert(file.name + " open error " + file.error);
		else {
			printf("%-16s ", file_getname(file.name));
			var sauce = lib.read(file);
			file.close();
			if(sauce == false && cmd != add) {
				alert("No SAUCE found");
				continue;
			}
			if(cmd == remove) {
				printf("Removing SAUCE from %s\r\n", file.name);
				var result = lib.remove(file.name);
				printf("%s\n", result == true ? "Success" : "FAILED!");		
				continue;
			}
			if(sauce == false) {
				sauce = { datatype: lib.defs.datatype.bin, cols: 80, comment: [], tinfos: '' };
			} else {
				printf("(%ux%u%s", sauce.cols, sauce.rows, sauce.ice_color ? " iCE":"");
				if(sauce.title) printf(" '%s'", sauce.title);
				if(sauce.author) printf(" by %s", sauce.author);
				if(sauce.group) printf(" [%s]", sauce.group);
				print(")");
				if(sauce.comment.length)
					print('\t' + sauce.comment.join(' '));
				if(verbosity)
					print(JSON.stringify(sauce, null, 4));
			}
			if(cmd != view) {
				printf("Editing %s\n", file.name);
				var cols = parseInt(prompt("Columns [" + sauce.cols + "]"));
				if(cols)
					sauce.cols = cols;
				for(var i in { title:0, author:0, group:0 })
					sauce[i] = prompt(i);
				sauce.comment.length = 0;
				for(var i=0; i < lib.defs.max_comments; i++) {
					var str = prompt("Comment " + i);
					if(!str)
						break;
					sauce.comment.push(str);
				}
				sauce.ice_color = !deny("Uses iCE (high-intensity background) colors");
				var result = lib.write(file.name, sauce);
				printf("%s\n", result == true ? "Success" : "FAILED!");
			}
		}
	}
}

main();
