// $Id: qwk.ssjs,v 1.2 2019/06/02 04:55:14 rswindell Exp $
// vi: tabstop=4

// Handle QWK packet transfers (uploads of REP packets and downloads of QWK packets)
// in the Synchronet Web Server (e.g. using exec/qnet-http.js)

// Although this script is really intended to be used with qnet-http.js, it
// can be used with other http-clients (e.g. for use with other BBS software).
// For example, using GNU Wget (place https:// before <hostname> for HTTPS):

// Upload a REP packet:
// $ wget --auth-no-challenge --post-file=<hub-ID>.qwk --http-user=<username>
//        --http-password=<password> <hub-hostname>/qwk.ssjs
       
// Generate/download a QWK packet:
// $ wget --auth-no-challenge --http-user=<username> --http-password=<password>
//        --content-disposition <hub-hostname>/qwk.ssjs
       
// Confirm successful download of a QWK packet:
// $ wget --auth-no-challenge --http-user=<username> --http-password=<password>
//        --post-data= <hub-hostname>/qwk.ssjs?received=<QWK-file-length>

const REVISION = "$Revision: 1.2 $".split(' ')[1];
log(LOG_INFO, "QWK Packet Handler (qwk.ssjs) " + REVISION);

const pack_timeout = 60;	// seconds
var qwkfile = system.data_dir + format("file/%04u.qwk", user.number);

function get(query)
{
	var semfile = system.data_dir + format("pack%04u.now", user.number);

	if(!file_exists(qwkfile)) {
        log("Requesting creation of QWK packet: " + qwkfile);
		file_touch(semfile);
		var start = time();
		while(file_exists(semfile)) {
			if(js.terminated)
				return "503 server terminated";
			if(time() - start >= pack_timeout)
				break;
		}
		if(file_exists(semfile)) {
			log(LOG_WARNING, "timeout generating QWK packet: " + qwkfile);
			return "503 timeout generating QWK packet";
		}
		if(!file_exists(qwkfile)) {
			mswait(500);
			if(!file_exists(qwkfile)) {
				log(LOG_INFO, "No QWK packet created: " + qwkfile);
				return "204 No QWK packet created (no new messages?)";
			}
		}
	}
	var file = new File(qwkfile);
	if(!file.open("rb")) {
		log(LOG_ERR, "error " + file.error + " opening QWK packet: " + file.name);
		return "503 failed open QWK packet";
	}
	http_reply.header["Content-Type"] = "application/octet-stream";
	http_reply.header["Content-Disposition"] = format('inline; filename="%s.qwk"', system.qwk_id);
	while(!file.eof) {
		log(LOG_DEBUG, "!eof " + file.name);
		var data = file.read();
		if(!data.length)
			break;
		log(LOG_DEBUG, "read " + data.length + " from " + file.name);
		write(data);
		log(LOG_DEBUG, "wrote to ringbuf");
	}
	log(LOG_DEBUG, "closing " + file.name);
	file.close();
	return "200 here's a QWK packet for you";
}

function post(query)
{
	log(LOG_INFO, "query: " + JSON.stringify(query));
	if(query["received"]) {
		var size = parseInt(query["received"], 10);
		var qwksize = file_size(qwkfile);
		if(size == qwksize) {
			log(LOG_INFO, "Received confirmation of successful QWK packet receipt");
			if(file_remove(qwkfile))
				return "204 Gotcha, QWK packet removed";
			return "503 Could not remove QWK packet!";
		}
		log(LOG_WARNING
			,format("Receive mismatch QWK packet receipt size (%lu), expected: %lu", size, qwksize));
		return "400 recipe size mismatch, expected: " + qwksize;
	}

	if(!http_request.post_data) {
		log(LOG_ERR, "no post data provided");
		return "500 No post data provided";
	}
	log(LOG_INFO, "received REP packet: " + http_request.post_data.length + " bytes");

	var repfile = system.data_dir + format("file/%04u.rep", user.number);

	if(file_exists(repfile)) {
		log(LOG_ERR, file.name + " already exists");
		return "409 REP packet already pending";
	}

	var file = new File(repfile);
	if(!file.open("wb")) {
		log(LOG_ERR, "error " + file.error + " opening REP packet: " + file.name);
		return "409 error creating REP packet";
	}
	file.write(http_request.post_data);
	file.close();
	return "200 bitchen";
}

function main()
{
	if(!user.number) {
		http_reply.status = "403 Must auth first";
		return;
	}
	switch(http_request.method) {
		case "GET":
			http_reply.status = get(http_request.query);
			break;
		case "POST":
			http_reply.status = post(http_request.query);
			break;
		default:
			http_reply.status = "404 method not supported";
			break;
	}
}

main();

