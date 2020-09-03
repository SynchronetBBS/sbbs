// $Id: sauce.js,v 1.6 2018/02/02 13:06:31 rswindell Exp $
// vi: tabstop=4

var lib = load({}, "sauce_lib.js");

function main()
{
	var i = 0;
	var verbosity = 0;
	var files = [];
	const view=0, add=1, edit=2, remove=3;
	var cmd = view;
	var edit_tinfo = false;

	for(i=0; i < argv.length; i++) {
		switch(argv[i]) {
			case '-v':
				verbosity++;
				break;
			case '-E':
				edit_tinfo = true;
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
				printf("(Type:%u %ux%u%s", sauce.datatype, sauce.cols, sauce.rows, sauce.ice_color ? " iCE":"");
				if(sauce.title) printf(" '%s'", sauce.title);
				if(sauce.author) printf(" by %s", sauce.author);
				if(sauce.group) printf(" [%s]", sauce.group);
				print(")");
				if(sauce.comment.length)
					print('\t' + sauce.comment.join(', '));
				if(verbosity)
					print(JSON.stringify(sauce, null, 4));
			}
			if(cmd != view) {
				printf("Editing %s\n", file.name);
				var cols = parseInt(prompt("Columns [" + sauce.cols + "]"));
				if(cols)
					sauce.cols = cols;
				for(var i in { title:0, author:0, group:0 }) {
					var str = prompt(i + " [" + sauce[i] + "]");
					if(str)
						sauce[i] = truncsp(str);
				}
				if(edit_tinfo) {
					for(var i = 1; i <= 4; i++) {
						var str = prompt("tinfo" + i + " [" + sauce["tinfo" + i] + "]");
						if(str)
							sauce["tinfo" + i] = parseInt(str);
					}
				}
				var orig_comment = sauce.comment.slice();
				sauce.comment.length = 0;
				for(var i=0; i < lib.defs.max_comments; i++) {
					printf("Comment[%u] ('.' to end)", i);
					var str;
					if(i < orig_comment.length) {
						str = prompt(" [" + orig_comment[i] + "]");
						if(!str)
							str = orig_comment[i];
					} else
						str = prompt("");
					if(str == '.')
						break;
					sauce.comment.push(truncsp(str));
				}
				sauce.ice_color = !deny("Uses iCE (high-intensity background) colors");
				var result = lib.write(file.name, sauce);
				printf("%s\n", result == true ? "Success" : "FAILED!");
			}
		}
	}
}

main();
