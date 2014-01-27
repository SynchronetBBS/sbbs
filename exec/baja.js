/** BAJA to JavaScript translation library

don't expect this to produce working scripts.
it is meant as a starting point and time-saver
for those who wish to convert a BAJA source file
to JavaScript.

~mcmlxxix/echicken
	
$id: $ **/

BAJA = new (function() {

/* load baja command library */
load("bajalib.js");

/* baja source file */
var srcName = argv[0];

/* js output file */
var destName = argv[1];

/* include directories */
var locations = [
	js.exec_dir,
	system.exec_dir,
	system.mods_dir,
	system.exec_dir + "load/",
	system.mods_dir + "load/"
];

/* handle your shit */
if(!srcName)
	srcName = prompt("Enter BAJA source file name: ");
if(!destName)
	destName = prompt("Enter JavaScript output file name: ");

/* load BAJA file contents */
function open(fName) {
	for(var i=0;i<locations.length && !file_exists(fName);i++) {
		fName = locations[i] + file_getname(fName);
	}
	if(!file_exists(fName)) 
		throw("file not found: " + fName);
	var f = new File(fName);
	log("loading source file: " + f.name);
	if(!f.open('r',false))
		throw("error opening file: " + f.name);
	var data = f.readAll();
	f.close();
	return data;
}

/* main loop */
function parse(data,script) {
	if(!script)
		script = [
			"const FALSE = NOT_EQUAL = 0, TRUE = EQUAL = (1<<0), LESS = (1<<1), GREATER = (1<<2);",
			"system.baja = { logicState:TRUE, userInput:'', cmdKey:'', temp:'', errNo : 0, etx:'', dir:0 };"
		];
	
	while(data.length > 0) {
		var line = data.shift();
		
		/* trim spaces */
		line = trim(line);
		
		try {
		/* parse baja */
		if(!consume(script,line)) {
			script.push("// PARSING ERROR: " + line);
		}
		} catch(e) {
			log(e);
			log(e.stack);
		}
	}
	return script;
}

/* trim leading and trailing spaces */
function trim(str) {
	return str.replace(/(^[\t\s]+|[\t\s]+$)/,'');
}

/* parse a line of baja code */
function consume(script, line) {
	
	/* handle blank lines */
	if(trim(line).length == 0) {
		script.push("");
		return true;
	}

	/* remove comments (and put on own line) */
	var comment = line.match(/[\t\s]*\#(.*)/);
	if(comment) {
		script.push("//" + comment[1]);
		line = line.replace(/[\t\s]*\#.*/,'');
	}
	
	/* if string is empty after comment is stripped, ignore */
	if(trim(line).length == 0)
		return true;
	
	//log(line);
	
	/* set labels */
	if(line[0] == ":") {
		script.push(line.substr(1) + ":");
		script.push(line.substr(1) + " = (function() {");
		return true;
	}
	
	/* handle !includes */
	var inc = line.match(/^\!include[\t\s]+(.*)/i);
	if(inc) {
		if(line.match(/sbbsdefs/i))
			script.push("load(\"sbbsdefs.js\");");
		else if(line.match(/userdefs/i))
			script.push("load(\"userdefs.js\");");
		else if(line.match(/nodedefs/i))
			script.push("load(\"nodedefs.js\");");
		else {
			var d = open(inc[1]);
			script.push("/** BEGIN FILE IMPORT: " + inc[1] + " **/");
			parse(d,script);
			script.push("/** END FILE IMPORT: " + inc[1] + " **/");
		}
		return true;
	}
	
	/* handle defs */
	var def = line.match(/^\!define[\t\s]+(.*)[\t\s]+(.*)/i);
	if(def && def[1] && def[2]) {
		var bit = def[2].match(/\.(\d+)/);
		if(bit)
			def[2] = "(1<<" + bit[1] + ")";
		script.push("var " + def[1] + " = " + def[2] + ";");
		return true;
	}
	
	/* handle globals */
	var glob = line.match(/^\!global[\t\s]+(.*)/i);
	if(glob && glob[1]) {
		/* parse object path: _USERON.ALIAS */
		var path = glob[1].split(".");
		var path_string = "";
		var global = globals;
		
		while(path.length > 0) {
			/* object properties: _USERON, ALIAS */
			var prop = path.shift();
			path_string += prop;
			
			/* define the property if it's not already */
			if(!global[prop]) {
				if(path.length > 0) {
					script.push(path_string + " = {};");
					global[prop] = {};
				}
				else {
					script.push(path_string + " = undefined;");
					global[prop] = undefined;
				}
			}
			
			/* climb into the object */
			global = global[prop];
			path_string += ".";
		}
		return true;
	}	

	/* handle other shit */
	line = line.match(/([^\s"]+|"[^"]*")/g);
	if(baja[line[0].toUpperCase()]) {
		var cmd = line[0].toUpperCase();
		var args = line.slice(1);
		var str = baja[cmd].apply({},args);
		if(str != undefined) {
			str = str.split(/\r\n/);
			while(str.length > 0)
				script.push(str.shift());
		}
		return true;
	}

	/* unhandled line */
	return false;
}

/* format raw code */
function form(array) {
	var output = [];
	var depth = 0;
	
	for(var l=0;l<array.length;l++) {
		var line = array[l]; 
		
		/* check for depth increment triggers */
		if(line.match(/^[^\}]*\{[^\}]*$|case[\t\s]+.*\:/)) {
			output.push(tabs(depth) + line);
			depth++;
		}
		
		/* check for depth decrement triggers */ 
		else if(line.match(/^[^\{]*\}[^\{]*$|break\;/i)) {
			depth--;
			if(depth < 0)
				depth = 0;
			output.push(tabs(depth) + line);
		}
		
		else {
			output.push(tabs(depth) + line);			
		}
	}
	return output;
}

/* get a string containing specified number of tabs */
function tabs(num) {
	return format("%*s",num,'').replace(/\s/g,'\t');
}

/* save javascript */
function close(fName,data) {
	var f = new File(fName);
	log("saving javascript file: " + f.name);
	if(!f.open('w',true))
		throw("error opening file: " + f.name);
	f.writeAll(data);
	f.close();
}

var data = open(srcName);
var script = parse(data);
var output = form(script);
close(destName,output);

})();

