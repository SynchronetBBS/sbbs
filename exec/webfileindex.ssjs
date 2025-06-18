var start = new Date();

require("file_size.js", "file_size_float");
var vs15 = "&#xFE0E;";
var folder = "&#x1F5C0;";
var file_folder = "&#x1F4C1;";
var dir_icon = file_folder;
var file_cab = "&#x1F5C4;";
var lib_icon = file_cab;
var image_icon = "&#x1F5BC;";
var eyeball = "&#x1F441;";
var mag_glass = "&#x1F50E;";
var archive_icon = mag_glass + vs15;

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
	writeln("div.img-container { width: 50%; height: 50%; }");
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

function lib_link(dir)
{
	var lib = file_area.lib[dir.lib_name];
	return lib.description.link(file_area.web_vpath_prefix + lib.vdir + "/");
}

var sorting_description = {
	"NAME_AI": "Name ascending",
	"NAME_DI": "Name descending",
	"NATURAL": "Date ascending",
	"DATE_D": "Date descending",
	"SIZE_A": "Size increasing",
	"SIZE_D": "Size decreasing"
};

function file_size(file)
{
	var size = file_size_float(file.size, 1, 1);

	if(file.sha1 === undefined)
		return size;
	return "<div title='SHA1: " + file.sha1 + "'>" + size + "</div>";
}

function file_date(file)
{
	if(!file.added)
		return "";
	return "<div title='" + system.timestr(file.added) + "'>"
		+ strftime("%b %d, %Y", file.added) + "</div>";
}

// Listing files in a directory
function dir_index(dir)
{
	dir = file_area.dir[dir];
	header(dir.description);
	writeln("[" + root_link() + "] / ");
	writeln(lib_link(dir));
	writeln("<br />");
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
	write("<th />");
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
		write("<td>");
		var view_icon = viewable_file(f.name);
		if(view_icon)
			write("<a href=?view=\"" + encodeURIComponent(f.name) + "\" title='View'>" + view_icon + "</a>");
		write("</td>");
		write("<td><a title='Download' href=" + encodeURIComponent(f.name) + ">" + f.name.bold() + "</a></td>");
		write("<td align=right>" + file_size(f) + "</td>");
		write("<td align=right>" + file_date(f) + "</td>");
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
	writeln("<p>" + list.length + " files listed");
	writeln("<br />");
}

// Listing the directories of a library
function lib_index(lib)
{
	header(file_area.lib[lib].description);
	writeln("[" + root_link() + "] / ");
//	writeln(file_area.lib[lib].description + "<br />");
	writeln("<p>");
	writeln("<ul>");
	for(var d in file_area.dir) {
		var dir = file_area.dir[d];
		if(dir.lib_name != lib)
			continue;
		if(!dir.can_access)
			continue;
		write(dir_icon + " " + dir.name.bold().link(dir.vdir + "/") + "<br />");
	}
	writeln("</ul>");
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
	var dir = {};
	for(var i in file_area.dir) {
		if(file_area.dir[i].vshortcut)
			dir['/' + file_area.dir[i].vshortcut + '/'] = file_area.dir[i].description;
	}
	var file = new File(system.ctrl_dir + "web_alias.ini");
	if(file.open("r")) {
		var list = file.iniGetObject();
		file.close();
		for(var i in list) {
			var desc = get_dir_desc(list[i]);
			if(desc)
				dir[i] = desc;
		}
	}
	if(!Object.keys(dir).length)
		return;
	writeln("<h3>Directories</h3>");
	writeln("<ul>");
	for(var i in dir)
		writeln(dir_icon + " <a href=" + i + ">" + dir[i].bold() + "</a><br />");
	writeln("</ul>");
	writeln("<p>");
}

function file_type(filename)
{
	var ext = file_getext(filename);
	if(ext)
		ext = ext.toLowerCase().slice(1);
	return ext;
}

function archive_file(filename)
{
	var ext = file_type(filename);
	if(!ext)
		return false;
	return ['zip', '7z', 'tgz', 'rar', 'lha', 'lzh', 'iso', 'cab'].indexOf(ext.toLowerCase()) >= 0;
}

function image_file(filename)
{
	var ext = file_type(filename);
	return ['jpg', 'jpeg', 'png', 'gif'].indexOf(ext) >= 0;
}

function viewable_file(filename)
{
	if(archive_file(filename))
		return archive_icon;
	if(image_file(filename))
		return image_icon;
	return false;
}

function nav_file(filename)
{
	var ret = {};
	var fb = FileBase(http_request.dir);
	if(!fb.open())
		return null;
	var list = fb.get_names().filter(viewable_file);
	fb.close();
	for(var i = 0; i < list.length; ++i) {
		if(list[i] == filename) {
			if(i)
				ret.prev = list[i - 1];
			if(i + 1 < list.length)
				ret.next = list[i + 1];
			return ret;
		}
	}
	return null;
}

function nav_links(nav)
{
	const left =
		'<svg xmlns="http://www.w3.org/2000/svg" width="15px" height="15px" viewBox="0 0 16 16" fill="none">' +
		'<title>Previous file</title>' +
		'<path d="M8 10L8 14L6 14L-2.62268e-07 8L6 2L8 2L8 6L16 6L16 10L8 10Z" fill="#000000"/>' +
		'</svg>';
	const right =
		'<svg xmlns="http://www.w3.org/2000/svg" width="15px" height="15px" viewBox="0 0 16 16" fill="none">' +
		'<title>Next file</title>' +
		'<path d="M8 6L8 2L10 2L16 8L10 14L8 14L8 10L-1.74845e-07 10L-3.01991e-07 6L8 6Z" fill="#000000"/>' +
		'</svg>';
	var result = '';
	if(nav && nav.prev)
		result += left.link("?view=" + nav.prev) + " ";
	if(nav && nav.next)
		result += right.link("?view=" + nav.next);
	return result;
}

function view_file(filename)
{
	if(filename[0] == '"')
		filename = filename.substring(1, filename.length - 1);
	header(" - " + filename);
	dir = file_area.dir[http_request.dir];
	writeln("[" + root_link() + "] / ");
	writeln(lib_link(dir) + " / ");
	writeln(dir.description.link("./"));
	writeln("<br />");
	writeln("<p>");
	var nav  = nav_file(filename);
	writeln(nav_links(nav));
	writeln("<p>");
	if(!viewable_file(filename)) {
		writeln("Non-viewable file type: " + filename);
		return;
	}
	var filename = file_area.dir[http_request.dir].path + filename;

	if(archive_file(filename))
		view_archive(filename);
	else if(image_file(filename))
		view_image(filename);
	writeln("<p>");
	writeln(nav_links(nav));
	writeln("<br />");
}

function view_archive(filename)
{
	var list;
	try {
		list = Archive(filename).list(false);
	} catch(e) {
		log(LOG_DEBUG, filename + " " + e);
		writeln(file_getname(filename) + e); //": Unsupported archive");
		return;
        }

	writeln('<table>');
	for(var i in list) {
		var file = list[i];
		if(file.type != 'file')
			continue;
		writeln('<tr>');
		writeln('<td>' + file.name.bold());
		writeln('<td align=right>' + file.size);
		writeln('<td>' + system.timestr(file.time).slice(4));
	}
	writeln('</table>');
}

function view_image(filename)
{
	writeln('<div style="width: 100%;">');
	writeln('<img style="object-fit: contain; width:50%" src=' +
		encodeURIComponent(file_getname(filename)) + ' />');
	writeln('</div>');
}

// Listing all libraries
function root_index()
{
	header("File Areas");
	shortcuts();
	writeln("<h3>Libraries</h3>");
	writeln("<ul>");
	for(var l in file_area.lib_list) {
		var lib = file_area.lib_list[l];
		write(lib_icon + " " + lib.description.bold().link(lib.vdir + "/")+ "<br />");
	}
	writeln("</ul>");
}

if(http_request.virtual_path[http_request.virtual_path.length - 1] != '/') {
	http_reply.status = "301 Moved";
	http_reply.header.Location = http_request.virtual_path + '/';
	exit();
}

if(http_request.dir !== undefined) {
	var view = http_request.query["view"];
	if(view === undefined)
		dir_index(http_request.dir);
	else
		view_file(view[0]);
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
