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
	writeln("div.form-container { margin-bottom: .5rem; }");
	writeln("div.form-container > form { display: inline; margin-right: .5rem; }");
	writeln("div.table-container { overlow: auto; }");
	writeln("table { border-collapse: collapse; }");
	writeln("thead th { position: sticky; top: 0; background-color: white; }");
	writeln("th { padding: .5rem; }");
	writeln("tbody > tr { border-top: 1px solid silver; }");
	writeln("tbody > tr:nth-child(even) { background-color: whitesmoke; }");
	writeln("td { padding: .5rem; vertical-align: top; }");
	writeln("td.desc:has(div) { cursor: pointer; }");
	writeln("div.extdesc { margin-top: .5rem; white-space: pre; line-height: 1; font-family: monospace; overflow: hidden; }");
	writeln(".visible { visbility: visible; display: block; }");
	writeln(".invisible { visibility: hidden; display: none; }");
	writeln("@media (prefers-color-scheme: dark) { body { color: #FAFAFA; background-color: #0A0A0A; } a { color: #FAFAFA; font-weight: bold; } a:visited { color: #FAFAFA; font-weight: bold; } thead th { background-color: black; } tbody > tr:nth-child(even) { background-color: #2A2A2A; } tbody > tr { border-color: #1F1F1F; } }");
	writeln("</style>");
	writeln("<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>");
	writeln("<title>" + system.name + " " + title + "</title>");
	writeln('<script>');
	writeln("function showExtDescs(evt) { document.querySelectorAll('div.extdesc').forEach(ext => { if (evt.target.checked) { ext.classList.remove('invisible'); ext.classList.add('visible'); } else { ext.classList.remove('visible'); ext.classList.add('invisible'); } }); }");
	writeln("function showExtDesc(evt) { const ext = evt.target.querySelector('.extdesc'); if (!ext) { return; } if (ext.classList.contains('invisible')) { ext.classList.remove('invisible'); ext.classList.add('visible'); } else { ext.classList.remove('visible'); ext.classList.add('invisible'); } }");
	writeln('</script>');
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

	var fb = new FileBase(dir.code);
	if(!fb.open()) {
		write("Error " + fb.error + " opening directory: " + dir.code);
		return;
	}
	var sort = http_request.query["sort"];
	if(sort !== undefined)
		sort = parseInt(sort[0], 10);
	var list = fb.get_list(FileBase.DETAIL.EXTENDED, true, sort);
	fb.close();
	if(!list) {
		write("file list is null");
		return;
	}

	writeln('<div class="form-container">');
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
	writeln('<input id="show-extdescs-switch" type="checkbox" onChange="showExtDescs(event)"><label title for="show-extdescs-switch">Show extended descriptions</label>');
	writeln('</div>');
	writeln('<div class="table-container">');
	writeln("<table>");
	write("<thead>");
	write("<tr align=left>");
	write("<th>Name</th>");
	write("<th align=right>Size</th>");
	write("<th align=center>Date</th>");
	write("<th>Description</th>");
	writeln("</tr>");
	writeln("</thead>");
	writeln("<tbody>");
	for(var l in list) {
		var f = list[l];
		if(f.size <= 0)
			continue;
		write("<tr>");
		write("<td>" + f.name.link(f.name) + "</td>");
		write("<td align=right>" + file_size_float(f.size, 1, 0) + "</td>");
		write("<td align=right>" + strftime("%b %d, %Y", f.added) + "</td>");
		write('<td class="desc" onclick="showExtDesc(event)">');
		write(utf8_encode(f.desc || ''));
		if (f.extdesc !== undefined) {
			write('<div class="extdesc invisible">' + utf8_encode(f.extdesc.replace(/\x01\//g, "\n").replace(/\x01./g, '')) + '</div>');
		}
		write('</td>');
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

function get_dir_desc(vpath)
{
	for(var i in file_area.dir) {
		var dir = file_area.dir[i];
		if((file_area.web_vpath_prefix + dir.vpath).toLowerCase() == vpath.toLowerCase())
			return dir.description;
	}
	return null;
}

// List configured aliases to directories as shortcuts
function shortcuts()
{
	var file = new File(system.ctrl_dir + "web_alias.ini");
	if(!file.open("r"))
		return;
	var list = file.iniGetObject();
	file.close();
	if(!list)
		return;
	var dir = {};
	for(var i in list) {
		var desc = get_dir_desc(list[i]);
		if(desc)
			dir[i] = desc;
	}
	if(!Object.keys(dir).length)
		return;
	writeln("<h3>Directories</h3>");
	writeln("<ul>");
	for(var i in dir)
		writeln("<a href=" + i + ">" + dir[i] + "</a><br />");
	writeln("</ul>");
	writeln("<p>");
}

// Listing all libraries
function root_index()
{
	header("File Areas");
	shortcuts();
	writeln("<h3>Libraries</h3>");
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
