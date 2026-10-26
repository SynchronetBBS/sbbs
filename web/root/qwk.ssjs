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

const REVISION = "1.3";
log(LOG_INFO, "QWK Packet Handler (qwk.ssjs) " + REVISION);

const pack_timeout = 60;	// seconds
var qwkfile = system.data_dir + format("file/%04u.qwk", user.number);

function get(query)
{
	if(!file_exists(qwkfile)) {
		if(typeof system.qwk_pack == "function") {
			var r = system.qwk_pack(user.number);
			log(LOG_INFO, "qwk_pack: " + r.result + " (" + r.msgs + " msgs)");
			if(r.result == "busy" || r.result == "throttled") {
				http_reply.header["Retry-After"] = "60";
				return "503 QWK packet creation already in progress";
			}
			if(r.result == "empty")
				return "204 No QWK packet created (no new messages?)";
			if(r.result != "ok")
				return "503 QWK packet creation failed: " + r.result;
		} else {
			// Older binary: ask the Terminal Server's event thread via semaphore file
			var semfile = system.data_dir + format("pack%04u.now", user.number);
			log("Requesting creation of QWK packet: " + qwkfile);
			file_touch(semfile);
			var start = time();
			while(file_exists(semfile)) {
				if(js.terminated)
					return "503 server terminated";
				if(time() - start >= pack_timeout)
					break;
				mswait(500);
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
		return "400 received size mismatch, expected: " + qwksize;
	}

	if(!http_request.post_data) {
		log(LOG_ERR, "no post data provided");
		return "500 No post data provided";
	}
	log(LOG_INFO, "received REP packet: " + http_request.post_data.length + " bytes");

	if(typeof system.qwk_unpack != "function")
		return post_legacy();

	var repdir = system.temp_dir + "rep/";
	mkdir(repdir);
	var path = repdir + format("%04u.%u.rep", user.number, client.socket);
	var file = new File(path);
	if(!file.open("wb")) {
		log(LOG_ERR, "error " + file.error + " opening REP packet: " + file.name);
		return "503 error creating REP packet";
	}
	file.write(http_request.post_data);
	file.close();

	var r = system.qwk_unpack(user.number, path);
	log(LOG_INFO, "qwk_unpack: " + r.result);
	switch(r.result) {
		case "ok":
			return "200 bitchen";
		case "busy":
		case "throttled":
			// 200, not 202: qnet-http.js re-sends the REP on any non-200,
			// which would post every reply twice once the sweep imports this copy
			if(queue_rep(path))
				return "200 bitchen";
			return "503 error queueing REP packet";
		case "failed":
			// The packet was received (a bad one is kept as .bad); re-sending won't help
			log(LOG_WARNING, "REP packet import failed: " + path);
			// An empty packet is left in place by the job outside the sweep directory
			if(file_exists(path))
				file_remove(path);
			return "200 bitchen";
		default:
			log(LOG_ERR, "REP packet import: " + r.result);
			file_remove(path);
			return "503 REP packet import failed: " + r.result;
	}
}

// Hand a REP packet to the Terminal Server event thread's sweep
function queue_rep(path)
{
	var base = system.data_dir + format("file/%04u", user.number);
	// data/file/ is shared by every host: name the staging file by host and server.
	// Not ".rep*": Windows matches the sweep's "*.rep" against 8.3 short names too.
	var tmp = base + format(".%s.web%u.tmp", system.host_name.replace(/[^\w.-]/g, "_"), client.socket);
	var dest = base + ".rep";
	for(var i = 0; i < 10 && file_exists(dest); i++)
		dest = base + format(".%x.rep", time() + i);
	if(file_exists(dest)) {
		log(LOG_ERR, "no free queue name for REP packet: " + dest);
		return false;
	}
	// Write under a name the sweep ignores, then rename into place
	var file = new File(tmp);
	if(!file.open("wb")) {
		log(LOG_ERR, "error " + file.error + " opening " + file.name);
		return false;
	}
	file.write(http_request.post_data);
	file.close();
	if(!file_rename(tmp, dest)) {
		log(LOG_ERR, "error renaming " + tmp + " to " + dest);
		file_remove(tmp);
		return false;
	}
	file_remove(path);
	log(LOG_INFO, "queued REP packet: " + dest);
	return true;
}

// Older binary: leave the REP for the Terminal Server event thread
function post_legacy()
{
	var repfile = system.data_dir + format("file/%04u.rep", user.number);

	if(file_exists(repfile)) {
		log(LOG_ERR, repfile + " already exists");
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

