if (js.global.MSG_DELETE == undefined)
	js.global.load("sbbsdefs.js");

function podcast_load_headers(base, from, to)
{
	var hdrs = [];
	var i;
	var hdr;

	for (i = base.first_msg; i <= base.last_msg; i++) {
		hdr = base.get_msg_header(i);
		if (hdr == null)
			continue;
		if (hdr.attr & MSG_DELETE)
			continue;
		if (hdr.attr & MSG_MODERATED) {
			if (!(hdr.attr & MSG_VALIDATED))
				break;
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
	m = body.match(/^[\r\n\s]*([\x00-\xff]+?)[\r\n\s]+(https?:\/\/[^\r\n\s]+)[\r\n\s]*$/);
	if (m==null)
		return;
	ret.title = hdr.subject;
	ret.description = m[1];
	ret.enclosure = m[2];
	ret.guid = hdr.id;
	ret.pubDate = (new Date(hdr.when_written_time * 1000)).toUTCString();
	return ret;
}
