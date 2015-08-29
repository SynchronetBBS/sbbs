if (js.global.HTTP == undefined)
	js.global.load("http.js");
if (js.global.MSG_DELETE == undefined)
	js.global.load("sbbsdefs.js");

function podcast_load_headers(base, from, to, all_hdrs)
{
	var all_msg_headers;
	var hdrs = [];
	var i;
	var hdr;
	var unvalidated = false;

	if (all_hdrs == undefined)
		all_hdrs = base.get_all_msg_headers();
	else {
		all_msg_headers = base.get_all_msg_headers();
		for (i in all_msg_headers)
			all_hdrs[i] = all_msg_headers[i];
	}
	for (i in all_hdrs) {
		hdr = all_hdrs[i];
		if (unvalidated) {
			delete all_hdrs[i];
			continue;
		}
		if (hdr == null) {
			delete all_hdrs[i];
			continue;
		}
		if (hdr.attr & MSG_DELETE) {
			delete all_hdrs[i];
			continue;
		}
		if (hdr.attr & MSG_MODERATED) {
			if (!(hdr.attr & MSG_VALIDATED)) {
				unvalidated=true;
				continue;
			}
		}
		if (hdr.thread_back != 0)
			continue;
		if (hdr.from.toLowerCase() != from.toLowerCase() || hdr.to.toLowerCase() != to.toLowerCase())
			continue;
		if (hdr.from_net_type != NET_NONE)
			continue;
		hdrs.push(hdr);
	}
	return hdrs;
}

function podcast_get_info(base, hdr)
{
	var body;
	var m;
	var ret={};

	body = base.get_msg_body(hdr.number);
	if (body == null)
		return;
	body = word_wrap(body, 65535, 79, false).replace(/\r/g, '');
	m = body.match(/^[\r\n\s]*([\x00-\xff]+?)[\r\n\s]+(https?:\/\/[^\r\n\s]+\.mp3)[\r\n\s]/);
	if (m==null)
		return;
	ret.title = hdr.subject;
	ret.description = m[1];
	ret.enclosure = {};
	ret.enclosure.url = m[2];
	ret.guid = hdr.id;
	ret.pubDate = (new Date(hdr.when_written_time * 1000)).toUTCString();
	return ret;
}

function podcast_get_enclosure_info(enc)
{
	var http;
	var hdrs;

	if (enc == undefined)
		return false;
	if (enc.url == undefined)
		return false;
	http = new HTTPRequest();
	hdrs = http.Head(enc.url);
	if (hdrs == undefined)
		return false;
	if (hdrs['Content-Type'] == undefined || hdrs['Content-Length'] == undefined) {
		log("HEAD request of "+enc.url+" did not return either Content-Type or Content-Length");
		return false;
	}
	enc.length = parseInt(hdrs['Content-Length'][0], 10);
	enc.type = hdrs['Content-Type'][0].replace(/^\s*(.*?)\s*/, "$1");
	return true;
}
