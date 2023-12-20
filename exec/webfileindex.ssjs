var start = new Date();

require("file_size.js", "file_size_float");

function header(title) {
	writeln("<html lang=\"en\">");
	writeln("<style>");
	writeln('body { font-family: system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue","Noto Sans","Liberation Sans",Arial,sans-serif,"Apple Color Emoji","Segoe UI Emoji","Segoe UI Symbol","Noto Color Emoji"; }');
	writeln("a:link { text-decoration: none; }");
	writeln("a:visited { text-decoration: none; }");
	writeln("a:hover { text-decoration: underline; }");
	writeln("a:active { text-decoration: none; }");
	writeln("div.table-container { overlow: auto; }");
	writeln("table { border-collapse: collapse; }");
	writeln("thead th { position: sticky; top: 0; background-color: white; }");
	writeln("th { padding: .5rem; }");
	writeln("tbody > tr { border-top: 1px solid silver; }");
	writeln("tbody > tr:nth-child(even) { background-color: whitesmoke; }");
	writeln("td { padding: .5rem; }");
	writeln("@media (prefers-color-scheme: dark) { body { color: #FAFAFA; background-color: #0A0A0A; } a { color: #FAFAFA; font-weight: bold; } a:visited { color: #FAFAFA; font-weight: bold; } thead th { background-color: black; } tbody > tr:nth-child(even) { background-color: #2A2A2A; } tbody > tr { border-color: #1F1F1F; } }");
	writeln("</style>");
	writeln("<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>");
	writeln("<title>" + system.name + " " + title + "</title>");
	writeln("</head>");
	writeln("<h1>" + system.name + " " + title + "</h1>");
}

function root_link()
{
	return "root".link(file_area.web_vpath_prefix);
}

var sorting_description = {
	"NAME_AI": "Name ascending",
	"NAME_DI": "Name descending",
	"NATURAL": "Date ascending",
	"DATE_D": "Date descending",
	"SIZE_A": "Size increasing",
	"SIZE_D": "Size decreasing"
};

// Listing files in a directory
function dir_index(dir)
{
	dir = file_area.dir[dir];
	header(dir.description);
	writeln("[" + root_link() + "] / ");
	var lib = file_area.lib[dir.lib_name];
	writeln(lib.description.link(file_area.web_vpath_prefix + lib.vdir + "/") + "<br />");
	writeln("<p>");
	
	writeln("<form>");  // Netscape requires this to be in a form <sigh>
	writeln("<select onChange='if(selectedIndex>0) location=options[selectedIndex].value;'>");
	writeln("<option>Sort...</option>");
	for(s in sorting_description) {
	    writeln(format("<option value='?sort=%s'>%s"
	        ,FileBase.SORT[s]
	        ,sorting_description[s]));
	}
	writeln("</select>");
	writeln("</form>");	
	
	var fb = new FileBase(dir.code);
	if(!fb.open()) {
		write("Error " + fb.error + " opening directory: " + dir.code);
		return;
	}
	var sort = http_request.query["sort"];
	if(sort !== undefined)
		sort = parseInt(sort[0], 10);
	var list = fb.get_list(FileBase.DETAIL.NORM, true, sort);
	fb.close();
	if(!list) {
		write("file list is null");
		return;
	}
	writeln('<div class="table-container">');
	writeln("<table>");
	write("<thead>");
	write("<tr align=left>");
	write("<th>Name");
	write("<th align=right>Size");
	write("<th align=center>Date");
	write("<th>Description");
	writeln("</tr>");
	writeln("</thead>");
	writeln("<tbody>");
	for(var l in list) {
		var f = list[l];
		write("<tr>");
		write("<td>" + f.name.link(f.name) + "</td>");
		write("<td align=right>" + file_size_float(f.size, 1, 0) + "</td>");
		write("<td align=right>" + strftime("%b %d, %Y", f.time) + "</td>");
		write("<td>" + utf8_encode(f.desc || "") + "</td>");
		writeln("</tr>");
	}
	writeln("</tbody>");
	writeln("</table>");
	writeln("</div>");
	writeln("<p>" + list.length + " files");
}

// Listing the directories of a library
function lib_index(lib)
{
	header(file_area.lib[lib].description);
	writeln("[" + root_link() + "] / ");
	writeln(file_area.lib[lib].description + "<br />");
	writeln("<p>");
	
    for(var d in file_area.dir) {
		var dir = file_area.dir[d];
        if(dir.lib_name != lib)
            continue;
		if(!dir.can_access)
			continue;
        write(dir.name.link(dir.vdir + "/") + "<br />");
    }
}

// Listing all libraries
function root_index()
{
	header("File Areas");
    for(var l in file_area.lib_list) {
		var lib = file_area.lib_list[l];
        write(lib.description.link(lib.vdir + "/")+ "<br />");
    }
}	

if(http_request.virtual_path[http_request.virtual_path.length - 1] != '/') {
	http_reply.status = "301 Moved";
	http_reply.header.Location = http_request.virtual_path + '/';
	exit();
}

if(http_request.dir !== undefined) {
	dir_index(http_request.dir);
} else if (http_request.lib !== undefined) {
	lib_index(http_request.lib);
} else {
	root_index();
}

var now = new Date();
write("<br>Dynamically generated ");
write(format("in %lu milliseconds ", now.valueOf()-start.valueOf()));
write("by <a href=http://www.synchro.net>" + server.version + "</a>");
writeln("<br>" + now);
writeln("</body>");
writeln("</html>");
