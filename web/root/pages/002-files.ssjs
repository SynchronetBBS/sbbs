//Files

if(typeof argv[0] !== 'boolean' || !argv[0])
	exit();

load(system.exec_dir + '../web/lib/init.js');
load(settings.web_lib + 'files.js');

//writeln('<script type="text/javascript" src="./js/files.js"></script>');

if (typeof http_request.query.dir !== 'undefined' &&
	typeof file_area.dir[http_request.query.dir[0]] !== 'undefined'
) {
	// File listing

	writeln(
		'<ol class="breadcrumb">' +
		'<li>' +
		'<a href="./?page=' + http_request.query.page[0] + '">Files</a>' +
		'</li>' +
		'<li>' +
		'<a href="./?page=' + http_request.query.page[0] + '&amp;library=' + file_area.dir[http_request.query.dir[0]].lib_index + '">' + file_area.dir[http_request.query.dir[0]].lib_name + '</a>' +
		'</li>' +
		'<li>' +
		'<a href="./?page=' + http_request.query.page[0] + '&amp;dir=' + http_request.query.dir[0] + '">' + http_request.query.dir[0] + '</a>' +
		'</li>' +
		'</ol>'
	);

	function writeFileDetails(file) {
		writeln(
			format(
				'<a href="./api/files.ssjs?call=download-file&amp;dir=%s&amp;file=%s" target="_blank" class="list-group-item striped">' +
				'<strong>%s</strong> (%s)' +
				'<p><em>Uploaded %s</em></p>' +
				'<p>%s</p>' +
				'%s' +
				'</a>',
				http_request.query.dir[0],
				file.name,
				file.name,
				file.size,
				system.timestr(file.uploadDate),
				file.description,
				(file.extendedDescription == "" ? "" : ("<p>" + file.extendedDescription + "</p>"))
			)
		);
	}

	writeln('<div id="file-list-container" class="list-group">');
	listFiles(http_request.query.dir[0]).forEach(writeFileDetails);
	writeln('</div>');


} else if(
	typeof http_request.query.library !== 'undefined' &&
	typeof file_area.lib_list[http_request.query.library[0]] !== 'undefined'
) {
	// File directory listing

	writeln(
		'<ol class="breadcrumb">' +
		'<li>' +
		'<a href="./?page=' + http_request.query.page[0] + '">Files</a>' +
		'</li>' +
		'<li>' +
		'<a href="./?page=' + http_request.query.page[0] + '&amp;library=' + http_request.query.library[0] + '">' + file_area.lib_list[http_request.query.library[0]].name + '</a>' +
		'</li>' +
		'</ol>'
	);

	function writeDirectory(dir) {
		writeln(
			format(
				'<a href="./?page=%s&amp;dir=%s" class="list-group-item striped">' +
				'<h4><strong>%s</strong></h4>' +
				'<p>%s : %s files</p>' +
				'</a>',
				http_request.query.page[0],
				dir.dir.code,
				dir.dir.name,
				dir.dir.description,
				dir.fileCount
			)
		);
	}

	writeln('<div id="file-list-container" class="list-group">');
	listDirectories(http_request.query.library[0]).forEach(writeDirectory);
	writeln('</div>');	

} else {
	// File library listing

	writeln(
		'<ol class="breadcrumb">' +
		'<li>' +
		'<a href="./?page=' + http_request.query.page[0] + '">Files</a>' +
		'</li>' +
		'</ol>'
	);

	function writeLibrary(library) {
		writeln(
			format(
				'<a href="./?page=%s&amp;library=%s" class="list-group-item striped">' +
				'<h3><strong>%s</strong></h3>' +
				'<p>%s : %s directories</p>' +
				'</a>',
				http_request.query.page[0],
				library.index,
				library.name,
				library.description,
				library.dir_list.length
			)
		);
	}

	writeln('<div id="file-list-container" class="list-group">');
	listLibraries().forEach(writeLibrary);
	writeln('</div>');

}