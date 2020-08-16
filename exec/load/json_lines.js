// $Id: json_lines.js,v 1.1 2019/08/15 19:15:39 rswindell Exp $

// Library for dealing with "JSON Lines" (.jsonl)
//                     and  "Newline Delimited JSON" (.ndjson) files

"use strict";

// Return true (success) or a string with an error
function add(filename, obj)
{
	var f = new File(filename);
	if(!f.open("a")) {
		return "Error " + f.error + " opening " + filename;
	}
	f.writeln(JSON.stringify(obj));
	f.close();
	return true;
}

// Returns an array object on success, string on error
// Pass a positive 'num' to return first num objects from file
// Pass a negative 'num' to return last -num objects from file
// If max_line_len == 0, default is 4096
// Pass 'recover' value of true to ignore unparseable lines
function get(filename, num, max_line_len, recover)
{
	var f = new File(filename);
	if(!f.open("r", true)) {
		return "Error " + f.error + " opening " + filename;
	}
	if(!max_line_len)
		max_line_len = 4096;
	var lines = f.readAll(max_line_len);
	f.close();
	if(num > 0)
		lines = lines.slice(0, num);
	else if(num < 0)
		lines = lines.slice(num);
	var list = [];
	for(var i in lines) {
		try {
			var obj = JSON.parse(lines[i]);
			list.push(obj);
		} catch(e) {
			if(!recover)
				return e + " line " + (Number(i) + 1) + " of " + filename;
		}
	}
	return list;
}

this;