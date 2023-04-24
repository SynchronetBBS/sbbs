// Upload a file via HTTP-POST
// vi: tabstop=4

require('sbbsdefs.js', 'LEN_FDESC');

"use strict";

function post(query)
{
//	log(LOG_INFO, "query: " + JSON.stringify(query));
	if(!http_request.post_data) {
		log(LOG_WARNING, "no post data provided");
		return "500 No post data provided";
	}
    if(!query.filename) {
        log(LOG_WARNING, "no filename specified");
        return "500 No filename specified";
    }
	var fname = file_getname(query.filename[0]);
	if(!check_filename(fname)) {
		log(LOG_WARNING, "Attempted disallowed filname: " + fname);
		return "500 Filename not allowed";
	}
	var fdesc;
	if(query.desc)
		fdesc = query.desc[0];
	
	log(LOG_INFO, format("received file (%s): %u bytes"
        ,fname, http_request.post_data.length));
    if(!file_area.upload_dir) {
        log(LOG_ERR, "No upload directory configured");
        return "500 No upload directory configured";
    }
    var dir = file_area.upload_dir;
	
	if(!dir.can_upload) {
		log(LOG_NOTICE, "User can't upload to dir: " + dir.code);
		return "500 Can't upload here";
	}

	var filename = dir.path + fname;
	if(file_exists(filename)) {
		log(LOG_WARNING, filename + " already exists");
		return "409 File already exists";
	}

	var filebase = new FileBase(dir.code);
	if(!filebase.open()) {
		log(LOG_ERR, "Failed to open: " + filebase.file);
		return "500 error opening " + filebase.file;
	}
	if(filebase.get(fname)) {
		log(LOG_WARNING, format("File (%s) already exists in %s", fname, dir.code));
		return "500 File already uploaded";
	}

	var file = new File(filename);
	if(!file.open("wb")) {
		log(LOG_ERR, "error " + file.error + " opening file: " + file.name);
		return "409 error creating file";
	}
	file.write(http_request.post_data);
	file.close();

	file = { name: fname, desc: format("%.*s", LEN_FDESC, fdesc), from: user.alias };
	file.cost = file_size(filename);
	log(LOG_INFO, "Adding " + file.name + " to " + filebase.file);
	var result = filebase.add(file);
	if(result)
		log(LOG_INFO, format("File (%s) added successfully to: ", file.name) + dir.code);
	else
		log(LOG_ERR, "Error " + filebase.last_error + " adding file to: " + dir.code);
	filebase.close();
	
	return result ? "200 bitchen" : "500 error";
}

function main()
{
	if(!user.number) {
		http_reply.status = "403 Must auth first";
		return;
	}
	switch(http_request.method) {
		case "POST":
            log(LOG_DEBUG, "http_request = " + JSON.stringify(http_request));
			http_reply.status = post(http_request.query);
			break;
		default:
			http_reply.status = "404 method not supported";
			break;
	}
}

main();

