load(system.exec_dir + "../web/lib/init.js");
load(settings.web_lib + "auth.js");
load(settings.web_lib + "mime-decode.js");

var barfOut = function(err) {
	log(err);
	exit();
}

if(	typeof http_request.query.sub == "undefined"
	||
	(	http_request.query.sub[0] != 'mail'
		&&
		typeof msg_area.sub[http_request.query.sub[0]] == "undefined"
	)
) {
	barfOut("Invalid sub.");
}

var sub = http_request.query.sub[0];

if(typeof http_request.query.msg == "undefined")
	barfOut("No message number provided.");
var id = parseInt(http_request.query.msg[0]);

if(typeof http_request.query.cid != "undefined")
	var cid = http_request.query.cid[0];
else if(typeof http_request.query.filename != "undefined")
	var filename = http_request.query.filename[0];
else
	barfOut("No attachment specified.");

var msgBase = new MsgBase(sub);
if(!msgBase.open())
	barfOut("Unable to open MsgBase " + sub);

var header = msgBase.get_msg_header(false, id);
if(header === null)
	barfOut("No such message.");
if(typeof msgBase.cfg == "undefined" && header.to_ext != user.number)
	barfOut("Not your message.");

var body = msgBase.get_msg_body(false, id, header);
if(body === null)
	barfOut("Cannot read message body!");
msgBase.close();

if(typeof cid != "undefined")
	var att = mime_get_cid_attach(header, body, cid);
else if(typeof filename != "undefined")
	var att = mime_get_attach(header, body, filename);

if(typeof att != "undefined") {
	if(typeof att.content_type != "undefined")
		http_reply.header["Content-Type"] = att.content_type;
	http_reply.header["Content-Length"] = att.body.length;
	write(att.body);
}