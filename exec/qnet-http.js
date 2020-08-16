// $Id: qnet-http.js,v 1.3 2019/05/27 02:05:26 rswindell Exp $

// QWK network HTTP[S] client

// Example usage in SCFG->Networks->QWK->Hubs->VERT->Call-out Command Line:
//
//                  ?qnet-http %s dove.synchro.net YOURPASS

const REVISION = "$Revision: 1.3 $".split(' ')[1];
log(LOG_INFO, "QNET-HTTP " + REVISION + " invoked with options: " + argv.join(' '));

load("http.js");

const HTTP_RESPONSE_SUCCESS = 200;
const HTTP_RESPONSE_NO_CONTENT = 204;

if(argc < 3) {
	alert("usage: [-s] <hub-id> <host-name>[:port] <your-password>");
	exit();
}
var scheme = "http://";
var userid = system.qwk_id;
if(argv[0] == '-s') {
	scheme = "https://";
	argv.shift();
}
var hubid = argv[0];
var hostname = argv[1];
var password = argv[2];
var url = format("%s%s/qwk.ssjs", scheme, hostname);
var rep = format("%s%s.rep", system.data_dir, hubid);
var qwk = format("%s%s.qwk", system.data_dir, hubid);

function send_rep(rep)
{
	print("Sending " + rep);
	var file = new File(rep);
	if(!file.open("rb")) {
		alert("error " + file.error + " opening " + file.name);
		return false;
	}
	var data = file.read();
	file.close();
	if(!data) {
		alert("No data read from " + file.name);
		return false;
	}
	var http = new HTTPRequest(userid, password);
	http.Post(url, data);
	if(http.response_code != HTTP_RESPONSE_SUCCESS) {
		alert(http.request + " Response: " + http.status_line);
		return false;
	}
	if(!file_remove(rep)) {
		log(LOG_ERR, "Error removing file: " + rep);
		return false;
	}
	return true;
}

function receive_qwk(qwk)
{
	print("Getting " + qwk);
	var file = new File(qwk);
	if(!file.open("wb")) {
		alert("error " + file.error + " opening " + file.name);
		return false;
	}
	var success = false;
	var http = new HTTPRequest(userid, password);
	var contents = http.Get(url);
	if(http.response_code == HTTP_RESPONSE_NO_CONTENT) {
		print("No packet received");
		success = true;
	}
	else if(http.response_code != HTTP_RESPONSE_SUCCESS)
		alert(http.request + " Response: " + http.status_line);
	else if(!contents)
		alert("No data received");
	else {
		print("Received " + contents.length + " bytes");
		file.write(contents);
		http.Post(url + '?received=' + file.position, '');
		success = (http.response_code == HTTP_RESPONSE_NO_CONTENT);
	}
	file.close();
	return success;
}

if(file_exists(rep)) {
	if(!send_rep(rep))
		exit(1);
}

if(!receive_qwk(qwk))
	exit(1);
